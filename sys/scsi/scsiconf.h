/*
 * Written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 * Ported to run under 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 *
 *	$Id: scsiconf.h,v 1.5 1993/08/28 03:08:54 rgrimes Exp $
 */

/***********************************************\
* these calls are called by the high-end	*
* drivers to get services from whatever low-end	*
* drivers they are attached to			*
\***********************************************/
struct scsi_switch
{
	int		(*scsi_cmd)();
	void		(*scsi_minphys)();
	int		(*open_target_lu)();
	int		(*close_target_lu)();
	long	int	(*adapter_info)(); /* see definitions below */
	char		*name; /* name of scsi bus controller */
	u_long	spare[2];
};
#define	AD_INF_MAX_CMDS		0x000000FF /* maximum number of entries
						queuable to a device by 
						the adapter */
/* 24 bits of other adapter characteristics go here */

/***********************************************\
* The scsi debug control bits			*
\***********************************************/
extern	int	scsi_debug;
#define PRINTROUTINES	0x01
#define	TRACEOPENS	0x02
#define	TRACEINTERRUPTS	0x04
#define	SHOWREQUESTS	0x08
#define	SHOWSCATGATH	0x10
#define	SHOWINQUIRY	0x20
#define	SHOWCOMMANDS	0x40


/********************************/
/* return values for scsi_cmd() */
/********************************/
#define SUCCESSFULLY_QUEUED	0
#define TRY_AGAIN_LATER		1
#define	COMPLETE		2
#define	HAD_ERROR		3
#define	ESCAPE_NOT_SUPPORTED	4

struct scsi_xfer
{
	struct	scsi_xfer *next;	/* when free */
	int	flags;
	u_char	adapter;
	u_char	targ;
	u_char	lu;
	u_char	retries;	/* the number of times to retry */
	long	int	timeout;	/* in miliseconds */
	struct	scsi_generic *cmd;
	int	cmdlen;
	u_char	*data;		/* either the dma address OR a uio address */
	int	datalen;	/* data len (blank if uio)    */
	int	resid;
	int	(*when_done)();
	int	done_arg;
	int	done_arg2;
	int	error;
	struct	buf *bp;
	struct	scsi_sense_data	sense;

	/* Believe it or not, Some targets fall on the ground with
	 * anything but a certain sense length.
	 */
	int req_sense_length;	/* Explicit request sense length */

	int status;		/* SCSI status */
};
/********************************/
/* Flag values			*/
/********************************/
#define	SCSI_NOSLEEP	0x01	/* Not a user... don't sleep		*/
#define	SCSI_NOMASK	0x02	/* dont allow interrupts.. booting	*/
#define	SCSI_NOSTART	0x04	/* left over from ancient history	*/
#define	ITSDONE		0x10	/* the transfer is as done as it gets	*/
#define	INUSE		0x20	/* The scsi_xfer block is in use	*/
#define	SCSI_SILENT	0x40	/* Don't report errors to console	*/
#define SCSI_ERR_OK	0x80	/* An error on this operation is OK.	*/
#define	SCSI_RESET	0x100	/* Reset the device in question		*/
#define	SCSI_DATA_UIO	0x200	/* The data address refers to a UIO	*/
#define	SCSI_DATA_IN	0x400	/* expect data to come INTO memory	*/
#define	SCSI_DATA_OUT	0x800	/* expect data to flow OUT of memory	*/
#define	SCSI_TARGET	0x1000	/* This defines a TARGET mode op.	*/
#define	SCSI_ESCAPE	0x2000	/* Escape operation			*/

/*************************************************************************/
/* Escape op codes.  This provides an extensible setup for operations    */
/* that are not scsi commands.  They are intended for modal operations.  */
/*************************************************************************/

#define SCSI_OP_TARGET	0x0001
#define	SCSI_OP_RESET	0x0002
#define	SCSI_OP_BDINFO	0x0003

/********************************/
/* Error values			*/
/********************************/
#define XS_NOERROR	0x0	/* there is no error, (sense is invalid)  */
#define XS_SENSE	0x1	/* Check the returned sense for the error */
#define	XS_DRIVER_STUFFUP 0x2	/* Driver failed to perform operation	  */
#define XS_TIMEOUT	0x03	/* The device timed out.. turned off?	  */
#define XS_SWTIMEOUT	0x04	/* The Timeout reported was caught by SW  */
#define XS_BUSY		0x08	/* The device busy, try again later?	  */

