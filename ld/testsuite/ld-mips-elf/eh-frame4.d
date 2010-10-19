#name: MIPS eh-frame 4
#source: eh-frame1.s
#source: eh-frame1.s
#as: -EB -mips3 -mabi=eabi --defsym alignment=2 --defsym fill=0 --defsym foo=0x50607080
#readelf: -wf
#ld: -EB -Teh-frame1.ld
#
# This test is for the semi-official ILP32 variation of EABI64.
#

The section \.eh_frame contains:

00000000 0000000c 00000000 CIE
  Version:               1
  Augmentation:          ""
  Code alignment factor: 1
  Data alignment factor: 4
  Return address column: 31

  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000010 0000000c 00000014 FDE cie=00000000 pc=00020000..00020010

00000020 0000000c 00000024 FDE cie=00000000 pc=00020010..00020030

# basic2 removed
00000030 0000000c 00000034 FDE cie=00000000 pc=00020030..00020060

# basic3 removed
00000040 0000000c 00000044 FDE cie=00000000 pc=00020060..000200a0

# basic4 removed
00000050 0000000c 00000054 FDE cie=00000000 pc=000200a0..000200f0

00000060 00000014 00000000 CIE
  Version:               1
  Augmentation:          "zP"
  Code alignment factor: 1
  Data alignment factor: 4
  Return address column: 31
  Augmentation data:     00 50 60 70 80

  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000078 00000010 0000001c FDE cie=00000060 pc=000200f0..00020100
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

0000008c 00000010 00000030 FDE cie=00000060 pc=00020100..00020120
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000000a0 00000014 00000000 CIE
  Version:               1
  Augmentation:          "zP"
  Code alignment factor: 1
  Data alignment factor: 4
  Return address column: 31
  Augmentation data:     50 00 00 00 50 60 70 80


000000b8 00000010 0000001c FDE cie=000000a0 pc=00020120..00020130
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000000cc 00000010 00000030 FDE cie=000000a0 pc=00020130..00020150
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000000e0 00000014 00000000 CIE
  Version:               1
  Augmentation:          "zPR"
  Code alignment factor: 1
  Data alignment factor: 4
  Return address column: 31
  Augmentation data:     00 50 60 70 80 00

  DW_CFA_nop

000000f8 00000010 0000001c FDE cie=000000e0 pc=00020150..00020160
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

# FDE for .discard removed
# zPR2 removed
0000010c 00000010 00000030 FDE cie=000000e0 pc=00020160..00020190
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000120 00000010 00000044 FDE cie=000000e0 pc=00020190..000201d0
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000134 0000000c 00000000 CIE
  Version:               1
  Augmentation:          ""
  Code alignment factor: 1
  Data alignment factor: 4
  Return address column: 31

  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000144 0000000c 00000014 FDE cie=00000134 pc=000201d0..000201e0

# basic1 removed, followed by repeat of above
00000154 0000000c 00000024 FDE cie=00000134 pc=000201e0..000201f0

00000164 0000000c 00000034 FDE cie=00000134 pc=000201f0..00020210

00000174 0000000c 00000044 FDE cie=00000134 pc=00020210..00020240

00000184 0000000c 00000054 FDE cie=00000134 pc=00020240..00020280

00000194 0000000c 00000064 FDE cie=00000134 pc=00020280..000202d0

000001a4 00000014 00000000 CIE
  Version:               1
  Augmentation:          "zP"
  Code alignment factor: 1
  Data alignment factor: 4
  Return address column: 31
  Augmentation data:     00 50 60 70 80

  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000001bc 00000010 0000001c FDE cie=000001a4 pc=000202d0..000202e0
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000001d0 00000010 00000030 FDE cie=000001a4 pc=000202e0..00020300
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000001e4 00000014 00000000 CIE
  Version:               1
  Augmentation:          "zP"
  Code alignment factor: 1
  Data alignment factor: 4
  Return address column: 31
  Augmentation data:     50 00 00 00 50 60 70 80


000001fc 00000010 0000001c FDE cie=000001e4 pc=00020300..00020310
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000210 00000010 00000030 FDE cie=000001e4 pc=00020310..00020330
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000224 00000014 00000000 CIE
  Version:               1
  Augmentation:          "zPR"
  Code alignment factor: 1
  Data alignment factor: 4
  Return address column: 31
  Augmentation data:     00 50 60 70 80 00

  DW_CFA_nop

0000023c 00000010 0000001c FDE cie=00000224 pc=00020330..00020340
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

# FDE for .discard removed
# zPR2 removed
00000250 00000010 00000030 FDE cie=00000224 pc=00020340..00020370
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000264 00000010 00000044 FDE cie=00000224 pc=00020370..000203b0
  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000278 0000000c 00000000 CIE
  Version:               1
  Augmentation:          ""
  Code alignment factor: 1
  Data alignment factor: 4
  Return address column: 31

  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000288 0000000c 00000014 FDE cie=00000278 pc=000203b0..000203c0
