/* IRQ-fed receive ring; bounded nonblocking foreground transmitter.
 * COM1/COM2 only, standard IRQ4/IRQ3. No DOS or rendering in the IRQ.
 * UART register handling follows our NuXTLink design, not its blocking protocol. */
#define RSIZE 2048
static unsigned portbase, portirq, portvector, txlen, txpos;
static volatile unsigned rxhead, rxtail, uart_errors, rxoverflow, irqcount;
static volatile unsigned uart_overrun, uart_parity, uart_framing, uart_break;
static unsigned char rxring[RSIZE], txwire[LSIZE*2+2];
static unsigned char slcr, sier, smcr, sdl, sdh, spic;
static int serialup, hasfifo, flowcontrol;
static volatile unsigned char rtslow;
static unsigned ctsretry;
static int ctsblocked;
static void (__interrupt __far *oldserial)(void);
/* LSR errors clear on read, including reads made while polling TX.
 * Call with interrupts disabled (or from the ISR) to protect the counters. */
static unsigned serial_status(void) {
    unsigned lsr=inp(portbase+5);
    if(lsr&0x1e) {
        ++uart_errors;
        if(lsr&2) ++uart_overrun;
        if(lsr&4) ++uart_parity;
        if(lsr&8) ++uart_framing;
        if(lsr&16) ++uart_break;
    }
    return lsr;
}
static void __interrupt __far serial_isr(void) {
    unsigned lsr, n=0, next;
    unsigned char b;
    ++irqcount;
    inp(portbase+2);
    /* Drain only our device. A dedicated standard COM IRQ is required. */
    while(n++<64) {
        lsr=serial_status();
        if(!(lsr&1)) break;
        b=inp(portbase);
        next=(rxhead+1)&(RSIZE-1);
        if(next==rxtail) ++rxoverflow;
        else { rxring[rxhead]=b; rxhead=next; }
    }
    if(flowcontrol && !rtslow && ((rxhead-rxtail)&(RSIZE-1))>1536) {
        outp(portbase+4,9); rtslow=1;
    }
    /* IER enables RX only. No modem-status IRQ needs acknowledgement here.
       DOSBox-X directserial turns an MSR read into a host-device query. */
    outp(0x20,0x20);
}
static int serial_open(unsigned com, unsigned long baud) {
    unsigned divisor=115200UL/baud;
    portbase=*(unsigned __far *)MK_FP(0x40,(com-1)*2);
    if(portbase!=(com==1?0x3f8:0x2f8)) return 0;
    rxhead=rxtail=uart_errors=rxoverflow=irqcount=0;
    uart_overrun=uart_parity=uart_framing=uart_break=0;
    txpos=txlen=rtslow=ctsblocked=ctsretry=0;
    portirq=com==1?4:3; portvector=8+portirq;
    slcr=inp(portbase+3); smcr=inp(portbase+4);
    outp(portbase+3,slcr&127); sier=inp(portbase+1);
    outp(portbase+1,0);
    outp(portbase+3,0x80); sdl=inp(portbase); sdh=inp(portbase+1);
    outp(portbase,divisor&255); outp(portbase+1,divisor>>8);
    outp(portbase+3,3); outp(portbase+2,7);
    hasfifo=(inp(portbase+2)&0xc0)==0xc0;
    if(!hasfifo) outp(portbase+2,0);
    /* A FIFO reset can leave a pending receive-timeout indication in some
       UART emulations. Reading RBR also acknowledges that empty/stale event
       before OUT2/IER enable; otherwise the first IRQ can remain asserted. */
    inp(portbase); inp(portbase+5); inp(portbase+6); inp(portbase+2);
    oldserial=_dos_getvect(portvector);
    _dos_setvect(portvector,serial_isr);
    _disable();
    spic=inp(0x21);
    outp(portbase+4,11); /* DTR, RTS, OUT2 */
    outp(portbase+1,1); /* RX only; TX is polled without waiting. */
    outp(0x21,spic&~(1<<portirq));
    serialup=1;
    _enable();
    return 1;
}
static void serial_close(void) {
    if(!serialup) return;
    _disable();
    outp(portbase+1,0);
    outp(0x21,(inp(0x21)&~(1<<portirq))|(spic&(1<<portirq)));
    outp(portbase+4,0); outp(portbase+2,0);
    _enable();
    _dos_setvect(portvector,oldserial);
    _disable();
    outp(portbase+3,0x80); outp(portbase,sdl); outp(portbase+1,sdh);
    outp(portbase+3,slcr&127); outp(portbase+1,sier);
    outp(portbase+4,smcr); outp(portbase+3,slcr);
    serialup=0;
    _enable();
}
static int serial_get(void) {
    unsigned b;
    if(rxtail==rxhead) return -1;
    b=rxring[rxtail]; rxtail=(rxtail+1)&(RSIZE-1);
    if(flowcontrol && rtslow && ((rxhead-rxtail)&(RSIZE-1))<512) {
        _disable();
        if(((rxhead-rxtail)&(RSIZE-1))<512) { outp(portbase+4,11); rtslow=0; }
        _enable();
    }
    return b;
}
static void serial_pump(void) {
    unsigned lsr, budget=hasfifo?16:1;
    if(txpos==txlen || (ctsblocked && ticks==ctsretry)) return;
    _disable(); lsr=serial_status(); _enable();
    if(!(lsr&32)) return;
    if(flowcontrol) {
        ctsblocked=!(inp(portbase+6)&16); ctsretry=ticks;
        if(ctsblocked) return;
    }
    while(budget-- && txpos<txlen) outp(portbase,txwire[txpos++]);
}
static int serial_send(const unsigned char *b) {
    if(txpos!=txlen) return 0;
    txlen=lencode(b,txwire); txpos=0;
    return 1;
}
