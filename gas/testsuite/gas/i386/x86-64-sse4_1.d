#objdump: -dw
#name: x86-64 SSE4.1

.*:     file format .*

Disassembly of section .text:

0+000 <foo>:
[ 	]*[0-9a-f]+:	66 0f 3a 0d 01 00    	blendpd \$0x0,\(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 0d c1 00    	blendpd \$0x0,%xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 0c 01 00    	blendps \$0x0,\(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 0c c1 00    	blendps \$0x0,%xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 15 01       	blendvpd %xmm0,\(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 15 c1       	blendvpd %xmm0,%xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 14 01       	blendvps %xmm0,\(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 14 c1       	blendvps %xmm0,%xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 41 01 00    	dppd   \$0x0,\(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 41 c1 00    	dppd   \$0x0,%xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 40 01 00    	dpps   \$0x0,\(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 40 c1 00    	dpps   \$0x0,%xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 48 0f 3a 17 c1 00 	extractps \$0x0,%xmm0,%rcx
[ 	]*[0-9a-f]+:	66 0f 3a 17 c1 00    	extractps \$0x0,%xmm0,%ecx
[ 	]*[0-9a-f]+:	66 0f 3a 17 01 00    	extractps \$0x0,%xmm0,\(%rcx\)
[ 	]*[0-9a-f]+:	66 0f 3a 21 c1 00    	insertps \$0x0,%xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 21 01 00    	insertps \$0x0,\(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 2a 01       	movntdqa \(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 42 01 00    	mpsadbw \$0x0,\(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 42 c1 00    	mpsadbw \$0x0,%xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 2b 01       	packusdw \(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 2b c1       	packusdw %xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 10 01       	pblendvb %xmm0,\(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 10 c1       	pblendvb %xmm0,%xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 0e 01 00    	pblendw \$0x0,\(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 0e c1 00    	pblendw \$0x0,%xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 29 c1       	pcmpeqq %xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 29 01       	pcmpeqq \(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 48 0f 3a 14 c1 00 	pextrb \$0x0,%xmm0,%rcx
[ 	]*[0-9a-f]+:	66 0f 3a 14 c1 00    	pextrb \$0x0,%xmm0,%ecx
[ 	]*[0-9a-f]+:	66 0f 3a 14 01 00    	pextrb \$0x0,%xmm0,\(%rcx\)
[ 	]*[0-9a-f]+:	66 0f 3a 16 c1 00    	pextrd \$0x0,%xmm0,%ecx
[ 	]*[0-9a-f]+:	66 0f 3a 16 01 00    	pextrd \$0x0,%xmm0,\(%rcx\)
[ 	]*[0-9a-f]+:	66 48 0f 3a 16 c1 00 	pextrq \$0x0,%xmm0,%rcx
[ 	]*[0-9a-f]+:	66 48 0f 3a 16 01 00 	pextrq \$0x0,%xmm0,\(%rcx\)
[ 	]*[0-9a-f]+:	66 48 0f c5 c8 00    	pextrw \$0x0,%xmm0,%rcx
[ 	]*[0-9a-f]+:	66 0f c5 c8 00       	pextrw \$0x0,%xmm0,%ecx
[ 	]*[0-9a-f]+:	66 0f 3a 15 01 00    	pextrw \$0x0,%xmm0,\(%rcx\)
[ 	]*[0-9a-f]+:	66 0f 38 41 c1       	phminposuw %xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 41 01       	phminposuw \(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 20 01 00    	pinsrb \$0x0,\(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 20 c1 00    	pinsrb \$0x0,%ecx,%xmm0
[ 	]*[0-9a-f]+:	66 48 0f 3a 20 c1 00 	pinsrb \$0x0,%rcx,%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 22 01 00    	pinsrd \$0x0,\(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 22 c1 00    	pinsrd \$0x0,%ecx,%xmm0
[ 	]*[0-9a-f]+:	66 48 0f 3a 22 01 00 	pinsrq \$0x0,\(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 48 0f 3a 22 c1 00 	pinsrq \$0x0,%rcx,%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 3c c1       	pmaxsb %xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 3c 01       	pmaxsb \(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 3d c1       	pmaxsd %xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 3d 01       	pmaxsd \(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 3f c1       	pmaxud %xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 3f 01       	pmaxud \(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 3e c1       	pmaxuw %xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 3e 01       	pmaxuw \(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 38 c1       	pminsb %xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 38 01       	pminsb \(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 39 c1       	pminsd %xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 39 01       	pminsd \(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 3b c1       	pminud %xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 3b 01       	pminud \(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 3a c1       	pminuw %xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 3a 01       	pminuw \(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 20 c1       	pmovsxbw %xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 20 01       	pmovsxbw \(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 21 c1       	pmovsxbd %xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 21 01       	pmovsxbd \(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 22 c1       	pmovsxbq %xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 22 01       	pmovsxbq \(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 23 c1       	pmovsxwd %xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 23 01       	pmovsxwd \(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 24 c1       	pmovsxwq %xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 24 01       	pmovsxwq \(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 25 c1       	pmovsxdq %xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 25 01       	pmovsxdq \(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 30 c1       	pmovzxbw %xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 30 01       	pmovzxbw \(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 31 c1       	pmovzxbd %xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 31 01       	pmovzxbd \(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 32 c1       	pmovzxbq %xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 32 01       	pmovzxbq \(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 33 c1       	pmovzxwd %xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 33 01       	pmovzxwd \(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 34 c1       	pmovzxwq %xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 34 01       	pmovzxwq \(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 35 c1       	pmovzxdq %xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 35 01       	pmovzxdq \(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 28 c1       	pmuldq %xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 28 01       	pmuldq \(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 40 c1       	pmulld %xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 40 01       	pmulld \(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 17 c1       	ptest  %xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 38 17 01       	ptest  \(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 09 01 00    	roundpd \$0x0,\(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 09 c1 00    	roundpd \$0x0,%xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 08 01 00    	roundps \$0x0,\(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 08 c1 00    	roundps \$0x0,%xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 0b 01 00    	roundsd \$0x0,\(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 0b c1 00    	roundsd \$0x0,%xmm1,%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 0a 01 00    	roundss \$0x0,\(%rcx\),%xmm0
[ 	]*[0-9a-f]+:	66 0f 3a 0a c1 00    	roundss \$0x0,%xmm1,%xmm0
#pass
