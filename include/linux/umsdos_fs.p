/* check.c 23/01/95 03.38.30 */
void check_page_tables (void);

/* dir.c 22/06/95 00.22.12 */
int  dummy_dir_read ( struct file *filp,
	 char *buf,
	 size_t size,
	 loff_t *count);
char * umsdos_d_path(struct dentry *, char *, int);
void umsdos_lookup_patch_new(struct dentry *, struct umsdos_info *);
int umsdos_is_pseudodos (struct inode *dir, struct dentry *dentry);
struct dentry *umsdos_lookup_x ( struct inode *dir, struct dentry *dentry, int nopseudo);
struct dentry *UMSDOS_lookup(struct inode *, struct dentry *);
struct dentry *umsdos_lookup_dentry(struct dentry *, char *, int, int);
struct dentry *umsdos_covered(struct dentry *, char *, int);

struct dentry *umsdos_solve_hlink (struct dentry *hlink);

/* emd.c 22/06/95 00.22.04 */
struct dentry *umsdos_get_emd_dentry(struct dentry *);
int umsdos_have_emd(struct dentry *);
int umsdos_make_emd(struct dentry *);
int umsdos_emd_dir_readentry (struct dentry *, loff_t *, struct umsdos_dirent *);
int umsdos_newentry (struct dentry *, struct umsdos_info *);
int umsdos_newhidden (struct dentry *, struct umsdos_info *);
int umsdos_delentry (struct dentry *, struct umsdos_info *, int);
int umsdos_findentry (struct dentry *, struct umsdos_info *, int);
int umsdos_isempty (struct dentry *);
int umsdos_writeentry (struct dentry *, struct umsdos_info *, int);

/* file.c 25/01/95 02.25.38 */

/* inode.c 12/06/95 09.49.40 */
void fill_new_filp (struct file *filp, struct dentry *dentry);
void UMSDOS_read_inode (struct inode *);
void UMSDOS_write_inode (struct inode *, int);
int UMSDOS_notify_change (struct dentry *, struct iattr *attr);
int umsdos_notify_change_locked(struct dentry *, struct iattr *attr);
void UMSDOS_put_inode (struct inode *);
int UMSDOS_statfs (struct super_block *, struct statfs *);
struct super_block *UMSDOS_read_super (struct super_block *, void *, int);
void UMSDOS_put_super (struct super_block *);

void umsdos_setup_dir(struct dentry *);
void umsdos_set_dirinfo_new(struct dentry *, off_t);
void umsdos_patch_dentry_inode (struct dentry *, off_t);
int umsdos_get_dirowner (struct inode *inode, struct inode **result);

/* ioctl.c 22/06/95 00.22.08 */
int UMSDOS_ioctl_dir (struct inode *dir,
	 struct file *filp,
	 unsigned int cmd,
	 unsigned long data);

/* mangle.c 25/01/95 02.25.38 */
void umsdos_manglename (struct umsdos_info *info);
int umsdos_evalrecsize (int len);
int umsdos_parse (const char *name,int len, struct umsdos_info *info);

/* namei.c 25/01/95 02.25.38 */
void umsdos_lockcreate (struct inode *dir);
void umsdos_startlookup (struct inode *dir);
void umsdos_unlockcreate (struct inode *dir);
void umsdos_endlookup (struct inode *dir);

int umsdos_readlink_x (	     struct dentry *dentry,
			     char *buffer,
			     int bufsiz);
int UMSDOS_symlink (struct inode *dir,
		    struct dentry *dentry,
		    const char *symname);
int UMSDOS_link (struct dentry *olddentry,
		 struct inode *dir,
		 struct dentry *dentry);
int UMSDOS_create (struct inode *dir,
		   struct dentry *dentry,
		   int mode);

int UMSDOS_mkdir (struct inode *dir,
		  struct dentry *dentry,
		  int mode);
int UMSDOS_mknod (struct inode *dir,
		  struct dentry *dentry,
		  int mode,
		  int rdev);
int UMSDOS_rmdir (struct inode *dir,struct dentry *dentry);
int UMSDOS_unlink (struct inode *dir, struct dentry *dentry);
int UMSDOS_rename (struct inode *old_dir,
		   struct dentry *old_dentry,
		   struct inode *new_dir,
		   struct dentry *new_dentry);

/* rdir.c 22/03/95 03.31.42 */
struct dentry *umsdos_rlookup_x (struct inode *dir, struct dentry *dentry, int nopseudo);
struct dentry *UMSDOS_rlookup (struct inode *dir, struct dentry *dentry);
