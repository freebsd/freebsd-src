/* cnode related routines for the coda kernel code
   (C) 1996 Peter Braam
   */

#include <linux/types.h>
#include <linux/string.h>
#include <linux/time.h>

#include <linux/coda.h>
#include <linux/coda_linux.h>
#include <linux/coda_fs_i.h>
#include <linux/coda_psdev.h>

extern int coda_debug;

inline int coda_fideq(ViceFid *fid1, ViceFid *fid2)
{
	if (fid1->Vnode != fid2->Vnode)   return 0;
	if (fid1->Volume != fid2->Volume) return 0;
	if (fid1->Unique != fid2->Unique) return 0;
	return 1;
}

inline int coda_isnullfid(ViceFid *fid)
{
	if (fid->Vnode || fid->Volume || fid->Unique) return 0;
	return 1;
}

static int coda_inocmp(struct inode *inode, unsigned long ino, void *opaque)
{
	return (coda_fideq((ViceFid *)opaque, &(ITOC(inode)->c_fid)));
}

static struct inode_operations coda_symlink_inode_operations = {
	readlink:	page_readlink,
	follow_link:	page_follow_link,
	setattr:	coda_notify_change,
};

/* cnode.c */
static void coda_fill_inode(struct inode *inode, struct coda_vattr *attr)
{
        CDEBUG(D_SUPER, "ino: %ld\n", inode->i_ino);

        if (coda_debug & D_SUPER ) 
		print_vattr(attr);

        coda_vattr_to_iattr(inode, attr);

        if (S_ISREG(inode->i_mode)) {
                inode->i_op = &coda_file_inode_operations;
                inode->i_fop = &coda_file_operations;
        } else if (S_ISDIR(inode->i_mode)) {
                inode->i_op = &coda_dir_inode_operations;
                inode->i_fop = &coda_dir_operations;
        } else if (S_ISLNK(inode->i_mode)) {
		inode->i_op = &coda_symlink_inode_operations;
		inode->i_data.a_ops = &coda_symlink_aops;
		inode->i_mapping = &inode->i_data;
	} else
                init_special_inode(inode, inode->i_mode, attr->va_rdev);
}

struct inode * coda_iget(struct super_block * sb, ViceFid * fid,
			 struct coda_vattr * attr)
{
	struct inode *inode;
	struct coda_inode_info *cii;
	ino_t ino = coda_f2i(fid);
	struct coda_sb_info *sbi = coda_sbp(sb);

	down(&sbi->sbi_iget4_mutex);
	inode = iget4(sb, ino, coda_inocmp, fid);

	if ( !inode ) { 
		CDEBUG(D_CNODE, "coda_iget: no inode\n");
		up(&sbi->sbi_iget4_mutex);
		return ERR_PTR(-ENOMEM);
	}

	/* check if the inode is already initialized */
	cii = ITOC(inode);
	if (coda_isnullfid(&cii->c_fid))
		/* new, empty inode found... initializing */
		cii->c_fid = *fid;
	up(&sbi->sbi_iget4_mutex);

	/* always replace the attributes, type might have changed */
	coda_fill_inode(inode, attr);
	return inode;
}

/* this is effectively coda_iget:
   - get attributes (might be cached)
   - get the inode for the fid using vfs iget
   - link the two up if this is needed
   - fill in the attributes
*/
int coda_cnode_make(struct inode **inode, ViceFid *fid, struct super_block *sb)
{
        struct coda_vattr attr;
        int error;
        
	/* We get inode numbers from Venus -- see venus source */
	error = venus_getattr(sb, fid, &attr);
	if ( error ) {
	    CDEBUG(D_CNODE, 
		   "coda_cnode_make: coda_getvattr returned %d for %s.\n", 
		   error, coda_f2s(fid));
	    *inode = NULL;
	    return error;
	} 

	*inode = coda_iget(sb, fid, &attr);
	if ( IS_ERR(*inode) ) {
		printk("coda_cnode_make: coda_iget failed\n");
                return PTR_ERR(*inode);
        }

	CDEBUG(D_DOWNCALL, "Done making inode: ino %ld, count %d with %s\n",
		(*inode)->i_ino, atomic_read(&(*inode)->i_count), 
		coda_f2s(&ITOC(*inode)->c_fid));
	return 0;
}


void coda_replace_fid(struct inode *inode, struct ViceFid *oldfid, 
		      struct ViceFid *newfid)
{
	struct coda_inode_info *cii;
	
	cii = ITOC(inode);

	if (!coda_fideq(&cii->c_fid, oldfid))
		BUG();

	/* replace fid and rehash inode */
	/* XXX we probably need to hold some lock here! */
	remove_inode_hash(inode);
	cii->c_fid = *newfid;
	inode->i_ino = coda_f2i(newfid);
	insert_inode_hash(inode);
}

/* convert a fid to an inode. */
struct inode *coda_fid_to_inode(ViceFid *fid, struct super_block *sb) 
{
	ino_t nr;
	struct inode *inode;
	struct coda_inode_info *cii;
	struct coda_sb_info *sbi;

	if ( !sb ) {
		printk("coda_fid_to_inode: no sb!\n");
		return NULL;
	}

	CDEBUG(D_INODE, "%s\n", coda_f2s(fid));

	sbi = coda_sbp(sb);
	nr = coda_f2i(fid);
	down(&sbi->sbi_iget4_mutex);
	inode = iget4(sb, nr, coda_inocmp, fid);
	if ( !inode ) {
		printk("coda_fid_to_inode: null from iget, sb %p, nr %ld.\n",
		       sb, (long)nr);
		goto out_unlock;
	}

	cii = ITOC(inode);

	/* The inode could already be purged due to memory pressure */
	if (coda_isnullfid(&cii->c_fid)) {
		inode->i_nlink = 0;
		iput(inode);
		goto out_unlock;
	}

        CDEBUG(D_INODE, "found %ld\n", inode->i_ino);
	up(&sbi->sbi_iget4_mutex);
	return inode;

out_unlock:
	up(&sbi->sbi_iget4_mutex);
	return NULL;
}

/* the CONTROL inode is made without asking attributes from Venus */
int coda_cnode_makectl(struct inode **inode, struct super_block *sb)
{
	int error = 0;

	*inode = iget(sb, CTL_INO);
	if ( *inode ) {
		(*inode)->i_op = &coda_ioctl_inode_operations;
		(*inode)->i_fop = &coda_ioctl_operations;
		(*inode)->i_mode = 0444;
		error = 0;
	} else { 
		error = -ENOMEM;
	}
    
	return error;
}

