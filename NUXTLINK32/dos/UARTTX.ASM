; Fill at most one empty TX FIFO. Caller checks THRE, timeout and ESC.
; DX=base, DS:SI=data, CX=1..16 (1 without FIFO), BX=required MSR bits.
; Return AX=bytes queued. Check peer lines before every byte; leave IF alone.
.8086
.model tiny
.code
public uart_tx_fill_
uart_tx_fill_ proc near
    xor di,di
    jcxz tx_done
tx_byte:
    or bl,bl
    jz tx_ready
    add dx,6
    in al,dx
    and al,bl
    cmp al,bl
    jne tx_blocked
    sub dx,6
tx_ready:
    mov al,[si]
    out dx,al
    inc si
    inc di
    loop tx_byte
    jmp short tx_done
tx_blocked:
    sub dx,6
tx_done:
    mov ax,di
    ret
uart_tx_fill_ endp
end
