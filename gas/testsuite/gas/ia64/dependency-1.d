# as: -xexplicit
# objdump: -d
# name: IA64 read-before-write dependency

# Note - this test is based on a bug reported here:
#  http://sources.redhat.com/ml/bug-gnu-utils/2003-03/msg00270.html
# With follows up on the binutils mailing list here:
#  http://sources.redhat.com/ml/binutils/2003-04/msg00162.html

.*: +file format .*

Disassembly of section \.text:

0+000 <foo>:
   0:.*0b 40 00 40 10 18.*\[MMI\].*ldfs f8=\[r32\];;
   6:.*00 40 84 30 33 00.*stfd \[r33\]=f8
   c:.*00 00 04 00.*nop\.i 0x0;;
