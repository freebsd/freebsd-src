/*
 * (Free/Net/386)BSD ST01/02, Future Domain TMC-885, TMC-950 SCSI driver for
 * Julians SCSI-code
 *
 * Copyright 1994, Kent Palmkvist (kentp@isy.liu.se)
 * Copyright 1994, Robert Knier (rknier@qgraph.com) 
 * Copyright 1992, 1994 Drew Eckhardt (drew@colorado.edu)
 * Copyright 1994, Julian Elischer (julian@tfs.com)
 *
 * Others that has contributed by example code is
 * 		Glen Overby (overby@cray.com)
 *		Tatu Yllnen
 *		Brian E Litzinger
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPERS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPERS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 *
 * kentp  940307 alpha version based on newscsi-03 version of Julians SCSI-code
 * kentp  940314 Added possibility to not use messages
 * rknier 940331 Added fast transfer code 
 * rknier 940407 Added assembler coded data transfers 
 *
 * $Id: seagate.c,v 1.3 1994/06/16 13:26:14 sean Exp $
 */

/*
 * What should really be done:
 * 
 * Add missing tests for timeouts
 * Restructure interrupt enable/disable code (runs to long with int disabled)
 * Find bug? giving problem with tape status
 * Add code to handle Future Domain 840, 841, 880 and 881
 * adjust timeouts (startup is very slow)
 * add code to use tagged commands in SCSI2
 * Add code to handle slow devices better (sleep if device not disconnecting)
 * Fix unnecessary interrupts
 */

/* Note to users trying to share a disk between DOS and unix:
 * The ST01/02 is a translating host-adapter. It is not giving DOS
 * the same number of heads/tracks/sectors as specified by the disk.
 * It is therefore important to look at what numbers DOS thinks the
 * disk has. Use these to disklabel your disk in an appropriate manner
 */
 
#include <sys/types.h>

#ifdef  KERNEL			/* don't laugh.. look for main() */
#include <sea.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <i386/isa/isa_device.h>
#endif /* KERNEL */
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#ifdef  KERNEL
#include "ddb.h"
#include "kernel.h"
#else /* KERNEL */
#define NSEA 1
#endif /* KERNEL */

extern int	hz;

#define	SEA_SCB_MAX	8	/* allow maximally 8 scsi control blocks */
#define SCB_TABLE_SIZE	8	/* start with 8 scb entries in table */
#define BLOCK_SIZE	512	/* size of READ/WRITE areas on SCSI card */

/*
 * defining PARITY causes parity data to be checked
 */
#define PARITY	1

/*
 * defining SEA_BLINDTRANSFER will make DATA IN and DATA OUT to be done with
 * blind transfers, i.e. no check is done for scsi phase changes. This will
 * result in data loss if the scsi device does not send its data using
 * BLOCK_SIZE bytes at a time.
 * If SEA_BLINDTRANSFER defined and SEA_ASSEMBLER also defined will result in
 * the use of blind transfers coded in assembler. SEA_ASSEMBLER is no good
 * without SEA_BLINDTRANSFER defined.
 */
#define SEA_BLINDTRANSFER	1	/* do blind transfers */
#define SEA_ASSEMBLER	1	/* Use assembly code for fast transfers */

/*
 * defining SEANOMSGS causes messages not to be used (thereby disabling
 * disconnects)
 */
/* #define SEANOMSGS 1 */

/*
 * defining SEA_NODATAOUT makes dataout phase being aborted
 */
/* #define SEA_NODATAOUT	1 */

/*
 * defining SEA_SENSEFIRST make REQUEST_SENSE opcode to be placed first
 */
/* #define SEA_SENSEFIRST       1 */

#define SEA_FREEBSD11	1	/* intermediate def. for FreeBSD 1.1 BETA */
				/* timeout function has changed */

/* Debugging definitions. Should not be used unless you want a lot of
   printouts even under normal conditions */

/* #define SEADEBUG	1 */	/* General info about errors */
/* #define SEADEBUG1	1 */	/* Info about internal results and errors */
/* #define SEADEBUG2	1 */	/* Display a lot about timeouts etc */
/* #define SEADEBUG3	1 */
/* #define SEADEBUG4	1 */
/* #define SEADEBUG5	1 */
/* #define SEADEBUG6	1 */	/* Display info about queue-lengths */
/* #define SEADEBUG7	1 */	/* Extra check on STATUS before phase check */
/* #define SEADEBUG8	1 */	/* Disregard non-BSY state in
				   sea_information_transfer */
/* #define SEADEBUG9	1 */	/* Enable printouts */
/* #define SEADEBUG11	1 */	/* stop everything except access to scsi id 1 */
/* #define SEADEBUG15	1 */	/* Display every byte sent/received */

#define NUM_CONCURRENT  1	/* number of concurrent ops per board */

/******************************* board definitions **************************/
/*
 * CONTROL defines
 */

#define CMD_RST		0x01		/* scsi reset */
#define CMD_SEL		0x02		/* scsi select */
#define CMD_BSY		0x04		/* scsi busy */
#define	CMD_ATTN	0x08		/* scsi attention */
#define CMD_START_ARB	0x10		/* start arbitration bit */
#define	CMD_EN_PARITY	0x20		/* enable scsi parity generation */
#define CMD_INTR	0x40		/* enable scsi interrupts */
#define CMD_DRVR_ENABLE	0x80		/* scsi enable */

/*
 * STATUS
 */

#define STAT_BSY	0x01		/* scsi busy */
#define STAT_MSG	0x02		/* scsi msg */
#define STAT_IO		0x04		/* scsi I/O */
#define STAT_CD		0x08		/* scsi C/D */
#define STAT_REQ	0x10		/* scsi req */
#define STAT_SEL	0x20		/* scsi select */
#define STAT_PARITY	0x40		/* parity error bit */
#define STAT_ARB_CMPL	0x80		/* arbitration complete bit */

/*
 * REQUESTS
 */

#define REQ_MASK	(STAT_CD | STAT_IO | STAT_MSG)
#define REQ_DATAOUT	0
#define REQ_DATAIN	STAT_IO
#define REQ_CMDOUT	STAT_CD
#define REQ_STATIN	(STAT_CD | STAT_IO)
#define REQ_MSGOUT	(STAT_MSG | STAT_CD)
#define REQ_MSGIN	(STAT_MSG | STAT_CD | STAT_IO)

#define REQ_UNKNOWN	0xff

#define SEAGATERAMOFFSET	0x00001800

#ifdef PARITY
	#define BASE_CMD (CMD_EN_PARITY | CMD_INTR)
#else
	#define BASE_CMD (CMD_INTR)
#endif

#define	SEAGATE	1
#define FD	2

/******************************************************************************
 *	This should be placed in a more generic file (presume in /sys/scsi)
 *	Message codes:
 */
#define MSG_ABORT	0x06
#define MSG_NOP		0x08
#define MSG_COMMAND_COMPLETE	0x00
#define	MSG_DISCONNECT	0x04
#define MSG_IDENTIFY	0x80
#define MSG_BUS_DEV_RESET	0x0c
#define	MSG_MESSAGE_REJECT	0x07
#define MSG_SAVE_POINTERS	0x02
#define MSG_RESTORE_POINTERS	0x03
/******************************************************************************/

#define IDENTIFY(can_disconnect,lun)	(MSG_IDENTIFY | ((can_disconnect) ? \
                                               0x40 : 0) | ((lun) & 0x07))

/* scsi control block used to keep info about a scsi command */ 
struct sea_scb
{
	int	flags;			/* status of the instruction */
#define	SCB_FREE	0
#define	SCB_ACTIVE	1
#define SCB_ABORTED	2
#define SCB_TIMEOUT	4
#define SCB_ERROR	8
#define	SCB_TIMECHK	16		/* We have set a timeout on this one */
	struct sea_scb *next;		/* in free list */
	struct scsi_xfer *xfer;		/* the scsi_xfer for this cmd */
        u_char * data;			/* position in data buffer so far */
	int32 datalen;			/* bytes remaining to transfer */;
};

/*
 * data structure describing current status of the scsi bus. One for each
 * controller card.
 */
struct	sea_data
{
	caddr_t	basemaddr;	/* Base address for card */
	char	ctrl_type;	/* FD or SEAGATE */
	caddr_t	st0x_cr_sr;	/* Address of control and status register */
	caddr_t	st0x_dr;	/* Address of data register */
	u_short	vect;		/* interrupt vector for this card */
	int	our_id;		/* our scsi id */
	int	numscb;		/* number of scsi control blocks */
	struct scsi_link sc_link;	/* struct connecting different data */
	struct sea_scb *connected;	/* currently connected command */
	struct sea_scb *issue_queue;	/* waiting to be issued */
	struct sea_scb *disconnected_queue;	/* waiting to reconnect */
	struct sea_scb scbs[SCB_TABLE_SIZE];
	struct sea_scb *free_scb;	/* free scb list */
	volatile unsigned char busy[8];	/* index=target, bit=lun, Keep track of
					   busy luns at device target */
} *seadata[NSEA];

/* flag showing if main routine is running. */
static volatile int main_running = 0;

#define	STATUS	(*(volatile unsigned char *) sea->st0x_cr_sr)
#define CONTROL	STATUS
#define DATA	(*(volatile unsigned char *) sea->st0x_dr)

/*
 * These are "special" values for the tag parameter passed to sea_select
 * Not implemented right now.
 */

#define TAG_NEXT	-1	/* Use next free tag */
#define TAG_NONE	-2	/*
				 * Establish I_T_L nexus instead of I_T_L_Q
				 * even on SCSI-II devices.
				 */

typedef struct {
	char *signature ;
	unsigned offset;
	unsigned length;
	unsigned char type;
} BiosSignature;

/*
 * Signatures for automatic recognition of board type
 */

static const BiosSignature signatures[] = {
{"ST01 v1.7  (C) Copyright 1987 Seagate", 15, 37, SEAGATE},
{"SCSI BIOS 2.00  (C) Copyright 1987 Seagate", 15, 40, SEAGATE},

/*
 * The following two lines are NOT mistakes. One detects ROM revision
 * 3.0.0, the other 3.2. Since seagate has only one type of SCSI adapter,
 * and this is not going to change, the "SEAGATE" and "SCSI" together
 * are probably "good enough"
 */

{"SEAGATE SCSI BIOS ", 16, 17, SEAGATE},
{"SEAGATE SCSI BIOS ", 17, 17, SEAGATE},

 /*
  * However, future domain makes several incompatible SCSI boards, so specific
  * signatures must be used.
  */

 {"FUTURE DOMAIN CORP. (C) 1986-1989 V5.0C2/14/89", 5, 45, FD},
 {"FUTURE DOMAIN CORP. (C) 1986-1989 V6.0A7/28/89", 5, 46, FD},
 {"FUTURE DOMAIN CORP. (C) 1986-1990 V6.0105/31/90",5, 47, FD},
 {"FUTURE DOMAIN CORP. (C) 1986-1990 V6.0209/18/90",5, 47, FD},
 {"FUTURE DOMAIN CORP. (C) 1986-1990 V7.009/18/90", 5, 46, FD},
 {"FUTURE DOMAIN CORP. (C) 1992 V8.00.004/02/92",   5, 44, FD},
 {"FUTURE DOMAIN TMC-950",			    5, 21, FD},
 };

#define NUM_SIGNATURES (sizeof(signatures) / sizeof(BiosSignature))

static const char * seagate_bases[] = {
	(char *) 0xc8000, (char *) 0xca000, (char *) 0xcc000,
	(char *) 0xce000, (char *) 0xdc000, (char *) 0xde000
};

#define NUM_BASES (sizeof(seagate_bases)/sizeof(char *))

int	sea_probe(struct isa_device *dev);
int	sea_attach(struct isa_device *dev);
int	seaintr(int unit);
int32	sea_scsi_cmd(struct scsi_xfer *xs);
#ifdef SEA_FREEBSD11
void	sea_timeout(caddr_t, int);
#else
void	sea_timeout(struct sea_scb *scb);
#endif
void	seaminphys(struct buf *bp);
void	sea_done(int unit, struct sea_scb *scb);
u_int32 sea_adapter_info(int unit);
struct sea_scb *sea_get_scb(int unit, int flags);
void	sea_free_scb(int unit, struct sea_scb *scb, int flags);
static void sea_main(void);
static void sea_information_transfer(struct sea_data *sea);
int	sea_poll(int unit, struct scsi_xfer *xs, struct sea_scb *scb);
int	sea_init(int unit);
int	sea_send_scb(struct sea_data *sea, struct sea_scb *scb);
int	sea_reselect(struct sea_data *sea);
int	sea_select(struct sea_data *sea, struct sea_scb *scb);
int	sea_transfer_pio(struct sea_data *sea, u_char *phase, int32 *count,
                          u_char **data);
int	sea_abort(int unit, struct sea_scb *scb);

static	sea_unit = 0;
static	sea_slot = -1;	/* last found board seagate_bases address index */
#define	FAIL	1
#define SUCCESS	0

#ifdef KERNEL
struct	scsi_adapter	sea_switch =
{
	sea_scsi_cmd,
	seaminphys,
	0,
	0,
	sea_adapter_info,
	"sea",
	0,0
};

/* the below structure is so we have a default dev struct for our link struct */
struct scsi_device sea_dev = 
{
	NULL,		/* use default error handler */
	NULL,		/* have a queue, served by this */
	NULL,		/* have no async handler */
	NULL,		/* Use default 'done' routine */
	"sea",
	0,
	0,0
};

struct isa_driver seadriver =
{
	sea_probe,
	sea_attach,
	"sea"
};

#endif /* KERNEL */

#ifdef SEADEBUG6
void sea_queue_length()
{
  struct sea_scb *tmp;
  int length = 0;

  if(seadata[0]->connected)
    length = 1;
  for(tmp = seadata[0]->issue_queue; tmp != NULL; tmp = tmp->next, length++);
  for(tmp = seadata[0]->disconnected_queue ; tmp != NULL; tmp->next, length++);
  printf("length:%d ",length);
}
#endif

/***********************************************************************\
* Check if the device can be found at the port given and if so, detect	*
* the type of board. Set it up ready for further work. Takes the	*
* isa_dev structure from autoconf as an argument.			*
* Returns 1 if card recognized, 0 if errors				*
\***********************************************************************/
int
sea_probe(dev)
struct isa_device *dev;
{
  int j;
  int unit = sea_unit;
  struct sea_data *sea;
  dev->id_unit = unit;

#ifdef	SEADEBUG2
  printf("sea_probe ");
#endif

  /* find unit and check we have that many defined */
  if(unit >= NSEA) {
    printf("sea%d: unit number too high\n",unit);
    return(0);
  }
  dev->id_unit = unit;
#ifdef SEADEBUG2
  printf("unit: %d\n",unit);
  printf("dev_addr: 0x%lx\n",dev->id_maddr);
#endif
  /* allocate a storage area for us */

  if (seadata[unit]) {
    printf("sea%d: memory already allocated\n", unit);
    return(0);
  }
#ifdef SEADEBUG2
  printf("Before malloc\n");
#endif
  sea = malloc(sizeof(struct sea_data), M_TEMP, M_NOWAIT);
  if (!sea) {
    printf("sea%d: cannot malloc!\n", unit);
    return(0);
  }

#ifdef SEADEBUG2
  printf("after malloc\n");
  for(j=0;j<32767;j++);
#endif
  bzero(sea,sizeof(struct sea_data));
  seadata[unit] = sea;

  /* check for address if no one specified */
  sea->basemaddr = NULL;

  /* Could try to find a board by looking through all possible addresses */
  /* This is not done the right way now, because I have not found a way  */
  /* to get a boards virtual memory address given its physical. There is */
  /* a function that returns the physical address for a given virtual    */
  /* address, but not the other way around */

  if(dev->id_maddr == 0) {
/*
    for(sea_slot++;sea_slot<NUM_BASES;sea_slot++)
      for(j = 0; !sea->basemaddr && j < NUM_SIGNATURES; ++j)
	if(!memcmp((void *)(seagate_bases[sea_slot]+signatures[j].offset),
		   (void *) signatures[j].signature, signatures[j].length)) {
	  sea->basemaddr = (void *)seagate_bases[sea_slot];
	  break;
	}
*/
  } else {

#ifdef SEADEBUG2
    printf("id_maddr != 0\n");
    for(j = 0; j < 32767 ; j++);
    for(j = 0; j < 32767 ; j++);
#endif
    /* find sea_slot position for overridden memory address */ 
    for(j = 0; ((char *)vtophys(dev->id_maddr) != seagate_bases[j]) && 
		j<NUM_BASES; ++j);
    if(j == NUM_BASES) {
      printf("sea: board not expected at address 0x%lx\n",dev->id_maddr);
      seadata[unit]=NULL;
      free(sea, M_TEMP);
      return(0);
    } else if(sea_slot > j) {
      printf("sea: board address 0x%lx already probed!\n", dev->id_maddr);
      seadata[unit]=NULL;
      free(sea, M_TEMP);
      return(0);
    } else {
      sea->basemaddr = dev->id_maddr;
    }

  }
#ifdef	SEADEBUG2
  printf("sea->basemaddr = %lx\n", sea->basemaddr);
#endif
	
  /* check board type */	/* No way to define this through config */
  for(j = 0; j < NUM_SIGNATURES; j++)
    if(!memcmp((void *) (sea->basemaddr + signatures[j].offset),
	       (void *) signatures[j].signature, signatures[j].length)) {
      sea->ctrl_type = signatures[j].type;
      break;
    }
  if(j == NUM_SIGNATURES) {
    printf("sea: Board type unknown at address 0x%lx\n",
	   sea->basemaddr);
    seadata[unit]=NULL;
    free(sea, M_TEMP);
    return(0);
  }

  /* Find controller and data memory addresses */
  sea->st0x_cr_sr = (void *) (((unsigned char *) sea->basemaddr) +
			      ((sea->ctrl_type == SEAGATE) ? 0x1a00 : 0x1c00));
  sea->st0x_dr = (void *) (((unsigned char *) sea->basemaddr) +
			   ((sea->ctrl_type == SEAGATE) ? 0x1c00 : 0x1e00));

  /* Test controller RAM (works the same way on future domain cards?) */
  *(sea->basemaddr + SEAGATERAMOFFSET) = 0xa5;
  *(sea->basemaddr + SEAGATERAMOFFSET + 1) = 0x5a;

  if((*(sea->basemaddr + SEAGATERAMOFFSET) != (char) 0xa5) ||
     (*(sea->basemaddr + SEAGATERAMOFFSET + 1) != (char) 0x5a)) {
    printf("sea%d: Board RAM failure\n",unit);
  }
  
  if(sea_init(unit) != 0) {
    seadata[unit] = NULL;
    free(sea,M_TEMP);
    return(0);
  }
	
  /* if its there put in it's interrupt vector */
  /* (Doesn't use dma, so no drq is set) */
  sea->vect = dev->id_irq;

  sea_unit++;
  return(1);
}

/***********************************************\
* Attach all sub-devices we can find		*
\***********************************************/
int
sea_attach(dev)
     struct isa_device *dev;
{
  int unit = dev->id_unit;
  struct sea_data *sea = seadata[unit];
  
#ifdef SEADEBUG2
  printf("sea_attach called\n");
#endif

  /* fill in the prototype scsi_link */
  sea->sc_link.adapter_unit = unit;
  sea->sc_link.adapter_targ = sea->our_id;
  sea->sc_link.adapter = &sea_switch;
  sea->sc_link.device = &sea_dev;
  
  /*****************************************************\
  * ask the adapter what subunits are present		*
  \*****************************************************/
  scsi_attachdevs(&(sea->sc_link));
  return 1;
}

/***********************************************\
* Return some information to the caller about	*
* the adapter and its capabilities		*
\***********************************************/
u_int32
sea_adapter_info(unit)
     int	unit;
{
#ifdef SEADEBUG2
  printf("sea_adapter_info called\n");
#endif
  return 1;
}

/***********************************************\
* Catch an interrupt from the adaptor		*
\***********************************************/
int
seaintr(unit)
     int	unit;
{
  int	done;
  struct sea_data *sea = seadata[unit];
  int oldpri;

#if SEADEBUG2
  printf(";");
#endif

  do {
    done = 1;
    /* dispatch to appropriate routine if found and done=0 */
    /* should check to see that this card really caused the interrupt */
    if ((STATUS & (STAT_SEL | STAT_IO)) == (STAT_SEL | STAT_IO)) {
      /* Reselect interrupt */
#ifdef SEADEBUG2
      printf(";2");
#endif
      done = 0;
/*      enable_intr(); */ /* ?? How should this be done ?? */
      sea_reselect(sea);
    } else if (STATUS & STAT_PARITY) {
      /* Parity error interrupt */
#ifdef SEADEBUG2
      printf(";3");
#endif
      printf("sea%d: PARITY interrupt\n", unit);
    } else {
#ifdef SEADEBUG2
/* 		printf("sea%d: unknown interrupt\n",unit); */
      printf(";4%x", STATUS);
#endif
    }
    if (!done) {
      oldpri = splbio(); /* disable_intr(); */
      if (!main_running) {
#ifdef SEADEBUG2
	printf(";5");
#endif
	main_running = 1;
	sea_main();
	/* main_running is cleared in sea_main once it can't
	 * do more work, and sea_main exits with interrupts
	 * disabled
	 */
	splx(oldpri); /* enable_intr(); */
      } else {
	splx(oldpri); /* enable_intr(); */
      }
    }
  } while (!done);
  return 1;
}

/***********************************************\
* Setup data structures, and reset the board	*
* and the scsi bus				*
\***********************************************/
int
sea_init(unit)
     int unit;
{
  long l;
  int i;
  struct sea_data *sea = seadata[unit];
  
#ifdef SEADEBUG2
  printf("sea_init called\n");
#endif
/* Reset the scsi bus (I don't know if this is needed */
  CONTROL = BASE_CMD | CMD_DRVR_ENABLE | CMD_RST;
  DELAY(25);	/* hold reset for at least 25 microseconds */
  CONTROL = BASE_CMD;
  DELAY(10); 	/* wait a Bus Clear Delay (800 ns + bus free delay (800 ns) */
  /* Set our id (don't know anything about this) */
  if(sea->ctrl_type == SEAGATE)
    sea->our_id = 7;
  else
    sea->our_id = 6;
  /* init fields used by our routines */
  sea->connected = NULL;
  sea->issue_queue = NULL;
  sea->disconnected_queue = NULL;
  for (i=0; i<8 ; i++)
    sea->busy[i] = 0;

  /* link up the free list of scbs */
  sea->numscb = SCB_TABLE_SIZE;
  sea->free_scb = (struct sea_scb *) & (sea->scbs[0]);
  for(i=1;i< SCB_TABLE_SIZE ; i++) {
    sea->scbs[i-1].next = &(sea->scbs[i]);
  }
  sea->scbs[SCB_TABLE_SIZE - 1].next = NULL; 

  return(0);
}

/***********************************************\
*						*
\***********************************************/
void	seaminphys(bp)
     struct	buf *bp;
{
#ifdef SEADEBUG2
/*  printf("seaminphys called\n"); */
  printf(",");
#endif
}

/***********************************************\
* start a scsi operation given the command and	*
* the data address. Also needs the unit, target	*
* and lu					*
* get a free scb and set it up			*
* call send_scb					*
* either start timer or wait until done		*
\***********************************************/
int32	sea_scsi_cmd(xs)
struct	scsi_xfer *xs;
{
  struct scsi_sense_data *s1, *s2;
  struct sea_scb *scb;
  int	i = 0;
  int	flags;
  int	unit = xs->sc_link->adapter_unit;
  struct sea_data *sea = seadata[unit];
  int	s;
  unsigned int stat;
  int32	result;

#ifdef SEADEBUG2
  /* printf("scsi_cmd\n"); */
  printf("=");
#endif

#ifdef SEADEBUG11
  if(xs->sc_link->target != 1) {
    xs->flags |= ITSDONE;
    xs->error = XS_TIMEOUT;
    return(HAD_ERROR);
  }
#endif

  flags = xs->flags;
  if(xs->bp) flags |= (SCSI_NOSLEEP);
  if(flags & ITSDONE) {
    printf("sea%d: Already done?", unit);
    xs->flags &= ~ITSDONE;
  }
  if(!(flags & INUSE)) {
    printf("sea%d: Not in use?", unit);
    xs->flags |= INUSE;
  }
  if (!(scb = sea_get_scb(unit, flags))) {
#ifdef SEADEBUG2
    printf("=2");
#endif
    xs->error = XS_DRIVER_STUFFUP;
    return(TRY_AGAIN_LATER);
  }

  /*
   * Put all the arguments for the xfer in the scb
   */
  scb->xfer = xs;
  scb->datalen = xs->datalen;
  scb->data = xs->data;

  if(flags & SCSI_RESET) {
    /* Try to send a reset command to the card. This is done by calling the
     * Reset function. Should then return COMPLETE. Need to take care of the
     * possible current connected command.
     * Not implemented right now.
     */
    printf("sea%d: Got a SCSI_RESET!\n",unit);
  }

  /* setup the scb to contain necessary values */
  /* The interresting values can be read from the xs that is saved */
  /* I therefore think that the structure can be kept very small */
  /* the driver doesn't use DMA so the scatter/gather is not needed ? */
#ifdef SEADEBUG6
  sea_queue_length();
#endif
  if (sea_send_scb(sea, scb) == 0) {
#ifdef SEADEBUG2
    printf("=3");
#endif
    xs->error = XS_DRIVER_STUFFUP;
    sea_free_scb(unit, scb, flags);
    return (TRY_AGAIN_LATER);
  }
  
  /*
   * Usually return SUCCESSFULLY QUEUED
   */
  if (!(flags & SCSI_NOMASK)) {
    if(xs->flags & ITSDONE) {	/* timout timer not started, already finished */
      /* Tried to return COMPLETE but the machine hanged with this */
#ifdef SEADEBUG2
      printf("=6");
#endif
      return(SUCCESSFULLY_QUEUED);
    }
#ifdef SEA_FREEBSD11
    timeout(sea_timeout, (caddr_t)scb, (xs->timeout * hz) / 1000);
#else
    timeout(sea_timeout, scb, (xs->timeout * hz) / 1000);
#endif
    scb->flags |= SCB_TIMECHK;
#ifdef SEADEBUG2
    printf("=4");
#endif
    return(SUCCESSFULLY_QUEUED);
  }

  /*
   * If we can't use interrupts, poll on completion
   */
   
  result = sea_poll(unit, xs, scb);
#ifdef SEADEBUG2
  printf("=5 %lx", result);
#endif
  return result;
}

/*
 * Get a free scb. If there are none, see if we can allocate a new one. If so,
 * put it in the hash table too, otherwise return an error or sleep.
 */

struct sea_scb *
sea_get_scb(unit, flags)
	int unit;
	int flags;
{
  struct sea_data *sea = seadata[unit];
  unsigned opri = 0;
  struct sea_scb * scbp;
  int hashnum;

#ifdef SEADEBUG2
/*  printf("get_scb\n"); */
  printf("(");
#endif

  if (!(flags & SCSI_NOMASK))
    opri = splbio();

#ifdef SEADEBUG3
  printf("(2 %lx ", sea->free_scb);
#endif

  /*
   * If we can and have to, sleep waiting for one to come free
   * but only if we can´t allocate a new one.
   */
  while (!(scbp = sea->free_scb)) {
#ifdef SEADEBUG12
    printf("(3");		
#endif
    if (sea->numscb < SEA_SCB_MAX) {
      printf("malloced new scbs\n");
      if (scbp = (struct sea_scb *) malloc(sizeof(struct sea_scb),
					   M_TEMP, M_NOWAIT)) {
	bzero(scbp, sizeof(struct sea_scb));
	sea->numscb++;
	scbp->flags = SCB_ACTIVE;
	scbp->next = NULL; 
      } else {
	printf("sea%d: Can't malloc SCB\n",unit);
      }
      goto gottit;
    } else {
#ifdef SEADEBUG12
      printf("(4");
#endif
      if(!(flags & SCSI_NOSLEEP)) {
#ifdef SEADEBUG2
	printf("(5");
#endif
	tsleep(&sea->free_scb, PRIBIO, "seascb", 0);
      }
    }
  }
  if (scbp) {
#ifdef SEADEBUG2
    printf("(6");
#endif
    /* Get SCB from free list */
    sea->free_scb = scbp->next;
    scbp->next = NULL;
    scbp->flags = SCB_ACTIVE;
  }
 gottit:
  if (!(flags & SCSI_NOMASK))
    splx(opri);

  return(scbp);
}

/*
 * sea_send_scb
 *
 * Try to send this command to the board. Because this board does not use any
 * mailboxes, this routine simply adds the command to the queue held by the
 * sea_data structure.
 * A check is done to see if the command contains a REQUEST_SENSE command, and
 * if so the command is put first in the queue, otherwise the command is added
 * to the end of the queue. ?? Not correct ??
 */
int
sea_send_scb(struct sea_data *sea, struct sea_scb *scb)
{
  struct sea_scb *tmp;
  int oldpri = 0;

#ifdef SEADEBUG2
  printf("+");
#endif

  if(!(scb->xfer->flags & SCSI_NOSLEEP)) {
    oldpri = splbio();
  }
  
  /* add to head of queue if queue empty or command is REQUEST_SENSE */

  if (!(sea->issue_queue)
#ifdef SEA_SENSEFIRST
      || (scb->xfer->cmd->opcode == (u_char) REQUEST_SENSE)
#endif
      ) {
#ifdef SEADEBUG2
    printf("+2");
#endif
    scb->next = sea->issue_queue;
    sea->issue_queue = scb;
  } else {
#ifdef SEADEBUG2
    printf("+3");
#endif
    for (tmp = sea->issue_queue; tmp->next; tmp = tmp->next);
    tmp->next = scb;
    scb->next = NULL;	/* placed at the end of the queue */
  }
  /* Try to do some work on the card */
  if (!main_running) {
    main_running = 1;
    sea_main();
    /* main running is cleared in sea_main once it can't
     * do more work, and sea_main exits with interrupts
     * disabled
     */
  }
  if(!(scb->xfer->flags & SCSI_NOSLEEP)) {
    splx(oldpri);	
  }
  return (1);	/* No possible errors right now */
}

/*
 * sea_main(void)
 *
 * corroutine that runs as long as more work can be done on the seagate host
 * adapter in a system. Both sea_scsi_cmd and sea_intr will try to start it in
 * case it is not running.
 */

static void sea_main(void)
{
  struct sea_data *sea; /* This time we look at all cards */
  struct sea_scb *tmp, *prev;
  int done;
  int unit;
  int oldpri;

#ifdef SEADEBUG2
  printf(".");
#endif

  /*
   * This should not be run with interrupts disabled, but use the splx code
   * instead
   */
  do {
    done = 1;
    for (sea=seadata[unit=0]; (unit < NSEA) && seadata[unit] ;
        sea=seadata[++unit]) {
      oldpri = splbio();
      if (!sea->connected) {
#ifdef SEADEBUG2
	printf(".2");
#endif
	/*
	 * Search through the issue_queue for a command destined for a
	 * target that's not busy.
	 */
	for (tmp = sea->issue_queue, prev = NULL; tmp ;
	     prev = tmp, tmp = tmp->next)
	  /* When we find one, remove it from the issue queue. */
	  if (!(sea->busy[tmp->xfer->sc_link->target] &
		(1 << tmp->xfer->sc_link->lun))) {
	    if (prev)
	      prev->next = tmp->next;
	    else
	      sea->issue_queue = tmp->next;
	    tmp->next = NULL;
	    
	    /* re-enable interrupts after finding one */
	    splx(oldpri);
	    
	    /*
	     * Attempt to establish an I_T_L nexus here.
	     * On success, sea->connected is set.
	     * On failure, we must add the command back to
	     * the issue queue so we can keep trying.
	     */
#ifdef SEADEBUG2
	    printf(".3");
#endif
	    
	    /* REQUEST_SENSE commands are issued without tagged
	     * queueing, even on SCSI-II devices because the
	     * contingent alligence condition exists for the
	     * entire unit.
	     */

	    /* First check that if any device has tried a reconnect while
	     * we have done other things with interrupts disabled
	     */

	    if ((STATUS & (STAT_SEL | STAT_IO)) == (STAT_SEL | STAT_IO)) {
#ifdef SEADEBUG2
	      printf(".7");
#endif
	      sea_reselect(sea);
	      break;
	    }
	    if (!sea_select(sea, tmp)) {
#ifdef SEADEBUG2
	      /* printf("Select returned ok\n"); */
	      printf(".4");
#endif
	      break;
	    } else {
	      oldpri = splbio();
	      tmp->next = sea->issue_queue;
	      sea->issue_queue = tmp;
	      splx(oldpri);
	      printf("sea_main: select failed\n");
	    }
	  } /* if target/lun is not busy */
      } /* if (!sea->connected) */
      
      if (sea->connected) {	/* we are connected. Do the task */
	splx(oldpri);
#ifdef SEADEBUG2
/*	printf("sea_main: starting information transfer!\n"); */
	printf(".5");
#endif
	sea_information_transfer(sea);
#ifdef SEADEBUG2
/*	printf("sea_main: sea->connected:%lx\n", sea->connected); */
	printf(".6%lx ", sea->connected);
#endif
	done = 0;
      } else
	break;
    } /* for instance */
  } while (!done);
  main_running = 0;
}

void
sea_free_scb(unit, scb, flags)
     int	unit;
     struct sea_scb *scb;
     int	flags;
{
  struct sea_data *sea = seadata[unit];
  unsigned int opri = 0;

#ifdef SEADEBUG2
/*  printf("free_scb\n"); */
  printf(")");
#endif

  if(!(flags & SCSI_NOMASK))
    opri = splbio();

  scb->next = sea->free_scb;
  sea->free_scb = scb;
  scb->flags = SCB_FREE;
  /*
   * If there were none, wake anybody waiting for one to come free,
   * starting with queued entries.
   */
  if(!scb->next) {
#ifdef SEADEBUG2
/*  	printf("free_scb waking up sleep\n"); */
    printf(")2");
#endif
#ifdef SEA_FREEBSD11
    wakeup((caddr_t)&sea->free_scb);
#else
    wakeup(&sea->free_scb);
#endif
  }

  if (!(flags & SCSI_NOMASK))
    splx(opri);
}

#ifdef SEA_FREEBSD11
void
sea_timeout(caddr_t arg1, int arg2)
#else
void
sea_timeout(struct sea_scb *scb)
#endif
{
#ifdef SEA_FREEBSD11
  struct sea_scb *scb = (struct sea_scb *)arg1;
#endif
  int unit;
  struct sea_data *sea;
  int s=splbio();

#ifdef SEADEBUG2
/*  printf("sea_timeout called\n"); */
  printf(":");
#endif

  unit = scb->xfer->sc_link->adapter_unit;
  sea = seadata[unit];
#ifndef SEADEBUG	/* print message only if not waiting unless debug */
  if(!(scb->xfer->flags & SCSI_NOMASK))
#endif
    printf("sea%d:%d:%d (%s%d) timed out ", unit,
      scb->xfer->sc_link->target,
      scb->xfer->sc_link->lun,
      scb->xfer->sc_link->device->name,
      scb->xfer->sc_link->dev_unit);

  /*
   * If it has been through before, then
   * a previous abort has failed, don't
   * try abort again
   */
  if (/* (sea_abort(unit, scb) != 1) ||*/ (scb->flags & SCB_ABORTED)) {
    /*
     * abort timed out
     */
#ifdef SEADEBUG2
/*  	printf("sea%d: Abort Operation has timed out\n", unit); */
    printf(":2");
#endif
    scb->xfer->retries = 0;
    scb->flags |= SCB_ABORTED;
    sea_done(unit, scb);
  } else {
 #ifdef SEADEBUG2
 /* 	printf("sea%d: Try to abort\n", unit); */
    printf(":3");
 #endif
    sea_abort(unit, scb);
 /* 	sea_send_scb(sea, ~SCSI_NOMASK, SEA_SCB_ABORT, scb); */
    /* 2 seconds for the abort */
 #ifdef SEA_FREEBSD11
    timeout(sea_timeout, (caddr_t)scb, 2*hz);
 #else
    timeout(sea_timeout, scb, 2*hz);
 #endif
    scb->flags |= (SCB_ABORTED | SCB_TIMECHK);
  }
  splx(s);
}
 
int
sea_reselect(sea)
	struct sea_data *sea;
{
  unsigned char target_mask;
  long l;
  unsigned char lun, phase;
  unsigned char msg[3];
  int32 len;
  u_char *data;
  struct sea_scb *tmp = 0, *prev = 0;
  int abort = 0;
  
#if SEADEBUG2
/*  printf("sea_reselect called\n"); */
  printf("}");
#endif
  
  if (!((target_mask = STATUS) & STAT_SEL)) {
    printf("sea: wrong state 0x%x\n", target_mask);
    return(0);
  }
  /* wait for a device to win the reselection phase */
  /* signals this by asserting the I/O signal */
  for(l=10; l && (STATUS & (STAT_SEL | STAT_IO | STAT_BSY)) 
      != (STAT_SEL | STAT_IO | 0);l--);
  /* !! Check for timeout here */
  /* the data bus contains original initiator id ORed with target id */
  target_mask = DATA;
  /* see that we really are the initiator */
  if (!(target_mask & ((sea->ctrl_type == SEAGATE) ? 0x80 : 0x40))) {
    printf("sea: polled reselection was not for me: %x\n",target_mask);
    return(0);
  }
  /* find target who won */
  target_mask &= ((sea->ctrl_type == SEAGATE) ? ~0x80 : ~0x40);
  /* host responds by asserting the BSY signal */
  CONTROL = (BASE_CMD | CMD_DRVR_ENABLE | CMD_BSY);
  /* target should respond by deasserting the SEL signal */
  for(l=50000;l && (STATUS & STAT_SEL);l++);
  /* remove the busy status */
  CONTROL = (BASE_CMD | CMD_DRVR_ENABLE);
  /* we are connected. Now we wait for the MSGIN condition */
  for(l=50000; l && !(STATUS & STAT_REQ);l--);
  /* !! Add timeout check here */
  /* hope we get an IDENTIFY message */
  len = 3;
  data = msg;
  phase = REQ_MSGIN;
  sea_transfer_pio(sea, &phase, &len, &data); 

  if (!(msg[0] & 0x80)) {
    printf("sea: Expecting IDENTIFY message, got 0x%x\n", msg[0]);
    abort = 1;
  } else {
    lun = (msg[0] & 0x07);

    /*
     * Find the command corresponding to the I_T_L or I_T_L_Q nexus we
     * just restablished, and remove it from the disconnected queue.
     */

    for(tmp = sea->disconnected_queue, prev = NULL;
	tmp; prev=tmp, tmp = tmp->next)
      if((target_mask == (1 << tmp->xfer->sc_link->target)) &&
	 (lun == tmp->xfer->sc_link->lun)) {
	if(prev) {
#ifdef SEADEBUG2   			
	  printf("}2");
#endif
	  prev->next = tmp->next;
	} else {
#ifdef SEADEBUG2
	  printf("}3");
#endif
	  sea->disconnected_queue = tmp->next;
	}
	tmp->next = NULL;
	break;
      }
    if (!tmp) {
      printf("sea: warning : target %02x lun %d not in disconnect_queue\n",
	     target_mask, lun);
      /*
       * Since we have an established nexus that we can't do anything with,
       * we must abort it.
       */
      abort = 1;
    }
  }

  if(abort) {
#ifdef SEADEBUG2
    printf("}4");
#endif
    msg[0] = MSG_ABORT;
    len = 1;
    data = msg;
    phase = REQ_MSGOUT;
    CONTROL = (BASE_CMD | CMD_ATTN);
    sea_transfer_pio(sea, &phase, &len, &data);
  } else {
#ifdef SEADEBUG2
    printf("}5");
#endif
    sea->connected = tmp;
  }
  /* return value not used yet */
  return 0;
}

/* Transfer data in given phase using polled I/O
*/

int sea_transfer_pio(struct sea_data *sea, u_char *phase, int32 *count,
		     u_char **data)
{
  register unsigned char p = *phase, tmp;
  register int c = *count;
  register unsigned char *d = *data;
  unsigned long int timeout;

#if SEADEBUG2
/*  printf("sea_transfer_pio called: len:%x\n",c); */
  printf("-1 %x %x", c, p);
#endif

  do {
    /* wait for assertion of REQ, after which the phase bits will be valid */
    for(timeout = 0; timeout < 5000000L ; timeout++)
      if ((tmp = STATUS) & STAT_REQ)
	break;
    if (!(tmp & STAT_REQ)) {
      printf("sea_transfer_pio: timeout waiting for STAT_REQ\n");
      break;
    }

    /* check for phase mismatch */
    /* Reached if the target decides that it has finished the transfer */
    if ((tmp & REQ_MASK) != p) {
#ifdef SEADEBUG1
/*      printf("-2 %x", tmp); */
      printf("sea:pio phase mismatch:%x, want:%x, len:%x\n",tmp,p,c);
#endif
      break;
    }

    /* Do actual transfer from SCSI bus to/from memory */
    if (!(p & STAT_IO))
      DATA = *d;
    else
      *d = DATA;
#ifdef SEADEBUG15
    printf("-7%x", *d);
#endif
    ++d;

    /* The SCSI standard suggests that in MSGOUT phase, the initiator
     * should drop ATN on the last byte of the message phase
     * after REQ has been asserted for the handshake but before
     * the initiator raises ACK.
     * Don't know how to accomplish this on the ST01/02
     */
    /* We don't mind right now. */

    /* The st01 code doesn't wait for STAT_REQ to be deasserted. Is this ok? */
/*    for(timeout=0;timeout<200000L;timeout++)
      if(!(STATUS & STAT_REQ))
        break;
    if(STATUS & STAT_REQ)
      printf("timeout on wait for !STAT_REQ"); */
/*      printf("*"); */
  } while (--c);

  *count = c;
  *data = d;
  tmp = STATUS;
  if (tmp & STAT_REQ) {
#if SEADEBUG2
    printf("-3%x", tmp);
#endif
    *phase = tmp & REQ_MASK;
  } else {
#if SEADEBUG2
    printf("-4%x", tmp);
#endif
    *phase = REQ_UNKNOWN;
  }
  if (!c || (*phase == p)) {
#if SEADEBUG2
    printf("-5%x %x", c, *phase);
#endif
    return 0;
  } else {
#if SEADEBUG2
    printf("-6");
#endif
    return -1;
  }
}

/* sea_select
 * establish I_T_L or I_T_L_Q nexus for new or existing command
 * including ARBITRATION, SELECTION, and initial message out for IDENTIFY and
 * queue messages.
 * return -1 if selection could not execute for some reason, 0 if selection
 * succeded or failed because the taget did not respond
 */
int sea_select(struct sea_data *sea, struct sea_scb *scb)
{
  unsigned char tmp[3], phase;
  u_char *data;
  int32 len;
  unsigned long timeout;

#ifdef SEADEBUG2 
/*  printf("sea_select called\n"); */
  printf("{");
#endif

  CONTROL = BASE_CMD;
  DATA = ((sea->ctrl_type == SEAGATE) ? 0x80 : 0x40);
  CONTROL = (BASE_CMD & ~CMD_INTR) | CMD_START_ARB;
  /* wait for arbitration to complete */
  for (timeout = 0; timeout < 3000000L ; timeout++) {
    if (STATUS & STAT_ARB_CMPL)
      break;
  }
  if (!(STATUS & STAT_ARB_CMPL)) {
    if (STATUS & STAT_SEL) {
      printf("sea: arbitration lost\n");
      scb->flags |= SCB_ERROR;
    } else {
      printf("sea: arbitration timeout.\n");
      scb->flags |=SCB_TIMEOUT;
    }
    CONTROL = BASE_CMD;
    return(-1);
  }
  DELAY(2);

#if SEADEBUG2 
/*  printf("after arbitration: STATUS=%x\n", STATUS); */
  printf("{2 %x", STATUS);
#endif

  DATA = (unsigned char)((1 << scb->xfer->sc_link->target) |
			 ((sea->ctrl_type == SEAGATE) ? 0x80 : 0x40));
#ifdef SEANOMSGS
  CONTROL = (BASE_CMD & (~CMD_INTR))| CMD_DRVR_ENABLE | CMD_SEL;
#else
  CONTROL = (BASE_CMD & (~CMD_INTR)) | CMD_DRVR_ENABLE | CMD_SEL | CMD_ATTN;
#endif
  DELAY(1); 
  /* wait for a bsy from target */
  for (timeout = 0; timeout < 2000000L; timeout++) {
    if (STATUS & STAT_BSY)
      break;
  }

#if SEADEBUG2
/*  printf("after wait for BSY: STATUS=%x,count=%lx\n", STATUS, timeout); */
  printf("{3 %x %x", STATUS, timeout);
#endif
  
  if (!(STATUS & STAT_BSY)) {
    /* should return some error to the higher level driver */
    CONTROL = BASE_CMD;
#if SEADEBUG2
/*  	printf("sea: target did not respond\n"); */
    printf("{4");
#endif
    scb->flags |= SCB_TIMEOUT;
    return 0;
  }

  /* Try to make the target to take a message from us */
#ifdef SEANOMSGS
  CONTROL = (BASE_CMD & (~CMD_INTR)) | CMD_DRVR_ENABLE;
#else
  CONTROL = (BASE_CMD & (~CMD_INTR)) | CMD_DRVR_ENABLE | CMD_ATTN;
#endif

  DELAY(1);
  
  /* should start a msg_out phase */
  for (timeout = 0; timeout < 2000000L ; timeout++) {
    if (STATUS & STAT_REQ)
      break;
  }

  CONTROL = BASE_CMD | CMD_DRVR_ENABLE;

#if SEADEBUG2 || SEADEBUG9
/*  printf("after wait for STAT_REQ: STATUS=%x,count=%lx\n", STATUS, timeout);
  printf("2:nd try after wait for STAT_REQ: STATUS=%x\n", STATUS); */
  printf("{5%x", timeout);
#endif
  
  if (!(STATUS & STAT_REQ)) {
    /* This should not be taken as an error, but more like an unsupported
     * feature!
     * Should set a flag indicating that the target don't support messages, and
     * continue without failure. (THIS IS NOT AN ERROR!)
     */
#if SEADEBUG
/*    printf("{6"); */
  	printf("sea: WARNING: target %x don't support messages?\n",
	       scb->xfer->sc_link->target); 
#endif
  } else {
    tmp[0] = IDENTIFY(1, scb->xfer->sc_link->lun);	/* allow disconnects */
    len = 1;
    data = tmp;
    phase = REQ_MSGOUT;
    /* Should do test on result of sea_transfer_pio */
#if SEADEBUG2
/*      printf("Trying a msg out phase\n"); */
    printf("{7");
#endif
    sea_transfer_pio(sea, &phase, &len, &data);
  }
  if (!(STATUS & STAT_BSY)) {
    printf("sea: after successful arbitrate: No STAT_BSY!\n");
  }
  
#if SEADEBUG2
  printf("{8");
#endif
  sea->connected = scb;
  sea->busy[scb->xfer->sc_link->target] |= (1 << scb->xfer->sc_link->lun);
  /* this assignment should depend on possibility to send a message to target */
  CONTROL = BASE_CMD | CMD_DRVR_ENABLE;
  /* reset pointer in command ??? */
  return 0;
}

/* sea_abort
   send an abort to the target
   return 1 success, 0 on failure
 */
int sea_abort(int unit, struct sea_scb *scb)
{
  struct sea_data *sea = seadata[unit];
  struct sea_scb *tmp, **prev;
  unsigned char msg, phase, *msgptr;
  int32 len;
  int oldpri;

#ifdef SEADEBUG2
/*	printf("sea_abort called\n"); */
  printf("\\");
#endif

  oldpri = splbio();

  /* If the command hasn't been issued yet, we simply remove it from the
   * issue queue
   */
  for (prev = (struct sea_scb **) &(sea->issue_queue),
       tmp = sea->issue_queue; tmp;
       prev = (struct sea_scb **) &(tmp->next), tmp = tmp->next)
    if (scb == tmp) {
      (*prev) = tmp->next;
      tmp->next = NULL;
      /* set some type of error result for this operation */
      splx(oldpri);
#ifdef SEADEBUG2
      printf("\\2");
#endif
      return 1;
    }

  /* If any commands are connected, we're going to fail the abort
   * and let the high level SCSI driver retry at a later time or issue a
   * reset
   */

  if(sea->connected) {
    splx(oldpri);
#ifdef SEADEBUG2
/*		printf("sea:abort error connected\n"); */
    printf("\\3");
#endif
    return 0;
  }

  /* If the command is currently disconnected from the bus, and there are
   * no connected commands, we reconnect the I_T_L or I_T_L_Q nexus
   * associated with it, go into message out, and send an abort message.
   */

  for (tmp = sea->disconnected_queue; tmp; tmp = tmp->next)
    if (scb == tmp) {
      splx(oldpri);
#ifdef SEADEBUG2
      printf("\\4");
#endif
      if (sea_select(sea,scb)) {
#ifdef SEADEBUG2
	printf("\\5");
#endif
	return 0;
      }
      msg = MSG_ABORT;
      msgptr = &msg;
      len = 1;
      phase = REQ_MSGOUT;
      CONTROL = BASE_CMD | CMD_ATTN;
      sea_transfer_pio(sea, &phase, &len, &msgptr);

      oldpri = splbio();
      for (prev = (struct sea_scb **) &(sea->disconnected_queue),
	   tmp = sea->disconnected_queue; tmp ;
	   prev = (struct sea_scb **) &(tmp->next), tmp = tmp->next)
	if (scb == tmp) {
	  *prev = tmp->next;
	  tmp->next = NULL;
	  /* set some type of error result for the operation */
#ifdef SEADEBUG2
	  printf("\\6");
#endif
	  splx(oldpri);
	  return 1;
	}
    }

  /* command not found in any queue, race condition in the code ? */

  splx(oldpri);
#ifdef SEADEBUG2
/*	printf("sea: WARNING: SCSI command probably completed successfully\n"
	       "              before abortion\n"); */
  printf("\\7");
#endif
  return 1;
	
}

void sea_done(int unit, struct sea_scb *scb)
{
  struct sea_data *sea = seadata[unit];
  struct scsi_xfer *xs = scb->xfer;


#ifdef SEADEBUG2
/*	printf("sea_done called\n"); */
  printf("&");
#endif

  if (scb->flags & SCB_TIMECHK) {
#ifdef SEADEBUG2
    printf("&2");
#endif
#ifdef SEA_FREEBSD11
    untimeout(sea_timeout, (caddr_t)scb);
#else
    untimeout(sea_timeout, scb);
#endif
  }

  xs->resid = scb->datalen;	/* How much of the buffer was not touched */

  if ((scb->flags == SCB_ACTIVE) || (xs->flags & SCSI_ERR_OK)) {
#ifdef SEADEBUG2
/*		printf("sea_done:Report no err in xs\n"); */
    printf("&3");
#endif
/*    xs->resid = 0; */
/*		xs->error = 0; */
  } else {

    if (!(scb->flags == SCB_ACTIVE)) {
      if ((scb->flags & SCB_TIMEOUT) || (scb->flags & SCB_ABORTED)) {
#ifdef SEADEBUG2
	printf("&6");
#endif
	xs->error = XS_TIMEOUT;
      }
      if (scb->flags & SCB_ERROR) {
#ifdef SEADEBUG2
	printf("&7");
#endif
	xs->error = XS_DRIVER_STUFFUP;
      }
    } else {

      /* !!! Add code to check for target status */
      /* say all error now */
      xs->error = XS_DRIVER_STUFFUP;
#ifdef SEADEBUG2
      printf("&4");
#endif
    }
  }
  xs->flags |= ITSDONE;
  sea_free_scb(unit, scb, xs->flags);
  scsi_done(xs);
#ifdef SEADEBUG2
/*	printf("Leaving sea_done\n"); */
  printf("&5");
#endif
}

/* wait for completion of command in polled mode */

int sea_poll(int unit, struct scsi_xfer *xs, struct sea_scb *scb)
{
  int count = 500; /* xs->timeout; */
  int oldpri;

#ifdef SEADEBUG2
/*	printf("sea_poll called\n"); */
  printf("?");
#endif

  while (count) {
    /* try to do something */
    oldpri = splbio();
    if (!main_running) {
      main_running = 1;
      sea_main();
      /* main_running is cleared in sea_main once it can't
       * do more work, and sea_main exits with interrupts
       * disabled
       */
      splx(oldpri);
    } else {
      splx(oldpri);
    }
    if (xs->flags & ITSDONE) {
      break;
    }
    DELAY(10);
    count--;
  }
#ifdef SEADEBUG2
  printf("?2 %x ", count);
/*	printf("sea_poll: count:%x\n",count); */
#endif
  if (count == 0) {
    /* we timed out, so call the timeout handler manually,
     * accounting for the fact that the clock is not running yet
     * by taking out the clock queue entry it makes.
     */
#ifdef SEADEBUG2
    printf("?3");
#endif
#ifdef SEA_FREEBSD11
    sea_timeout((caddr_t)scb, 0);
#else
    sea_timeout(scb);
#endif

    /* because we are polling, take out the timeout entry
     * sea_timeout made
     */
#ifdef SEADEBUG2
    printf("?4");
#endif
#ifdef SEA_FREEBSD11
    untimeout(sea_timeout, (caddr_t) scb);
#else
    untimeout(sea_timeout, scb);
#endif
    count = 50;
    while (count) {
      /* once again, wait for the int bit */
      oldpri = splbio();
      if (!main_running) {
	main_running = 1;
	sea_main();
	/* main_running is cleared by sea_main once it can't
	 * do more work, and sea_main exits with interrupts
	 * disabled
	 */
	splx(oldpri);
      } else {
	splx(oldpri);
      }
      if (xs->flags & ITSDONE) {
	break;
      }
      DELAY(10);
      count--;
    }
    if (count == 0) {
      /* we timed out again... This is bad. Notice that
       * this time there is no clock queue entry to remove
       */
#ifdef SEADEBUG2
      printf("?5");
#endif
#ifdef SEA_FREEBSD11
      sea_timeout((caddr_t)scb, 0);
#else
      sea_timeout(scb);
#endif
    }
  }
#ifdef SEADEBUG2
/*  printf("sea_poll: xs->error:%x\n",xs->error); */
    printf("?6%x",xs->error);
#endif
  if (xs->error) {
#ifdef SEADEBUG2
/*    printf("done return error\n"); */
      printf("?7");
#endif
    return (HAD_ERROR);
  }
#ifdef SEADEBUG2
/*  printf("done return complete\n"); */
    printf("?8");
#endif
  return (COMPLETE);
}

/*
 * sea_information_transfer
 * Do the transfer. We know we are connected. Update the flags,
 * call sea_done when task accomplished. Dialog controlled by the
 * target
 */
static void sea_information_transfer (struct sea_data *sea)
{
  long int timeout;
  int unit = sea->sc_link.adapter_unit;
  unsigned char msgout = MSG_NOP;
  int32 len;
  int oldpri;
  u_char *data;
  unsigned char phase, tmp, old_phase=REQ_UNKNOWN;
  struct sea_scb *scb = sea->connected;
  int loop;

#if SEADEBUG2
/*	printf("sea_information_transfer called\n"); */
  printf("!");
#endif

  for(timeout = 0; timeout < 10000000L ; timeout++) {
    tmp = STATUS;
    if (!(tmp & STAT_BSY)) {
/*      for(loop=0;loop < 20 ; loop++) {
        if((tmp=STATUS) & STAT_BSY)
          break;
      } */
#ifndef SEADEBUG8
      if(!(tmp & STAT_BSY)) {
        printf("sea: !STAT_BSY unit in data transfer!\n");
        oldpri = splbio();
        sea->connected = NULL;
        scb->flags = SCB_ERROR;
        splx(oldpri);
        sea_done(unit, scb);
        return;
      }
#endif
    }

    /* we only have a valid SCSI phase when REQ is asserted */
    if (tmp & STAT_REQ) {
      phase = (tmp & REQ_MASK);
      if (phase != old_phase) {
	old_phase = phase;
      }

#ifdef SEADEBUG7
      printf("!2%x", phase);
      for(loop=0;loop < 20; loop++) {
        phase = STATUS;
        printf("!6%x",phase);
        phase = phase & REQ_MASK;
      }
#endif

      switch (phase) {
      case REQ_DATAOUT:
#ifdef	SEA_NODATAOUT
	printf("sea: SEA_NODATAOUT set, attempted DATAOUT aborted\n");
	msgout = MSG_ABORT;
	CONTROL = BASE_CMD | CMD_ATTN;
	break;
#endif
      case REQ_DATAIN:
/*        data = scb->xfer->data;
        len = scb->xfer->datalen;
*/        if(!(scb->data)) {
          printf("no data address!\n");
        }
#ifdef SEA_BLINDTRANSFER
        if (scb->datalen && !(scb->datalen % BLOCK_SIZE)) {
          while (scb->datalen) {
            for(timeout = 0; timeout < 5000000L ; timeout++)
              if((tmp = STATUS) & STAT_REQ)
                break;
            if(!(tmp & STAT_REQ)) {
              printf("sea_transfer_pio: timeout waiting for STAT_REQ\n");
              /* getchar(); */
            }
            if((tmp & REQ_MASK) != phase) {
#ifdef SEADEBUG1
              printf("sea:infotransfer phase mismatch:%x, want:%x, len:%x\n",
		     tmp,phase,scb->datalen);
              /* getchar(); */
#endif
              break;
            }
            if(!(phase & STAT_IO)) {
#ifdef SEA_ASSEMBLER
              asm("
		shr $2, %%ecx;
		cld;
		rep;
		movsl; " : :
		"D" (sea->st0x_dr), "S" (scb->data), "c" (BLOCK_SIZE) :
		"cx", "si", "di" );
              scb->data += BLOCK_SIZE;
#else
              for(count=0; count < BLOCK_SIZE; count++) {
		DATA = *(scb->data);
                scb->data++;
              }
#endif
            } else {
#ifdef SEA_ASSEMBLER
              asm("
		shr $2, %%ecx;
		cld;
		rep;
		movsl; " : :
		"S" (sea->st0x_dr), "D" (scb->data), "c" (BLOCK_SIZE) :
		"cx", "si", "di" );
              scb->data += BLOCK_SIZE;
#else
              for(count=0; count < BLOCK_SIZE; count++) {
                *scb->data = DATA;
                scb->data++;
              }
#endif
            }
            scb->datalen -= BLOCK_SIZE;
          }
        }

        /* save current position into the command structure */
/*	scb->xfer->data = data;
	scb->xfer->datalen = len; */
#endif 

        sea_transfer_pio(sea, &phase, &(scb->datalen), &(scb->data));
/*        scb->xfer->data = data;
	scb->xfer->datalen = len;
*/        break;

      case REQ_MSGIN:
        /* don't handle multi-byte messages here, because they
	 * should not be present here
	 */
	len = 1;
	data = &tmp;
	sea_transfer_pio(sea, &phase, &len, &data);
	/* scb->MessageIn = tmp; */

	switch (tmp) {

	case MSG_ABORT:
	  scb->flags = SCB_ABORTED;
	  printf("sea:Command aborted by target\n");
	  CONTROL = BASE_CMD;
	  sea_done(unit, scb);
	  return;
					
	case MSG_COMMAND_COMPLETE:
	  oldpri = splbio();
	  sea->connected = NULL;
	  splx(oldpri);
#ifdef SEADEBUG2
	  printf("!3");
#endif
	  sea->busy[scb->xfer->sc_link->target] &= 
	    ~(1 << scb->xfer->sc_link->lun);

	  CONTROL = BASE_CMD;
	  sea_done(unit, scb);
	  return;
	case MSG_MESSAGE_REJECT:
	  /*	printf("sea: message_reject recieved\n"); */
	  printf("!4");
	  break;
	case MSG_DISCONNECT:
	  oldpri = splbio();
	  scb->next = sea->disconnected_queue;
	  sea->disconnected_queue = scb;
	  sea->connected = NULL;
	  CONTROL = BASE_CMD;
	  splx(oldpri);
#ifdef SEADEBUG2
/*					printf("msg_disconnect\n"); */
	  printf("!5");
#endif
	  return;
	  /* save/restore of pointers are ignored */
	case MSG_SAVE_POINTERS:
	case MSG_RESTORE_POINTERS:
#if SEADEBUG2
	  printf("sea: rec save/restore ptrs\n");
#endif
	  break;
	default:
	  /* this should be handled in the pio data transfer phase, as the
	   * ATN should be raised before ACK goes false when rejecting a message
	   */
#ifdef SEADEBUG
          printf("sea: Unknown message in:%x\n", tmp);
#endif
          break;
	} /* switch (tmp) */
	break;
      case REQ_MSGOUT:
	len = 1;
	data = &msgout;
	/* sea->last_message = msgout; */
	sea_transfer_pio(sea, &phase, &len, &data);
	if (msgout == MSG_ABORT) {
	  printf("sea: sent message abort to target\n");
	  oldpri = splbio();
	  sea->busy[scb->xfer->sc_link->target] &= 
	    ~(1 << scb->xfer->sc_link->lun);
	  sea->connected = NULL;
	  scb->flags = SCB_ABORTED;
	  splx(oldpri); 
	  /* enable interrupt from scsi */
	  sea_done(unit, scb);
	  return;
	}
	msgout = MSG_NOP;
	break;
      case REQ_CMDOUT:
	len = scb->xfer->cmdlen;
	data = (char *) scb->xfer->cmd;
	sea_transfer_pio(sea, &phase, &len, &data);
	break;
      case REQ_STATIN:
	len = 1;
	data = &tmp;
	sea_transfer_pio(sea, &phase, &len, &data);
	scb->xfer->status = tmp;
	break;
      default:
	printf("sea: unknown phase\n");
      } /* switch (phase) */
    } /* if (tmp & STAT_REQ) */
  } /* for (...) */
  /* if we get here we have got a timeout!  */
  printf("sea: Timeout in data transfer\n");
  scb->flags = SCB_TIMEOUT;
  /* should I clear scsi-bus state? */
  sea_done(unit, scb);
}


