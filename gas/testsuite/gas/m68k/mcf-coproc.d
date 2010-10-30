#objdump: -d
#as: -mcpu=5475

.*:     file format .*

Disassembly of section .text:

0+ <start>:
[ 0-9a-f]+:	fcc0 0050      	cp0bcbusy [0-9a-f]+ <zero>
[ 0-9a-f]+:	fc80 2123      	cp0ldl %d0,%d2,#1,#291
[ 0-9a-f]+:	fc88 a201      	cp0ldl %a0,%a2,#2,#1
[ 0-9a-f]+:	fc50 a401      	cp0ldw %a0@,%a2,#3,#1
[ 0-9a-f]+:	fc18 aa01      	cp0ldb %a0@\+,%a2,#6,#1
[ 0-9a-f]+:	fca0 ac01      	cp0ldl %a0@-,%a2,#7,#1
[ 0-9a-f]+:	fca8 ae01 0010 	cp0ldl %a0@\(16\),%a2,#8,#1
[ 0-9a-f]+:	fd80 2123      	cp0stl %d2,%d0,#1,#291
[ 0-9a-f]+:	fd88 a201      	cp0stl %a2,%a0,#2,#1
[ 0-9a-f]+:	fd50 a401      	cp0stw %a2,%a0@,#3,#1
[ 0-9a-f]+:	fd18 aa01      	cp0stb %a2,%a0@\+,#6,#1
[ 0-9a-f]+:	fda0 ac01      	cp0stl %a2,%a0@-,#7,#1
[ 0-9a-f]+:	fda8 ae01 0010 	cp0stl %a2,%a0@\(16\),#8,#1
[ 0-9a-f]+:	fc00 0e00      	cp0nop #8
[ 0-9a-f]+:	fc80 0400      	cp0nop #3
[ 0-9a-f]+:	fc80 1400      	cp0ldl %d0,%d1,#3,#0
[ 0-9a-f]+:	fc88 0400      	cp0ldl %a0,%d0,#3,#0
[ 0-9a-f]+:	fc90 0400      	cp0ldl %a0@,%d0,#3,#0
[ 0-9a-f]+:	fca8 0400 0010 	cp0ldl %a0@\(16\),%d0,#3,#0
[ 0-9a-f]+ <zero>:
[ 0-9a-f]+:	4e71           	nop
[ 0-9a-f]+:	fec0 0050      	cp1bcbusy [0-9a-f]+ <one>
[ 0-9a-f]+:	fe80 2123      	cp1ldl %d0,%d2,#1,#291
[ 0-9a-f]+:	fe88 a201      	cp1ldl %a0,%a2,#2,#1
[ 0-9a-f]+:	fe50 a401      	cp1ldw %a0@,%a2,#3,#1
[ 0-9a-f]+:	fe18 aa01      	cp1ldb %a0@\+,%a2,#6,#1
[ 0-9a-f]+:	fea0 ac01      	cp1ldl %a0@-,%a2,#7,#1
[ 0-9a-f]+:	fea8 ae01 0010 	cp1ldl %a0@\(16\),%a2,#8,#1
[ 0-9a-f]+:	ff80 2123      	cp1stl %d2,%d0,#1,#291
[ 0-9a-f]+:	ff88 a201      	cp1stl %a2,%a0,#2,#1
[ 0-9a-f]+:	ff50 a401      	cp1stw %a2,%a0@,#3,#1
[ 0-9a-f]+:	ff18 aa01      	cp1stb %a2,%a0@\+,#6,#1
[ 0-9a-f]+:	ffa0 ac01      	cp1stl %a2,%a0@-,#7,#1
[ 0-9a-f]+:	ffa8 ae01 0010 	cp1stl %a2,%a0@\(16\),#8,#1
[ 0-9a-f]+:	fe00 0e00      	cp1nop #8
[ 0-9a-f]+:	fe80 0400      	cp1nop #3
[ 0-9a-f]+:	fe80 1400      	cp1ldl %d0,%d1,#3,#0
[ 0-9a-f]+:	fe88 0400      	cp1ldl %a0,%d0,#3,#0
[ 0-9a-f]+:	fe90 0400      	cp1ldl %a0@,%d0,#3,#0
[ 0-9a-f]+:	fea8 0400 0010 	cp1ldl %a0@\(16\),%d0,#3,#0
[ 0-9a-f]+ <one>:
[ 0-9a-f]+:	4e71           	nop
