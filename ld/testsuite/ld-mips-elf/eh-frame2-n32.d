#name: MIPS eh-frame 2, n32
#source: eh-frame1.s
#source: eh-frame1.s
#as: -EB -n32 --defsym alignment=2 --defsym fill=0
#readelf: --relocs -wf
#ld: -shared -melf32btsmipn32 -Teh-frame1.ld

Relocation section '\.rel\.dyn' .*:
 *Offset .*
00000000  00000000 R_MIPS_NONE *
# Initial PCs for the FDEs attached to CIE 0xb8
000300d8  00000003 R_MIPS_REL32 *
000300ec  00000003 R_MIPS_REL32 *
# Likewise CIE 0x218
00030238  00000003 R_MIPS_REL32 *
0003024c  00000003 R_MIPS_REL32 *
0003008b  00000503 R_MIPS_REL32      00000000   foo
000300cc  00000503 R_MIPS_REL32      00000000   foo
0003010a  00000503 R_MIPS_REL32      00000000   foo
000301eb  00000503 R_MIPS_REL32      00000000   foo
0003022c  00000503 R_MIPS_REL32      00000000   foo
0003026a  00000503 R_MIPS_REL32      00000000   foo
#...
The section \.eh_frame contains:

00000000 00000010 00000000 CIE
  Version:               1
  Augmentation:          "zR"
  Code alignment factor: 1
  Data alignment factor: 4
  Return address column: 31
  Augmentation data:     10

  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000014 00000010 00000018 FDE cie=00000000 pc=00020000..00020010
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000028 00000010 0000002c FDE cie=00000000 pc=00020010..00020030
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

# basic2 removed
0000003c 00000010 00000040 FDE cie=00000000 pc=00020030..00020060
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

# basic3 removed
00000050 00000010 00000054 FDE cie=00000000 pc=00020060..000200a0
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

# basic4 removed
00000064 00000010 00000068 FDE cie=00000000 pc=000200a0..000200f0
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000078 00000014 00000000 CIE
  Version:               1
  Augmentation:          "zRP"
  Code alignment factor: 1
  Data alignment factor: 4
  Return address column: 31
  Augmentation data:     10 00 00 00 00 00

  DW_CFA_nop

00000090 00000010 0000001c FDE cie=00000078 pc=000200f0..00020100
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000000a4 00000010 00000030 FDE cie=00000078 pc=00020100..00020120
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000000b8 00000014 00000000 CIE
  Version:               1
  Augmentation:          "zP"
  Code alignment factor: 1
  Data alignment factor: 4
  Return address column: 31
  Augmentation data:     50 00 00 00 00 00 00 00


000000d0 00000010 0000001c FDE cie=000000b8 pc=00020120..00020130
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000000e4 00000010 00000030 FDE cie=000000b8 pc=00020130..00020150
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000000f8 00000014 00000000 CIE
  Version:               1
  Augmentation:          "zPR"
  Code alignment factor: 1
  Data alignment factor: 4
  Return address column: 31
  Augmentation data:     00 00 00 00 00 10

  DW_CFA_nop

00000110 00000010 0000001c FDE cie=000000f8 pc=00020150..00020160
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

# FDE for .discard removed
# zPR2 removed
00000124 00000010 00000030 FDE cie=000000f8 pc=00020160..00020190
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000138 00000010 00000044 FDE cie=000000f8 pc=00020190..000201d0
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

0000014c 00000010 00000000 CIE
  Version:               1
  Augmentation:          "zR"
  Code alignment factor: 1
  Data alignment factor: 4
  Return address column: 31
  Augmentation data:     10

  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000160 00000010 00000018 FDE cie=0000014c pc=000201d0..000201e0
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

# basic1 removed, followed by repeat of above
00000174 00000010 0000002c FDE cie=0000014c pc=000201e0..000201f0
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000188 00000010 00000040 FDE cie=0000014c pc=000201f0..00020210
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

0000019c 00000010 00000054 FDE cie=0000014c pc=00020210..00020240
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000001b0 00000010 00000068 FDE cie=0000014c pc=00020240..00020280
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000001c4 00000010 0000007c FDE cie=0000014c pc=00020280..000202d0
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000001d8 00000014 00000000 CIE
  Version:               1
  Augmentation:          "zRP"
  Code alignment factor: 1
  Data alignment factor: 4
  Return address column: 31
  Augmentation data:     10 00 00 00 00 00

  DW_CFA_nop

000001f0 00000010 0000001c FDE cie=000001d8 pc=000202d0..000202e0
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000204 00000010 00000030 FDE cie=000001d8 pc=000202e0..00020300
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000218 00000014 00000000 CIE
  Version:               1
  Augmentation:          "zP"
  Code alignment factor: 1
  Data alignment factor: 4
  Return address column: 31
  Augmentation data:     50 00 00 00 00 00 00 00


00000230 00000010 0000001c FDE cie=00000218 pc=00020300..00020310
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000244 00000010 00000030 FDE cie=00000218 pc=00020310..00020330
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000258 00000014 00000000 CIE
  Version:               1
  Augmentation:          "zPR"
  Code alignment factor: 1
  Data alignment factor: 4
  Return address column: 31
  Augmentation data:     00 00 00 00 00 10

  DW_CFA_nop

00000270 00000010 0000001c FDE cie=00000258 pc=00020330..00020340
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000284 00000010 00000030 FDE cie=00000258 pc=00020340..00020370
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000298 00000010 00000044 FDE cie=00000258 pc=00020370..000203b0
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000002ac 00000010 00000000 CIE
  Version:               1
  Augmentation:          "zR"
  Code alignment factor: 1
  Data alignment factor: 4
  Return address column: 31
  Augmentation data:     10

  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000002c0 00000010 00000018 FDE cie=000002ac pc=000203b0..000203c0
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

