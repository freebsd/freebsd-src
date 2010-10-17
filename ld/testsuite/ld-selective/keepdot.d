#ld: --gc-sections -Bstatic -e _start -T keepdot.ld
#name: Preserve default . = 0
#objdump: -h

# Check that GC:ing does not mess up the default value for dot.

#...
[ 	]+.[ 	]+\.myinit[ 	]+0+[48][ 	]+0+[ 	]+0+ .*
#pass
