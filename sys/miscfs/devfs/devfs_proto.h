/* $FreeBSD: src/sys/miscfs/devfs/devfs_proto.h,v 1.14.8.1 2000/08/03 01:04:28 peter Exp $ */
/* THIS FILE HAS BEEN PRODUCED AUTOMATICALLY */
void	devfs_sinit(void *junk);
devnm_p	dev_findname(dn_p dir,char *name);
int	dev_finddir(char *orig_path, dn_p dirnode, int create, dn_p *dn_pp);
int	dev_add_name(char *name, dn_p dirnode, devnm_p back, dn_p dnp,
	     devnm_p *devnm_pp);
int	dev_add_node(int entrytype, union typeinfo *by, dn_p proto,
	dn_p *dn_pp,struct  devfsmount *dvm);
int	dev_touch(devnm_p key)		/* update the node for this dev */;
void	devfs_dn_free(dn_p dnp);
int	devfs_propogate(devnm_p parent,devnm_p child);
int	dev_dup_plane(struct devfsmount *devfs_mp_p);
void	devfs_free_plane(struct devfsmount *devfs_mp_p);
int	dev_dup_entry(dn_p parent, devnm_p back, devnm_p *dnm_pp,
	      struct devfsmount *dvm);
int	dev_free_name(devnm_p devnmp);
void	dev_free_hier(devnm_p devnmp);
int	devfs_vntodn(struct vnode *vn_p, dn_p *dn_pp);
int	devfs_dntovn(dn_p dnp, struct vnode **vn_pp);
int	dev_add_entry(char *name, dn_p parent, int type, union typeinfo *by,
	      dn_p proto, struct devfsmount *dvm, devnm_p *nm_pp);
int	devfs_mount(struct mount *mp, char *path, caddr_t data,
	    struct nameidata *ndp, struct proc *p);
void	devfs_dropvnode(dn_p dnp);
/* THIS FILE PRODUCED AUTOMATICALLY */
/* DO NOT EDIT (see reproto.sh) */
