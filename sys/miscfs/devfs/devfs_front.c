/*
 * Written by Julian Elischer (julian@DIALix.oz.au)
 *
 *	$Header: /sys/miscfs/devfs/RCS/devfs_front.c,v 1.3 1995/01/07 04:20:25 root Exp root $
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
int devfs_add_fronts(devb_p parent,devb_p child) /*proto*/
{
	devf_p	newfp;
	devf_p  falias;

	DBPRINT(("	devfs_add_fronts\n"));
	/***********************************************\
	* Find the frontnodes of the parent node	*
	\***********************************************/
	for (falias = parent->fronts; falias; falias = falias->next_front)
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
	devf_p newfp;
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
int dev_mk_front(dn_p parent,devb_p back,devf_p *devf_pp , struct devfsmount *dvm) /*proto*/
{
	devf_p	newfp;
	struct	devfsmount *dmt;
	devb_p	newback;
	devf_p	newfront;
	int	error;

	DBPRINT(("	dev_mk_front\n"));
	if(parent && (parent->type != DEV_DIR)) return EINVAL;
		/*XXX*/ /* printf?*/
	if(!(newfp = malloc(sizeof(*newfp),M_DEVFSFRONT,M_NOWAIT)))
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
	* Check the removal code for this! XXX			*
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
		newfp->dnp = back->dnp;
		newfp->dnp->links++; /* wherever it is.....*/
		break;
	case	DEV_DIR:
		newfp->dnp = malloc(sizeof(devnode_t),
					M_DEVFSNODE,M_NOWAIT);
		if(!(newfp->dnp))
		{
			free(newfp,M_DEVFSFRONT);
			return ENOMEM;
		}
		/*
		 * we have two options.. bcopy and reset some items,
		 * or bzero and reset or copy some items...
		 */
		bcopy(back->dnp,newfp->dnp,sizeof(devnode_t));
		newfp->dnp->links = 1;		/*  EXTRA from '.' */
		newfp->dnp->links++; /* wherever it is.....*/
		newfp->dnp->by.Dir.dirlast =
				&newfp->dnp->by.Dir.dirlist;
		newfp->dnp->by.Dir.dirlist = NULL;
		newfp->dnp->by.Dir.entrycount = 0;
		newfp->dnp->vn = NULL;
		newfp->dnp->vn_id = 0;
		break;
	case	DEV_SLNK: /* should never happen */
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
		 */
		newfp->dnp->by.Dir.parent = newfp->dnp;
		newfp->dnp->links++;	/* extra for '..'*/
		newfp->dnp->dvm = dvm;
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
	newfp->next_front = *back->lastfront; 
	newfp->prev_frontp = back->lastfront;
	*back->lastfront = newfp;
	back->lastfront = &(newfp->next_front);
	back->frontcount++;
	newfp->realthing = back;

	/*
	 * If it is a directory, then recurse down all the other
	 * subnodes in it....
	 */
	if ( newfp->dnp->type == DEV_DIR)
	{
		for(newback = back->dnp->by.BackDir.dirlist;
				newback; newback = newback->next)
		{
			if(error = dev_mk_front(newfp->dnp,
						newback, &newfront, NULL))
			{
				return error;
			}
		}
	}
	*devf_pp = newfp;
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
	devf_p	parent;
	devf_p	new;
	devb_p	realthing;
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
	devf_p devfp;

	DBPRINT(("	devfs_free_plane\n"));
	devfp = devfs_mp_p->plane_root;
	if(devfp) dev_free_front(devfp);
}

/*
 * Remove all the front nodes associated with a backing node
 */
void devfs_remove_fronts(devb_p devbp) /*proto*/
{
	while(devbp->fronts)
	{
		dev_free_front(devbp->fronts);
	}
}
/***************************************************************\
* Free a front node (and any below it of it's a directory node)	*
\***************************************************************/
void dev_free_front(devf_p devfp) /*proto*/
{
	dn_p	parent = devfp->parent;
	devb_p	back;

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
	if (back = devfp->realthing) /* yes an assign */
	{
		if( *devfp->prev_frontp = devfp->next_front)/* yes, assign */
		{
			devfp->next_front->prev_frontp = devfp->prev_frontp;
		}
		else
		{
			back->lastfront = devfp->prev_frontp;
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
*     Think about this: 					*
* Though this routine uses a front node, it also uses a backing	*
* node indirectly, via the 'realthing' link. This may prove bad	*
* in the case of a user-added slink, where there migh not be a	*
* backing node. (e.g. if a slink points out of the fs it CAN'T	*
* have a backing node, unlike a hardlink which does..)		*
* we are going to have to think very carefully about slinks..	*
\***************************************************************/
int devfs_dntovn(dn_p front, struct vnode **vn_pp) /*proto*/
{
	struct vnode *vn_p, *nvp;
	int error = 0;

	vn_p = front->vn;
DBPRINT(("dntovn "));
	if( vn_p)
	{
		if(vn_p->v_id != front->vn_id)
		{
			printf("bad-id ");
			goto skip;
		}
		if(vn_p->v_tag != VT_DEVFS)
		{
			printf("bad-tag ");
			goto skip;
		}
		if(vn_p->v_op != *(front->ops))
		{
			printf("bad-ops ");
			goto skip;
		}
		if((dn_p)(vn_p->v_data) != front)
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
			front->dvm->mount,
			*(front->ops),
			&vn_p)))
	{
		front->vn = vn_p;
		front->vn_id = vn_p->v_id;
		*vn_pp = vn_p;
DBPRINT(("(New vnode)"));
		switch(front->type)
		{
		case	DEV_SLNK:
			break;
		case	DEV_DIR:
			if(front->by.Dir.parent == front)
			{
				vn_p->v_flag |= VROOT;
			}
			vn_p->v_type = VDIR;
			break;
		case	DEV_BDEV:
			vn_p->v_type = VBLK;
			if (nvp = checkalias(vn_p,
			   front->by.Bdev.dev,
			  (struct mount *)0))
			{
				vput(vn_p);
				vn_p = nvp;
			}
			break;
		case	DEV_CDEV:
			vn_p->v_type = VCHR;
			if (nvp = checkalias(vn_p,
			   front->by.Cdev.dev,
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
			vn_p->v_mount  = front->dvm->mount;/* Duplicated */
			*vn_pp = vn_p;
			vn_p->v_data = (void *)front;
/*XXX*/ /* maybe not.. I mean what if it's a dev... (vnode at back)*/
		}
		else
		{
			error = EINVAL;
		}
	}
	return(error);
}

