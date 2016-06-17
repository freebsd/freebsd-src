#ifndef _FAT_CVF
#define _FAT_CVF

#define CVF_USE_READPAGE  0x0001

struct cvf_format
{ int cvf_version;
  char* cvf_version_text;
  unsigned long flags;
  int (*detect_cvf) (struct super_block*sb);
  int (*mount_cvf) (struct super_block*sb,char*options);
  int (*unmount_cvf) (struct super_block*sb);
  struct buffer_head* (*cvf_bread) (struct super_block*sb,int block);
  struct buffer_head* (*cvf_getblk) (struct super_block*sb,int block);
  void (*cvf_brelse) (struct super_block *sb,struct buffer_head *bh);
  void (*cvf_mark_buffer_dirty) (struct super_block *sb,
                              struct buffer_head *bh);
  void (*cvf_set_uptodate) (struct super_block *sb,
                         struct buffer_head *bh,
                         int val);
  int (*cvf_is_uptodate) (struct super_block *sb,struct buffer_head *bh);
  void (*cvf_ll_rw_block) (struct super_block *sb,
                        int opr,
                        int nbreq,
                        struct buffer_head *bh[32]);
  int (*fat_access) (struct super_block *sb,int nr,int new_value);
  int (*cvf_statfs) (struct super_block *sb,struct statfs *buf, int bufsiz);
  int (*cvf_bmap) (struct inode *inode,int block);
  ssize_t (*cvf_file_read) ( struct file *, char *, size_t, loff_t *);
  ssize_t (*cvf_file_write) ( struct file *, const char *, size_t, loff_t *);
  int (*cvf_mmap) (struct file *, struct vm_area_struct *);
  int (*cvf_readpage) (struct inode *, struct page *);
  int (*cvf_writepage) (struct inode *, struct page *);
  int (*cvf_dir_ioctl) (struct inode * inode, struct file * filp,
                        unsigned int cmd, unsigned long arg);
  void (*zero_out_cluster) (struct inode*, int clusternr);
};

int register_cvf_format(struct cvf_format*cvf_format);
int unregister_cvf_format(struct cvf_format*cvf_format);
void dec_cvf_format_use_count_by_version(int version);
int detect_cvf(struct super_block*sb,char*force);

extern struct cvf_format *cvf_formats[];
extern int cvf_format_use_count[];

#endif
