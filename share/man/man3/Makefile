#	@(#)Makefile	8.2 (Berkeley) 12/13/93

MAN3=	assert.0 bitstring.0 end.0 intro.0 queue.0 stdarg.0
MLINKS+=end.3 edata.3 end.3 etext.3 moncontrol.3 monstartup.3
MLINKS+=queue.3 list_entry.3 queue.3 list_head.3 queue.3 list_init.3
MLINKS+=queue.3 list_insert_after.3 queue.3 list_insert_head.3
MLINKS+=queue.3 list_remove.3 queue.3 tailq_entry.3 queue.3 tailq_head.3
MLINKS+=queue.3 tailq_init.3 queue.3 tailq_insert_after.3
MLINKS+=queue.3 tailq_insert_head.3 queue.3 tailq_insert_tail.3
MLINKS+=queue.3 tailq_remove.3 queue.3 circleq_entry.3 queue.3 circleq_head.3
MLINKS+=queue.3 circleq_init.3 queue.3 circleq_insert_after.3
MLINKS+=queue.3 circleq_insert_before.3 queue.3 circleq_insert_head.3
MLINKS+=queue.3 circleq_insert_tail.3 queue.3 circleq_remove.3
MLINKS+=stdarg.3 varargs.3 stdarg.3 va_arg.3 stdarg.3 va_end.3
MLINKS+=stdarg.3 va_start.3

all: ${MAN3}

clean depend lint tags:

cleandir:
	rm -f ${MAN3}

install: maninstall

.include <bsd.prog.mk>
