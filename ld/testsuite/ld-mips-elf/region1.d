# as: -mabi=eabi -mips1 -G0
# source: region1a.s
# source: region1b.s
# ld: -T region1.t
# name: MIPS region1
# objdump: --headers
#...
  0 \.text +0+004 +0+10000 .*
#...
  1 \.data +0+004 +0+20000 .*
#pass
