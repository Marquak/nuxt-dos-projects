; Bounded, already-clipped Bresenham lines in the 320x200 RAM frame.
; AL colour, AH scanline bit, BX tile, DI pixel, DX error, CX remaining.
; Background indices >=192 protect the car. Interrupts stay enabled.
.186
.model small
.code
public fast320_line_asm_
fast320_line_asm_ proc near
    push bp
    push ds
    push es
    push word ptr [si+20] ; y-major
    push word ptr [si+14] ; minor delta
    push word ptr [si+12] ; major delta
    push word ptr [si+10] ; x tile boundary (0 forward, 7 backward)
    push word ptr [si+8]  ; y step in pixel bytes
    push word ptr [si+6]  ; x step
    mov bp,sp
    mov di,[si]
    mov bx,[si+2]
    mov cx,[si+12]
    mov dx,cx
    shr dx,1
    inc cx
    mov ax,[si+16]
    mov es,ax
    mov ax,[si+4]
    push ax
    mov ax,[si+18]
    mov ds,ax
    pop ax
    cmp word ptr [bp+10],0
    jne ypixel
xpixel:
    cmp byte ptr [di],192
    jae xskip
    mov es:[di],al
    or es:[bx+64000],ah
xskip:
    dec cx
    jz finished
    add di,[bp]
    mov si,di
    and si,7
    cmp si,[bp+4]
    jne xnowrap
    add bx,[bp]
xnowrap:
    sub dx,[bp+8]
    jns xpixel
    add dx,[bp+6]
    add di,[bp+2]
    cmp word ptr [bp+2],0
    jl xup
    rol ah,1
    jnc xpixel
    add bx,40
    jmp xpixel
xup:
    ror ah,1
    jnc xpixel
    sub bx,40
    jmp xpixel
ypixel:
    cmp byte ptr [di],192
    jae yskip
    mov es:[di],al
    or es:[bx+64000],ah
yskip:
    dec cx
    jz finished
    add di,[bp+2]
    cmp word ptr [bp+2],0
    jl yup
    rol ah,1
    jnc yminor
    add bx,40
    jmp short yminor
yup:
    ror ah,1
    jnc yminor
    sub bx,40
yminor:
    sub dx,[bp+8]
    jns ypixel
    add dx,[bp+6]
    add di,[bp]
    mov si,di
    and si,7
    cmp si,[bp+4]
    jne ynowrap
    add bx,[bp]
ynowrap:
    jmp ypixel
finished:
    add sp,12
    pop es
    pop ds
    pop bp
    ret
fast320_line_asm_ endp

; Q11 scale/depth lookup. DS input/output, ES far table, BP table offset.
; Caller uses this only for bounded road coordinates / depths <2048.
public road_lookup_
road_lookup_ proc near
    push bp
    push es
    mov es,dx
    mov bp,bx
road_next:
    mov bx,[si+4]
    cmp bx,56
    jle road_hidden
    shl bx,1
    add bx,bp
    mov bx,es:[bx]
    mov ax,[si]
    imul bx
    add ax,1024
    adc dx,0
    shr ax,11
    shl dx,5
    or ax,dx
    mov [di],ax
    mov ax,[si+2]
    imul bx
    add ax,1024
    adc dx,0
    shr ax,11
    shl dx,5
    or ax,dx
    mov [di+2],ax
    mov byte ptr [di+4],1
    jmp short road_advance
road_hidden:
    mov word ptr [di],0
    mov word ptr [di+2],0
    mov byte ptr [di+4],0
road_advance:
    add si,6
    add di,6
    loop road_next
    pop es
    pop bp
    ret
road_lookup_ endp

; Convert one bit of 40 dirty masks into at most 20 start/length pairs.
; SI masks, AL bit, DI near output; AX number of pairs. No segment changes.
public fast320_runs_
fast320_runs_ proc near
    xor bx,bx
    xor dx,dx
runs_seek:
    cmp bx,40
    jae runs_done
    test [si+bx],al
    jnz runs_start
    inc bx
    jmp short runs_seek
runs_start:
    mov cx,bx
    shl cx,3
    mov [di],cx
    add di,2
runs_more:
    inc bx
    cmp bx,40
    jae runs_end
    test [si+bx],al
    jnz runs_more
runs_end:
    mov cx,bx
    shl cx,3
    sub cx,[di-2]
    mov [di],cx
    add di,2
    inc dx
    jmp short runs_seek
runs_done:
    mov ax,dx
    ret
fast320_runs_ endp

; SI near masks[40], BL row bit, DI row offset, AX source seg, DX dest seg.
; Coalesce neighbouring dirty cells and copy aligned words. No VGA reads,
; no CLI, no extra framebuffer. AX returns the exact copied byte count.
public fast320_blitrow_
fast320_blitrow_ proc near
    push bp
    push ds
    push es
    push ax                ; source segment
    push si                ; mask start
    push bx                ; row bit
    mov bp,sp
    mov es,dx
    xor dx,dx              ; total words copied
    mov bx,si
    add bx,40              ; mask end, exclusive
    cld
blit_seek:
    cmp si,bx
    jae blit_done
    mov al,[bp]
    test [si],al
    jnz blit_start
    inc si
    add di,8
    jmp short blit_seek
blit_start:
    xor cx,cx
blit_more:
    inc si
    add cx,4               ; four words per dirty 8-pixel cell
    cmp si,bx
    jae blit_copy
    test [si],al
    jnz blit_more
blit_copy:
    add dx,cx
    push si
    push ds
    mov ds,[bp+4]
    mov si,di
    rep movsw
    pop ds
    pop si
    jmp short blit_seek
blit_done:
    mov ax,dx
    shl ax,1
    add sp,6
    pop es
    pop ds
    pop bp
    ret
fast320_blitrow_ endp

; Gather one tile row with segments loaded once, not per C far access.
; AX old segment, DX new segment (0 => old only), SI offset, DI masks[40].
; Return AL union of row bits and AH number of nonempty new tiles (0..40).
public fast320_masks_
fast320_masks_ proc near
    push es
    mov es,ax
    mov bx,40
    xor cx,cx
mask_old:
    mov al,es:[si]
    mov [di],al
    or cl,al
    inc si
    inc di
    dec bx
    jnz mask_old
    or dx,dx
    jz masks_done
    mov es,dx
    sub si,40
    sub di,40
    mov bx,40
mask_new:
    mov al,es:[si]
    or [di],al
    or cl,al
    or al,al
    jz mask_empty
    inc ch
mask_empty:
    inc si
    inc di
    dec bx
    jnz mask_new
masks_done:
    mov ax,cx
    pop es
    ret
fast320_masks_ endp
end
