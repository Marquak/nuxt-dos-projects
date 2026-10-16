; Detection from our ORBIT87/STARFIELD code. Original 8087 instructions.
; No emulation interrupt fixups: assemble with -fp0 -fpi87.
.8086
.8087
.model small
.code
public probe87_, control87_, setcontrol87_, divide87_
probe87_ proc near
 fnstcw word ptr [si+2]
 fninit
 mov ax,5a5ah
 mov [si],ax
 fnstsw word ptr [si]
 cmp byte ptr [si],0
 jne absent
 fnstcw word ptr [si]
 mov ax,[si]
 and ax,103fh
 cmp ax,003fh
 jne absent
 fldcw word ptr [si+2]
 mov ax,1
 ret
absent:
 xor ax,ax
 ret
probe87_ endp
control87_ proc near
 fstcw word ptr [si]
 wait
 ret
control87_ endp
setcontrol87_ proc near
 fldcw word ptr [si]
 wait
 ret
setcontrol87_ endp
; DS:SI -> {long numerator; int denominator; int result;}
divide87_ proc near
 fild dword ptr [si]
 fidiv word ptr [si+4]
 fistp word ptr [si+6]
 wait
 mov ax,[si+6]
 ret
divide87_ endp
end
