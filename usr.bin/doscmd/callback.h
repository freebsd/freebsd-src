/* 
** No copyright?!
**
** $FreeBSD$
*/
typedef void		(*callback_t)(regcontext_t *REGS);

extern void		register_callback(u_long vec, callback_t func, char *name);
extern callback_t	find_callback(u_long vec);
extern u_long		insert_generic_trampoline(size_t len, u_char *p);
extern u_long		insert_softint_trampoline(void);
extern u_long		insert_hardint_trampoline(void);
extern u_long		insert_null_trampoline(void);
