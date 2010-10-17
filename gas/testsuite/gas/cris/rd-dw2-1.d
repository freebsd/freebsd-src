#readelf: -wl
#source: addi.s
#as: --em=criself --gdwarf2

# A most simple instruction sequence.

Dump of debug contents of section \.debug_line:

  Length:                      .*
  DWARF Version:               2
  Prologue Length:             .*
  Minimum Instruction Length:  2
  Initial value of 'is_stmt':  1
  Line Base:                   -5
  Line Range:                  14
  Opcode Base:                 10
  \(Pointer size:               4\)

 Opcodes:
  Opcode 1 has 0 args
  Opcode 2 has 1 args
  Opcode 3 has 1 args
  Opcode 4 has 1 args
  Opcode 5 has 1 args
  Opcode 6 has 0 args
  Opcode 7 has 0 args
  Opcode 8 has 0 args
  Opcode 9 has 1 args

 The Directory Table:
  .*/gas/testsuite/gas/cris

 The File Name Table:
  Entry	Dir	Time	Size	Name
  1	1	0	0	addi\.s

 Line Number Statements:
  Extended opcode 2: set Address to 0x0
  Special opcode 9: advance Address by 0 to 0x0 and Line by 4 to 5
  Special opcode 20: advance Address by 2 to 0x2 and Line by 1 to 6
  Special opcode 20: advance Address by 2 to 0x4 and Line by 1 to 7
  Special opcode 20: advance Address by 2 to 0x6 and Line by 1 to 8
  Special opcode 20: advance Address by 2 to 0x8 and Line by 1 to 9
  Special opcode 20: advance Address by 2 to 0xa and Line by 1 to 10
  Special opcode 20: advance Address by 2 to 0xc and Line by 1 to 11
  Special opcode 20: advance Address by 2 to 0xe and Line by 1 to 12
  Special opcode 20: advance Address by 2 to 0x10 and Line by 1 to 13
  Special opcode 20: advance Address by 2 to 0x12 and Line by 1 to 14
  Special opcode 20: advance Address by 2 to 0x14 and Line by 1 to 15
  Special opcode 20: advance Address by 2 to 0x16 and Line by 1 to 16
  Advance PC by 2 to 18
  Extended opcode 1: End of Sequence
