 .text
 ; Test that this works both with a symbol defined in a section...
 .dword __Edata+0x40000000

 ; ...as well as absolute symbol (defined outside sections in the
 ; linker script).
 .dword __Sdata+0x40000000

 .data
 ; Make sure we get the same section alignment for *-elf as for *-linux*.
 .balign 0x2000

 ; Make .data non-empty.
 .dword 0
