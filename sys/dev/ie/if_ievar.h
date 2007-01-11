/*-
 * $FreeBSD: src/sys/dev/ie/if_ievar.h,v 1.3 2005/06/10 16:49:10 brooks Exp $
 */

enum ie_hardware {
	IE_NONE,
        IE_STARLAN10,
        IE_EN100,
        IE_SLFIBER,
        IE_3C507,
        IE_NI5210,
        IE_EE16,
        IE_UNKNOWN
};

/*
 * Ethernet status, per interface.
 */
struct ie_softc {
	struct	 ifnet *ifp;
	void	 (*ie_reset_586) (struct ie_softc *);
	void	 (*ie_chan_attn) (struct ie_softc *);
	enum	 ie_hardware hard_type;
	int	 hard_vers;
	int	 unit;
	u_char	 enaddr[6];

	device_t		dev;

	struct resource *	io_res;
	int			io_rid;
	bus_space_tag_t		io_bt;
	bus_space_handle_t	io_bh;

	struct resource	*	irq_res;
	int			irq_rid;
	void *			irq_ih;

	struct resource *	mem_res;
	int			mem_rid;
	bus_space_tag_t		mem_bt;
	bus_space_handle_t	mem_bh;

	u_int	 port;		/* i/o base address for this interface */
	caddr_t	 iomem;		/* memory size */
	caddr_t	 iomembot;	/* memory base address */
	unsigned iosize;
	int	 bus_use;	/* 0 means 16bit, 1 means 8 bit adapter */

	int	 want_mcsetup;
	int	 promisc;
	int	 nframes;
	int	 nrxbufs;
	int	 ntxbufs;
	volatile struct ie_int_sys_conf_ptr *iscp;
	volatile struct ie_sys_ctl_block *scb;
	volatile struct ie_recv_frame_desc **rframes;	/* nframes worth */
	volatile struct ie_recv_buf_desc **rbuffs;	/* nrxbufs worth */
	volatile u_char **cbuffs;			/* nrxbufs worth */
	int	 rfhead, rftail, rbhead, rbtail;

	volatile struct ie_xmit_cmd **xmit_cmds;	/* ntxbufs worth */
	volatile struct ie_xmit_buf **xmit_buffs;	/* ntxbufs worth */
	volatile u_char	 **xmit_cbuffs;			/* ntxbufs worth */
	int	 xmit_count;

	struct	 ie_en_addr mcast_addrs[MAXMCAST + 1];
	int	 mcast_count;

	u_short	 irq_encoded;	/* encoded interrupt on IEE16 */
};
#define PORT(sc)	sc->port
#define MEM(sc)		sc->iomem

void            ie_intr			(void *);
int		ie_alloc_resources	(device_t);
void		ie_release_resources	(device_t);
int		ie_attach		(device_t);
int		ie_detach		(device_t);

void		el_reset_586		(struct ie_softc *);
void		el_chan_attn		(struct ie_softc *);

void		sl_reset_586		(struct ie_softc *);
void		sl_chan_attn		(struct ie_softc *);

void		ee16_reset_586		(struct ie_softc *);
void		ee16_chan_attn		(struct ie_softc *);

void		sl_read_ether		(struct ie_softc *, unsigned char *);
int		check_ie_present	(struct ie_softc *);

