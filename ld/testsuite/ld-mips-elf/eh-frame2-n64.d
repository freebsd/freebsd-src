#name: MIPS eh-frame 2, n64
#source: eh-frame1.s
#source: eh-frame1.s
#as: -EB -64 --defsym alignment=3 --defsym fill=0
#readelf: --relocs -wf
#ld: -shared -melf64btsmip -Teh-frame1.ld

Relocation section '\.rel\.dyn' .*:
 *Offset .*
000000000000  000000000000 R_MIPS_NONE *
 *Type2: R_MIPS_NONE *
 *Type3: R_MIPS_NONE *
# Initial PCs for the FDEs attached to CIE 0x118
000000030140  000000001203 R_MIPS_REL32 *
 *Type2: R_MIPS_64 *
 *Type3: R_MIPS_NONE *
000000030160  000000001203 R_MIPS_REL32 *
 *Type2: R_MIPS_64 *
 *Type3: R_MIPS_NONE *
# Likewise CIE 0x330
000000030358  000000001203 R_MIPS_REL32 *
 *Type2: R_MIPS_64 *
 *Type3: R_MIPS_NONE *
000000030378  000000001203 R_MIPS_REL32 *
 *Type2: R_MIPS_64 *
 *Type3: R_MIPS_NONE *
0000000300cb  000500001203 R_MIPS_REL32      0000000000000000 foo
 *Type2: R_MIPS_64 *
 *Type3: R_MIPS_NONE *
000000030130  000500001203 R_MIPS_REL32      0000000000000000 foo
 *Type2: R_MIPS_64 *
 *Type3: R_MIPS_NONE *
00000003018a  000500001203 R_MIPS_REL32      0000000000000000 foo
 *Type2: R_MIPS_64 *
 *Type3: R_MIPS_NONE *
0000000302e3  000500001203 R_MIPS_REL32      0000000000000000 foo
 *Type2: R_MIPS_64 *
 *Type3: R_MIPS_NONE *
000000030348  000500001203 R_MIPS_REL32      0000000000000000 foo
 *Type2: R_MIPS_64 *
 *Type3: R_MIPS_NONE *
0000000303a2  000500001203 R_MIPS_REL32      0000000000000000 foo
 *Type2: R_MIPS_64 *
 *Type3: R_MIPS_NONE *
#...
The section \.eh_frame contains:

00000000 00000014 00000000 CIE
  Version:               1
  Augmentation:          "zR"
  Code alignment factor: 1
  Data alignment factor: 4
  Return address column: 31
  Augmentation data:     10

  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000018 0000001c 0000001c FDE cie=00000000 pc=00020000..00020010
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000038 0000001c 0000003c FDE cie=00000000 pc=00020010..00020030
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

# basic2 removed
00000058 0000001c 0000005c FDE cie=00000000 pc=00020030..00020060
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

# basic3 removed
00000078 0000001c 0000007c FDE cie=00000000 pc=00020060..000200a0
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

# basic4 removed
00000098 0000001c 0000009c FDE cie=00000000 pc=000200a0..000200f0
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000000b8 0000001c 00000000 CIE
  Version:               1
  Augmentation:          "zRP"
  Code alignment factor: 1
  Data alignment factor: 4
  Return address column: 31
  Augmentation data:     10 00 00 00 00 00 00 00 00 00

  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000000d8 0000001c 00000024 FDE cie=000000b8 pc=000200f0..00020100
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000000f8 0000001c 00000044 FDE cie=000000b8 pc=00020100..00020120
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000118 0000001c 00000000 CIE
  Version:               1
  Augmentation:          "zP"
  Code alignment factor: 1
  Data alignment factor: 4
  Return address column: 31
  Augmentation data:     50 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00


00000138 0000001c 00000024 FDE cie=00000118 pc=00020120..00020130
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000158 0000001c 00000044 FDE cie=00000118 pc=00020130..00020150
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000178 0000001c 00000000 CIE
  Version:               1
  Augmentation:          "zPR"
  Code alignment factor: 1
  Data alignment factor: 4
  Return address column: 31
  Augmentation data:     00 00 00 00 00 00 00 00 00 10

  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000198 0000001c 00000024 FDE cie=00000178 pc=00020150..00020160
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

# FDE for .discard removed
# zPR2 removed
000001b8 0000001c 00000044 FDE cie=00000178 pc=00020160..00020190
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000001d8 0000001c 00000064 FDE cie=00000178 pc=00020190..000201d0
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000001f8 00000014 00000000 CIE
  Version:               1
  Augmentation:          "zR"
  Code alignment factor: 1
  Data alignment factor: 4
  Return address column: 31
  Augmentation data:     10

  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000210 0000001c 0000001c FDE cie=000001f8 pc=000201d0..000201e0
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

# basic1 removed, followed by repeat of above
00000230 0000001c 0000003c FDE cie=000001f8 pc=000201e0..000201f0
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000250 0000001c 0000005c FDE cie=000001f8 pc=000201f0..00020210
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000270 0000001c 0000007c FDE cie=000001f8 pc=00020210..00020240
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000290 0000001c 0000009c FDE cie=000001f8 pc=00020240..00020280
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000002b0 0000001c 000000bc FDE cie=000001f8 pc=00020280..000202d0
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000002d0 0000001c 00000000 CIE
  Version:               1
  Augmentation:          "zRP"
  Code alignment factor: 1
  Data alignment factor: 4
  Return address column: 31
  Augmentation data:     10 00 00 00 00 00 00 00 00 00

  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000002f0 0000001c 00000024 FDE cie=000002d0 pc=000202d0..000202e0
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000310 0000001c 00000044 FDE cie=000002d0 pc=000202e0..00020300
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000330 0000001c 00000000 CIE
  Version:               1
  Augmentation:          "zP"
  Code alignment factor: 1
  Data alignment factor: 4
  Return address column: 31
  Augmentation data:     50 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00


00000350 0000001c 00000024 FDE cie=00000330 pc=00020300..00020310
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000370 0000001c 00000044 FDE cie=00000330 pc=00020310..00020330
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000390 0000001c 00000000 CIE
  Version:               1
  Augmentation:          "zPR"
  Code alignment factor: 1
  Data alignment factor: 4
  Return address column: 31
  Augmentation data:     00 00 00 00 00 00 00 00 00 10

  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000003b0 0000001c 00000024 FDE cie=00000390 pc=00020330..00020340
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000003d0 0000001c 00000044 FDE cie=00000390 pc=00020340..00020370
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000003f0 0000001c 00000064 FDE cie=00000390 pc=00020370..000203b0
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000410 00000014 00000000 CIE
  Version:               1
  Augmentation:          "zR"
  Code alignment factor: 1
  Data alignment factor: 4
  Return address column: 31
  Augmentation data:     10

  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000428 0000001c 0000001c FDE cie=00000410 pc=000203b0..000203c0
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

