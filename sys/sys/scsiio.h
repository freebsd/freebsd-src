/*
 * $Id: scsiio.h,v 1.5 1996/01/30 23:01:14 mpp Exp $
 */
#ifndef	_SYS_SCSIIO_H_
#define	_SYS_SCSIIO_H_

#ifndef KERNEL
#include <sys/types.h>
#endif
#include <sys/ioccom.h>

#define	SENSEBUFLEN 48

typedef struct	scsireq {
	u_long	flags;		/* info about the request status and type */
	u_long	timeout;
	u_char	cmd[16];	/* 12 is actually the max */
	u_char	cmdlen;
	caddr_t	databuf;	/* address in user space of buffer */
	u_long	datalen;	/* size of user buffer (request) */
	u_long	datalen_used;	/* size of user buffer (used)*/
	u_char	sense[SENSEBUFLEN]; /* returned sense will be in here */
	u_char	senselen;	/* sensedata request size (MAX of SENSEBUFLEN)*/
	u_char	senselen_used;	/* return value only */
	u_char	status;		/* what the scsi status was from the adapter */
	u_char	retsts;		/* the return status for the command */
	int	error;		/* error bits */
} scsireq_t;

/* bit definitions for flags */
#define SCCMD_READ		0x00000001
#define SCCMD_WRITE		0x00000002
#define SCCMD_IOV		0x00000004
#define SCCMD_ESCAPE		0x00000010
#define SCCMD_TARGET		0x00000020


/* definitions for the return status (retsts) */
#define SCCMD_OK	0x00
#define SCCMD_TIMEOUT	0x01
#define SCCMD_BUSY	0x02
#define SCCMD_SENSE	0x03
#define SCCMD_UNKNOWN	0x04

#define SCIOCCOMMAND	_IOWR('Q', 1, scsireq_t)

#define SC_DB_CMDS	0x00000001	/* show all scsi cmds and errors */
#define SC_DB_FLOW	0x00000002	/* show routines entered	*/
#define SC_DB_FLOW2	0x00000004	/* show path INSIDE routines	*/
#define SC_DB_DMA	0x00000008	/* show DMA segments etc	*/
#define SCIOCDEBUG	_IOW('Q', 2, int)	/* from 0 to 15 */

struct	scsi_addr {
	int	scbus;		/* -1 if wildcard */
	int	target;		/* -1 if wildcard */
	int	lun;		/* -1 if wildcard */
} ;

#define SCIOCREPROBE	_IOW('Q', 3, struct scsi_addr) /* look for new devs */
#define SCIOCIDENTIFY	_IOR('Q', 4, struct scsi_addr) /* where are you? */
#define SCIOCDECONFIG	_IO('Q', 5)	/* please disappear */
#define SCIOCRECONFIG	_IO('Q', 6)	/* please check again */
#define SCIOCRESET	_IO('Q', 7)	/* reset the device */
#define SCIOCADDR	_IOW('Q', 8, struct scsi_addr) /* Change address */
#define SCIOCFREEZE	_IO('Q', 9) /* Freeze SCSI */
#define SCIOCTHAW	_IO('Q', 10) /* Thaw SCSI */
#define SCIOCFREEZETHAW	_IOW('Q', 11, int) /* Freeze SCSI for some seconds */
#define SCIOCWAITTHAW	_IO('Q', 12) /* Wait for SCSI to thaw */

#endif /* !_SYS_SCSIIO_H_ */
