; CRC-16/CCITT, unchanged polynomial 1021h and caller-supplied seed.
; DS:SI=data, CX=length, AX=seed, DS:DI=256-word table. Returns AX.
.8086
.model tiny
.code
public crc_fast_
crc_fast_ proc near
    jcxz crc_done
crc_byte:
    xor bx,bx
    mov bl,ah
    xor bl,[si]
    shl bx,1
    mov ah,al
    xor al,al
    xor ax,[bx+di]
    inc si
    loop crc_byte
crc_done:
    ret
crc_fast_ endp
end
