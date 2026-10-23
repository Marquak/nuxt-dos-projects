/* Bounds-check a raw LZ4 block with near pointers,
 * then use Jim Leonard's 8088 copy engine.
 */
extern unsigned lz4_raw(const unsigned char *src, unsigned n,
                        unsigned char *dst);
#pragma aux lz4_raw parm [si] [cx] [di] value [ax] modify [ax bx cx dx si di es]

int lz4_unpack(const unsigned char *src, unsigned n,
               unsigned char *dst, unsigned expected)
{
    const unsigned char *ip=src, *end=src+n;
    unsigned out=0,literals,length,offset,extra,token,had_match=0;
    if(!n||!expected||expected>4096)return 0;
    for(;;){
        if(ip>=end)return 0;
        token=*ip++;
        literals=token>>4;
        if(literals>expected-out)return 0;
        if(literals==15){
            do{
                if(ip>=end)return 0;
                extra=*ip++;
                if(extra>expected-out-literals)return 0;
                literals+=extra;
            }while(extra==255);
        }
        if(literals>(unsigned)(end-ip))return 0;
        ip+=literals;out+=literals;
        if(ip==end){
            if(out!=expected||(had_match&&literals<5))return 0;
            return lz4_raw(src,n,dst)==expected;
        }
        if((unsigned)(end-ip)<2)return 0;
        offset=ip[0]|((unsigned)ip[1]<<8);ip+=2;
        if(!offset||offset>out)return 0;
        length=(token&15)+4;
        if(length>expected-out)return 0;
        if(length==19){
            do{
                if(ip>=end)return 0;
                extra=*ip++;
                if(extra>expected-out-length)return 0;
                length+=extra;
            }while(extra==255);
        }
        out+=length;had_match=1;
    }
}
