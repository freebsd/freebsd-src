#objdump: -t
#name: cofftag

.*:     file format .*

SYMBOL TABLE:
\[  0\]\(sec -2\)\(fl 0x00\)\(ty   0\)\(scl 103\) \(nx 1\) 0x0+0000 foo.c
File 
\[  2\]\(sec  1\)\(fl 0x00\)\(ty   0\)\(scl   6\) \(nx 0\) 0x0+0000 gcc2_compiled.
\[  3\]\(sec  1\)\(fl 0x00\)\(ty   0\)\(scl   6\) \(nx 0\) 0x0+0000 ___gnu_compiled_c
\[  4\]\(sec -2\)\(fl 0x00\)\(ty   a\)\(scl  15\) \(nx 1\) 0x0+0000 _token
AUX lnno 0 size 0x4 tagndx 0 endndx 10
\[  6\]\(sec -(1|2)\)\(fl 0x00\)\(ty   b\)\(scl  16\) \(nx 0\) 0x0+0000 _operator
\[  7\]\(sec -(1|2)\)\(fl 0x00\)\(ty   b\)\(scl  16\) \(nx 0\) 0x0+0001 _flags
\[  8\]\(sec -(1|2)\)\(fl 0x00\)\(ty   0\)\(scl 102\) \(nx 1\) 0x0+0004 .eos
AUX lnno 0 size 0x4 tagndx 4
\[ 10\]\(sec  1\)\(fl 0x00\)\(ty   0\)\(scl   3\) \(nx 1\) 0x[0-9a-f]+ .text
AUX scnlen 0x[0-9a-f]+ nreloc 0 nlnno 0
\[ 12\]\(sec  2\)\(fl 0x00\)\(ty   0\)\(scl   3\) \(nx 1\) 0x[0-9a-f]+ .data
AUX scnlen 0x[0-9a-f]+ nreloc 0 nlnno 0
\[ 14\]\(sec  3\)\(fl 0x00\)\(ty   0\)\(scl   3\) \(nx 1\) 0x[0-9a-f]+ .bss
AUX scnlen 0x[0-9a-f]+ nreloc 0 nlnno 0
\[ 16\]\(sec  2\)\(fl 0x00\)\(ty   2\)\(scl   2\) \(nx 0\) 0x0+0000 _token
\[ 17\]\(sec  2\)\(fl 0x00\)\(ty   a\)\(scl   2\) \(nx 1\) 0x[0-9a-f]+ _what
AUX lnno 0 size 0x4 tagndx 4
