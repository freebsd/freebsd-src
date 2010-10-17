# Simple base-plus-offset
b GREG @
a TETRA 42
  LDO $43,a+52

  LOC @+256
c GREG @
d TETRA 28
  LDO $143,d+12
  LDO $243,a+12
  LDA $103,d+40
  LDA $13,a+24
