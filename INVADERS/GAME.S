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
 # /S skips every AdLib access; /? gives usage.
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
 jne arg_next
 mov byte ptr [skip_sound],1
arg_next:
 loop args
check_video:
 mov ax,0x1a00
 xor bx,bx
 int 0x10
 cmp al,0x1a
 je video_ok
 mov ah,0x12
 mov bx,0x10
 int 0x10
 cmp bl,3
 jbe video_ok
 mov dx,OFFSET err_video
 jmp dos_error
video_ok:
 mov ah,0x0f
 int 0x10
 and al,0x7f
 mov byte ptr [old_mode],al
 mov byte ptr [old_page],bh
 call sound_detect
 mov ax,0x000d
 int 0x10
 mov ah,0x0f
 int 0x10
 and al,0x7f
 cmp al,0x0d
 je mode_ok
 call restore_video
 mov dx,OFFSET err_video
 jmp dos_error
mode_ok:
 call graphics_setup
 call install_keyboard
 mov ax,0x40
 mov es,ax
 mov ax,word ptr es:[0x6c]
 mov word ptr [rng],ax
 or word ptr [rng],1
 mov word ptr [last_tick],ax
 mov ax,0xa000
 mov es,ax
 call title_screen
main_loop:
 # Use IRQ-driven key states, including releases: smooth simultaneous input.
 cmp byte ptr [keys+1],0
 jne quit
 call ticks
 cmp ax,word ptr [last_tick]
 jne next_tick
 sti
 hlt
 jmp main_loop
next_tick:
 mov word ptr [last_tick],ax
 inc word ptr [frame]
 call sound_tick
 # Sound toggle (edge).
 cmp byte ptr [keys+50],0
 je check_pause
 cmp byte ptr [previous+50],0
 jne check_pause
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
 cmp byte ptr [keys+25],0
 je dispatch
 cmp byte ptr [previous+25],0
 jne dispatch
 cmp byte ptr [state],1
 je pause_game
 cmp byte ptr [state],2
 jne dispatch
 text 118,118,15,msg_pause
 mov byte ptr [state],1
 jmp dispatch
pause_game:
 mov byte ptr [state],2
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
 cmp byte ptr [keys+57],0
 jne start_edge
 cmp byte ptr [keys+28],0
 je frame_done
 cmp byte ptr [previous+28],0
 jne frame_done
 jmp new_game
start_edge:
 cmp byte ptr [previous+57],0
 jne frame_done
new_game:
 mov word ptr [score],0
 mov byte ptr [lives],3
 mov byte ptr [wave],1
 call new_wave
 jmp frame_done
between_waves:
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
 push es
 push cs
 pop es
 mov si,OFFSET keys
 mov di,OFFSET previous
 mov cx,128
 rep movsb
 pop es
 jmp main_loop
quit:
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
# The PIT/IRQ0 and DOS timekeeping are never reprogrammed.
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
ticks:
 push es
 mov ax,0x40
 mov es,ax
 mov ax,word ptr es:[0x6c]
 pop es
 ret
random:
 mov ax,word ptr [rng]
 shr ax,1
 jnc rng_store
 xor ax,0xb400
rng_store:
 mov word ptr [rng],ax
 ret

# EGA mode 0Dh: map mask selects color planes; XOR and latches let exactly
# the same sprite erase itself. Only moving objects touch display memory.
graphics_setup:
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
 rep stosw
 mov ax,0x1803
 out dx,ax
 restore
 ret
# AX=x, DX=y, SI=bitmap, CX=width in bytes, BP=rows, BL=color.
# All registers preserved. Caller keeps bitmap within the 320x200 screen.
sprite:
 save
 mov word ptr [draw_x],ax
 mov byte ptr [draw_width],cl
 mov ax,dx
 mov di,40
 mul di
 mov di,ax
 mov ax,word ptr [draw_x]
 mov cl,3
 shr ax,cl
 add di,ax
 mov dx,0x3c4
 mov ah,bl
 mov al,2
 out dx,ax
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
hud:
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
 mov al,2
 call effect
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

# One simulation step per BIOS tick (~18.2 Hz). Drop excess time when slow.
game_tick:
 call player_update
 call shot_update
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
 cmp byte ptr [player_visible],0
 je player_move
 call player_sprite
 mov byte ptr [player_visible],0
player_move:
 cmp byte ptr [keys+75],0
 je player_right
 sub word ptr [player_x],5
 cmp word ptr [player_x],8
 jge player_right
 mov word ptr [player_x],8
player_right:
 cmp byte ptr [keys+77],0
 je player_fire
 add word ptr [player_x],5
 cmp word ptr [player_x],296
 jle player_fire
 mov word ptr [player_x],296
player_fire:
 cmp byte ptr [invulnerable],0
 je player_try_fire
 dec byte ptr [invulnerable]
player_try_fire:
 cmp byte ptr [keys+57],0
 je player_draw
 cmp byte ptr [shot_active],0
 jne player_draw
 mov ax,word ptr [player_x]
 add ax,7
 mov word ptr [shot_x],ax
 mov word ptr [shot_y],170
 mov byte ptr [shot_active],1
 call shot_sprite
 mov al,0
 call effect
player_draw:
 cmp byte ptr [invulnerable],0
 je player_show
 test word ptr [frame],2
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
 mov cx,7
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
 # BX=index, returns AX=x and DX=y; preserves BX.
 mov ax,bx
 mov cl,10
 div cl
 push ax
 xor ah,ah
 mov dx,ax
 mov cl,4
 shl dx,cl
 add dx,word ptr [group_y]
 pop ax
 mov al,ah
 xor ah,ah
 mov cl,24
 mul cl
 add ax,word ptr [group_x]
 ret
alien_sprite:
 push bx
 call alien_position
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
 xor bx,bx
alien_hit_loop:
 cmp byte ptr [aliens+bx],0
 je alien_hit_next
 call alien_position
 mov cx,word ptr [shot_x]
 sub cx,ax
 cmp cx,16
 jae alien_hit_next
 mov cx,word ptr [shot_y]
 add cx,4
 cmp cx,dx
 jl alien_hit_next
 add dx,8
 cmp word ptr [shot_y],dx
 jge alien_hit_next
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
alien_hit_next:
 inc bx
 cmp bx,50
 jb alien_hit_loop
 xor al,al
 ret
march:
 call draw_aliens
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
 xor byte ptr [anim],1
 call draw_aliens
 mov al,2
 call effect
 # Once the lowest surviving row enters the shield band, remove shields.
 xor bx,bx
 mov word ptr [lowest],0
find_lowest:
 cmp byte ptr [aliens+bx],0
 je lowest_next
 call alien_position
 add dx,8
 cmp dx,word ptr [lowest]
 jbe lowest_next
 mov word ptr [lowest],dx
lowest_next:
 inc bx
 cmp bx,50
 jb find_lowest
 cmp word ptr [lowest],148
 jb march_speed
 # XOR erase shields, zero their collision masks, without erasing aliens.
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
 call hud
 dec byte ptr [lives]
 call hud
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
 test word ptr [frame],1
 jnz ufo_done
 call ufo_sprite
 add word ptr [ufo_x],3
 cmp word ptr [ufo_x],303
 ja ufo_hide
 call ufo_sprite
 test word ptr [frame],7
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
 call hud
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
 call hud
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
 mov al,byte ptr [effect_slide+bx]
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
usage: .ascii "XT INVADERS 1.0 - EGA/VGA + AdLib arcade game\r\n"
 .ascii "INVADERS [/S]    /S skips AdLib probing and runs silently.\r\n"
 .ascii "Arrows: move  Space: fire/start  P: pause  M: mute  Esc: quit\r\n"
 .ascii "DOS 2+, 8088/V20+, EGA or VGA. AdLib at 388h is optional.\r\n$"
err_memory: .ascii "Not enough DOS memory for XT INVADERS.\r\n$"
err_dos: .ascii "XT INVADERS requires DOS 2.0 or later.\r\n$"
err_video: .ascii "XT INVADERS requires EGA or VGA mode 0Dh.\r\n$"
farewell: .ascii "Thanks for playing XT INVADERS!\r\n$"
msg_xt: .asciz "8088 / V20"
msg_title: .asciz "X T   I N V A D E R S"
msg_subtitle: .asciz "DEFEND THE LAST OUTPOST"
msg_30: .asciz "SCOUT       30 POINTS"
msg_20: .asciz "RAIDER      20 POINTS"
msg_10: .asciz "DRONE       10 POINTS"
msg_start: .asciz "PRESS SPACE TO START"
msg_controls: .asciz "ARROWS MOVE    HOLD SPACE TO FIRE"
msg_controls2: .asciz "P PAUSE    M SOUND    ESC QUIT"
msg_top_controls: .asciz "ARROWS MOVE   SPACE FIRE"
msg_bottom_controls: .asciz "P PAUSE    M SOUND    ESC QUIT"
msg_pause: .asciz "-- PAUSED --"
msg_end: .asciz "OUTPOST LOST"
msg_gameover: .asciz "G A M E  O V E R"
msg_final: .asciz "YOUR FINAL SCORE"
msg_restart: .asciz "PRESS SPACE TO TRY AGAIN"
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
effect_lengths: .byte 3,4,1,9,2
effect_low: .byte 240,220,120,190,170
effect_high: .byte 0x15,0x0d,0x08,0x09,0x12
effect_slide: .byte 55,42,0,17,55
.include "ART.INC"

.section .bss
bss_start:
old_irq: .skip 4
keys: .skip 128
previous: .skip 128
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
