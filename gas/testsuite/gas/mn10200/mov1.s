	.text
	mov d1,a2
	mov a2,d1
	mov d1,d2
	mov a2,a1
	mov psw,d3
	mov d2,psw
	mov mdr,d1
	mov d2,mdr
	mov (a2),d1
	mov (8,a2),d1
	mov (256,a2),d1
	mov (131071,a2),d1
