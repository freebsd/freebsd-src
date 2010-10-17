#readelf: -wl
#source: branch-warn-2.s
#as: --em=criself --gdwarf2

# Simple branch-expansion, type 2.
#...
 Line Number Statements:
  Extended opcode 2: set Address to 0x0
  Special opcode 12: advance Address by 0 to 0x0 and Line by 7 to 8
  Advance PC by 32780 to 800c
  Special opcode 8: advance Address by 0 to 0x800c and Line by 3 to 11
  Advance PC by 2 to 800e
  Extended opcode 1: End of Sequence
