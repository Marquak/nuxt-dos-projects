# TEXTMODE 1.0: 80x25 / 80x50 VGA text, without ANSI.SYS.
# Original code, MIT license. GNU assembler, Intel syntax, 8086 instructions.
.intel_syntax noprefix
.code16
.arch i8086
.section .text
.global _start
_start:
    push cs
    pop ds
    cld
    xor cx, cx
    mov cl, byte ptr [0x80]
    mov si, 0x81
skip_leading:
    jcxz usage
    lodsb
    dec cx
    cmp al, ' '
    je skip_leading
    cmp al, 9
    je skip_leading
    jcxz usage
    mov ah, byte ptr [si]
    inc si
    dec cx
    mov dl, 25
    cmp ax, 0x3532                # "25", little endian
    je trailing
    mov dl, 50
    cmp ax, 0x3035                # "50"
    jne usage
trailing:
    jcxz parsed
    lodsb
    dec cx
    cmp al, ' '
    je trailing
    cmp al, 9
    je trailing
usage:
    mov dx, offset usage_msg
    mov ax, 0x4c01
    jmp message_exit
parsed:
    mov byte ptr [target], dl
    mov ax, 0x1a00                # VGA display combination code
    xor bx, bx
    int 0x10
    cmp al, 0x1a
    jne no_vga
    cmp bl, 7                     # VGA mono display
    je set_mode
    cmp bl, 8                     # VGA colour display
    je set_mode
no_vga:
    mov dx, offset vga_msg
    mov ax, 0x4c02
    jmp message_exit
set_mode:
    mov ax, 0x1202                # Select 400 scanlines for next mode set
    mov bl, 0x30
    int 0x10
    mov ax, 3                     # BIOS call preserves installed INT 10 hooks
    int 0x10
    cmp byte ptr [target], 50
    jne verify
    mov ax, 0x1112                # ROM 8x8 font; update row count / text geometry
    xor bx, bx                    # Font block 0
    int 0x10
    mov ah, 1
    mov cx, 0x0607                # Two-line underline cursor for 8x8 font
    int 0x10
verify:
    mov ax, 0x40
    mov es, ax
    cmp byte ptr es:[0x49], 3
    jne failed
    cmp word ptr es:[0x4a], 80
    jne failed
    mov al, byte ptr es:[0x84]
    inc al
    cmp al, byte ptr [target]
    jne failed
    mov ax, 0x4c00
    int 0x21                     # Normal exit, no TSR or allocated memory remains
failed:
    mov dx, offset fail_msg
    mov ax, 0x4c03
message_exit:
    push ax
    mov ah, 9
    int 0x21
    pop ax
    int 0x21

target: .byte 0
usage_msg: .ascii "Usage: TEXTMODE 25 or TEXTMODE 50\r\n$"
vga_msg: .ascii "VGA BIOS required.\r\n$"
fail_msg: .ascii "BIOS did not select the requested text mode.\r\n$"
