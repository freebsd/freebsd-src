#define su_lasti	u.bfs_sb.si_lasti
#define su_blocks	u.bfs_sb.si_blocks
#define su_freeb	u.bfs_sb.si_freeb
#define su_freei	u.bfs_sb.si_freei
#define su_lf_ioff	u.bfs_sb.si_lf_ioff
#define su_lf_sblk	u.bfs_sb.si_lf_sblk
#define su_lf_eblk	u.bfs_sb.si_lf_eblk
#define su_imap		u.bfs_sb.si_imap
#define su_sbh		u.bfs_sb.si_sbh
#define su_bfs_sb	u.bfs_sb.si_bfs_sb

#define iu_dsk_ino	u.bfs_i.i_dsk_ino
#define iu_sblock	u.bfs_i.i_sblock
#define iu_eblock	u.bfs_i.i_eblock

#define printf(format, args...) \
do { \
	printk(KERN_ERR "BFS-fs: %s(): ", __FUNCTION__); \
	printk(format, args); \
} while (0)
