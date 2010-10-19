#readelf: -wl
#source: fragtest.s
#as: --em=criself --gdwarf2

# Highly "fragmented" code.
#...
 Line Number Statements:
  Extended opcode 2: set Address to 0x0
  Special opcode .*: advance Address by 0 to 0x0 and Line by 4 to 5
  Special opcode .*: advance Address by 2 to 0x2 and Line by 1 to 6
  Advance PC by 126 to 0x80
  Special opcode .*: advance Address by 0 to 0x80 and Line by 2 to 8
  Special opcode .*: advance Address by 2 to 0x82 and Line by 1 to 9
  Advance PC by 226 to 0x164
  Special opcode .*: advance Address by 0 to 0x164 and Line by 6 to 15
  Special opcode .*: advance Address by 4 to 0x168 and Line by 1 to 16
  Advance PC by 126 to 0x1e6
  Special opcode .*: advance Address by 0 to 0x1e6 and Line by 2 to 18
  Special opcode .*: advance Address by 4 to 0x1ea and Line by 1 to 19
  Advance PC by 1126 to 0x650
  Special opcode .*: advance Address by 0 to 0x650 and Line by 6 to 25
  Special opcode .*: advance Address by 4 to 0x654 and Line by 1 to 26
  Advance PC by 126 to 0x6d2
  Special opcode .*: advance Address by 0 to 0x6d2 and Line by 2 to 28
  Special opcode .*: advance Address by 12 to 0x6de and Line by 1 to 29
  Advance Line by 11 to 40
  Advance PC by 33250 to 0x88c0
  Copy
  Special opcode .*: advance Address by 2 to 0x88c2 and Line by 1 to 41
  Advance PC by 128 to 0x8942
  Special opcode .*: advance Address by 0 to 0x8942 and Line by 2 to 43
  Special opcode .*: advance Address by 2 to 0x8944 and Line by 1 to 44
  Advance PC by 248 to 0x8a3c
  Special opcode .*: advance Address by 0 to 0x8a3c and Line by 6 to 50
  Special opcode .*: advance Address by 4 to 0x8a40 and Line by 1 to 51
  Advance PC by 128 to 0x8ac0
  Special opcode .*: advance Address by 0 to 0x8ac0 and Line by 2 to 53
  Special opcode .*: advance Address by 4 to 0x8ac4 and Line by 1 to 54
  Advance PC by 252 to 0x8bc0
  Special opcode .*: advance Address by 0 to 0x8bc0 and Line by 6 to 60
  Special opcode .*: advance Address by 4 to 0x8bc4 and Line by 1 to 61
  Advance PC by 128 to 0x8c44
  Special opcode .*: advance Address by 0 to 0x8c44 and Line by 2 to 63
  Special opcode .*: advance Address by 4 to 0x8c48 and Line by 1 to 64
  Advance PC by 124 to 0x8cc4
  Extended opcode 1: End of Sequence
