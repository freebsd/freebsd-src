/* 
 */
/*
 * HISTORY
 * 
 *
 * PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
 * --------------------         -----   ----------------------
 * CURRENT PATCH LEVEL:         1       00098
 * --------------------         -----   ----------------------
 *
 * 16 Feb 93	Julian Elischer		ADDED for SCSI system
 * 
 */

#include	<sys/types.h>
#include	<ch.h>

#include <sys/param.h>
#include <sys/systm.h>

#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/chio.h>

#if defined(OSF)
#define SECSIZE	512
#endif /* defined(OSF) */

#include <scsi/scsi_all.h>
#include <scsi/scsi_changer.h>
#include <scsi/scsiconf.h>


struct  scsi_xfer ch_scsi_xfer[NCH];
int     ch_xfer_block_wait[NCH];


#define PAGESIZ 	4096
#define STQSIZE		4
#define	CH_RETRIES	4


#define MODE(z)		(  (minor(z) & 0x0F) )
#define UNIT(z)		(  (minor(z) >> 4) )

#ifndef	MACH
#define ESUCCESS 0
#endif	MACH

int	ch_info_valid[NCH];	/* the info about the device is valid */
int	ch_initialized[NCH] ;
int	ch_debug = 1;

int chattach();
int ch_done();
struct	ch_data
{
	int	flags;
	struct	scsi_switch *sc_sw;	/* address of scsi low level switch */
	int	ctlr;			/* so they know which one we want */
	int	targ;			/* our scsi target ID */
	int	lu;			/* out scsi lu */
	short	chmo;			/* Offset of first CHM */
	short	chms;			/* No. of CHM */
	short	slots;			/* No. of Storage Elements */
	short	sloto;			/* Offset of first SE */
	short	imexs;			/* No. of Import/Export Slots */
	short	imexo;			/* Offset of first IM/EX */
	short	drives;			/* No. of CTS */
	short	driveo;			/* Offset of first CTS */
	short	rot;			/* CHM can rotate */
	u_long	op_matrix;		/* possible opertaions */
	u_short lsterr;		/* details of lasterror */
	u_char	stor;			/* posible Storage locations */
}ch_data[NCH];

#define CH_OPEN		0x01
#define CH_KNOWN	0x02

static	int	next_ch_unit = 0;
/***********************************************************************\
* The routine called by the low level scsi routine when it discovers	*
* A device suitable for this driver					*
\***********************************************************************/

int	chattach(ctlr,targ,lu,scsi_switch)
struct	scsi_switch *scsi_switch;
{
	int	unit,i,stat;
	unsigned char *tbl;

	if(scsi_debug & PRINTROUTINES) printf("chattach: ");
	/*******************************************************\
	* Check we have the resources for another drive		*
	\*******************************************************/
	unit = next_ch_unit++;
	if( unit >= NCH)
	{
		printf("Too many scsi changers..(%d > %d) reconfigure kernel",(unit + 1),NCH);
		return(0);
	}
	/*******************************************************\
	* Store information needed to contact our base driver	*
	\*******************************************************/
	ch_data[unit].sc_sw	=	scsi_switch;
	ch_data[unit].ctlr	=	ctlr;
	ch_data[unit].targ	=	targ;
	ch_data[unit].lu	=	lu;

	/*******************************************************\
	* Use the subdriver to request information regarding	*
	* the drive. We cannot use interrupts yet, so the	*
	* request must specify this.				*
	\*******************************************************/
	if((ch_mode_sense(unit,  SCSI_NOSLEEP |  SCSI_NOMASK /*| SCSI_SILENT*/)))
	{
		printf("	ch%d: scsi changer, %d slot(s) %d drive(s) %d arm(s) %d i/e-slot(s) \n",
			unit, ch_data[unit].slots, ch_data[unit].drives, ch_data[unit].chms, ch_data[unit].imexs);
		stat=CH_KNOWN;
	}
	else
	{
		printf("	ch%d: scsi changer :- offline\n", unit);
		stat=CH_OPEN;
	}
	ch_initialized[unit] = stat;

	return;

}



/*******************************************************\
*	open the device.				*
\*******************************************************/
chopen(dev)
{
	int errcode = 0;
	int unit,mode;

	unit = UNIT(dev);
	mode = MODE(dev);

	/*******************************************************\
	* Check the unit is legal                               *
	\*******************************************************/
	if ( unit >= NCH )
	{
		printf("ch %d  > %d\n",unit,NCH);
		errcode = ENXIO;
		return(errcode);
	}
	/*******************************************************\
	* Only allow one at a time				*
	\*******************************************************/
	if(ch_data[unit].flags & CH_OPEN)
	{
		printf("CH%d already open\n",unit);
		errcode = ENXIO;
		goto bad;
	}
	
	if(ch_debug||(scsi_debug & (PRINTROUTINES | TRACEOPENS)))
		printf("chopen: dev=0x%x (unit %d (of %d))\n"
				,   dev,      unit,   NCH);
	/*******************************************************\
	* Make sure the device has been initialised		*
	\*******************************************************/

	if (!ch_initialized[unit])
		return(ENXIO);
	if (ch_initialized[unit]!=CH_KNOWN) {
		if((ch_mode_sense(unit,  SCSI_NOSLEEP |  SCSI_NOMASK /*| SCSI_SILENT*/)))
		{
			ch_initialized[unit]=CH_KNOWN;
		}
	  else
		{
			printf("  ch%d: scsi changer :- offline\n", unit);
			return(ENXIO);
		}
	}
	/*******************************************************\
	* Check that it is still responding and ok.		*
	\*******************************************************/

	if(ch_debug || (scsi_debug & TRACEOPENS))
		printf("device is ");
	if (!(ch_req_sense(unit, 0)))
	{
		errcode = ENXIO;
		if(ch_debug || (scsi_debug & TRACEOPENS))
			printf("not responding\n");
		goto bad;
	}
	if(ch_debug || (scsi_debug & TRACEOPENS))
		printf("ok\n");

	if(!(ch_test_ready(unit,0)))
	{
		printf("ch%d not ready\n",unit);
		return(EIO);
	}

	ch_info_valid[unit] = TRUE;

	/*******************************************************\
	* Load the physical device parameters			*
	\*******************************************************/

	ch_data[unit].flags = CH_OPEN;
	return(errcode);
bad:
	return(errcode);
}

/*******************************************************\
* close the device.. only called if we are the LAST	*
* occurence of an open device				*
\*******************************************************/
chclose(dev)
{
	unsigned char unit,mode;

	unit = UNIT(dev);
	mode = MODE(dev);

	if(scsi_debug & TRACEOPENS)
		printf("Closing device");
	ch_data[unit].flags = 0;
	return(0);
}



/***************************************************************\
* chstart                                                       *
* This routine is also called after other non-queued requests	*
* have been made of the scsi driver, to ensure that the queue	*
* continues to be drained.					*
\***************************************************************/
/* chstart() is called at splbio */
chstart(unit)
{
	int			drivecount;
	register struct buf	*bp = 0;
	register struct buf	*dp;
	struct	scsi_xfer	*xs;
	int			blkno, nblk;


	if(scsi_debug & PRINTROUTINES) printf("chstart%d ",unit);
	/*******************************************************\
	* See if there is a buf to do and we are not already	*
	* doing one						*
	\*******************************************************/
	xs=&ch_scsi_xfer[unit];
	if(xs->flags & INUSE)
	{
		return;    /* unit already underway */
	}
	if(ch_xfer_block_wait[unit]) /* a special awaits, let it proceed first */
	{
		wakeup(&ch_xfer_block_wait[unit]);
		return;
	}

	return;

}


/*******************************************************\
* This routine is called by the scsi interrupt when	*
* the transfer is complete.
\*******************************************************/
int	ch_done(unit,xs)
int	unit;
struct	scsi_xfer	*xs;
{
	struct	buf		*bp;
	int	retval;

	if(ch_debug||(scsi_debug & PRINTROUTINES)) printf("ch_done%d ",unit);
	if (! (xs->flags & INUSE))
		panic("scsi_xfer not in use!");
	wakeup(xs);
}
/*******************************************************\
* Perform special action on behalf of the user		*
* Knows about the internals of this device		*
\*******************************************************/
chioctl(dev, cmd, arg, mode)
dev_t dev;
int cmd;
caddr_t arg;
{
	/* struct ch_cmd_buf *args;*/
	union scsi_cmd *scsi_cmd;
	register i,j;
	unsigned int opri;
	int errcode = 0;
	unsigned char unit;
	int number,flags,ret;

	/*******************************************************\
	* Find the device that the user is talking about	*
	\*******************************************************/
	flags = 0;	/* give error messages, act on errors etc. */
	unit = UNIT(dev);

	switch(cmd)
	{
		case CHIOOP: {
			struct chop *ch=(struct chop *) arg;
			if (ch_debug)
				printf("[chtape_chop: %x]\n", ch->ch_op);

			 switch ((short)(ch->ch_op)) {
				case CHGETPARAM:
					ch->u.getparam.chmo=	 ch_data[unit].chmo;
					ch->u.getparam.chms=   ch_data[unit].chms;
					ch->u.getparam.sloto=  ch_data[unit].sloto;
					ch->u.getparam.slots=  ch_data[unit].slots;
					ch->u.getparam.imexo=  ch_data[unit].imexo;
					ch->u.getparam.imexs=  ch_data[unit].imexs;
					ch->u.getparam.driveo= ch_data[unit].driveo;
					ch->u.getparam.drives= ch_data[unit].drives;
					ch->u.getparam.rot=    ch_data[unit].rot;
					ch->result=0;
					return 0;
					break;
				case CHPOSITION: 
				 return ch_position(unit,&ch->result,ch->u.position.chm,
						ch->u.position.to,
						flags);
				case CHMOVE: 
				 return ch_move(unit,&ch->result, ch->u.position.chm,
					ch->u.move.from, ch->u.move.to,
								flags);
				case CHGETELEM:
				 return ch_getelem(unit,&ch->result, ch->u.get_elem_stat.type,
					ch->u.get_elem_stat.from, &ch->u.get_elem_stat.elem_data,
									flags);
				default:
				 return EINVAL;
		}

	 }
	default:
		return EINVAL;
	}

	return(ret?ESUCCESS:EIO);
}

ch_getelem(unit,stat,type,from,data,flags)
int unit,from,flags;
short *stat;
char *data;
{
	struct scsi_read_element_status scsi_cmd;
	char elbuf[32];
	int ret;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = READ_ELEMENT_STATUS;
	scsi_cmd.element_type_code=type;
	scsi_cmd.starting_element_addr[0]=(from>>8)&0xff;
	scsi_cmd.starting_element_addr[1]=from&0xff;
	scsi_cmd.number_of_elements[1]=1;
	scsi_cmd.allocation_length[2]=32;
	
	if ((ret=ch_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			elbuf,
			32,
			100000,
			flags) !=ESUCCESS)) {
			*stat=ch_data[unit].lsterr;
			bcopy(elbuf+16,data,16);
			return ret;
			}
	bcopy(elbuf+16,data,16); /*Just a hack sh */
	return ret;
}

ch_move(unit,stat,chm,from,to,flags)
int unit,chm,from,to,flags;
short *stat;
{
	struct scsi_move_medium scsi_cmd;
	int ret;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = MOVE_MEDIUM;
	scsi_cmd.transport_element_address[0]=(chm>>8)&0xff;
	scsi_cmd.transport_element_address[1]=chm&0xff;
	scsi_cmd.source_address[0]=(from>>8)&0xff;
	scsi_cmd.source_address[1]=from&0xff;
	scsi_cmd.destination_address[0]=(to>>8)&0xff;
	scsi_cmd.destination_address[1]=to&0xff;
	scsi_cmd.invert=(chm&CH_INVERT)?1:0;
	if ((ret=ch_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			NULL,
			0,
			100000,
			flags) !=ESUCCESS)) {
			*stat=ch_data[unit].lsterr;
			return ret;
			}
	return ret;
}

ch_position(unit,stat,chm,to,flags)
int unit,chm,to,flags;
short *stat;
{
	struct scsi_position_to_element scsi_cmd;
	int ret;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = POSITION_TO_ELEMENT;
	scsi_cmd.transport_element_address[0]=(chm>>8)&0xff;
	scsi_cmd.transport_element_address[1]=chm&0xff;
	scsi_cmd.source_address[0]=(to>>8)&0xff;
	scsi_cmd.source_address[1]=to&0xff;
	scsi_cmd.invert=(chm&CH_INVERT)?1:0;
	if ((ret=ch_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			NULL,
			0,
			100000,
			flags) !=ESUCCESS)) {
			*stat=ch_data[unit].lsterr;
			return ret;
			}
	return ret;
}

/*******************************************************\
* Check with the device that it is ok, (via scsi driver)*
\*******************************************************/
ch_req_sense(unit, flags)
int	flags;
{
	struct	scsi_sense_data sense;
	struct scsi_sense scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = REQUEST_SENSE;
	scsi_cmd.length = sizeof(sense);

	if (ch_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(struct	scsi_sense),
			&sense,
			sizeof(sense),
			100000,
			flags | SCSI_DATA_IN) != 0)
	{
		return(FALSE);
	}
	else 
		return(TRUE);
}

/*******************************************************\
* Get scsi driver to send a "are you ready" command	*
\*******************************************************/
ch_test_ready(unit,flags)
int	unit,flags;
{
	struct scsi_test_unit_ready scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = TEST_UNIT_READY;

	if (ch_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(struct	scsi_test_unit_ready),
			0,
			0,
			100000,
			flags) != 0) {
		return(FALSE);
	} else 
		return(TRUE);
}


#ifdef	__STDC__
#define b2tol(a)	(((unsigned)(a##_1) << 8) + (unsigned)a##_0 )
#else
#define b2tol(a)	(((unsigned)(a/**/_1) << 8) + (unsigned)a/**/_0 )
#endif

/*******************************************************\
* Get the scsi driver to send a full inquiry to the	*
* device and use the results to fill out the global 	*
* parameter structure.					*
\*******************************************************/
ch_mode_sense(unit, flags)
int	unit,flags;
{
	struct scsi_mode_sense scsi_cmd;
	u_char scsi_sense[128];	/* Can't use scsi_mode_sense_data because of */
				/* missing block descriptor 		     */
	u_char *b;
	int i,l;

	/*******************************************************\
	* First check if we have it all loaded			*
	\*******************************************************/
	if (ch_info_valid[unit]==CH_KNOWN) return(TRUE);
	/*******************************************************\
	* First do a mode sense 				*
	\*******************************************************/
	ch_info_valid[unit] &= ~CH_KNOWN;
	for(l=1;l>=0;l--) {
		bzero(&scsi_cmd, sizeof(scsi_cmd));
		scsi_cmd.op_code = MODE_SENSE;
		scsi_cmd.dbd = l;
		scsi_cmd.page_code = 0x3f;	/* All Pages */
		scsi_cmd.length = sizeof(scsi_sense);
	/*******************************************************\
	* do the command, but we don't need the results		*
	* just print them for our interest's sake		*
	\*******************************************************/
		if (ch_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(struct	scsi_mode_sense),
			&scsi_sense,
			sizeof(scsi_sense),
			5000,
			flags | SCSI_DATA_IN) == 0) {
				ch_info_valid[unit] = CH_KNOWN;
				break;
			}
	}
	if (ch_info_valid[unit]!=CH_KNOWN)   {
		if(!(flags & SCSI_SILENT))
			printf("could not mode sense for unit %d\n", unit);
		return(FALSE);
	}
	l=scsi_sense[0]-3;
	b=&scsi_sense[4];
	/*****************************\
	* To avoid alignment problems *
	\*****************************/
/*FIX THIS FOR MSB */
#define p2copy(valp)	 (valp[1]+ (valp[0]<<8));valp+=2
#define p4copy(valp)	 (valp[3]+ (valp[2]<<8) + (valp[1]<<16) + (valp[0]<<24));valp+=4
#if 0
	printf("\nmode_sense %d\n",l);
	for(i=0;i<l+4;i++) {
		printf("%x%c",scsi_sense[i],i%8==7?'\n':':');
	}
	printf("\n");
#endif
	for(i=0;i<l;) {
		int pc=(*b++)&0x3f;
		int pl=*b++;
		u_char *bb=b;
		switch(pc) {
			case 0x1d:
				ch_data[unit].chmo  =p2copy(bb);
				ch_data[unit].chms  =p2copy(bb);
				ch_data[unit].sloto =p2copy(bb);
				ch_data[unit].slots =p2copy(bb);
				ch_data[unit].imexo =p2copy(bb);
				ch_data[unit].imexs =p2copy(bb);
				ch_data[unit].driveo =p2copy(bb);
				ch_data[unit].drives =p2copy(bb);
				break;
			case 0x1e:
				ch_data[unit].rot = (*b)&1;
				break;
			case 0x1f:
				ch_data[unit].stor = *b&0xf;
				bb+=2;
				ch_data[unit].stor =p4copy(bb);
				break;
			default:
				break;
		}
		b+=pl;
		i+=pl+2;
	}
	if (ch_debug)
	{
		printf("unit %d: cht(%d-%d)slot(%d-%d)imex(%d-%d)cts(%d-%d) %s rotate\n",
				unit,
				ch_data[unit].chmo,
				ch_data[unit].chms,
				ch_data[unit].sloto,
				ch_data[unit].slots,
				ch_data[unit].imexo,
				ch_data[unit].imexs,
				ch_data[unit].driveo,
				ch_data[unit].drives,
				ch_data[unit].rot?"can":"can't");
	}
	return(TRUE);
}

/*******************************************************\
* ask the scsi driver to perform a command for us.	*
* Call it through the switch table, and tell it which	*
* sub-unit we want, and what target and lu we wish to	*
* talk to. Also tell it where to find the command	*
* how long int is.					*
* Also tell it where to read/write the data, and how	*
* long the data is supposed to be			*
\*******************************************************/
int	ch_scsi_cmd(unit,scsi_cmd,cmdlen,data_addr,datalen,timeout,flags)

int	unit,flags;
struct	scsi_generic *scsi_cmd;
int	cmdlen;
int	timeout;
u_char	*data_addr;
int	datalen;
{
	struct	scsi_xfer *xs;
	int	retval;
	int	s;

	if(ch_debug||(scsi_debug & PRINTROUTINES)) printf("\nch_scsi_cmd%d %x",
				unit,scsi_cmd->opcode);
	if(ch_data[unit].sc_sw)	/* If we have a scsi driver */
	{

		xs = &(ch_scsi_xfer[unit]);
		if(!(flags & SCSI_NOMASK))
			s = splbio();
		ch_xfer_block_wait[unit]++;	/* there is someone waiting */
		while (xs->flags & INUSE)
		{
			sleep(&ch_xfer_block_wait[unit],PRIBIO+1);
		}
		ch_xfer_block_wait[unit]--;
		xs->flags = INUSE;
		if(!(flags & SCSI_NOMASK))
			splx(s);

		/*******************************************************\
		* Fill out the scsi_xfer structure			*
		\*******************************************************/
		xs->flags	|=	flags;
		xs->adapter	=	ch_data[unit].ctlr;
		xs->targ	=	ch_data[unit].targ;
		xs->lu		=	ch_data[unit].lu;
		xs->retries	=	CH_RETRIES;
		xs->timeout	=	timeout;
		xs->cmd		=	scsi_cmd;
		xs->cmdlen	=	cmdlen;
		xs->data	=	data_addr;
		xs->datalen	=	datalen;
		xs->resid	=	datalen;
		xs->when_done	=	(flags & SCSI_NOMASK)
					?(int (*)())0
					:ch_done;
		xs->done_arg	=	unit;
		xs->done_arg2	=	(int)xs;
retry:		xs->error	=	XS_NOERROR;
		xs->bp		=	0;
		ch_data[unit].lsterr=0;
		retval = (*(ch_data[unit].sc_sw->scsi_cmd))(xs);
		switch(retval)
		{
		case	SUCCESSFULLY_QUEUED:
			while(!(xs->flags & ITSDONE))
				sleep(xs,PRIBIO+1);

		case	HAD_ERROR:
		case	COMPLETE:
			switch(xs->error)
			{
			case	XS_NOERROR:
				retval = ESUCCESS;
				break;
			case	XS_SENSE:
				retval = (ch_interpret_sense(unit,xs));
				break;
			case	XS_DRIVER_STUFFUP:
				retval = EIO;
				break;
			case    XS_TIMEOUT:
				if(xs->retries-- )
				{
					xs->flags &= ~ITSDONE;
					goto retry;
				}
				retval = EIO;
				break;
			case    XS_BUSY:
				if(xs->retries-- )
				{
					xs->flags &= ~ITSDONE;
					goto retry;
				}
				retval = EIO;
				break;
			default:
				retval = EIO;
				printf("st%d: unknown error category from scsi driver\n"
					,unit);
				break;
			}	
			break;
		case 	TRY_AGAIN_LATER:
			if(xs->retries-- )
			{
				xs->flags &= ~ITSDONE;
				goto retry;
			}
			retval = EIO;
			break;
		default:
			retval = EIO;
		}
		xs->flags = 0;	/* it's free! */
		chstart(unit);
	}
	else
	{
		printf("chd: not set up\n",unit);
		return(EINVAL);
	}
	return(retval);
}
/***************************************************************\
* Look at the returned sense and act on the error and detirmine	*
* The unix error number to pass back... (0 = report no error)	*
\***************************************************************/

int	ch_interpret_sense(unit,xs)
int	unit;
struct	scsi_xfer	*xs;
{
	struct	scsi_sense_data *sense;
	int	key;
	int	silent = xs->flags & SCSI_SILENT;

	/***************************************************************\
	* If errors are ok, report a success				*
	\***************************************************************/
	if(xs->flags & SCSI_ERR_OK) return(ESUCCESS);

	/***************************************************************\
	* Get the sense fields and work out what CLASS			*
	\***************************************************************/
	sense = &(xs->sense);
	switch(sense->error_class)
	{
	/***************************************************************\
	* If it's class 7, use the extended stuff and interpret the key	*
	\***************************************************************/
	case 7:
		{
		key=sense->ext.extended.sense_key;
		if(sense->ext.extended.ili)
			if(!silent)
			{
				printf("length error ");
			}
			if(sense->valid)
				xs->resid = ntohl(*((long *)sense->ext.extended.info));
				if(xs->bp)
				{
					xs->bp->b_flags |= B_ERROR;
					return(ESUCCESS);
				}
		if(sense->ext.extended.eom)
			if(!silent) printf("end of medium ");
		if(sense->ext.extended.filemark)
			if(!silent) printf("filemark ");
		if(ch_debug)
		{
			printf("code%x class%x valid%x\n"
					,sense->error_code
					,sense->error_class
					,sense->valid);
			printf("seg%x key%x ili%x eom%x fmark%x\n"
					,sense->ext.extended.segment
					,sense->ext.extended.sense_key
					,sense->ext.extended.ili
					,sense->ext.extended.eom
					,sense->ext.extended.filemark);
			printf("info: %x %x %x %x followed by %d extra bytes\n"
					,sense->ext.extended.info[0]
					,sense->ext.extended.info[1]
					,sense->ext.extended.info[2]
					,sense->ext.extended.info[3]
					,sense->ext.extended.extra_len);
			printf("extra: %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n"
					,sense->ext.extended.extra_bytes[0]
					,sense->ext.extended.extra_bytes[1]
					,sense->ext.extended.extra_bytes[2]
					,sense->ext.extended.extra_bytes[3]
					,sense->ext.extended.extra_bytes[4]
					,sense->ext.extended.extra_bytes[5]
					,sense->ext.extended.extra_bytes[6]
					,sense->ext.extended.extra_bytes[7]
					,sense->ext.extended.extra_bytes[8]
					,sense->ext.extended.extra_bytes[9]
					,sense->ext.extended.extra_bytes[10]
					,sense->ext.extended.extra_bytes[11]
					,sense->ext.extended.extra_bytes[12]
					,sense->ext.extended.extra_bytes[13]
					,sense->ext.extended.extra_bytes[14]
					,sense->ext.extended.extra_bytes[15]);
				    
		}
		switch(key)
		{
		case	0x0:
			return(ESUCCESS);
		case	0x1:
			if(!silent)
			{
				printf("st%d: soft error(corrected) ", unit); 
				if(sense->valid)
				{
			   		printf("block no. %d (decimal)\n",
			  		(sense->ext.extended.info[0] <<24)|
			  		(sense->ext.extended.info[1] <<16)|
			  		(sense->ext.extended.info[2] <<8)|
			  		(sense->ext.extended.info[3] ));
				}
			 	else
				{
			 		printf("\n");
				}
			}
			return(ESUCCESS);
		case	0x2:
			if(!silent) printf("st%d: not ready\n ", unit); 
			ch_data[unit].lsterr=(sense->ext.extended.info[12]<<8)|
								sense->ext.extended.info[13] ;
			return(ENODEV);
		case	0x3:
			if(!silent)
			{
				printf("st%d: medium error ", unit); 
				if(sense->valid)
				{
			   		printf("block no. %d (decimal)\n",
			  		(sense->ext.extended.info[0] <<24)|
			  		(sense->ext.extended.info[1] <<16)|
			  		(sense->ext.extended.info[2] <<8)|
			  		(sense->ext.extended.info[3] ));
				}
			 	else
				{
			 		printf("\n");
				}
			}
			return(EIO);
		case	0x4:
			if(!silent) printf("st%d: non-media hardware failure\n ",
				unit); 
			ch_data[unit].lsterr=(sense->ext.extended.info[12]<<8)|
								sense->ext.extended.info[13] ;
			return(EIO);
		case	0x5:
			if(!silent) printf("st%d: illegal request\n ", unit); 
			ch_data[unit].lsterr=(sense->ext.extended.info[12]<<8)|
								sense->ext.extended.info[13] ;
			return(EINVAL);
		case	0x6:
			if(!silent) printf("st%d: Unit attention.\n ", unit); 
			ch_data[unit].lsterr=(sense->ext.extended.info[12]<<8)|
								sense->ext.extended.info[13] ;
			ch_info_valid[unit] = FALSE;
			if (ch_data[unit].flags & CH_OPEN) /* TEMP!!!! */
				return(EIO);
			else
				return(ESUCCESS);
		case	0x7:
			if(!silent)
			{
				printf("st%d: attempted protection violation "
								, unit); 
				if(sense->valid)
				{
			   		printf("block no. %d (decimal)\n",
			  		(sense->ext.extended.info[0] <<24)|
			  		(sense->ext.extended.info[1] <<16)|
			  		(sense->ext.extended.info[2] <<8)|
			  		(sense->ext.extended.info[3] ));
				}
			 	else
				{
			 		printf("\n");
				}
			}
			return(EACCES);
		case	0x8:
			if(!silent)
			{
				printf("st%d: block wrong state (worm)\n "
							, unit); 
				if(sense->valid)
				{
			   		printf("block no. %d (decimal)\n",
			  		(sense->ext.extended.info[0] <<24)|
			  		(sense->ext.extended.info[1] <<16)|
			  		(sense->ext.extended.info[2] <<8)|
			  		(sense->ext.extended.info[3] ));
				}
			 	else
				{
			 		printf("\n");
				}
			}
			return(EIO);
		case	0x9:
			if(!silent) printf("st%d: vendor unique\n",
				unit); 
			return(EIO);
		case	0xa:
			if(!silent) printf("st%d: copy aborted\n ",
				unit); 
			return(EIO);
		case	0xb:
			if(!silent) printf("st%d: command aborted\n ",
				unit); 
			ch_data[unit].lsterr=(sense->ext.extended.info[12]<<8)|
								sense->ext.extended.info[13] ;
			return(EIO);
		case	0xc:
			if(!silent)
			{
				printf("st%d: search returned\n ", unit); 
				if(sense->valid)
				{
			   		printf("block no. %d (decimal)\n",
			  		(sense->ext.extended.info[0] <<24)|
			  		(sense->ext.extended.info[1] <<16)|
			  		(sense->ext.extended.info[2] <<8)|
			  		(sense->ext.extended.info[3] ));
				}
			 	else
				{
			 		printf("\n");
				}
			}
			return(ESUCCESS);
		case	0xd:
			if(!silent) printf("st%d: volume overflow\n ",
				unit); 
			return(ENOSPC);
		case	0xe:
			if(!silent)
			{
			 	printf("st%d: verify miscompare\n ", unit); 
				if(sense->valid)
				{
			   		printf("block no. %d (decimal)\n",
			  		(sense->ext.extended.info[0] <<24)|
			  		(sense->ext.extended.info[1] <<16)|
			  		(sense->ext.extended.info[2] <<8)|
			  		(sense->ext.extended.info[3] ));
				}
			 	else
				{
			 		printf("\n");
				}
			}
			return(EIO);
		case	0xf:
			if(!silent) printf("st%d: unknown error key\n ",
				unit); 
			return(EIO);
		}
		break;
	}
	/***************************************************************\
	* If it's NOT class 7, just report it.				*
	\***************************************************************/
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
		{
			if(!silent) printf("st%d: error class %d code %d\n",
				unit,
				sense->error_class,
				sense->error_code);
		if(sense->valid)
			if(!silent) printf("block no. %d (decimal)\n",
			(sense->ext.unextended.blockhi <<16),
			+ (sense->ext.unextended.blockmed <<8),
			+ (sense->ext.unextended.blocklow ));
		}
		return(EIO);
	}
}



