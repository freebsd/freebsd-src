#source: b-twoinsn.s
#source: b-post1.s
#source: b-nosym.s
#ld: --oformat binary
#objcopy_linked_file:
#objdump: -st 2>/dev/null

# Note that we have to redirect stderr when objdumping to get rid of the
# "no symbols" message that would otherwise cause a spurious failure and
# which we seemingly can't identify or prune in another way.

.*:     file format mmo

SYMBOL TABLE:
no symbols

Contents of section \.text:
 0000 e3fd0001 e3fd0004                    .*
