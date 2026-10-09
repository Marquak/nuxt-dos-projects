/* Optional 8087 work, isolated from the software-floating-point DOS build.
 * All arithmetic here is 8087-compatible: notably no FSIN or FNSTSW AX.
 * Raw ESC bytes prevent Watcom's -fpc mode from substituting emulation INTs.
 *
 * Detection follows Intel's 1987 80386 Hardware Reference Manual,
 * Figure 5-4 (pp. 5-9/5-10), explicitly covering 8086/8088 plus 8087.
 * The two MOVs following FNINIT allow the original 8087 to initialize.
 * Memory FNSTSW/FNSTCW synchronize via the coprocessor bus interface;
 * Intel explicitly says not to add WAIT before presence is established.
 * AP-113 (1981), p.25 explains this data synchronization in detail.
 * https://www.bitsavers.org/components/intel/80386/1987_80386_Hardware_Reference_Manual.pdf
 * https://www.bitsavers.org/components/intel/appNotes/AP-113_Getting_Started_With_the_Numeric_Data_Processor_Feb81.pdf
 */
#include "FPU.H"

#ifndef MATRIX_MATH_HOST
static int probe(unsigned *scratch);
#pragma aux probe = \
    0xdb 0xe3               /* FNINIT (never FINIT/FWAIT here). */ \
    "mov ax,5a5ah" \
    "mov [si],ax" \
    0xdd 0x3c               /* FNSTSW word ptr [si]. */ \
    "cmp byte ptr [si],0" \
    "jne absent" \
    0xd9 0x3c               /* FNSTCW word ptr [si]. */ \
    "mov ax,[si]" \
    "and ax,103fh" \
    "cmp ax,003fh" \
    "jne absent" \
    "mov ax,1" \
    "jmp finished" \
    "absent: xor ax,ax" \
    "finished:" \
    parm [si] value [ax] modify exact [ax];

static void get_control(unsigned *control);
#pragma aux get_control = \
    0xd9 0x3c               /* FNSTCW word ptr [si]. */ \
    parm [si] modify exact [];

static void set_control(const unsigned *control);
#pragma aux set_control = \
    0x9b 0xd9 0x2c          /* FWAIT; FLDCW word ptr [si]. */ \
    parm [si] modify exact [];

static void clear_exceptions(void);
#pragma aux clear_exceptions = \
    0xdb 0xe2               /* FNCLEX. */ \
    modify exact [];

static void root87(const unsigned *input, unsigned *output);
#pragma aux root87 = \
    0x9b 0xdf 0x04          /* FWAIT; FILD word ptr [si]. */ \
    0x9b 0xd9 0xfa          /* FWAIT; FSQRT. */ \
    0x9b 0xdf 0x1d          /* FWAIT; FISTP word ptr [di]. */ \
    0x9b                    /* Wait for the result before the CPU reads it. */ \
    parm [si] [di] modify exact [];
#endif

int matrix_fpu_detect(void)
{
#ifdef MATRIX_MATH_HOST
    return 0;
#else
    unsigned scratch;
    return probe(&scratch);
#endif
}

/* Restoring integer square root; input is at most 1024. Convert the floor
 * root to nearest integer exactly: n > root*root + root rounds upwards. */
static unsigned root_integer(unsigned n)
{
    unsigned remainder=n, root=0, bit=1024;
    while(bit>remainder)bit>>=2;
    while(bit){
        if(remainder>=root+bit){remainder-=root+bit;root=(root>>1)+bit;}
        else root>>=1;
        bit>>=2;
    }
    if(remainder>root)root++;
    return root;
}

void matrix_glow_table(unsigned char out[MATRIX_GLOW_STEPS], int use_fpu)
{
    unsigned i,n,root;
#ifndef MATRIX_MATH_HOST
    unsigned old_control, nearest=0x037f;
    if(use_fpu){get_control(&old_control);set_control(&nearest);}
#else
    (void)use_fpu;
#endif
    for(i=0;i<MATRIX_GLOW_STEPS;i++){
        n=i*(MATRIX_GLOW_STEPS-i);
#ifndef MATRIX_MATH_HOST
        if(use_fpu)root87(&n,&root);else
#endif
        root=root_integer(n);
        out[i]=(unsigned char)((root+2)>>2);
    }
#ifndef MATRIX_MATH_HOST
    if(use_fpu){clear_exceptions();set_control(&old_control);}
#endif
}
