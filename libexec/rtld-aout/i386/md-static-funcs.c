/*
 *	$Id: md-static-funcs.c,v 1.2 1994/02/13 20:42:06 jkh Exp $
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

