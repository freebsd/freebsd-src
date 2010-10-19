#nm: --defined-only
#name: weakref tests, local syms
#source: weakref1.s
# aix drops local symbols
# see weakref1.d for comments on the other not-targets
#not-target: *-*-aix* alpha*-*-osf* *-*-ecoff pdp11-*-aout

# the rest of this file is generated with the following script:
# # script begin
# sed -n 's,^\(l[^ ]*\)[ 	]*:.*,.* t \1,p;s:^[ 	]*\.set[ 	][ 	]*\(l[^ ]*\)[ 	]*,.*:.* t \1:p' weakref1.s | uniq | while read line; do echo "#..."; echo "$line"; done
# echo \#pass
# # script output:
#...
.* t l
#...
.* t ld1
#...
.* t ld2
#...
.* t ld3
#...
.* t ld4
#...
.* t ld8
#...
.* t ld9
#pass
