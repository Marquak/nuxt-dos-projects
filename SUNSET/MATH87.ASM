; SUNSET DRIVE: original-8087 batch matrix/perspective projection.
; WASM -0 -fp0 -fpi87 is required. -fp0 emits 8087 WAIT protocol;
; -fpi87 prevents WASM from replacing x87 opcodes with emulator INTs.
; No 186+, 287+, FSIN, FCOS or FNSTSW AX instructions occur here.
; All numerical paths return with an empty x87 operand stack.
.8086
.8087
.model small
.code
public probe87_, control87_, setcontrol87_, finish87_, project87_, direct87_

; SI -> two words. No WAIT until a coprocessor is established present.
; This is the proven ORBIT87 probe (Intel 80386 Hardware Reference,
; Figure 5-4, covering 8086/8088+8087). Non-waiting memory stores use
; the original coprocessor bus interface. Save/restore caller's CW.
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

; SI -> Vec3[count], DI -> Vec2[count], CX = count, BX -> Project87.
; Vec3 and Vec2 strides are both 6 bytes in the DOS small model.
; Compute exact Q14 dot products in extended precision, clip against
; 56*16384, then reuse scale/Z for both screen coordinates. There is
; one perspective divide per live vertex and one call per entire batch.
project87_ proc near
    push ax
    push cx
    push si
    push di
    or cx,cx
    jnz project_next
    jmp project_done
project_next:
    fild word ptr [si]
    fimul word ptr [bx+12]
    fild word ptr [si+2]
    fimul word ptr [bx+14]
    faddp st(1),st
    fild word ptr [si+4]
    fimul word ptr [bx+16]
    faddp st(1),st
    fiadd dword ptr [bx+20]
    fcom qword ptr [bx+24]
    fstsw word ptr [bx+32]
    wait
    mov ax,[bx+32]
    sahf
    jbe project_hidden
    fidivr word ptr [bx+18]   ; ST(0) = scale / Zq14
    fild word ptr [si]
    fimul word ptr [bx]
    fild word ptr [si+2]
    fimul word ptr [bx+2]
    faddp st(1),st
    fild word ptr [si+4]
    fimul word ptr [bx+4]
    faddp st(1),st
    fmul st,st(1)
    fistp word ptr [di]      ; leave reciprocal on stack
    fild word ptr [si]
    fimul word ptr [bx+6]
    fild word ptr [si+2]
    fimul word ptr [bx+8]
    faddp st(1),st
    fild word ptr [si+4]
    fimul word ptr [bx+10]
    faddp st(1),st
    fmul st,st(1)
    fistp word ptr [di+2]
    fstp st(0)              ; discard reciprocal; stack empty
    wait
    mov byte ptr [di+4],1
    jmp short project_advance
project_hidden:
    fstp st(0)              ; discard clipped Z; stack empty
    mov word ptr [di],0
    mov word ptr [di+2],0
    mov byte ptr [di+4],0
project_advance:
    add si,6
    add di,6
    dec cx
    jz project_done
    jmp project_next
project_done:
    wait
    pop di
    pop si
    pop cx
    pop ax
    ret
project87_ endp

; Identity camera / zero distance fast path used by the endless road.
; CPU clips integer depth while the original 8087 computes scale/Z once,
; then projects both coordinates. No redundant zero/identity products.
direct87_ proc near
    push cx
    push si
    push di
    or cx,cx
    jz direct_done
direct_next:
    cmp word ptr [si+4],56
    jle direct_hidden
    fild word ptr [si+4]
    fidivr word ptr [bx+18]
    fild word ptr [si]
    fmul st,st(1)
    fistp word ptr [di]
    fild word ptr [si+2]
    fmul st,st(1)
    fistp word ptr [di+2]
    fstp st(0)
    mov byte ptr [di+4],1
    jmp short direct_advance
direct_hidden:
    mov word ptr [di],0
    mov word ptr [di+2],0
    mov byte ptr [di+4],0
direct_advance:
    add si,6
    add di,6
    loop direct_next
direct_done:
    wait
    pop di
    pop si
    pop cx
    ret
direct87_ endp

; Build the 57..2047 depth factors once. SI points to {long numerator,
; short depth}; DX:DI is the far output table starting at entry 57.
; All opcodes are original-8087; stack is empty on return.
public depth_table87_
depth_table87_ proc near
    push es
    mov es,dx
    mov word ptr [si+4],57
    mov cx,1991
    fild dword ptr [si]
depth_next:
    fld st(0)
    fidiv word ptr [si+4]
    fistp word ptr es:[di]
    add di,2
    inc word ptr [si+4]
    loop depth_next
    fstp st(0)
    wait
    pop es
    ret
depth_table87_ endp
end
