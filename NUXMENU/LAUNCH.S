/* Tiny 8086 MZ supervisor. No C runtime or resident UI.
 * DOS 3+: get our path from the environment trailer, shrink to actual size,
 * EXEC NUI, then EXEC the chosen program. COMMAND.COM only for BAT/prompt.
 */
.intel_syntax noprefix
.code16
.arch i8086
.section .text
.global _start
_start:
 cld
 mov ax,ds
 mov cs:[psp],ax
 push cs
 pop ds
 mov es,ax
 mov si,0x80
 xor cx,cx
 mov cl,es:[si]
 cmp cx,117
 ja fatal
 mov byte ptr [uitail],cl
 add byte ptr [uitail],9
 inc si
 mov di,offset uitail+10
copy_tail:
 jcxz tail_done
 mov al,es:[si]
 mov [di],al
 inc si
 inc di
 loop copy_tail
tail_done:
 mov byte ptr [di],13
 mov byte ptr [di+1],0
 mov ax,es:[0x2c]
 or ax,ax
 jz fatal
 mov es,ax
 xor si,si
 /* Scan a DOS environment (at most 32 KiB). */
env_scan:
 cmp si,32760
 ja fatal
 cmp word ptr es:[si],0
 je env_found
 inc si
 jmp env_scan
env_found:
 add si,4
 mov di,offset home
 xor bx,bx
 mov cx,79
path_copy:
 mov al,es:[si]
 inc si
 mov [di],al
 inc di
 cmp al,'\\'
 jne path_next
 mov bx,di
path_next:
 or al,al
 jz path_done
 loop path_copy
 jmp fatal
path_done:
 or bx,bx
 jz fatal
 mov byte ptr [bx],0
 sub bx,offset home
 cmp bx,68
 ja fatal
 mov [home_len],bx
 push cs
 pop es
 mov si,offset home
 mov di,offset uipath
 call copy_string
 dec di
 mov si,offset ui_name
 call copy_string
 mov si,offset home
 mov di,offset jobpath
 call copy_string
 dec di
 mov si,offset job_name
 call copy_string
 mov si,offset home
 mov di,offset batpath
 call copy_string
 dec di
 mov si,offset bat_name
 call copy_string
 mov ah,0x19
 int 0x21
 add al,'A'
 mov [original],al
 mov si,offset original+3
 xor dx,dx
 mov ah,0x47
 int 0x21
 jc fatal
 mov es,[psp]
 mov bx,(image_end-_start+256+15)/16
 mov ah,0x4a
 int 0x21
 jc fatal
 push cs
 pop es
 mov word ptr [tailptr],offset uitail
menu_loop:
 mov dx,offset home
 call chdir
 jc fatal
 mov dx,offset jobpath
 mov ah,0x41
 int 0x21
 mov dx,offset batpath
 mov ah,0x41
 int 0x21
 mov dx,offset uipath
 call execute
 jc fatal
 mov ah,0x4d
 int 0x21
 cmp ax,10
 jne exit_ok
 /* Subsequent UI invocations restore navigation; no startup overrides. */
 mov word ptr [tailptr],offset returntail
 mov dx,offset jobpath
 mov ax,0x3d00
 int 0x21
 jc launch_error
 mov bx,ax
 mov dx,offset job
 mov cx,294
 mov ah,0x3f
 int 0x21
 pushf
 push ax
 mov ah,0x3e
 int 0x21
 pop ax
 popf
 jc launch_error
 cmp ax,294
 jne launch_error
 cmp word ptr [job],0x4d4e
 jne launch_error
 cmp word ptr [job+2],0x3431
 jne launch_error
 /* Terminate fields defensively; job comes only from our UI. */
 mov byte ptr [job+85],0
 mov byte ptr [job+165],0
 mov byte ptr [job+293],0
 mov dx,offset jobpath
 mov ah,0x41
 int 0x21
 mov dx,offset job+86
 call chdir
 jc launch_error
 mov si,offset job+166
 mov di,offset childtail+1
 mov al,32
 stosb
 mov cx,1
child_args:
 lodsb
 or al,al
 jz args_done
 cmp cx,126
 jae launch_error
 stosb
 inc cx
 jmp child_args
args_done:
 mov byte ptr [di],13
 mov byte ptr [di+1],0
 mov [childtail],cl
 mov word ptr [tailptr],offset childtail
 mov dx,offset job+6
 call execute
 mov word ptr [tailptr],offset returntail
 jc launch_error
 cmp byte ptr [job+4],0
 je menu_loop
 mov dx,offset pause_msg
 call print
 xor ax,ax
 int 0x16
 jmp menu_loop
launch_error:
 mov word ptr [tailptr],offset returntail
 mov dx,offset error_msg
 call print
 xor ax,ax
 int 0x16
 jmp menu_loop
exit_ok:
 mov [exit_code],al
 mov dx,offset original
 call chdir
 mov al,[exit_code]
 mov ah,0x4c
 int 0x21
fatal:
 push cs
 pop ds
 mov dx,offset error_msg
 call print
 cmp byte ptr [original],'A'
 jb fatal_exit
 mov dx,offset original
 call chdir
fatal_exit:
 mov ax,0x4c01
 int 0x21
/* DS:SI to ES:DI, including NUL. */
copy_string:
 lodsb
 stosb
 or al,al
 jnz copy_string
 ret
/* Absolute drive/path. Preserve carry on chdir failure. */
chdir:
 push dx
 mov si,dx
 mov dl,[si]
 and dl,0xdf
 sub dl,'A'
 cmp dl,25
 ja bad_drive
 mov ah,0x0e
 int 0x21
 pop dx
 mov word ptr [trimmed],0
 mov si,dx
cwd_end:
 cmp byte ptr [si],0
 je cwd_trim
 inc si
 jmp cwd_end
cwd_trim:
 mov bx,si
 sub bx,dx
 cmp bx,3
 jbe cwd_call
 cmp byte ptr [si-1],'\\'
 jne cwd_call
 dec si
 mov byte ptr [si],0
 mov [trimmed],si
cwd_call:
 push si
 mov ah,0x3b
 int 0x21
 pop si
 pushf
 mov si,[trimmed]
 or si,si
 jz cwd_done
 mov byte ptr [si],'\\'
cwd_done:
 popf
 ret
bad_drive:
 pop dx
 stc
 ret
print:
 mov ah,9
 int 0x21
 ret
/* DOS EXEC may destroy registers, including SS:SP on DOS 2.x.
 * Keep recovery state in CS and restore segments before reading results. */
execute:
 mov [execpath],dx
 push cs
 pop es
 mov ax,cs
 mov [params+4],ax
 mov [params+8],ax
 mov [params+12],ax
 mov si,[tailptr]
 mov [params+2],si
 inc si
 mov di,offset fcb1
 mov ax,0x2901
 int 0x21
 mov di,offset fcb2
 mov ax,0x2901
 int 0x21
 mov dx,[execpath]
 mov bx,offset params
 mov [savedsp],sp
 mov ax,0x4b00
 int 0x21
 mov cs:[execresult],ax
 lahf
 mov cs:[execflags],ah
 cli
 mov ax,cs
 mov ss,ax
 mov sp,cs:[savedsp]
 sti
 mov ds,ax
 mov es,ax
 cld
 mov ax,[execresult]
 test byte ptr [execflags],1
 jnz exec_fail
 clc
 ret
exec_fail:
 stc
 ret
.align 2
psp: .word 0
home_len: .word 0
trimmed: .word 0
tailptr: .word 0
savedsp: .word 0
execpath: .word 0
execresult: .word 0
execflags: .byte 0
exit_code: .byte 0
params: .word 0,0,0,fcb1,0,fcb2,0
ui_name: .asciz "NUI.EXE"
job_name: .asciz "NM-RUN.DAT"
bat_name: .asciz "NM-RUN.BAT"
error_msg: .ascii "NuXT Menu: launch failed. Check paths, memory and both EXEs.\r\n$"
pause_msg: .ascii "\r\nPress a key to return to the menu.\r\n$"
returntail: .byte 16
 .ascii " /DRIVER /RETURN"
 .byte 13,0
uitail: .byte 0
 .ascii " /DRIVER "
 .space 119
childtail: .space 129
fcb1: .space 37
fcb2: .space 37
home: .space 80
uipath: .space 80
jobpath: .space 80
batpath: .space 80
original: .byte 0,':','\\'
 .space 77
job: .space 294
.align 16
 .space 512
image_end:
