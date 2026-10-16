#!/usr/bin/env python3
"""Execute release assembly with Unicorn. Functional checks, not cycle timings.

Pass a directory containing standalone CRC.com, HSDEC.com and UARTTX.com,
linked with crc_fast_, hs_decode_ and uart_tx_fill_ as their entry points.
"""
import binascii
import random
import struct
import sys
from pathlib import Path
from unicorn import Uc, UC_ARCH_X86, UC_MODE_16, UC_HOOK_INSN
from unicorn.x86_const import *
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'linux'))
from heatshrink_codec import encode


def machine(code, **registers):
    cpu=Uc(UC_ARCH_X86,UC_MODE_16);cpu.mem_map(0,0x100000)
    cpu.mem_write(0x10100,code);cpu.mem_write(0x3fff0,struct.pack('<H',0x800))
    defaults=dict(CS=0x1000,DS=0x2000,SS=0x3000,SP=0xfff0,ES=0x1234,BP=0xdead,EFLAGS=0x602)
    defaults.update(registers)
    for key,value in defaults.items():cpu.reg_write(globals()['UC_X86_REG_'+key],value)
    return cpu


def execute(cpu, flags=0x600):
    cpu.emu_start(0x10100,0x10800,count=2000000)
    assert cpu.reg_read(UC_X86_REG_IP)==0x800, 'Did not return'
    for reg,value in ((UC_X86_REG_ES,0x1234),(UC_X86_REG_BP,0xdead),
                      (UC_X86_REG_DS,0x2000),(UC_X86_REG_SP,0xfff2)):
        assert cpu.reg_read(reg)==value
    assert cpu.reg_read(UC_X86_REG_EFLAGS)&0x600==flags
    return cpu.reg_read(UC_X86_REG_AX)


def packed_bits(bits):
    bits+='0'*(-len(bits)%8)
    return bytes(int(bits[i:i+8],2) for i in range(0,len(bits),8))


def main():
    folder=Path(sys.argv[1]);rng=random.Random(8733)
    crc_code=(folder/'CRC.com').read_bytes();hs_code=(folder/'HSDEC.com').read_bytes()
    tx_code=(folder/'UARTTX.com').read_bytes();cases=0
    table=b''.join(struct.pack('<H',binascii.crc_hqx(bytes([n]),0)) for n in range(256))
    for size in (0,1,2,9,128,256,512,2048,4096,4100):
        data=rng.randbytes(size)
        for seed in (0,1,0x1234,0xffff):
            cpu=machine(crc_code,SI=0x1000,CX=size,AX=seed,DI=0x4000)
            cpu.mem_write(0x21000,data or b'\0');cpu.mem_write(0x24000,table)
            assert execute(cpu)==binascii.crc_hqx(data,seed)
            assert cpu.reg_read(UC_X86_REG_DI)==0x4000
            cases+=1

    def decode(packed,size,expected=None,valid=True):
        cpu=machine(hs_code,SI=0x1000,CX=len(packed),DI=0x4000,AX=size)
        cpu.mem_write(0x21000,packed or b'\xff')
        cpu.mem_write(0x23fff,b'\xa5'*(max(size,1)+2))
        assert execute(cpu)==int(valid),(len(packed),size,packed[:30])
        assert bytes(cpu.mem_read(0x23fff,1))==b'\xa5'
        assert bytes(cpu.mem_read(0x24000+max(size,1),1))==b'\xa5'
        if valid:assert bytes(cpu.mem_read(0x24000,size))==expected

    for size in (1,2,3,15,16,17,31,127,128,255,256,511,512,2048,4095,4096):
        for data in (b'\0'*size,b'A'*size,rng.randbytes(size),
                     (bytes(range(256))*17)[:size],(b'ABC\0NX\xff'*600)[:size]):
            packed=encode(data);decode(packed,size,data);cases+=1
            decode(packed[:-1],size,valid=False);cases+=1
            decode(packed+b'\0',size,valid=False);cases+=1
    # Explicit prehistory, overlapping and maximum-distance references.
    for distance in (1,2,16,4096):
        packed=packed_bits('0'+format(distance-1,'012b')+'1111')
        decode(packed,16,b'\0'*16);cases+=1
        decode(packed,15,valid=False);cases+=1
    packed=packed_bits('1'+format(65,'08b')+'0'+'0'*12+'1111')
    decode(packed,17,b'A'*17);cases+=1
    for n in range(len(packed)):decode(packed[:n],17,valid=False);cases+=1
    decode(b'\xff\xff',1,valid=False);cases+=1  # nonzero padding
    decode(b'\0',0,valid=False);cases+=1
    decode(b'\0',4097,valid=False);cases+=1
    for _ in range(30):
        size=rng.randrange(1,4097);data=bytes(rng.randrange(8) for i in range(size))
        decode(encode(data),size,data);cases+=1

    for port in (0x3f8,0x2f8):
        for size in (0,1,8,16):
            for mask in (0,0x10,0xa0,0xb0):
                for stop in (0,3,16):
                    cpu=machine(tx_code,DX=port,SI=0x1000,CX=size,BX=mask)
                    data=bytes(range(size));cpu.mem_write(0x21000,data or b'\0');sent=[]
                    def read(c,p,n,u):
                        assert (p,n)==(port+6,1)
                        return mask if len(sent)<stop else 0
                    def write(c,p,n,value,u):
                        assert (p,n)==(port,1)
                        assert not mask or len(sent)<stop
                        assert len(sent)<16
                        sent.append(value)
                    cpu.hook_add(UC_HOOK_INSN,read,None,1,0,UC_X86_INS_IN)
                    cpu.hook_add(UC_HOOK_INSN,write,None,1,0,UC_X86_INS_OUT)
                    count=execute(cpu)
                    assert count==min(size,stop) if mask else count==size
                    assert bytes(sent)==data[:count]
                    assert cpu.reg_read(UC_X86_REG_DX)==port
                    cases+=1
    print(f'PASS {cases} assembly cases: CRC seeds/bounds; heatshrink literals, '
          'overlap, zero history, truncation/padding/length rejection; FIFO and peer pauses; saved registers/IF/DF')


if __name__=='__main__':main()
