; Original-8087 instructions only; explicit WAIT protocol.
.8086
.8087
.model small
.code
public probe87_, control87_, setcontrol87_, finish87_, table87_

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

; SI -> signed 32-bit numerator, DI -> 2048 word coefficients.
; Only depth 64..2047 is projected. Caller sets round-toward-zero.
; Compute the expensive perspective division once per depth at startup.
table87_ proc near
    push bp
    push si
    push di
    sub sp,2
    mov bp,sp
    mov word ptr [bp],64
    add di,128
    mov cx,1984
next_depth:
    fild dword ptr [si]
    fidiv word ptr [bp]
    fistp word ptr [di]
    add di,2
    inc word ptr [bp]
    loop next_depth
    wait
    add sp,2
    pop di
    pop si
    pop bp
    ret
table87_ endp
end
