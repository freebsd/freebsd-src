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

/* dpt_dm.c: Dump a DPT metrics structure */

#ident "$FreeBSD: src/usr.sbin/dpt/dpt_dm/dpt_dm.c,v 1.3 1999/08/28 01:16:07 peter Exp $"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <sys/ioctl.h>
#include <scsi/scsi_all.h>
#include <scsi/scsi_message.h>
#include <scsi/scsiconf.h>

#define DPT_MEASURE_PERFORMANCE

#include <sys/dpt.h>

char	*
scsi_cmd_name(u_int8_t cmd)
{
    switch (cmd) {
    case 0x40:
	return ("Change Definition [7.1]");
	break;
    case 0x39:
	return ("Compare [7,2]");
	break;
    case 0x18:
	return ("Copy [7.3]");
	break;
    case 0x3a:
	return ("Copy and Verify [7.4]");
	break;
    case 0x04:
	return ("Format Unit [6.1.1]");
	break;
    case 0x12:
	return ("Inquiry [7.5]");
	break;
    case 0x36:
	return ("lock/Unlock Cache [6.1.2]");
	break;
    case 0x4c:
	return ("Log Select [7.6]");
	break;
    case 0x4d:
	return ("Log Sense [7.7]");
	break;
    case 0x15:
	return ("Mode select (6) [7.8]");
	break;
    case 0x55:
	return ("Mode Select (10) [7.9]");
	break;
    case 0x1a:
	return ("Mode Sense (6) [7.10]");
	break;
    case 0x5a:
	return ("Mode Sense (10) [7.11]");
	break;
    case 0xa7:
	return ("Move Medium Attached [SMC]");
	break;
    case 0x5e:
	return ("Persistent Reserve In [7.12]");
	break;
    case 0x5f:
	return ("Persistent Reserve Out [7.13]");
	break;
    case 0x1e:
	return ("Prevent/Allow Medium Removal [7.14]");
	break;
    case 0x08:
	return ("Read, Receive (6) [6.1.5]");
	break;
    case 0x28:
	return ("Read (10) [6.1.5]");
	break;
    case 0xa8:
	return ("Read (12) [6.1.5]");
	break;
    case 0x3c:
	return ("Read Buffer [7.15]");
	break;
    case 0x25:
	return ("Read Capacity [6.1.6]");
	break;
    case 0x37:
	return ("Read Defect Data (10) [6.1.7]");
	break;
    case 0xb7:
	return ("Read Defect Data (12) [6.2.5]");
	break;
    case 0xb4:
	return ("Read Element Status Attached [SMC]");
	break;
    case 0x3e:
	return ("Read Long [6.1.8]");
	break;
    case 0x07:
	return ("Reassign Blocks [6.1.9]");
	break;
    case 0x81:
	return ("Rebuild [6.1.10]");
	break;
    case 0x1c:
	return ("Receive Diagnostics Result [7.16]");
	break;
    case 0x82:
	return ("Regenerate [6.1.11]");
	break;
    case 0x17:
	return ("Release(6) [7.17]");
	break;
    case 0x57:
	return ("Release(10) [7.18]");
	break;
    case 0xa0:
	return ("Report LUNs [7.19]");
	break;
    case 0x03:
	return ("Request Sense [7.20]");
	break;
    case 0x16:
	return ("Resereve (6) [7.21]");
	break;
    case 0x56:
	return ("Reserve(10) [7.22]");
	break;
    case 0x2b:
	return ("Reserve(10) [6.1.12]");
	break;
    case 0x1d:
	return ("Send Disagnostics [7.23]");
	break;
    case 0x33:
	return ("Set Limit (10) [6.1.13]");
	break;
    case 0xb3:
	return ("Set Limit (12) [6.2.8]");
	break;
    case 0x1b:
	return ("Start/Stop Unit [6.1.14]");
	break;
    case 0x35:
	return ("Synchronize Cache [6.1.15]");
	break;
    case 0x00:
	return ("Test Unit Ready [7.24]");
	break;
    case 0x3d:
	return ("Update Block (6.2.9");
	break;
    case 0x2f:
	return ("Verify (10) [6.1.16, 6.2.10]");
	break;
    case 0xaf:
	return ("Verify (12) [6.2.11]");
	break;
    case 0x0a:
	return ("Write, Send (6) [6.1.17, 9.2]");
	break;
    case 0x2a:
	return ("Write (10) [6.1.18]");
	break;
    case 0xaa:
	return ("Write (12) [6.2.13]");
	break;
    case 0x2e:
	return ("Write and Verify (10) [6.1.19, 6.2.14]");
	break;
    case 0xae:
	return ("Write and Verify (12) [6.1.19, 6.2.15]");
	break;
    case 0x03b:
	return ("Write Buffer [7.25]");
	break;
    case 0x03f:
	return ("Write Long [6.1.20]");
	break;
    case 0x041:
	return ("Write Same [6.1.21]");
	break;
    case 0x052:
	return ("XD Read [6.1.22]");
	break;
    case 0x050:
	return ("XD Write [6.1.22]");
	break;
    case 0x080:
	return ("XD Write Extended [6.1.22]");
	break;
    case 0x051:
	return ("XO Write [6.1.22]");
	break;
    default:
	return ("Unknown SCSI Command");
    }
}

static const char *
tmpsprintf(int buffer, const char *format, ...) 
{
    static char buffers[16][16];
    va_list ap;

    va_start(ap, format);
    vsnprintf(buffers[buffer], 16, format, ap);
    va_end(ap);
    return buffers[buffer];
}


int
main(int argc, char **argv, char **argp)
{
    dpt_perf_t metrics;
    int result;
    int fd;
    int ndx;

    if ( (fd = open(argv[1], O_RDWR, S_IRUSR | S_IWUSR)) == -1 ) {
	(void)fprintf(stderr, "%s ERROR:  Failed to open \"%s\" - %s\n",
		      argv[0], argv[1], strerror(errno));
	exit(1);
    }

    if ( (result = ioctl(fd, DPT_IOCTL_INTERNAL_METRICS, &metrics)) != 0 ) {
	(void)fprintf(stderr, "%s ERROR:  Failed to send IOCTL %lx - %s\n",
		      argv[0], DPT_IOCTL_INTERNAL_METRICS,
		      strerror(errno));
	exit(2);
    }

    /* Interrupt related measurements */
    (void)fprintf(stdout, "Interrupts:%u:%u:%s:%u\n\nCommands:\n",
		  metrics.aborted_interrupts,
		  metrics.spurious_interrupts,
		  metrics.min_intr_time == BIG_ENOUGH ?
		  	"N/A" :
		  	tmpsprintf(0, "%u", metrics.min_intr_time),
		  metrics.max_intr_time);

    /* SCSI Commands, can be no more than 256 of them */
    for (ndx = 0; ndx < 256; ndx++) {
	if (metrics.command_count[ndx] != 0) {
	    (void)fprintf(stdout, "%u:%s:%u:%s:%d\n",
			  ndx,
			  scsi_cmd_name((u_int8_t)ndx), 
			  metrics.command_count[ndx],
			  metrics.min_command_time[ndx] == BIG_ENOUGH ?
			  	"N/A" :
			  	tmpsprintf(0, "%u", metrics.min_command_time[ndx]),
			  metrics.max_command_time[ndx]);
	}
    }
    
    (void)fprintf(stdout, "\nREAD by size:\n");

    /* READ/WRITE statistics, per block size */

    for ( ndx = 0; ndx < 10; ndx++) {		
	if (metrics.read_by_size_count[ndx] != 0) {
	    char* block_size;
	
	    switch ( ndx ) {
	    case SIZE_512:
		block_size = "512";
		break;
	    case SIZE_1K:	
		block_size = "1K";
		break;
	    case SIZE_2K:
		block_size = "2K";
		break;	
	    case SIZE_4K:	
		block_size = "4K";
		break;
	    case SIZE_8K:
		block_size = "8K";
		break;
	    case SIZE_16K:
		block_size = "16K";
		break;
	    case SIZE_32K:
		block_size = "32K";
		break;
	    case SIZE_64K:
		block_size = "64K";
		break;
	    case SIZE_BIGGER:
		block_size = "BIGGER";
		break;
	    case SIZE_OTHER:
		block_size = "OTHER";
		break;
	    default:
		block_size = "Gcc, shut up!";
	    }
	    
	    (void)fprintf(stdout, "%s:%u:%u:%u\n", block_size,
			  metrics.read_by_size_count[ndx],
			  metrics.read_by_size_min_time[ndx],
			  metrics.read_by_size_max_time[ndx]);
	}
    }
	    
    (void)fprintf(stdout, "\nWRITE by size:\n");

    for ( ndx = 0; ndx < 10; ndx++) {		
	if (metrics.write_by_size_count[ndx] != 0) {
	    char* block_size;
	
	    switch ( ndx ) {
	    case SIZE_512:
		block_size = "512";
		break;
	    case SIZE_1K:	
		block_size = "1K";
		break;
	    case SIZE_2K:
		block_size = "2K";
		break;	
	    case SIZE_4K:	
		block_size = "4K";
		break;
	    case SIZE_8K:
		block_size = "8K";
		break;
	    case SIZE_16K:
		block_size = "16K";
		break;
	    case SIZE_32K:
		block_size = "32K";
		break;
	    case SIZE_64K:
		block_size = "64K";
		break;
	    case SIZE_BIGGER:
		block_size = "BIGGER";
		break;
	    case SIZE_OTHER:
		block_size = "OTHER";
		break;
	    default:
		block_size = "Gcc, shut up!";
	    }
	    
	    (void)fprintf(stdout, "%s:%u:%u:%u\n", block_size,
			  metrics.write_by_size_count[ndx],
			  metrics.write_by_size_min_time[ndx],
			  metrics.write_by_size_max_time[ndx]);
	}
	
    }
    
    (void)fprintf(stdout, "\nQueues:%u:%s:%u:%u:%s:%u:%u:%s:%u\n", 
		  metrics.max_waiting_count,
		  metrics.min_waiting_time == BIG_ENOUGH ?
		  	"N/A" :
		  	tmpsprintf(0, "%u", metrics.min_waiting_time),
		  metrics.max_waiting_time,
		  metrics.max_submit_count,
		  metrics.min_submit_time == BIG_ENOUGH ?
		  	"N/A" :
		  	tmpsprintf(1, "%u", metrics.min_submit_time),
		  metrics.max_submit_time,
		  metrics.max_complete_count,
		  metrics.min_complete_time == BIG_ENOUGH ?
		  	"N/A" :
		  	tmpsprintf(2, "%u", metrics.min_complete_time),
		  metrics.max_complete_time);

    (void)fprintf(stdout, "Hardware Ports:%u:%u:%u:%s\n",
		  metrics.command_collisions,
		  metrics.command_too_busy,
		  metrics.max_eata_tries,
		  metrics.min_eata_tries == BIG_ENOUGH ?
		  	"N/A" :
		    	tmpsprintf(0, "%u", metrics.min_eata_tries));

    return(0);
}
