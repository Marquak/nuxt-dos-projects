; Original V20 planar VGA renderer. The caller bounds endpoints.
; Each line excludes its final pixel so joined XOR polygon edges meet cleanly.
.186
.model small
.data
rows dw 600 dup(0)
stride dw 80
vbytes dw 38400
vheight dw 480
ystep dw 0
xdir dw 0
wrapcol dw 0
wrapwidth dw 0
wraptop dw 0
wrapoffset dw 0
wrapbytes dw 0
cached dw 0ffffh
joybegin dw 0
joyflags db 0
joybits db 0
.code
public video_init_, ink_, raster_, wipe_, dot_, segment_
public hudpatch_
; DI=video offset, DS:SI=seven left-to-right 16-bit XOR masks,
; CX=vertical repeat (1 or 2). No video access for unchanged byte masks.
; Caller selects colour planes and XOR raster operation. Preserve both
; plane latches and neighbouring pixels by reading before each byte write.
hudpatch_ proc near
 push bp
 push es
 mov ax,0a000h
 mov es,ax
 mov dx,cx
 mov bx,7
 cld
hp_row:
 lodsw
 mov bp,ax
 mov cx,dx
hp_repeat:
 mov ax,bp
 or ah,ah
 jz hp_right
 mov al,es:[di]
 mov es:[di],ah
hp_right:
 mov ax,bp
 or al,al
 jz hp_next
 mov ah,es:[di+1]
 mov es:[di+1],al
hp_next:
 add di,stride
 loop hp_repeat
 dec bx
 jnz hp_row
 pop es
 pop bp
 ret
hudpatch_ endp
; Sparse pre-rasterized asteroid XOR masks. DX:SI=source pairs
; (video-relative offset, high-byte pixel mask), AX=base, CX=shift, BX=count.
; Preserve caller DS, ES, BP; timer/keyboard ISRs supply their own DS.
public outline_
outline_ proc near
 push bp
 push ds
 push es
 mov bp,ax
 mov ax,0a000h
 mov es,ax
 mov ds,dx
 cld
 or bx,bx
 jz ol_done
ol_loop:
 lodsw
 mov di,ax
 add di,bp
 lodsw
 mov al,es:[di]
 mov es:[di],ah
 dec bx
 jnz ol_loop
ol_done:
 pop es
 pop ds
 pop bp
 ret
outline_ endp
public outline_wrapped_
ow_stride dw 0
ow_top dw 0
ow_end dw 0
ow_size dw 0
; Same sparse masks, with source column stored in the unused low mask byte.
; CL=signed left column, CH=stride. Wrap byte addresses without line drawing.
outline_wrapped_ proc near
 push bp
 push ds
 push es
 mov bp,ax
 mov ax,0a000h
 mov es,ax
 mov ds,dx
 cld
 or bx,bx
 jz ow_done
 ow_loop:
 lodsw
 mov di,ax
 add di,bp
 lodsw
 add al,cl
 js ow_left
 cmp al,ch
 jb ow_vertical
 sub di,cs:ow_stride
 jmp short ow_vertical
 ow_left:
 add di,cs:ow_stride
 ow_vertical:
 cmp di,cs:ow_top
 jb ow_above
 cmp di,cs:ow_end
 jb ow_write
 sub di,cs:ow_size
 jmp short ow_write
 ow_above:
 add di,cs:ow_size
 ow_write:
 mov al,es:[di]
 mov es:[di],ah
 dec bx
 jnz ow_loop
 ow_done:
 pop es
 pop ds
 pop bp
 ret
outline_wrapped_ endp
public difference_
; SI -> cache segment, old pointer/count/base, new pointer/count/base.
; Merge sorted old/new byte masks in RAM. Equal bits cancel before VGA access.
; SS locals remain accessible after changing DS to the far cache segment.
difference_ proc near
 push bp
 mov bp,sp
 sub sp,10
 push ds
 push es
 mov ax,[si+4]
 mov [bp-2],ax
 mov ax,[si+6]
 mov [bp-4],ax
 mov ax,[si+10]
 mov [bp-6],ax
 mov ax,[si+12]
 mov [bp-8],ax
 mov bx,[si+8]
 mov dx,[si]
 mov si,[si+2]
 mov ax,0a000h
 mov es,ax
 mov ds,dx
 dif_loop:
 cmp word ptr [bp-2],0
 je dif_new_tail
 cmp word ptr [bp-6],0
 je dif_old_tail
 mov ax,[si]
 add ax,[bp-4]
 mov dx,[bx]
 add dx,[bp-8]
 cmp ax,dx
 jb dif_old
 ja dif_new
 mov di,ax
 mov ax,[si+2]
 xor ax,[bx+2]
 add si,4
 add bx,4
 dec word ptr [bp-2]
 dec word ptr [bp-6]
 or ah,ah
 jz dif_loop
 jmp short dif_write
 dif_old_tail:
 cmp word ptr [bp-2],0
 je dif_done
 mov ax,[si]
 add ax,[bp-4]
 dif_old:
 mov di,ax
 mov ax,[si+2]
 add si,4
 dec word ptr [bp-2]
 jmp short dif_write
 dif_new_tail:
 cmp word ptr [bp-6],0
 je dif_done
 mov dx,[bx]
 add dx,[bp-8]
 dif_new:
 mov di,dx
 mov ax,[bx+2]
 add bx,4
 dec word ptr [bp-6]
 dif_write:
 mov al,es:[di]
 mov es:[di],ah
 jmp dif_loop
 dif_done:
 pop es
 pop ds
 mov sp,bp
 pop bp
 ret
difference_ endp
video_init_ proc near ; AX=bytes per row, BX=height
 mov vheight,bx
 mov stride,ax
 mov cx,bx
 mov bx,offset rows
 xor dx,dx
vi_loop:
 mov [bx],dx
 add bx,2
 add dx,ax
 loop vi_loop
 mov vbytes,dx
 mov cached,0ffffh
 ret
video_init_ endp
ink_ proc near ; AX=plane mask (XOR colour)
 cmp ax,cached
 je ink_done
 mov cached,ax
 mov ah,al
 mov al,2
 mov dx,03c4h
 out dx,ax
ink_done:
 ret
ink_ endp
raster_ proc near ; AX=0 replace, 10h OR, 18h XOR
 mov ah,al
 mov al,3
 mov dx,03ceh
 out dx,ax
 ret
raster_ endp
wipe_ proc near
 push es
 mov ax,0a000h
 mov es,ax
 mov dx,03c4h
 mov ax,0f02h
 out dx,ax
 mov cached,15
 mov dx,03ceh
 mov ax,0003h
 out dx,ax
 xor di,di
 mov cx,vbytes
 xor ax,ax
 cld
 rep stosb
 mov ax,1803h
 out dx,ax
 pop es
 ret
wipe_ endp
dot_ proc near ; AX=x, BX=physical y
 push es
 shl bx,1
 mov di,rows[bx]
 mov bx,ax
 shr bx,3
 add di,bx
 and ax,7
 mov cl,al
 mov ah,80h
 shr ah,cl
 mov bx,0a000h
 mov es,bx
 mov al,es:[di]
 mov es:[di],ah
 pop es
 ret
dot_ endp
segment_ proc near ; AX=x1 BX=y1 CX=x2 DX=y2
 push bp
 push es
 mov si,bx
 shl si,1
 mov di,rows[si]
 mov si,ax
 shr si,3
 add di,si
 sub cx,ax
 mov xdir,1
 jns seg_xpos
 neg cx
 mov xdir,-1
seg_xpos:
 sub dx,bx
 mov si,stride
 jns seg_ypos
 neg dx
 neg si
seg_ypos:
 mov ystep,si
 mov si,cx ; abs x delta
 mov bx,dx ; abs y delta
 and ax,7
 mov cl,al
 mov ah,80h
 shr ah,cl
 mov dx,0a000h
 mov es,dx
 cmp si,bx
 jb seg_ymajor
 mov dx,si ; major
 mov si,bx ; minor
 mov cx,dx
 or cx,cx
 jnz seg_xcount
 inc cx
seg_xcount:
 mov bp,dx
 shr bp,1
 cmp xdir,0
 jl seg_xminus
seg_xplus:
 mov al,es:[di]
 mov es:[di],ah
 sub bp,si
 jns seg_xp_noy
 add di,ystep
 add bp,dx
seg_xp_noy:
 ror ah,1
 jnc seg_xp_nobyte
 inc di
seg_xp_nobyte:
 loop seg_xplus
 jmp seg_done
seg_xminus:
 mov al,es:[di]
 mov es:[di],ah
 sub bp,si
 jns seg_xm_noy
 add di,ystep
 add bp,dx
seg_xm_noy:
 rol ah,1
 jnc seg_xm_nobyte
 dec di
seg_xm_nobyte:
 loop seg_xminus
 jmp seg_done
seg_ymajor:
 mov dx,bx
 mov cx,dx
 mov bp,dx
 shr bp,1
 cmp xdir,0
 jl seg_yminus
seg_yplus:
 mov al,es:[di]
 mov es:[di],ah
 add di,ystep
 sub bp,si
 jns seg_yp_nox
 add bp,dx
 ror ah,1
 jnc seg_yp_nox
 inc di
seg_yp_nox:
 loop seg_yplus
 jmp seg_done
seg_yminus:
 mov al,es:[di]
 mov es:[di],ah
 add di,ystep
 sub bp,si
 jns seg_ym_nox
 add bp,dx
 rol ah,1
 jnc seg_ym_nox
 dec di
seg_ym_nox:
 loop seg_yminus
seg_done:
 pop es
 pop bp
 ret
segment_ endp

; Seam checks occur only on a scanline/byte change. No C call per pixel.
wrap_y macro
 local above,done
 add di,ystep
 cmp di,wrapoffset
 jb above
 cmp di,vbytes
 jb done
 sub di,wrapbytes
 jmp done
above:
 add di,wrapbytes
done:
endm
wrap_right macro
 local done
 inc di
 inc wrapcol
 mov al,byte ptr wrapcol
 cmp al,byte ptr stride
 jb done
 mov wrapcol,0
 sub di,stride
done:
endm
wrap_left macro
 local done
 dec di
 dec wrapcol
 jns done
 push ax
 mov ax,stride
 dec ax
 mov wrapcol,ax
 pop ax
 add di,stride
done:
endm
public wrap_init_, wrapped_
wrap_init_ proc near ; AX=first playfield scanline
 mov wraptop,ax
 shl ax,1
 mov bx,ax
 mov ax,rows[bx]
 mov wrapoffset,ax
 mov cs:ow_top,ax
 mov bx,vbytes
 mov cs:ow_end,bx
 sub bx,ax
 mov wrapbytes,bx
 mov cs:ow_size,bx
 mov ax,stride
 mov cs:ow_stride,ax
 shl ax,3
 mov wrapwidth,ax
 ret
wrap_init_ endp
wrapped_ proc near ; AX=x1 BX=y1 CX=x2 DX=y2
 ; Shift both endpoints together so the starting pixel is in the playfield.
 ; Keep the original signed deltas, including when both edges wrap.
 sub cx,ax
 sub dx,bx
 cmp ax,0
 jge wr_left_ok
 add ax,wrapwidth
wr_left_ok:
 cmp ax,wrapwidth
 jl wr_right_ok
 sub ax,wrapwidth
wr_right_ok:
 cmp bx,wraptop
 jge wr_top_ok
 add bx,vheight
 sub bx,wraptop
wr_top_ok:
 cmp bx,vheight
 jl wr_bottom_ok
 sub bx,vheight
 add bx,wraptop
wr_bottom_ok:
 add cx,ax
 add dx,bx
 push bp
 push es
 mov si,bx
 shl si,1
 mov di,rows[si]
 mov si,ax
 shr si,3
 add di,si
 mov wrapcol,si
 sub cx,ax
 mov xdir,1
 jns wr_xpos
 neg cx
 mov xdir,-1
wr_xpos:
 sub dx,bx
 mov si,stride
 jns wr_ypos
 neg dx
 neg si
wr_ypos:
 mov ystep,si
 mov si,cx ; abs x delta
 mov bx,dx ; abs y delta
 and ax,7
 mov cl,al
 mov ah,80h
 shr ah,cl
 mov dx,0a000h
 mov es,dx
 cmp si,bx
 jb wr_ymajor
 mov dx,si ; major
 mov si,bx ; minor
 mov cx,dx
 or cx,cx
 jnz wr_xcount
 inc cx
wr_xcount:
 mov bp,dx
 shr bp,1
 cmp xdir,0
 jl wr_xminus
wr_xplus:
 mov al,es:[di]
 mov es:[di],ah
 sub bp,si
 jns wr_xp_noy
 wrap_y
 add bp,dx
wr_xp_noy:
 ror ah,1
 jnc wr_xp_nobyte
 wrap_right
wr_xp_nobyte:
 loop wr_xplus
 jmp wr_done
wr_xminus:
 mov al,es:[di]
 mov es:[di],ah
 sub bp,si
 jns wr_xm_noy
 wrap_y
 add bp,dx
wr_xm_noy:
 rol ah,1
 jnc wr_xm_nobyte
 wrap_left
wr_xm_nobyte:
 loop wr_xminus
 jmp wr_done
wr_ymajor:
 mov dx,bx
 mov cx,dx
 mov bp,dx
 shr bp,1
 cmp xdir,0
 jl wr_yminus
wr_yplus:
 mov al,es:[di]
 mov es:[di],ah
 wrap_y
 sub bp,si
 jns wr_yp_nox
 add bp,dx
 ror ah,1
 jnc wr_yp_nox
 wrap_right
wr_yp_nox:
 loop wr_yplus
 jmp wr_done
wr_yminus:
 mov al,es:[di]
 mov es:[di],ah
 wrap_y
 sub bp,si
 jns wr_ym_nox
 add bp,dx
 rol ah,1
 jnc wr_ym_nox
 wrap_left
wr_ym_nox:
 loop wr_yminus
wr_done:
 pop es
 pop bp
 ret
wrapped_ endp
; Measure both joystick axes in PIT clocks, with interrupts left enabled.
; PIT channel 0 must be running at the game's divisor 32768.
public joyread_
joyclock proc near
 xor al,al
 out 43h,al
 in al,40h
 mov ah,al
 in al,40h
 xchg al,ah
 ret
joyclock endp
joyread_ proc near ; SI points to x, y, buttons words; AX=1 valid, 0 timeout
 push bp
 mov word ptr [si],0ffffh
 mov word ptr [si+2],0ffffh
 mov word ptr [si+4],0
 mov joyflags,3
 call joyclock
 mov joybegin,ax
 mov dx,0201h
 mov al,0ffh
 out dx,al
 mov bp,4096
jr_loop:
 in al,dx
 mov joybits,al
 and al,3
 cmp al,joyflags
 je jr_time
 call joyclock
 mov bx,joybegin
 sub bx,ax
 and bx,7fffh
 test joyflags,1
 jz jr_axis_y
 test joybits,1
 jnz jr_axis_y
 mov [si],bx
 and joyflags,0feh
jr_axis_y:
 test joyflags,2
 jz jr_donecheck
 test joybits,2
 jnz jr_donecheck
 mov [si+2],bx
 and joyflags,0fdh
jr_donecheck:
 cmp joyflags,0
 je jr_success
jr_time:
 call joyclock
 mov bx,joybegin
 sub bx,ax
 and bx,7fffh
 cmp bx,2386
 jae jr_fail
 dec bp
 jnz jr_loop
jr_fail:
 xor ax,ax
 pop bp
 ret
jr_success:
 cmp bx,2386
 jae jr_fail
 mov al,joybits
 not al
 shr al,4
 and ax,3
 mov [si+4],ax
 mov ax,1
 pop bp
 ret
joyread_ endp
end
