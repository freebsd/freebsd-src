/*
 * 	$Id: isofs_bmap.c,v 1.4 1993/12/19 00:51:03 wollman Exp $
 */

#include "param.h"
#include "systm.h"
#include "namei.h"
#include "buf.h"
#include "file.h"
#include "vnode.h"
#include "mount.h"

#include "iso.h"
#include "isofs_node.h"

int
iso_bmap(ip, lblkno, result)
	struct iso_node *ip;
	int lblkno;
	int *result;
{
	*result = (ip->iso_extent + lblkno)
		* (ip->i_mnt->im_bsize / DEV_BSIZE);
	return (0);
}
