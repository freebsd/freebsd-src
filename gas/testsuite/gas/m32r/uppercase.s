	.text
	.global foo
foo:
	mv R0,R1
	mvfc R0,CBR

high:
	seth r0,#HIGH(high)
shigh:
	seth r0,#SHIGH(shigh)
low:
	or3 r0,r0,#LOW(low)
sda:
	add3 r0,r0,#SDA(sdavar)
