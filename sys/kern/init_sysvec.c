/*
 * sysentvec for native FreeBSD a.out executable format.
 *
 * $FreeBSD$
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
	sendsig,
	sigcode,
	&szsigcode,
	0,
	"FreeBSD a.out"
};
