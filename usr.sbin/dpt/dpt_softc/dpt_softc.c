/*
 *       Copyright (c) 1997 by Simon Shapiro
 *       All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/* dpt_softc.c:  Dunp a DPT control structure */

#ident "$FreeBSD$"

#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <sys/ioctl.h>
#include <scsi/scsi_all.h>
#include <scsi/scsi_message.h>
#include <scsi/scsiconf.h>

/*
 * The following two defines alter the size and composition of dpt_softc_t.
 * If useland does not match the kenel, disaster will ensue.
 * Since we do not know how to pick up kernel options from here,
 * and since we always use these options, we will enable them here.
 *
 * If you build a kernel without these options, edit here and recompile.
 */
#define DPT_MEASURE_PERFORMANCE

#include <sys/dpt.h>

static char i2bin_bitmap[48];	/* Used for binary dump of registers */

char	*
i2bin(unsigned int no, int length)
{
    int	ndx, rind;
    
    for (ndx = 0, rind = 0; ndx < 32; ndx++, rind++) {
	i2bin_bitmap[rind] = (((no << ndx) & 0x80000000) ? '1' : '0');
	
	if (((ndx % 4) == 3))
	    i2bin_bitmap[++rind] = ' ';
    }
	
    if ((ndx % 4) == 3)
	i2bin_bitmap[rind - 1] = '\0';
    else
	i2bin_bitmap[rind] = '\0';
    
    switch (length) {
    case 8:
	return (i2bin_bitmap + 30);
	break;
    case 16:
	return (i2bin_bitmap + 20);
	break;
    case 24:
	return (i2bin_bitmap + 10);
	break;
    case 32:
	return (i2bin_bitmap);
    default:
	return ("i2bin: Invalid length Specs");
	break;
    }
}
int
main(int argc, char **argv, char **argp)
{
    dpt_user_softc_t udpt;
    int result;
    int fd;

    if ( (fd = open(argv[1], O_RDWR, S_IRUSR | S_IWUSR)) == -1 ) {
	(void)fprintf(stderr, "%s ERROR:  Failed to open \"%s\" - %s\n",
		      argv[0], argv[1], strerror(errno));
	exit(1);
    }

    if ( (result = ioctl(fd, DPT_IOCTL_SOFTC, &udpt)) != 0 ) {
		(void)fprintf(stderr, "%s ERROR:  Failed to send IOCTL %lx - %s\n",
					  argv[0], DPT_IOCTL_SOFTC,
					  strerror(errno));
		exit(2);
    }

    (void)fprintf(stdout, "Counters:%d:%d:%d:%d:%d:%d:%d\n",
		  udpt.total_ccbs_count,
		  udpt.free_ccbs_count,
		  udpt.waiting_ccbs_count,
		  udpt.submitted_ccbs_count,
		  udpt.completed_ccbs_count,
		  udpt.commands_processed,
		  udpt.lost_interrupts);

    (void)fprintf(stdout, "Queue Status:%s\n",
		  i2bin(udpt.queue_status, sizeof(udpt.queue_status) * 8));
    
    (void)fprintf(stdout, "Free lock:%s\n",
		  i2bin(udpt.free_lock, sizeof(udpt.free_lock) * 8));
    
    (void)fprintf(stdout, "Waiting lock:%s\n",
		  i2bin(udpt.waiting_lock, sizeof(udpt.waiting_lock) * 8));
    
    (void)fprintf(stdout, "Submitted lock:%s\n",
		  i2bin(udpt.submitted_lock, sizeof(udpt.submitted_lock) * 8));
    
    (void)fprintf(stdout, "Completed lock:%s\n",
		  i2bin(udpt.completed_lock, sizeof(udpt.completed_lock) * 8));
    
    (void)fprintf(stdout, "Configuration:%s:%d:%d:%d:%x:%d:%d\n",
		  udpt.handle_interrupts ? "Yes" : "No",
		  udpt.max_id,
		  udpt.max_lun,
		  udpt.channels,
		  udpt.io_base,
		  udpt.irq,
		  udpt.dma_channel);
    
    (void)fprintf(stdout, "ID:%x:%x:%s:%s:%s:%s:%x\n",
		  udpt.board_data.deviceType,
		  udpt.board_data.rm_dtq,
		  udpt.board_data.vendor,
		  udpt.board_data.modelNum,
		  udpt.board_data.firmware,
		  udpt.board_data.protocol,
		  udpt.EATA_revision);

    (void)fprintf(stdout,"Capabilities:%x:%d:%s:%s:%s:%s:%s\n",
		  udpt.bustype,
		  udpt.channels,
		  i2bin((u_int32_t)udpt.state, sizeof(udpt.state) * 8),
		  udpt.primary ? "Yes" : "No",
		  udpt.more_support ? "Yes" : "No",
		  udpt.immediate_support ? "Yes" : "No",
		  udpt.broken_INQUIRY ? "Yes" : "No");

    (void)fprintf(stdout,"More Config:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d\n",
		  udpt.resetlevel[0],
		  udpt.resetlevel[1],
		  udpt.resetlevel[2],
		  udpt.cplen,		
		  udpt.cppadlen,
		  udpt.queuesize,
		  udpt.sgsize,
		  udpt.hostid[0],
		  udpt.hostid[1],
		  udpt.hostid[2]);

    (void)fprintf(stdout,"Cache:%s:%d\n",
		  (udpt.cache_type == DPT_NO_CACHE)
		  ? "None"
		  : (udpt.cache_type == DPT_CACHE_WRITETHROUGH)
		  ? "Write-Through" : "Write-Back",
		  udpt.cache_size);

    return(0);
}
