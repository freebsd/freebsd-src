ALIGN=1024
RESNUM 0x0000, 0x8000
; Putting in .lit1 gives errors.
ORDER .data=0x80002000, .data1, .lit, .bss
; Let's put this on the command line so it goes first, which is what
; GDB expects.
; LOAD /s2/amd/29k/lib/crt0.o
LOAD /s2/amd/29k/lib/libqcb0h.lib
LOAD /s2/amd/29k/lib/libscb0h.lib
LOAD /s2/amd/29k/lib/libacb0h.lib
