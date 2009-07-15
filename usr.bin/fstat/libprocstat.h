#define	PS_FST_VTYPE_VNON	1
#define	PS_FST_VTYPE_VREG	2
#define	PS_FST_VTYPE_VDIR	3
#define	PS_FST_VTYPE_VBLK	4
#define	PS_FST_VTYPE_VCHR	5
#define	PS_FST_VTYPE_VLNK	6
#define	PS_FST_VTYPE_VSOCK	7
#define	PS_FST_VTYPE_VFIFO	8
#define	PS_FST_VTYPE_VBAD	9
#define	PS_FST_VTYPE_UNKNOWN	255

#define	PS_FST_TYPE_VNODE	1
#define	PS_FST_TYPE_FIFO	2
#define	PS_FST_TYPE_SOCKET	3
#define	PS_FST_TYPE_PIPE	4
#define	PS_FST_TYPE_PTS		5

struct procstat {
        int     type;
        kvm_t   *kd;
};

#define	PS_FST_FD_RDIR -1
#define	PS_FST_FD_CDIR -2
#define	PS_FST_FD_JAIL -3
#define	PS_FST_FD_TRACE -4
#define	PS_FST_FD_TEXT -5
#define	PS_FST_FD_MMAP -6

#define PS_FST_FFLAG_READ	0x0001
#define PS_FST_FFLAG_WRITE	0x0002
#define	PS_FST_FFLAG_NONBLOCK	0x0004
#define	PS_FST_FFLAG_APPEND	0x0008
#define	PS_FST_FFLAG_SHLOCK	0x0010
#define	PS_FST_FFLAG_EXLOCK	0x0020
#define	PS_FST_FFLAG_ASYNC	0x0040
#define	PS_FST_FFLAG_SYNC	0x0080
#define	PS_FST_FFLAG_NOFOLLOW	0x0100
#define	PS_FST_FFLAG_CREAT	0x0200
#define	PS_FST_FFLAG_TRUNC	0x0400
#define	PS_FST_FFLAG_EXCL	0x0800

#define	PS_FST_FLAG_ERROR 0x01
#define	PS_FST_FLAG_UNKNOWNFS 0x02

void	procstat_close(struct procstat *procstat);
struct procstat	*procstat_open(const char *nlistf, const char *memf);
struct kinfo_proc	*procstat_getprocs(struct procstat *procstat,
    int what, int arg, unsigned int *count);
struct filestat_list	*procstat_getfiles(struct procstat *procstat,
    struct kinfo_proc *kp);
struct filestat_list	*procstat_getfiles_sysctl(struct kinfo_proc *kp);
struct filestat_list	*procstat_getfiles_kvm(kvm_t *kd, struct kinfo_proc *kp);
int	vtrans_kvm(kvm_t *kd, struct vnode *vp, int fd, int flags, struct filestat *fst);
char *procstat_kdevtoname(struct procstat *procstat, struct cdev *cdev);
dev_t procstat_dev2udev(struct procstat *procstat, struct cdev *cdev);
int	procstat_get_vnode_info(struct procstat *procstat, struct filestat *fst,
    struct vnstat *vn, char *errbuf);
int	procstat_get_pts_info(struct procstat *procstat, struct filestat *fst,
    struct ptsstat *pts, char *errbuf);
int	procstat_get_pipe_info(struct procstat *procstat, struct filestat *fst,
    struct pipestat *pipe, char *errbuf);
