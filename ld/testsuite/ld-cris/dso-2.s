	.text
	.global	export_1
	.type	export_1,@function
export_1:
	jump [$r1+dsofn:GOTPLT16]
	jump [$r1+dsofn:GOTPLT]
