# Check illegal 64bit crc32 in SSE4.2

	.text
foo:

crc32b (%rsi), %al
crc32w (%rsi), %ax
crc32 (%rsi), %al
crc32 (%rsi), %ax
crc32 (%rsi), %eax
crc32 (%rsi), %rax
crc32  %al, %al
crc32b  %al, %al
crc32  %ax, %ax
crc32w  %ax, %ax
crc32  %rax, %eax
crc32  %eax, %rax
crc32l  %rax, %eax
crc32l  %eax, %rax
crc32q  %eax, %rax
crc32q  %rax, %eax

.intel_syntax noprefix
crc32  al,byte ptr [rsi]
crc32  ax, word ptr [rsi]
crc32  rax,word ptr [rsi]
crc32  rax,dword ptr [rsi]
crc32  al,[rsi]
crc32  ax,[rsi]
crc32  eax,[rsi]
crc32  rax,[rsi]
crc32  al,al
crc32  ax, ax
crc32  rax,eax
