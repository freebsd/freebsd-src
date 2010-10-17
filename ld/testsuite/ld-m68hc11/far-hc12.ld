ENTRY(_start)
SECTIONS {
  .text 0xc000 : {
    *(.text)
  }
  .bank1 0x10000 : {
    *(.bank1)
  }
  .bank2 0x14000 : {
    *(.bank2)
  }
  .bank3 0x18000 : {
    *(.bank3)
  }
  .bss 0x2000 : {
    *(.bss)
  }
}
