	.text
	mov d1,d2
	mov d1,a2
	mov a2,d1
	mov a2,a1
	mov sp,a2
	mov a1,sp
	mov d2,psw
	mov mdr,d1
	mov d2,mdr
	mov (a2),d1
	mov (8,a2),d1
	mov (256,a2),d1
	mov (131071,a2),d1
	mov (8,sp),d1
	mov (256,sp),d1
	mov psw,d3
