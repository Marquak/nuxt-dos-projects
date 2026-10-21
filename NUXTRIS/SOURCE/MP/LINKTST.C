#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "LINK.H"
static unsigned char b[LSIZE];
static void seal(void){lput16(b+LCRC,lcrc(b,LCRC));}
static unsigned encode(unsigned char *out){
    return lencode(b,out);
}
int main(void){
    unsigned i,j,n; int ok,r; unsigned char out[LSIZE*2+2]; LDecoder d;
    uint32_t value;
    unsigned char snapshot[LSIZE]={0},changed[LSIZE];
    /* Every byte that describes the players/rules must trigger a snapshot;
       sequence, ACK, clock and CRC are handled separately by heartbeat. */
    for(i=0;i<LSIZE;i++) {
        memcpy(changed,snapshot,LSIZE); changed[i]=1;
        assert(!!lchanged(changed,snapshot)==
            ((i>=4 && i<12) || (i>=16 && i<25) || (i>=27 && i<LCRC)));
    }
    assert(lnumber("115200",115200UL,&value) && value==115200UL);
    assert(lnumber("0",9,&value) && value==0);
    assert(lnumber("4294967295",0xffffffffUL,&value) && value==0xffffffffUL);
    assert(!lnumber("4294967296",0xffffffffUL,&value));
    assert(!lnumber("65537",2,&value));
    assert(!lnumber("115201",115200UL,&value));
    assert(!lnumber("",10,&value)); assert(!lnumber("-1",10,&value));
    assert(!lnumber("1x",10,&value)); assert(!lnumber("+1",10,&value));
    assert(lcrc((unsigned char *)"123456789",9)==0x4b37);
    b[0]='N';b[1]='T';b[2]=LVERSION;lput16(b+158,1);lput32(b+4,0x7e7d);
    b[22]=2;lput16(b+23,1);b[44]=1;b[47]=1;b[48]=1;b[49]=1;
    b[162]=5;b[163]=2;b[170]=9;b[171]=1;
    lput32(b+164,123456);lput16(b+168,40);
    memcpy(b+50,"BOB",3);seal();
    assert(lvalid(b));
    b[17]=64; b[160]=0x7e; b[161]=0xff; seal(); assert(lvalid(b));
    n=encode(out); memset(&d,0,sizeof(d)); ok=0;
    for(i=0;i<n;i++) { assert(out[i]!=0xff); if(ldecode(&d,out[i])==1) ++ok; }
    assert(ok==1 && !memcmp(d.data,b,LSIZE));
    b[17]=b[160]=b[161]=0; seal();
    for(i=0;i<LSIZE;i++)for(j=0;j<8;j++){b[i]^=1<<j;assert(!lvalid(b));b[i]^=1<<j;}
    assert(lnewer(0,65535));assert(!lnewer(65535,0));assert(!lnewer(8,8));
    n=encode(out);memset(&d,0,sizeof(d));ok=0;
    for(i=0;i<n;i++) if(ldecode(&d,out[i])==1)ok++;
    assert(ok==1);
    for(j=1;j<n-1;j++){
        memset(&d,0,sizeof(d));ok=0;
        for(i=0;i<n;i++)if(i!=j && ldecode(&d,out[i])==1)ok++;
        assert(ok==0);
        for(i=0;i<n;i++)if(ldecode(&d,out[i])==1)ok++;
        assert(ok==1);
    }
    memset(&d,0,sizeof(d));for(i=0;i<2000;i++)ldecode(&d,65);
    assert(ldecode(&d,0x7e)==-1);
    for(i=0,r=0;i<n;i++)if(ldecode(&d,out[i])==1)r++;
    assert(r==1);
    /* Every possible nonce byte, plus a negative shape coordinate (FF).
       Linux directserial must never see an unescaped FF on the wire. */
    b[42]=255;
    for(j=0;j<256;j++) {
        lput32(b+4,0x100UL+j);seal();assert(lvalid(b));
        n=encode(out);memset(&d,0,sizeof(d));ok=0;
        for(i=0;i<n;i++) {
            assert(out[i]!=255);
            if(ldecode(&d,out[i])==1)ok++;
        }
        assert(ok==1 && !memcmp(d.data,b,LSIZE));
    }
    b[22]=0;seal();assert(lvalid(b)); /* Marathon */
    b[22]=1;seal();assert(lvalid(b)); /* Sprint */
    b[22]=3;seal();assert(!lvalid(b));b[22]=2;
    b[162]=6;seal();assert(!lvalid(b));b[162]=5;
    b[163]=3;seal();assert(!lvalid(b));b[163]=2;
    b[170]=20;seal();assert(!lvalid(b));b[170]=9;
    b[171]=2;seal();assert(!lvalid(b));b[171]=1;
    lput32(b+164,1000000000UL);seal();assert(!lvalid(b));lput32(b+164,123456);
    b[58]=0x80;seal();assert(!lvalid(b));b[58]=0;
    b[41]=25;seal();assert(!lvalid(b));b[41]=0;
    b[37]=20;seal();assert(!lvalid(b));
    printf("PASS: snapshot change coverage, strict numeric options, CRC reference, %u single-bit corruptions, Tetris event bytes, loss/escape recovery, overlong frame, sequence wrap, mode/pile/winner validation.\n",LSIZE*8);
    return 0;
}
