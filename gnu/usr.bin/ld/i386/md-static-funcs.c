/*
 *	$Id: md-static-funcs.c,v 1.2.8.1 1996/05/02 16:08:53 jdp Exp $
 *
 * Called by ld.so when onanating.
 * This *must* be a static function, so it is not called through a jmpslot.
 */

static inline void
md_relocate_simple(r, relocation, addr)
struct relocation_info	*r;
long			relocation;
char			*addr;
{
	*(long *)addr += relocation;
}

