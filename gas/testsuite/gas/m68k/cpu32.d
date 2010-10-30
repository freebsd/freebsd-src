#name: cpu32
#objdump: -d
#as: -mcpu32

.*:     file format .*

Disassembly of section .text:

0+ <.text>:
[ 0-9a-f]+:	4afa           	bgnd
[ 0-9a-f]+:	f800 2001      	tblub %d0,%d1,%d2
[ 0-9a-f]+:	f800 2041      	tbluw %d0,%d1,%d2
[ 0-9a-f]+:	f800 2081      	tblul %d0,%d1,%d2
[ 0-9a-f]+:	f800 2401      	tblunb %d0,%d1,%d2
[ 0-9a-f]+:	f800 2441      	tblunw %d0,%d1,%d2
[ 0-9a-f]+:	f800 2481      	tblunl %d0,%d1,%d2
[ 0-9a-f]+:	f800 2801      	tblsb %d0,%d1,%d2
[ 0-9a-f]+:	f800 2841      	tblsw %d0,%d1,%d2
[ 0-9a-f]+:	f800 2881      	tblsl %d0,%d1,%d2
[ 0-9a-f]+:	f800 2c01      	tblsnb %d0,%d1,%d2
[ 0-9a-f]+:	f800 2c41      	tblsnw %d0,%d1,%d2
[ 0-9a-f]+:	f800 2c81      	tblsnl %d0,%d1,%d2
[ 0-9a-f]+:	f810 1100      	tblub %a0@,%d1
[ 0-9a-f]+:	f810 1140      	tbluw %a0@,%d1
[ 0-9a-f]+:	f810 1180      	tblul %a0@,%d1
[ 0-9a-f]+:	f810 1500      	tblunb %a0@,%d1
[ 0-9a-f]+:	f810 1540      	tblunw %a0@,%d1
[ 0-9a-f]+:	f810 1580      	tblunl %a0@,%d1
[ 0-9a-f]+:	f810 1900      	tblsb %a0@,%d1
[ 0-9a-f]+:	f810 1940      	tblsw %a0@,%d1
[ 0-9a-f]+:	f810 1980      	tblsl %a0@,%d1
[ 0-9a-f]+:	f810 1d00      	tblsnb %a0@,%d1
[ 0-9a-f]+:	f810 1d40      	tblsnw %a0@,%d1
[ 0-9a-f]+:	f810 1d80      	tblsnl %a0@,%d1
#...
