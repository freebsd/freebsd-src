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

/* dpt_sig.c:  Dunp a DPT Signature */

#ident "$FreeBSD$"

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

/* A primitive subset of isgraph.	Used by hex_dump below */
#define IsGraph(val)	( (((val) >= ' ') && ((val) <= '~')) )

/*
 * This function dumps bytes to the screen in hex format.
 */
void
hex_dump(u_int8_t * data, int length, char *name, int no)
{
    int	line, column, ndx;
	
    (void)fprintf(stdout, "Kernel Hex Dump for %s-%d at %p (%d bytes)\n",
				  name, no, data, length);
	
    /* Zero out all the counters and repeat for as many bytes as we have */
    for (ndx = 0, column = 0, line = 0; ndx < length; ndx++) {
		/* Print relative offset at the beginning of every line */
		if (column == 0)
			(void)fprintf(stdout, "%04x ", ndx);
	
		/* Print the byte as two hex digits, followed by a space */
		(void)fprintf(stdout, "%02x ", data[ndx]);
	
		/* Split the row of 16 bytes in half */
		if (++column == 8) {
			(void)fprintf(stdout, " ");
		}
		/* St the end of each row of 16 bytes, put a space ... */
		if (column == 16) {
			(void)fprintf(stdout, "	");
		
			/* ... and then print the ASCII-visible on a line. */
			for (column = 0; column < 16; column++) {
				int	ascii_pos = ndx - 15 + column;
	
				/*
				 * Non-printable and non-ASCII are just a
				 * dot. ;-(
				 */
				if (IsGraph(data[ascii_pos]))
					(void)fprintf(stdout, "%c", data[ascii_pos]);
				else
					(void)fprintf(stdout, ".");
			}
		
			/* Each line ends with a new line */
			(void)fprintf(stdout, "\n");
			column = 0;
		
			/*
			 * Every 256 bytes (16 lines of 16 bytes each) have
			 * an empty line, separating them from the next
			 * ``page''. Yes, I programmed on a Z-80, where a
			 * page was 256 bytes :-)
			 */
			if (++line > 15) {
				(void)fprintf(stdout, "\n");
				line = 0;
			}
		}
    }
	
    /*
     * We are basically done. We do want, however, to handle the ASCII
     * translation of fractional lines.
     */
    if ((ndx == length) && (column != 0)) {
		int	modulus = 16 - column, spaces = modulus * 3, skip;
	
		/*
		 * Skip to the right, as many spaces as there are bytes
		 * ``missing'' ...
		 */
		for (skip = 0; skip < spaces; skip++)
			(void)fprintf(stdout, " ");
	
		/* ... And the gap separating the hex dump from the ASCII */
		(void)fprintf(stdout, "  ");
	
		/*
		 * Do not forget the extra space that splits the hex dump
		 * vertically
		 */
		if (column < 8)
	    (void)fprintf(stdout, " ");
	
		for (column = 0; column < (16 - modulus); column++) {
			int	ascii_pos = ndx - (16 - modulus) + column;
		
			if (IsGraph(data[ascii_pos]))
				(void)fprintf(stdout, "%c", data[ascii_pos]);
			else
				(void)fprintf(stdout, ".");
		}
		(void)fprintf(stdout, "\n");
    }
}


int
main(int argc, char **argv, char **argp)
{
    eata_pt_t pass_thru;
    dpt_sig_t signature;
    char     *sp1;
    char     *sp2;
    
    int result;
    int fd;
    int ndx;

    /* If we do not do that, gcc complains about uninitialized usage (?) */
    sp1 = "Unknown";
    sp2 = "Unknown";
    
    if ( (fd = open(argv[1], O_RDWR, S_IRUSR | S_IWUSR)) == -1 ) {
		(void)fprintf(stderr, "%s ERROR:  Failed to open \"%s\" "
			      "- %s\n",
			      argv[0], argv[1], strerror(errno));
		exit(1);
    }

    pass_thru.eataID[0] = 'E';
    pass_thru.eataID[1] = 'A';
    pass_thru.eataID[2] = 'T';
    pass_thru.eataID[3] = 'A';
    pass_thru.command   = DPT_SIGNATURE;
    pass_thru.command_buffer = (u_int8_t *)&signature;

    if ( (result = ioctl(fd, DPT_IOCTL_SEND, &pass_thru)) != 0 ) {
		(void)fprintf(stderr, "%s ERROR:  Failed to send IOCTL "
			      "%lx - %s\n",
			      argv[0], DPT_IOCTL_SEND,
			      strerror(errno));
		exit(1);
    }

    /* dsSignature is not null terminated! */
    for (ndx = 0; ndx < sizeof(signature.dsSignature); ndx++)
		(void)fputc(signature.dsSignature[ndx], stdout);
    
    (void)fprintf(stdout, ":%x:", signature.SigVersion);

    switch (signature.ProcessorFamily) {
    case PROC_INTEL:
		sp1 = "Intel";
	switch ( signature.Processor ) {
	case PROC_8086:
	    sp2 = "8086";
	    break;
	case PROC_286:
	    sp2 = "80286";
	    break;
	case PROC_386:
	    sp2 = "386";
	    break;
	case PROC_486:
	    sp2 = "486";
	    break;
	case PROC_PENTIUM:
	    sp2 = "Pentium";
	    break;
	case PROC_P6:
	    sp2 = "PentiumPro";
	    break;
	default:
	    sp2 = "Unknown Processor";
	    break;
	}
	break;
    case PROC_MOTOROLA:
		sp1 = "Motorola";
	switch ( signature.Processor ) {
	case PROC_68000:
	    sp2 = "68000";
	    break;
	case PROC_68020:
	    sp2 = "68020";
	    break;
	case PROC_68030:
	    sp2 = "68030";
	    break;
	case PROC_68040:
	    sp2 = "68040";
	    break;
	default:
	    sp2 = "Unknown Processor";
	    break;
	}
	break;
    case PROC_MIPS4000:
		sp1 = "MIPS/SGI";
		break;
    case PROC_ALPHA:
		sp1 = "DEC Alpha";
		break;
    default:
		sp1 = "Unknown Processor Family";
		break;
    }
    
    (void)fprintf(stdout, "%s:%s:", sp1, sp2);

    switch ( signature.Filetype ) {
    case FT_EXECUTABLE:
		sp1 = "Executable";
		break;
    case FT_SCRIPT:
		sp1 = "Script";
		break;
    case FT_HBADRVR:
		sp1 = "HBA Driver";
		break;
    case FT_OTHERDRVR:
		sp1 = "Other Driver";
		break;
    case FT_IFS:
		sp1 = "Installable FileSystem";
		break;
    case FT_ENGINE:
		sp1 = "DPT Engine";
		break;
    case FT_COMPDRVR:
		sp1 = "Compressed Driver";
		break;
    case FT_LANGUAGE:
		sp1 = "Language File";
		break;
    case FT_FIRMWARE:
		sp1 = "DownLoadable Firmware";
		break;
    case FT_COMMMODL:
		sp1 = "Communications Module";
		break;
    case FT_INT13:
		sp1 = "INT13 Type HBA Driver";
		break;
    case FT_HELPFILE:
		sp1 = "Help File";
		break;
    case FT_LOGGER:
		sp1 = "Event Logger";
		break;
    case FT_INSTALL:
		sp1 = "Installation Procedure";
		break;
    case FT_LIBRARY:
		sp1 = "Storage Manager Real-Mode Call";
		break;
    case FT_RESOURCE:
		sp1 = "Storage Manager Resource File";
		break;
    case FT_MODEM_DB:
		sp1 = "Storage Manager Modem Database";
		break;
    default:
		sp1 = "Unknown File Type";
		break;
    }
    
    switch ( signature.FiletypeFlags ) {
    case FTF_DLL:
		sp2 = "Dynamically Linked Library";
		break;
    case FTF_NLM:
		sp2 = "NetWare Loadable Module";
		break;
    case FTF_OVERLAYS:
		sp2 = "Uses Overlays";
		break;
    case FTF_DEBUG:
		sp2 = "Debug Version";
		break;
    case FTF_TSR:
		sp2 = "DOS Terminate-n-Stay Resident Thing";
		break;
    case FTF_SYS:
		sp2 = "DOS Loadable Driver";
		break;
    case FTF_PROTECTED:
		sp2 = "Runs in Protected Mode";
		break;
    case FTF_APP_SPEC:
		sp2 = "Application Specific";
		break;
    default:
		sp2 = "Unknown File Type Flag";
		break;
    }
    
    (void)fprintf(stdout, "%s:%s:", sp1, sp2);

    switch ( signature.OEM ) {
    case OEM_DPT:
		sp1 = "DPT";
		break;
    case OEM_ATT:
		sp1 = "AT&T";
		break;
    case OEM_NEC:
		sp1 = "NEC";
		break;
    case OEM_ALPHA:
		sp1 = "Alphatronix";
		break;
    case OEM_AST:
		sp1 = "AST";
		break;
    case OEM_OLIVETTI:
		sp1 = "Olivetti";
		break;
    case OEM_SNI:
		sp1 = "Siemens/Nixdorf";
		break;
    default:
		sp1 = "Unknown OEM";
		break;
    }
   
    switch ( signature.OS ) {
    case OS_DOS:
		sp2 = "DOS";
		break;
    case OS_WINDOWS:
		sp2 = "Microsoft Windows 3.x";
		break;
    case OS_WINDOWS_NT:
		sp2 = "Microsoft Windows NT";
		break;
    case OS_OS2M:
		sp2 = "OS/2 1.2.x,MS 1.3.0,IBM 1.3.x";
		break;
    case OS_OS2L:
		sp2 = "Microsoft OS/2 1.301 - LADDR";
		break;
    case OS_OS22x:
		sp2 = "IBM OS/2 2.x";
		break;
    case OS_NW286:
		sp2 = "Novell NetWare 286";
		break;
    case OS_NW386:
		sp2 = "Novell NetWare 386";
		break;
    case OS_GEN_UNIX:
		sp2 = "Generic Unix";
		break;
    case OS_SCO_UNIX:
		sp2 = "SCO Unix";
		break;
    case OS_ATT_UNIX:
		sp2 = "AT&T Unix";
		break;
    case OS_UNIXWARE:
		sp2 = "UnixWare Unix";
		break;
    case OS_INT_UNIX:
		sp2 = "Interactive Unix";
		break;
    case OS_SOLARIS:
		sp2 = "SunSoft Solaris";
		break;
    case OS_QNX:
		sp2 = "QNX for Tom Moch";
		break;
    case OS_NEXTSTEP:
		sp2 = "NeXTSTEP";
		break;
    case OS_BANYAN:
		sp2 = "Banyan Vines";
		break;
    case OS_OLIVETTI_UNIX:
		sp2 = "Olivetti Unix";
		break;
    case OS_FREEBSD:
		sp2 = "FreeBSD 2.2 and later";
		break;
    case OS_OTHER:
		sp2 = "Other";
		break;
    default:
		sp2 = "Unknown O/S";
		break;
    }
    
    (void)fprintf(stdout, "%s:%s:\n", sp1, sp2);

    if ( signature.Capabilities & CAP_RAID0 )
		(void)fprintf(stdout, "RAID-0:");
    
    if ( signature.Capabilities & CAP_RAID1 )
		(void)fprintf(stdout, "RAID-1:");
    
    if ( signature.Capabilities & CAP_RAID3 )
		(void)fprintf(stdout, "RAID-3:");
    
    if ( signature.Capabilities & CAP_RAID5 )
		(void)fprintf(stdout, "RAID-5:");
    
    if ( signature.Capabilities & CAP_SPAN )
		(void)fprintf(stdout, "SPAN:");
    
    if ( signature.Capabilities & CAP_PASS )
		(void)fprintf(stdout, "PASS:");
    
    if ( signature.Capabilities & CAP_OVERLAP )
		(void)fprintf(stdout, "OVERLAP:");
    
    if ( signature.Capabilities & CAP_ASPI )
		(void)fprintf(stdout, "ASPI:");
    
    if ( signature.Capabilities & CAP_ABOVE16MB )
		(void)fprintf(stdout, "ISA16MB:");
    
    if ( signature.Capabilities & CAP_EXTEND )
		(void)fprintf(stdout, "ISA16MB:");

    (void)fprintf(stdout, "\n");

    if ( signature.DeviceSupp & DEV_DASD )
		(void)fprintf(stdout, "DASD:");

    if ( signature.DeviceSupp & DEV_TAPE )
		(void)fprintf(stdout, "Tape:");

    if ( signature.DeviceSupp & DEV_PRINTER )
		(void)fprintf(stdout, "Printer:");

    if ( signature.DeviceSupp & DEV_PROC )
		(void)fprintf(stdout, "CPU:");

    if ( signature.DeviceSupp & DEV_WORM )
		(void)fprintf(stdout, "WORM:");

    if ( signature.DeviceSupp & DEV_CDROM )
		(void)fprintf(stdout, "CDROM:");

    if ( signature.DeviceSupp & DEV_SCANNER )
		(void)fprintf(stdout, "Scanner:");

    if ( signature.DeviceSupp & DEV_OPTICAL )
		(void)fprintf(stdout, "Optical:");

    if ( signature.DeviceSupp & DEV_JUKEBOX )
		(void)fprintf(stdout, "Jukebox:");

    if ( signature.DeviceSupp & DEV_COMM )
		(void)fprintf(stdout, "Comm:");

    if ( signature.DeviceSupp & DEV_OTHER )
		(void)fprintf(stdout, "Other:");

    if ( signature.DeviceSupp & DEV_ALL )
		(void)fprintf(stdout, "All:");

    (void)fprintf(stdout, "\n");

    if ( signature.AdapterSupp & ADF_2001 )
		(void)fprintf(stdout, "PM2001:");

    if ( signature.AdapterSupp & ADF_2012A )
		(void)fprintf(stdout, "PM2012A:");

    if ( signature.AdapterSupp & ADF_PLUS_ISA )
		(void)fprintf(stdout, "PM2011+PM2021:");

    if ( signature.AdapterSupp & ADF_PLUS_EISA )
		(void)fprintf(stdout, "PM2012B+PM2022:");

    if ( signature.AdapterSupp & ADF_SC3_ISA )
		(void)fprintf(stdout, "PM2021:");

    if ( signature.AdapterSupp & ADF_SC3_EISA )
		(void)fprintf(stdout, "PM2022+PM2122:");

    if ( signature.AdapterSupp & ADF_SC3_PCI )
		(void)fprintf(stdout, "SmartCache III PCI:");

    if ( signature.AdapterSupp & ADF_SC4_ISA )
		(void)fprintf(stdout, "SmartCache IV ISA:");

    if ( signature.AdapterSupp & ADF_SC4_EISA )
		(void)fprintf(stdout, "SmartCache IV EISA:");

    if ( signature.AdapterSupp & ADF_SC4_PCI )
		(void)fprintf(stdout, "SmartCache IV PCI:");

    if ( signature.AdapterSupp & ADF_ALL_MASTER )
		(void)fprintf(stdout, "All Bus Mastering:");

    if ( signature.AdapterSupp & ADF_ALL_CACHE )
		(void)fprintf(stdout, "All Caching:");

    if ( signature.AdapterSupp & ADF_ALL )
		(void)fprintf(stdout, "All HBAs:");
    
    (void)fprintf(stdout, "\n");

    if ( signature.Application & APP_DPTMGR )
		(void)fprintf(stdout, "DPTMGR:");
    
    if ( signature.Application & APP_ENGINE )
		(void)fprintf(stdout, "Engine:");
    
    if ( signature.Application & APP_SYTOS )
		(void)fprintf(stdout, "Systron Sytos Plus:");
    
    if ( signature.Application & APP_CHEYENNE )
		(void)fprintf(stdout, "Cheyenne ARCServe + ARCSolo:");
    
    if ( signature.Application & APP_MSCDEX )
		(void)fprintf(stdout, "Microsoft CD-ROM extensions:");
    
    if ( signature.Application & APP_NOVABACK )
		(void)fprintf(stdout, "NovaStor Novaback:");
    
    if ( signature.Application & APP_AIM )
		(void)fprintf(stdout, "Archive Information Manager:");
    
    (void)fprintf(stdout, "\n");

    if ( signature.Requirements & REQ_SMARTROM )
		(void)fprintf(stdout, "SmartROM:");
    
    if ( signature.Requirements & REQ_DPTDDL )
		(void)fprintf(stdout, "DPTDDL.SYS:");
    
    if ( signature.Requirements & REQ_HBA_DRIVER )
		(void)fprintf(stdout, "HBA Driver:");
    
    if ( signature.Requirements & REQ_ASPI_TRAN )
		(void)fprintf(stdout, "ASPI Transport Modules:");
    
    if ( signature.Requirements & REQ_ENGINE )
		(void)fprintf(stdout, "DPT Engine:");
    
    if ( signature.Requirements & REQ_COMM_ENG )
		(void)fprintf(stdout, "DPT Comm Engine:");
    
    (void)fprintf(stdout, "\n");
    
    (void)fprintf(stdout, "%x.%x.%x:%d.%d.%d\n",
				  signature.Version, signature.Revision,
				  signature.SubRevision,
				  signature.Month, signature.Day, signature.Year + 1980);
    
    return(0);
}
