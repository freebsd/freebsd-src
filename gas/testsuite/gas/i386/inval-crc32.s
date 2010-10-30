# Check illegal crc32 in SSE4.2

	.text
foo:

crc32b (%esi), %al
crc32w (%esi), %ax
crc32 (%esi), %al
crc32 (%esi), %ax
crc32 (%esi), %eax
crc32  %al, %al
crc32b  %al, %al
crc32  %ax, %ax
crc32w  %ax, %ax

.intel_syntax noprefix
crc32  al,byte ptr [esi]
crc32  ax, word ptr [esi]
crc32  al, [esi]
crc32  ax, [esi]
crc32  eax, [esi]
crc32  al,al
crc32  ax, ax
