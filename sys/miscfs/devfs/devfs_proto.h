/* THIS FILE PRODUCED AUTOMATICALLY */
void  devfs_sinit(void *junk) /*proto*/;
devnm_p dev_findname(dn_p dir,char *name) /*proto*/;
int	dev_finddir(char *orig_path, dn_p dirnode, int create, dn_p *dn_pp) /*proto*/;
int	dev_add_name(char *name, dn_p dirnode, devnm_p back, dn_p dnp, devnm_p *devnm_pp) /*proto*/;
int	dev_add_node(int entrytype, union typeinfo *by, dn_p proto, dn_p *dn_pp) /*proto*/;
int	dev_touch(devnm_p key)		/* update the node for this dev */ /*proto*/;
void	devfs_dn_free(dn_p dnp) /*proto*/;
int devfs_add_fronts(devnm_p parent,devnm_p child) /*proto*/;
int dev_dup_plane(struct devfsmount *devfs_mp_p) /*proto*/;
void  devfs_free_plane(struct devfsmount *devfs_mp_p) /*proto*/;
int dev_dup_entry(dn_p parent, devnm_p back, devnm_p *dnm_pp, struct devfsmount *dvm) /*proto*/;
void dev_free_name(devnm_p devnmp) /*proto*/;
int devfs_vntodn(struct vnode *vn_p, dn_p *dn_pp) /*proto*/;
int devfs_dntovn(dn_p dnp, struct vnode **vn_pp) /*proto*/;
int dev_add_entry(char *name, dn_p parent, int type, union typeinfo *by, devnm_p *nm_pp) /*proto*/ ;
int devfs_mount( struct mount *mp, char *path, caddr_t data, struct nameidata *ndp, struct proc *p) /*proto*/;
void	devfs_dropvnode(dn_p dnp) /*proto*/;
/* THIS FILE PRODUCED AUTOMATICALLY */
/* DO NOT EDIT (see reproto.sh) */
