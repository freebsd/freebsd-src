#as: -EL
#objdump: -dr -EL

.*: +file format elf32-.*arc

Disassembly of section .text:

00000000 <.text>:
   0:	00 00 a0 1f 	1fa00000     flag       r0
   4:	01 80 bf 1f 	1fbf8001     flag       1
   8:	02 80 bf 1f 	1fbf8002     flag       2
   c:	04 80 bf 1f 	1fbf8004     flag       4
  10:	08 80 bf 1f 	1fbf8008     flag       8
  14:	10 80 bf 1f 	1fbf8010     flag       16
  18:	20 80 bf 1f 	1fbf8020     flag       32
  1c:	40 80 bf 1f 	1fbf8040     flag       64
  20:	80 80 bf 1f 	1fbf8080     flag       128
  24:	00 00 bf 1f 	1fbf0000     flag       0x8000_0001
  28:	01 00 00 80 
  2c:	0b 00 a0 1f 	1fa0000b     flag.lt    r0
  30:	09 00 bf 1f 	1fbf0009     flag.gt    1
  34:	01 00 00 00 
  38:	09 00 bf 1f 	1fbf0009     flag.gt    2
  3c:	02 00 00 00 
  40:	09 00 bf 1f 	1fbf0009     flag.gt    4
  44:	04 00 00 00 
  48:	09 00 bf 1f 	1fbf0009     flag.gt    8
  4c:	08 00 00 00 
  50:	09 00 bf 1f 	1fbf0009     flag.gt    16
  54:	10 00 00 00 
  58:	09 00 bf 1f 	1fbf0009     flag.gt    32
  5c:	20 00 00 00 
  60:	09 00 bf 1f 	1fbf0009     flag.gt    64
  64:	40 00 00 00 
  68:	09 00 bf 1f 	1fbf0009     flag.gt    128
  6c:	80 00 00 00 
  70:	0a 00 bf 1f 	1fbf000a     flag.ge    0x8000_0001
  74:	01 00 00 80 
