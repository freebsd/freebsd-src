#source: sec-1.s
#source: start.s
#source: data1.s
#ld: -m mmo
#objdump: -sh

.*:     file format mmo

Sections:
Idx Name          Size      VMA               LMA               File off  Algn
  0 \.text         0+4  0+  0+  0+  2\*\*2
                  CONTENTS, ALLOC, LOAD, CODE
  1 secname       0+19  0+4  0+4  0+  2\*\*2
                  CONTENTS, ALLOC, LOAD, READONLY, CODE
  2 \.a\.fourth\.section 0+10  0+20  0+20  0+  2\*\*2
                  CONTENTS, ALLOC, LOAD, READONLY, DATA
  3 \.data         0+4  2000000000000004  2000000000000004  0+  2\*\*2
                  CONTENTS, ALLOC, LOAD
  4 anothersec    0+13  2000000000000008  2000000000000008  0+  2\*\*2
                  CONTENTS, ALLOC, LOAD, DATA
  5 thirdsec      0+a  200000000000001c  200000000000001c  0+  2\*\*2
                  CONTENTS, READONLY
Contents of section \.text:
 0000 e3fd0001                             .*
Contents of section secname:
 0004 00000001 00000002 00000003 00000004  .*
 0014 ffffffff fffff827 50                 .*
Contents of section \.a\.fourth\.section:
 0020 00000000 0087a238 00000000 302a55a8  .*
Contents of section \.data:
 2000000000000004 0000002c                             .*
Contents of section anothersec:
 2000000000000008 0000000a 00000009 00000008 00000007  .*
 2000000000000018 252729                               .*
Contents of section thirdsec:
 200000000000001c 00030d41 000186a2 2628               .*
