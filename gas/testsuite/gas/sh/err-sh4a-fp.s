! { dg-do assemble }

	.text
	.p2align 2

	fpchg	fpul		! { dg-error "excess operands" }

	fsrra	fr1, fr2	! { dg-error "excess operands" }
	fsrra			! { dg-error "invalid operands|missing operand" }
	fsrra	fpul		! { dg-error "invalid operands" }
	fsrra	dr0, dr2	! { dg-error "invalid operands" }

	fsca	dr0, fpul	! { dg-error "invalid operands" }
	fsca	fpul, fr0	! { dg-error "invalid operands" }
	fsca	fpul		! { dg-error "invalid operands|missing operand" }
