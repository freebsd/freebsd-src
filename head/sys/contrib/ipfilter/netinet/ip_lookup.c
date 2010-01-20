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
#if (defined(__osf__) || defined(AIX) || defined(__hpux) || defined(__sgi)) && defined(_KERNEL)
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
static const char rcsid[] = "@(#)$Id: ip_lookup.c,v 2.35.2.19 2007/10/11 09:05:51 darrenr Exp $";
#endif

#ifdef	IPFILTER_LOOKUP
int	ip_lookup_inited = 0;

static int iplookup_addnode __P((caddr_t));
static int iplookup_delnode __P((caddr_t data));
static int iplookup_addtable __P((caddr_t));
static int iplookup_deltable __P((caddr_t));
static int iplookup_stats __P((caddr_t));
static int iplookup_flush __P((caddr_t));
static int iplookup_iterate __P((void *, int, void *));
static int iplookup_deltok __P((void *, int, void *));


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
int ip_lookup_ioctl(data, cmd, mode, uid, ctx)
caddr_t data;
ioctlcmd_t cmd;
int mode, uid;
void *ctx;
{
	int err;
	SPL_INT(s);

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

	case SIOCLOOKUPITER :
		err = iplookup_iterate(data, uid, ctx);
		break;

	case SIOCIPFDELTOK :
		err = iplookup_deltok(data, uid, ctx);
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

	err = BCOPYIN(data, &op, sizeof(op));
	if (err != 0)
		return EFAULT;

	if (op.iplo_unit < 0 || op.iplo_unit > IPL_LOGMAX)
		return EINVAL;

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

	err = BCOPYIN(data, &op, sizeof(op));
	if (err != 0)
		return EFAULT;

	if (op.iplo_unit < 0 || op.iplo_unit > IPL_LOGMAX)
		return EINVAL;

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

	err = BCOPYIN(data, &op, sizeof(op));
	if (err != 0)
		return EFAULT;

	if (op.iplo_unit < 0 || op.iplo_unit > IPL_LOGMAX)
		return EINVAL;

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

	/*
	 * For anonymous pools, copy back the operation struct because in the
	 * case of success it will contain the new table's name.
	 */
	if ((err == 0) && ((op.iplo_arg & LOOKUP_ANON) != 0)) {
		err = BCOPYOUT(&op, data, sizeof(op));
		if (err != 0)
			err = EFAULT;
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

	err = BCOPYIN(data, &op, sizeof(op));
	if (err != 0)
		return EFAULT;

	if (op.iplo_unit < 0 || op.iplo_unit > IPL_LOGMAX)
		return EINVAL;

	op.iplo_name[sizeof(op.iplo_name) - 1] = '\0';

	/*
	 * create a new pool - fail if one already exists with
	 * the same #
	 */
	switch (op.iplo_type)
	{
	case IPLT_POOL :
		err = ip_pool_destroy(op.iplo_unit, op.iplo_name);
		break;

	case IPLT_HASH :
		err = fr_removehtable(op.iplo_unit, op.iplo_name);
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

	err = BCOPYIN(data, &op, sizeof(op));
	if (err != 0)
		return EFAULT;

	if (op.iplo_unit < 0 || op.iplo_unit > IPL_LOGMAX)
		return EINVAL;

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

	err = BCOPYIN(data, &flush, sizeof(flush));
	if (err != 0)
		return EFAULT;

	unit = flush.iplf_unit;
	if ((unit < 0 || unit > IPL_LOGMAX) && (unit != IPLT_ALL))
		return EINVAL;

	flush.iplf_name[sizeof(flush.iplf_name) - 1] = '\0';

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
		err = BCOPYOUT(&flush, data, sizeof(flush));
		if (err != 0)
			err = EFAULT;
	}
	return err;
}


/* ------------------------------------------------------------------------ */
/* Function:    ip_lookup_delref                                            */
/* Returns:     void                                                        */
/* Parameters:  type(I) - table type to operate on                          */
/*              ptr(I)  - pointer to object to remove reference for         */
/*                                                                          */
/* This function organises calling the correct deref function for a given   */
/* type of object being passed into it.                                     */
/* ------------------------------------------------------------------------ */
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


/* ------------------------------------------------------------------------ */
/* Function:    iplookup_iterate                                            */
/* Returns:     int     - 0 = success, else error                           */
/* Parameters:  data(I) - pointer to data from ioctl call                   */
/*              uid(I)  - uid of caller                                     */
/*              ctx(I)  - pointer to give the uid context                   */
/*                                                                          */
/* Decodes ioctl request to step through either hash tables or pools.       */
/* ------------------------------------------------------------------------ */
static int iplookup_iterate(data, uid, ctx)
void *data;
int uid;
void *ctx;
{
	ipflookupiter_t iter;
	ipftoken_t *token;
	int err;
	SPL_INT(s);

	err = fr_inobj(data, &iter, IPFOBJ_LOOKUPITER);
	if (err != 0)
		return err;

	if (iter.ili_unit > IPL_LOGMAX)
		return EINVAL;

	if (iter.ili_ival != IPFGENITER_LOOKUP)
		return EINVAL;

	SPL_SCHED(s);
	token = ipf_findtoken(iter.ili_key, uid, ctx);
	if (token == NULL) {
		RWLOCK_EXIT(&ipf_tokens);
		SPL_X(s);
		return ESRCH;
	}

	switch (iter.ili_type)
	{
	case IPLT_POOL :
		err = ip_pool_getnext(token, &iter);
		break;
	case IPLT_HASH :
		err = fr_htable_getnext(token, &iter);
		break;
	default :
		err = EINVAL;
		break;
	}
	RWLOCK_EXIT(&ipf_tokens);
	SPL_X(s);

	return err;
}


/* ------------------------------------------------------------------------ */
/* Function:    iplookup_iterderef                                          */
/* Returns:     int     - 0 = success, else error                           */
/* Parameters:  data(I) - pointer to data from ioctl call                   */
/*                                                                          */
/* Decodes ioctl request to remove a particular hash table or pool and      */
/* calls the relevant function to do the cleanup.                           */
/* ------------------------------------------------------------------------ */
void ip_lookup_iterderef(type, data)
u_32_t type;
void *data;
{
	iplookupiterkey_t	key;

	key.ilik_key = type;

	if (key.ilik_unstr.ilik_ival != IPFGENITER_LOOKUP)
		return;

	switch (key.ilik_unstr.ilik_type)
	{
	case IPLT_HASH :
		fr_htable_iterderef((u_int)key.ilik_unstr.ilik_otype,
				    (int)key.ilik_unstr.ilik_unit, data);
		break;
	case IPLT_POOL :
		ip_pool_iterderef((u_int)key.ilik_unstr.ilik_otype,
				  (int)key.ilik_unstr.ilik_unit, data);
		break;
	}
}


/* ------------------------------------------------------------------------ */
/* Function:    iplookup_deltok                                             */
/* Returns:     int     - 0 = success, else error                           */
/* Parameters:  data(I) - pointer to data from ioctl call                   */
/*              uid(I)  - uid of caller                                     */
/*              ctx(I)  - pointer to give the uid context                   */
/*                                                                          */
/* Deletes the token identified by the combination of (type,uid,ctx)        */
/* "key" is a combination of the table type, iterator type and the unit for */
/* which the token was being used.                                          */
/* ------------------------------------------------------------------------ */
static int iplookup_deltok(data, uid, ctx)
void *data;
int uid;
void *ctx;
{
	int error, key;
	SPL_INT(s);

	SPL_SCHED(s);
	error = BCOPYIN(data, &key, sizeof(key)); 
	if (error == 0)
		error = ipf_deltoken(key, uid, ctx);
	SPL_X(s);
	return error;
}


#else /* IPFILTER_LOOKUP */

/*ARGSUSED*/
int ip_lookup_ioctl(data, cmd, mode, uid, ctx)
caddr_t data;
ioctlcmd_t cmd;
int mode, uid;
void *ctx;
{
	return EIO;
}
#endif /* IPFILTER_LOOKUP */
