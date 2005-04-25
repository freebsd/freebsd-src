/*	$FreeBSD$	*/

/*
 * Copyright (C) 2002-2003 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#if defined(KERNEL) || defined(_KERNEL)
# undef KERNEL
# undef _KERNEL
# define        KERNEL	1
# define        _KERNEL	1
#endif
#if defined(__osf__)
# define _PROTO_NET_H_
#endif
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/file.h>
#if __FreeBSD_version >= 220000 && defined(_KERNEL)
# include <sys/fcntl.h>
# include <sys/filio.h>
#else
# include <sys/ioctl.h>
#endif
#if !defined(_KERNEL)
# include <string.h>
# define _KERNEL
# ifdef __OpenBSD__
struct file;
# endif
# include <sys/uio.h>
# undef _KERNEL
#endif
#include <sys/socket.h>
#if (defined(__osf__) || defined(__hpux) || defined(__sgi)) && defined(_KERNEL)
# ifdef __osf__
#  include <net/radix.h>
# endif
# include "radix_ipf_local.h"
# define _RADIX_H_
#endif
#include <net/if.h>
#if defined(__FreeBSD__)
#  include <sys/cdefs.h>
#  include <sys/proc.h>
#endif
#if defined(_KERNEL)
# include <sys/systm.h>
# if !defined(__SVR4) && !defined(__svr4__)
#  include <sys/mbuf.h>
# endif
#endif
#include <netinet/in.h>

#include "netinet/ip_compat.h"
#include "netinet/ip_fil.h"
#include "netinet/ip_pool.h"
#include "netinet/ip_htable.h"
#include "netinet/ip_lookup.h"
/* END OF INCLUDES */

#if !defined(lint)
static const char rcsid[] = "@(#)Id: ip_lookup.c,v 2.35.2.5 2004/07/06 11:16:25 darrenr Exp";
#endif

#ifdef	IPFILTER_LOOKUP
int	ip_lookup_inited = 0;

static int iplookup_addnode __P((caddr_t));
static int iplookup_delnode __P((caddr_t data));
static int iplookup_addtable __P((caddr_t));
static int iplookup_deltable __P((caddr_t));
static int iplookup_stats __P((caddr_t));
static int iplookup_flush __P((caddr_t));


/* ------------------------------------------------------------------------ */
/* Function:    iplookup_init                                               */
/* Returns:     int     - 0 = success, else error                           */
/* Parameters:  Nil                                                         */
/*                                                                          */
/* Initialise all of the subcomponents of the lookup infrstructure.         */
/* ------------------------------------------------------------------------ */
int ip_lookup_init()
{

	if (ip_pool_init() == -1)
		return -1;

	RWLOCK_INIT(&ip_poolrw, "ip pool rwlock");

	ip_lookup_inited = 1;

	return 0;
}


/* ------------------------------------------------------------------------ */
/* Function:    iplookup_unload                                             */
/* Returns:     int     - 0 = success, else error                           */
/* Parameters:  Nil                                                         */
/*                                                                          */
/* Free up all pool related memory that has been allocated whilst IPFilter  */
/* has been running.  Also, do any other deinitialisation required such     */
/* ip_lookup_init() can be called again, safely.                            */
/* ------------------------------------------------------------------------ */
void ip_lookup_unload()
{
	ip_pool_fini();
	fr_htable_unload();

	if (ip_lookup_inited == 1) {
		RW_DESTROY(&ip_poolrw);
		ip_lookup_inited = 0;
	}
}


/* ------------------------------------------------------------------------ */
/* Function:    iplookup_ioctl                                              */
/* Returns:     int      - 0 = success, else error                          */
/* Parameters:  data(IO) - pointer to ioctl data to be copied to/from user  */
/*                         space.                                           */
/*              cmd(I)   - ioctl command number                             */
/*              mode(I)  - file mode bits used with open                    */
/*                                                                          */
/* Handle ioctl commands sent to the ioctl device.  For the most part, this */
/* involves just calling another function to handle the specifics of each   */
/* command.                                                                 */
/* ------------------------------------------------------------------------ */
int ip_lookup_ioctl(data, cmd, mode)
caddr_t data;
ioctlcmd_t cmd;
int mode;
{
	int err;
# if defined(_KERNEL) && !defined(MENTAT) && defined(USE_SPL)
	int s;
# endif

	mode = mode;	/* LINT */

	SPL_NET(s);

	switch (cmd)
	{
	case SIOCLOOKUPADDNODE :
	case SIOCLOOKUPADDNODEW :
		WRITE_ENTER(&ip_poolrw);
		err = iplookup_addnode(data);
		RWLOCK_EXIT(&ip_poolrw);
		break;

	case SIOCLOOKUPDELNODE :
	case SIOCLOOKUPDELNODEW :
		WRITE_ENTER(&ip_poolrw);
		err = iplookup_delnode(data);
		RWLOCK_EXIT(&ip_poolrw);
		break;

	case SIOCLOOKUPADDTABLE :
		WRITE_ENTER(&ip_poolrw);
		err = iplookup_addtable(data);
		RWLOCK_EXIT(&ip_poolrw);
		break;

	case SIOCLOOKUPDELTABLE :
		WRITE_ENTER(&ip_poolrw);
		err = iplookup_deltable(data);
		RWLOCK_EXIT(&ip_poolrw);
		break;

	case SIOCLOOKUPSTAT :
	case SIOCLOOKUPSTATW :
		WRITE_ENTER(&ip_poolrw);
		err = iplookup_stats(data);
		RWLOCK_EXIT(&ip_poolrw);
		break;

	case SIOCLOOKUPFLUSH :
		WRITE_ENTER(&ip_poolrw);
		err = iplookup_flush(data);
		RWLOCK_EXIT(&ip_poolrw);
		break;

	default :
		err = EINVAL;
		break;
	}
	SPL_X(s);
	return err;
}


/* ------------------------------------------------------------------------ */
/* Function:    iplookup_addnode                                            */
/* Returns:     int     - 0 = success, else error                           */
/* Parameters:  data(I) - pointer to data from ioctl call                   */
/*                                                                          */
/* Add a new data node to a lookup structure.  First, check to see if the   */
/* parent structure refered to by name exists and if it does, then go on to */
/* add a node to it.                                                        */
/* ------------------------------------------------------------------------ */
static int iplookup_addnode(data)
caddr_t data;
{
	ip_pool_node_t node, *m;
	iplookupop_t op;
	iphtable_t *iph;
	iphtent_t hte;
	ip_pool_t *p;
	int err;

	err = 0;
	BCOPYIN(data, &op, sizeof(op));
	op.iplo_name[sizeof(op.iplo_name) - 1] = '\0';

	switch (op.iplo_type)
	{
	case IPLT_POOL :
		if (op.iplo_size != sizeof(node))
			return EINVAL;

		err = COPYIN(op.iplo_struct, &node, sizeof(node));
		if (err != 0)
			return EFAULT;

		p = ip_pool_find(op.iplo_unit, op.iplo_name);
		if (p == NULL)
			return ESRCH;

		/*
		 * add an entry to a pool - return an error if it already
		 * exists remove an entry from a pool - if it exists
		 * - in both cases, the pool *must* exist!
		 */
		m = ip_pool_findeq(p, &node.ipn_addr, &node.ipn_mask);
		if (m)
			return EEXIST;
		err = ip_pool_insert(p, &node.ipn_addr.adf_addr,
				     &node.ipn_mask.adf_addr, node.ipn_info);
		break;

	case IPLT_HASH :
		if (op.iplo_size != sizeof(hte))
			return EINVAL;

		err = COPYIN(op.iplo_struct, &hte, sizeof(hte));
		if (err != 0)
			return EFAULT;

		iph = fr_findhtable(op.iplo_unit, op.iplo_name);
		if (iph == NULL)
			return ESRCH;
		err = fr_addhtent(iph, &hte);
		break;

	default :
		err = EINVAL;
		break;
	}
	return err;
}


/* ------------------------------------------------------------------------ */
/* Function:    iplookup_delnode                                            */
/* Returns:     int     - 0 = success, else error                           */
/* Parameters:  data(I) - pointer to data from ioctl call                   */
/*                                                                          */
/* Delete a node from a lookup table by first looking for the table it is   */
/* in and then deleting the entry that gets found.                          */
/* ------------------------------------------------------------------------ */
static int iplookup_delnode(data)
caddr_t data;
{
	ip_pool_node_t node, *m;
	iplookupop_t op;
	iphtable_t *iph;
	iphtent_t hte;
	ip_pool_t *p;
	int err;

	err = 0;
	BCOPYIN(data, &op, sizeof(op));

	op.iplo_name[sizeof(op.iplo_name) - 1] = '\0';

	switch (op.iplo_type)
	{
	case IPLT_POOL :
		if (op.iplo_size != sizeof(node))
			return EINVAL;

		err = COPYIN(op.iplo_struct, &node, sizeof(node));
		if (err != 0)
			return EFAULT;

		p = ip_pool_find(op.iplo_unit, op.iplo_name);
		if (!p)
			return ESRCH;

		m = ip_pool_findeq(p, &node.ipn_addr, &node.ipn_mask);
		if (m == NULL)
			return ENOENT;
		err = ip_pool_remove(p, m);
		break;

	case IPLT_HASH :
		if (op.iplo_size != sizeof(hte))
			return EINVAL;

		err = COPYIN(op.iplo_struct, &hte, sizeof(hte));
		if (err != 0)
			return EFAULT;

		iph = fr_findhtable(op.iplo_unit, op.iplo_name);
		if (iph == NULL)
			return ESRCH;
		err = fr_delhtent(iph, &hte);
		break;

	default :
		err = EINVAL;
		break;
	}
	return err;
}


/* ------------------------------------------------------------------------ */
/* Function:    iplookup_addtable                                           */
/* Returns:     int     - 0 = success, else error                           */
/* Parameters:  data(I) - pointer to data from ioctl call                   */
/*                                                                          */
/* Create a new lookup table, if one doesn't already exist using the name   */
/* for this one.                                                            */
/* ------------------------------------------------------------------------ */
static int iplookup_addtable(data)
caddr_t data;
{
	iplookupop_t op;
	int err;

	err = 0;
	BCOPYIN(data, &op, sizeof(op));

	op.iplo_name[sizeof(op.iplo_name) - 1] = '\0';

	switch (op.iplo_type)
	{
	case IPLT_POOL :
		if (ip_pool_find(op.iplo_unit, op.iplo_name) != NULL)
			err = EEXIST;
		else
			err = ip_pool_create(&op);
		break;

	case IPLT_HASH :
		if (fr_findhtable(op.iplo_unit, op.iplo_name) != NULL)
			err = EEXIST;
		else
			err = fr_newhtable(&op);
		break;

	default :
		err = EINVAL;
		break;
	}
	return err;
}


/* ------------------------------------------------------------------------ */
/* Function:    iplookup_deltable                                           */
/* Returns:     int     - 0 = success, else error                           */
/* Parameters:  data(I) - pointer to data from ioctl call                   */
/*                                                                          */
/* Decodes ioctl request to remove a particular hash table or pool and      */
/* calls the relevant function to do the cleanup.                           */
/* ------------------------------------------------------------------------ */
static int iplookup_deltable(data)
caddr_t data;
{
	iplookupop_t op;
	int err;

	BCOPYIN(data, &op, sizeof(op));
	op.iplo_name[sizeof(op.iplo_name) - 1] = '\0';

	if (op.iplo_arg & IPLT_ANON)
		op.iplo_arg &= IPLT_ANON;

	/*
	 * create a new pool - fail if one already exists with
	 * the same #
	 */
	switch (op.iplo_type)
	{
	case IPLT_POOL :
		err = ip_pool_destroy(&op);
		break;

	case IPLT_HASH :
		err = fr_removehtable(&op);
		break;

	default :
		err = EINVAL;
		break;
	}
	return err;
}


/* ------------------------------------------------------------------------ */
/* Function:    iplookup_stats                                              */
/* Returns:     int     - 0 = success, else error                           */
/* Parameters:  data(I) - pointer to data from ioctl call                   */
/*                                                                          */
/* Copy statistical information from inside the kernel back to user space.  */
/* ------------------------------------------------------------------------ */
static int iplookup_stats(data)
caddr_t data;
{
	iplookupop_t op;
	int err;

	err = 0;
	BCOPYIN(data, &op, sizeof(op));

	switch (op.iplo_type)
	{
	case IPLT_POOL :
		err = ip_pool_statistics(&op);
		break;

	case IPLT_HASH :
		err = fr_gethtablestat(&op);
		break;

	default :
		err = EINVAL;
		break;
	}
	return err;
}


/* ------------------------------------------------------------------------ */
/* Function:    iplookup_flush                                              */
/* Returns:     int     - 0 = success, else error                           */
/* Parameters:  data(I) - pointer to data from ioctl call                   */
/*                                                                          */
/* A flush is called when we want to flush all the nodes from a particular  */
/* entry in the hash table/pool or want to remove all groups from those.    */
/* ------------------------------------------------------------------------ */
static int iplookup_flush(data)
caddr_t data;
{
	int err, unit, num, type;
	iplookupflush_t flush;

	err = 0;
	BCOPYIN(data, &flush, sizeof(flush));

	flush.iplf_name[sizeof(flush.iplf_name) - 1] = '\0';

	unit = flush.iplf_unit;
	if ((unit < 0 || unit > IPL_LOGMAX) && (unit != IPLT_ALL))
		return EINVAL;

	type = flush.iplf_type;
	err = EINVAL;
	num = 0;

	if (type == IPLT_POOL || type == IPLT_ALL) {
		err = 0;
		num = ip_pool_flush(&flush);
	}

	if (type == IPLT_HASH  || type == IPLT_ALL) {
		err = 0;
		num += fr_flushhtable(&flush);
	}

	if (err == 0) {
		flush.iplf_count = num;
		err = COPYOUT(&flush, data, sizeof(flush));
	}
	return err;
}


void ip_lookup_deref(type, ptr)
int type;
void *ptr;
{
	if (ptr == NULL)
		return;

	WRITE_ENTER(&ip_poolrw);
	switch (type)
	{
	case IPLT_POOL :
		ip_pool_deref(ptr);
		break;

	case IPLT_HASH :
		fr_derefhtable(ptr);
		break;
	}
	RWLOCK_EXIT(&ip_poolrw);
}


#else /* IPFILTER_LOOKUP */

/*ARGSUSED*/
int ip_lookup_ioctl(data, cmd, mode)
caddr_t data;
ioctlcmd_t cmd;
int mode;
{
	return EIO;
}
#endif /* IPFILTER_LOOKUP */
