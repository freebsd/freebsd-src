/* 
** No copyright?!
**
** $FreeBSD: src/usr.bin/doscmd/callback.h,v 1.2.2.1 2002/04/25 11:04:50 tg Exp $
*/
typedef void		(*callback_t)(regcontext_t *);

void		register_callback(u_long, callback_t, const char *);
callback_t	find_callback(u_long);
u_long		insert_generic_trampoline(size_t, u_char *);
u_long		insert_softint_trampoline(void);
u_long		insert_hardint_trampoline(void);
u_long		insert_null_trampoline(void);
