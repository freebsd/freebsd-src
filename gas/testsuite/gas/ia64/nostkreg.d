#objdump: -dr
#name: ia64 not stacked registers

.*: +file format .*

Disassembly of section \.text:

0+000 <_start>:
[[:space:]]*[[:xdigit:]]+:[[:space:][:xdigit:]]+\[M[IM]I\][[:space:]]+mov[[:space:]]+r5=0
[[:space:]]+0:[[:space:]]+IMM22[[:space:]]+in00
[[:space:]]+1:[[:space:]]+IMM22[[:space:]]+loc96
[[:space:]]*[[:xdigit:]]+:[[:space:][:xdigit:]]+mov[[:space:]]+r6=0
[[:space:]]*[[:xdigit:]]+:[[:space:][:xdigit:]]+mov[[:space:]]+r7=r32
[[:space:]]*[[:xdigit:]]+:[[:space:][:xdigit:]]+\[M[IM]B\][[:space:]]+mov[[:space:]]+r8=r34
[[:space:]]*[[:xdigit:]]+:[[:space:][:xdigit:]]+mov[[:space:]]+r9=r36
[[:space:]]*[[:xdigit:]]+:[[:space:][:xdigit:]]+br\.ret\.sptk\.few[[:space:]]+(b0|rp);;
