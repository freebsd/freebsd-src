#name: mcf-trap
#objdump: -d
#as: -m5208

.*:     file format .*

Disassembly of section .text:

0+ <.text>:
[ 0-9a-f]+:	51fc           	t[rap]*f
[ 0-9a-f]+:	51fa 1234      	t[rap]*fw #4660
[ 0-9a-f]+:	51fb 1234 5678 	t[rap]*fl #305419896
[ 0-9a-f]+:	51fc           	t[rap]*f
[ 0-9a-f]+:	51fa 1234      	t[rap]*fw #4660
[ 0-9a-f]+:	51fb 1234 5678 	t[rap]*fl #305419896
