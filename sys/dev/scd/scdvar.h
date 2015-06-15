/*
 * $FreeBSD$
 */

struct scd_mbx {
	short		retry;
	short		nblk;
	int		sz;
	u_long		skip;
	struct bio *	bp;
	short		count;
};

struct scd_data {
	char			double_speed;
	char *			name;
	short			flags;
	int			blksize;
	u_long			disksize;
	int			openflag;
	struct {
		unsigned int	adr :4;
		unsigned int	ctl :4; /* xcdplayer needs this */
		unsigned char	start_msf[3];
	} toc[MAX_TRACKS];
	short			first_track;
	short			last_track;
	struct  ioc_play_msf	last_play;

	short			audio_status;
	struct bio_queue_head	head;	     /* head of bio queue */
	struct scd_mbx		mbx;
};

struct scd_softc {
	device_t		dev;
	struct cdev *scd_dev_t;
	int			debug;

	struct resource *	port;
	int			port_rid;
	int			port_type;

	struct mtx		mtx;

	struct callout		timer;
	int			ch_state;
	struct scd_mbx *	ch_mbxsave;

	struct scd_data		data;
};

#define	SCD_LOCK(_sc)		mtx_lock(&_sc->mtx)
#define	SCD_UNLOCK(_sc)		mtx_unlock(&_sc->mtx)
#define	SCD_ASSERT_LOCKED(_sc)	mtx_assert(&_sc->mtx, MA_OWNED)

#define	SCD_READ(_sc, _reg) \
	bus_read_1(_sc->port, _reg)
#define	SCD_READ_MULTI(_sc, _reg, _addr, _count) \
	bus_read_multi_1(_sc->port, _reg, _addr, _count)
#define	SCD_WRITE(_sc, _reg, _val) \
	bus_write_1(_sc->port, _reg, _val)

int	scd_probe	(struct scd_softc *);
int	scd_attach	(struct scd_softc *);
