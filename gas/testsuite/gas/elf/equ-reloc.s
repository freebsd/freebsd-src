	.data
	.long x1, x2

	.global x1, x2, z2

	.set x1, y1
	.set x2, y2
	.set x2, z2

	.section .bss, "aw", %nobits
x1:
	.zero	4
y1:
	.zero	4
y2:
	.zero	4
