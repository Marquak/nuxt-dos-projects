# NUXTDASH 1.3 - 8086/8088-compatible, DOS 2.0+, GNU assembler Intel syntax.
# No 186+ instructions or direct port I/O. Read-only x87 control-word probe.
.intel_syntax noprefix
.code16
.arch i8086
.section .text
.global _start, mem_end

.macro textat row, col, label
    mov di, OFFSET frame + (\row)*160 + (\col)*2
    mov si, OFFSET \label
    call puts
.endm
.macro numat row, col, value
    mov di, OFFSET frame + (\row)*160 + (\col)*2
    mov ax, word ptr [\value]
    call decimal
.endm
.macro hexat row, col, value
    mov di, OFFSET frame + (\row)*160 + (\col)*2
    mov ax, word ptr [\value]
    call hexword
.endm

_start:
    # DOS may load a COM file with less than a full 64 KiB allocation.
    # Check our NOLOAD buffers and stack fit before using either.
    push cs
    pop ds
    mov ax, word ptr [2]
    mov bx, cs
    sub ax, bx
    mov dx, OFFSET mem_end
    add dx, 15
    mov cl, 4
    shr dx, cl
    cmp ax, dx
    jae enough_memory
    mov dx, OFFSET low_memory
    mov ah, 9
    int 0x21
    int 0x20
enough_memory:
    cld
    cli
    mov ax, cs
    mov ss, ax
    mov sp, OFFSET stack_top
    sti
    mov ds, ax
    mov es, ax
    mov ah, 0x30
    int 0x21
    mov word ptr [dosver], ax
    cmp al, 2
    jae version_ok
    mov dx, OFFSET old_dos
    mov ah, 9
    int 0x21
    int 0x20
version_ok:
    xor cx, cx
    mov cl, byte ptr [0x80]
    mov si, 0x81
arg_loop:
    jcxz args_done
    lodsb
    cmp al, '?'
    je usage
    loop arg_loop
args_done:
    # Release the rest of the transient program area, retaining our buffers/stack.
    mov bx, OFFSET mem_end
    add bx, 15
    mov cl, 4
    shr bx, cl
    mov word ptr [resident], bx
    mov ah, 0x4a
    int 0x21
    mov byte ptr [shrink_failed], 0
    jnc resized
    mov byte ptr [shrink_failed], 1
resized:
    call video_start
    call gather
    mov byte ptr [page], 0
redraw:
    call draw
    call present
    call status
main_loop:
    mov ah, 1
    int 0x16
    jz idle
    xor ah, ah
    int 0x16
    cmp al, 27
    je quit
    cmp al, 'q'
    je quit
    cmp al, 'Q'
    je quit
    cmp al, 'r'
    je refresh
    cmp al, 'R'
    je refresh
    cmp al, 's'
    je save_report
    cmp al, 'S'
    je save_report
    cmp al, 9
    je next_page
    cmp ah, 0x4d
    je next_page
    cmp ah, 0x51
    je next_page
    cmp ah, 0x4b
    je previous_page
    cmp ah, 0x49
    je previous_page
    cmp al, '1'
    jb function_key
    cmp al, '6'
    ja function_key
    sub al, '1'
    jmp choose
function_key:
    cmp ah, 0x3b
    jb main_loop
    cmp ah, 0x40
    ja main_loop
    mov al, ah
    sub al, 0x3b
    jmp choose
next_page:
    mov al, byte ptr [page]
    inc al
    cmp al, 6
    jb choose
    xor al, al
    jmp choose
previous_page:
    mov al, byte ptr [page]
    or al, al
    jnz previous_decrement
    mov al, 6
previous_decrement:
    dec al
choose:
    cmp al, byte ptr [page]
    je main_loop
    mov byte ptr [page], al
    jmp redraw
refresh:
    call gather
    jmp redraw
idle:
    # HLT wakes for timer/keyboard IRQs. No busy loop on the XT.
    sti
    hlt
    call read_ticks
    sub ax, word ptr [last_tick]
    cmp ax, 18
    jb main_loop
    call status
    jmp main_loop
quit:
    call video_stop
    mov ax, 0x4c00
    int 0x21
usage:
    mov dx, OFFSET help_text
    mov ah, 9
    int 0x21
    mov ax, 0x4c00
    int 0x21

# Preserve an existing 80-column text screen and active page, including cursor.
# For a graphics/40-column start, select 80x25; restore the old mode on exit.
video_start:
    mov ah, 0x0f
    int 0x10
    and al, 0x7f
    mov byte ptr [old_mode], al
    mov byte ptr [old_page], bh
    mov byte ptr [old_columns], ah
    cmp ah, 80
    jne change_mode
    cmp al, 2
    je keep_mode
    cmp al, 3
    je keep_mode
    cmp al, 7
    je keep_mode
change_mode:
    mov byte ptr [mode_changed], 1
    mov ax, 3
    int 0x10
keep_mode:
    mov ah, 0x0f
    int 0x10
    mov byte ptr [active_page], bh
    mov byte ptr [current_mode], al
    mov ax, 0xb800
    cmp byte ptr [current_mode], 7
    jne video_segment
    mov ax, 0xb000
video_segment:
    mov word ptr [video_seg], ax
    mov ah, 3
    mov bh, byte ptr [active_page]
    int 0x10
    mov word ptr [old_cursor], dx
    mov word ptr [old_shape], cx
    mov ax, 0x40
    mov es, ax
    mov ax, word ptr es:[0x4e]
    mov word ptr [video_offset], ax
    # Save the first 25 rows, even if DOS was in 43/50-line mode.
    push ds
    mov ax, word ptr [video_seg]
    mov si, word ptr [video_offset]
    mov di, OFFSET saved_screen
    push cs
    pop es
    mov ds, ax
    mov cx, 2000
    rep movsw
    pop ds
    mov ah, 1
    mov cx, 0x2000
    int 0x10
    push cs
    pop es
    ret
video_stop:
    cmp byte ptr [mode_changed], 0
    jne restore_mode
    mov ax, word ptr [video_seg]
    mov es, ax
    mov di, word ptr [video_offset]
    mov si, OFFSET saved_screen
    mov cx, 2000
    rep movsw
    mov ah, 1
    mov cx, word ptr [old_shape]
    int 0x10
    mov ah, 2
    mov bh, byte ptr [active_page]
    mov dx, word ptr [old_cursor]
    int 0x10
    ret
restore_mode:
    xor ah, ah
    mov al, byte ptr [old_mode]
    int 0x10
    mov ah, 5
    mov al, byte ptr [old_page]
    int 0x10
    ret

gather:
    int 0x12
    mov word ptr [base_kb], ax
    int 0x11
    mov word ptr [equipment], ax
    mov ah, 0x19
    int 0x21
    mov byte ptr [drive], al
    # DOS AH=48h, impossible request: returns largest available block in BX.
    mov bx, 0xffff
    mov ah, 0x48
    int 0x21
    jc allocation_error
    # Defensive fallback: release any allocation if an unusual DOS succeeds.
    mov es, ax
    mov ah, 0x49
    int 0x21
    xor bx, bx
    jmp free_failed
allocation_error:
    cmp ax, 8
    je free_failed
    xor bx, bx
free_failed:
    mov word ptr [free_para], bx
    mov ax, 0x40
    mov es, ax
    xor bx, bx
    mov di, OFFSET ports
    mov cx, 7
ports_read:
    mov ax, word ptr es:[bx]
    mov word ptr [di], ax
    add bx, 2
    add di, 2
    loop ports_read
    mov ax, 0xf000
    mov es, ax
    mov al, byte ptr es:[0xfffe]
    mov byte ptr [model_id], al
    mov si, 0xfff5
    mov di, OFFSET bios_date
    mov cx, 8
bios_date_read:
    mov al, byte ptr es:[si]
    cmp al, 32
    jb sanitize
    cmp al, 126
    jbe date_store
sanitize:
    mov al, '?'
date_store:
    mov byte ptr [di], al
    inc si
    inc di
    loop bios_date_read
    mov byte ptr [video_kind], 0
    # VGA/MCGA display-combination query. Unsupported BIOS leaves AL != 1Ah.
    mov ax, 0x1a00
    xor bx, bx
    int 0x10
    cmp al, 0x1a
    jne try_ega
    mov byte ptr [video_kind], 2
    mov byte ptr [display_code], bl
    jmp gather_done
try_ega:
    mov ah, 0x12
    mov bx, 0x0010
    int 0x10
    cmp bl, 0x10
    je gather_done
    cmp bl, 3
    ja gather_done
    mov byte ptr [video_kind], 1
gather_done:
    call probe_vram
    call probe_fpu
    call measure_cpu
    call gather_umb
    call gather_drives
    call probe_system_bios
    push cs
    pop es
    ret

# Sergey Kiselev/Monotech system BIOS POST banner, within F8000-FFFFF.
# Version is read from ROM, never inferred from the board or ROM date.
probe_system_bios:
    mov word ptr [system_bios_text], OFFSET system_bios_unknown
    mov ax, 0xf000
    mov es, ax
    mov bx, 0x8000
system_bios_scan:
    mov si, OFFSET system_bios_marker
    mov di, bx
    mov cx, 15
    repe cmpsb
    jne system_bios_next
    mov al, byte ptr es:[di]
    cmp al, '0'
    jb system_bios_next
    cmp al, '9'
    ja system_bios_next
    mov si, OFFSET system_bios_version
    mov cx, 20
system_bios_copy:
    mov al, byte ptr es:[di]
    cmp al, '.'
    jne system_bios_character
    cmp byte ptr es:[di+1], ' '
    je system_bios_complete
system_bios_character:
    cmp al, 33
    jb system_bios_next
    cmp al, 126
    ja system_bios_next
    mov byte ptr [si], al
    inc si
    inc di
    loop system_bios_copy
    jmp system_bios_next
system_bios_complete:
    mov byte ptr [si], 0
    mov word ptr [system_bios_text], OFFSET system_bios_version
    jmp system_bios_done
system_bios_next:
    inc bx
    cmp bx, 0xffdb
    jbe system_bios_scan
system_bios_done:
    push cs
    pop es
    ret

# XTIDE r638 ROMVARS format XUB212: read configuration only. Never issue
# controller commands. ROM fields are NOT proof a controller/drive responds.
gather_drives:
    mov word ptr [xtide_seg], 0
    mov byte ptr [xtide_config], 0
    mov dx, 0xc000
xtide_scan:
    mov es, dx
    cmp word ptr es:[0], 0xaa55
    jne xtide_next
    cmp byte ptr es:[2], 1
    jb xtide_next
    cmp word ptr es:[6], 0x5558
    jne xtide_next
    cmp word ptr es:[8], 0x3242
    jne xtide_next
    cmp word ptr es:[10], 0x3231
    jne xtide_next
    mov word ptr [xtide_seg], dx
    mov si, 12
    mov di, OFFSET xtide_title
    mov cx, 32
    call rom_string
    mov si, 44
    mov di, OFFSET xtide_version
    mov cx, 18
    call rom_string
    mov al, byte ptr es:[0x48]
    mov byte ptr [xtide_count], al
    cmp al, 1
    jb xtide_found
    cmp al, 4
    ja xtide_found
    mov ax, word ptr es:[0x4e]
    mov word ptr [xtide_port], ax
    mov ax, word ptr es:[0x50]
    mov word ptr [xtide_control], ax
    mov al, byte ptr es:[0x52]
    mov byte ptr [xtide_type], al
    mov al, byte ptr es:[0x53]
    mov byte ptr [xtide_irq], al
    mov byte ptr [xtide_config], 1
    jmp xtide_found
xtide_next:
    add dx, 0x80
    cmp dx, 0xf800
    jbe xtide_scan
xtide_found:
    mov ax, 0x40
    mov es, ax
    mov al, byte ptr es:[0x75]
    mov byte ptr [disk_count], al
    mov byte ptr [floppy_count], 0
    mov ax, word ptr [equipment]
    test al, 1
    jz drives_clear
    mov cl, 6
    shr ax, cl
    and al, 3
    inc al
    mov byte ptr [floppy_count], al
drives_clear:
    push cs
    pop es
    mov di, OFFSET disks
    xor ax, ax
    mov cx, 32
    rep stosw
    mov si, OFFSET disks
    mov byte ptr [si+1], 0x80
    cmp byte ptr [disk_count], 0
    je floppies_query
    call query_hard_disk
    cmp byte ptr [disk_count], 2
    jb floppies_query
    mov si, OFFSET disks+16
    mov byte ptr [si+1], 0x81
    call query_hard_disk
floppies_query:
    mov si, OFFSET disks+32
    cmp byte ptr [floppy_count], 0
    je drives_done
    call query_geometry
    cmp byte ptr [floppy_count], 2
    jb drives_done
    mov si, OFFSET disks+48
    mov byte ptr [si+1], 1
    call query_geometry
drives_done:
    push cs
    pop es
    ret
rom_string:
    # Fixed-size, sanitized copy; embedded NUL terminates the displayed text.
    mov al, byte ptr es:[si]
    cmp al, 32
    jb rom_string_space
    cmp al, 126
    jbe rom_string_store
rom_string_space:
    xor al, al
rom_string_store:
    mov byte ptr [di], al
    inc si
    inc di
    loop rom_string
    mov byte ptr [di], 0
    ret

# Keep our pointer registers even with old BIOSes that clobber them.
disk_bios:
    push ds
    push es
    push si
    push di
    push bp
    stc
    int 0x13
    sti
    cld
    pop bp
    pop di
    pop si
    pop es
    pop ds
    ret

# SI -> 16-byte record: valid,id,cyl,heads,spt,sectors32,size_kind,error,type.
query_geometry:
    xor ax, ax
    mov es, ax
    xor di, di
    xor bx, bx
    xor cx, cx
    xor dx, dx
    mov dl, byte ptr [si+1]
    mov ax, 0x0800
    call disk_bios
    mov byte ptr [si+13], ah
    jc geometry_done
    test cl, 0x3f
    jz geometry_done
    # Reject phantom drives returned by some BIOSes for an absent index.
    mov al, byte ptr [si+1]
    and al, 0x7f
    cmp al, dl
    jae geometry_done
    mov byte ptr [si], 1
    mov byte ptr [si+14], bl
    xor ax, ax
    mov al, dh
    inc ax
    mov word ptr [si+4], ax
    mov al, cl
    and ax, 0x3f
    mov word ptr [si+6], ax
    mov al, ch
    mov ah, cl
    mov cl, 6
    shr ah, cl
    inc ax
    mov word ptr [si+2], ax
    # Cylinders*heads can reach 262144, so multiply the small factors first.
    mov ax, word ptr [si+4]
    mul word ptr [si+6]
    mul word ptr [si+2]
    mov word ptr [si+8], ax
    mov word ptr [si+10], dx
geometry_done:
    ret
query_hard_disk:
    call query_geometry
    cmp byte ptr [si], 0
    je hard_done
    mov bx, 0x55aa
    mov dl, byte ptr [si+1]
    mov ah, 0x41
    call disk_bios
    jc hard_legacy_size
    cmp bx, 0xaa55
    jne hard_legacy_size
    test cx, 1
    jz hard_legacy_size
    push si
    push cs
    pop es
    mov di, OFFSET disk_parameters
    xor ax, ax
    mov cx, 15
    rep stosw
    mov word ptr [disk_parameters], 30
    mov dl, byte ptr [si+1]
    mov si, OFFSET disk_parameters
    mov ah, 0x48
    call disk_bios
    pop si
    jc hard_legacy_size
    cmp word ptr [disk_parameters], 26
    jb hard_legacy_size
    cmp word ptr [disk_parameters+24], 512
    jne hard_legacy_size
    mov ax, word ptr [disk_parameters+20]
    or ax, word ptr [disk_parameters+22]
    jnz hard_legacy_size
    mov ax, word ptr [disk_parameters+16]
    mov dx, word ptr [disk_parameters+18]
    mov bx, ax
    or bx, dx
    jz hard_legacy_size
    mov word ptr [si+8], ax
    mov word ptr [si+10], dx
    mov byte ptr [si+12], 1
    ret
hard_legacy_size:
    mov dl, byte ptr [si+1]
    xor cx, cx
    xor dx, dx
    mov dl, byte ptr [si+1]
    mov ah, 0x15
    call disk_bios
    jc hard_done
    cmp ah, 3
    jne hard_done
    mov ax, cx
    or ax, dx
    jz hard_done
    mov word ptr [si+8], dx
    mov word ptr [si+10], cx
    mov byte ptr [si+12], 2
hard_done:
    ret

# Read-only BIOS memory-size queries. The 512-byte VBE information buffer
# reuses report scratch space, which is idle during gather. No mode changes,
# bank changes, direct port I/O, or VRAM pattern writes.
probe_vram:
    mov word ptr [video_blocks], 0
    mov word ptr [video_ram_source], OFFSET vram_unreported
    cmp byte ptr [video_kind], 2
    jne vram_probe_done
    push cs
    pop es
    mov di, OFFSET report_buffer
    xor ax, ax
    mov cx, 256
    cld
    rep stosw
    mov word ptr [report_buffer], 0x4256
    mov word ptr [report_buffer+2], 0x3245
    mov di, OFFSET report_buffer
    mov ax, 0x4f00
    int 0x10
    push cs
    pop ds
    cmp ax, 0x004f
    jne vram_try_trident
    cmp word ptr [report_buffer], 0x4556
    jne vram_try_trident
    cmp word ptr [report_buffer+2], 0x4153
    jne vram_try_trident
    cmp word ptr [report_buffer+4], 0x0100
    jb vram_try_trident
    mov ax, word ptr [report_buffer+18]
    or ax, ax
    jz vram_try_trident
    mov word ptr [video_blocks], ax
    mov word ptr [video_ram_source], OFFSET vram_vbe
    jmp vram_probe_done
vram_try_trident:
    # Vendor-specific calls are gated by the VGA ROM's signature and a
    # case-insensitive TRIDENT string within its declared first 32 KiB.
    mov ax, 0xc000
    mov es, ax
    cmp word ptr es:[0], 0xaa55
    jne vram_probe_done
    xor ax, ax
    mov al, byte ptr es:[2]
    or al, al
    jz vram_probe_done
    cmp al, 64
    jbe vram_rom_length
    mov al, 64
vram_rom_length:
    mov cl, 9
    shl ax, cl
    sub ax, 6
    mov cx, ax
    xor di, di
vram_rom_search:
    xor bx, bx
vram_rom_char:
    mov al, byte ptr es:[bx+di]
    and al, 0xdf
    cmp al, byte ptr [trident_name+bx]
    jne vram_rom_next
    inc bx
    cmp bx, 7
    jb vram_rom_char
    # Trident INT 10h AH=12h BL=12h, AL=12h success; AH=256 KiB banks.
    # AL starts at zero so an unsupported call cannot look successful.
    mov ax, 0x1200
    mov bx, 0x0012
    int 0x10
    push cs
    pop ds
    cmp al, 0x12
    jne vram_probe_done
    or ah, ah
    jz vram_probe_done
    mov al, ah
    xor ah, ah
    shl ax, 1
    shl ax, 1
    mov word ptr [video_blocks], ax
    mov word ptr [video_ram_source], OFFSET vram_trident
    jmp vram_probe_done
vram_rom_next:
    inc di
    loop vram_rom_search
vram_probe_done:
    push cs
    pop es
    cld
    ret

# Unsigned DX:AX decimal, retaining 8086-only arithmetic. Supports the full
# VBE word of 64 KiB blocks without wrapping at 64 MiB.
decimal32:
    push ax
    push bx
    push cx
    push dx
    push si
    push bp
    xor cx, cx
    mov bx, 10
decimal32_div:
    mov bp, ax
    mov ax, dx
    xor dx, dx
    div bx
    mov si, ax
    mov ax, bp
    div bx
    push dx
    inc cx
    mov dx, si
    or si, ax
    jnz decimal32_div
decimal32_out:
    pop ax
    add al, '0'
    call putc
    loop decimal32_out
    pop bp
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

# Two different sentinels must become identical control words. No FNINIT,
# FWAIT, stack operations or exception clearing: leave arithmetic state alone.
# Intel AP-113 documents synchronization of original-8087 FNSTCW stores.
probe_fpu:
    mov byte ptr [fpu_response], 0
    mov word ptr [fpu_cw1], 0x5a5a
    mov word ptr [fpu_cw2], 0xa5a5
    .byte 0xd9, 0x3e
    .word fpu_cw1
    .byte 0xd9, 0x3e
    .word fpu_cw2
    mov ax, word ptr [fpu_cw1]
    cmp ax, word ptr [fpu_cw2]
    jne probe_fpu_done
    mov byte ptr [fpu_response], 1
probe_fpu_done:
    ret

# NEC AAD ignores its immediate radix; Intel uses it. Only NEC-compatible
# timing is calibrated here. V20 and V30 cannot be distinguished by this.
# Use four standard 18.2065 Hz BDA ticks; never reprogram PIT or mask IRQs.
# 512 unrolled AAM instructions = 7680 clocks at NEC's documented 15 each.
# Bookkeeping adds approximately 60 clocks. Interrupt/refresh/prefetch costs
# make this a rough effective-clock estimate, not a frequency counter.
measure_cpu:
    mov word ptr [cpu_speed], 0
    mov word ptr [cpu_message], OFFSET cpu_other
    mov ax, 0x0101
    .byte 0xd5, 0x10
    cmp al, 11
    jne cpu_done
    mov word ptr [cpu_message], OFFSET cpu_no_timer
    push es
    mov ax, 0x40
    mov es, ax
    mov bx, word ptr es:[0x6c]
    mov cx, 0xffff
cpu_sync:
    mov ax, word ptr es:[0x6c]
    cmp ax, bx
    jne cpu_synced
    loop cpu_sync
    jmp cpu_measure_done
cpu_synced:
    mov bx, ax
    xor si, si
    mov cx, 1024
cpu_work:
    xor ax, ax
    .rept 512
    aam
    .endr
    inc si
    mov ax, word ptr es:[0x6c]
    sub ax, bx
    cmp ax, 4
    jae cpu_elapsed
    dec cx
    jz cpu_measure_done
    jmp cpu_work
cpu_elapsed:
    # Reject midnight discontinuity or an interrupted sample exceeding 4 ticks.
    cmp ax, 4
    jne cpu_measure_done
    mov ax, si
    mov bx, 3523
    mul bx
    mov bx, 1000
    cmp dx, bx
    jae cpu_measure_done
    div bx
    # Avoid displaying nonsense for unsupported emulators/configurations.
    cmp ax, 100
    jb cpu_measure_done
    cmp ax, 2500
    ja cpu_measure_done
    mov word ptr [cpu_speed], ax
cpu_measure_done:
    pop es
cpu_done:
    ret

# DOS 5+ UMB accounting. Temporarily link (only if necessary), walk validated
# MCBs, restore original link state on every scan exit. No allocation policy
# changes, physical probing, XMS calls or memory writes outside our segment.
gather_umb:
    mov byte ptr [umb_valid], 0
    mov byte ptr [umb_restore], 0
    mov word ptr [umb_total], 0
    mov word ptr [umb_free], 0
    mov word ptr [umb_largest], 0
    mov word ptr [umb_message], OFFSET umb_unknown
    cmp byte ptr [dosver], 5
    jb umb_done
    mov ax, 0x5802
    int 0x21
    jc umb_done
    cmp al, 1
    ja umb_done
    mov byte ptr [umb_old_link], al
    cmp al, 1
    je umb_linked
    mov ax, 0x5803
    mov bx, 1
    int 0x21
    jc umb_no_link
    mov byte ptr [umb_restore], 1
umb_linked:
    mov ah, 0x52
    int 0x21
    mov ax, word ptr es:[bx-2]
    call walk_mcb
    jmp umb_cleanup
umb_no_link:
    mov word ptr [umb_message], OFFSET umb_none
    jmp umb_done
umb_cleanup:
    cmp byte ptr [umb_restore], 0
    je umb_done
    mov ax, 0x5803
    xor bx, bx
    int 0x21
    jnc umb_done
    mov byte ptr [umb_valid], 0
    mov word ptr [umb_message], OFFSET umb_restore_error
umb_done:
    push cs
    pop es
    ret

# Input AX = first MCB segment; output totals in paragraphs.
walk_mcb:
    mov cx, 4096
mcb_next:
    cmp ax, 0x50
    jb mcb_bad
    mov es, ax
    mov dl, byte ptr es:[0]
    cmp dl, 'M'
    je mcb_type_ok
    cmp dl, 'Z'
    jne mcb_bad
mcb_type_ok:
    mov bx, word ptr es:[3]
    mov di, ax
    inc di
    jz mcb_bad
    add di, bx
    jc mcb_bad
    cmp di, ax
    jbe mcb_bad
    cmp ax, 0xa000
    jb mcb_skip
    mov si, word ptr es:[1]
    cmp si, 7
    je mcb_skip
    cmp si, 8
    jne mcb_count
    cmp word ptr es:[8], 0x4353
    je mcb_skip
mcb_count:
    add word ptr [umb_total], bx
    inc word ptr [umb_total]
    or si, si
    jnz mcb_skip
    add word ptr [umb_free], bx
    cmp bx, word ptr [umb_largest]
    jbe mcb_skip
    mov word ptr [umb_largest], bx
mcb_skip:
    cmp dl, 'Z'
    je mcb_complete
    mov ax, di
    loop mcb_next
mcb_bad:
    mov word ptr [umb_message], OFFSET umb_bad
    ret
mcb_complete:
    mov word ptr [umb_message], OFFSET umb_none
    cmp word ptr [umb_total], 0
    je mcb_return
    mov byte ptr [umb_valid], 1
    mov word ptr [umb_message], OFFSET umb_present
mcb_return:
    ret

para_kib:
    add ax, 32
    mov cl, 6
    shr ax, cl
    call decimal
    mov si, OFFSET kib
    call puts
    ret

# Render one whole page off-screen; steady-state updates touch only row 23.
draw:
    push cs
    pop es
    mov di, OFFSET frame
    mov ax, 0x1720
    cmp byte ptr [current_mode], 7
    jne clear_frame
    mov ax, 0x0720
clear_frame:
    mov cx, 2000
    rep stosw
    mov byte ptr [attr], 0x1f
    cmp byte ptr [current_mode], 7
    jne header_color
    mov byte ptr [attr], 0x0f
header_color:
    textat 1, 2, title
    mov byte ptr [attr], 0x70
    mov di, OFFSET frame + 3*160
    mov ax, 0x7020
    mov cx, 80
    rep stosw
    textat 3, 2, tabs
    mov byte ptr [attr], 0x1e
    cmp byte ptr [current_mode], 7
    jne body_color
    mov byte ptr [attr], 0x0f
body_color:
    cmp byte ptr [page], 0
    je overview
    cmp byte ptr [page], 1
    je memory_video
    cmp byte ptr [page], 2
    je ports_page
    cmp byte ptr [page], 3
    je vectors_page
    cmp byte ptr [page], 4
    je nuxt_page
    jmp drives_page

overview:
    textat 5, 3, overview_title
    textat 17, 3, system_bios_label
    mov si, word ptr [system_bios_text]
    mov di, OFFSET frame + 17*160 + 28*2
    call puts
    textat 7, 3, dos_label
    mov di, OFFSET frame + 7*160 + 28*2
    xor ax, ax
    mov al, byte ptr [dosver]
    call decimal
    mov al, '.'
    call putc
    mov al, byte ptr [dosver+1]
    call dec2
    textat 8, 3, base_label
    numat 8, 28, base_kb
    mov si, OFFSET kib
    call puts
    textat 9, 3, free_label
    mov di, OFFSET frame + 9*160 + 28*2
    call free_kib
    textat 10, 3, model_label
    mov di, OFFSET frame + 10*160 + 28*2
    mov al, byte ptr [model_id]
    call hexbyte
    textat 11, 3, date_label
    textat 11, 28, bios_date
    textat 12, 3, eq_label
    hexat 12, 28, equipment
    textat 13, 3, fpu_label
    mov si, OFFSET no_fpu
    test word ptr [equipment], 2
    jz fpu_text
    mov si, OFFSET yes_fpu
fpu_text:
    mov di, OFFSET frame + 13*160 + 28*2
    call puts
    textat 14, 3, drive_label
    mov di, OFFSET frame + 14*160 + 28*2
    mov al, byte ptr [drive]
    add al, 'A'
    call putc
    mov al, ':'
    call putc
    textat 15, 3, cpu_label
    mov di, OFFSET frame + 15*160 + 28*2
    cmp word ptr [cpu_speed], 0
    je cpu_unavailable
    mov ax, word ptr [cpu_speed]
    xor dx, dx
    mov bx, 100
    div bx
    call decimal
    mov al, '.'
    call putc
    mov ax, dx
    call dec2
    mov si, OFFSET cpu_units
    call puts
    jmp cpu_draw_done
cpu_unavailable:
    mov si, word ptr [cpu_message]
    call puts
cpu_draw_done:
    textat 16, 3, probe_label
    mov di, OFFSET frame + 16*160 + 28*2
    mov si, OFFSET probe_absent_text
    cmp byte ptr [fpu_response], 0
    je probe_draw
    mov si, OFFSET probe_present_text
probe_draw:
    call puts
    textat 18, 3, target_title
    textat 19, 3, target_cpu
    textat 20, 3, target_video
    textat 21, 3, overview_note
    jmp footer

memory_video:
    textat 5, 3, memory_title
    textat 7, 3, base_label
    numat 7, 30, base_kb
    mov si, OFFSET kib
    call puts
    textat 8, 3, free_label
    mov di, OFFSET frame + 8*160 + 30*2
    call free_kib
    textat 9, 3, resident_label
    mov di, OFFSET frame + 9*160 + 30*2
    mov ax, word ptr [resident]
    mov cl, 4
    shl ax, cl
    call decimal
    mov si, OFFSET bytes
    call puts
    textat 10, 3, umb_label
    mov di, OFFSET frame + 10*160 + 30*2
    mov si, word ptr [umb_message]
    call puts
    cmp byte ptr [umb_valid], 1
    jne umb_draw_done
    textat 11, 3, umb_tot_label
    mov di, OFFSET frame + 11*160 + 30*2
    mov ax, word ptr [umb_total]
    call para_kib
    mov si, OFFSET umb_used_label
    call puts
    mov ax, word ptr [umb_total]
    sub ax, word ptr [umb_free]
    call para_kib
    textat 12, 3, umb_free_label
    mov di, OFFSET frame + 12*160 + 30*2
    mov ax, word ptr [umb_free]
    call para_kib
    mov si, OFFSET umb_largest_label
    call puts
    mov ax, word ptr [umb_largest]
    call para_kib
umb_draw_done:
    textat 14, 3, video_label
    mov di, OFFSET frame + 14*160 + 30*2
    mov si, OFFSET legacy_video
    cmp byte ptr [video_kind], 1
    jne not_ega
    mov si, OFFSET ega_video
not_ega:
    cmp byte ptr [video_kind], 2
    jne video_text
    mov si, OFFSET vga_video
video_text:
    call puts
    textat 15, 3, mode_label
    mov di, OFFSET frame + 15*160 + 30*2
    mov al, byte ptr [current_mode]
    call hexbyte
    mov si, OFFSET page_label
    call puts
    xor ax, ax
    mov al, byte ptr [active_page]
    call decimal
    textat 16, 3, startup_label
    mov di, OFFSET frame + 16*160 + 30*2
    mov al, byte ptr [old_mode]
    call hexbyte
    mov si, OFFSET page_label
    call puts
    xor ax, ax
    mov al, byte ptr [old_page]
    call decimal
    textat 17, 3, vram_label
    hexat 17, 30, video_seg
    mov al, ':'
    call putc
    mov ax, word ptr [video_offset]
    call hexword
    textat 18, 3, dcc_label
    mov di, OFFSET frame + 18*160 + 30*2
    cmp byte ptr [video_kind], 2
    jne no_dcc
    mov al, byte ptr [display_code]
    call hexbyte
    jmp mem_notes
no_dcc:
    mov si, OFFSET na
    call puts
mem_notes:
    textat 19, 3, video_ram_label
    mov di, OFFSET frame + 19*160 + 30*2
    mov ax, word ptr [video_blocks]
    or ax, ax
    jz vram_unknown
    mov bx, 64
    mul bx
    call decimal32
    mov si, OFFSET kib
    call puts
vram_unknown:
    mov si, word ptr [video_ram_source]
    call puts
    textat 20, 3, memory_note1
    textat 21, 3, umb_note
    jmp footer

ports_page:
    textat 5, 3, ports_title
    textat 7, 3, ports_header
    xor bx, bx
    mov di, OFFSET frame + 9*160 + 3*2
serial_row:
    push di
    mov si, OFFSET com
    call puts
    mov ax, bx
    shr ax, 1
    inc ax
    call decimal
    pop di
    push di
    add di, 12*2
    mov ax, word ptr [ports+bx]
    call port_value
    pop di
    add di, 160
    add bx, 2
    cmp bx, 8
    jb serial_row
    mov bx, 8
    mov di, OFFSET frame + 14*160 + 3*2
parallel_row:
    push di
    mov si, OFFSET lpt
    call puts
    mov ax, bx
    sub ax, 6
    shr ax, 1
    call decimal
    pop di
    push di
    add di, 12*2
    mov ax, word ptr [ports+bx]
    call port_value
    pop di
    add di, 160
    add bx, 2
    cmp bx, 14
    jb parallel_row
    textat 19, 3, ports_note1
    textat 20, 3, ports_note2
    textat 21, 3, ports_note3
    jmp footer
port_value:
    or ax, ax
    jz port_missing
    call hexword
    mov si, OFFSET port_present
    call puts
    ret
port_missing:
    mov si, OFFSET port_absent
    call puts
    ret

vectors_page:
    textat 5, 3, vector_title
    textat 7, 3, vector_header
    mov bp, OFFSET vector_table
    mov di, OFFSET frame + 8*160 + 3*2
    mov cx, 12
vector_row:
    push cx
    push di
    mov al, byte ptr [bp]
    call hexbyte
    add di, 3*2
    mov si, word ptr [bp+1]
    call puts
    pop di
    push di
    add di, 29*2
    mov al, byte ptr [bp]
    mov ah, 0x35
    push es
    int 0x21
    mov ax, es
    pop es
    call hexword
    mov al, ':'
    call putc
    mov ax, bx
    call hexword
    pop di
    add di, 160
    add bp, 3
    pop cx
    loop vector_row
    textat 21, 3, vector_note
    jmp footer
drives_page:
    textat 5, 3, drives_title
    textat 7, 3, xtide_label
    mov di, OFFSET frame+7*160+22*2
    cmp word ptr [xtide_seg], 0
    je drive_no_rom
    mov ax, word ptr [xtide_seg]
    call hexword
    mov si, OFFSET xtide_rom_units
    call puts
    textat 8, 3, xtide_title
    textat 9, 3, xtide_version
    cmp byte ptr [xtide_config], 1
    jne drive_counts
    textat 10, 3, controller_label
    mov di, OFFSET frame+10*160+22*2
    xor bx, bx
    mov bl, byte ptr [xtide_type]
    cmp bl, 26
    ja controller_unknown
    test bl, 1
    jnz controller_unknown
    mov si, word ptr [controller_types+bx]
    call puts
    jmp controller_ports
controller_unknown:
    mov si, OFFSET controller_other
    call puts
    mov al, byte ptr [xtide_type]
    call hexbyte
controller_ports:
    textat 11, 3, controller_ports_label
    hexat 11, 25, xtide_port
    mov al, '/'
    call putc
    mov ax, word ptr [xtide_control]
    call hexword
    mov si, OFFSET irq_label
    call puts
    xor ax, ax
    mov al, byte ptr [xtide_irq]
    call decimal
    mov si, OFFSET controllers_label
    call puts
    xor ax, ax
    mov al, byte ptr [xtide_count]
    call decimal
    jmp drive_counts
drive_no_rom:
    mov si, OFFSET xtide_missing
    call puts
drive_counts:
    textat 12, 3, hd_count_label
    mov di, OFFSET frame+12*160+22*2
    xor ax, ax
    mov al, byte ptr [disk_count]
    call decimal
    mov si, OFFSET disk_limit
    call puts
    mov si, OFFSET disks
    mov di, OFFSET frame+13*160+3*2
    mov al, 0x80
    call hexbyte
    call render_disk
    mov si, OFFSET disks+16
    mov di, OFFSET frame+14*160+3*2
    mov al, 0x81
    call hexbyte
    call render_disk
    textat 16, 3, fd_count_label
    mov di, OFFSET frame+16*160+22*2
    xor ax, ax
    mov al, byte ptr [floppy_count]
    call decimal
    mov si, OFFSET disk_limit
    call puts
    mov si, OFFSET disks+32
    mov di, OFFSET frame+17*160+3*2
    mov al, 'A'
    call putc
    call render_floppy
    mov si, OFFSET disks+48
    mov di, OFFSET frame+18*160+3*2
    mov al, 'B'
    call putc
    call render_floppy
    textat 20, 3, disk_note
    textat 21, 3, floppy_note
    jmp footer
render_floppy:
    mov al, ':'
    call putc
    cmp byte ptr [si], 0
    je render_disk
    push si
    mov si, OFFSET two_spaces
    call puts
    pop si
    push si
    xor bx, bx
    mov bl, byte ptr [si+14]
    cmp bl, 6
    jbe floppy_type_ok
    xor bx, bx
floppy_type_ok:
    shl bx, 1
    mov si, word ptr [floppy_types+bx]
    call puts
    pop si
    ret
render_disk:
    mov bp, si
    mov si, OFFSET disk_unavailable
    cmp byte ptr [bp], 0
    je disk_text_return
    mov si, OFFSET chs_label
    call puts
    mov ax, word ptr [bp+2]
    call decimal
    mov al, '/'
    call putc
    mov ax, word ptr [bp+4]
    call decimal
    mov al, '/'
    call putc
    mov ax, word ptr [bp+6]
    call decimal
    mov si, OFFSET two_spaces
    call puts
    # Whole MiB from 512-byte sectors, retaining both 16-bit halves.
    mov ax, word ptr [bp+8]
    mov dx, word ptr [bp+10]
    mov cx, 11
disk_mib_shift:
    shr dx, 1
    rcr ax, 1
    loop disk_mib_shift
    call decimal32
    mov si, OFFSET disk_mib_chs
    cmp byte ptr [bp+12], 0
    je disk_text_return
    mov si, OFFSET disk_mib_bios
disk_text_return:
    call puts
    ret

nuxt_page:
    textat 5, 3, nuxt_title
    textat 7, 3, nuxt_1
    textat 8, 3, nuxt_2
    textat 9, 3, nuxt_3
    textat 10, 3, nuxt_4
    textat 12, 3, nuxt_5
    textat 13, 3, nuxt_6
    textat 14, 3, nuxt_7
    textat 16, 3, nuxt_8
    textat 17, 3, nuxt_9
    textat 18, 3, nuxt_10
    textat 20, 3, nuxt_11
    textat 21, 3, nuxt_12
footer:
    mov byte ptr [attr], 0x70
    mov di, OFFSET frame + 24*160
    mov ax, 0x7020
    mov cx, 80
    rep stosw
    mov si, word ptr [footer_message]
    mov di, OFFSET frame + 24*160 + 2*2
    call puts
    ret

# Explicit save only. DOS 3+ create-new prevents replacing existing files.
# One buffered write per page, no disk access in the idle/status path.
save_report:
    mov word ptr [footer_message], OFFSET save_failed
    cmp byte ptr [dosver], 3
    jb save_not_supported
    call gather
    mov byte ptr [report_name+4], '0'
    mov byte ptr [report_name+5], '0'
create_report:
    mov dx, OFFSET report_name
    xor cx, cx
    mov ah, 0x5b
    int 0x21
    jnc report_open
    cmp ax, 80
    jne report_return
    inc byte ptr [report_name+5]
    cmp byte ptr [report_name+5], '9'
    jbe create_report
    mov byte ptr [report_name+5], '0'
    inc byte ptr [report_name+4]
    cmp byte ptr [report_name+4], '9'
    jbe create_report
    mov word ptr [footer_message], OFFSET save_full
    jmp report_return
report_open:
    mov word ptr [report_handle], ax
    mov al, byte ptr [page]
    mov byte ptr [saved_page], al
    mov byte ptr [page], 0
report_page:
    call draw
    mov si, OFFSET frame
    mov di, OFFSET report_buffer
    mov bx, 23
report_row:
    mov cx, 80
report_char:
    lodsb
    stosb
    inc si
    loop report_char
    mov ax, 0x0a0d
    stosw
    dec bx
    jnz report_row
    mov ax, 0x0a0d
    stosw
    mov bx, word ptr [report_handle]
    mov dx, OFFSET report_buffer
    mov cx, 1888
    mov ah, 0x40
    int 0x21
    jc report_write_failed
    cmp ax, 1888
    jne report_write_failed
    inc byte ptr [page]
    cmp byte ptr [page], 6
    jb report_page
    mov ah, 0x3e
    int 0x21
    jc report_restore
    mov word ptr [footer_message], OFFSET save_ok
    jmp report_restore
report_write_failed:
    mov ah, 0x3e
    int 0x21
    # Remove incomplete report after write failure.
    mov dx, OFFSET report_name
    mov ah, 0x41
    int 0x21
report_restore:
    mov al, byte ptr [saved_page]
    mov byte ptr [page], al
report_return:
    jmp redraw
save_not_supported:
    mov word ptr [footer_message], OFFSET save_old_dos
    jmp report_return

present:
    mov ax, word ptr [video_seg]
    mov es, ax
    mov di, word ptr [video_offset]
    mov si, OFFSET frame
    mov cx, 2000
    rep movsw
    push cs
    pop es
    ret

read_ticks:
    push es
    mov ax, 0x40
    mov es, ax
    # Atomic BDA read; unlike INT 1Ah/AH=00h, does not consume midnight flag.
    pushf
    cli
    mov ax, word ptr es:[0x6c]
    mov dx, word ptr es:[0x6e]
    popf
    pop es
    ret
status:
    push cs
    pop es
    mov di, OFFSET frame + 23*160
    mov ax, 0x1720
    cmp byte ptr [current_mode], 7
    jne status_clear
    mov ax, 0x0720
status_clear:
    mov cx, 80
    rep stosw
    mov byte ptr [attr], ah
    textat 23, 3, clock_label
    mov ah, 0x2c
    int 0x21
    mov di, OFFSET frame + 23*160 + 9*2
    mov al, ch
    call dec2
    mov al, ':'
    call putc
    mov al, cl
    call dec2
    mov al, ':'
    call putc
    mov al, dh
    call dec2
    textat 23, 23, ticks_label
    call read_ticks
    mov word ptr [last_tick], ax
    push ax
    mov ax, dx
    call hexword
    pop ax
    call hexword
    textat 23, 49, keys_label
    push es
    mov ax, 0x40
    mov es, ax
    mov al, byte ptr es:[0x17]
    pop es
    call hexbyte
    cmp byte ptr [shrink_failed], 0
    je status_copy
    textat 23, 63, shrink_note
status_copy:
    mov ax, word ptr [video_seg]
    mov es, ax
    mov di, word ptr [video_offset]
    add di, 23*160
    mov si, OFFSET frame + 23*160
    mov cx, 80
    rep movsw
    push cs
    pop es
    ret

# String/number primitives: ES:DI destination, DS:SI string, current attribute.
# Preserve working registers except DI; puts advances SI past terminator.
puts:
    push ax
puts_loop:
    lodsb
    or al, al
    jz puts_done
    call putc
    jmp puts_loop
puts_done:
    pop ax
    ret
putc:
    push ax
    mov ah, byte ptr [attr]
    stosw
    pop ax
    ret
hexword:
    push ax
    mov al, ah
    call hexbyte
    pop ax
    call hexbyte
    ret
hexbyte:
    push ax
    push cx
    mov cl, 4
    shr al, cl
    call nibble
    pop cx
    pop ax
    push ax
    and al, 15
    call nibble
    pop ax
    ret
nibble:
    and al, 15
    add al, '0'
    cmp al, '9'
    jbe nibble_out
    add al, 7
nibble_out:
    call putc
    ret
dec2:
    push ax
    push bx
    xor ah, ah
    mov bl, 10
    div bl
    add al, '0'
    call putc
    mov al, ah
    add al, '0'
    call putc
    pop bx
    pop ax
    ret
decimal:
    push ax
    push bx
    push cx
    push dx
    xor cx, cx
    mov bx, 10
decimal_div:
    xor dx, dx
    div bx
    push dx
    inc cx
    or ax, ax
    jnz decimal_div
decimal_out:
    pop ax
    add al, '0'
    call putc
    loop decimal_out
    pop dx
    pop cx
    pop bx
    pop ax
    ret
free_kib:
    mov ax, word ptr [free_para]
    mov cl, 6
    shr ax, cl
    call decimal
    mov si, OFFSET kib
    call puts
    ret

.section .data
title: .asciz "NUXTDASH  /  XT hardware dashboard                              v1.3"
tabs: .asciz "1 Overview  2 Memory/Video  3 Ports  4 Interrupts  5 NuXT  6 Drives"
footer_text: .asciz "1-6/F1-F6/Arrows: Pages   R: Refresh   S: Save report   Esc/Q: Exit"
footer_message: .word footer_text
startup_label: .asciz "Startup mode (hex)"
save_ok: .ascii "Saved "
report_name: .asciz "DASH00.TXT"
save_failed: .asciz "Report save failed. Check disk space/path. Esc/Q: Exit"
save_full: .asciz "DASH00.TXT through DASH99.TXT already exist. No files replaced."
save_old_dos: .asciz "Report saving needs DOS 3.0+. Dashboard still works on DOS 2."
report_handle: .word 0
saved_page: .byte 0
overview_title: .asciz "SYSTEM OVERVIEW"
dos_label: .asciz "DOS reported version"
base_label: .asciz "BIOS conventional RAM"
free_label: .asciz "Largest free DOS block"
model_label: .asciz "BIOS model byte (hex)"
date_label: .asciz "ROM date field"
eq_label: .asciz "Equipment word (hex)"
fpu_label: .asciz "BIOS coprocessor flag"
drive_label: .asciz "Current DOS drive"
yes_fpu: .asciz "Set (not execution-tested)"
no_fpu: .asciz "Clear (not execution-tested)"
target_title: .asciz "YOUR TARGET PROFILE  (user supplied; not hardware detection)"
target_cpu: .asciz "NuXT / NEC V20 -- executable uses only 8086/8088 instructions"
target_video: .asciz "Trident SVGA / 512 KiB VRAM -- dashboard uses text mode"
overview_note: .asciz "Target profile is user supplied; BIOS/DOS values are queried."
memory_title: .asciz "MEMORY / VIDEO"
resident_label: .asciz "Program block requested"
psp_label: .asciz "Program / PSP segment"
environment_label: .asciz "Environment segment"
video_label: .asciz "Video BIOS interface"
legacy_video: .asciz "Legacy / no EGA-VGA response"
ega_video: .asciz "EGA query available"
vga_video: .asciz "VGA/MCGA query available"
mode_label: .asciz "Dashboard mode (hex)"
page_label: .asciz "    active page "
vram_label: .asciz "Text buffer segment:offset"
dcc_label: .asciz "Display-combination code"
na: .asciz "N/A"
memory_note1: .asciz "Free block is the largest DOS allocation, not total free RAM."
memory_note2: .asciz "It excludes this program; DOS allocation/UMB policy applies."
memory_note3: .asciz "No destructive memory test."
memory_note4: .asciz "VRAM size and graphics modes are not inferred from text mode."
ports_title: .asciz "SERIAL / PARALLEL PORTS"
ports_header: .asciz "PORT        I/O BASE (hex)     BIOS DATA AREA ENTRY"
com: .asciz "COM"
lpt: .asciz "LPT"
port_present: .asciz "               Present"
port_absent: .asciz "----               Not listed"
ports_note1: .asciz "Read from 0040:0000 (COM1-4) and 0040:0008 (LPT1-3)."
ports_note2: .asciz "A BIOS entry does not prove a device is connected or working."
ports_note3: .asciz "UART model, FIFO, baud rate and IRQ assignments are not probed."
vector_title: .asciz "INTERRUPT VECTORS  /  current handler addresses"
vector_header: .asciz "INT  SERVICE                 SEGMENT:OFFSET (hex)"
vector_note: .asciz "Addresses are a snapshot. They do not identify handler ownership."
v08: .asciz "Timer IRQ"
v09: .asciz "Keyboard IRQ"
v10: .asciz "Video BIOS"
v11: .asciz "Equipment BIOS"
v12: .asciz "Memory-size BIOS"
v13: .asciz "Disk BIOS"
v16: .asciz "Keyboard BIOS"
v17: .asciz "Printer BIOS"
v1a: .asciz "Clock BIOS"
v21: .asciz "DOS services"
v2f: .asciz "Multiplex services"
v33: .asciz "Mouse driver (if any)"
vector_table:
.byte 0x08
.word v08
.byte 0x09
.word v09
.byte 0x10
.word v10
.byte 0x11
.word v11
.byte 0x12
.word v12
.byte 0x13
.word v13
.byte 0x16
.word v16
.byte 0x17
.word v17
.byte 0x1a
.word v1a
.byte 0x21
.word v21
.byte 0x2f
.word v2f
.byte 0x33
.word v33
nuxt_title: .asciz "NUXT v2.0 REFERENCE  /  manual defaults, NOT detected settings"
nuxt_1: .asciz "CPU clocks     4.77 / 7.16 / 9.55 MHz (estimate on Overview)"
nuxt_2: .asciz "RS-232         COM1  03F8h / IRQ4    [SW3 default]"
nuxt_3: .asciz "PS/2 mouse     COM2  02F8h / IRQ3    [SW4; serial converter]"
nuxt_4: .asciz "Parallel       0378h / IRQ7         [SW5 default]"
nuxt_5: .asciz "XT-IDE Rev2    0170h; boot ROM normally starts at F0000h"
nuxt_6: .asciz "UART hardware  16C552: two 16550 UARTs + bidirectional parallel"
nuxt_7: .asciz "Video option   Trident TVGA9000i; your VRAM configuration: 512 KiB"
nuxt_8: .asciz "C0000-C7FFF    VGA BIOS; keep the overlapping UMB bank disabled"
nuxt_9: .asciz "C8000-EFFFF    Five 32 KiB UMB banks (160 KiB), if enabled/free"
nuxt_10: .asciz "F0000-FFFFF    System ROM, including XT-IDE and system BIOS"
nuxt_11: .asciz "Reference: Monotech NuXT v2.0 User Manual, May 2020, pp. 3-13."
nuxt_12: .asciz "Switches, firmware, add-in cards and DOS drivers can alter this."
clock_label: .asciz "Time"
ticks_label: .asciz "BIOS ticks "
keys_label: .asciz "Key flags "
shrink_note: .asciz "Resize failed"
kib: .asciz " KiB"
bytes: .asciz " bytes"
low_memory: .ascii "NUXTDASH needs about 22 KiB of program memory.\r\n$"
old_dos: .ascii "NUXTDASH requires DOS 2.0 or later.\r\n$"
help_text: .ascii "NUXTDASH 1.3 - XT hardware dashboard\r\n"
 .ascii "Run DASH.COM under DOS 2.0+ on an 8088/V20 or newer PC.\r\n"
 .ascii "1-6/F1-F6/Arrows/Tab: pages. R: refresh. S: save report. Esc/Q: exit.\r\n"
 .ascii "Existing 80-column text screen and cursor are restored on exit.\r\n"
 .ascii "Graphics/40-column starts switch to 80x25; old mode is restored,\r\n"
 .ascii "but previous graphics/40-column screen contents are not saved.\r\n$"
drives_title: .asciz "DRIVES / CONTROLLER BIOS INFORMATION"
xtide_label: .asciz "XTIDE ROM segment"
xtide_rom_units: .asciz "h (XUB212 layout)"
xtide_missing: .asciz "No supported XUB212 ROM found"
controller_label: .asciz "Controller 1 config"
controller_ports_label: .asciz "Command/control base"
irq_label: .asciz "  IRQ "
controllers_label: .asciz "  Controllers "
controller_other: .asciz "Type code "
hd_count_label: .asciz "BIOS hard disks"
fd_count_label: .asciz "BIOS floppy drives"
disk_limit: .asciz " (first two shown)"
chs_label: .asciz "h  CHS "
two_spaces: .asciz "  "
disk_unavailable: .asciz "  Not listed / query unavailable"
disk_mib_chs: .asciz " MiB (CHS estimate)"
disk_mib_bios: .asciz " MiB (BIOS capacity)"
disk_note: .asciz "ROM config is not a hardware test. Geometry may be translated."
floppy_note: .asciz "Floppy type is BIOS-configured; Gotek reports its emulated type."
fd_unknown: .asciz "Type unknown (BIOS query succeeds)"
fd360: .asciz "360 KiB, 5.25 inch"
fd1200: .asciz "1.2 MB, 5.25 inch"
fd720: .asciz "720 KiB, 3.5 inch"
fd1440: .asciz "1.44 MB, 3.5 inch"
fd2880: .asciz "2.88 MB (BIOS type 5/6)"
floppy_types: .word fd_unknown,fd360,fd1200,fd720,fd1440,fd2880,fd2880
ct0: .asciz "16-bit ATA"
ct2: .asciz "32-bit ATA"
ct4: .asciz "ATA in 8-bit mode"
ct6: .asciz "Juko D16-X"
ct8: .asciz "XTIDE rev 1"
ct10: .asciz "XTIDE rev 2 (A0/A3 swapped)"
ct12: .asciz "XTIDE rev 2, Olivetti"
ct14: .asciz "XT-CF PIO8"
ct16: .asciz "XT-CF PIO8 + BIU offload"
ct18: .asciz "XT-CF PIO16 + BIU offload"
ct20: .asciz "XT-CF DMA"
ct22: .asciz "JR-IDE/ISA (memory mapped)"
ct24: .asciz "ADP50L (memory mapped)"
ct26: .asciz "Serial virtual controller"
controller_types: .word ct0,ct2,ct4,ct6,ct8,ct10,ct12,ct14,ct16,ct18,ct20,ct22,ct24,ct26
xtide_seg: .word 0
xtide_config: .byte 0
xtide_count: .byte 0
xtide_type: .byte 0
xtide_irq: .byte 0
xtide_port: .word 0
xtide_control: .word 0
xtide_title: .fill 33,1,0
xtide_version: .fill 19,1,0
disk_count: .byte 0
floppy_count: .byte 0
disks: .fill 64,1,0
disk_parameters: .fill 30,1,0
video_ram_label: .asciz "Video RAM reported"
video_blocks: .word 0
video_ram_source: .word vram_unreported
vram_unreported: .asciz "Unknown (no supported size query)"
vram_vbe: .asciz " (VBE BIOS)"
vram_trident: .asciz " (Trident BIOS)"
trident_name: .ascii "TRIDENT"
cpu_label: .asciz "CPU speed (approx.)"
cpu_units: .asciz " MHz (NEC timing; R refreshes)"
cpu_other: .asciz "N/A: needs V20/V30 timing"
cpu_no_timer: .asciz "N/A: timer/sample out of range"
probe_label: .asciz "8087 / x87 probe"
probe_present_text: .asciz "Responds (control word read)"
probe_absent_text: .asciz "No instruction response"
umb_label: .asciz "DOS-managed UMBs"
umb_tot_label: .asciz "Upper total / used"
umb_used_label: .asciz "    used "
umb_free_label: .asciz "Upper free / largest"
umb_largest_label: .asciz "    largest "
umb_present: .asciz "Present (DOS MCB chain)"
umb_none: .asciz "None exposed by DOS"
umb_unknown: .asciz "Unavailable (needs DOS 5+ API)"
umb_bad: .asciz "Unavailable: invalid MCB chain"
umb_restore_error: .asciz "Warning: UMB link restore failed"
umb_note: .asciz "UMB KiB rounded nearest; used includes headers; reserved holes excluded."
cpu_speed: .word 0
cpu_message: .word cpu_other
fpu_cw1: .word 0
fpu_cw2: .word 0
fpu_response: .byte 0
umb_message: .word umb_unknown
umb_total: .word 0
umb_free: .word 0
umb_largest: .word 0
umb_valid: .byte 0
umb_restore: .byte 0
umb_old_link: .byte 0
dosver: .word 0
resident: .word 0
base_kb: .word 0
free_para: .word 0
equipment: .word 0
ports: .fill 7,2,0
system_bios_marker: .ascii " BIOS, Version "
system_bios_label: .asciz "System BIOS version"
system_bios_unknown: .asciz "Unrecognized ROM banner"
system_bios_text: .word system_bios_unknown
system_bios_version: .fill 21,1,0
bios_date: .fill 9,1,0
video_seg: .word 0
video_offset: .word 0
old_cursor: .word 0
old_shape: .word 0
last_tick: .word 0
old_mode: .byte 0
old_columns: .byte 0
old_page: .byte 0
active_page: .byte 0
current_mode: .byte 0
mode_changed: .byte 0
shrink_failed: .byte 0
page: .byte 0
attr: .byte 0
model_id: .byte 0
drive: .byte 0
video_kind: .byte 0
display_code: .byte 0

.section .bss
.align 2
frame: .skip 4000
saved_screen: .skip 4000
report_buffer: .skip 1888
.align 16
stack_bottom: .skip 1024
stack_top:
mem_end:
.section .note.GNU-stack,"",@progbits
