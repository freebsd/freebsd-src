struct ctl_buf {
	char *			text;
	size_t			used;
};

#define	MAX_LINELEN		990	/* Like SMTP. */
#define	MAX_NTOP		(sizeof "[255.255.255.255].65535")

#define	allocated_p(Buf) ((Buf).text != NULL)
#define	buffer_init(Buf) ((Buf).text = 0, (Buf.used) = 0)

#define	ctl_bufget	__ctl_bufget
#define	ctl_bufput	__ctl_bufput
#define	ctl_sa_ntop	__ctl_sa_ntop
#define	ctl_sa_copy	__ctl_sa_copy

int			ctl_bufget(struct ctl_buf *, ctl_logfunc);
void			ctl_bufput(struct ctl_buf *);
const char *		ctl_sa_ntop(const struct sockaddr *, char *, size_t,
				    ctl_logfunc);
void			ctl_sa_copy(const struct sockaddr *,
				    struct sockaddr *);
