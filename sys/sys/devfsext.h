void *dev_add(char *path,
		char *name,
		void *funct,
		int minor,
		int chrblk,
		uid_t uid,
		gid_t gid,
		int perms)  ; 
#define DV_CHR 0
#define DV_BLK 1
#define DV_DEV 2
