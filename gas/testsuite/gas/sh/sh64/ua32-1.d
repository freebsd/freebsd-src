#as: --abi=32
#objdump: -sr
#source: ua-1.s
#name: Unaligned pseudos, 32-bit ABI.

# Note that the relocs for externsym0 + 3 and externsym2 + 42 are
# partial-in-place, i.e. REL-like, and are not displayed correctly.

.*:     file format .*-sh64.*

RELOCATION RECORDS FOR \[\.rodata\]:
OFFSET  *TYPE  *VALUE 
0+0f R_SH_DIR32        externsym0
0+1b R_SH_64           externsym1\+0x0+29
0+2c R_SH_DIR32        externsym2
0+30 R_SH_64           externsym3\+0x0+2b


Contents of section \.rodata:
 0000 01234567 89abcdef 2a4a2143 b1abcd00  .*
 0010 00000301 2c456d89 ab1d0f00 00000000  .*
 0020 00000002 01a34b67 c9ab0d4f 0000002a  .*
 0030 00000000 00000000                    .*
