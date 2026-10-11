# XT INVADERS - original DOS arcade game. GNU as, 8086 instruction set.
.intel_syntax noprefix
.code16
.arch i8086
.global _start, mem_end
.macro save
 push ax
 push bx
 push cx
 push dx
 push si
 push di
 push bp
.endm
.macro restore
 pop bp
 pop di
 pop si
 pop dx
 pop cx
 pop bx
 pop ax
.endm
.macro text x,y,color,label
 mov ax, \x
 mov dx, \y
 mov bl, \color
 mov si, OFFSET \label
 call draw_text
.endm
.section .text
_start:
 push cs
 pop ds
 mov ax, word ptr [2]
 mov bx, cs
 sub ax,bx
 mov dx,OFFSET mem_end
 add dx,15
 mov cl,4
 shr dx,cl
 cmp ax,dx
 jae memory_ok
 mov dx,OFFSET err_memory
 jmp dos_error
memory_ok:
 cli
 mov ax,cs
 mov ss,ax
 mov sp,OFFSET stack_top
 sti
 cld
 mov es,ax
 # Zero the NOLOAD data explicitly.
 mov di,OFFSET bss_start
 mov cx,OFFSET mem_end
 sub cx,di
 xor ax,ax
 rep stosb
 mov ah,0x30
 int 0x21
 cmp al,2
 jae dos_ok
 mov dx,OFFSET err_dos
 jmp dos_error
dos_ok:
 mov bx,OFFSET mem_end
 add bx,15
 mov cl,4
 shr bx,cl
 mov ah,0x4a
 int 0x21
 # /S silent, /V VGA 640x480, /E EGA 320x200, /J joystick calibration.
 xor cx,cx
 mov cl,byte ptr [0x80]
 mov si,0x81
args:
 jcxz check_video
 lodsb
 cmp al,'?'
 jne arg_sound
 mov dx,OFFSET usage
 jmp dos_message
arg_sound:
 and al,0xdf
 cmp al,'S'
 jne arg_video
 mov byte ptr [skip_sound],1
arg_video:
 cmp al,'V'
 jne arg_ega
 mov byte ptr [video_mode],0x12
arg_ega:
 cmp al,'E'
 jne arg_joy
 mov byte ptr [video_mode],0x0d
arg_joy:
 cmp al,'J'
 jne arg_next
 mov byte ptr [want_joy],1
arg_next:
 loop args
check_video:
 mov ax,0x1a00
 xor bx,bx
 int 0x10
 cmp al,0x1a
 jne check_ega
 cmp bl,7
 je video_ok
 cmp bl,8
 je video_ok
check_ega:
 cmp byte ptr [video_mode],0x12
 je video_error
 mov ah,0x12
 mov bx,0x10
 int 0x10
 cmp bl,3
 jbe video_ok
 video_error:
 mov dx,OFFSET err_video
 jmp dos_error
video_ok:
 mov ah,0x0f
 int 0x10
 and al,0x7f
 mov byte ptr [old_mode],al
 mov byte ptr [old_page],bh
 call sound_detect
 xor ax,ax
 mov al,byte ptr [video_mode]
 int 0x10
 mov ah,0x0f
 int 0x10
 and al,0x7f
 cmp al,byte ptr [video_mode]
 je mode_ok
 call restore_video
 mov dx,OFFSET err_video
 jmp dos_error
mode_ok:
 call graphics_setup
 call install_keyboard
 call install_timer
 mov ax,0x40
 mov es,ax
 mov ax,word ptr es:[0x6c]
 mov word ptr [rng],ax
 or word ptr [rng],1
 mov word ptr [last_tick],0
 mov ax,0xa000
 mov es,ax
 call title_screen
 cmp byte ptr [want_joy],0
 je main_loop
 call joy_calibrate
main_loop:
 # Atomic sleep avoids losing a timer interrupt between check and HLT.
 cli
 mov ax,word ptr [clock_ticks]
 sub ax,word ptr [last_tick]
 jnz next_tick
 sti
 hlt
 jmp main_loop
next_tick:
 sti
 # Keep fractional cadence; bound recovery to four steps after a slow frame.
 cmp ax,4
 jbe tick_ready
 mov ax,word ptr [clock_ticks]
 sub ax,4
 mov word ptr [last_tick],ax
tick_ready:
 inc word ptr [last_tick]
 inc word ptr [frame]
 call input_snapshot
 call joy_poll
 cmp byte ptr [pressed+1],0
 je check_calibration
 cmp byte ptr [state],6
 jne quit
 mov byte ptr [joy_enabled],0
 mov byte ptr [state],0
 call title_screen
 jmp frame_done
check_calibration:
 cmp byte ptr [state],6
 jne normal_input
 call joy_calibration_input
 jmp frame_done
normal_input:
 cmp byte ptr [pressed+36],0
 je check_sound_tick
 cmp byte ptr [state],0
 je begin_joy
 cmp byte ptr [state],3
 jne check_sound_tick
begin_joy:
 call joy_calibrate
 jmp frame_done
check_sound_tick:
 call sound_tick
 # Sound toggle (edge).
 cmp byte ptr [pressed+50],0
 je check_pause
 cmp byte ptr [state],0
 je mute_title
 call hud
 call toggle_sound
 call hud
 jmp check_pause
mute_title:
 call toggle_sound
 call title_screen
check_pause:
 cmp byte ptr [pressed+25],0
 jne pause_pressed
 cmp byte ptr [joy_pause_edge],0
 je dispatch
pause_pressed:
 cmp byte ptr [state],1
 je pause_game
 cmp byte ptr [state],2
 jne dispatch
 text 118,118,15,msg_pause
 mov byte ptr [state],1
 jmp dispatch
pause_game:
 mov byte ptr [state],2
 mov byte ptr [fire_pending],0
 call sound_silence
 text 118,118,15,msg_pause
dispatch:
 mov al,byte ptr [state]
 cmp al,0
 je title_input
 cmp al,3
 je title_input
 cmp al,2
 je frame_done
 cmp al,4
 je between_waves
 cmp al,5
 je dying
 call game_tick
 jmp frame_done
title_input:
 cmp byte ptr [pressed+57],0
 jne new_game
 cmp byte ptr [pressed+28],0
 jne new_game
 cmp byte ptr [joy_fire_edge],0
 je frame_done
new_game:
 mov word ptr [score],0
 mov byte ptr [lives],3
 mov byte ptr [wave],1
 call new_wave
 jmp frame_done
between_waves:
 test word ptr [frame],1
 jnz frame_done
 dec byte ptr [state_timer]
 jnz frame_done
 cmp byte ptr [wave],99
 jae wave_capped
 inc byte ptr [wave]
wave_capped:
 cmp byte ptr [lives],5
 jae wave_lives
 inc byte ptr [lives]
wave_lives:
 call new_wave
 jmp frame_done
dying:
 test word ptr [frame],1
 jnz frame_done
 dec byte ptr [state_timer]
 jnz frame_done
 cmp byte ptr [lives],0
 je end_game
 mov ax,word ptr [player_x]
 mov dx,175
 mov si,OFFSET explosion
 mov cx,2
 mov bp,10
 mov bl,12
 call sprite
 mov word ptr [player_x],152
 mov byte ptr [invulnerable],36
 mov byte ptr [player_visible],0
 mov byte ptr [state],1
 call player_draw
 jmp frame_done
end_game:
 call game_over
frame_done:
 jmp main_loop
quit:
 call remove_timer
 call remove_keyboard
 call sound_silence
 call restore_video
 mov dx,OFFSET farewell
 mov ah,9
 int 0x21
 mov ax,0x4c00
 int 0x21
restore_video:
 xor ah,ah
 mov al,byte ptr [old_mode]
 int 0x10
 mov ah,5
 mov al,byte ptr [old_page]
 int 0x10
 ret
dos_error:
 mov ah,9
 int 0x21
 mov ax,0x4c01
 int 0x21
dos_message:
 mov ah,9
 int 0x21
 mov ax,0x4c00
 int 0x21

# IRQ1 owns keyboard while playing; XT latch acknowledge + PIC EOI.
# Make edges are latched in the IRQ so quick press/release pairs survive.
install_keyboard:
 mov ax,0x3509
 int 0x21
 mov word ptr [old_irq],bx
 mov word ptr [old_irq+2],es
 mov dx,OFFSET keyboard_irq
 mov ax,0x2509
 int 0x21
 mov ax,0xa000
 mov es,ax
 ret
remove_keyboard:
 push ds
 lds dx,dword ptr [old_irq]
 mov ax,0x2509
 int 0x21
 pop ds
 ret
keyboard_irq:
 push ax
 push bx
 push ds
 push cs
 pop ds
 in al,0x60
 cmp al,0xe0
 je key_ack
 cmp al,0xe1
 je key_ack
 mov ah,1
 test al,0x80
 jz key_make
 xor ah,ah
key_make:
 and ax,0xff7f
 xor bx,bx
 mov bl,al
 test ah,ah
 jz key_store
 cmp byte ptr [keys+bx],0
 jne key_store
 mov byte ptr [key_edges+bx],1
key_store:
 mov byte ptr [keys+bx],ah
key_ack:
 in al,0x61
 mov ah,al
 or al,0x80
 out 0x61,al
 mov al,ah
 out 0x61,al
 mov al,0x20
 out 0x20,al
 pop ds
 pop bx
 pop ax
 iret
# 36.413 Hz PIT mode 2; forward alternate interrupts to the previous IRQ0.
# BIOS time therefore retains its usual 18.2065 Hz cadence. No DOS in ISR.
install_timer:
 mov ax,0x3508
 int 0x21
 mov word ptr [old_timer],bx
 mov word ptr [old_timer+2],es
 mov dx,OFFSET timer_irq
 mov ax,0x2508
 int 0x21
 cli
 mov al,0x34
 out 0x43,al
 xor al,al
 out 0x40,al
 mov al,0x80
 out 0x40,al
 sti
 mov ax,0xa000
 mov es,ax
 ret
remove_timer:
 push ds
 cli
 mov al,0x36
 out 0x43,al
 xor al,al
 out 0x40,al
 out 0x40,al
 lds dx,dword ptr [old_timer]
 mov ax,0x2508
 int 0x21
 pop ds
 sti
 ret
timer_irq:
 push ax
 inc word ptr cs:[clock_ticks]
 xor byte ptr cs:[bios_phase],1
 jz timer_chain
 mov al,0x20
 out 0x20,al
 pop ax
 iret
timer_chain:
 pop ax
 ljmp cs:[old_timer]
input_snapshot:
 save
 push es
 push cs
 pop es
 mov si,OFFSET key_edges
 mov di,OFFSET pressed
 mov cx,64
 cli
 rep movsw
 mov di,OFFSET key_edges
 mov cx,64
 xor ax,ax
 rep stosw
 sti
 pop es
 restore
 ret
random:
 mov ax,word ptr [rng]
 shr ax,1
 jnc rng_store
 xor ax,0xb400
rng_store:
 mov word ptr [rng],ax
 ret

# Standard analog-compatible PC gameport, joystick 1: X bit 0, buttons 4/5.
# PIT-based time units do not depend on CPU speed. Only the required axis
# is read, with a 2 ms time limit AND a hard loop bound for broken timers.
# Interrupts remain enabled during polling, including keyboard and IRQ0.
pit_count:
 pushf
 cli
 xor al,al
 out 0x43,al
 in al,0x40
 mov ah,al
 in al,0x40
 xchg al,ah
 popf
 ret
joy_read:
 push bx
 push cx
 push dx
 push si
 mov dx,0x201
 call pit_count
 mov si,ax
 out dx,al
 mov cx,2048
joy_wait:
 in al,dx
 test al,1
 jz joy_read_done
 # Check elapsed time regularly, without repeatedly delaying the port read.
 test cl,15
 jnz joy_continue
 call pit_count
 mov bx,si
 sub bx,ax
 and bx,0x7fff
 cmp bx,2386
 jae joy_timeout
joy_continue:
 loop joy_wait
joy_timeout:
 mov ax,0xffff
 jmp joy_read_return
joy_read_done:
 call pit_count
 sub si,ax
 and si,0x7fff
 mov ax,si
 cmp ax,2386
 jae joy_timeout
joy_read_return:
 pop si
 pop dx
 pop cx
 pop bx
 ret
joy_poll:
 save
 mov byte ptr [joy_left],0
 mov byte ptr [joy_right],0
 mov byte ptr [joy_fire],0
 mov byte ptr [joy_fire_edge],0
 mov byte ptr [joy_pause_edge],0
 cmp byte ptr [joy_enabled],0
 je joy_poll_done
 call joy_read
 mov word ptr [joy_raw],ax
 mov byte ptr [joy_valid],0
 cmp ax,0xffff
 jne joy_connected
 # Don't leave held controls stuck after an unplug. Stop polling altogether
 # after eight failures; J from title/game-over offers recalibration.
 mov byte ptr [joy_old_buttons],0
 inc byte ptr [joy_failures]
 cmp byte ptr [state],6
 je joy_poll_done
 cmp byte ptr [joy_failures],8
 jb joy_poll_done
 mov byte ptr [joy_enabled],0
 jmp joy_poll_done
joy_connected:
 mov byte ptr [joy_failures],0
 mov byte ptr [joy_valid],1
 cmp byte ptr [state],6
 je joy_get_buttons
 cmp ax,word ptr [joy_low]
 jae joy_test_right
 mov byte ptr [joy_left],1
joy_test_right:
 cmp ax,word ptr [joy_high]
 jbe joy_get_buttons
 mov byte ptr [joy_right],1
joy_get_buttons:
 mov dx,0x201
 in al,dx
 not al
 and al,0x30
 mov byte ptr [joy_buttons],al
 mov ah,byte ptr [joy_old_buttons]
 not ah
 and ah,al
 mov byte ptr [joy_old_buttons],al
 test al,0x10
 jz joy_test_edge
 mov byte ptr [joy_fire],1
joy_test_edge:
 test ah,0x10
 jz joy_test_pause
 mov byte ptr [joy_fire_edge],1
joy_test_pause:
 test ah,0x20
 jz joy_poll_done
 mov byte ptr [joy_pause_edge],1
joy_poll_done:
 restore
 ret
joy_calibrate:
 call sound_silence
 mov byte ptr [state],6
 mov byte ptr [cal_step],0
 mov byte ptr [joy_enabled],1
 mov byte ptr [joy_old_buttons],0x30
 mov byte ptr [joy_failures],0
joy_cal_screen:
 call clear_screen
 text 100,35,11,msg_cal_title
 mov si,OFFSET msg_cal_center
 cmp byte ptr [cal_step],0
 je joy_cal_prompt
 mov si,OFFSET msg_cal_left
 cmp byte ptr [cal_step],1
 je joy_cal_prompt
 mov si,OFFSET msg_cal_right
joy_cal_prompt:
 mov ax,61
 mov dx,85
 mov bl,15
 call draw_text
 text 82,112,14,msg_cal_accept
 text 58,145,8,msg_cal_cancel
 ret
joy_calibration_input:
 cmp byte ptr [pressed+28],0
 jne joy_cal_accept
 cmp byte ptr [pressed+57],0
 jne joy_cal_accept
 cmp byte ptr [joy_fire_edge],0
 je joy_cal_done
joy_cal_accept:
 cmp byte ptr [joy_valid],0
 je joy_cal_fail
 # Median of three timer measurements reduces interruption jitter.
 call joy_read
 mov bx,ax
 call joy_read
 mov cx,ax
 call joy_read
 cmp bx,cx
 jbe cal_sort2
 xchg bx,cx
cal_sort2:
 cmp cx,ax
 jbe cal_sort3
 xchg cx,ax
cal_sort3:
 cmp bx,cx
 jbe cal_median
 xchg bx,cx
cal_median:
 mov ax,cx
 cmp ax,2386
 jae joy_cal_fail
 cmp byte ptr [cal_step],0
 jne joy_cal_left
 mov word ptr [joy_center],ax
 jmp joy_cal_next
joy_cal_left:
 cmp byte ptr [cal_step],1
 jne joy_cal_right
 mov word ptr [joy_min],ax
 jmp joy_cal_next
joy_cal_right:
 mov bx,word ptr [joy_center]
 sub ax,bx
 jc joy_cal_fail
 cmp ax,32
 jb joy_cal_fail
 shr ax,1
 add ax,bx
 mov word ptr [joy_high],ax
 sub bx,word ptr [joy_min]
 jc joy_cal_fail
 cmp bx,32
 jb joy_cal_fail
 shr bx,1
 mov ax,word ptr [joy_center]
 sub ax,bx
 mov word ptr [joy_low],ax
 mov byte ptr [state],0
 call title_screen
 ret
joy_cal_next:
 inc byte ptr [cal_step]
 call joy_cal_screen
joy_cal_done:
 ret
joy_cal_fail:
 mov byte ptr [joy_enabled],0
 mov byte ptr [state],0
 call title_screen
 # Replace setup hint with an actionable calibration failure.
 text 64,175,11,msg_joystick
 text 64,175,12,msg_cal_failed
 ret

# EGA mode 0Dh: map mask selects color planes; XOR and latches let exactly
# the same sprite erase itself. Only moving objects touch display memory.
graphics_setup:
 xor bx,bx
 xor ax,ax
 mov dx,40
 cmp byte ptr [video_mode],0x12
 jne make_rows
 mov ax,3200
 mov dx,160
make_rows:
 mov word ptr [row_offsets+bx],ax
 add ax,dx
 add bx,2
 cmp bx,400
 jb make_rows
 mov ax,0xa000
 mov es,ax
 mov dx,0x3ce
 mov ax,0x0001
 out dx,ax
 mov ax,0x0005
 out dx,ax
 mov ax,0xff08
 out dx,ax
 ret
clear_screen:
 save
 mov dx,0x3c4
 mov ax,0x0f02
 out dx,ax
 mov dx,0x3ce
 mov ax,0x0003
 out dx,ax
 xor ax,ax
 xor di,di
 mov cx,4000
 cmp byte ptr [video_mode],0x12
 jne clear_pixels
 mov cx,19200
clear_pixels:
 rep stosw
 mov byte ptr [plane_cache],15
 mov ax,0x1803
 out dx,ax
 restore
 ret
# AX=x, DX=y, SI=bitmap, CX=width in bytes, BP=rows, BL=color.
# All registers preserved. Logical coordinates stay 320x200 in both modes.
# VGA doubles pixels to 640x400 and centres that playfield in 640x480.
sprite:
 save
 mov word ptr [draw_x],ax
 mov byte ptr [draw_width],cl
 mov di,dx
 shl di,1
 mov di,word ptr [row_offsets+di]
 cmp bl,byte ptr [plane_cache]
 je sprite_plane_ready
 mov byte ptr [plane_cache],bl
 mov dx,0x3c4
 mov ah,bl
 mov al,2
 out dx,ax
sprite_plane_ready:
 cmp byte ptr [video_mode],0x12
 je sprite_vga
 mov ax,word ptr [draw_x]
 mov cl,3
 shr ax,cl
 add di,ax
 mov cx,word ptr [draw_x]
 and cl,7
sprite_row:
 push di
 mov ch,byte ptr [draw_width]
 xor bl,bl
sprite_byte:
 lodsb
 mov ah,al
 xor al,al
 shr ax,cl
 or ah,bl
 mov bl,al
 mov al,byte ptr es:[di]
 mov al,ah
 stosb
 dec ch
 jnz sprite_byte
 or cl,cl
 jz sprite_next
 mov al,byte ptr es:[di]
 mov al,bl
 mov byte ptr es:[di],al
sprite_next:
 pop di
 add di,40
 dec bp
 jnz sprite_row
 restore
 ret
# Expand each source byte with a 256-word lookup. Each output byte is
# XORed into both scanlines. Reading latches before each write is required.
sprite_vga:
 mov ax,word ptr [draw_x]
 shl ax,1
 mov cx,ax
 and cl,7
 mov byte ptr [draw_shift],cl
 mov cl,3
 shr ax,cl
 add di,ax
vga_row:
 push di
 mov cl,byte ptr [draw_shift]
 mov ch,byte ptr [draw_width]
 mov byte ptr [draw_carry],0
vga_byte:
 lodsb
 xor ah,ah
 mov bx,ax
 shl bx,1
 mov dx,word ptr [double_bits+bx]
 # First eight expanded pixels.
 mov ah,dh
 xor al,al
 shr ax,cl
 or ah,byte ptr [draw_carry]
 mov byte ptr [draw_carry],al
 mov al,byte ptr es:[di]
 mov byte ptr es:[di],ah
 mov al,byte ptr es:[di+80]
 mov byte ptr es:[di+80],ah
 inc di
 # Last eight expanded pixels.
 mov ah,dl
 xor al,al
 shr ax,cl
 or ah,byte ptr [draw_carry]
 mov byte ptr [draw_carry],al
 mov al,byte ptr es:[di]
 mov byte ptr es:[di],ah
 mov al,byte ptr es:[di+80]
 mov byte ptr es:[di+80],ah
 inc di
 dec ch
 jnz vga_byte
 test cl,cl
 jz vga_next
 mov ah,byte ptr [draw_carry]
 mov al,byte ptr es:[di]
 mov byte ptr es:[di],ah
 mov al,byte ptr es:[di+80]
 mov byte ptr es:[di+80],ah
vga_next:
 pop di
 add di,160
 dec bp
 jnz vga_row
 restore
 ret
# Font is a compact original 5x7 bitmap table, 6-pixel advance.
draw_text:
 save
 mov word ptr [text_x],ax
 mov word ptr [text_y],dx
 mov byte ptr [text_color],bl
text_char:
 lodsb
 or al,al
 jz text_done
 push si
 sub al,32
 xor ah,ah
 mov bl,7
 mul bl
 mov si,OFFSET font
 add si,ax
 mov ax,word ptr [text_x]
 mov dx,word ptr [text_y]
 mov bl,byte ptr [text_color]
 mov cx,1
 mov bp,7
 call sprite
 add word ptr [text_x],6
 pop si
 jmp text_char
text_done:
 restore
 ret
# AX unsigned number to exactly CX decimal digits at DS:DI.
format_number:
 save
 add di,cx
 dec di
 mov bx,10
number_loop:
 xor dx,dx
 div bx
 add dl,'0'
 mov byte ptr [di],dl
 dec di
 loop number_loop
 restore
 ret
hud_format:
 save
 mov ax,word ptr [score]
 mov di,OFFSET score_digits
 mov cx,5
 call format_number
 mov ax,word ptr [best]
 mov di,OFFSET best_digits
 call format_number
 xor ax,ax
 mov al,byte ptr [wave]
 mov di,OFFSET wave_digits
 mov cx,2
 call format_number
 mov al,byte ptr [lives]
 add al,'0'
 mov byte ptr [life_digit],al
 restore
 ret
hud_numbers:
 save
 call hud_format
 text 42,3,15,score_digits
 text 132,3,11,best_digits
 text 230,3,14,wave_digits
 text 302,3,10,life_digit
 restore
 ret
hud:
 save
 call hud_format
 text 6,3,15,hud_score
 text 102,3,11,hud_best
 text 200,3,14,hud_wave
 text 266,3,10,hud_lives
 mov si,OFFSET muted_text
 cmp byte ptr [sound_on],0
 je hud_sound
 mov si,OFFSET adlib_text
hud_sound:
 mov ax,6
 mov dx,13
 mov bl,8
 call draw_text
 restore
 ret

# Static wave scenery. Integer Bresenham, drawn once, no framebuffer copy.
# XOR sprites restore these lines when erased, including damaged shields.
background:
 save
 mov dx,0x3ce
 mov ax,0x1003
 out dx,ax
 xor ax,ax
 mov al,byte ptr [wave]
 dec ax
 mov bl,6
 div bl
 mov bl,ah
 xor bh,bh
 shl bx,1
 mov si,word ptr [background_table+bx]
 lodsw
 mov cx,ax
background_line:
 push cx
 lodsw
 mov word ptr [line_x],ax
 lodsw
 mov word ptr [line_y],ax
 lodsw
 mov word ptr [line_x2],ax
 lodsw
 mov word ptr [line_y2],ax
 push si
 call wire_line
 pop si
 pop cx
 loop background_line
 mov dx,0x3ce
 mov ax,0x1803
 out dx,ax
 restore
 ret
wire_line:
 mov ax,word ptr [line_x2]
 sub ax,word ptr [line_x]
 mov word ptr [line_sx],1
 jns wire_dx
 neg ax
 mov word ptr [line_sx],-1
wire_dx:
 mov word ptr [line_dx],ax
 mov ax,word ptr [line_y2]
 sub ax,word ptr [line_y]
 mov word ptr [line_sy],1
 jns wire_dy
 neg ax
 mov word ptr [line_sy],-1
wire_dy:
 neg ax
 mov word ptr [line_dy],ax
 add ax,word ptr [line_dx]
 mov word ptr [line_error],ax
wire_pixel:
 mov ax,word ptr [line_x]
 mov dx,word ptr [line_y]
 mov si,OFFSET wire_dot
 mov cx,1
 mov bp,1
 mov bl,1
 call sprite
 cmp ax,word ptr [line_x2]
 jne wire_step
 cmp dx,word ptr [line_y2]
 je wire_done
wire_step:
 mov ax,word ptr [line_error]
 shl ax,1
 cmp ax,word ptr [line_dy]
 jl wire_y
 mov dx,word ptr [line_dy]
 add word ptr [line_error],dx
 mov dx,word ptr [line_sx]
 add word ptr [line_x],dx
wire_y:
 cmp ax,word ptr [line_dx]
 jg wire_pixel
 mov dx,word ptr [line_dx]
 add word ptr [line_error],dx
 mov dx,word ptr [line_sy]
 add word ptr [line_y],dx
 jmp wire_pixel
wire_done:
 ret

# Screens and world initialization.
title_screen:
 call clear_screen
 text 127,23,11,msg_xt
 text 91,39,15,msg_title
 text 82,55,8,msg_subtitle
 mov ax,70
 mov dx,79
 mov si,OFFSET alien_top
 mov cx,2
 mov bp,8
 mov bl,13
 call sprite
 text 106,80,13,msg_30
 mov ax,70
 mov dx,95
 mov si,OFFSET alien_mid
 mov bl,10
 call sprite
 text 106,96,10,msg_20
 mov ax,70
 mov dx,111
 mov si,OFFSET alien_low
 mov bl,11
 call sprite
 text 106,112,11,msg_10
 text 91,135,14,msg_start
 text 58,153,7,msg_controls
 text 64,165,7,msg_controls2
 text 64,175,11,msg_joystick
 mov si,OFFSET silent_text
 cmp byte ptr [skip_sound],0
 jne title_audio
 mov si,OFFSET absent_text
 cmp byte ptr [sound_present],0
 je title_audio
 mov si,OFFSET adlib_text
 cmp byte ptr [sound_on],0
 jne title_audio
 mov si,OFFSET muted_text
title_audio:
 mov ax,112
 mov dx,184
 mov bl,10
 call draw_text
 ret
new_wave:
 call sound_silence
 call clear_screen
 call background
 mov byte ptr [shields_crushed],0
 mov byte ptr [state],1
 mov byte ptr [anim],0
 mov word ptr [group_x],40
 mov word ptr [group_y],43
 mov word ptr [direction],4
 mov word ptr [player_x],152
 mov byte ptr [player_visible],0
 mov byte ptr [invulnerable],0
 mov byte ptr [alive_count],50
 mov byte ptr [move_timer],6
 mov byte ptr [fire_timer],12
 mov byte ptr [ufo_active],0
 mov word ptr [ufo_timer],150
 mov byte ptr [shot_active],0
 mov byte ptr [fire_pending],0
 mov byte ptr [march_note],0
 mov word ptr [enemy_y],0
 mov word ptr [enemy_y+2],0
 mov word ptr [enemy_y+4],0
 mov si,OFFSET aliens
 mov cx,50
new_aliens:
 mov byte ptr [si],1
 inc si
 loop new_aliens
 push es
 push cs
 pop es
 mov di,OFFSET shields
 mov cx,4
shield_init:
 push cx
 mov si,OFFSET shield_template
 mov cx,64
 rep movsb
 pop cx
 loop shield_init
 pop es
 call hud
 text 102,13,8,msg_top_controls
 text 67,192,8,msg_bottom_controls
 xor bx,bx
shield_draw_init:
 call draw_shield
 inc bx
 cmp bx,4
 jb shield_draw_init
 call draw_aliens
 call player_draw
 ret
game_over:
 mov byte ptr [state],3
 call clear_screen
 text 127,35,12,msg_end
 text 100,53,15,msg_gameover
 text 100,79,7,msg_final
 mov ax,word ptr [score]
 mov di,OFFSET final_digits
 mov cx,5
 call format_number
 text 145,95,14,final_digits
 text 76,128,10,msg_restart
 text 100,148,7,msg_quit
 call hud
 ret

# Player and laser at 36.4 Hz. World/aliens retain the original real-time
# speed. A tap queues briefly, and a freed projectile slot fires this step.
game_tick:
 call player_update
 call shot_update
 cmp byte ptr [state],1
 jne game_tick_done
 call player_try_fire
 test word ptr [frame],1
 jnz game_tick_done
 call enemy_update
 cmp byte ptr [state],1
 jne game_tick_done
 call ufo_update
 dec byte ptr [move_timer]
 jnz no_march
 call march
 cmp byte ptr [state],1
 jne game_tick_done
no_march:
 dec byte ptr [fire_timer]
 jnz game_tick_done
 call enemy_fire
game_tick_done:
 ret
player_update:
 mov ax,word ptr [player_x]
 mov dx,word ptr [frame]
 and dx,1
 add dx,2
 xor bx,bx
 cmp byte ptr [keys+75],0
 jne move_left
 cmp byte ptr [joy_left],0
 je test_right
move_left:
 dec bx
test_right:
 cmp byte ptr [keys+77],0
 jne move_right
 cmp byte ptr [joy_right],0
 je move_ship
move_right:
 inc bx
move_ship:
 test bx,bx
 jz player_clamp
 jg player_to_right
 sub ax,dx
 jmp player_clamp
player_to_right:
 add ax,dx
player_clamp:
 cmp ax,8
 jge player_upper
 mov ax,8
player_upper:
 cmp ax,296
 jle player_visibility
 mov ax,296
player_visibility:
 mov dl,1
 cmp byte ptr [invulnerable],0
 je player_compare
 test word ptr [frame],1
 jnz player_blink
 dec byte ptr [invulnerable]
player_blink:
 test word ptr [frame],4
 jz player_compare
 xor dl,dl
player_compare:
 cmp ax,word ptr [player_x]
 jne player_changed
 cmp dl,byte ptr [player_visible]
 je player_update_done
player_changed:
 push ax
 push dx
 cmp byte ptr [player_visible],0
 je player_old_erased
 call player_sprite
player_old_erased:
 pop dx
 pop ax
 mov word ptr [player_x],ax
 mov byte ptr [player_visible],dl
 test dl,dl
 jz player_update_done
 call player_sprite
player_update_done:
 ret
player_try_fire:
 cmp byte ptr [keys+57],0
 jne request_shot
 cmp byte ptr [pressed+57],0
 jne request_shot
 cmp byte ptr [joy_fire],0
 jne request_shot
 cmp byte ptr [joy_fire_edge],0
 je try_pending
request_shot:
 mov byte ptr [fire_pending],6
try_pending:
 cmp byte ptr [fire_pending],0
 je fire_done
 dec byte ptr [fire_pending]
 cmp byte ptr [shot_active],0
 jne fire_done
 mov byte ptr [fire_pending],0
 mov ax,word ptr [player_x]
 add ax,7
 mov word ptr [shot_x],ax
 mov word ptr [shot_y],170
 mov byte ptr [shot_active],1
 call shot_sprite
 mov al,0
 call effect
fire_done:
 ret
player_draw:
 cmp byte ptr [invulnerable],0
 je player_show
 test word ptr [frame],4
 jnz player_draw_done
player_show:
 call player_sprite
 mov byte ptr [player_visible],1
player_draw_done:
 ret
player_sprite:
 mov ax,word ptr [player_x]
 mov dx,176
 mov si,OFFSET ship
 mov cx,2
 mov bp,8
 mov bl,10
 call sprite
 ret
shot_sprite:
 mov ax,word ptr [shot_x]
 mov dx,word ptr [shot_y]
 mov si,OFFSET laser
 mov cx,1
 mov bp,5
 mov bl,15
 call sprite
 ret
shot_update:
 cmp byte ptr [shot_active],0
 je shot_done
 call shot_sprite
 mov ax,word ptr [shot_x]
 mov word ptr [hit_x],ax
 # Test each crossed point against shield pixels; never tunnel through them.
 mov cx,word ptr [frame]
 and cx,1
 add cx,3
shot_travel:
 dec word ptr [shot_y]
 mov ax,word ptr [shot_y]
 mov word ptr [hit_y],ax
 push cx
 call shield_hit
 pop cx
 or al,al
 jnz shot_remove
 loop shot_travel
 cmp word ptr [shot_y],23
 jl shot_remove
 call alien_hit
 or al,al
 jnz shot_remove
 cmp byte ptr [ufo_active],0
 je shot_draw
 mov ax,word ptr [shot_y]
 cmp ax,31
 ja shot_draw
 mov ax,word ptr [shot_x]
 sub ax,word ptr [ufo_x]
 cmp ax,16
 jae shot_draw
 call ufo_sprite
 mov byte ptr [ufo_active],0
 mov word ptr [ufo_timer],180
 mov ax,100
 call add_score
 mov al,1
 call effect
 jmp shot_remove
shot_draw:
 call shot_sprite
 ret
shot_remove:
 mov byte ptr [shot_active],0
shot_done:
 ret

alien_position:
 # Two byte tables replace repeated DIV/MUL for all 50 alien positions.
 xor ax,ax
 mov al,byte ptr [alien_rows+bx]
 mov dx,ax
 add dx,word ptr [group_y]
 mov al,byte ptr [alien_columns+bx]
 add ax,word ptr [group_x]
 ret
alien_sprite:
 call alien_position
alien_at:
 push bx
 mov si,OFFSET alien_low
 mov byte ptr [alien_color],11
 cmp bx,30
 jae alien_art
 mov si,OFFSET alien_mid
 mov byte ptr [alien_color],10
 cmp bx,10
 jae alien_art
 mov si,OFFSET alien_top
 mov byte ptr [alien_color],13
alien_art:
 cmp byte ptr [anim],0
 je alien_frame
 add si,16
alien_frame:
 mov cx,2
 mov bp,8
 mov bl,byte ptr [alien_color]
 call sprite
 pop bx
 ret
draw_aliens:
 xor bx,bx
draw_alien_loop:
 cmp byte ptr [aliens+bx],0
 je draw_alien_next
 call alien_sprite
draw_alien_next:
 inc bx
 cmp bx,50
 jb draw_alien_loop
 ret
alien_hit:
 # First candidate row intersected by the five-pixel projectile.
 mov ax,word ptr [shot_y]
 add ax,4
 sub ax,word ptr [group_y]
 js alien_hit_no
 mov dx,ax
 mov cl,4
 shr dx,cl
 cmp dx,5
 jae alien_hit_no
 and ax,15
 sub ax,4
 cmp ax,8
 jge alien_hit_no
 mov ax,word ptr [shot_x]
 sub ax,word ptr [group_x]
 js alien_hit_no
 mov cl,24
 div cl
 cmp al,10
 jae alien_hit_no
 cmp ah,16
 jae alien_hit_no
 xor ah,ah
 mov bx,dx
 shl bx,1
 shl dx,1
 shl dx,1
 shl dx,1
 add bx,dx
 add bx,ax
 cmp byte ptr [aliens+bx],0
 je alien_hit_no
 call alien_sprite
 mov byte ptr [aliens+bx],0
 dec byte ptr [alive_count]
 mov ax,10
 cmp bx,30
 jae alien_points
 mov ax,20
 cmp bx,10
 jae alien_points
 mov ax,30
alien_points:
 call add_score
 mov al,1
 call effect
 cmp byte ptr [alive_count],0
 jne alien_hit_yes
 mov byte ptr [state],4
 mov byte ptr [state_timer],30
 call clear_enemy_shots
 mov ax,100
 call add_score
 text 130,124,14,msg_waveclear
alien_hit_yes:
 mov al,1
 ret
alien_hit_no:
 xor al,al
 ret
march:
 mov ax,word ptr [group_x]
 mov word ptr [march_dx],ax
 mov ax,word ptr [group_y]
 mov word ptr [march_dy],ax
 mov ax,word ptr [direction]
 add word ptr [group_x],ax
 cmp word ptr [group_x],8
 jl turn_formation
 cmp word ptr [group_x],80
 jle formation_place
turn_formation:
 sub word ptr [group_x],ax
 neg word ptr [direction]
 add word ptr [group_y],6
formation_place:
 mov ax,word ptr [group_x]
 sub word ptr [march_dx],ax
 mov ax,word ptr [group_y]
 sub word ptr [march_dy],ax
 xor byte ptr [anim],1
 # Erase and replace one alien at a time, rather than blanking the fleet.
 xor bx,bx
march_draw:
 cmp byte ptr [aliens+bx],0
 je march_draw_next
 call alien_position
 push ax
 push dx
 add ax,word ptr [march_dx]
 add dx,word ptr [march_dy]
 xor byte ptr [anim],1
 call alien_at
 xor byte ptr [anim],1
 pop dx
 pop ax
 call alien_at
march_draw_next:
 inc bx
 cmp bx,50
 jb march_draw
 mov al,2
 call effect
 # Once the lowest surviving row enters the shield band, remove shields.
 mov bx,49
find_lowest:
 cmp byte ptr [aliens+bx],0
 jne lowest_found
 dec bx
 jns find_lowest
 ret
lowest_found:
 call alien_position
 add dx,8
 mov word ptr [lowest],dx
 cmp word ptr [lowest],148
 jb march_speed
 # XOR erase shields only once, without erasing aliens or the backdrop.
 cmp byte ptr [shields_crushed],0
 jne check_invasion
 mov byte ptr [shields_crushed],1
 xor bx,bx
shield_crush:
 call draw_shield
 inc bx
 cmp bx,4
 jb shield_crush
 push es
 push cs
 pop es
 mov di,OFFSET shields
 xor ax,ax
 mov cx,128
 rep stosw
 pop es
 check_invasion:
 cmp word ptr [lowest],175
 jb march_speed
 mov byte ptr [lives],0
 call game_over
 ret
march_speed:
 xor ax,ax
 mov al,byte ptr [alive_count]
 mov bl,10
 div bl
 inc al
 cmp byte ptr [wave],3
 jb set_move_timer
 cmp al,1
 jbe set_move_timer
 dec al
set_move_timer:
 mov byte ptr [move_timer],al
 ret

# Enemy shots: three simultaneous bolts. Y=0 marks a free slot.
enemy_sprite:
 push bx
 mov ax,word ptr [enemy_x+bx]
 mov dx,word ptr [enemy_y+bx]
 mov si,OFFSET bolt
 mov cx,1
 mov bp,5
 mov bl,12
 call sprite
 pop bx
 ret
clear_enemy_shots:
 xor bx,bx
clear_enemy_loop:
 cmp word ptr [enemy_y+bx],0
 je clear_enemy_next
 call enemy_sprite
 mov word ptr [enemy_y+bx],0
clear_enemy_next:
 add bx,2
 cmp bx,6
 jb clear_enemy_loop
 ret
enemy_update:
 xor bx,bx
enemy_update_loop:
 cmp word ptr [enemy_y+bx],0
 je enemy_next
 call enemy_sprite
 mov ax,word ptr [enemy_x+bx]
 mov word ptr [hit_x],ax
 add word ptr [hit_x],1
 mov cx,3
 cmp byte ptr [wave],4
 jb enemy_travel
 inc cx
enemy_travel:
 inc word ptr [enemy_y+bx]
 mov ax,word ptr [enemy_y+bx]
 add ax,4
 mov word ptr [hit_y],ax
 push bx
 push cx
 call shield_hit
 pop cx
 pop bx
 or al,al
 jnz enemy_remove
 loop enemy_travel
 cmp word ptr [enemy_y+bx],182
 jae enemy_remove
 cmp byte ptr [invulnerable],0
 jne enemy_redraw
 cmp word ptr [enemy_y+bx],172
 jb enemy_redraw
 mov ax,word ptr [enemy_x+bx]
 add ax,2
 sub ax,word ptr [player_x]
 cmp ax,16
 jae enemy_redraw
 mov word ptr [enemy_y+bx],0
 call player_hit
 ret
enemy_redraw:
 call enemy_sprite
 jmp enemy_next
enemy_remove:
 mov word ptr [enemy_y+bx],0
enemy_next:
 add bx,2
 cmp bx,6
 jb enemy_update_loop
 ret
enemy_fire:
 mov byte ptr [fire_timer],12
 cmp byte ptr [wave],3
 jb find_free_bolt
 mov byte ptr [fire_timer],8
find_free_bolt:
 xor bx,bx
free_bolt_loop:
 cmp word ptr [enemy_y+bx],0
 je free_bolt_found
 add bx,2
 cmp bx,6
 jb free_bolt_loop
 ret
free_bolt_found:
 mov word ptr [bolt_slot],bx
 call random
 xor dx,dx
 mov cx,10
 div cx
 mov bx,dx
 add bx,40
bottom_alien:
 cmp byte ptr [aliens+bx],0
 jne enemy_shoot
 cmp bx,10
 jb no_enemy_shoot
 sub bx,10
 jmp bottom_alien
enemy_shoot:
 call alien_position
 add ax,7
 add dx,8
 cmp dx,174
 jae no_enemy_shoot
 mov bx,word ptr [bolt_slot]
 mov word ptr [enemy_x+bx],ax
 mov word ptr [enemy_y+bx],dx
 call enemy_sprite
no_enemy_shoot:
 ret
player_hit:
 call clear_enemy_shots
 cmp byte ptr [shot_active],0
 je hit_no_player_shot
 call shot_sprite
 mov byte ptr [shot_active],0
hit_no_player_shot:
 cmp byte ptr [player_visible],0
 je hit_player_erased
 call player_sprite
 mov byte ptr [player_visible],0
hit_player_erased:
 call hud_numbers
 dec byte ptr [lives]
 call hud_numbers
 mov ax,word ptr [player_x]
 mov dx,175
 mov si,OFFSET explosion
 mov cx,2
 mov bp,10
 mov bl,12
 call sprite
 mov byte ptr [state],5
 mov byte ptr [state_timer],20
 mov al,3
 call effect
 ret

ufo_sprite:
 mov ax,word ptr [ufo_x]
 mov dx,24
 mov si,OFFSET saucer
 mov cx,2
 mov bp,7
 mov bl,12
 call sprite
 ret
ufo_update:
 cmp byte ptr [ufo_active],0
 jne ufo_move
 dec word ptr [ufo_timer]
 jnz ufo_done
 mov byte ptr [ufo_active],1
 mov word ptr [ufo_x],0
 call ufo_sprite
 ret
ufo_move:
 test word ptr [frame],2
 jnz ufo_done
 call ufo_sprite
 add word ptr [ufo_x],3
 cmp word ptr [ufo_x],303
 ja ufo_hide
 call ufo_sprite
 test word ptr [frame],15
 jnz ufo_done
 mov al,4
 call effect
ufo_done:
 ret
ufo_hide:
 mov byte ptr [ufo_active],0
 mov word ptr [ufo_timer],180
 ret
add_score:
 push ax
 call hud_numbers
 pop ax
 add word ptr [score],ax
 jnc score_no_overflow
 mov word ptr [score],65535
score_no_overflow:
 mov ax,word ptr [score]
 cmp ax,word ptr [best]
 jbe score_hud
 mov word ptr [best],ax
score_hud:
 call hud_numbers
 ret

# Destructible 32x16 shields; 5x5 craters and per-pixel collision.
# BX=shield number; all registers preserved.
draw_shield:
 save
 mov ax,bx
 mov cl,6
 shl ax,cl
 mov si,OFFSET shields
 add si,ax
 add ax,40
 mov dx,148
 mov cx,4
 mov bp,16
 mov bl,10
 call sprite
 restore
 ret
shield_hit:
 # Returns AL=1 if hit. Preserves caller CX/BX via caller where needed.
 mov ax,word ptr [hit_y]
 sub ax,148
 cmp ax,16
 jae shield_miss
 mov word ptr [local_y],ax
 mov ax,word ptr [hit_x]
 sub ax,40
 jc shield_miss
 mov dx,ax
 mov cl,6
 shr ax,cl
 cmp ax,4
 jae shield_miss
 mov word ptr [shield_id],ax
 and dx,63
 cmp dx,32
 jae shield_miss
 mov word ptr [local_x],dx
 # Probe actual pixel, not the rectangular outline.
 mov ax,word ptr [local_y]
 call shield_address
 test byte ptr [si],al
 jz shield_miss
 mov bx,word ptr [shield_id]
 call draw_shield
 mov ax,word ptr [local_y]
 sub ax,2
 mov word ptr [crater_y],ax
 mov bp,5
crater_row:
 mov ax,word ptr [local_x]
 sub ax,2
 mov word ptr [crater_x],ax
 mov di,5
crater_pixel:
 mov ax,word ptr [crater_y]
 cmp ax,16
 jae crater_next
 mov dx,word ptr [crater_x]
 cmp dx,32
 jae crater_next
 call shield_address
 not al
 and byte ptr [si],al
crater_next:
 inc word ptr [crater_x]
 dec di
 jnz crater_pixel
 inc word ptr [crater_y]
 dec bp
 jnz crater_row
 mov bx,word ptr [shield_id]
 call draw_shield
 mov al,1
 ret
shield_miss:
 xor al,al
 ret
# AX=local y, DX=local x -> DS:SI byte and AL bit mask.
shield_address:
 push bx
 push cx
 mov si,word ptr [shield_id]
 mov cl,6
 shl si,cl
 shl ax,1
 shl ax,1
 add si,ax
 mov ax,dx
 mov cl,3
 shr ax,cl
 add si,ax
 add si,OFFSET shields
 mov bx,dx
 and bx,7
 mov al,byte ptr [bit_masks+bx]
 pop cx
 pop bx
 ret

# OPL2 at 388h/389h. Six status reads after address, 35 after data.
# Interrupts may lengthen these minimum delays; they cannot shorten them.
# AL=register, AH=value. All registers preserved.
opl_write:
 push ax
 push cx
 push dx
 mov dx,0x388
 out dx,al
 mov cx,6
opl_address_wait:
 in al,dx
 loop opl_address_wait
 inc dx
 mov al,ah
 out dx,al
 dec dx
 mov cx,35
opl_data_wait:
 in al,dx
 loop opl_data_wait
 pop dx
 pop cx
 pop ax
 ret
sound_detect:
 cmp byte ptr [skip_sound],0
 jne sound_detection_done
 mov ax,0x6004
 call opl_write
 mov ax,0x8004
 call opl_write
 mov dx,0x388
 in al,dx
 and al,0xe0
 mov byte ptr [opl_status],al
 mov ax,0xff02
 call opl_write
 mov ax,0x2104
 call opl_write
 mov cx,200
opl_timer_wait:
 in al,dx
 loop opl_timer_wait
 in al,dx
 and al,0xe0
 mov bl,al
 mov ax,0x6004
 call opl_write
 mov ax,0x8004
 call opl_write
 cmp byte ptr [opl_status],0
 jne sound_detection_done
 cmp bl,0xc0
 jne sound_detection_done
 mov byte ptr [sound_present],1
 mov byte ptr [sound_on],1
 # Initialize OPL registers only after a successful timer test.
 mov cx,0xf5
 mov al,1
 xor ah,ah
opl_reset:
 call opl_write
 inc al
 loop opl_reset
 mov ax,0x2001
 call opl_write
 mov ax,0x0008
 call opl_write
 mov ax,0x00bd
 call opl_write
 xor bx,bx
patch_loop:
 # Five distinct FM voices, two operators per channel.
 mov al,byte ptr [op_offsets+bx]
 mov byte ptr [op_current],al
 add al,0x20
 mov ah,1
 call opl_write
 add al,3
 call opl_write
 mov al,byte ptr [op_current]
 add al,0x40
 mov ah,byte ptr [mod_levels+bx]
 call opl_write
 add al,3
 mov ah,2
 call opl_write
 mov al,byte ptr [op_current]
 add al,0x60
 mov ah,0xf4
 call opl_write
 add al,3
 mov ah,0xf3
 call opl_write
 mov al,byte ptr [op_current]
 add al,0x80
 mov ah,0x47
 call opl_write
 add al,3
 mov ah,0x37
 call opl_write
 mov al,byte ptr [op_current]
 add al,0xe0
 mov ah,byte ptr [waveforms+bx]
 call opl_write
 add al,3
 xor ah,ah
 call opl_write
 mov al,bl
 add al,0xc0
 mov ah,byte ptr [feedbacks+bx]
 call opl_write
 inc bx
 cmp bx,5
 jb patch_loop
 # March channel: low, softly modulated thud with a fast decay.
 mov ax,0x0122
 call opl_write
 mov ax,0x0125
 call opl_write
 mov ax,0x2842
 call opl_write
 mov ax,0x0245
 call opl_write
 mov ax,0xf862
 call opl_write
 mov ax,0xf865
 call opl_write
 mov ax,0x0a82
 call opl_write
 mov ax,0x0a85
 call opl_write
 mov ax,0x00c2
 call opl_write
sound_detection_done:
 ret
toggle_sound:
 cmp byte ptr [sound_present],0
 je toggle_done
 call sound_silence
 xor byte ptr [sound_on],1
toggle_done:
 ret
sound_silence:
 save
 cmp byte ptr [sound_present],0
 je silence_done
 xor bx,bx
silence_loop:
 mov al,bl
 add al,0xb0
 xor ah,ah
 call opl_write
 mov byte ptr [sfx_time+bx],0
 inc bx
 cmp bx,9
 jb silence_loop
silence_done:
 restore
 ret
# AL effect = channel: 0 laser,1 alien hit,2 march,3 player hit,4 UFO.
effect:
 save
 cmp byte ptr [sound_on],0
 je effect_done
 xor ah,ah
 mov bx,ax
 mov al,bl
 add al,0xb0
 xor ah,ah
 call opl_write
 mov al,byte ptr [effect_lengths+bx]
 mov byte ptr [sfx_time+bx],al
 mov al,byte ptr [effect_low+bx]
 cmp bx,2
 jne effect_pitch
 push bx
 xor bx,bx
 mov bl,byte ptr [march_note]
 mov al,byte ptr [march_pitches+bx]
 inc bl
 and bl,3
 mov byte ptr [march_note],bl
 pop bx
effect_pitch:
 mov byte ptr [sfx_low+bx],al
 mov al,bl
 add al,0xa0
 mov ah,byte ptr [sfx_low+bx]
 call opl_write
 mov al,bl
 add al,0xb0
 mov ah,byte ptr [effect_high+bx]
 or ah,0x20
 call opl_write
effect_done:
 restore
 ret
sound_tick:
 save
 cmp byte ptr [sound_on],0
 je sound_tick_done
 xor bx,bx
sound_channel:
 cmp byte ptr [sfx_time+bx],0
 je sound_next
 dec byte ptr [sfx_time+bx]
 jz sound_keyoff
 test word ptr [frame],1
 jnz sound_next
 mov al,byte ptr [effect_slide+bx]
 test al,al
 jz sound_next
 sub byte ptr [sfx_low+bx],al
 mov al,bl
 add al,0xa0
 mov ah,byte ptr [sfx_low+bx]
 call opl_write
 jmp sound_next
sound_keyoff:
 mov al,bl
 add al,0xb0
 mov ah,byte ptr [effect_high+bx]
 call opl_write
sound_next:
 inc bx
 cmp bx,5
 jb sound_channel
sound_tick_done:
 restore
 ret

.section .data
old_mode: .byte 3
old_page: .byte 0
skip_sound: .byte 0
video_mode: .byte 0x0d
want_joy: .byte 0
usage: .ascii "XT INVADERS 2.0 - EGA/VGA + AdLib arcade game\r\n"
 .ascii "INVADERS [/E | /V] [/J] [/S]\r\n"
 .ascii "/E 320x200  /V 640x480 16 colors  /J calibrate gameport  /S silent\r\n"
 .ascii "Arrows: move  Space: fire/start  P: pause  M: mute  Esc: quit\r\n"
 .ascii "DOS 2+, 8088/V20+, EGA or VGA. AdLib at 388h is optional.\r\n$"
err_memory: .ascii "Not enough DOS memory for XT INVADERS.\r\n$"
err_dos: .ascii "XT INVADERS requires DOS 2.0 or later.\r\n$"
err_video: .ascii "Video mode unavailable. /E requires EGA; /V requires VGA.\r\n$"
farewell: .ascii "Thanks for playing XT INVADERS!\r\n$"
msg_xt: .asciz "8088 / V20"
msg_title: .asciz "X T   I N V A D E R S"
msg_subtitle: .asciz "DEFEND THE LAST OUTPOST"
msg_30: .asciz "SCOUT       30 POINTS"
msg_20: .asciz "RAIDER      20 POINTS"
msg_10: .asciz "DRONE       10 POINTS"
msg_start: .asciz "SPACE OR FIRE TO START"
msg_controls: .asciz "ARROWS MOVE    HOLD SPACE TO FIRE"
msg_controls2: .asciz "P PAUSE    M SOUND    ESC QUIT"
msg_joystick: .asciz "J SET UP JOYSTICK OR GAMEPAD"
msg_cal_title: .asciz "GAMEPORT CALIBRATION"
msg_cal_center: .asciz "RELEASE STICK OR PAD TO CENTRE"
msg_cal_left: .asciz "HOLD STICK OR PAD FULL LEFT"
msg_cal_right: .asciz "HOLD STICK OR PAD FULL RIGHT"
msg_cal_accept: .asciz "PRESS ENTER OR BUTTON ONE"
msg_cal_cancel: .asciz "ESC CANCELS - KEYBOARD STILL WORKS"
msg_cal_failed: .asciz "NO VALID X AXIS - PRESS J TO RETRY"
msg_top_controls: .asciz "ARROWS MOVE   SPACE FIRE"
msg_bottom_controls: .asciz "P PAUSE    M SOUND    ESC QUIT"
msg_pause: .asciz "-- PAUSED --"
msg_end: .asciz "OUTPOST LOST"
msg_gameover: .asciz "G A M E  O V E R"
msg_final: .asciz "YOUR FINAL SCORE"
msg_restart: .asciz "SPACE OR FIRE TO TRY AGAIN"
msg_quit: .asciz "ESC RETURNS TO DOS"
msg_waveclear: .asciz "WAVE CLEAR"
adlib_text: .asciz "ADLIB ON"
muted_text: .asciz "SOUND OFF"
absent_text: .asciz "NO ADLIB - SILENT"
silent_text: .asciz "SILENT MODE"
hud_score: .ascii "SCORE "
score_digits: .asciz "00000"
hud_best: .ascii "BEST "
best_digits: .asciz "00000"
hud_wave: .ascii "WAVE "
wave_digits: .asciz "01"
hud_lives: .ascii "LIVES "
life_digit: .asciz "3"
final_digits: .asciz "00000"
bit_masks: .byte 128,64,32,16,8,4,2,1
op_offsets: .byte 0,1,2,8,9
mod_levels: .byte 0x18,0x08,0x25,0x00,0x12
waveforms: .byte 0,1,0,1,2
feedbacks: .byte 0x06,0x0e,0x04,0x0e,0x08
effect_lengths: .byte 6,8,2,18,4
effect_low: .byte 240,220,120,190,170
effect_high: .byte 0x15,0x0d,0x09,0x09,0x12
march_pitches: .byte 238,205,175,148
effect_slide: .byte 55,42,0,17,55
# Duplicated bits, MSB-first; generated by the assembler.
double_bits:
.set d,0
.rept 256
 .word ((d & 128)*384) | ((d & 64)*192) | ((d & 32)*96) | ((d & 16)*48) | ((d & 8)*24) | ((d & 4)*12) | ((d & 2)*6) | ((d & 1)*3)
 .set d,d+1
.endr
alien_rows:
.set a,0
.rept 50
 .byte (a/10)*16
 .set a,a+1
.endr
alien_columns:
.set a,0
.rept 50
 .byte (a%10)*24
 .set a,a+1
.endr
wire_dot: .byte 128
.include "WIRE.INC"
.include "ART.INC"

.section .bss
bss_start:
old_irq: .skip 4
keys: .skip 128
key_edges: .skip 128
pressed: .skip 128
old_timer: .skip 4
clock_ticks: .skip 2
bios_phase: .skip 1
row_offsets: .skip 400
plane_cache: .skip 1
draw_shift: .skip 1
draw_carry: .skip 1
fire_pending: .skip 1
march_note: .skip 1
march_dx: .skip 2
march_dy: .skip 2
joy_enabled: .skip 1
joy_left: .skip 1
joy_right: .skip 1
joy_fire: .skip 1
joy_fire_edge: .skip 1
joy_pause_edge: .skip 1
joy_buttons: .skip 1
joy_old_buttons: .skip 1
joy_raw: .skip 2
joy_valid: .skip 1
joy_failures: .skip 1
joy_center: .skip 2
joy_min: .skip 2
joy_low: .skip 2
joy_high: .skip 2
cal_step: .skip 1
shields_crushed: .skip 1
line_x: .skip 2
line_y: .skip 2
line_x2: .skip 2
line_y2: .skip 2
line_dx: .skip 2
line_dy: .skip 2
line_sx: .skip 2
line_sy: .skip 2
line_error: .skip 2
state: .skip 1
state_timer: .skip 1
score: .skip 2
best: .skip 2
lives: .skip 1
wave: .skip 1
frame: .skip 2
last_tick: .skip 2
rng: .skip 2
player_x: .skip 2
player_visible: .skip 1
invulnerable: .skip 1
shot_active: .skip 1
shot_x: .skip 2
shot_y: .skip 2
aliens: .skip 50
alive_count: .skip 1
group_x: .skip 2
group_y: .skip 2
direction: .skip 2
anim: .skip 1
alien_color: .skip 1
move_timer: .skip 1
fire_timer: .skip 1
lowest: .skip 2
enemy_x: .skip 6
enemy_y: .skip 6
bolt_slot: .skip 2
ufo_x: .skip 2
ufo_active: .skip 1
ufo_timer: .skip 2
shields: .skip 256
hit_x: .skip 2
hit_y: .skip 2
shield_id: .skip 2
local_x: .skip 2
local_y: .skip 2
crater_x: .skip 2
crater_y: .skip 2
draw_x: .skip 2
draw_width: .skip 1
text_x: .skip 2
text_y: .skip 2
text_color: .skip 1
sound_present: .skip 1
sound_on: .skip 1
opl_status: .skip 1
op_current: .skip 1
sfx_time: .skip 9
sfx_low: .skip 9
.align 16
stack_bottom: .skip 1024
stack_top:
mem_end:
.section .note.GNU-stack,"",@progbits
