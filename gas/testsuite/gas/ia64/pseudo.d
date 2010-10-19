# as: -xnone -mtune=itanium1
# objdump: -d
# name: ia64 pseudo-ops

.*: +file format .*

Disassembly of section \.text:

0+0 <_start>:
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+(\[[[:upper:]]+\])?[[:space:]]+alloc r8=ar\.pfs,0,0,0
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+(\[[[:upper:]]+\])?[[:space:]]+cmp\.eq p6,p0=r0,r0
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+(\[[[:upper:]]+\])?[[:space:]]+cmp\.eq p7,p0=0,r0
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+(\[[[:upper:]]+\])?[[:space:]]+cmp4\.eq p8,p0=r0,r0
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+(\[[[:upper:]]+\])?[[:space:]]+nop\.. 0x0
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+(\[[[:upper:]]+\])?[[:space:]]+cmp4\.eq p9,p0=0,r0
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+(\[[[:upper:]]+\])?[[:space:]]+cmp8xchg16\.acq r9=\[r0\],r0,ar\.csd,ar\.ccv
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+(\[[[:upper:]]+\])?[[:space:]]+cmpxchg4\.acq r10=\[r0\],r0,ar\.ccv
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+(\[[[:upper:]]+\])?[[:space:]]+fclass\.m p10,p0=f0,0x1
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+(\[[[:upper:]]+\])?[[:space:]]+nop\.. 0x0
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+(\[[[:upper:]]+\])?[[:space:]]+fcmp\.eq\.s0 p11,p0=f0,f0
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+(\[[[:upper:]]+\])?[[:space:]]+nop\.. 0x0
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+(\[[[:upper:]]+\])?[[:space:]]+ld16 r11,ar\.csd=\[r0\]
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+(\[[[:upper:]]+\])?[[:space:]]+nop\.. 0x0
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+(\[[[:upper:]]+\])?[[:space:]]+mov pr=r0,0xfffffffffffffffe
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+(\[[[:upper:]]+\])?[[:space:]]+st16 \[r0\]=r0,ar\.csd
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+(\[[[:upper:]]+\])?[[:space:]]+tbit\.z p0,p12=r0,0
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+(\[[[:upper:]]+\])?[[:space:]]+tnat\.z p0,p13=r0(;;)?
#...
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+(\[[[:upper:]]+\])?[[:space:]]+tf\.z p3,p2=33(;;)?
