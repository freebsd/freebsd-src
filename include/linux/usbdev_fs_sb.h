struct usbdev_sb_info {
	struct list_head slist;
	struct list_head ilist;
	uid_t devuid;
	gid_t devgid;
	umode_t devmode;
	uid_t busuid;
	gid_t busgid;
	umode_t busmode;
	uid_t listuid;
	gid_t listgid;
	umode_t listmode;
};
