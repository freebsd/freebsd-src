	.text
	.arm
	.global mapping
mapping:
	nop
	bl mapping

	.global thumb_mapping
	.thumb_func
thumb_mapping:
	.thumb
	nop
	bl thumb_mapping
	
	.data
	.word 0x123456

	.section foo,"ax"
	nop
