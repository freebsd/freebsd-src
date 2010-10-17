# Test that .text section alignments use nops (0xA0000000) to fill
# rather than 0.
	.text
	adds	%r4,%r5,%r6
	.align 16
	adds	%r10,%r11,%r12
        fmlow.dd        %f22,%f24,%f26
        pfadd.ss        %f14,%f15,%f16
        pfadd.sd        %f17,%f18,%f20


