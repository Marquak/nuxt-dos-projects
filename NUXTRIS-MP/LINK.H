/* NuXTris serial wire format. Explicit bytes, no native structs on the wire.
 * Original project code, MIT. A complete snapshot heals lost movement packets.
 * CRC-16/IBM covers every byte before the CRC. */
#include <stdint.h>
#include <string.h>
#define LSIZE 162
#define LCRC (LSIZE-2)
#define LVERSION 2
#define LCONNECT 0
#define LREADY 1
#define LRUN 2
#define LFINISH 3
#define LABORT 4
static uint16_t lget16(const unsigned char *b) {
    return (uint16_t)(b[0] | ((uint16_t)b[1] << 8));
}
static uint32_t lget32(const unsigned char *b) {
    return (uint32_t)lget16(b) | ((uint32_t)lget16(b + 2) << 16);
}
static void lput16(unsigned char *b, uint16_t n) { b[0] = n; b[1] = n >> 8; }
static void lput32(unsigned char *b, uint32_t n) {
    lput16(b, (uint16_t)n); lput16(b + 2, (uint16_t)(n >> 16));
}
static uint16_t lcrc(const unsigned char *b, unsigned n) {
    static const uint16_t tab[16] = {0,0xcc01,0xd801,0x1400,0xf001,0x3c00,0x2800,0xe401,
        0xa001,0x6c00,0x7800,0xb401,0x5000,0x9c01,0x8801,0x4400};
    uint16_t c = 0xffff;
    while (n--) { c ^= *b++; c = (c >> 4) ^ tab[c & 15]; c = (c >> 4) ^ tab[c & 15]; }
    return c;
}
static int lnewer(uint16_t a, uint16_t b) { return (int16_t)(a - b) > 0; }
/* Time and ACK progress ride the heartbeat; gameplay/control changes send
 * sooner. The complete snapshot and CRC format stay compatible. */
static int lchanged(const unsigned char *a, const unsigned char *b) {
    return memcmp(a+4,b+4,8) || memcmp(a+16,b+16,9) || memcmp(a+27,b+27,LCRC-27);
}
/* Strict decimal options, with no wrap, signs or trailing junk. */
static int lnumber(const char *s,uint32_t limit,uint32_t *value) {
    uint32_t n=0,top=limit/10;
    unsigned digit,last=(unsigned)(limit%10);
    if(!*s) return 0;
    while(*s) {
        if(*s<'0' || *s>'9') return 0;
        digit=*s++-'0';
        if(n>top || (n==top && digit>last)) return 0;
        n=n*10+digit;
    }
    *value=n;
    return 1;
}
/* Caller provides LSIZE*2+2 bytes. Escape FF as well: DOSBox-X's Linux
 * directserial backend interprets raw FF as a termios error marker even
 * with PARMRK disabled. Existing decoders already accept this escape.
 * The original payload and its CRC are unchanged. */
static unsigned lencode(const unsigned char *b, unsigned char *out) {
    unsigned i,n=0;
    out[n++]=0x7e;
    for(i=0;i<LSIZE;i++) {
        if(b[i]==0x7e || b[i]==0x7d || b[i]==0xff) {
            out[n++]=0x7d; out[n++]=b[i]^32;
        } else out[n++]=b[i];
    }
    out[n++]=0x7e;
    return n;
}
static int lname(const unsigned char *b) {
    unsigned i;
    for (i=0;i<8;i++)
        if (b[i] && b[i]!=' ' && !(b[i]>='A' && b[i]<='Z') && !(b[i]>='0' && b[i]<='9')) return 0;
    return 1;
}
static int lvalid(const unsigned char *b) {
    unsigned i, j;
    if (b[0]!='N' || b[1]!='T' || b[2]!=LVERSION || b[3]>1 || !lget32(b+4) ||
        b[16]>LABORT || (b[17]&~63) || b[20]>1 || b[21]>9 || b[22]>10 ||
        !lget16(b+23) || lget16(b+25)>21848 || lget32(b+27)>999999999UL ||
        lget32(b+31)>lget32(b+27) || b[37]>19 || b[38]>6 || b[39]>6 ||
        (int8_t)b[40]<-2 || (int8_t)b[40]>11 || (int8_t)b[41]<-3 || (int8_t)b[41]>19 ||
        !lname(b+50) || !lget16(b+158) || lget16(b+LCRC)!=lcrc(b,LCRC)) return 0;
    for(i=42;i<50;i++) if ((int8_t)b[i]<-2 || (int8_t)b[i]>2) return 0;
    for(i=0;i<4;i++) for(j=i+1;j<4;j++)
        if(b[42+2*i]==b[42+2*j] && b[43+2*i]==b[43+2*j]) return 0;
    for(i=58;i<158;i++) if ((b[i]&15)>7 || (b[i]>>4)>7) return 0;
    return 1;
}
typedef struct { unsigned used, escaped, bad; unsigned char data[LSIZE]; } LDecoder;
/* Returns 1 for a validated frame, -1 for a rejected frame, 0 for partial data.
 * Delimiter always resets the parser, including after overflow or lost escape. */
static int ldecode(LDecoder *d,unsigned c) {
    int result=0;
    if(c==0x7e) {
        if(d->used || d->bad)
            result=!d->bad && !d->escaped && d->used==LSIZE && lvalid(d->data)?1:-1;
        d->used=d->escaped=d->bad=0;
    } else if(!d->bad) {
        if(c==0x7d && !d->escaped) d->escaped=1;
        else {
            if(d->escaped) { c^=32; d->escaped=0; }
            if(d->used<LSIZE) d->data[d->used++]=c;
            else d->bad=1;
        }
    }
    return result;
}
