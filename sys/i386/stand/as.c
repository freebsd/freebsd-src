/*
 * sys/i386/stand/as.c
 *
 *	from: 386BSD 0.1
 *	$Id: as.c,v 1.2 1993/10/16 18:49:19 rgrimes Exp $
 * Standalone driver for Adaptech 1542 SCSI
 * 
 * Pace Willisson        pace@blitz.com       April 8, 1992
 */

#include "param.h"
#include "disklabel.h"
#include "i386/isa/asreg.h"
#include "saio.h"

#ifdef ASDEBUG
#define ASPRINT(x) { printf x; DELAY (10000); }
#else
#define ASPRINT(x)
#endif

#define NRETRIES 3

int as_port = 0x330;

struct mailbox_entry mailbox[2];

int
asopen(io)
struct iob *io;
{
        struct disklabel *dd;
	char cdb[6];
	char data[12];
	int val;
	int oval;
	int i;
	struct iob aio;

	if (io->i_unit < 0 || io->i_unit > 8
	    || io->i_part < 0 || io->i_part > 8
	    || io->i_ctlr < 0 || io->i_ctlr > 0)
		return (-1);

	/* dma setup: see page 5-31 in the Adaptech manual */
	outb (0xd6, 0xc1);
	outb (0xd4, 0x01);

	ASPRINT (("resetting adaptech card... "));

	outb (as_port + AS_CONTROL, AS_CONTROL_SRST);

	/* delay a little */
	for (i = 0; i < 100; i++)
		inb (0x84);
	
	while (inb (as_port + AS_STATUS) != (AS_STATUS_INIT | AS_STATUS_IDLE))
		;

	ASPRINT (("reset ok "));

	as_put_byte (AS_CMD_MAILBOX_INIT);
	as_put_byte (1); /* one mailbox out, one in */
	as_put_byte ((int)mailbox >> 16);
	as_put_byte ((int)mailbox >> 8);
	as_put_byte ((int)mailbox);

	while (inb (as_port + AS_STATUS) & AS_STATUS_INIT)
		;

	ASPRINT (("mailbox init ok "));

	/* do mode select to set the logical block size */
	bzero (cdb, 6);
	cdb[0] = 0x15; /* MODE SELECT */
	cdb[4] = 12; /* parameter list length */

	bzero (data, 12);
	data[3] = 8; /* block descriptor length */
	data[9] = DEV_BSIZE >> 16;
	data[10] = DEV_BSIZE >> 8;
	data[11] = DEV_BSIZE;

	if (ascmd (io->i_unit, 0, cdb, 6, data, 12, 1) < 0) {
		printf ("as%d: error setting logical block size\n",
			io->i_unit);
		return (-1);
	}

	aio = *io;
	aio.i_bn = LABELSECTOR;
	aio.i_cc = DEV_BSIZE;
	/*io->i_ma = buf;*/
	aio.i_boff = 0;

#ifdef was
	if (asstrategy (&aio, F_READ) == DEV_BSIZE) {
		dd = (struct disklabel *)aio.i_ma;
		io->i_boff = dd->d_partitions[io->i_part].p_offset;
		ASPRINT (("partition offset %d ", io->i_boff));
	}
#else
{
extern struct disklabel disklabel;
		io->i_boff = disklabel.d_partitions[io->i_part].p_offset;
		ASPRINT (("partition offset %d ", io->i_boff));
}
#endif

	ASPRINT (("asopen ok "));
	return(0);
}

/* func is F_WRITE or F_READ
 * io->i_unit, io->i_part, io->i_bn is starting block
 * io->i_cc is byte count
 * io->i_ma is memory address
 * io->i_boff is block offset for this partition (set up in asopen)
 */
int
asstrategy(io, func)
struct iob *io;
{
	char cdb[6];
	int blkno;
	int retry;

	ASPRINT (("asstrategy(target=%d, block=%d+%d, count=%d) ",
		  io->i_unit, io->i_bn, io->i_boff, io->i_cc));

	if (func == F_WRITE) {
		printf ("as%d: write not supported\n", io->i_unit);
		return (0);
	}

	if (io->i_cc == 0)
		return (0);

	if (io->i_cc % DEV_BSIZE != 0) {
		printf ("as%d: transfer size not multiple of %d\n",
			io->i_unit, DEV_BSIZE);
		return (0);
	}

	/* retry in case we get a unit-attention error, which just
	 * means the drive has been reset since the last command
	 */
	for (retry = 0; retry < NRETRIES; retry++) {
		blkno = io->i_bn + io->i_boff;

		cdb[0] = 8; /* scsi read opcode */
		cdb[1] = (blkno >> 16) & 0x1f;
		cdb[2] = blkno >> 8;
		cdb[3] = blkno;
		cdb[4] = io->i_cc / DEV_BSIZE;
		cdb[5] = 0; /* control byte (used in linking) */

		if (ascmd (io->i_unit, 1, cdb, 6, io->i_ma, io->i_cc,
			   retry == NRETRIES - 1) >= 0) {
			ASPRINT (("asstrategy ok "));
			return (io->i_cc);
		}
	}

	ASPRINT (("asstrategy failed "));
	return (0);
}

int
ascmd (target, readflag, cdb, cdblen, data, datalen, printerr)
int target;
int readflag;
char *cdb;
int cdblen;
char *data;
int datalen;
int printerr;
{
	struct ccb ccb;
	int physaddr;
	unsigned char *sp;
	int i;

	if (mailbox[0].cmd != 0)
		/* this can't happen, unless the card flakes */
		_stop ("asstart: mailbox not available\n");

	bzero (&ccb, sizeof ccb);

	ccb.ccb_opcode = 0;
	ccb.ccb_addr_and_control = target << 5;
	if (datalen != 0)
		ccb.ccb_addr_and_control |= readflag ? 8 : 0x10;
	else
		ccb.ccb_addr_and_control |= 0x18;

	ccb.ccb_data_len_msb = datalen >> 16;
	ccb.ccb_data_len_mid = datalen >> 8;
	ccb.ccb_data_len_lsb = datalen;

	ccb.ccb_requst_sense_allocation_len = MAXSENSE;

	physaddr = (int)data;
	ccb.ccb_data_ptr_msb = physaddr >> 16;
	ccb.ccb_data_ptr_mid = physaddr >> 8;
	ccb.ccb_data_ptr_lsb = physaddr;

	ccb.ccb_scsi_command_len = cdblen;
	bcopy (cdb, ccb.ccb_cdb, cdblen);

#ifdef ASDEBUG
	printf ("ccb: ");
	for (i = 0; i < 48; i++)
		printf ("%x ", ((unsigned char *)&ccb)[i]);
	printf ("\n");
	/*getchar ();*/
#endif

	physaddr = (int)&ccb;
	mailbox[0].msb = physaddr >> 16;
	mailbox[0].mid = physaddr >> 8;
	mailbox[0].lsb = physaddr;
	mailbox[0].cmd = 1;
	
	/* tell controller to look in its mailbox */
	outb (as_port + AS_CONTROL, AS_CONTROL_IRST);
	as_put_byte (AS_CMD_START_SCSI_COMMAND);

	/* wait for status */
	ASPRINT (("waiting for status..."));
	while (mailbox[1].cmd == 0)
		;
	mailbox[1].cmd = 0;


	if (ccb.ccb_host_status != 0 || ccb.ccb_target_status != 0) {
#ifdef ASDEBUG
		printerr = 1;
#endif
		if (printerr) {
			printf ("as%d error: hst=%x tst=%x sense=",
				target,
				ccb.ccb_host_status,
				ccb.ccb_target_status);
			sp = ccb_sense (&ccb);
			for (i = 0; i < 8; i++)
				printf ("%x ", sp[i]);
			printf ("\n");
#ifdef ASDEBUG
			/*getchar ();*/
#endif
		}
		return (-1);
	}
	
	ASPRINT (("ascmd ok "));

	return (0);
}

int
as_put_byte (val)
int val;
{
	while (inb (as_port + AS_STATUS) & AS_STATUS_CDF)
		;
	outb (as_port + AS_DATA_OUT, val);
}
		
