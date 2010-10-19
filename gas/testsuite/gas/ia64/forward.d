# as: -xexplicit
# objdump: -d
# name ia64 forward references

.*: +file format .*

Disassembly of section \.text:

0+ <_start>:
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+\[MIB\][[:space:]]+alloc r31=ar.pfs,12,6,8
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+[[:space:]]+dep.z r2=1,5,7
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+\(p0?6\)[[:space:]]+br.cond.sptk.few 0+ <_start>;;
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+\[MIB\][[:space:]]+alloc r31=ar.pfs,0,0,0
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+[[:space:]]+dep.z r3=-1,1,1
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+\(p0?7\)[[:space:]]+br(\.cond)?\.sptk(\.few)? [[:xdigit:]]+0 <.*>;;
