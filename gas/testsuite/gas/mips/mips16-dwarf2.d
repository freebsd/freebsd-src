#readelf: -r -wl
#name: MIPS16 DWARF2
#as: -mabi=32 -mips16 -no-mdebug -g0
#source: mips16-dwarf2.s

Relocation section '\.rel\.debug_info' at offset .* contains 4 entries:
 *Offset * Info * Type * Sym\.Value * Sym\. Name
0+0006 * 0+..02 * R_MIPS_32 * 0+0000 * \.debug_abbrev
0+000c * 0+..02 * R_MIPS_32 * 0+0000 * \.debug_line
0+0010 * 0+..02 * R_MIPS_32 * 0+0000 * \.text
0+0014 * 0+..02 * R_MIPS_32 * 0+0000 * \.text

Relocation section '\.rel\.debug_line' at offset .* contains 1 entries:
 *Offset * Info * Type * Sym\.Value * Sym\. Name
0+0033 * 0+..02 * R_MIPS_32 * 0+0000 * \.text

#...
 Line Number Statements:
  Extended opcode 2: set Address to 0x1
  Copy
  Special opcode .*: advance Address by 2 to 0x3 and Line by 1 to 2
  Special opcode .*: advance Address by 2 to 0x5 and Line by 1 to 3
  Special opcode .*: advance Address by 4 to 0x9 and Line by 1 to 4
  Special opcode .*: advance Address by 2 to 0xb and Line by 1 to 5
  Special opcode .*: advance Address by 4 to 0xf and Line by 1 to 6
  Special opcode .*: advance Address by 4 to 0x13 and Line by 1 to 7
  Advance PC by 2286 to 0x901
  Special opcode .*: advance Address by 0 to 0x901 and Line by 1 to 8
  Advance PC by 15 to 0x910
  Extended opcode 1: End of Sequence
