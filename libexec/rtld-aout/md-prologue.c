/*
 * rtld entry pseudo code - turn into assembler and tweak it
 */

#include <sys/types.h>
#include <sys/types.h>
#include <a.out.h>
#include "link.h"
#include "md.h"

extern long	_GOT_[];
extern void	(*rtld)();
extern void	(*binder())();

void
rtld_entry(version, crtp)
int version;
struct crt *crtp;
{
	register struct link_dynamic	*dp;
	register void			(*f)();

	/* __DYNAMIC is first entry in GOT */
	dp = (struct link_dynamic *) (_GOT_[0]+crtp->crt_ba);

	f = (void (*)())((long)rtld + crtp->crt_ba);
	(*f)(version, crtp, dp);
}

void
binder_entry()
{
	extern int PC;
	struct jmpslot	*sp;
	void	(*func)();

	func = binder(PC, sp->reloc_index & 0x003fffff);
	(*func)();
}
