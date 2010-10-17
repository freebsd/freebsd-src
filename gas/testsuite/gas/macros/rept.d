#objdump: -r
#name: macro rept

.*: +file format .*

RELOCATION RECORDS FOR .*
OFFSET[ 	]+TYPE[ 	]+VALUE.*
0+00[ 	]+[a-zA-Z0-9_]+[ 	]+foo1
0+04[ 	]+[a-zA-Z0-9_]+[ 	]+foo1
0+08[ 	]+[a-zA-Z0-9_]+[ 	]+foo1
