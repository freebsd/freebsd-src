.*: Assembler messages:
.*:6: Error: .*
.*:7: Error: .*
.*:8: Error: .*
.*:9: Error: .*
.*:10: Error: .*
.*:11: Error: .*
.*:12: Error: .*
.*:13: Error: .*
.*:14: Error: .*
.*:17: Error: .*
.*:18: Error: .*
.*:19: Error: .*
.*:20: Error: .*
.*:21: Error: .*
.*:22: Error: .*
.*:23: Error: .*
GAS LISTING .*


[ 	]*1[ 	]+\# Check illegal crc32 in SSE4\.2
[ 	]*2[ 	]+
[ 	]*3[ 	]+\.text
[ 	]*4[ 	]+foo:
[ 	]*5[ 	]+
[ 	]*6[ 	]+crc32b \(%esi\), %al
[ 	]*7[ 	]+crc32w \(%esi\), %ax
[ 	]*8[ 	]+crc32 \(%esi\), %al
[ 	]*9[ 	]+crc32 \(%esi\), %ax
[ 	]*10[ 	]+crc32 \(%esi\), %eax
[ 	]*11[ 	]+crc32  %al, %al
[ 	]*12[ 	]+crc32b  %al, %al
[ 	]*13[ 	]+crc32  %ax, %ax
[ 	]*14[ 	]+crc32w  %ax, %ax
[ 	]*15[ 	]+
[ 	]*16[ 	]+\.intel_syntax noprefix
[ 	]*17[ 	]+crc32  al,byte ptr \[esi\]
[ 	]*18[ 	]+crc32  ax, word ptr \[esi\]
[ 	]*19[ 	]+crc32  al, \[esi\]
[ 	]*20[ 	]+crc32  ax, \[esi\]
[ 	]*21[ 	]+crc32  eax, \[esi\]
[ 	]*22[ 	]+crc32  al,al
[ 	]*23[ 	]+crc32  ax, ax
