/*#define SCSIDEBUG 1*/
/*
 * Written by Julian Elischer (julian@tfs.com)
 *
 *	$Id: scsi_debug.h,v 1.3 93/10/10 09:26:05 julian Exp Locker: julian $
 */
#ifndef	_SCSI_SCSI_DEBUG_H
#define _SCSI_SCSI_DEBUG_H 1

/*
 * These are the new debug bits.  (Sat Oct  2 12:46:46 WST 1993)
 * the following DEBUG bits are defined to exist in the flags word of
 * the scsi_link structure.
 */
#define	SDEV_DB1		0x10	/* scsi commands, errors, data	*/ 
#define	SDEV_DB2		0x20	/* routine flow tracking */
#define	SDEV_DB3		0x40	/* internal to routine flows	*/
#define	SDEV_DB4		0x80	/* level 4 debugging for this dev */

/* target and LUN we want to debug */
#define	DEBUGTARG 9 /*9 = dissable*/
#define	DEBUGLUN  0
#define	DEBUGLEVEL  	(SDEV_DB1|SDEV_DB2)
 
/*
 * This is the usual debug macro for use with the above bits
 */
#ifdef	SCSIDEBUG
#define	SC_DEBUG(sc_link,Level,Printstuff) \
	if((sc_link)->flags & (Level))		\
	{					\
		printf("%s%d(%s%d:%d:%d): ",	\
			sc_link->device->name,	\
			sc_link->dev_unit,	\
			sc_link->adapter->name,	\
			sc_link->adapter_unit,	\
			sc_link->target,	\
			sc_link->lun);		\
 		printf Printstuff;		\
	}
#define	SC_DEBUGN(sc_link,Level,Printstuff) \
	if((sc_link)->flags & (Level))		\
	{					\
 		printf Printstuff;		\
	}
#else
#define SC_DEBUG(A,B,C) /* not included */
#define SC_DEBUGN(A,B,C) /* not included */
#endif

#endif /*_SCSI_SCSI_DEBUG_H*/
/* END OF FILE */

