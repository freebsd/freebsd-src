# objdump: -dr
# as: -linkrelax
# source: pushgo-op.s
.*:     file format elf64-mmix

Disassembly of section \.text:

0000000000000000 <Main>:
   0:	be170c43 	pushgo \$23,\$12,\$67
   4:	becb0c43 	pushgo \$203,\$12,\$67
   8:	be200c43 	pushgo \$32,\$12,\$67
   c:	be200c43 	pushgo \$32,\$12,\$67
  10:	be172043 	pushgo \$23,\$32,\$67
  14:	becb2043 	pushgo \$203,\$32,\$67
  18:	bee88543 	pushgo \$232,\$133,\$67
  1c:	bee88543 	pushgo \$232,\$133,\$67
  20:	be170c49 	pushgo \$23,\$12,\$73
  24:	becb0c49 	pushgo \$203,\$12,\$73
  28:	be1f0ce9 	pushgo \$31,\$12,\$233
  2c:	be1f0ce9 	pushgo \$31,\$12,\$233
  30:	be1726d4 	pushgo \$23,\$38,\$212
  34:	becb26d4 	pushgo \$203,\$38,\$212
  38:	be04afb5 	pushgo \$4,\$175,\$181
  3c:	be04afb5 	pushgo \$4,\$175,\$181
  40:	bf170cb0 	pushgo \$23,\$12,176
  44:	bfcb0cb0 	pushgo \$203,\$12,176
  48:	bf200cb0 	pushgo \$32,\$12,176
  4c:	bf200cb0 	pushgo \$32,\$12,176
  50:	bf1720b0 	pushgo \$23,\$32,176
  54:	bfcb20b0 	pushgo \$203,\$32,176
  58:	bfe885b0 	pushgo \$232,\$133,176
  5c:	bfe885b0 	pushgo \$232,\$133,176
  60:	bf170ccb 	pushgo \$23,\$12,203
  64:	bfcb0ccb 	pushgo \$203,\$12,203
  68:	bf1f0cd5 	pushgo \$31,\$12,213
  6c:	bf1f0cd5 	pushgo \$31,\$12,213
  70:	bf1726d3 	pushgo \$23,\$38,211
  74:	bfcb26d3 	pushgo \$203,\$38,211
  78:	bf04afa1 	pushgo \$4,\$175,161
  7c:	bf04afa1 	pushgo \$4,\$175,161
  80:	bf170c00 	pushgo \$23,\$12,0
  84:	bfcb0c00 	pushgo \$203,\$12,0
  88:	bf290c00 	pushgo \$41,\$12,0
  8c:	bff10c00 	pushgo \$241,\$12,0
  90:	bf171b00 	pushgo \$23,\$27,0
  94:	bfcb3000 	pushgo \$203,\$48,0
  98:	bfdfdb00 	pushgo \$223,\$219,0
  9c:	bfdfe500 	pushgo \$223,\$229,0
  a0:	bf170c00 	pushgo \$23,\$12,0
  a4:	bfcb0c00 	pushgo \$203,\$12,0
  a8:	bf200c00 	pushgo \$32,\$12,0
  ac:	bf200c00 	pushgo \$32,\$12,0
  b0:	bf172000 	pushgo \$23,\$32,0
  b4:	bfcb2000 	pushgo \$203,\$32,0
  b8:	bfe88500 	pushgo \$232,\$133,0
  bc:	bfe88500 	pushgo \$232,\$133,0
  c0:	bf170c00 	pushgo \$23,\$12,0
  c4:	bfcb0c00 	pushgo \$203,\$12,0
  c8:	bf1f0c00 	pushgo \$31,\$12,0
  cc:	bf1f0c00 	pushgo \$31,\$12,0
  d0:	bf172600 	pushgo \$23,\$38,0
  d4:	bfcb2600 	pushgo \$203,\$38,0
  d8:	bf04af00 	pushgo \$4,\$175,0
  dc:	bf04af00 	pushgo \$4,\$175,0
