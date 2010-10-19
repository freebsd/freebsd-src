       .section .text
       .global _fun

xc16x_cpl_cplb:

	cpl   r0
	cplb rl0
