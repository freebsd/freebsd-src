/*
 * $FreeBSD: src/libexec/rtld-aout/i386/md-static-funcs.c,v 1.7 1999/08/28 00:10:07 peter Exp $
 *
 * Called by ld.so when onanating.
 * This *must* be a static function, so it is not called through a jmpslot.
 */

static void
md_relocate_simple(r, relocation, addr)
struct relocation_info	*r;
long			relocation;
char			*addr;
{
	if (r->r_relative)
		*(long *)addr += relocation;
}

