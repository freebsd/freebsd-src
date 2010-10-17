#readelf: -wl
#source: brokw-1.s
#as: --em=criself --gdwarf2

# Most simple broken word.
#...
 Line Number Statements:
  Extended opcode 2: set Address to 0x0
  Special opcode 7: advance Address by 0 to 0x0 and Line by 2 to 3
  Special opcode 37: advance Address by 4 to 0x4 and Line by 4 to 7
  Special opcode 111: advance Address by 14 to 0x12 and Line by 8 to 15
  Advance PC by 32768 to 8012
  Special opcode 9: advance Address by 0 to 0x8012 and Line by 4 to 19
  Advance PC by 2 to 8014
  Extended opcode 1: End of Sequence
