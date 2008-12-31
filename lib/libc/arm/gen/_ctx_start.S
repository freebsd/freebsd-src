#include <machine/asm.h>

.ident	"$FreeBSD: src/lib/libc/arm/gen/_ctx_start.S,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $"
ENTRY(_ctx_start)
	mov	lr, pc
	mov	pc, r4
	mov	r0, r5
	bl	_C_LABEL(ctx_done)
	bl	_C_LABEL(abort)
