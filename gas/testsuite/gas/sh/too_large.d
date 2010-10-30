#name: Check for bogus overflow errors in .byte directives
#as: -big -relax -isa=sh4a
#nm: -n

[ 	]*U \.L318
[ 	]*U \.L319
[ 	]*U \.L320
[ 	]*U \.L321
0+00100 t \.L307
