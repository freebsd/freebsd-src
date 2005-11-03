#include <machine/asm.h>

.ident	"$FreeBSD: src/lib/libc/arm/gen/_ctx_start.S,v 1.2 2004/11/05 23:53:02 cognet Exp $"
ENTRY(_ctx_start)
	mov	lr, pc
	mov	pc, r4
	mov	r0, r5
	bl	_C_LABEL(ctx_done)
	bl	_C_LABEL(abort)
