
/*
 *  Written by Julian Elischer (julian@DIALix.oz.au)
 *
 *	$Header: /home/ncvs/src/sys/miscfs/devfs/devfs_tree.c,v 1.21 1996/04/02 04:53:05 scrappy Exp $
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
void  devfs_sinit(void *junk) /*proto*/
{
	int retval; /* we will discard this */
	devnm_p new;
	/*
	 * call the right routine at the right time with the right args....
	 */
	retval = dev_add_entry("root", NULL, DEV_DIR, NULL, &dev_root);
#ifdef PARANOID
	if(retval) panic("devfs_sinit: dev_add_entry failed ");
#endif
	devfs_hidden_mount = (struct mount *)malloc(sizeof(struct mount),
							M_MOUNT,M_NOWAIT);
#ifdef PARANOID
	if(!devfs_hidden_mount) panic("devfs_sinit: malloc failed");
#endif
	bzero(devfs_hidden_mount,sizeof(struct mount));
	devfs_mount(devfs_hidden_mount,"dummy",NULL,NULL,NULL);
	dev_root->dnp->dvm = (struct devfsmount *)devfs_hidden_mount->mnt_data;
	devfs_up_and_going = 1;
	printf("DEVFS: ready for devices\n");
}

/***********************************************************************\
*************************************************************************
*	Routines used to find our way to a point in the tree		*
*************************************************************************
\***********************************************************************/


/***************************************************************\
* Search down the linked list off a dir to find "name"		*
* return the dn_p for that node.
\***************************************************************/
devnm_p dev_findname(dn_p dir,char *name) /*proto*/
{
	devnm_p newfp;
	DBPRINT(("	dev_findname(%s)\n",name));
	if(dir->type != DEV_DIR) return 0;/*XXX*/ /* printf?*/

	if(name[0] == '.')
	{
		if(name[1] == 0)
		{
			return dir->by.Dir.myname;
		}
		if((name[1] == '.') && (name[2] == 0))
		{
			/* for root, .. == . */
			return dir->by.Dir.parent->by.Dir.myname;
		}
	}
	newfp = dir->by.Dir.dirlist;
	while(newfp)
	{
		if(!(strcmp(name,newfp->name)))
			return newfp;
		newfp = newfp->next;
	}
	return NULL;
}

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
	/***************************************\
	* always absolute, skip leading / 	*
	*  get rid of / or // or /// etc.	*
	\***************************************/
	while(*path == '/') path++;
	/***************************************\
	* If nothing left, then parent was it..	*
	\***************************************/
	if ( *path == '\0' ) {
		*dn_pp = dirnode;
		return 0;
	}

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
	devnmp = dev_findname(dirnode,name);
	if(devnmp)
	{	/* check it's a directory */
		dnp = devnmp->dnp;
		if(dnp->type != DEV_DIR) return ENOTDIR;
	}
	else
	{
		/***************************************\
		* The required element does not exist	*
		* So we will add it if asked to.	*
		\***************************************/
		if(!create) return ENOENT;

		if(retval = dev_add_entry(name, dirnode, DEV_DIR,
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
*									*
* Creates a name node, and links it to the supplied node		*
\***********************************************************************/
int	dev_add_name(char *name, dn_p dirnode, devnm_p back, dn_p dnp, devnm_p *devnm_pp) /*proto*/
{
	devnm_p devnmp;
	devnm_p realthing;	/* needed to create an alias */
	int	retval;

	DBPRINT(("dev_add_name\n"));
	if(dirnode ) {
		if(dirnode->type != DEV_DIR) return(ENOTDIR);
	
		if( dev_findname(dirnode,name))
			return(EEXIST);
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

	/* inherrit our parent's mount info */ /*XXX*/
	/* a kludge but.... */
	if(dirnode && ( dnp->dvm == NULL)) {
		dnp->dvm = dirnode->dvm;
		if(!dnp->dvm) printf("parent had null dvm ");
	}

	/*
	 * Link the two together
	 * include the implicit link in the count of links to the devnode..
	 * this stops it from being accidentally freed later.
	 */
	devnmp->dnp = dnp;
	dnp->links++ ; /* implicit from our own name-node */
	if(dnp->type == DEV_DIR) {
		dnp->by.Dir.myname = devnmp;
		/*
		 * If we are unlinking from an old dir, decrement it's links
		 * as we point our '..' elsewhere
		 */
		if(dnp->by.Dir.parent) {
			dnp->by.Dir.parent->links--;
		}
	 	if(dirnode) {
			dnp->by.Dir.parent = dirnode;
		} else {
			dnp->by.Dir.parent = dnp;
		}
		dnp->by.Dir.parent->links++; /* account for the new '..' */
	}

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


/***********************************************************************\
* Add a new element to the devfs backing structure. 			*
*									*
* Creates a new dev_node to go with it					*
* 'by' gives us info to make our node					*
* note the 'links' count is 0 (except if a dir)				*
* but it is only cleared on a transition				*
* so this is ok till we link it to something				*
\***********************************************************************/
int	dev_add_node(int entrytype, union typeinfo *by, dn_p proto, dn_p *dn_pp) /*proto*/
{
	dn_p	dnp;
	int	retval;

	DBPRINT(("dev_add_node\n"));
	if(!(dnp = (dn_p)malloc(sizeof(devnode_t),
			M_DEVFSNODE, M_NOWAIT)))
	{
		return ENOMEM;
	}
	if(proto) {
		/*  XXX should check that we are NOT copying a device node */
		bcopy(proto, dnp, sizeof(devnode_t));
		/* some things you DON'T copy */
		dnp->links = 0;
		dnp->dvm = NULL;
		dnp->vn = NULL;
		dnp->len = 0;
	} else {
		bzero(dnp,sizeof(devnode_t));
		dnp->type = entrytype;
		TIMEVAL_TO_TIMESPEC(&time,&(dnp->ctime))
		dnp->mtime = dnp->ctime;
		dnp->atime = dnp->ctime;
	}
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
		dnp->by.Dir.entrycount = 0;
		/*  until we know better, it has a null parent pointer*/
		dnp->by.Dir.parent = NULL;
		dnp->links++; /* for .*/
		dnp->by.Dir.myname = NULL;
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
	default:
		return EINVAL;
	}

	*dn_pp = dnp;
	return 0 ;
}


/***************************************************************\
* DEV_NODE reference count manipulations.. when a ref count	*
* reaches 0, the node is to be deleted				*
\***************************************************************/
int	dev_touch(devnm_p key)		/* update the node for this dev */ /*proto*/
{
	DBPRINT(("dev_touch\n"));
	TIMEVAL_TO_TIMESPEC(&time,&(key->dnp->mtime))
	return 0; /*XXX*/
}

void	devfs_dn_free(dn_p dnp) /*proto*/
{
	if(--dnp->links <= 0 ) /* can be -1 for initial free, on error */
	{
		/*probably need to do other cleanups XXX */
	        devfs_dropvnode(dnp);
	        free (dnp, M_DEVFSNODE);
	}
}

/***********************************************************************\
*	Front Node Operations						* 
*	Add or delete a chain of front nodes				*
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
* This allows a new node to be propogated through all mounted planes	*
*									*
\***********************************************************************/
int devfs_add_fronts(devnm_p parent,devnm_p child) /*proto*/
{
	int	error;
	devnm_p newnmp;
	devnm_p  falias;
	dn_p	dnp = child->dnp;
	int type = child->dnp->type;

	DBPRINT(("	devfs_add_fronts\n"));
	/***********************************************\
	* Find the frontnodes of the parent node	*
	\***********************************************/
	for (falias = parent->next_front; falias; falias = falias->next_front)
	{
		/*
		 * If a Dir (XXX symlink too one day)
		 * Make the node, using the original as a prototype)
		 */
		if(type == DEV_DIR) {
			if ( error = dev_add_node(type, NULL, dnp, &dnp))
			{
				printf("Device %s: node allocation failed (E=%d)\n",
					child->name,error);
				continue;
			}
		}
		if ( error = dev_add_name(child->name,falias->dnp
						,child, dnp,&newnmp)) {
			if( type == DEV_DIR) {
				devfs_dn_free(dnp); /* 1->0 */
			}
			printf("Device %s: allocation failed (E=%d)\n",
				child->name,error);
			continue;
		}

	}
	return 0;	/* for now always succeed */
}

/***********************************************************************
 * remove all instances of this devicename [for backing nodes..]
 * note.. if there is another link to the node (non dir nodes only)
 * then the devfs_node will still exist as the ref count will be non-0
 * removing a directory node will remove all sup-nodes on all planes (ZAP)
 *
 * Used by device drivers to remove nodes that are no longer relevant
 * The argument is the 'cookie' they were given when they created the node
 * this function is exported.. see sys/devfsext.h
 ***********************************************************************/
void	devfs_remove_dev(void *devnmp)
{
	DBPRINT(("devfs_remove_dev\n"));
	/*
	 * Keep removing the next front node till no more exist
	 */
	while(((devnm_p)devnmp)->next_front)
	{
		dev_free_name(((devnm_p)devnmp)->next_front);
	}
	/*
	 * then free the main node
	 */
	dev_free_name((devnm_p)devnmp);
	return ;
}

/***************************************************************
 * duplicate the backing tree into a tree of nodes hung off the
 * mount point given as the argument. Do this by
 * calling dev_dup_entry which recurses all the way
 * up the tree..
 * If we are the first plane, just return the base root 
 **************************************************************/
int dev_dup_plane(struct devfsmount *devfs_mp_p) /*proto*/
{
	devnm_p	new;
	int	error = 0;

	DBPRINT(("	dev_dup_plane\n"));
	if(devfs_up_and_going) {
		if(error = dev_dup_entry(NULL, dev_root, &new, devfs_mp_p)) {
			return error;
		}
	} else { /* we are doing the dummy mount during initialisation.. */
		new = dev_root;
	}
	devfs_mp_p->plane_root = new;

	return error;
}



/***************************************************************\
* Free a whole plane
\***************************************************************/
void  devfs_free_plane(struct devfsmount *devfs_mp_p) /*proto*/
{
	devnm_p devnmp;

	DBPRINT(("	devfs_free_plane\n"));
	devnmp = devfs_mp_p->plane_root;
	if(devnmp) dev_free_name(devnmp);
	devfs_mp_p->plane_root = NULL;
}

/***************************************************************\
* Create and link in a new front element.. 			*
* Parent can be 0 for a root node				*
* Not presently usable to make a symlink XXX			*
* recursively will create subnodes corresponding to equivalent	*
* child nodes in the base level					*
\***************************************************************/
int dev_dup_entry(dn_p parent, devnm_p back, devnm_p *dnm_pp, struct devfsmount *dvm) /*proto*/
{
	devnm_p	newnmp;
	struct	devfsmount *dmt;
	devnm_p	newback;
	devnm_p	newfront;
	int	error;
	dn_p	dnp = back->dnp;
	int type = back->dnp->type;

	DBPRINT(("	dev_dup_entry\n"));
	/*
	 * go get the node made (if we need to)
	 * use the back one as a prototype
	 */
	if ( type == DEV_DIR) {
		error = dev_add_node( dnp->type, NULL, dnp, &dnp);
		if(error) {
			printf("dev_dup_entry: node alloc failed\n");
			return error;
		}
	} 
	error = dev_add_name(back->name,parent,back,dnp,&newnmp);
	if ( error ) {
		if ( type == DEV_DIR) {
			devfs_dn_free(dnp); /* 1->0 */
		}
		return error;
	}

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
			if(error = dev_dup_entry(newnmp->dnp,
						newback, &newfront, NULL))
			{
				break; /* back out with an error */
			}
		}
	}
	*dnm_pp = newnmp;
	return error;
}

/***************************************************************\
* Free a name node (and any below it of it's a directory node)	*
* remember that if there are other names pointing to the	*
* dev_node then it may not get freed yet			*
* can handle if there is no dnp 				*
\***************************************************************/
void dev_free_name(devnm_p devnmp) /*proto*/
{
	dn_p	parent = devnmp->parent;
	dn_p	dnp = devnmp->dnp;
	devnm_p	back;

	DBPRINT(("	dev_free_name\n"));
	if(dnp) {
		if(dnp->type == DEV_DIR)
		{
			while(dnp->by.Dir.dirlist)
			{
				dev_free_name(dnp->by.Dir.dirlist);
			}
			/*
			 * drop the reference counts on our and our parent's
			 * nodes for "." and ".." (root has ".." -> "." )
			 */
			devfs_dn_free(dnp);	/* account for '.' */
			devfs_dn_free(dnp->by.Dir.parent); /* '..' */
			/* should only have one reference left
				(from name element) */
		}
		devfs_dn_free(dnp);
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
	free(devnmp,M_DEVFSNAME);
	return;
}

/*******************************************************\
*********************************************************
* ROUTINES to control the connection between devfs	*
* nodes and the system's vnodes				*
*********************************************************
\*******************************************************/

/*******************************************************\
* Theoretically this could be called for any kind of 	*
* vnode, however in practice it must be a DEVFS vnode	*
\*******************************************************/
int devfs_vntodn(struct vnode *vn_p, dn_p *dn_pp) /*proto*/
{

DBPRINT(("	vntodn "));
	if(vn_p->v_tag != VT_DEVFS)
	{
		printf("bad-tag2 ");
		Debugger("bad-tag ");
		return(EINVAL);
	}
#if 0
	/*
	 * XXX: This is actually a "normal" case when vclean calls us without
	 * XXX: incrementing the reference count first.
	 */
	if(vn_p->v_usecount == 0)
	{
		printf("No references! ");
	}
#endif
	if((vn_p->v_type == VBAD) || (vn_p->v_type == VNON))
	{
		printf("bad-type2 ");
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
#if 0
			/* XXX: This is `normal'... */
			printf("bad-id ");
#endif
			goto skip;
		}
		if(vn_p->v_tag != VT_DEVFS)
		{
#if 0
			/* XXX: This is `normal'... */
			printf("bad-tag ");
#endif
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

/***********************************************************************\
* add a whole device, with no prototype.. make name element and node	*
\***********************************************************************/
int dev_add_entry(char *name, dn_p parent, int type, union typeinfo *by, devnm_p *nm_pp) /*proto*/ 
{
	dn_p	dnp;
	int	error = 0;

	DBPRINT(("	devfs_add_entry\n"));
	if (error = dev_add_node(type, by, NULL, &dnp))
	{
		printf("Device %s: base node allocation failed (E=%d)\n",
			name,error);
		return error;
	}
	if ( error = dev_add_name(name ,parent ,NULL, dnp, nm_pp))
	{
		devfs_dn_free(dnp); /* 1->0 for dir, 0->(-1) for other */
		printf("Device %s: name slot allocation failed (E=%d)\n",
			name,error);
		
	}
	return error;
}

/***********************************************************************\
* Add the named device entry into the given directory, and make it 	*
* The appropriate type... (called (sometimes indirectly) by drivers..)	*
* this function is exported.. see sys/devfsext.h			*
* Has the capacity to take  printf type arguments to format the device 	*
* names									*
\***********************************************************************/
void *devfs_add_devswf(
		void *devsw,
		int minor,
		int chrblk,
		uid_t uid,
		gid_t gid,
		int perms,
		char *fmt,
		...)
{
	int	major;
	devnm_p	new_dev;
	dn_p	dnp;	/* devnode for parent directory */
	struct	cdevsw *cd;
	struct	bdevsw *bd;
	int	retval;
	union	typeinfo by;

	va_list ap;
	char *name, *path, buf[256]; /* XXX */
	int i;

	va_start(ap, fmt);
	i = kvprintf(fmt, NULL, (void*)buf, 32, ap);
	va_end(ap);
	buf[i] = '\0';
	name = NULL;

	for(i=strlen(buf); i>0; i--)
		if(buf[i] == '/') {
			name=&buf[i];
			buf[i]=0;
			break;
		}

	if (name) {
		*name++ = '\0';
		path = buf;
	} else {
		name = buf;
		path = "/";
	}

	DBPRINT(("dev_add\n"));
	retval = dev_finddir(path,NULL,1,&dnp);
	if (retval) return 0;
	switch(chrblk)
	{
	case	DV_CHR:
		cd = devsw;
		major = cd->d_maj;
		if ( major == -1 ) return NULL;
		by.Cdev.cdevsw = cd;
		by.Cdev.dev = makedev(major, minor);
		if( dev_add_entry(name, dnp, DEV_CDEV, &by,&new_dev))
			return NULL;
		break;
	case	DV_BLK:
		bd = devsw;
		major = bd->d_maj;
		if ( major == -1 ) return NULL;
		by.Bdev.bdevsw = bd;
		by.Bdev.dev = makedev(major, minor);
		if( dev_add_entry(name, dnp, DEV_BDEV, &by, &new_dev))
			return NULL;
		break;
	default:
		return NULL;
	}
	new_dev->dnp->gid = gid;
	new_dev->dnp->uid = uid;
	new_dev->dnp->mode |= perms;
	return new_dev;
}

/***********************************************************************\
* Add the named device entry into the given directory, and make it 	*
*  a link to the already created device given as an arg..		*
* this function is exported.. see sys/devfsext.h			*
\***********************************************************************/
void *devfs_link(void *original, char *fmt, ...)
{
	devnm_p	new_dev;
	devnm_p	orig = (devnm_p) original;
	dn_p	dirnode;	/* devnode for parent directory */
	int	retval;
	int major ;
	union	typeinfo by;

        va_list ap;
        char *p, buf[256]; /* XXX */
        int i;

        va_start(ap, fmt);
        i = kvprintf(fmt, NULL, (void*)buf, 32, ap);
        va_end(ap);
        buf[i] = '\0';
        p = NULL;

        for(i=strlen(buf); i>0; i--)
                if(buf[i] == '/') {
                        p=&buf[i];
                        buf[i]=0;
                        break;
                }

	DBPRINT(("dev_add\n"));

	/*
	 *  The DEV_CDEV below is not used other than it must NOT be DEV_DIR
	 * the correctness of original shuold be checked..
	 */

        if (p) {
                *p++ = '\0';
		retval = dev_finddir(buf,NULL,1,&dirnode);
		if (retval) return 0;
		if( dev_add_name(p, dirnode, NULL, orig->dnp, &new_dev))
			return NULL;
	} else {
		retval = dev_finddir("/",NULL,1,&dirnode);
		if (retval) return 0;
		if( dev_add_name(buf, dirnode, NULL, orig->dnp, &new_dev))
			return NULL;
	}
	return new_dev;
}

