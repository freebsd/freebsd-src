
Relocation section '\.rel\.dyn' at offset .* contains .* entries:
 Offset     Info    Type            Sym\.Value  Sym\. Name
00000000  00000000 R_MIPS_NONE      
#
# The order and addresses of the next eight entries don't matter.  The
# important thing is that there is exactly one entry per GOT TLS slot
# and that the addresses match those in the .got dump.
#
001c4080  0000002f R_MIPS_TLS_TPREL3
001c4084  0000002f R_MIPS_TLS_TPREL3
001c4088  0000002f R_MIPS_TLS_TPREL3
001c408c  0000002f R_MIPS_TLS_TPREL3
001d00cc  0000002f R_MIPS_TLS_TPREL3
001d00d0  0000002f R_MIPS_TLS_TPREL3
001d00d4  0000002f R_MIPS_TLS_TPREL3
001d00d8  0000002f R_MIPS_TLS_TPREL3
.* R_MIPS_REL32 .*
#pass
