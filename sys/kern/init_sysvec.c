/*
 * sysentvec for native FreeBSD a.out executable format.
 *
 * $Id: init_sysvec.c,v 1.2.2.1 1998/05/13 07:04:52 tg Exp $
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/syscall.h>
#include <sys/signalvar.h>
#include <machine/md_var.h>

struct sysentvec aout_sysvec = {
	SYS_MAXSYSCALL,
	sysent,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	sendsig,
	sigcode,
	&szsigcode,
	0,
	"FreeBSD a.out"
};
