/*
 *	$Id: md-static-funcs.c,v 1.3 1995/11/02 18:47:55 nate Exp $
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

