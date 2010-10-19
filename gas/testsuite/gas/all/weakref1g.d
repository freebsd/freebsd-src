#nm: --defined-only --extern-only
#name: weakref tests, global syms
#source: weakref1.s
# see weakref1.d for comments on the not-targets
# ecoff (OSF/alpha) lacks .weak support
# pdp11 lacks .long
#not-target: alpha*-*-osf* *-*-ecoff pdp11-*-aout

# the rest of this file is generated with the following script:
# # script begin
# echo \#...
# sed -n 's,^[ 	]*\.global \(g.*\),.* T \1,p' weakref1.s | uniq
# echo \#pass
# # script output:
#...
.* T gd6
.* T gd7
#pass
