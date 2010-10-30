#objdump: -dw
#name: x86-64 SSE4.2

.*:     file format .*

Disassembly of section .text:

0+000 <foo>:
[ 	]*[0-9a-f]+:	f2 0f 38 f0 d9       	crc32b %cl,%ebx
[ 	]*[0-9a-f]+:	f2 48 0f 38 f0 d9    	crc32b %cl,%rbx
[ 	]*[0-9a-f]+:	66 f2 0f 38 f1 d9    	crc32w %cx,%ebx
[ 	]*[0-9a-f]+:	f2 0f 38 f1 d9       	crc32l %ecx,%ebx
[ 	]*[0-9a-f]+:	f2 48 0f 38 f1 d9    	crc32q %rcx,%rbx
[ 	]*[0-9a-f]+:	f2 0f 38 f0 19       	crc32b \(%rcx\),%ebx
[ 	]*[0-9a-f]+:	66 f2 0f 38 f1 19    	crc32w \(%rcx\),%ebx
[ 	]*[0-9a-f]+:	f2 0f 38 f1 19       	crc32l \(%rcx\),%ebx
[ 	]*[0-9a-f]+:	f2 48 0f 38 f1 19    	crc32q \(%rcx\),%rbx
[ 	]*[0-9a-f]+:	f2 0f 38 f0 d9       	crc32b %cl,%ebx
[ 	]*[0-9a-f]+:	f2 48 0f 38 f0 d9    	crc32b %cl,%rbx
[ 	]*[0-9a-f]+:	66 f2 0f 38 f1 d9    	crc32w %cx,%ebx
[ 	]*[0-9a-f]+:	f2 0f 38 f1 d9       	crc32l %ecx,%ebx
[ 	]*[0-9a-f]+:	f2 48 0f 38 f1 d9    	crc32q %rcx,%rbx
[ 	]*[0-9a-f]+:	66 0f 38 37 01       	pcmpgtq \(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 37 c1       	pcmpgtq %xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 61 01 00    	pcmpestri \$0x0,\(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 61 c1 00    	pcmpestri \$0x0,%xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 60 01 01    	pcmpestrm \$0x1,\(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 60 c1 01    	pcmpestrm \$0x1,%xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 63 01 02    	pcmpistri \$0x2,\(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 63 c1 02    	pcmpistri \$0x2,%xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 62 01 03    	pcmpistrm \$0x3,\(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 62 c1 03    	pcmpistrm \$0x3,%xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 f3 0f b8 19       	popcnt \(%rcx\),%bx
[ 	]*[0-9a-f]+:	f3 0f b8 19          	popcnt \(%rcx\),%ebx
[ 	]*[0-9a-f]+:	f3 48 0f b8 19       	popcnt \(%rcx\),%rbx
[ 	]*[0-9a-f]+:	66 f3 0f b8 19       	popcnt \(%rcx\),%bx
[ 	]*[0-9a-f]+:	f3 0f b8 19          	popcnt \(%rcx\),%ebx
[ 	]*[0-9a-f]+:	f3 48 0f b8 19       	popcnt \(%rcx\),%rbx
[ 	]*[0-9a-f]+:	66 f3 0f b8 d9       	popcnt %cx,%bx
[ 	]*[0-9a-f]+:	f3 0f b8 d9          	popcnt %ecx,%ebx
[ 	]*[0-9a-f]+:	f3 48 0f b8 d9       	popcnt %rcx,%rbx
[ 	]*[0-9a-f]+:	66 f3 0f b8 d9       	popcnt %cx,%bx
[ 	]*[0-9a-f]+:	f3 0f b8 d9          	popcnt %ecx,%ebx
[ 	]*[0-9a-f]+:	f3 48 0f b8 d9       	popcnt %rcx,%rbx
#pass
