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

/* dpt_ctlinfo.c:  Dunp a DPT HBA Information Block */

#ident "$Id: dpt_ctlinfo.c,v 1.1 1998/01/22 23:32:27 ShimonR Exp ShimonR $"

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

#define DPT_MEASURE_PERFORMANCE

#include <sys/dpt.h>


int
main(int argc, char **argv, char **argp)
{
    eata_pt_t		pass_thru;
	dpt_compat_ha_t compat_softc;
    
    int result;
    int fd;
    int ndx;
    
    if ( (fd = open(argv[1], O_RDWR, S_IRUSR | S_IWUSR)) == -1 ) {
		(void)fprintf(stderr, "%s ERROR:  Failed to open \"%s\" - %s\n",
					  argv[0], argv[1], strerror(errno));
		exit(1);
    }

    pass_thru.eataID[0] = 'E';
    pass_thru.eataID[1] = 'A';
    pass_thru.eataID[2] = 'T';
    pass_thru.eataID[3] = 'A';
    pass_thru.command   = DPT_CTRLINFO;
    pass_thru.command_buffer = (u_int8_t *)&compat_softc;

    if ( (result = ioctl(fd, DPT_IOCTL_SEND, &pass_thru)) != 0 ) {
		(void)fprintf(stderr, "%s ERROR:  Failed to send IOCTL %x - %s\n",
					  argv[0], DPT_IOCTL_SEND,
					  strerror(errno));
		exit(1);
    }

	(void)fprintf(stdout, "%x:", compat_softc.ha_state);

	for (ndx = 0; ndx < MAX_CHANNELS; ndx++)
		(void)fprintf(stdout, (ndx == (MAX_CHANNELS - 1)) ? "%d:" : "%d,",
							   compat_softc.ha_id[ndx]);

	(void)fprintf(stdout, "%d:", compat_softc.ha_vect);
	(void)fprintf(stdout, "%x:", compat_softc.ha_base);
	(void)fprintf(stdout, "%d:", compat_softc.ha_max_jobs);

	switch (compat_softc.ha_cache) {
	case DPT_NO_CACHE:
		(void)fprintf(stdout, "No Cache:");
		break;
	case DPT_CACHE_WRITETHROUGH:
		(void)fprintf(stdout, "WriteThrough:");
		break;
	case DPT_CACHE_WRITEBACK:
		(void)fprintf(stdout, "WriteBack:");
		break;
	default:
		(void)fprintf(stdout, "UnKnown (%d):", compat_softc.ha_cache);
	}

	(void)fprintf(stdout, "%d:", compat_softc.ha_cachesize);
	(void)fprintf(stdout, "%d:", compat_softc.ha_nbus);
	(void)fprintf(stdout, "%d:", compat_softc.ha_ntargets);
	(void)fprintf(stdout, "%d:", compat_softc.ha_nluns);
	(void)fprintf(stdout, "%d:", compat_softc.ha_tshift);
	(void)fprintf(stdout, "%d:", compat_softc.ha_bshift);

	(void)fprintf(stdout, "%d:", compat_softc.ha_npend);
	(void)fprintf(stdout, "%d:", compat_softc.ha_active_jobs);

	(void)fprintf(stdout, "%s\n", compat_softc.ha_fw_version);



    return(0);
}
