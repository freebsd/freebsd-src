 .text
  nop
locsym1:
  .global locsym2
locsym2:
  nop
  jump locsym1
  jump locsym2
  jump locsym3
  jump locsym4
  jump extsym
  jsr locsym1
  jsr locsym2
  jsr locsym3
  jsr locsym4
  jsr extsym
  jsrc locsym1
  .dword 0
  jsrc locsym2
  .dword 0
  jsrc locsym3
  .dword 0
  jsrc locsym4
  .dword 0
  jsrc extsym
  .dword 0
  nop
  .global locsym3
locsym3:
locsym4:
  nop
