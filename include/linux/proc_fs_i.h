struct proc_inode_info {
	struct task_struct *task;
	int type;
	union {
		int (*proc_get_link)(struct inode *, struct dentry **, struct vfsmount **);
		int (*proc_read)(struct task_struct *task, char *page);
	} op;
	struct file *file;
};
