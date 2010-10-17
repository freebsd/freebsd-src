	.text
	putx d1
	getx d2
	mulq d1,d2
	mulq 16,d2
	mulq 256,d3
	mulq 131071,d3
	mulqu d1,d2
	mulqu 16,d2
	mulqu 256,d3
	mulqu 131071,d3
	sat16 d2,d3
	sat24 d3,d2
	bsch d1,d2
	
