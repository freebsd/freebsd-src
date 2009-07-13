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

#define PS_FST_FFLAG_READ 0x01
#define PS_FST_FFLAG_WRITE 0x02

#define	PS_FST_FLAG_ERROR 0x01
#define	PS_FST_FLAG_UNKNOWNFS 0x02

void	procstat_close(struct procstat *procstat);
struct procstat	*procstat_open(const char *nlistf, const char *memf);
struct kinfo_proc	*procstat_getprocs(struct procstat *procstat,
    int what, int arg, unsigned int *count);
struct filestat	*procstat_getfiles(struct procstat *procstat,
    struct kinfo_proc *kp, unsigned int *cnt);
struct filestat	*procstat_getfiles_sysctl(struct kinfo_proc *kp, unsigned int *cnt);
struct filestat	*procstat_getfiles_kvm(kvm_t *kd, struct kinfo_proc *kp, unsigned int *cnt);
int	vtrans_kvm(kvm_t *kd, struct vnode *vp, int fd, int flags, struct filestat *fst);
