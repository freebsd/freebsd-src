/*
 * $FreeBSD: src/libexec/rtld-aout/i386/md-static-funcs.c,v 1.6.2.1 1999/08/29 15:04:04 peter Exp $
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

