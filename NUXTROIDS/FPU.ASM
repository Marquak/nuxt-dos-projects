; Original 8087 instructions, shared project probe from STARFIELD.
.8086
.8087
.model small
.code
public probe87_, rotate87_, done87_
; SI -> scratch status/CW. No WAIT until detection succeeds.
probe87_ proc near
 fnstcw word ptr [si+2]
 fninit
 mov word ptr [si],5a5ah
 fnstsw word ptr [si]
 cmp byte ptr [si],0
 jne absent87
 fnstcw word ptr [si]
 mov ax,[si]
 and ax,103fh
 cmp ax,003fh
 jne absent87
 mov word ptr [si],037fh
 fldcw word ptr [si]
 mov ax,1
 ret
absent87:
 xor ax,ax
 ret
probe87_ endp
; SI -> x,y,cos,sin,outx,outy. Q14 coefficients; native pixel output.
; Explicit waits protect 8087 memory operands and store completion.
rotate87_ proc near
 wait
 fild word ptr [si]
 fimul word ptr [si+4]
 fild word ptr [si+2]
 fimul word ptr [si+6]
 fsubp st(1),st
 fidiv word ptr cs:scale87
 fistp word ptr [si+8]
 fild word ptr [si]
 fimul word ptr [si+6]
 fild word ptr [si+2]
 fimul word ptr [si+4]
 faddp st(1),st
 fidiv word ptr cs:scale87
 fistp word ptr [si+10]
 wait
 ret
rotate87_ endp
; SI -> saved caller control word; stack empty after geometry preparation.
done87_ proc near
 fclex
 fldcw word ptr [si]
 wait
 ret
done87_ endp
scale87 dw 16384
end
