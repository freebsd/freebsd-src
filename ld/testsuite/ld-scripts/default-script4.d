# source: default-script.s
# ld: --default-script default-script.t -defsym _START=0x8000000
# nm: -n

#...
0*8000000 . _START
#...
0*8000000 t text
#pass
