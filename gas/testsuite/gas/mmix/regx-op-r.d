# objdump: -dr
# as: -linkrelax
# source: regx-op.s
.*:     file format elf64-mmix

Disassembly of section \.text:

0000000000000000 <Main>:
   0:	9a7b0c43 	preld 123,\$12,\$67
   4:	9c200c43 	prego 32,\$12,\$67
   8:	b87b2043 	syncd 123,\$32,\$67
   c:	ba008543 	prest 0,\$133,\$67
  10:	b47b0c49 	stco 123,\$12,\$73
  14:	bc820ce9 	syncid 130,\$12,\$233
  18:	9a7b26d4 	preld 123,\$38,\$212
  1c:	9c01afb5 	prego 1,\$175,\$181
  20:	b97b0cb0 	syncd 123,\$12,176
  24:	bb200cb0 	prest 32,\$12,176
  28:	b57b84b0 	stco 123,\$132,176
  2c:	bde885b0 	syncid 232,\$133,176
  30:	9b7b0ccb 	preld 123,\$12,203
  34:	9de70cd5 	prego 231,\$12,213
  38:	b97b26d3 	syncd 123,\$38,211
  3c:	bb04afa1 	prest 4,\$175,161
  40:	b57b0c00 	stco 123,\$12,0
  44:	bd170c00 	syncid 23,\$12,0
  48:	9b020c00 	preld 2,\$12,0
  4c:	9de88500 	prego 232,\$133,0
  50:	b97b0c00 	syncd 123,\$12,0
  54:	bb0d0c00 	prest 13,\$12,0
  58:	b57b2600 	stco 123,\$38,0
  5c:	bd04af00 	syncid 4,\$175,0
  60:	9b7b0c00 	preld 123,\$12,0
  64:	9d200c00 	prego 32,\$12,0
  68:	b97b2000 	syncd 123,\$32,0
  6c:	bbe88500 	prest 232,\$133,0
