/* $FreeBSD$ */

#include <machine/asmacros.h>

	.data
smapi32_segment_tmp:	.word	0
smapi32_offset_tmp:	.long	0
	.text
/*
 * smapi32(input_param, output_param)
 *	struct smapi_bios_parameter *input_parm;
 *	struct smapi_bios_parameter *output_parm;
 */
ENTRY(smapi32)
	pushl	%ebp			/* Save frame */
	movl	%esp,%ebp

	pushl	%ds
	pushl	0x0c(%ebp)		/* Output Param */
	pushl	%ds
	pushl	0x08(%ebp)		/* Input Param */

	movl	$0,%eax			/* Clear EAX (return 0) */
	movw	%cs,smapi32_segment	/* Save CS */
	lcall	*(smapi32_offset)

	leave
	ret

/*
 * smapi32(offset, segment, input_param, output_param)
 *	u_int offset;
 *	u_short segment;
 *	struct smapi_bios_parameter *input_parm;
 *	struct smapi_bios_parameter *output_parm;
 */
ENTRY(smapi32_new)
	pushl	%ebp			/* Save frame */
        movl    0x08(%esp),%ebp
	movl	%ebp,smapi32_offset_tmp
	movw	0x0c(%esp),%bp
	movw	%bp,smapi32_segment_tmp

	pushl	%ds
	pushl	0x20(%ebp)		/* Output Param */
	pushl	%ds
	pushl	0x10(%ebp)		/* Input Param */

	movl	$0,%eax
	movw	%cs,smapi32_segment_tmp
	lcall	*(smapi32_offset_tmp)

	leave
	ret
