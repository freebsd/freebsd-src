#source: start.s
#source: bpo-4.s
#source: pad2p18m32.s
#source: bpo-1.s
#as: -linker-allocated-gregs
#ld: -m mmo
#error: Too many global registers: 224
