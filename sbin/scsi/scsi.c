/*
 * Written By Julian ELischer
 * Copyright julian Elischer 1993.
 * Permission is granted to use or redistribute this file in any way as long
 * as this notice remains. Julian Elischer does not guarantee that this file 
 * is totally correct for any given task and users of this file must 
 * accept responsibility for any damage that occurs from the application of this
 * file.
 * 
 * (julian@tfs.com julian@dialix.oz.au)
 *
 *	$Id: scsi.c,v 1.1 1993/11/18 05:05:28 rgrimes Exp $
 */

#include <stdio.h>
#include <sys/scsiio.h>
#include <sys/file.h>
#include <scsi/scsi_all.h>
void show_mem();
int	fd;
int	debuglevel;
int	dflag,inqflag;
int	reprobe;
int	bus = -1;	/* all busses */
int	targ = -1;	/* all targs */
int	lun = 0;	/* just lun 0 */

main(int argc, char **argv, char **envp)
{
	struct scsi_addr scaddr;
	struct scsi_inquiry_data dat;

	procargs(argc,argv,envp);
	if(reprobe) {
		scaddr.scbus = bus;
		scaddr.target = targ;
		scaddr.lun = lun;	

		if (ioctl(fd,SCIOCREPROBE,&scaddr) == -1)
		{
			perror("ioctl");
		}
	}
	if(dflag) {
		if (ioctl(fd,SCIOCDEBUG,&debuglevel) == -1) {
			perror("ioctl [SCIODEBUG]");
			exit(1);
		}
	}

	if(inqflag) {
		inq(fd,&dat);
		show_mem(&dat,sizeof(dat));
	}
}

/*
 * Do a scsi operation asking a device what it is
 * Use the scsi_cmd routine in the switch table.
 */
int inq(fd,inqbuf)
	int	fd;
	struct scsi_inquiry_data *inqbuf;
{
	struct scsi_inquiry *cmd;
	scsireq_t	req;
	cmd = (struct scsi_inquiry *) req.cmd;

	bzero(&req,sizeof(req));
	
	cmd->op_code = INQUIRY;
	cmd->length = sizeof(struct scsi_inquiry_data);

	req.flags = SCCMD_READ;		/* info about the request status and type */
	req.timeout = 2000;
	req.cmdlen = sizeof(*cmd);
	req.databuf = (caddr_t)inqbuf;	/* address in user space of buffer */
	req.datalen = sizeof(*inqbuf);	/* size of user buffer */
	if (ioctl(fd,SCIOCCOMMAND,&req) == -1)
	{
		perror("ioctl");
		exit (1);
	}
}


void
show_mem(address, num)
	unsigned char *address;
	int num;
{
	int x, y;
	printf("------------------------------");
	for (y = 0; y < num; y += 1) {
		if (!(y % 16))
			printf("\n%03d: ", y);
		printf("%02x ", *address++);
	}
	printf("\n------------------------------\n");
}


