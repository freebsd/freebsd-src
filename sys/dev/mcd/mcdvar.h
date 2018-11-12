/*
 * $FreeBSD$
 */

struct mcd_mbx {
	short		retry;
	short		nblk;
	int		sz;
	u_long		skip;
	struct bio *	bp;
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
	int			partflags;
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
	struct cdev *mcd_dev_t;
	int			debug;

	struct resource *	port;
	int			port_rid;
	int			port_type;

	struct resource *	irq;
	int			irq_rid;
	int			irq_type;
	void *			irq_ih;

	struct resource *	drq;
	int			drq_rid;
	int			drq_type;

	struct mtx		mtx;

	struct callout		timer;
	int			ch_state;
	struct mcd_mbx *	ch_mbxsave;

	struct mcd_data		data;
};

#define	MCD_LOCK(_sc)		mtx_lock(&_sc->mtx)
#define	MCD_UNLOCK(_sc)		mtx_unlock(&_sc->mtx)
#define	MCD_ASSERT_LOCKED(_sc)	mtx_assert(&_sc->mtx, MA_OWNED)

#define	MCD_READ(_sc, _reg)		bus_read_1(_sc->port, _reg)
#define	MCD_WRITE(_sc, _reg, _val)	bus_write_1(_sc->port, _reg, _val)

int	mcd_probe	(struct mcd_softc *);
int	mcd_attach	(struct mcd_softc *);
