; Bounded heatshrink block decoder: window=12, lookahead=4, MSB first.
; Format reference: Scott Vokes' heatshrink (third_party/heatshrink/LICENSE).
; Independent blocks <=4096 bytes let the output buffer serve as history.
; Initial history is zero, including references before the first literal.
; SI=input, CX=packed length, DI=output, AX=exact decoded length.
; Returns AX=1 valid / 0 invalid. Preserves BP, ES, DS, IF and DF.
.8086
.model tiny
.code
public hs_decode_
hs_decode_ proc near
    push bp
    mov bp,sp
    push es
    pushf
    sub sp,6
    push ds
    pop es
    cld
    or ax,ax
    jz hs_bad
    cmp ax,4096
    ja hs_bad
    add ax,di
    jc hs_bad
    mov [bp-10],ax             ; output end
    mov [bp-8],di              ; output start
    mov ax,si
    add ax,cx
    jc hs_bad
    mov [bp-6],ax              ; input end
    xor dx,dx                 ; DL=shift register, DH=bits left
hs_token:
    cmp di,[bp-10]
    je hs_finish
    call hs_bit
    jnc hs_reference
    mov cx,8
    call hs_bits
    mov al,bl
    stosb
    jmp short hs_token
hs_reference:
    mov cx,12
    call hs_bits
    inc bx                    ; distance 1..4096
    push bx
    mov cx,4
    call hs_bits
    mov cx,bx
    inc cx                    ; length 1..16
    pop bx
    mov ax,[bp-10]
    sub ax,di
    cmp cx,ax
    ja hs_bad                 ; never overrun the advertised output
    mov ax,di
    sub ax,[bp-8]              ; bytes already produced
    cmp bx,ax
    jbe hs_copy
    sub bx,ax                 ; number of leading prehistory zeroes
    cmp bx,cx
    jbe hs_zeroes
    mov bx,cx
hs_zeroes:
    sub cx,bx
    push cx
    mov cx,bx
    xor ax,ax
    rep stosb
    pop cx
    jcxz hs_token
    push si
    mov si,[bp-8]              ; prehistory ends at output start
    rep movsb
    pop si
    jmp short hs_token
hs_copy:
    push si
    mov si,di
    sub si,bx
    rep movsb                 ; forward copying permits overlapping matches
    pop si
    jmp short hs_token
hs_finish:
    cmp si,[bp-6]
    jne hs_bad
    or dl,dl                  ; only zero bit padding, no extra bytes
    jne hs_bad
    mov ax,1
    jmp short hs_return
hs_bad:
    xor ax,ax
hs_return:
    lea sp,[bp-4]             ; also unwinds hs_bit on truncated input
    popf
    pop es
    pop bp
    ret
hs_bits:
    xor bx,bx
hs_bits_loop:
    call hs_bit
    rcl bx,1
    loop hs_bits_loop
    ret
hs_bit:
    or dh,dh
    jnz hs_have_byte
    cmp si,[bp-6]
    jae hs_bad
    mov dl,[si]
    inc si
    mov dh,8
hs_have_byte:
    dec dh
    add dl,dl                 ; CF is the next input bit
    ret
hs_decode_ endp
end
