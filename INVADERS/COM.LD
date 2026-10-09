OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
SECTIONS {
  . = 0x100;
  .text : { *(.text) }
  .data : { *(.data) }
  .bss (NOLOAD) : { *(.bss) }
  /DISCARD/ : { *(.note*) *(.comment) }
  ASSERT(mem_end < 0xff00, "Program exceeds COM segment")
}
