; ORBIT 87: original-8087 numerical kernels, small-model Watcom ABI.
; Assemble with WASM -0 -fp0 -fpi87. The -fpi87 is ESSENTIAL: otherwise
; WASM emits software-emulation INTs. -fp0 inserts the 8087 WAIT protocol.
; No 186+, 287+, FSIN/FCOS or FNSTSW AX instructions are used.
; Every numerical kernel returns with an empty coprocessor stack.
.8086
.8087
.model small
.code
public probe87_, control87_, setcontrol87_, finish87_
public project87_, gravity87_, attractor87_

; SI -> two words. No WAIT may execute until presence is established.
; Intel 80386 Hardware Reference (1987), Fig. 5-4 covers 8086/8088+8087.
; The two MOVs give the original 8087 time to initialize. FNSTSW/FNSTCW
; memory stores synchronize through its bus interface, even without WAIT.
probe87_ proc near
    fnstcw word ptr [si+2]
    fninit
    mov ax,5a5ah
    mov [si],ax
    fnstsw word ptr [si]
    cmp byte ptr [si],0
    jne probe_absent
    fnstcw word ptr [si]
    mov ax,[si]
    and ax,103fh
    cmp ax,003fh
    jne probe_absent
    fldcw word ptr [si+2]
    mov ax,1
    ret
probe_absent:
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
    ret
setcontrol87_ endp
finish87_ proc near
    fclex
    fldcw word ptr [si]
    ret
finish87_ endp

; SI -> Vec3, DI -> Vec2, BX -> Project87 (layout documented in MATH.C).
; Matrix coefficients are Q14. Keeping all three dot products unrounded
; through the perspective divide avoids the integer path's vertex wobble.
project87_ proc near
    fild word ptr [si]
    fimul word ptr [bx+12]
    fild word ptr [si+2]
    fimul word ptr [bx+14]
    faddp st(1),st
    fild word ptr [si+4]
    fimul word ptr [bx+16]
    faddp st(1),st
    fiadd dword ptr [bx+24]
    fst qword ptr [bx+28]
    fidiv word ptr [bx+22]
    fistp word ptr [bx+36]
    wait
    cmp word ptr [bx+36],16
    jle project_hidden
    fild word ptr [si]
    fimul word ptr [bx]
    fild word ptr [si+2]
    fimul word ptr [bx+2]
    faddp st(1),st
    fild word ptr [si+4]
    fimul word ptr [bx+4]
    faddp st(1),st
    fimul word ptr [bx+18]
    fdiv qword ptr [bx+28]
    fistp word ptr [di]
    fild word ptr [si]
    fimul word ptr [bx+6]
    fild word ptr [si+2]
    fimul word ptr [bx+8]
    faddp st(1),st
    fild word ptr [si+4]
    fimul word ptr [bx+10]
    faddp st(1),st
    fimul word ptr [bx+18]
    fdiv qword ptr [bx+28]
    fistp word ptr [di+2]
    wait
    mov byte ptr [di+4],1
    ret
project_hidden:
    mov word ptr [di],0
    mov word ptr [di+2],0
    mov byte ptr [di+4],0
    ret
project87_ endp

; SI -> integer position, DI -> Q12 acceleration Vec3.
; A = -GM * P / (dot(P,P)+softening^2)^(3/2), a Plummer potential.
; The square root and reciprocal are genuinely evaluated every step.
gravity87_ proc near
    fild word ptr [si]
    fimul word ptr [si]
    fild word ptr [si+2]
    fimul word ptr [si+2]
    faddp st(1),st
    fild word ptr [si+4]
    fimul word ptr [si+4]
    faddp st(1),st
    fiadd word ptr [bx+4]
    fst qword ptr [bx+8]
    fsqrt
    fmul qword ptr [bx+8]
    fidivr dword ptr [bx]
    fst qword ptr [bx+16]
    fimul word ptr [si]
    fistp word ptr [di]
    fld qword ptr [bx+16]
    fimul word ptr [si+2]
    fistp word ptr [di+2]
    fld qword ptr [bx+16]
    fimul word ptr [si+4]
    fistp word ptr [di+4]
    wait
    ret
gravity87_ endp

; SI -> Lorenz87 state/constants, DI -> Vec3. Explicit Euler dt=0.006.
; All three increments use OLD x,y,z before the state is updated.
attractor87_ proc near
    fld qword ptr [si+8]
    fsub qword ptr [si]
    fmul qword ptr [si+24]
    fstp qword ptr [si+48]
    fld qword ptr [si+32]
    fsub qword ptr [si+16]
    fmul qword ptr [si]
    fsub qword ptr [si+8]
    fmul qword ptr [si+40]
    fstp qword ptr [si+56]
    fld qword ptr [si]
    fmul qword ptr [si+8]
    fld qword ptr [si+16]
    fmul qword ptr [si+64]
    fsubp st(1),st
    fmul qword ptr [si+40]
    fstp qword ptr [si+72]
    fld qword ptr [si]
    fadd qword ptr [si+48]
    fst qword ptr [si]
    fimul word ptr [si+80]
    fistp word ptr [di]
    fld qword ptr [si+8]
    fadd qword ptr [si+56]
    fst qword ptr [si+8]
    fimul word ptr [si+80]
    fistp word ptr [di+2]
    fld qword ptr [si+16]
    fadd qword ptr [si+72]
    fst qword ptr [si+16]
    fisub word ptr [si+82]
    fimul word ptr [si+80]
    fistp word ptr [di+4]
    wait
    ret
attractor87_ endp
end
