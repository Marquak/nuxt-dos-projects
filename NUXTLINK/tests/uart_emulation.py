#!/usr/bin/env python3
"""Execute the assembled UART routine in a 16-bit CPU with scripted port I/O.

Requires Unicorn (test dependency only). Pass a standalone DOS COM linked from
UART.ASM with uart_rx_ as its entry point. This tests semantics, not V20 timing.
"""
import struct
import sys
from pathlib import Path
from unicorn import Uc, UC_ARCH_X86, UC_MODE_16, UC_HOOK_INSN
from unicorn.x86_const import *


def run(code, count, *, base=0x3f8, gap=0, fault=0, fault_at=0,
        available=None, tick_start=0):
    cpu=Uc(UC_ARCH_X86,UC_MODE_16)
    cpu.mem_map(0,0x100000)
    cpu.mem_write(0x10100,code)
    cpu.mem_write(0x3fff0,struct.pack('<H',0x500))
    cpu.mem_write(0x22fff,b'\xa5'*(count+2))
    for reg,value in ((UC_X86_REG_CS,0x1000),(UC_X86_REG_DS,0x2000),
                      (UC_X86_REG_SS,0x3000),(UC_X86_REG_SP,0xfff0),
                      (UC_X86_REG_ES,0x1234),(UC_X86_REG_BP,0xdead),
                      (UC_X86_REG_BX,0xbeef),(UC_X86_REG_DX,base),
                      (UC_X86_REG_DI,0x3000),(UC_X86_REG_CX,count),
                      (UC_X86_REG_EFLAGS,0x602)):
        cpu.reg_write(reg,value)
    payload=bytes((i*73+31)&255 for i in range(count+3))
    consumed=polls=0
    waiting=gap
    available=count+3 if available is None else available
    cpu.mem_write(0x46c,struct.pack('<H',tick_start))

    def port_in(machine,port,size,user):
        nonlocal consumed,polls,waiting
        assert size==1
        if port==base+5:
            polls+=1
            machine.mem_write(0x46c,struct.pack('<H',(tick_start+polls//128)&65535))
            if consumed>=available:return 0x60
            if waiting:
                waiting-=1
                return 0x60
            return 0x61 | (fault if consumed==fault_at else 0)
        assert port==base,hex(port)
        assert consumed<available
        byte=payload[consumed];consumed+=1;waiting=gap
        return byte

    cpu.hook_add(UC_HOOK_INSN,port_in,None,1,0,UC_X86_INS_IN)
    cpu.emu_start(0x10100,0x10500,count=2000000)
    assert cpu.reg_read(UC_X86_REG_IP)==0x500,'Routine did not return'
    assert cpu.reg_read(UC_X86_REG_ES)==0x1234
    assert cpu.reg_read(UC_X86_REG_BP)==0xdead
    assert cpu.reg_read(UC_X86_REG_BX)==0xbeef
    assert cpu.reg_read(UC_X86_REG_DS)==0x2000
    assert cpu.reg_read(UC_X86_REG_SP)==0xfff2
    assert cpu.reg_read(UC_X86_REG_EFLAGS)&0x600==0x600,'IF/DF changed'
    assert cpu.reg_read(UC_X86_REG_AX)==consumed
    assert bytes(cpu.mem_read(0x23000,consumed))==payload[:consumed]
    assert bytes(cpu.mem_read(0x22fff,1))==b'\xa5'
    assert bytes(cpu.mem_read(0x23000+consumed,count-consumed+1))==b'\xa5'*(count-consumed+1)
    return consumed,cpu.reg_read(UC_X86_REG_DX),polls


def main():
    code=Path(sys.argv[1]).read_bytes()
    cases=0
    for base in (0x3f8,0x2f8,0x3e8,0x2e8):
        for count in (0,1,4,128,256,514):
            for gap in (0,3):
                got,fault,_=run(code,count,base=base,gap=gap)
                assert (got,fault)==(count,0)
                cases+=1
    for flag in (2,4,8,16,30):
        for position in (0,7,513):
            got,fault,_=run(code,514,fault=flag,fault_at=position)
            assert got==position and fault&30==flag
            cases+=1
    for available in (0,7):
        for tick_start in (0,65530):
            got,fault,polls=run(code,514,available=available,tick_start=tick_start)
            assert got==available and fault==0x100
            assert polls<2560,'Inter-byte timeout was not bounded'
            cases+=1
    print(f'PASS {cases} assembly cases: exact binary reads, four COM bases, gaps, '
          'all UART faults, partial/empty timeouts, tick wrap, buffer bounds and saved registers')


if __name__=='__main__':main()
