
/*
 * Copyright 1997,1998 Julian Elischer.  All rights reserved.
 * julian@freebsd.org
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 *	$Id: devfs_tree.c,v 1.53 1998/04/20 03:57:35 julian Exp $
 */


/* SPLIT_DEVS means each devfs uses a different vnode for the same device */
/* Otherwise the same device always ends up at the same vnode even if  */
/* reached througgh a different devfs instance. The practical difference */
/* is that with the same vnode, chmods and chowns show up on all instances of */
/* a device. (etc) */

#define SPLIT_DEVS 1 /* maybe make this an option */
/*#define SPLIT_DEVS 1*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/devfsext.h>

#include <machine/stdarg.h>

#include <miscfs/devfs/devfsdefs.h>


static MALLOC_DEFINE(M_DEVFSNODE, "DEVFS node", "DEVFS node");
static MALLOC_DEFINE(M_DEVFSNAME, "DEVFS name", "DEVFS name");

devnm_p	dev_root;		/* root of the backing tree */
struct mount *devfs_hidden_mount;
int devfs_up_and_going; 

/*
 * Set up the root directory node in the backing plane
 * This is happenning before the vfs system has been
 * set up yet, so be careful about what we reference..
 * Notice that the ops are by indirection.. as they haven't
 * been set up yet!
 * DEVFS has a hidden mountpoint that is used as the anchor point
 * for the internal 'blueprint' version of the dev filesystem tree.
 */
/*proto*/
void
devfs_sinit(void *junk)
{
	int retval; /* we will discard this */

	/*
	 * call the right routine at the right time with the right args....
	 */
	retval = dev_add_entry("root", NULL, DEV_DIR, NULL, NULL, NULL, &dev_root);
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
	/* part 2 of this is done later */
}
SYSINIT(devfs, SI_SUB_DEVFS, SI_ORDER_FIRST, devfs_sinit, NULL)


/***********************************************************************\
*************************************************************************
*	Routines used to find our way to a point in the tree		*
*************************************************************************
\***********************************************************************/


/***************************************************************\
* Search down the linked list off a dir to find "name"		*
* return the dn_p for that node.
\***************************************************************/
/*proto*/
devnm_p
dev_findname(dn_p dir,char *name)
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
*	char	*orig_path,	 find this dir (err if not dir)		*
*	dn_p	dirnode,	 starting point  (0 = root)	 	*
*	int	create,		 create path if not found 		*
*	dn_p	*dn_pp)		 where to return the node of the dir	*
\***********************************************************************/
/*proto*/
int
dev_finddir(char *orig_path, dn_p dirnode, int create, dn_p *dn_pp)
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
	while((*cp != '/') && (*cp != 0)) {
		cp++;
	}

	/***********************************************\
	* Check to see if it's the last component	*
	\***********************************************/
	if(*cp) {
		path = cp + 1;	/* path refers to the rest */
		*cp = 0; 	/* name is now a separate string */
		if(!(*path)) {
			path = (char *)0; /* was trailing slash */
		}
	} else {
		path = NULL;	/* no more to do */
	}

	/***************************************\
	* Start scanning along the linked list	*
	\***************************************/
	devnmp = dev_findname(dirnode,name);
	if(devnmp) {	/* check it's a directory */
		dnp = devnmp->dnp;
		if(dnp->type != DEV_DIR) return ENOTDIR;
	} else {
		/***************************************\
		* The required element does not exist	*
		* So we will add it if asked to.	*
		\***************************************/
		if(!create) return ENOENT;

		if(retval = dev_add_entry(name, dirnode, DEV_DIR,
					NULL, NULL, NULL, &devnmp)) {
			return retval;
		}
		dnp = devnmp->dnp;
		devfs_propogate(dirnode->by.Dir.myname,devnmp);
	}
	if(path != NULL) {	/* decide whether to recurse more or return */
		return (dev_finddir(path,dnp,create,dn_pp));
	} else {
		*dn_pp = dnp;
		return 0;
	}
}


/***********************************************************************\
* Add a new NAME element to the devfs					*
* If we're creating a root node, then dirname is NULL			*
* Basically this creates a new namespace entry for the device node	*
*									*
* Creates a name node, and links it to the supplied node		*
\***********************************************************************/
/*proto*/
int
dev_add_name(char *name, dn_p dirnode, devnm_p back, dn_p dnp,
	     devnm_p *devnm_pp)
{
	devnm_p devnmp;

	DBPRINT(("dev_add_name\n"));
	if(dirnode != NULL ) {
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
				M_DEVFSNAME, M_NOWAIT))) {
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

	/* 
	 * Make sure that we can find all the links that reference a node
	 * so that we can get them all if we need to zap the node.
	 */
	if(dnp->linklist) {
		devnmp->nextlink = dnp->linklist;
		devnmp->prevlinkp = devnmp->nextlink->prevlinkp;
		devnmp->nextlink->prevlinkp = &(devnmp->nextlink);
		*devnmp->prevlinkp = devnmp;
		dnp->linklist = devnmp;
	} else {
		devnmp->nextlink = devnmp;
		devnmp->prevlinkp = &(devnmp->nextlink);
		dnp->linklist = devnmp;
	}

	/*
	 * If the node is a directory, then we need to handle the 
	 * creation of the .. link.
	 * A NULL dirnode indicates a root node, so point to ourself.
	 */
	if(dnp->type == DEV_DIR) {
		dnp->by.Dir.myname = devnmp;
		/*
		 * If we are unlinking from an old dir, decrement its links
		 * as we point our '..' elsewhere
		 * Note: it's up to the calling code to remove the 
		 * us from the original directory's list
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
	 * put the name into the directory entry.
	 */
	strcpy(devnmp->name, name);


	/*
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

	*devnm_pp = devnmp;
	return 0 ;
}


/***********************************************************************\
* Add a new element to the devfs plane. 				*
*									*
* Creates a new dev_node to go with it if the prototype should not be	*
* reused. (Is a DIR, or we select SPLIT_DEVS at compile time)		*
* 'by' gives us info to make our node if we don't have a prototype.	*
* If 'by is null and proto exists, then the 'by' field of		*
* the proto is used intead in the CREATE case.				*
* note the 'links' count is 0 (except if a dir)				*
* but it is only cleared on a transition				*
* so this is ok till we link it to something				*
* Even in SPLIT_DEVS mode,						*
* if the node already exists on the wanted plane, just return it	*
\***********************************************************************/
/*proto*/
int
dev_add_node(int entrytype, union typeinfo *by, dn_p proto,
	dn_p *dn_pp,struct  devfsmount *dvm)
{
	dn_p	dnp;

	DBPRINT(("dev_add_node\n"));
#if defined SPLIT_DEVS
	/*
	 * If we have a prototype, then check if there is already a sibling
	 * on the mount plane we are looking at, if so, just return it.
	 */
	if (proto) {
		dnp = proto->nextsibling;
		while( dnp != proto) {
			if (dnp->dvm == dvm) {
				*dn_pp = dnp;
				return (0);
			}
			dnp = dnp->nextsibling;
		}
		if (by == NULL)
			by = &(proto->by);
	}
#else	/* SPLIT_DEVS */
	if ( proto ) {
		switch (proto->type) {
			case DEV_BDEV:
			case DEV_CDEV:
			case DEV_DDEV:
				*dn_pp = proto;
				return 0;
		}
	}
#endif	/* SPLIT_DEVS */
	if(!(dnp = (dn_p)malloc(sizeof(devnode_t),
			M_DEVFSNODE, M_NOWAIT)))
	{
		return ENOMEM;
	}

	/*
	 * If we have a proto, that means that we are duplicating some
	 * other device, which can only happen if we are not at the back plane
	 */
	if(proto) {
		bcopy(proto, dnp, sizeof(devnode_t));
		dnp->links = 0;
		dnp->linklist = NULL;
		dnp->vn = NULL;
		dnp->len = 0;
		/* add to END of siblings list */
		dnp->prevsiblingp = proto->prevsiblingp;
		*(dnp->prevsiblingp) = dnp;
		dnp->nextsibling = proto;
		proto->prevsiblingp = &(dnp->nextsibling);
	} else {
		/* 
		 * We have no prototype, so start off with a clean slate
		 */
		bzero(dnp,sizeof(devnode_t));
		dnp->type = entrytype;
		getnanotime(&(dnp->ctime));
		dnp->mtime = dnp->ctime;
		dnp->atime = dnp->ctime;
		dnp->nextsibling = dnp;
		dnp->prevsiblingp = &(dnp->nextsibling);
	}
	dnp->dvm = dvm;

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
	case DEV_SLNK:
		/*
		 * As it's a symlink allocate and store the link info
		 * Symlinks should only ever be created by the user,
		 * so they are not on the back plane and should not be 
		 * propogated forward.. a bit like directories in that way..
		 * A symlink only exists on one plane and has its own
		 * node.. therefore we might be on any random plane.
		 */
		dnp->by.Slnk.name = malloc(by->Slnk.namelen+1,
					M_DEVFSNODE, M_NOWAIT);
		if (!dnp->by.Slnk.name) {
			free(dnp,M_DEVFSNODE);
			return ENOMEM;
		}
		strncpy(dnp->by.Slnk.name,by->Slnk.name,by->Slnk.namelen);
		dnp->by.Slnk.namelen = by->Slnk.namelen;
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
		dnp->by.Ddev.ops = by->Ddev.ops;
		dnp->ops = by->Ddev.ops;
		break;
	default:
		return EINVAL;
	}

	*dn_pp = dnp;
	return 0 ;
}


/*proto*/
int
dev_touch(devnm_p key)		/* update the node for this dev */
{
	DBPRINT(("dev_touch\n"));
	getnanotime(&(key->dnp->mtime));
	return 0; /*XXX*/
}

/*proto*/
void
devfs_dn_free(dn_p dnp)
{
	if(--dnp->links <= 0 ) /* can be -1 for initial free, on error */
	{
		/*probably need to do other cleanups XXX */
		if (dnp->nextsibling != dnp) {
			dn_p* prevp = dnp->prevsiblingp;
			*prevp = dnp->nextsibling;
			dnp->nextsibling->prevsiblingp = prevp;
			
		}
		if(dnp->type == DEV_SLNK) {
			free(dnp->by.Slnk.name,M_DEVFSNODE);
		}
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
/*proto*/
int
devfs_propogate(devnm_p parent,devnm_p child)
{
	int	error;
	devnm_p newnmp;
	dn_p	dnp = child->dnp;
	dn_p	pdnp = parent->dnp;
	dn_p	adnp = parent->dnp;
	int type = child->dnp->type;

	DBPRINT(("	devfs_propogate\n"));
	/***********************************************\
	* Find the other instances of the parent node	*
	\***********************************************/
	for (adnp = pdnp->nextsibling;
		adnp != pdnp;
		adnp = adnp->nextsibling)
	{
		/*
		 * Make the node, using the original as a prototype)
		 * if the node already exists on that plane it won't be
		 * re-made..
		 */
		if ( error = dev_add_entry(child->name, adnp, type,
					NULL, dnp, adnp->dvm, &newnmp)) {
			printf("duplicating %s failed\n",child->name);
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
void
devfs_remove_dev(void *devnmp)
{
	dn_p dnp = ((devnm_p)devnmp)->dnp;
	dn_p dnp2;

	DBPRINT(("devfs_remove_dev\n"));
	/* keep removing the next sibling till only we exist. */
	while((dnp2 = dnp->nextsibling) != dnp) {

		/*
		 * Keep removing the next front node till no more exist
		 */
		dnp->nextsibling = dnp2->nextsibling; 
		dnp->nextsibling->prevsiblingp = &(dnp->nextsibling);
		dnp2->nextsibling = dnp2;
		dnp2->prevsiblingp = &(dnp2->nextsibling);
		while(dnp2->linklist)
		{
			dev_free_name(dnp2->linklist);
		}
	}

	/*
	 * then free the main node
	 * If we are not running in SPLIT_DEVS mode, then
	 * THIS is what gets rid of the propogated nodes.
	 */
	while(dnp->linklist)
	{
		dev_free_name(dnp->linklist);
	}
	return ;
}


/***************************************************************
 * duplicate the backing tree into a tree of nodes hung off the
 * mount point given as the argument. Do this by
 * calling dev_dup_entry which recurses all the way
 * up the tree..
 * If we are the first plane, just return the base root 
 **************************************************************/
/*proto*/
int
dev_dup_plane(struct devfsmount *devfs_mp_p)
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
/*proto*/
void
devfs_free_plane(struct devfsmount *devfs_mp_p)
{
	devnm_p devnmp;

	DBPRINT(("	devfs_free_plane\n"));
	devnmp = devfs_mp_p->plane_root;
	if(devnmp) {
		dev_free_hier(devnmp);
		dev_free_name(devnmp);
	}
	devfs_mp_p->plane_root = NULL;
}

/***************************************************************\
* Create and link in a new front element.. 			*
* Parent can be 0 for a root node				*
* Not presently usable to make a symlink XXX			*
* (Ok, symlinks don't propogate)
* recursively will create subnodes corresponding to equivalent	*
* child nodes in the base level					*
\***************************************************************/
/*proto*/
int
dev_dup_entry(dn_p parent, devnm_p back, devnm_p *dnm_pp,
	      struct devfsmount *dvm)
{
	devnm_p	newnmp;
	devnm_p	newback;
	devnm_p	newfront;
	int	error;
	dn_p	dnp = back->dnp;
	int type = dnp->type;

	DBPRINT(("	dev_dup_entry\n"));
	/*
	 * go get the node made (if we need to)
	 * use the back one as a prototype
	 */
	if ( error = dev_add_entry(back->name, parent, type,
				NULL, dnp,
				parent?parent->dvm:dvm, &newnmp)) {
		printf("duplicating %s failed\n",back->name);
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
	if (type == DEV_DIR)
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
* Free a name node						*
* remember that if there are other names pointing to the	*
* dev_node then it may not get freed yet			*
* can handle if there is no dnp 				*
\***************************************************************/
/*proto*/
int
dev_free_name(devnm_p devnmp)
{
	dn_p	parent = devnmp->parent;
	dn_p	dnp = devnmp->dnp;

	DBPRINT(("	dev_free_name\n"));
	if(dnp) {
		if(dnp->type == DEV_DIR)
		{
			if(dnp->by.Dir.dirlist)
				return (ENOTEMPTY);
			devfs_dn_free(dnp); /* account for '.' */
			devfs_dn_free(dnp->by.Dir.parent); /* '..' */
		}
		/*
		 * unlink us from the list of links for this node
		 * If we are the only link, it's easy!
		 * if we are a DIR of course there should not be any
		 * other links.
	 	 */
		if(devnmp->nextlink == devnmp) {
				dnp->linklist = NULL;
		} else {
			if(dnp->linklist == devnmp) {
				dnp->linklist = devnmp->nextlink;
			}
			devnmp->nextlink->prevlinkp = devnmp->prevlinkp;
			*devnmp->prevlinkp = devnmp->nextlink;
		}
		devfs_dn_free(dnp);
	}

	/*
	 * unlink ourselves from the directory on this plane
	 */
	if(parent) /* if not fs root */
	{
		if( (*devnmp->prevp = devnmp->next) )/* yes, assign */
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

	/***************************************************************\
	* If the front node has its own devnode structure,		*
	* then free it.							*
	\***************************************************************/
	free(devnmp,M_DEVFSNAME);
	return 0;
}

/***************************************************************\
* Free a hierarchy starting at a directory node name 			*
* remember that if there are other names pointing to the	*
* dev_node then it may not get freed yet			*
* can handle if there is no dnp 				*
* leave the node itself allocated.				*
\***************************************************************/
/*proto*/
void
dev_free_hier(devnm_p devnmp)
{
	dn_p	dnp = devnmp->dnp;

	DBPRINT(("	dev_free_hier\n"));
	if(dnp) {
		if(dnp->type == DEV_DIR)
		{
			while(dnp->by.Dir.dirlist)
			{
				dev_free_hier(dnp->by.Dir.dirlist);
				dev_free_name(dnp->by.Dir.dirlist);
			}
		}
	}
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
/*proto*/
int
devfs_vntodn(struct vnode *vn_p, dn_p *dn_pp)
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
	switch(vn_p->v_type) {
	case VBAD:
		printf("bad-type2 (VBAD)");
		return(EINVAL);
#if 0
	case VNON:
		printf("bad-type2 (VNON)");
		return(EINVAL);
#endif
	}
	*dn_pp = (dn_p)vn_p->v_data;

	return(0);
}

/***************************************************************\
* given a dev_node, find the appropriate vnode if one is already*
* associated, or get a new one an associate it with the dev_node*
* need to check about vnode references.. should we increment it?*
\***************************************************************/
/*proto*/
int
devfs_dntovn(dn_p dnp, struct vnode **vn_pp)
{
	struct vnode *vn_p, *nvp;
	int error = 0;
	struct proc *p = curproc;	/* XXX */

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
			vget(vn_p, LK_EXCLUSIVE, p);
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
			vn_p->v_type = VLNK;
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
		vn_lock(vn_p, LK_EXCLUSIVE | LK_RETRY, p);
	}
	return error;
}

/***********************************************************************\
* add a whole device, with no prototype.. make name element and node	*
* Used for adding the original device entries 				*
\***********************************************************************/
/*proto*/
int
dev_add_entry(char *name, dn_p parent, int type, union typeinfo *by,
	      dn_p proto, struct devfsmount *dvm, devnm_p *nm_pp)
{
	dn_p	dnp;
	int	error = 0;

	DBPRINT(("	devfs_add_entry\n"));
	if (error = dev_add_node(type, by, proto, &dnp, 
			(parent?parent->dvm:dvm)))
	{
		printf("Device %s: base node allocation failed (Errno=%d)\n",
			name,error);
		return error;
	}
	if ( error = dev_add_name(name ,parent ,NULL, dnp, nm_pp))
	{
		devfs_dn_free(dnp); /* 1->0 for dir, 0->(-1) for other */
		printf("Device %s: name slot allocation failed (Errno=%d)\n",
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
void *
devfs_add_devswf(void *devsw, int minor, int chrblk, uid_t uid,
		 gid_t gid, int perms, char *fmt, ...)
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
		if( dev_add_entry(name, dnp, DEV_CDEV, &by, NULL, NULL, &new_dev))
			return NULL;
		break;
	case	DV_BLK:
		bd = devsw;
		major = bd->d_maj;
		if ( major == -1 ) return NULL;
		by.Bdev.bdevsw = bd;
		by.Bdev.dev = makedev(major, minor);
		if( dev_add_entry(name, dnp, DEV_BDEV, &by, NULL, NULL, &new_dev))
			return NULL;
		break;
	default:
		return NULL;
	}
	new_dev->dnp->gid = gid;
	new_dev->dnp->uid = uid;
	new_dev->dnp->mode |= perms;
	devfs_propogate(dnp->by.Dir.myname,new_dev);
	return new_dev;
}

/***********************************************************************\
* Add the named device entry into the given directory, and make it 	*
*  a link to the already created device given as an arg..		*
* this function is exported.. see sys/devfsext.h			*
\***********************************************************************/
void *
devfs_link(void *original, char *fmt, ...)
{
	devnm_p	new_dev;
	devnm_p	orig = (devnm_p) original;
	dn_p	dirnode;	/* devnode for parent directory */
	int	retval;

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
	 * the correctness of original should be checked..
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
	devfs_propogate(dirnode->by.Dir.myname,new_dev);
	return new_dev;
}

/*
 * internal kernel call to open a device. Return either 0 or an open vnode.
 */
struct vnode *
devfs_open_device(char *path, int type)
{
	register char *lastslash;
	char *nextpart;
	devnm_p	nm_p;
	dn_p dirnode;
	struct vnode *vn;

	/*
	 * If the caller didn't supply a full path, ignore and be
	 * noisy about it.
	 */
	if (*path != '/') {
		printf (__FUNCTION__ ": caller supplied bad path\n");
		return (NULL);
	}

	/*
	 * find the last '/'. Unfortunatly rindex() while being in
	 * libkern source, is not being compiled.. do it by hand.
	 * lastslash = strrchr(path,(int)'c');
	 * There will be at LEAST one '/'.
	 */
	{
		register char *p = path; /* don't destroy path */

		for (lastslash = NULL;*p; ++p) {
			if (*p == '/')
				lastslash = p;
		}
	}
	dirnode = dev_root->dnp;
	if(lastslash != path) {
		/* find the directory we need */
		*lastslash = '\0';
		if (dev_finddir(path, dirnode, NULL, &dirnode) != 0) {
			*lastslash = '/';
			return (NULL);
		}
		/* ok we found the directory, put the slash back */
		*lastslash = '/';
	}
	nextpart = ++lastslash;
	if (*nextpart == '\0')
		return (NULL);
	/*
 	 * Now only return true if it exists and is the right type.
	 */
	if ((nm_p = dev_findname(dirnode, nextpart)) == NULL) {
		return (NULL);
	}
	switch(type) {
	case DV_BLK:
		if( nm_p->dnp->type != DEV_BDEV)
			return (NULL);
		break;
	case DV_CHR:
		if( nm_p->dnp->type != DEV_CDEV)
			return (NULL);
		break;
	}

	if ( devfs_dntovn(nm_p->dnp, &vn))
		return (NULL);

#if 0
	if ( VOP_OPEN(vn, FREAD, proc0.p_cred->pc_ucred, &proc0)) {
	 	vput(vn);
		return (NULL);
	}
#endif
	return (vn);	
}

/*
 * internal kernel call to close a devfs device.
 * It should have been openned by th ecall above.
 * try not mix it with user-openned vnodes.
 * Frees the vnode.
 */
void
devfs_close_device(struct vnode *vn)
{
#if 0
	VOP_CLOSE(vn, 0, proc0.p_cred->pc_ucred, &proc0) ;
#endif
	vput(vn);
}

/*
 * Little utility routine for compatibilty.
 * Returns the dev_t that a devfs vnode represents.
 * should go away after dev_t go away :).
 */
dev_t 
devfs_vntodev(struct vnode *vn)
{
	register dn_p  dnp; 
	dnp = (dn_p)vn->v_data;
	switch (dnp->type) {
	case	DEV_BDEV:
		return (dnp->by.Bdev.dev);
		break;
	case	DEV_CDEV:
		return (dnp->by.Cdev.dev);
		break;
	}
	panic ("bad devfs DEVICE vnode");
}
