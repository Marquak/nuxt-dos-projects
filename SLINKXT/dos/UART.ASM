; Slink-XT: bounded 8086 receive loop, no DOS/BIOS calls in the byte path.
; DX=UART base, DI=DS destination, CX=exact byte count.
; Returns DX:AX = fault:received. Fault is raw LSR on an error, 0100h on timeout.
; The caller owns RTS and checksum verification. ES/BP are preserved; IF is untouched.
.8086
.model tiny
.code
public uart_rx_
uart_rx_ proc near
    push es
    push bp
    xor si,si
    mov ax,0040h
    mov es,ax
    add dx,5
    jcxz rx_done
rx_poll:
    in al,dx
    test al,1eh
    jnz rx_fault
    test al,1
    jnz rx_byte
    mov bp,es:[006ch]
rx_wait:
    in al,dx
    test al,1eh
    jnz rx_fault
    test al,1
    jnz rx_byte
    mov ax,es:[006ch]
    sub ax,bp
    cmp ax,18
    jb rx_wait
    mov dx,0100h
    jmp short rx_return
rx_byte:
    sub dx,5
    in al,dx
    mov [di],al
    inc di
    inc si
    add dx,5
    loop rx_poll
rx_done:
    xor dx,dx
    jmp short rx_return
rx_fault:
    xor ah,ah
    mov dx,ax
rx_return:
    mov ax,si
    pop bp
    pop es
    ret
uart_rx_ endp
end
