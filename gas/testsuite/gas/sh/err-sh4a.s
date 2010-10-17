! { dg-do assemble }

	.text
	.p2align 2

	movli.l @r7,r13		! { dg-error "invalid operands" }
	movco.l r1,@r0		! { dg-error "invalid operands" }

	movli.l r0,@r0		! { dg-error "invalid operands" }
	movco.l @r0,r0		! { dg-error "invalid operands" }

	movli.l	r1		! { dg-error "invalid operands|missing operand" }
	movco.l r0		! { dg-error "invalid operands|missing operand" }

	movli.l @r1,r0,r2	! { dg-error "excess operands" }
	movco.l r0,@r1,r2	! { dg-error "excess operands" }

	movua.l @r0,r1		! { dg-error "invalid operands" }
	movua.l @r0,r1,r2	! { dg-error "invalid operands" }
	movua.l @r1+		! { dg-error "invalid operands|missing operand" }
	movua.l r0,@r1		! { dg-error "invalid operands" }
	movua.l @(r0,r1),r2	! { dg-error "invalid operands" }
	movua.l @-r5,r1		! { dg-error "invalid operands" }

	icbi	r0		! { dg-error "invalid operands" }

	prefi	r7		! { dg-error "invalid operands" }

	synco	r0		! { dg-error "excess operands" }
