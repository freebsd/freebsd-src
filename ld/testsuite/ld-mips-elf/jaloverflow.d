#name: JAL overflow
#source: jaloverflow.s
#as:
#ld: -Ttext=0xffffff0 -e start
#error: .*relocation truncated to fit.*

# This tests whether we correctly detect overflow in the jal
# instruction.  jal is a bit weird since the upper four bits of the
# destination address are taken from the source address.  So overflow
# occurs if the source and destination address do not have the same
# most significant four bits.
