#readelf: -wl
#source: branch-warn-3.s
#as: --em=criself --gdwarf2

# Simple branch-expansion, type 3.
#...
 Line Number Statements:
  Extended opcode 2: set Address to 0x0
  Special opcode 12: advance Address by 0 to 0x0 and Line by 7 to 8
  Advance PC by 32770 to 8002
  Special opcode 7: advance Address by 0 to 0x8002 and Line by 2 to 10
  Special opcode 90: advance Address by 12 to 0x800e and Line by 1 to 11
  Advance PC by 2 to 8010
  Extended opcode 1: End of Sequence
