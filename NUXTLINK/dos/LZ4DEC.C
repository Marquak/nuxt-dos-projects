/* Bounded raw LZ4 block decoder for 8086 near buffers (max 4096 bytes).
 * The encoder runs on Linux. Each block starts with an empty dictionary, so
 * a match may refer only to bytes already produced in this output buffer.
 */
#include <string.h>

int lz4_unpack(const unsigned char *src, unsigned n,
               unsigned char *dst, unsigned expected)
{
    unsigned in=0, out=0, literals, length, offset, extra, token;
    if(expected>4096 || !expected)return 0;
    while(in<n){
        token=src[in++];
        literals=token>>4;
        if(literals>expected-out)return 0;
        if(literals==15){
            do{
                if(in>=n)return 0;
                extra=src[in++];
                if(extra>expected-out-literals)return 0;
                literals+=extra;
            }while(extra==255);
        }
        if(literals>n-in)return 0;
        memcpy(dst+out,src+in,literals);
        in+=literals;out+=literals;
        if(in==n)return out==expected;
        if(n-in<2)return 0;
        offset=src[in]|((unsigned)src[in+1]<<8);
        in+=2;
        if(!offset||offset>out)return 0;
        length=(token&15)+4;
        if(length>expected-out)return 0;
        if(length==19){
            do{
                if(in>=n)return 0;
                extra=src[in++];
                if(extra>expected-out-length)return 0;
                length+=extra;
            }while(extra==255);
        }
        /* Offset one repeats the preceding byte. Fill it in one operation,
         * and bulk-copy other nonoverlapping matches. Keep the byte loop for
         * matches that overlap their own output. */
        if(offset==1){
            memset(dst+out,dst[out-1],length);
            out+=length;
        }else if(length<=offset){
            memcpy(dst+out,dst+out-offset,length);
            out+=length;
        }else{
            while(length--){dst[out]=dst[out-offset];out++;}
        }
    }
    return 0;
}
