# source: default-script.s
# ld: -defsym _START=0x8000000 -dT default-script.t
# nm: -n

#...
0*8000000 . _START
#...
0*8000000 t text
#pass
