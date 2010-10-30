#objdump: -dwMintel
#name: i386 crc32 (Intel disassembly)
#source: crc32.s

.*: +file format .*

Disassembly of section .text:

0+ <foo>:
[ 	]*[a-f0-9]+:	f2 0f 38 f0 06       	crc32  eax,BYTE PTR \[esi\]
[ 	]*[a-f0-9]+:	66 f2 0f 38 f1 06    	crc32  eax,WORD PTR \[esi\]
[ 	]*[a-f0-9]+:	f2 0f 38 f1 06       	crc32  eax,DWORD PTR \[esi\]
[ 	]*[a-f0-9]+:	f2 0f 38 f0 c0       	crc32  eax,al
[ 	]*[a-f0-9]+:	f2 0f 38 f0 c0       	crc32  eax,al
[ 	]*[a-f0-9]+:	66 f2 0f 38 f1 c0    	crc32  eax,ax
[ 	]*[a-f0-9]+:	66 f2 0f 38 f1 c0    	crc32  eax,ax
[ 	]*[a-f0-9]+:	f2 0f 38 f1 c0       	crc32  eax,eax
[ 	]*[a-f0-9]+:	f2 0f 38 f1 c0       	crc32  eax,eax
[ 	]*[a-f0-9]+:	f2 0f 38 f0 06       	crc32  eax,BYTE PTR \[esi\]
[ 	]*[a-f0-9]+:	66 f2 0f 38 f1 06    	crc32  eax,WORD PTR \[esi\]
[ 	]*[a-f0-9]+:	f2 0f 38 f1 06       	crc32  eax,DWORD PTR \[esi\]
[ 	]*[a-f0-9]+:	f2 0f 38 f0 c0       	crc32  eax,al
[ 	]*[a-f0-9]+:	66 f2 0f 38 f1 c0    	crc32  eax,ax
[ 	]*[a-f0-9]+:	f2 0f 38 f1 c0       	crc32  eax,eax
#pass
