#objdump: -r
# name : addends
.*: +file format a.out-sunos-big

RELOCATION RECORDS FOR \[.text\]:
OFFSET   TYPE +VALUE 
0+08 WDISP22 +foo1\+0xf+fc
0+0c WDISP22 +foo1\+0xf+f8
0+10 WDISP22 +foo1\+0xf+f0
0+14 WDISP22 +foo1\+0xf+ec
0+1c 32 +foo1
0+20 32 +foo1\+0x0+4
#0+20 32 +foo1\+0x0+4
