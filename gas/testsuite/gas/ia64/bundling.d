# objdump: -d
# name: ia64 explicit bundling

.*: +file format .*

Disassembly of section \.text:

0+0 <_start>:
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+\[MII]       nop\.m 0x0
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+nop\.i 0x0;;
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+mov\.i r31=ar\.lc;;
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+\[..B]       nop\.. 0x0
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+nop\.. 0x0
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+br\.ret\.sptk\.few b0;;
