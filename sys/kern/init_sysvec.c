/*
 * sysentvec for native FreeBSD a.out executable format.
 *
 * $Id: init_sysvec.c,v 1.2 1996/06/18 05:15:46 dyson Exp $
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
