	.macro  one_sym count
	.globl  sym_2_\count
sym_2_\count:
        la      $2, sym_2_\count
	.endm

	.text
	.ent	func2
func2:
	.frame	$sp,0,$31
	.set noreorder
	.cpload	$25
	.set reorder
	.cprestore 8
	.set noreorder

	.irp    thou,0,1,2,3,4,5,6,7,8
	.irp    hund,0,1,2,3,4,5,6,7,8,9
	.irp    tens,0,1,2,3,4,5,6,7,8,9
	.irp    ones,0,1,2,3,4,5,6,7,8,9
	one_sym \thou\hund\tens\ones
	.endr
	.endr
	.endr
	.endr

	.end	func2
