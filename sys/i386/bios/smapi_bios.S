#include <machine/asm.h>
__FBSDID("$FreeBSD$");

#include <machine/asmacros.h>

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
	movl	%esp,%ebp

	movl	0x08(%ebp),%eax
	movl	%eax,smapi32_offset
	movw	0x0c(%ebp),%ax
	movw	%ax,smapi32_segment

	pushl	%ds
	pushl	0x20(%ebp)		/* Output Param */
	pushl	%ds
	pushl	0x10(%ebp)		/* Input Param */

	movl	$0,%eax
	movw	%cs,smapi32_segment
	lcall	*(smapi32_offset)

	leave
	ret
