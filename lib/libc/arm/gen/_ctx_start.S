#include <machine/asm.h>

.ident	"$FreeBSD$"
ENTRY(_ctx_start)
	mov	pc, r0
	mov	r0, r1
	bl	_C_LABEL(ctx_done)
	bl	_C_LABEL(abort)
