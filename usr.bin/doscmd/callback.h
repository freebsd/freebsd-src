/* 
** No copyright?!
**
** $FreeBSD: src/usr.bin/doscmd/callback.h,v 1.2 1999/08/28 01:00:05 peter Exp $
*/
typedef void		(*callback_t)(regcontext_t *REGS);

extern void		register_callback(u_long vec, callback_t func, char *name);
extern callback_t	find_callback(u_long vec);
extern u_long		insert_generic_trampoline(size_t len, u_char *p);
extern u_long		insert_softint_trampoline(void);
extern u_long		insert_hardint_trampoline(void);
extern u_long		insert_null_trampoline(void);
