
/*
 *  Written by Julian Elischer (julian@DIALix.oz.au)
 *
 *	$Header: /home/ncvs/src/sys/miscfs/devfs/devfs_front.c,v 1.5 1995/09/03 08:39:26 julian Exp $
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
#include "sys/devfsext.h"

SYSINIT(devfs, SI_SUB_DEVFS, SI_ORDER_FIRST, devfs_sinit, NULL)

devnm_p	dev_root;		/* root of the backing tree */
struct mount *devfs_hidden_mount;
int devfs_up_and_going; 

/*
 * Set up the root directory node in the backing plane
 * This is happenning before the vfs system has been
 * set up yet, so be careful about what we reference..
 * Notice that the ops are by indirection.. as they haven't
 * been set up yet!
 */
void  devfs_sinit() /*proto*/
{
	int retval; /* we will discard this */
	devnm_p new;
	/*
	 * call the right routine at the right time with the right args....
	 */
	retval = dev_add_node("root",NULL,NULL,DEV_DIR,NULL,&dev_root);
	devfs_hidden_mount = (struct mount *)malloc(sizeof(struct mount),
							M_MOUNT,M_NOWAIT);
#ifdef PARANOID
	if(!devfs_hidden_mount) panic("devfs-main-mount: malloc failed");
#endif
	bzero(devfs_hidden_mount,sizeof(struct mount));
	devfs_mount(devfs_hidden_mount,"dummy",NULL,NULL,NULL);
	dev_root->dnp->dvm = (struct devfsmount *)devfs_hidden_mount->mnt_data;
	devfs_up_and_going = 1;
	printf("DEVFS: ready for devices\n");
}

/***********************************************************************\
*	Routines used to add and remove nodes from the base tree	*
\***********************************************************************/
/***********************************************************************\
* Given a starting node (0 for root) and a pathname, return the node	*
* for the end item on the path. It MUST BE A DIRECTORY. If the 'CREATE'	*
* option is true, then create any missing nodes in the path and create	*
* and return the final node as well.					*
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
	devnm_p	devnmp;
	dn_p	dnp;
	char	pathbuf[DEVMAXPATHSIZE];
	char	*path;
	char	*name;
	register char *cp;
	int	retval;


	DBPRINT(("dev_finddir\n"));
	/***************************************\
	* If no parent directory is given	*
	* then start at the root of the tree	*
	\***************************************/
	if(!dirnode) dirnode = dev_root->dnp;

	/***************************************\
	* Sanity Checks				*
	\***************************************/
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
	dnp = dev_findname(dirnode,name);
	if(dnp)
	{	/* check it's a directory */
		if(dnp->type != DEV_DIR) return ENOTDIR;
	}
	else
	{
		/***************************************\
		* The required element does not exist	*
		* So we will add it if asked to.	*
		\***************************************/
		if(!create) return ENOENT;

		if(retval = dev_add_node(name, dirnode, NULL ,DEV_DIR,
					NULL, &devnmp))
		{
			return retval;
		}
		dnp = devnmp->dnp;
	}
	if(path)	/* decide whether to recurse more or return */
	{
		return (dev_finddir(path,dnp,create,dn_pp));
	}
	else
	{
		*dn_pp = dnp;
		return 0;
	}
}


/***********************************************************************\
* Add a new element to the devfs backing structure. 			*
* If we're creating a root node, then dirname is NULL			*
* If devnode is non zero, then we just want to create a link to it	*
* This implies that we are not at base level and it's a not a DIR	*
\***********************************************************************/
int	dev_add_node(char *name, dn_p dirnode,devnm_p back, int entrytype, union typeinfo *by, devnm_p *devnm_pp) /*proto*/
{
	devnm_p devnmp;
	devnm_p realthing;	/* needed to create an alias */
	dn_p	dnp;
	int	retval;

	DBPRINT(("dev_add_node\n"));
	if(dirnode ) {
		if(dirnode->type != DEV_DIR) return(ENOTDIR);
	
		dnp = dev_findname(dirnode,name);
		if(dnp) /* if we actually found it.. */
			return(EEXIST);
		dnp = NULL; /*just want the return code..*/
	}
	/*
	 * make sure the name is legal
	 * slightly misleading in the case of NULL
	 */
	if( !name || (strlen(name) > (DEVMAXNAMESIZE - 1)))
			return (ENAMETOOLONG);
	/*
	 * Allocate and fill out a new directory entry 
	 */
	if(!(devnmp = (devnm_p)malloc(sizeof(devnm_t),
				M_DEVFSNAME, M_NOWAIT)))
	{
		return ENOMEM;
	}
	bzero(devnmp,sizeof(devnm_t));

	/*
	 * If we don't already have one,
	 * Allocate a devfs node..
	 * If we already have one, create a new link to it.
	 * DIR types ALWAYS get a new node..
	 */
	if (( entrytype == DEV_DIR) || (!back)) {
		if(!(dnp = (dn_p)malloc(sizeof(devnode_t),
				M_DEVFSNODE, M_NOWAIT)))
		{
			free(devnmp,M_DEVFSNAME);
			return ENOMEM;
		}
		bzero(dnp,sizeof(devnode_t));
		/*
		 * note the node type we are adding
		 * and set the creation times to NOW
		 * put in it's name
		 */
		dnp->type = entrytype;
		TIMEVAL_TO_TIMESPEC(&time,&(dnp->ctime))
		dnp->mtime = dnp->ctime;
		dnp->atime = dnp->ctime;
		/*
		 * fill out the dev node according to type
		 */
		switch(entrytype) {
		case DEV_DIR:
			/*
			 * As it's a directory, make sure
			 * it has a null entries list
			 */
			dnp->by.Dir.dirlast = &(dnp->by.Dir.dirlist);
			dnp->by.Dir.dirlist = (devnm_p)0;
			if ( dirnode ) { 
				dnp->by.Dir.parent = (dn_p)dirnode;
			} else {
				/* root loops to self */
				dnp->by.Dir.parent = dnp;
			}
			dnp->by.Dir.parent->links++; /* account for .. */
			dnp->links++; /* for .*/
			dnp->by.Dir.myname = devnmp;
			/*
			 * make sure that the ops associated with it are the ops
			 * that we use (by default) for directories
			 */
			dnp->ops = &devfs_vnodeop_p;
			dnp->mode |= 0555;	/* default perms */
			break;
		/*******************************************************\
		* The rest of these can't happen except in the back plane*
		\*******************************************************/
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
			realthing->as.back.aliases = devnmp;
			break;
		}
		/* inherrit our parent's mount info */
		if(dirnode) {
			dnp->dvm = dirnode->dvm;
		}
	} else {
		dnp = back->dnp;
	}

	/*
	 * Link the two together
	 * include the implicit link in the count of links to the devnode..
	 * this stops it from being accidentally freed later.
	 */
	devnmp->dnp = dnp;
	dnp->links++ ; /* implicit from our own name-node */

	/*
	 * put in it's name
	 */
	strcpy(devnmp->name,name);


	/*
	 * And set up the 'clones' list (empty if new node)
	 */
	if(back) {
		/*******************************************************\
		* Put it in the appropriate back/front list too.	*
		\*******************************************************/
		devnmp->next_front = *back->prev_frontp;
		devnmp->prev_frontp = back->prev_frontp;
		*back->prev_frontp = devnmp;
		back->prev_frontp = &(devnmp->next_front);
		devnmp->as.front.realthing = back;
		
	} else {
		devnmp->prev_frontp = &(devnmp->next_front);
		devnmp->next_front = NULL;
	}


	/*******************************************
	 * Check if we are not making a root node..
	 * (i.e. have parent)
	 */
	if(dirnode) {
		/*
	 	 * Put it on the END of the linked list of directory entries
	 	 */
		devnmp->parent = dirnode; /* null for root */
		devnmp->prevp = dirnode->by.Dir.dirlast;
		devnmp->next = *(devnmp->prevp); /* should be NULL */ /*right?*/
		*(devnmp->prevp) = devnmp;
		dirnode->by.Dir.dirlast = &(devnmp->next);
		dirnode->by.Dir.entrycount++;
		dirnode->len += strlen(name) + 8;/*ok, ok?*/
	}

	/*
	 * If we have a parent, then maybe we should duplicate
	 * ourselves onto any plane that the parent is on...
	 * Though this may be better handled elsewhere as
	 * it stops this routine from being used for front nodes
	 */
	if(dirnode && !back) {
		if(retval = devfs_add_fronts(dirnode->by.Dir.myname,devnmp))
        	{
			/*XXX*//* no idea what to do if it fails... */
			return retval;
		}
	}

	*devnm_pp = devnmp;
	return 0 ;
}

/***********************************************************************
 * remove all fronts to this dev and also it's aliases,
 * Then remove this node.
 * For now only allow DEVICE nodes to go.. XXX
 * directory nodes are more complicated and may need more work..
 ***********************************************************************/
int	dev_remove(devnm_p devnmp) /*proto*/
{
	devnm_p alias;

	DBPRINT(("dev_remove\n"));
	/*
	 * Check the type of the node.. for now don't allow dirs
	 */
	switch(devnmp->dnp->type)
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
	 * Remember, aliases can't have front nodes
	 */
	while ( alias = devnmp->as.back.aliases)
	{
		devnmp->as.back.aliases = alias->dnp->by.Alias.next;
		devfs_dn_free(alias->dnp);
		free (alias, M_DEVFSNAME);
	}
	/*
	 * Now remove front items of the Main node itself
	 */
	devfs_remove_fronts(devnmp);

	/*
	 * now we should free the main node
	 */
	devfs_dn_free(devnmp->dnp);
	free (devnmp, M_DEVFSNAME);
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
void *dev_add(char *path,
		char *name,
		void *funct,
		int minor,
		int chrblk,
		uid_t uid,
		gid_t gid,
		int perms)
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
	case	DV_CHR:
		major = get_cdev_major_num(funct);
		by.Cdev.cdevsw = cdevsw + major;
		by.Cdev.dev = makedev(major, minor);
		if( dev_add_node(name, dnp, NULL, DEV_CDEV,
				&by,&new_dev))
			return 0;
		break;
	case	DV_BLK:
		major = get_bdev_major_num(funct);
		by.Bdev.bdevsw = bdevsw + major;
		by.Bdev.dev = makedev(major, minor);
		if( dev_add_node(name, dnp, NULL, DEV_BDEV,
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


/***********************************************************************\
*	Front Node Operations						* 
\***********************************************************************/

/***********************************************************************\
* Given a directory backing node, and a child backing node, add the	*
* appropriate front nodes to the front nodes of the directory to	*
* represent the child node to the user					*
*									*
* on failure, front nodes will either be correct or not exist for each	*
* front dir, however dirs completed will not be stripped of completed	*
* frontnodes on failure of a later frontnode				*
*									*
\***********************************************************************/
int devfs_add_fronts(devnm_p parent,devnm_p child) /*proto*/
{
	devnm_p newnmp;
	devnm_p  falias;
	int type = child->dnp->type;

	DBPRINT(("	devfs_add_fronts\n"));
	/***********************************************\
	* Find the frontnodes of the parent node	*
	\***********************************************/
	for (falias = parent->next_front; falias; falias = falias->next_front)
	{
		if(dev_findname(falias->dnp,child->name))
		{
			printf("Device %s not created, already exists\n",
				child->name);
			continue;
		}
		if (dev_add_node(child->name,parent->dnp,child,
					type,NULL,&newnmp))
		{
			printf("Device %s: allocation failed\n",
				child->name);
			continue;
		}

	}
	return(0);	/* for now always succeed */
}

/***************************************************************\
* Search down the linked list off a dir to find "name"		*
* return the dn_p for that node.
\***************************************************************/
dn_p dev_findname(dn_p dir,char *name) /*proto*/
{
	devnm_p newfp;
	DBPRINT(("	dev_findname(%s)\n",name));
	if(dir->type != DEV_DIR) return 0;/*XXX*/ /* printf?*/

	if(name[0] == '.')
	{
		if(name[1] == 0)
		{
			return dir;
		}
		if((name[1] == '.') && (name[2] == 0))
		{
			return dir->by.Dir.parent; /* for root, .. == . */
		}
	}
	newfp = dir->by.Dir.dirlist;
	while(newfp)
	{
		if(!(strcmp(name,newfp->name)))
			return newfp->dnp;
		newfp = newfp->next;
	}
	return (dn_p)0;
}

/***************************************************************\
* Create and link in a new front element.. 			*
* Parent can be 0 for a root node				*
* Not presently usable to make a symlink XXX			*
* Must teach this to handle where there is no back node		*
* maybe split into two bits?					*
\***************************************************************/
int dev_mk_front(dn_p parent, devnm_p back, devnm_p *dnm_pp, struct devfsmount *dvm) /*proto*/
{
	devnm_p	newnmp;
	struct	devfsmount *dmt;
	devnm_p	newback;
	devnm_p	newfront;
	int	error;
	dn_p	dnp = back->dnp;

	DBPRINT(("	dev_mk_front\n"));
	/*
	 * go get the node made
	 */
	error = dev_add_node(back->name,parent,back,dnp->type,NULL,&newnmp);
	if ( error ) return error;

	/*
	 * If we have just made the root, then insert the pointer to the
	 * mount information
	 */
	if(dvm) {
		newnmp->dnp->dvm = dvm;
	}

	/*
	 * If it is a directory, then recurse down all the other
	 * subnodes in it....
	 * note that this time we don't pass on the mount info..
	 */
	if ( newnmp->dnp->type == DEV_DIR)
	{
		for(newback = back->dnp->by.Dir.dirlist;
				newback; newback = newback->next)
		{
			if(error = dev_mk_front(newnmp->dnp,
						newback, &newfront, NULL))
			{
				break; /* back out with an error */
			}
		}
	}
	*dnm_pp = newnmp;
	return error;
}

/*
 * duplicate the backing tree into a tree of nodes hung off the
 * mount point given as the argument. Do this by
 * calling dev_mk_front() which recurses all the way
 * up the tree..
 */
int devfs_make_plane(struct devfsmount *devfs_mp_p) /*proto*/
{
	devnm_p	new;
	int	error = 0;

	DBPRINT(("	devfs_make_plane\n"));
	if(devfs_up_and_going) {
		if(error = dev_mk_front(NULL, dev_root, &new, devfs_mp_p)) {
			return error;
		}
	} else { /* we are doing the dummy mount during initialisation.. */
		new = dev_root;
	}
	devfs_mp_p->plane_root = new;

	return error;
}

void  devfs_free_plane(struct devfsmount *devfs_mp_p) /*proto*/
{
	devnm_p devnmp;

	DBPRINT(("	devfs_free_plane\n"));
	devnmp = devfs_mp_p->plane_root;
	if(devnmp) dev_free_name(devnmp);
	devfs_mp_p->plane_root = NULL;
}
/*
 * Remove all the front nodes associated with a backing node
 */
void devfs_remove_fronts(devnm_p devnmp) /*proto*/
{
	while(devnmp->next_front)
	{
		dev_free_name(devnmp->next_front);
	}
}

/***************************************************************\
* Free a front node (and any below it of it's a directory node)	*
\***************************************************************/
void dev_free_name(devnm_p devnmp) /*proto*/
{
	dn_p	parent = devnmp->parent;
	devnm_p	back;

	DBPRINT(("	dev_free_name\n"));
	if(devnmp->dnp->type == DEV_DIR)
	{
		while(devnmp->dnp->by.Dir.dirlist)
		{
			dev_free_name(devnmp->dnp->by.Dir.dirlist);
		}
		/*
		 * drop the reference counts on our and our parent's
		 * nodes for "." and ".." (root has ".." -> "." )
		 */
		devfs_dn_free(devnmp->dnp);	/* account for '.' */
		devfs_dn_free(devnmp->dnp->by.Dir.parent); /* and '..' */
		/* should only have one reference left (from name element) */
	}
	/*
	 * unlink ourselves from the directory on this plane
	 */
	if(parent) /* if not fs root */
	{
		if( *devnmp->prevp = devnmp->next)/* yes, assign */
		{
			devnmp->next->prevp = devnmp->prevp;
		}
		else
		{
			parent->by.Dir.dirlast
				= devnmp->prevp;
		}
		parent->by.Dir.entrycount--;
		parent->len -= strlen(devnmp->name) + 8;
	}

	/*
	 * If the node has a backing pointer we need to free ourselves
	 * from that..
	 * Remember that we may not HAVE a backing node.
	 */
	if (back = devnmp->as.front.realthing) /* yes an assign */
	{
		if( *devnmp->prev_frontp = devnmp->next_front)/* yes, assign */
		{
			devnmp->next_front->prev_frontp = devnmp->prev_frontp;
		}
		else
		{
			back->prev_frontp = devnmp->prev_frontp;
		}
	}
	/***************************************************************\
	* If the front node has it's own devnode structure,		*
	* then free it.							*
	\***************************************************************/
	devfs_dn_free(devnmp->dnp);
	free(devnmp,M_DEVFSNAME);
	return;
}

/*******************************************************\
* Theoretically this could be called for any kind of 	*
* vnode, however in practice it must be a DEVFS vnode	*
\*******************************************************/
int devfs_vntodn(struct vnode *vn_p, dn_p *dn_pp) /*proto*/
{

DBPRINT(("	vntodn "));
	if(vn_p->v_tag != VT_DEVFS)
	{
		printf("bad-tag ");
		Debugger("bad-tag ");
		return(EINVAL);
	}
	if(vn_p->v_usecount == 0)
	{
		printf("not locked! ");
	}
	if((vn_p->v_type == VBAD) || (vn_p->v_type == VNON))
	{
		printf("bad-type ");
		return(EINVAL);
	}
	*dn_pp = (dn_p)vn_p->v_data;

	return(0);
}

/***************************************************************\
* given a dev_node, find the appropriate vnode if one is already*
* associated, or get a new one an associate it with the dev_node*
* need to check about vnode references.. should we increment it?*
\***************************************************************/
int devfs_dntovn(dn_p dnp, struct vnode **vn_pp) /*proto*/
{
	struct vnode *vn_p, *nvp;
	int error = 0;

	vn_p = dnp->vn;
DBPRINT(("dntovn "));
	if( vn_p)
	{
		if(vn_p->v_id != dnp->vn_id)
		{
			printf("bad-id ");
			goto skip;
		}
		if(vn_p->v_tag != VT_DEVFS)
		{
			printf("bad-tag ");
			goto skip;
		}
		if(vn_p->v_op != *(dnp->ops))
		{
			printf("bad-ops ");
			goto skip;
		}
		if((dn_p)(vn_p->v_data) != dnp)
		{
			printf("bad-rev_link ");
			goto skip;
		}
		if(vn_p->v_type != VNON)
		{
			vget(vn_p,0/*lockflag ?*/); /*XXX*/
			*vn_pp = vn_p;
			return(0);
		}
		else
		{
			printf("bad-type");
		}
skip:
		vn_p = (struct vnode *) 0;
	}
	if(!(error = getnewvnode(VT_DEVFS,
			dnp->dvm->mount,
			*(dnp->ops),
			&vn_p)))
	{
		dnp->vn = vn_p;
		dnp->vn_id = vn_p->v_id;
		*vn_pp = vn_p;
DBPRINT(("(New vnode)"));
		switch(dnp->type)
		{
		case	DEV_SLNK:
			break;
		case	DEV_DIR:
			if(dnp->by.Dir.parent == dnp)
			{
				vn_p->v_flag |= VROOT;
			}
			vn_p->v_type = VDIR;
			break;
		case	DEV_BDEV:
			vn_p->v_type = VBLK;
			if (nvp = checkalias(vn_p,
			   dnp->by.Bdev.dev,
			  (struct mount *)0))
			{
				vput(vn_p);
				vn_p = nvp;
			}
			break;
		case	DEV_CDEV:
			vn_p->v_type = VCHR;
			if (nvp = checkalias(vn_p,
			   dnp->by.Cdev.dev,
			  (struct mount *)0))
			{
				vput(vn_p);
				vn_p = nvp;
			}
			break;
		case	DEV_DDEV:
			break;
		}
		if ( vn_p)
		{
			vn_p->v_mount  = dnp->dvm->mount;/* XXX Duplicated */
			*vn_pp = vn_p;
			vn_p->v_data = (void *)dnp;
		}
		else
		{
			error = EINVAL;
		}
	}
	return error;
}
