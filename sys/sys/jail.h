/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: jail.h,v 1.1 1999/04/28 11:38:03 phk Exp $
 *
 */

#ifndef _SYS_JAIL_H_
#define _SYS_JAIL_H_

struct jail {
	char *path;
	char *hostname;
	u_int32_t ip_number;
};

#ifndef KERNEL

int jail __P((struct jail *));

#else /* KERNEL */

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_PRISON);
#endif

/*
 * This structure describes a prison.  It is pointed to by all struct
 * proc's of the inmates.  pr_ref keeps track of them and is used to
 * delete the struture when the last inmate is dead.
 */

struct prison {
	int		pr_ref;
	char 		pr_host[MAXHOSTNAMELEN];
	u_int32_t	pr_ip;
};

#endif /* !KERNEL */
#endif /* !_SYS_JAIL_H_ */
