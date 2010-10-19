#objdump: -r
#name: macro vararg

.*: +file format .*

RELOCATION RECORDS FOR .*
OFFSET[ 	]+TYPE[ 	]+VALUE.*
0+00[ 	]+[a-zA-Z0-9_]+[ 	]+foo1
0+04[ 	]+[a-zA-Z0-9_]+[ 	]+foo2
0+08[ 	]+[a-zA-Z0-9_]+[ 	]+foo3
0+0c[ 	]+[a-zA-Z0-9_]+[ 	]+foo4
0+10[ 	]+[a-zA-Z0-9_]+[ 	]+foo5
0+14[ 	]+[a-zA-Z0-9_]+[ 	]+foo6
