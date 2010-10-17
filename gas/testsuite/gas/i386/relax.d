#name: i386 relax
#objdump: -s

.*: +file format .*i386.*

Contents of section .text:
 0+00 90 ?90 ?eb ?14 eb ?12 ?41 ?42 43 ?44 ?45 ?46 47 ?48 ?49 ?00  .*
 0+10 00 ?00 ?00 ?00 00 ?00 ?00 ?00  .*
Contents of section .gcc_except_table:
 0+000 02[ 	]*.[ 	]*
Contents of section .gnu.linkonce.t.blah:
 0+000 eb00[ 	]+..[ 	]*
