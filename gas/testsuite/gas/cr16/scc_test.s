        .text
        .global main
main:
	##########
	# SCond reg
	##########
	seq	r2
	sne	r3
	scs	r3
	scc	r4
	shi	r5
	sls	r6
	sgt	r7
	sfs	r8
	sfc	r9
	slo	r10
	shs	r1
	slt	r11
	sge	r0
