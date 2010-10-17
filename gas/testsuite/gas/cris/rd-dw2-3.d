#readelf: -wl
#source: branch-warn-1.s
#as: --em=criself --gdwarf2

# Simple branch-expansion, type 1.
#...
 Line Number Statements:
  Extended opcode 2: set Address to 0x0
  Special opcode 12: advance Address by 0 to 0x0 and Line by 7 to 8
  Special opcode 90: advance Address by 12 to 0xc and Line by 1 to 9
  Advance PC by 2 to e
  Extended opcode 1: End of Sequence
