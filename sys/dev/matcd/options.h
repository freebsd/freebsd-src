/*options.h--------------------------------------------------------------------

	Matsushita(Panasonic) / Creative CD-ROM Driver	(matcd)
	Authored by Frank Durda IV

Copyright 1994, 1995, 2002, 2003  Frank Durda IV.  All rights reserved.
"FDIV" is a trademark of Frank Durda IV.

------------------------------------------------------------------------------

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. Neither the name of the author nor the names of their contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.

------------------------------------------------------------------------------

See matcd.c for Edit History
*/

/* $FreeBSD$
*/

/*----------------------------------------------------------------------------
	Conditional compilation flags - change to suit your system
----------------------------------------------------------------------------*/

/*	AUTOHUNT	Adds extra code that allows the driver to search
			for interface cards rather than having to hard-code
			the locations in the kernel /boot/device.hint file,
			which is only read at boot-time.

			Leaving AUTOHUNT enabled is the recommended setting.
*/

/*#define AUTOHUNT*/


/*	NUMCTRLRS	Configures support for between one and four host
			interfaces, for up to 16 drives.  The number of
			entries in the kernel config file is used by default,
			but this may be changed to a specific value if
			desired.   The number of controllers cannot exceed 4.

			Setting the value smaller than four slightly reduces
			the amount of memory used by the driver, but since
			the driver is under 30K, it probably isn't worth
			changing.
*/

#define	NUMCTRLRS	4	/*Limit driver to four host interfaces*/


/*	FULLDRIVER	If not set, the audio, non-data functions and some
			error recovery functions are eliminated from the
			compiled driver.  The resulting driver will be smaller,
			savings which may be needed on a boot or fixit floppy.

			Leaving FULLDRIVER enabled is the recommended setting.
*/

#ifndef BOOTMFS
#define FULLDRIVER
#endif /*BOOTMFS*/


/*	RESETONBOOT	This setting causes the driver to reset the drive(s)
			during probing.  This causes any audio playback to be
			aborted and the drives will close their trays if they
			are open.

			Leaving RESETONBOOT enabled is the recommended setting.
*/

#define RESETONBOOT


/*	LOCKDRIVE	If enabled, when a drive is opened using a minor
			number greater than 127, the drive door is locked.
			The drive door remains locked until all partitions on
			on the drive are closed.  The EJECT, ALLOW and PREVENT
			ioctls are refused when this locking mechanism is
			active.  The additional code for this feature is small.

			Leaving LOCKDRIVE enabled is the recommended setting.
*/

#define LOCKDRIVE

/*	KRYTEN		This enables a bug that someone might consider to be a
			a feature.  If KRYTEN is enabled and you are playing
			audio and you issue the resume-play ioctl, the audio
			will stutter, playing the same quarter of a second or
			so of audio several times before resuming normally.
			Resuming from a pause never causes the stutter.
			See Edit History for Edit 14.

			Leaving KRYTEN disabled is the recommended setting.
*/

/*#define KRYTEN*/


/*---------------------------------------------------------------------------
	This structure contains the hints for where we should look for the
	host adapter.  If you want to change where the driver searches or
	reduce the places searched to avoid confusing some other device,
	either specify explicit addresses in the /boot/device.hints file
	(preferred) or change this array.

	If the /boot/device.hints config file has multiple ? entries for
	the matcd driver, the probe routines will use this table multiple
	times and will eliminate each failed entry that probe tries.

	WARNING:  The number of controller entries for this driver in
	/boot/device.hints must be less than or equal to the number of hints
	if hints are used.

	If you add entries to the table, add them immediately before
	the -1 end-of-table marker.  The values already present are
	the ones used by Creative Labs boards and those of a few
	other vendors.

	Each additional entry increases the boot time by four seconds,
	and can increase the chance of accessing some other device by mistake.
	Therefore, the list should be kept to a minimum.  Once the
	devices have been correctly located, the system should be
	configured so that it looks only at the correct location from
	that point on.

	Be sure to search devices located below 0x3ff BEFORE scanning
	higher locations.  Some boards don't decode all I/O address lines,
	so 0x230 and 0x630 appear identical on the simpler boards.
---------------------------------------------------------------------------*/

#ifdef AUTOHUNT
#ifdef DRIVESPERC		/*Declared only when compiling matcd.c*/
int	port_hints[]={
			0x230,	/*SB Pro & SB16*/
			0x240,	/*SB Pro & SB16*/
			0x250,	/*Creative omniCD standalone boards*/
			0x260,	/*Creative omniCD standalone boards*/
			0x340,	/*Laser Mate*/
			0x360,	/*Laser Mate*/
			0x630,	/*IBM*/
#if 0
/*	These locations are alternate settings for LaserMate and IBM
	boards, but they usually conflict with network and SCSI cards.
	I recommend against probing these randomly.
*/
			0x310,	/*Laser Mate*/
			0x320,	/*Laser Mate*/
			0x330,	/*Laser Mate*/
			0x350,	/*Laser Mate*/
			0x370,	/*Laser Mate*/
			0x650,	/*IBM*/
			0x670,	/*IBM*/
			0x690,	/*IBM*/
#endif /*0*/
			-1};	/*use.  Table MUST end with -1*/
#endif /*DRIVESPERC*/
#endif /*AUTOHUNT*/


/*---------------------------------------------------------------------------
	Debugging flags - Turn these on only if you are looking at a
			problem.
---------------------------------------------------------------------------*/

/*	DEBUGOPEN	If enabled, debug messages for open and close
			operations.
*/

/*#define    	DEBUGOPEN*/


/*	DEBUGIO		If enabled, reports on calls to strategy, start
			and other I/O related functions.
*/

/*#define   	DEBUGIO*/


/*	DEBUGCMD	If enabled, shows the actual commands being issued
			to the CD-ROM drives.
*/

/*#define 	DEBUGCMD*/


/*	DEBUGSLEEP	If enabled, reports on timeouts, wakeups, dropped
			threads, etc.
*/

/*#define 	DEBUGSLEEP*/


/*	DEBUGIOCTL	If enabled, reports on the various ioctl-related
			calls and operations.  You might have to enable
			DEBUGCMD as well to get enough debugging information.
*/

/*#define 	DEBUGIOCTL*/


/*	DEBUGPROBE	If enabled, reports on the process of locating
			adapters and drives.  The debugging in matcdprobe()
			and matcdattach() routines is enabled with this
			flag.
*/

/*#define 	DEBUGPROBE*/

/*End of options.h*/

