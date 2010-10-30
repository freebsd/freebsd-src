# Check 64bit crc32 in SSE4.2

	.text
foo:

crc32b (%rsi), %eax
crc32b (%rsi), %rax
crc32w (%rsi), %eax
crc32l (%rsi), %eax
crc32q (%rsi), %rax
crc32  %al, %eax
crc32b  %al, %eax
crc32  %al, %rax
crc32b  %al, %rax
crc32  %ax, %eax
crc32w  %ax, %eax
crc32  %eax, %eax
crc32l  %eax, %eax
crc32  %rax, %rax
crc32q  %rax, %rax

.intel_syntax noprefix
crc32  rax,byte ptr [rsi]
crc32  eax,byte ptr [rsi]
crc32  eax, word ptr [rsi]
crc32  eax,dword ptr [rsi]
crc32  rax,qword ptr [rsi]
crc32  eax,al
crc32  rax,al
crc32  eax, ax
crc32  eax,eax
crc32  rax,rax

.p2align 4,0
