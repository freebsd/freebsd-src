#readelf: -wf
#name: CFI common 6
The section .eh_frame contains:

00000000 00000018 00000000 CIE
  Version:               1
  Augmentation:          "zPLR"
  Code alignment factor: .*
  Data alignment factor: .*
  Return address column: .*
  Augmentation data:     03 .. .. .. .. 0c 1b

  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

0000001c 00000018 00000020 FDE cie=00000000 pc=00000000..00000004
  Augmentation data:     (00 00 00 00 de ad be ef|ef be ad de 00 00 00 00)

  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000038 00000010 00000000 CIE
  Version:               1
  Augmentation:          "zLR"
  Code alignment factor: .*
  Data alignment factor: .*
  Return address column: .*
  Augmentation data:     0c 1b

  DW_CFA_nop

0000004c 00000018 00000018 FDE cie=00000038 pc=00000004..00000008
  Augmentation data:     (00 00 00 00 de ad be ef|ef be ad de 00 00 00 00)

  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000068 00000018 0000006c FDE cie=00000000 pc=00000008..0000000c
  Augmentation data:     (00 00 00 00 be ef de ad|ad de ef be 00 00 00 00)

  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

00000084 00000018 00000000 CIE
  Version:               1
  Augmentation:          "zPLR"
  Code alignment factor: .*
  Data alignment factor: .*
  Return address column: .*
  Augmentation data:     1b .. .. .. .. 1b 1b

  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000000a0 00000014 00000020 FDE cie=00000084 pc=0000000c..00000010
  Augmentation data:     .. .. .. ..

  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

000000b8 00000014 00000038 FDE cie=00000084 pc=00000010..00000014
  Augmentation data:     .. .. .. ..

  DW_CFA_nop
  DW_CFA_nop
  DW_CFA_nop

