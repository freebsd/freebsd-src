#source: keepdot.s
#ld: --gc-sections -Bstatic -e _start -T keepdot0.ld
#name: Preserve explicit . = 0
#objdump: -h

# Check that GC:ing does not mess up the value for dot when specified
# as 0.

#...
[ 	]+.[ 	]+\.myinit[ 	]+0+[48][ 	]+0+[ 	]+0+ .*
#pass
