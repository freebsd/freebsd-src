# Streaming SIMD extensions 4.2 Instructions

	.text
foo:
	crc32		%cl,%ebx
	crc32		%cx,%ebx
	crc32		%ecx,%ebx
	crc32b		(%ecx),%ebx
	crc32w		(%ecx),%ebx
	crc32l		(%ecx),%ebx
	crc32b		%cl,%ebx
	crc32w		%cx,%ebx
	crc32l		%ecx,%ebx
	pcmpgtq		(%ecx),%xmm0
	pcmpgtq		%xmm1,%xmm0
	pcmpestri	$0x0,(%ecx),%xmm0
	pcmpestri	$0x0,%xmm1,%xmm0
	pcmpestrm	$0x1,(%ecx),%xmm0
	pcmpestrm	$0x1,%xmm1,%xmm0
	pcmpistri	$0x2,(%ecx),%xmm0
	pcmpistri	$0x2,%xmm1,%xmm0
	pcmpistrm	$0x3,(%ecx),%xmm0
	pcmpistrm	$0x3,%xmm1,%xmm0
	popcnt		(%ecx),%bx
	popcnt		(%ecx),%ebx
	popcntw		(%ecx),%bx
	popcntl		(%ecx),%ebx
	popcnt		%cx,%bx
	popcnt		%ecx,%ebx
	popcntw		%cx,%bx
	popcntl		%ecx,%ebx

	.p2align	4,0
