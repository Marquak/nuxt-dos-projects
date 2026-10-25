.intel_syntax noprefix
.code16
.global _start

# GAS expands a distant conditional jump to 0F 8x, which needs a 386.
# Keep every service dispatch valid on an 8088/V20.
.macro dispatch return_ip, destination
    cmp word ptr ss:[bp+2], \return_ip
    jne .Ldispatch_next\@
    jmp \destination
.Ldispatch_next\@:
.endm

# One-file EXE prefix. The original game's image starts 100h paragraphs
# after this prefix; its own entry remains at 0100:000Eh.
.org 0x100
_start:
    mov bx, ds
    mov ax, cs
    mov ds, ax
    mov es, bx
    mov word ptr [psp_seg], bx
    mov si, 0x81
    xor cx, cx
    mov cl, byte ptr es:[0x80]
arg_loop:
    jcxz arg_done
    cmp byte ptr es:[si], ' '
    je arg_next
    cmp byte ptr es:[si], '/'
    jne arg_next
    cmp cx, 2
    jb arg_next
    mov al, byte ptr es:[si+1]
    and al, 0xdf
    cmp al, 'F'
    jne arg_slow
    mov byte ptr [tick_spacing], 3
    jmp arg_next
arg_slow:
    cmp al, 'S'
    jne arg_timing
    mov byte ptr [tick_spacing], 5
    jmp arg_next
arg_timing:
    cmp al, 'A'
    jne arg_tick
    mov byte ptr [accurate_action], 1
    jmp arg_next
arg_tick:
    cmp al, 'T'
    jne arg_voice
    cmp cx, 3
    jb arg_next
    mov al, byte ptr es:[si+2]
    sub al, '0'
    cmp al, 2
    jb arg_next
    cmp al, 9
    ja arg_next
    mov byte ptr [tick_spacing], al
    jmp arg_next
arg_voice:
    cmp al, 'V'
    jne arg_next
    cmp cx, 3
    jb arg_next
    mov al, byte ptr es:[si+2]
    sub al, '0'
    cmp al, 1
    jb arg_next
    cmp al, 4
    ja arg_next
    mov byte ptr [voice_factor], al
arg_next:
    inc si
    dec cx
    jmp arg_loop
arg_done:
    mov ah, 0x0f
    int 0x10
    mov byte ptr [old_mode], al
    call calibrate_voice

    mov ah, 0x35
    mov al, 0x66
    int 0x21
    mov word ptr [old_vector], bx
    mov word ptr [old_vector+2], es

    mov ah, 0x35
    mov al, 0x10
    int 0x21
    mov word ptr [old_video], bx
    mov word ptr [old_video+2], es

    mov ax, 0x351f
    int 0x21
    mov word ptr [old_font], bx
    mov word ptr [old_font+2], es

    mov dx, offset timer_hook
    mov ah, 0x25
    mov al, 0x66
    int 0x21

    mov dx, offset video_hook
    mov ah, 0x25
    mov al, 0x10
    int 0x21

    # Start the original image in this process, preserving the PSP in ES.
    # Its startup immediately establishes its own data segment and stack.
    mov bx, word ptr [psp_seg]
    mov ax, cs
    add ax, 0x100
    mov ds, bx
    mov es, bx
    push ax
    mov bx, 0x000e
    push bx
    retf

exit_game:
    # Ctrl-X must remove our hooks before DOS releases this EXE's memory.
    mov ax, cs
    mov ds, ax
    lds dx, [old_video]
    mov ax, 0x2510
    int 0x21
    mov ax, cs
    mov ds, ax
    lds dx, [old_vector]
    mov ax, 0x2566
    int 0x21
    mov ax, cs
    mov ds, ax
    call restore_display
    mov ax, 0x4c00
    int 0x21

restore_display:
    in al, 0x61
    and al, 0xfc
    out 0x61, al
    mov al, byte ptr [old_mode]
    xor ah, ah
    int 0x10
    mov ax, cs
    mov ds, ax
    lds dx, [old_font]
    mov ax, 0x251f
    int 0x21
    mov ax, cs
    mov ds, ax
    ret

timer_hook:
    push bp
    mov bp, sp
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    mov ax, cs
    mov ds, ax
    test word ptr ss:[bp+6], 0x0200
    jz hook_if_disabled
    sti
hook_if_disabled:
    dispatch 0x0078, normal_game_exit
    dispatch 0x607f, keyboard_read
    dispatch 0x784b, game_tick
    dispatch 0x7463, action_countdown
    dispatch 0x6ece, joystick_read
    dispatch 0x713c, sound_loop
    dispatch 0x717a, sound_loop
    dispatch 0x7269, voice_delay_set
    dispatch 0x7271, voice_delay_set
    dispatch 0x7145, sound_square
    dispatch 0x71a9, sound_burst
    dispatch 0x1e7a, delay_2078
    dispatch 0x1e9a, delay_2098
    dispatch 0x251a, delay_2718
    dispatch 0x4179, delay_4377
    dispatch 0x42de, delay_44dc
    dispatch 0x430b, delay_4509
    dispatch 0x4923, delay_4b21
    dispatch 0x649c, delay_669a
    dispatch 0x7402, delay_7600
    dispatch 0x741e, delay_761c
    dispatch 0x7c41, delay_7e3f
    dispatch 0x7c61, delay_7e5f
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    pop bp
    jmp far ptr cs:[old_vector]

keyboard_read:
    xor ah, ah
    int 0x16
    cmp al, 0x18
    jne keyboard_return
    jmp exit_game
keyboard_return:
    mov word ptr ss:[bp-2], ax
    jmp timer_done

normal_game_exit:
    # The original startup's MOV SP,BP / RETF was replaced by INT 66h.
    # Restore that stack before invoking DOS, then use the same exit path.
    mov ax, word ptr ss:[bp]
    mov sp, ax
    jmp exit_game

game_tick:
    sti
    cmp byte ptr [accurate_action], 0
    je game_tick_continue
    # Start a new action's clock at the end of the update that created it,
    # before the wait for the next update. A cancelled or completed action
    # must not lend its fractional second to the next one.
    mov ax, word ptr ss:[bp-14]
    mov es, ax
    mov al, byte ptr es:[0x01d3]
    test al, al
    jne game_tick_action
    mov byte ptr [action_active], 0
    jmp game_tick_continue
game_tick_action:
    cmp byte ptr [action_active], 0
    je game_tick_action_new
    cmp al, byte ptr [action_expected]
    jne game_tick_action_new
    mov ah, byte ptr es:[0x01d4]
    cmp ah, byte ptr [action_target]
    je game_tick_continue
game_tick_action_new:
    mov byte ptr [action_active], 1
    mov byte ptr [action_expected], al
    mov al, byte ptr es:[0x01d4]
    mov byte ptr [action_target], al
    mov ax, 0x40
    mov es, ax
    mov ax, word ptr es:[0x6c]
    mov word ptr [action_last_tick], ax
    mov word ptr [action_remainder], 0
game_tick_continue:
    mov ax, 0x40
    mov es, ax
    mov ax, word ptr es:[0x6c]
    cmp byte ptr [tick_seen], 0
    jne wait_tick
    mov byte ptr [tick_seen], 1
    jmp tick_done
wait_tick:
    mov ax, word ptr es:[0x6c]
    mov dx, ax
    sub dx, word ptr [last_tick]
    xor bx, bx
    mov bl, byte ptr [tick_spacing]
    cmp dx, bx
    jae tick_done
    hlt
    jmp wait_tick
tick_done:
    mov word ptr [last_tick], ax
    mov word ptr ss:[bp+2], 0x7363
    jmp timer_done

# The default reproduces the original update-based countdown. /A instead
# advances the shared 1D3h action counter from elapsed BIOS ticks. The
# original MOV [1D3h],AL follows the replaced XOR AH,AH / DEC AX sequence.
action_countdown:
    cmp byte ptr [accurate_action], 0
    jne action_accurate
    mov ax, word ptr ss:[bp-2]
    xor ah, ah
    dec ax
    mov word ptr ss:[bp-2], ax
    jmp timer_done
action_accurate:
    mov ax, word ptr ss:[bp-14]
    mov es, ax
    mov al, byte ptr es:[0x01d3]
    mov dl, byte ptr es:[0x01d4]
    xor ah, ah
    mov bx, 0x40
    mov es, bx
    mov bx, word ptr es:[0x6c]
    cmp byte ptr [action_active], 0
    je action_new
    cmp al, byte ptr [action_expected]
    jne action_new
    cmp dl, byte ptr [action_target]
    jne action_new
    mov cx, bx
    sub bx, word ptr [action_last_tick]
    mov word ptr [action_last_tick], cx
    # A long interruption can expire the whole action immediately. Do not
    # spend thousands of iterations catching up after a clock jump.
    cmp bx, 4600
    jae action_expired
    add bx, word ptr [action_remainder]
    jc action_expired
action_seconds:
    cmp bx, 18
    jb action_ready
    sub bx, 18
    dec al
    jnz action_seconds
action_expired:
    xor ax, ax
    xor bx, bx
action_ready:
    mov word ptr [action_remainder], bx
    mov byte ptr [action_expected], al
    mov word ptr ss:[bp-2], ax
    jmp timer_done
action_new:
    mov byte ptr [action_active], 1
    mov byte ptr [action_expected], al
    mov byte ptr [action_target], dl
    mov word ptr [action_last_tick], bx
    mov word ptr [action_remainder], 0
    mov word ptr ss:[bp-2], ax
    jmp timer_done

# The original gameport reader takes 250 back-to-back port samples. On a
# faster CPU its counts shrink and the calibration windows become unstable.
# Return the same 0..254 axis values in units of 16 PIT clocks instead.
# A missing/stuck axis retains the original FFh sentinel after 3.35 ms.
joystick_read:
    mov ax, word ptr ss:[bp-14]
    mov es, ax
    mov byte ptr es:[0x02bb], 0
    mov byte ptr es:[0x02bc], 0
    mov byte ptr es:[0x4ac1], 0xff
    mov byte ptr es:[0x4ac2], 0xff
    call read_pit
    mov si, ax
    mov dx, 0x0201
    xor al, al
    out dx, al
    in al, dx
    test al, 0x10
    jne joystick_button_two
    mov byte ptr es:[0x02bb], 1
joystick_button_two:
    test al, 0x20
    jne joystick_start
    mov byte ptr es:[0x02bc], 1
joystick_start:
joystick_poll:
    mov dx, 0x0201
    in al, dx
    xor ah, ah
    mov di, ax
    call read_pit
    mov cx, si
    sub cx, ax
    cmp cx, 4000
    jb joystick_x
    jmp joystick_timeout
joystick_x:
    mov bx, cx
    shr bx, 1
    shr bx, 1
    shr bx, 1
    shr bx, 1
    cmp byte ptr es:[0x4ac1], 0xff
    jne joystick_y
    test di, 1
    jne joystick_y
    mov byte ptr es:[0x4ac1], bl
joystick_y:
    cmp byte ptr es:[0x4ac2], 0xff
    jne joystick_check
    test di, 2
    jne joystick_check
    mov byte ptr es:[0x4ac2], bl
joystick_check:
    cmp byte ptr es:[0x4ac1], 0xff
    je joystick_poll
    cmp byte ptr es:[0x4ac2], 0xff
    je joystick_poll
    jmp timer_done
joystick_timeout:
    mov byte ptr es:[0x4ac1], 0xff
    mov byte ptr es:[0x4ac2], 0xff
    jmp timer_done

# Both original speech setup paths store AX at DS:9e90h. The CPU loop itself
# remains in the game. An 8088 at 4.77 MHz takes about 17 CPU clocks per LOOP,
# or 4.25 PIT counts. For 8192 iterations that is 34816 PIT counts. Scale the
# original small delay by measured LOOP speed on this CPU. The old INT-per-
# sample hook had more overhead than the speech interval on a real V20.
voice_delay_set:
    mov ax, word ptr ss:[bp-14]
    mov es, ax
    xor ax, ax
    mov al, byte ptr ss:[bp-2]
    test ax, ax
    je voice_store
    mov bx, 34816
    mul bx
    cmp dx, word ptr [voice_loop_pit]
    jae voice_saturate
    div word ptr [voice_loop_pit]
    test ax, ax
    jne voice_multiply
    inc ax
voice_multiply:
    xor bx, bx
    mov bl, byte ptr [voice_factor]
    mul bx
    test dx, dx
    je voice_store
voice_saturate:
    mov ax, 0xffff
voice_store:
    mov word ptr es:[0x9e90], ax
    jmp timer_done

# The original 8088 LOOP instruction takes roughly 17 clocks when taken.
# The PIT is one quarter of an XT CPU clock; 4.25 PIT counts per iteration.
sound_loop:
    mov cx, word ptr ss:[bp-6]
    jcxz sound_loop_wrap
    mov ax, cx
    mov bx, 4
    mul bx
    shr cx, 1
    shr cx, 1
    add ax, cx
    adc dx, 0
    call wait_ticks32
    jmp sound_loop_done
sound_loop_wrap:
    mov ax, 0x4000
    mov dx, 4
    call wait_ticks32
sound_loop_done:
    mov word ptr ss:[bp-6], 0
    jmp timer_done

# 7343h is a square nested delay: n*n inner iterations, with n=256 for AL=0.
sound_square:
    xor ax, ax
    mov al, byte ptr ss:[bp-2]
    test al, al
    jne square_multiply
    mov ax, 256
square_multiply:
    mov bx, ax
    mul bx
    mov bx, 10
    mov si, dx
    mul bx
    mov cx, ax
    mov di, dx
    mov ax, si
    mul bx
    add di, ax
    mov dx, di
    mov ax, cx
    call wait_ticks32
    mov byte ptr ss:[bp-2], 0
    mov word ptr ss:[bp+2], 0x714f
    jmp timer_done

# The original effect rapidly decrements BL and toggles the speaker whenever
# it wraps, repeating 255 steps per AL count. Schedule those transitions from
# PIT time instead of running the CPU-counted nested loop.
sound_burst:
    xor ax, ax
    mov al, byte ptr ss:[bp-2]
    test al, al
    jne burst_count
    mov ax, 256
burst_count:
    mov bx, 255
    mul bx
    mov si, ax
    mov ax, word ptr ss:[bp-14]
    mov es, ax
    mov ax, word ptr ss:[bp-4]
    mov byte ptr es:[0x9e8a], al
    mov byte ptr es:[0x9e89], 0
    xor ah, ah
    mov di, ax
    test di, di
    jne burst_loop
    mov di, 256
burst_loop:
    test si, si
    je burst_done
    cmp si, di
    jb burst_tail
    mov ax, di
    mov bx, 10
    mul bx
    call wait_ticks32
    sub si, di
    cmp byte ptr es:[0x4b3c], 0
    jne burst_muted
    in al, 0x61
    and al, 0xfe
    xor al, 2
    out 0x61, al
burst_muted:
    mov di, 256
    jmp burst_loop
burst_tail:
    mov ax, si
    mov bx, 10
    mul bx
    call wait_ticks32
burst_done:
    xor ax, ax
    mov al, byte ptr ss:[bp-2]
    test al, al
    jne burst_result
    mov ax, 256
burst_result:
    mov bx, 255
    mul bx
    sub byte ptr ss:[bp-4], al
    mov byte ptr ss:[bp-6], 0
    mov word ptr ss:[bp+2], 0x71c3
    jmp timer_done

delay_2078:
    mov word ptr ss:[bp+2], 0x1e83
    jmp delay_bp2
delay_2098:
    mov word ptr ss:[bp+2], 0x1ea3
    jmp delay_bp2
delay_2718:
    mov word ptr ss:[bp+2], 0x257a
    mov bx, word ptr ss:[bp]
    mov word ptr ss:[bx+0x14], 10000
    mov cx, 10000
    jmp delay_scaled
delay_4377:
    mov word ptr ss:[bp+2], 0x4183
    mov bx, word ptr ss:[bp]
    mov word ptr ss:[bx+2], 10000
    mov cx, 10000
    jmp delay_scaled
delay_44dc:
    mov word ptr ss:[bp+2], 0x42e7
    jmp delay_bp4
delay_4509:
    mov word ptr ss:[bp+2], 0x4314
    jmp delay_bp4
delay_4b21:
    mov word ptr ss:[bp+2], 0x492c
    mov bx, word ptr ss:[bp]
    mov cx, word ptr ss:[bx+6]
    mov word ptr ss:[bx+6], 0
    jmp delay_scaled
delay_669a:
    mov word ptr ss:[bp+2], 0x64a6
    mov bx, word ptr ss:[bp]
    mov word ptr ss:[bx+6], 2001
    mov cx, 2001
    jmp delay_scaled
delay_7600:
    mov word ptr ss:[bp+2], 0x740c
    mov bx, word ptr ss:[bp]
    mov word ptr ss:[bx+0x14], 1001
    mov cx, 1001
    jmp delay_scaled
delay_761c:
    mov word ptr ss:[bp+2], 0x7428
    mov bx, word ptr ss:[bp]
    mov word ptr ss:[bx+0x12], 3800
    mov cx, 3800
    jmp delay_scaled
delay_7e3f:
    mov word ptr ss:[bp+2], 0x7c4a
    jmp delay_bp2
delay_7e5f:
    mov word ptr ss:[bp+2], 0x7c6a
    jmp delay_bp2
delay_bp2:
    mov bx, word ptr ss:[bp]
    mov cx, word ptr ss:[bx+2]
    mov word ptr ss:[bx+2], 0
    jmp delay_scaled
delay_bp4:
    mov bx, word ptr ss:[bp]
    mov cx, word ptr ss:[bx+4]
    mov word ptr ss:[bx+4], 0
delay_scaled:
    test cx, cx
    jle timer_done
    mov ax, cx
    mov bx, 14
    mul bx
    call wait_ticks32
timer_done:
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    pop bp
    iret

# The measurement loop is the same two-byte LOOP-to-self as the game uses.
# Interrupts stay masked for this short calibration so BIOS work cannot
# inflate the result. A minimum divisor avoids overflow on very fast CPUs.
calibrate_voice:
    pushf
    cli
    call read_pit
    mov si, ax
    mov cx, 8192
voice_measure_loop:
    loop voice_measure_loop
    call read_pit
    sub si, ax
    popf
    cmp si, 128
    jae voice_measure_ok
    mov si, 128
voice_measure_ok:
    mov word ptr [voice_loop_pit], si
    ret

# Wait DX:AX PIT input clocks in pieces shorter than one 16-bit wrap.
wait_ticks32:
    push si
    push di
    mov si, ax
    mov di, dx
wait_piece:
    test di, di
    jne wait_full_piece
    cmp si, 50000
    jae wait_full_piece
    test si, si
    je wait_all_done
    mov ax, si
    call wait_pit
    jmp wait_all_done
wait_full_piece:
    mov ax, 50000
    sub si, ax
    sbb di, 0
    call wait_pit
    jmp wait_piece
wait_all_done:
    pop di
    pop si
    ret

wait_pit:
    push bx
    push cx
    push dx
    mov dx, ax
    call read_pit
    mov bx, ax
wait_pit_loop:
    call read_pit
    mov cx, bx
    sub cx, ax
    cmp cx, dx
    jb wait_pit_loop
    pop dx
    pop cx
    pop bx
    ret

# Latch PIT channel 0 without changing its mode or divisor.
read_pit:
    pushf
    cli
    xor al, al
    out 0x43, al
    in al, 0x40
    mov ah, al
    in al, 0x40
    xchg al, ah
    popf
    ret

# SeaVGABIOS and some other later BIOSes reset INT 1Fh on mode changes.
# Castle Wolfenstein installs its 128 custom graphics glyphs before setting
# mode 4/8. Restore them after the BIOS has changed modes. Other video calls
# are passed through without modifying the calling convention.
video_hook:
    cmp ax, 4
    je video_mode
    cmp ax, 8
    je video_mode
    jmp far ptr cs:[old_video]
video_mode:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push bp
    push ds
    push es
    pushf
    call far ptr cs:[old_video]
    mov ax, ds
    mov es, ax
    mov bp, 0x46c0
    mov ax, 0x1120
    pushf
    call far ptr cs:[old_video]
    pop es
    pop ds
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    iret

old_vector: .word 0, 0
old_video: .word 0, 0
old_font: .word 0, 0
old_mode: .byte 3
psp_seg: .word 0
last_tick: .word 0
tick_seen: .byte 0
tick_spacing: .byte 8
voice_factor: .byte 2
accurate_action: .byte 0
voice_loop_pit: .word 34816
action_active: .byte 0
action_expected: .byte 0
action_target: .byte 0
action_last_tick: .word 0
action_remainder: .word 0
