#include <machine/asm.h>

.ident	"$FreeBSD: src/lib/libc/arm/gen/_ctx_start.S,v 1.2.20.1 2009/04/15 03:14:26 kensmith Exp $"
ENTRY(_ctx_start)
	mov	lr, pc
	mov	pc, r4
	mov	r0, r5
	bl	_C_LABEL(ctx_done)
	bl	_C_LABEL(abort)
