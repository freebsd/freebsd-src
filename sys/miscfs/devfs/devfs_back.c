
/*
 *  Written by Julian Elischer (julian@DIALix.oz.au)
 *
 *	$Header: /pub/FreeBSD/FreeBSD-CVS/src/sys/miscfs/devfs/Attic/devfs_back.c,v 1.3 1995/05/30 08:06:49 rgrimes Exp $
 */

#include "param.h"
#include "systm.h"
#include "types.h"
#include "kernel.h"
#include "file.h"		/* define FWRITE ... */
#include "conf.h"
#include "stat.h"
#include "mount.h"
#include "vnode.h"
#include "malloc.h"
#include "dir.h"		/* defines dirent structure		*/
#include "devfsdefs.h"



devnm_p	dev_root;		/* root of the backing tree */
int	devfs_set_up = 0;	/* note tha we HAVE set up the backing tree */

/*
 * Set up the root directory node in the backing plane
 * This is happenning before the vfs system has been
 * set up yet, so be careful about what we reference..
 * Notice that the ops are by indirection.. as they haven't
 * been set up yet!
 */
void  devfs_back_init() /*proto*/
{

	devnm_p devbp;
	dn_p dnp;
	/*
	 * This may be called several times.. only do it if it needs
	 * to be done.
	 */
	if(!devfs_set_up)
	{
		/*
	 	 * Allocate and fill out a new backing node
	 	 */
		if(!(devbp = (devnm_p)malloc(sizeof(devnm_t),
					M_DEVFSBACK, M_NOWAIT)))
		{
			return ;
		}
		bzero(devbp,sizeof(devnm_t));
		/*
		 * And the devnode associated with it
		 */
		if(!(dnp = (dn_p)malloc(sizeof(devnode_t),
					M_DEVFSNODE, M_NOWAIT)))
		{
			free(devbp,M_DEVFSBACK);
			return ;
		}
		bzero(dnp,sizeof(devnode_t));
		/*
		 * Link the two together
		 */
		devbp->dnp = dnp;
		dnp->links = 1;
		/*
		 * set up the directory node for the root
		 * and put in all the usual entries for a directory node
		 */
		dnp->type = DEV_DIR;
		dnp->links++; /* for .*/
		/* root loops to self */
		dnp->by.Dir.parent = dnp;
		dnp->links++; /* for ..*/
		/*
		 * set up the list of children (none so far)
		 */
		dnp->by.Dir.dirlist = (devnm_p)0;
		dnp->by.Dir.dirlast =
				&dnp->by.Dir.dirlist;
		dnp->by.Dir.myname = devbp;
		/*
		 * set up a pointer to directory type ops
		 */
		dnp->ops = &devfs_vnodeop_p;
		dnp->mode |= 0555;	/* default perms */
		/*
		 * note creation times etc, as now (boot time)
		 */
		TIMEVAL_TO_TIMESPEC(&time,&(dnp->ctime))
		dnp->mtime = dnp->ctime;
		dnp->atime = dnp->ctime;

		/*
		 * and the list of layers
		 */
		devbp->next_front = NULL;
		devbp->prev_frontp = &(devbp->next_front);


		/*
		 * next time, we don't need to do all this
		 */
		dev_root = devbp;
		devfs_set_up = 1;
	}
}

/***********************************************************************\
* Given a starting node (0 for root) and a pathname, return the node	*
* for the end item on the path. It MUST BE A DIRECTORY. If the 'CREATE'	*
* option is true, then create any missing nodes in the path and create	*
* and return the final node as well.					*
* Generally, this MUST be the first function called by any module	*
* as it also calls the initial setup code, in case it has never been	*
* done yet.								*
* This is used to set up a directory, before making nodes in it..	*
*									*
* Warning: This function is RECURSIVE.					*
*	char	*path,		 find this dir (err if not dir)		*
*	dn_p	dirnode,	 starting point  (0 = root)	 	*
*	int	create,		 create path if not found 		*
*	dn_p	*dn_pp)		 where to return the node of the dir	*
\***********************************************************************/
int	dev_finddir(char *orig_path, dn_p dirnode, int create, dn_p *dn_pp) /*proto*/
{
	devnm_p	devbp;
	char	pathbuf[DEVMAXPATHSIZE];
	char	*path;
	char	*name;
	register char *cp;
	int	retval;


	DBPRINT(("dev_finddir\n"));
	devfs_back_init();		/* in case we are the first */
	if(!dirnode) dirnode = dev_root->dnp;
	if(dirnode->type != DEV_DIR) return ENOTDIR;
	if(strlen(orig_path) > (DEVMAXPATHSIZE - 1)) return ENAMETOOLONG;
	path = pathbuf;
	strcpy(path,orig_path);
	while(*path == '/') path++;	/* always absolute, skip leading / */
	/***************************************\
	* find the next segment of the name	*
	\***************************************/
	cp = name = path;
	while((*cp != '/') && (*cp != 0))
	{
		cp++;
	}
	/***********************************************\
	* Check to see if it's the last component	*
	\***********************************************/
	if(*cp)
	{
		path = cp + 1;	/* path refers to the rest */
		*cp = 0; 	/* name is now a separate string */
		if(!(*path))
		{
			path = (char *)0; /* was trailing slash */
		}
	}
	else
	{
		path = (char *)0;	/* no more to do */
	}

	/***************************************\
	* Start scanning along the linked list	*
	\***************************************/
	devbp = dirnode->by.Dir.dirlist;
	while(devbp && strcmp(devbp->name,name))
	{
		devbp = devbp->next;
	}
	if(devbp)
	{	/* check it's a directory */
		if(devbp->dnp->type != DEV_DIR) return ENOTDIR;
	}
	else
	{
		/***************************************\
		* The required element does not exist	*
		* So we will add it if asked to.	*
		\***************************************/
		if(!create) return ENOENT;

		if(retval = dev_add_node(name, dirnode ,DEV_DIR,
					NULL, &devbp))
		{
			return retval;
		}
	}
	if(path)	/* decide whether to recurse more or return */
	{
		return (dev_finddir(path,devbp->dnp,create,dn_pp));
	}
	else
	{
		*dn_pp = devbp->dnp;
		return 0;
	}
}

/***********************************************************************\
* Add a new element to the devfs backing structure. 			*
\***********************************************************************/
int	dev_add_node(char *name, dn_p dirnode, int entrytype, union typeinfo *by, devnm_p *devnm_pp) /*proto*/
{
	devnm_p devbp;
	devnm_p realthing;	/* needed to create an alias */
	dn_p	dnp;
	int	retval;

	DBPRINT(("dev_add_node\n"));
	if(dirnode->type != DEV_DIR) return(ENOTDIR);
	if(strlen(name) > (DEVMAXNAMESIZE - 1)) return (ENAMETOOLONG);

	retval = dev_finddir(name,dirnode,0,&dnp); /*don't create!*/
	dnp = NULL; /*just want the return code..*/
	if(retval != ENOENT) /* only acceptable answer */
		return(EEXIST);
	/*
	 * Allocate and fill out a new backing node
	 */
	if(!(devbp = (devnm_p)malloc(sizeof(devnm_t),
				M_DEVFSBACK, M_NOWAIT)))
	{
		return ENOMEM;
	}
	bzero(devbp,sizeof(devnm_t));
	if(!(dnp = (dn_p)malloc(sizeof(devnode_t),
				M_DEVFSNODE, M_NOWAIT)))
	{
		free(devbp,M_DEVFSBACK);
		return ENOMEM;
	}
	bzero(dnp,sizeof(devnode_t));
	devbp->dnp = dnp;
	dnp->links = 1; /* implicit from our own name-node */

	/*
	 * note the node type we are adding
	 * and set the creation times to NOW
	 * put in it's name
	 * include the implicit link in the count of links to the devnode..
	 * this stops it from being accidentally freed later.
	 */
	strcpy(devbp->name,name);
	dnp->type = entrytype;
	TIMEVAL_TO_TIMESPEC(&time,&(dnp->ctime))
	dnp->mtime = dnp->ctime;
	dnp->atime = dnp->ctime;

	/*
	 * And set up a new 'clones' list (empty)
	 */
	devbp->prev_frontp = &(devbp->next_front);

	/*
	 * Put it on the END of the linked list of directory entries
	 */
	devbp->parent = dirnode;
	devbp->prevp = dirnode->by.Dir.dirlast;
	devbp->next = *(devbp->prevp); /* should be NULL */ /*right?*/
	*(devbp->prevp) = devbp;
	dirnode->by.Dir.dirlast = &(devbp->next);
	dirnode->by.Dir.entrycount++;

	/*
	 * return the answer
	 */
	switch(entrytype) {
	case DEV_DIR:
		/*
		 * As it's a directory, make sure it has a null entries list
		 */
		dnp->by.Dir.dirlast =
				&(dnp->by.Dir.dirlist);
		dnp->by.Dir.dirlist = (devnm_p)0;
		dnp->by.Dir.parent = (dn_p)dirnode;
		dnp->by.Dir.myname = devbp;
		/*
		 * make sure that the ops associated with it are the ops
		 * that we use (by default) for directories
		 */
		dnp->ops = &devfs_vnodeop_p;
		dnp->mode |= 0555;	/* default perms */
		break;
	case DEV_BDEV:
		/*
		 * Make sure it has DEVICE type ops
		 * and device specific fields are correct
		 */
		dnp->ops = &dev_spec_vnodeop_p;
		dnp->by.Bdev.bdevsw = by->Bdev.bdevsw;
		dnp->by.Bdev.dev = by->Bdev.dev;
		break;
	case DEV_CDEV:
		/*
		 * Make sure it has DEVICE type ops
		 * and device specific fields are correct
		 */
		dnp->ops = &dev_spec_vnodeop_p;
		dnp->by.Cdev.cdevsw = by->Cdev.cdevsw;
		dnp->by.Cdev.dev = by->Cdev.dev;
		break;
	case DEV_DDEV:
		/*
		 * store the address of (the address of) the ops
		 * and the magic cookie to use with them
		 */
		dnp->by.Ddev.arg = by->Ddev.arg;
		dnp->ops = by->Ddev.ops;
		break;


	case DEV_ALIAS:
		/*
		 * point to the node we want to shadow
		 * Also store the fact we exist so that aliases
		 * can be deleted accuratly when the original node
		 * is deleted.. (i.e. when device is removed)
		 */
		realthing = by->Alias.realthing;
		dnp->by.Alias.realthing = realthing;
		dnp->by.Alias.next = realthing->as.back.aliases;
		realthing->as.back.aliases = devbp;
		realthing->as.back.alias_count++;
		break;
	}

	if(retval = devfs_add_fronts(dirnode->by.Dir.myname/*XXX*/,devbp))
        {
		/*XXX*//* no idea what to do if it fails... */
		return retval;
	}

	*devnm_pp = devbp;
	return 0 ;
}

/***********************************************************************
 * remove all fronts to this dev and also it's aliases,
 * Then remove this node.
 * For now only allow DEVICE nodes to go.. XXX
 * directory nodes are more complicated and may need more work..
 */
int	dev_remove(devnm_p devbp) /*proto*/
{
	devnm_p alias;

	DBPRINT(("dev_remove\n"));
	/*
	 * Check the type of the node.. for now don't allow dirs
	 */
	switch(devbp->dnp->type)
	{
	case DEV_BDEV:
	case DEV_CDEV:
	case DEV_DDEV:
	case DEV_ALIAS:
	case DEV_SLNK:
		break;
	case DEV_DIR:
	default:
		return(EINVAL);
	}
	/*
	 * Free each alias
	 */
	while ( devbp->as.back.alias_count)
	{
		alias = devbp->as.back.aliases;
		devbp->as.back.aliases = alias->dnp->by.Alias.next;
		devbp->as.back.alias_count--;
		devfs_dn_free(alias->dnp);
		free (alias, M_DEVFSBACK);
	}
	/*
	 * Now remove front items of the Main node itself
	 */
	devfs_remove_fronts(devbp);

	/*
	 * now we should free the main node
	 */
	devfs_dn_free(devbp->dnp);
	free (devbp, M_DEVFSBACK);
	return 0;
}

int	dev_touch(devnm_p key)		/* update the node for this dev */ /*proto*/
{
	DBPRINT(("dev_touch\n"));
	TIMEVAL_TO_TIMESPEC(&time,&(key->dnp->mtime))
	return 0; /*XXX*/
}

void	devfs_dn_free(dn_p dnp) /*proto*/
{
	if(dnp->links <= 0)
	{
		printf("devfs node reference count bogus\n");
		Debugger("devfs_dn_free");
		return;
	}
	if(--dnp->links == 0 )
	{
	        devfs_dropvnode(dnp);
	        free (dnp, M_DEVFSNODE);
	}
}
/***********************************************************************\
* UTILITY routine:							*
* Return the major number for the cdevsw entry containing the given	*
* address.								*
\***********************************************************************/
int get_cdev_major_num(caddr_t addr)	/*proto*/
{
	int	index = 0;

	DBPRINT(("get_cdev_major_num\n"));
	while (index < nchrdev)
	{
		if(((caddr_t)(cdevsw[index].d_open) == addr)
		 ||((caddr_t)(cdevsw[index].d_read) == addr)
		 ||((caddr_t)(cdevsw[index].d_ioctl) == addr))
		{
			return index;
		}
		index++;
	}
	return -1;
}

int get_bdev_major_num(caddr_t addr)	/*proto*/
{
	int	index = 0;

	DBPRINT(("get_bdev_major_num\n"));
	while (index < nblkdev)
	{
		if(((caddr_t)(bdevsw[index].d_open) == addr)
		 ||((caddr_t)(bdevsw[index].d_strategy) == addr)
		 ||((caddr_t)(bdevsw[index].d_ioctl) == addr))
		{
			return index;
		}
		index++;
	}
	return -1;
}

/***********************************************************************\
* Add the named device entry into the given directory, and make it 	*
* The appropriate type... (called (sometimes indirectly) by drivers..)	*
\***********************************************************************/
devnm_p dev_add(char *path,char *name,caddr_t funct,int minor,int chrblk,uid_t uid,gid_t gid, int perms) /*proto*/
{
	devnm_p	new_dev;
	dn_p	dnp;	/* devnode for parent directory */
	int	retval;
	int major ;
	union	typeinfo by;

	DBPRINT(("dev_add\n"));
	retval = dev_finddir(path,NULL,1,&dnp);
	if (retval) return 0;
	switch(chrblk)
	{
	case	0:
		major = get_cdev_major_num(funct);
		by.Cdev.cdevsw = cdevsw + major;
		by.Cdev.dev = makedev(major, minor);
		if( dev_add_node(name, dnp, DEV_CDEV,
				&by,&new_dev))
			return 0;
		break;
	case	1:
		major = get_bdev_major_num(funct);
		by.Bdev.bdevsw = bdevsw + major;
		by.Bdev.dev = makedev(major, minor);
		if( dev_add_node(name, dnp, DEV_BDEV,
				&by, &new_dev))
			return 0;
		break;
	default:
		return(0);
	}
	new_dev->dnp->gid = gid;
	new_dev->dnp->uid = uid;
	new_dev->dnp->mode |= perms;
	return new_dev;
}



