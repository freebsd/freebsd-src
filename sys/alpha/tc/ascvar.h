/* $FreeBSD: src/sys/alpha/tc/ascvar.h,v 1.3 1999/10/05 20:46:55 n_hibma Exp $ */
/*	$NetBSD: ascvar.h,v 1.4 1997/11/28 18:23:40 mhitch Exp $	*/


/*
 * State kept for each active SCSI device.
 */
struct script;

typedef struct scsi_state {
	struct script *script;	/* saved script while processing error */
	int	statusByte;	/* status byte returned during STATUS_PHASE */
	int	error;		/* errno to pass back to device driver */
	u_char	*dmaBufAddr;	/* DMA buffer address */
	int	dmalen;		/* amount to transfer in this chunk */
	int	dmaresid;	/* amount not transfered if chunk suspended */
	int	buflen;		/* total remaining amount of data to transfer */
	char	*buf;		/* current pointer within scsicmd->buf */
	int	flags;		/* see below */
	int	msglen;		/* number of message bytes to read */
	int	msgcnt;		/* number of message bytes received */
	u_char	sync_period;	/* DMA synchronous period */
	u_char	sync_offset;	/* DMA synchronous xfer offset or 0 if async */
	u_char	msg_out;	/* next MSG_OUT byte to send */
	u_char	msg_in[16];	/* buffer for multibyte messages */
} State;

/* state flags */
#define DISCONN		0x001	/* true if currently disconnected from bus */
#define DMA_IN_PROGRESS	0x002	/* true if data DMA started */
#define DMA_IN		0x004	/* true if reading from SCSI device */
#define DMA_RESUME	0x08	/* true if DMA was interrupted by disc. */
#define DMA_OUT		0x010	/* true if writing to SCSI device */
#define DID_SYNC	0x020	/* true if synchronous offset was negotiated */
#define TRY_SYNC	0x040	/* true if try neg. synchronous offset */
#define PARITY_ERR	0x080	/* true if parity error seen */
#define CHECK_SENSE	0x100	/* true if doing sense command */


/*
 * State kept for each active SCSI host interface (53C94).
 */

struct asc_softc {
	device_t sc_dev;			/* us as a device */
	asc_regmap_t	*regs;		/* chip address */
	volatile int	*dmar;		/* DMA address register address */
	int		sc_id;		/* SCSI ID of this interface */
	int		myidmask;	/* ~(1 << myid) */
	int		state;		/* current SCSI connection state */
	int		target;		/* target SCSI ID if busy */
	struct script	*script;	/* next expected interrupt & action */
	ScsiCmd		*cmd[ASC_NCMD];	/* active command indexed by SCSI ID */
	State		st[ASC_NCMD];	/* state info for each active command */
	/* Start dma routine */
	int  (*dma_start) __P((struct asc_softc *asc,
				struct scsi_state *state,
				caddr_t cp, int flag, int len, int off));
	/* End dma routine */
	void	(*dma_end) __P((struct asc_softc *asc,
				struct scsi_state *state, int flag));

	u_char		*dma_next;
	int		dma_xfer;	/* Dma len still to go */
	int		min_period;	/* Min transfer period clk/byte */
	int		max_period;	/* Max transfer period clk/byte */
	int		ccf;		/* CCF, whatever that really is? */
	int		timeout_250;	/* 250ms timeout */
	int		tb_ticks;	/* 4ns. ticks/tb channel ticks */
#ifdef USE_NEW_SCSI
	struct scsipi_link sc_link;		/* scsipi link struct */
#endif
};
typedef struct asc_softc *asc_softc_t;

#define	ASC_STATE_IDLE		0	/* idle state */
#define	ASC_STATE_BUSY		1	/* selecting or currently connected */
#define ASC_STATE_TARGET	2	/* currently selected as target */
#define ASC_STATE_RESEL		3	/* currently waiting for reselect */


#define ASC_SPEED_25_MHZ	250
#define ASC_SPEED_12_5_MHZ	125

void	ascattach __P((struct asc_softc *asc, int bus_speed));
int	asc_intr __P ((void *asc));

/*
 * Dma operations.
 */
#define	ASCDMA_READ	1
#define	ASCDMA_WRITE	2
