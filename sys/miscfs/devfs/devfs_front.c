/*
 * Written by Julian Elischer (julian@DIALix.oz.au)
 *
 *	$Header: /pub/FreeBSD/FreeBSD-CVS/src/sys/miscfs/devfs/Attic/devfs_front.c,v 1.4 1995/05/30 08:06:50 rgrimes Exp $
 *
 */

#include "param.h"
#include "systm.h"
#include "kernel.h"
#include "file.h"		/* define FWRITE ... */
#include "conf.h"
#include "stat.h"
#include "mount.h"
#include "vnode.h"
#include "malloc.h"
#include "dir.h"		/* defines dirent structure		*/
#include "devfsdefs.h"



/***********************************************************************\
* Given a directory backing node, and a child backing node, add the	*
* appropriate front nodes to the front nodes of the directory to	*
* represent the child node to the user					*
*									*
* on failure, front nodes will either be correct or not exist for each	*
* front dir, however dirs completed will not be stripped of completed	*
* frontnodes on failure of a later parent frontnode			*
*									*
\***********************************************************************/
int devfs_add_fronts(devnm_p parent,devnm_p child) /*proto*/
{
	devnm_p	newfp;
	devnm_p  falias;

	DBPRINT(("	devfs_add_fronts\n"));
	/***********************************************\
	* Find the frontnodes of the parent node	*
	\***********************************************/
	for (falias = parent->next_front; falias; falias = falias->next_front)
	{
		if(dev_findfront(falias->dnp,child->name))
		{
			printf("Device %s not created, already exists\n",
				child->name);
			continue;
		}
		if( dev_mk_front(falias->dnp,child,&newfp,NULL))
		{
			printf("Device %s: allocation failed\n",
				child->name);
			continue;
		}

	}
	return(0);	/* for now always succeed */
}

/***************************************************************\
* Search down the linked list off a front dir to find "name"	*
* return the dn_p for that node.
\***************************************************************/
dn_p dev_findfront(dn_p dir,char *name) /*proto*/
{
	devnm_p newfp;
	DBPRINT(("	dev_findfront(%s)\n",name));
	if(dir->type != DEV_DIR) return 0;/*XXX*/ /* printf?*/

	if(name[0] == '.')
	{
		if(name[1] == 0)
		{
			return dir;
		}
		if((name[1] == '.') && (name[2] == 0))
		{
			if(dir->by.Dir.parent == dir) /* root? */
				return dir;
			else
				return dir->by.Dir.parent;
		}
	}
	newfp = dir->by.Dir.dirlist;
	while(newfp)
	{
		if(!(strcmp(name,newfp->name)))
			break;
		newfp = newfp->next;
	}
	if(newfp)
		return newfp->dnp;
	else
		return (dn_p)0;
}

/***************************************************************\
* Create and link in a new front element.. 			*
* Parent can be 0 for a root node				*
* Not presently usable to make a symlink XXX			*
* Must teach this to handle where there is no back node		*
* maybe split into two bits?					*
\***************************************************************/
int dev_mk_front(dn_p parent,devnm_p back,devnm_p *devnm_pp , struct devfsmount *dvm) /*proto*/
{
	devnm_p	newfp;
	struct	devfsmount *dmt;
	devnm_p	newback;
	devnm_p	newfront;
	int	error;
	dn_p	dnp;

	DBPRINT(("	dev_mk_front\n"));
	if(parent && (parent->type != DEV_DIR)) return EINVAL;
		/*XXX*/ /* printf?*/
	if(!(newfp = malloc(sizeof(devnm_t),M_DEVFSFRONT,M_NOWAIT)))
	{
		return(ENOMEM);
	}
	bzero(newfp,sizeof(*newfp));
	strcpy(newfp->name,back->name);

	/*******************************************************\
	* If we are creating an alias, Then we need to find the *
	* real object's file_node. (It must pre-exist)		*
 	* this means that aliases have no front nodes...	*
	* In effect ALIAS back nodes are just place markers	*
	\*******************************************************/
	if(back->dnp->type == DEV_ALIAS)
	{
		back = back->dnp->by.Alias.realthing;
	}

	/*
	 * either use the existing devnode or make our own,
	 * depending on if we are a dev or a dir.
	 */
	switch(back->dnp->type) {
	case	DEV_BDEV:
	case	DEV_CDEV:
	case	DEV_DDEV:
		dnp = newfp->dnp = back->dnp;
		newfp->dnp->links++; /* wherever it is.....*/
		break;
	case	DEV_DIR:
		dnp = newfp->dnp = malloc(sizeof(devnode_t),
					M_DEVFSNODE,M_NOWAIT);
		if(!(dnp))
		{
			free(newfp,M_DEVFSFRONT);
			return ENOMEM;
		}
		/*
		 * we have two options.. bcopy and reset some items,
		 * or bzero and reset or copy some items...
		 */
		bcopy(back->dnp,newfp->dnp,sizeof(devnode_t));
		dnp->links = 1;		/*  EXTRA from '.' */
		dnp->links++; /* wherever it is.....*/
		dnp->by.Dir.dirlast =
				&dnp->by.Dir.dirlist;
		dnp->by.Dir.dirlist = NULL;
		dnp->by.Dir.entrycount = 0;
		dnp->vn = NULL;
		dnp->vn_id = 0;
		break;
	case	DEV_SLNK: /* should never happen XXX (hmm might)*/
	default:
		printf("unknown DEV type\n");
		return EINVAL;
	}
	/*******************************************************\
	* Put it in the parent's directory list (at the end).	*
	\*******************************************************/
	if(parent)
	{
		newfp->next = *parent->by.Dir.dirlast;
		newfp->prevp = parent->by.Dir.dirlast;
		*parent->by.Dir.dirlast = newfp;
		parent->by.Dir.dirlast = &newfp->next;
		parent->by.Dir.entrycount++;
		newfp->dnp->dvm = parent->dvm; /* XXX bad for devs */
		if(back->dnp->type == DEV_DIR)
		{
			newfp->dnp->by.Dir.parent
				= parent;
			parent->links++;	/* only dirs have '..'*/
		}
		parent->len += strlen(newfp->name) + 8;/*ok, ok?*/
	} else {
		/*
		 * it's the root node, put in the dvm
		 * and link it to itself...
		 * we know it's a DIR
		 */
		dnp->by.Dir.parent = newfp->dnp;
		dnp->links++;	/* extra for '..'*/
		dnp->dvm = dvm;
	}

	/*
	 * not accounted for in the link counts..
	 * only used to get from the front name entries
	 * to the total length of the names
	 * which is stored in the parent's devnode
	 */
 	newfp->parent = parent; /* is NULL for root */
	/*******************************************************\
	* Put it in the appropriate back/front list too.	*
	\*******************************************************/
	newfp->next_front = *back->prev_frontp;
	newfp->prev_frontp = back->prev_frontp;
	*back->prev_frontp = newfp;
	back->prev_frontp = &(newfp->next_front);
	back->frontcount++;
	newfp->as.front.realthing = back;

	/*
	 * If it is a directory, then recurse down all the other
	 * subnodes in it....
	 */
	if ( newfp->dnp->type == DEV_DIR)
	{
		for(newback = back->dnp->by.Dir.dirlist;
				newback; newback = newback->next)
		{
			if(error = dev_mk_front(newfp->dnp,
						newback, &newfront, NULL))
			{
				return error;
			}
		}
	}
	*devnm_pp = newfp;
	return(0);
}

/*
 * duplicate the backing tree into a tree of nodes hung off the
 * mount point given as the argument. Do this by
 * calling dev_mk_front() which recurses all the way
 * up the tree..
 */
int devfs_make_plane(struct devfsmount *devfs_mp_p) /*proto*/
{
	devnm_p	parent;
	devnm_p	new;
	devnm_p	realthing;
	int	error;

	DBPRINT(("	devfs_make_plane\n"));
	realthing = dev_root;
	if(error = dev_mk_front(0, realthing,&new, devfs_mp_p))
	{
		return error;
	}
	devfs_mp_p->plane_root = new;

	return error;
}

void  devfs_free_plane(struct devfsmount *devfs_mp_p) /*proto*/
{
	devnm_p devfp;

	DBPRINT(("	devfs_free_plane\n"));
	devfp = devfs_mp_p->plane_root;
	if(devfp) dev_free_front(devfp);
}

/*
 * Remove all the front nodes associated with a backing node
 */
void devfs_remove_fronts(devnm_p devbp) /*proto*/
{
	while(devbp->next_front)
	{
		dev_free_front(devbp->next_front);
	}
}
/***************************************************************\
* Free a front node (and any below it of it's a directory node)	*
\***************************************************************/
void dev_free_front(devnm_p devfp) /*proto*/
{
	dn_p	parent = devfp->parent;
	devnm_p	back;

	DBPRINT(("	dev_free_front\n"));
	if(devfp->dnp->type == DEV_DIR)
	{
		while(devfp->dnp->by.Dir.dirlist)
		{
			dev_free_front(devfp->dnp->by.Dir.dirlist);
		}
		/*
		 * drop the reference counts on our and our parent's
		 * nodes for "." and ".." (root has ".." -> "." )
		 */
		devfs_dn_free(devfp->dnp);	/* account for '.' */
		devfs_dn_free(devfp->dnp->by.Dir.parent); /* and '..' */
	}
	/*
	 * unlink ourselves from the directory on this plane
	 */
	if(parent) /* if not fs root */
	{
		if( *devfp->prevp = devfp->next)/* yes, assign */
		{
			devfp->next->prevp = devfp->prevp;
		}
		else
		{
			parent->by.Dir.dirlast
				= devfp->prevp;
		}
		parent->by.Dir.entrycount--;
		parent->len -= strlen(devfp->name);
	}
	/*
	 * If the node has a backing pointer we need to free ourselves
	 * from that..
	 * Remember that we may not HAVE a backing node.
	 */
	if (back = devfp->as.front.realthing) /* yes an assign */
	{
		if( *devfp->prev_frontp = devfp->next_front)/* yes, assign */
		{
			devfp->next_front->prev_frontp = devfp->prev_frontp;
		}
		else
		{
			back->prev_frontp = devfp->prev_frontp;
		}
		back->frontcount--;
	}
	/***************************************************************\
	* If the front node has it's own devnode structure,		*
	* then free it.							*
	\***************************************************************/
	devfs_dn_free(devfp->dnp);
	free(devfp,M_DEVFSFRONT);
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
