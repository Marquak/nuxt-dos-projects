; Original V20 planar VGA renderer. The caller bounds endpoints.
; Each line excludes its final pixel so joined XOR polygon edges meet cleanly.
.186
.model small
.data
rows dw 600 dup(0)
stride dw 80
vbytes dw 38400
ystep dw 0
xdir dw 0
cached dw 0ffffh
joybegin dw 0
joyflags db 0
joybits db 0
.code
public video_init_, ink_, raster_, wipe_, dot_, segment_
public clearcolumn_
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
clearcolumn_ proc near ; DI byte offset, CX rows; caller sets GC bit mask.
 push es
 mov ax,0a000h
 mov es,ax
cc_loop:
 mov al,es:[di] ; load ALL four latches before a masked write
 mov byte ptr es:[di],0
 add di,80
 loop cc_loop
 pop es
 ret
clearcolumn_ endp
video_init_ proc near ; AX=bytes per row, BX=height
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
