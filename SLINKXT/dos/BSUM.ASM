; BSD checksum core adapted from bsum 1.1 for Slink-XT,
; Copyright (C) 2017 Mateusz Viste, MIT license (see ../BSUM-LICENSE-AND-NOTES.TXT).
; DS:SI=data, CX=length, DI=rolling checksum. Returns AX.
; The 8086 loop follows bsum's LODSB / ROR / ADD / DEC / JNZ sequence.
.8086
.model tiny
.code
public bsum_fast_
bsum_fast_ proc near
    cld
    xor ah,ah
    jcxz bsum_done
bsum_byte:
    lodsb
    ror di,1
    add di,ax
    dec cx
    jnz bsum_byte
bsum_done:
    mov ax,di
    ret
bsum_fast_ endp
end
