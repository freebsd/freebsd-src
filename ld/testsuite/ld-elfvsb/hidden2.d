#source: hidden2.s
#ld: -shared -T hidden2.ld
#readelf: -Ds
# It is also ok to remove this symbol, but we currently make it local.

Symbol table for image:
#...
[ 	]*[0-9]+ +[0-9]+: [0-9a-fA-F]* +0  OBJECT  LOCAL HIDDEN +ABS foo
#pass
