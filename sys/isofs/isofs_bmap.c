/*
 * 	$Id: isofs_bmap.c,v 1.2 1993/07/20 03:27:26 jkh Exp $
 */

#include "param.h"
#include "namei.h"
#include "buf.h"
#include "file.h"
#include "vnode.h"
#include "mount.h"

#include "iso.h"
#include "isofs_node.h"

iso_bmap(ip, lblkno, result)
struct iso_node *ip;
int lblkno;
int *result;
{
	*result = (ip->iso_extent + lblkno)
		* (ip->i_mnt->im_bsize / DEV_BSIZE);
	return (0);
}
