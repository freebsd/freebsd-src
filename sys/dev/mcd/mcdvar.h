/*
 * $FreeBSD$
 */

struct mcd_mbx {
	short		unit;
	short		port;
	short		retry;
	short		nblk;
	int		sz;
	u_long		skip;
	struct bio *	bp;
	int		p_offset;
	short		count;
	short		mode;
};

struct mcd_data {
	short			type;
	char *			name;
	short			config;
	short			flags;
	u_char			read_command;
	short			status;
	int			blksize;
	u_long			disksize;
	int			iobase;
	struct disklabel	dlabel;
	int			partflags[MAXPARTITIONS];
	int			openflags;
	struct mcd_volinfo	volinfo;   
	struct mcd_qchninfo	toc[MCD_MAXTOCS]; 
	short			audio_status;
	short			curr_mode;
	struct mcd_read2	lastpb;
	short			debug;
	struct bio_queue_head	head;	     /* head of bio queue */
	struct mcd_mbx		mbx;
};

struct mcd_softc {
	device_t		dev;
	dev_t			mcd_dev_t[3];
	int			debug;

	struct resource *	port;
	int			port_rid;
	int			port_type;
	bus_space_tag_t		port_bst;
	bus_space_handle_t	port_bsh;

	struct resource *	irq;
	int			irq_rid;
	int			irq_type;
	void *			irq_ih;

	struct resource *	drq;
	int			drq_rid;
	int			drq_type;

	struct mtx		mtx;

	struct callout_handle	ch;
	int			ch_state;
	struct mcd_mbx *	ch_mbxsave;

	struct mcd_data		data;
};

#define	MCD_LOCK(_sc)		splx(&(_sc)->mtx
#define	MCD_UNLOCK(_sc)		splx(&(_sc)->mtx

#define	MCD_READ(_sc, _reg) \
	bus_space_read_1(_sc->port_bst, _sc->port_bsh, _reg)
#define	MCD_WRITE(_sc, _reg, _val) \
	bus_space_write_1(_sc->port_bst, _sc->port_bsh, _reg, _val)

int	mcd_probe	(struct mcd_softc *);
int	mcd_attach	(struct mcd_softc *);
