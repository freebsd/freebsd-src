/*matcd.c--------------------------------------------------------------------

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

The original version of this software was developed specifically for the
FreeBSD Project.

Dedicated to:	My family, my Grandfather,
		and Max, my Golden Retriever

Thanks to:	Jordan Hubbard (jkh) for getting me ramped-up to 2.x system
		quickly enough to make the 2.1 release.  He put up with
		plenty of silly questions and might get the post of
		ambassador some day.

and 		The people who donated equipment and other material to make
		development of this driver possible.  Donations and
		sponsors for projects are appreciated.

-----No changes should be needed above this line-----------------------------

Edit History - (should be in sync with any source control log entries)

	Never seen one of these sections before?  Ok, here is how it works.
	Every time you change the code, you increment the ANSI edit number,
	that number over there in the <%d> and in the (%d) in the
	version string.  You never set this number lower than it is.
	Near, or preferably on lines that change, insert the edit
	number.  If there is a number there already, you can replace it
	with a newer one.  This makes searches for code changes very fast.

	In the edit history, start with the edit number, and a good
	description of what changes were made.  Then follow it with
	the date, your name and an EMAIL address where you can be reached.

	Please follow this practice; it helps leave understandable code in
	your wake.

	ANSI version codes have major and minor release numbers.  Major
	releases numbered 1 thru n.  Major feature additions should get a new
	major release number.  Minor releases start with a null and then
	letters A thru Z as needed.  So  3A(456) is Major release 3, Minor
	release 1, Edit 456 (the equivalent in XX.XX.XX version numbering used
	by certain monopolies would be 03.01.456), and 5(731) is Major release
	5, Minor release 0, Edit 731.  Typically only the author will change
	the major and minor release codes in small projects.

				EDIT edit Edit HISTORY history History

<1>	This initial version is to get basic filesystem I/O working
	using the SoundBlaster 16 interface.  The stand-alone adapter
	card doesn't work yet.
	December 1994  Frank Durda IV	uhclem at freebsd.org

<2>	Corrections to resolve a race condition when multiple drives
	on the same controller was active.  Fixed drive 1 & 2 swap
	problem.  See selectdrive().
	21-Jan-1995  Frank Durda IV	uhclem at freebsd.org

<3>	Added automatic probing and support for all Creative Labs sound
	cards with the Creative/Panasonic interface and the stand-alone
	interface adapters.  See AUTOHUNT and FULLCONFIG conditionals
	for more information.
	21-Jan-1995  Frank Durda IV	uhclem at freebsd.org

<4>	Rebundled debug conditionals.
	14-Feb-1995  Frank Durda IV	uhclem at freebsd.org

<5>	Changes needed to work on FreeBSD 2.1.  Also added draincmd
	since some conditions cause the drive to produce surprise data.
	See setmode and draincmd
	19-Feb-1995  Frank Durda IV	uhclem at freebsd.org

<6>	Got rid of some redundant error code by creating chk_error().
	Also built a nice generic bus-lock function.
	20-Feb-1995  Frank Durda IV	uhclem at freebsd.org

<7>	Improved comments, general structuring.
	Fixed a problem with disc eject not working if LOCKDRIVE was set.
	Apparently the drive will reject an EJECT command if the drive
	is LOCKED.
	21-Feb-1995  Frank Durda IV	uhclem at freebsd.org

<8>	Final device name selected and actually made to compile under
	>2.0. For newer systems, it is "matcd", for older it is "mat".
	24-Feb-1995  Frank Durda IV	uhclem at freebsd.org

<9>	Added some additional disk-related ioctl functions that didn't
	make it into earlier versions.
	26-Feb-1995  Frank Durda IV	uhclem at freebsd.org

<10>	Updated some conditionals so the code will compile under
	1.1.5.1, although this is not the supported platform.
	Also found that some other devices probe code was changing the
	settings for the port 0x302 debug board, so added code to set it
	to a sane state before we use it.
	26-Feb-1995  Frank Durda IV	uhclem at freebsd.org

<11>	The Copyright and Use statement has been replaced in all files
	with a new version.
	01-Mar-1995  Frank Durda IV	uhclem at freebsd.org

<12>	Added ioctls having to do with audio volume, routing and playback
	speed.  Also added some code I think is for dynamic loading.
	12-Mar-1995  Frank Durda IV	uhclem at freebsd.org

<13>	Added ioctls to return TOC headers and entries.
	19-Mar-1995  Frank Durda IV	uhclem at freebsd.org

<14>	More ioctls to finish out general audio support and some clean-up.
	Also fixed a bug in open where CD label information would not
	always be cleared after a disc change.

	Added a check to block attempts to resume audio if already playing.
	The resulting sound is a cross between Kryten and Max Headroom.
	But, if you *want* this "feature", enable #define KRYTEN
	in options.h.

	Complaints about too many comments are noted but that is all.  :-)
	21-Mar-1995  Frank Durda IV	uhclem at freebsd.org

<15>	LOCKDRIVE has been modified so that a new series of minor
	numbers are created.  When these are opened, the selected
	drive will have its door locked and the device must be completely
	closed to unlock the media.  The EJECT ioctl will be refused
	when the drive is locked this way.   This is useful for
	servers and other places where the media needs to remain in the
	drive.  Bit 7 of the minor number controls locking.

	As of this edit, the code compiles with no warnings with -Wall set.
	22-Mar-1995  Frank Durda IV	uhclem at freebsd.org

<16>	Added a new check in the probe code that looks for the drive
	interface being in an idle state after issuing a reset.  If this
	isn't the case, then the device at this location isn't a
	Matsushita CD-ROM drive.  This will prevent hangs in draincmd later.
	Added the tray close ioctl.  This required modifications to open
	to allow the character devices to be "partially" opened so that
	the close ioctl could be issued when the open would otherwise fail.
	Close also delays slightly after completing because the drive
	doesn't update its disc and media status instantly.
	Also created the capability ioctl that lets an application find out
	up front what things a drive can do.
	Fixed a global spelling error.
	Changed matcddriver structure to simply say "matcd".  The original
	string "matcd interface " broke the kernel -c boot mechanism.
	Updated the #includes in response to a complaint in first release.
	Updated and tested conditionals so that driver will still compile
	under FreeBSD 1.1.5.1 as well as 2.0 and early 2.1.
	04-Apr-1995  Frank Durda IV	uhclem at freebsd.org

<17>	The function matcd_toc_entries which is executed in response to
	the CDIOREADTOCENTRYS ioctl didn't cope with programs that only
	requested part of the TOC.  This change is based on code submitted
	by Doug Robson (dfr@render.com).
	(This change was introduced out of order and exists in FreeBSD
	 2.0.5 without the version stamp being updated.  I.N.M.F.)
	01-Jun-1995  Frank Durda IV	uhclem at freebsd.org

<18>	While working on the TEAC CD-ROM driver (teaccd) that is reusing
	chunks of code from this driver, I discovered several functions,
	arrays and other things that should have been declared 'static'.
	These changes are necessary if the TEAC CD-ROM driver is to be
	present at the same time as matcd.
	Also fixed the residual buss vs bus symbols and strings.
	There are no functional code changes in this edit.
	02-May-1995  Frank Durda IV	uhclem at freebsd.org

<19>	Creative has changed the Status port slightly in their
	sound boards based on the Vibra-16 (and probably the Vibra-16S)
	chipset.  This change masks some unused bits that were formally
	on all the time and are doing different things in this design.
	The changes are transparent to all other supported boards.
	20-Jun-1995  Frank Durda IV	uhclem at freebsd.org

<20>	Code was added to detect non-Creative (SoundBlaster) host
	interfaces, and the driver will  switch to code compatible with the
	detected host interface.  This should add support for MediaVision,
	IBM, Reveal, and other compatible adapters with split
	data/status-ports.  This code allows a mix of SoundBlaster (Type 0)
	and non-SoundBlaster (Type 1) boards in the same system with no
	special configuration.

	I also updated the attach code to display the interface type and
	changed the host interface probe messages to reflect the "c" for
	controller in controller-specific messages as the existing messages
	were confusing when a second card was in place .  The kernel -c
	tables have been updated accordingly, so you now have a matcdc%d
	controller to change settings on.
	24-Jun-95  Frank Durda IV	uhclem at freebsd.org

<21>	Added interface handling code in two of those "this should not
	happen" routines, draincmd and get_stat.   Since these routines are
	called by functions during probing that may not know what type
	interface is out there, the code assumes that a given adapter is
	both a type 0 and a type 1 adapter at the same time.  Plus,
	this code gets executed once in a very long time so the cost of
	assuming both host adapter types is not significant.
	04-Jul-1995  Frank Durda IV	uhclem at freebsd.org

<22>	Four external interface prototypes were altered by someone else.
	I believe these changes are for making GCC and/or the linker shut-up
	when building some other part of the system since matcd already
	compiles -Wall with no warnings...
	08-Sep-1995  Frank Durda IV	uhclem at freebsd.org

<23>	This change implements the ioctls for preventing media removal
	and allowing media removal.
	Currently, these calls will work according to the following rules:
			No "l" devs opened	Any "l" dev open
	CDALLOW		accepted always		rejected always
	CDPREVENT	accepted always		accepted always

	One refinement might be to allow CDALLOW/CDPREVENT to always
	work if UID 0 issued the ioctl, but that will wait for later.

	I also made a change to the information that the toc_entry code
	returns so that xcdplayer won't malfunction.  (It would not play
	the last track on a non-mixed mode audio CD.)  Unlike cdplayer,
	xcdplayer asks for track information one track at a time, and
	calls for information on the lead-out track by its official
	number (0xaa), rather than referring to the "after last" (n+1) track
	as cdplayer does.  Anyway, this change should make both players
	happy.
	16-Sep-1995  Frank Durda IV	uhclem at freebsd.org

<24>	In Edit 15 when the extra devs were created for selective locking,
	the door locking was broken if a non-locking dev on the drive is
	closed.  The problem was caused by not tracking locked devs and
	non-locking devs as being different partitions.   The change is to
	simply use the locking dev bit to flag a set of shadow partitions
	when it comes to lock operations.  All other operations treat the
	locked and unlocked partitions as being identical.
	18-Sep-1995  Frank Durda IV	uhclem at freebsd.org

<25>	During work on Edit 23, I noted that on slow and very busy systems,
	sometimes the driver would go to sleep forever.  The problem appears
	to have been a race condition caused by doing separate timeout/sleep
	calls without using SPL first.  The change here is to use tsleep
	which provides the equivalent of timeout/sleep timeout/tsleep if the
	last paremeter is tsleep is set to the time value that would have been
	given to timeout.
	I also fixed some duplicate location strings in the tsleep calls.
	24-Sep-1995  Frank Durda IV	uhclem at freebsd.org

<26>	Moved a function declaration that generated two warnings with
	the FULLCONFIG/FULLDRIVER conditionals disabled.
	Updated the igot function so that it correctly reports limited
	functions when a sub-set driver is compiled.
	Eliminated FULLCONFIG conditional and now set controller counts
	based on the NMATCD #define produced by the config process.
	Also, disable the audio-related ioctls based on the BOOTMFS
	conditional to help make the boot floppy kernel smaller.
	18-Oct-1995  Frank Durda IV	uhclem at freebsd.org

<27>	An unknown number of modifications were made by third parties
	after edit 26, but they neglected to document everything that
	was changed and why.  These are known collectively as edit 27.

	All Edit 27 changes were later discarded as part of the creation
	of Release 3, as some of the changes became incompatible with
	FreeBSD 4.x/5.x, and some broke drive operations that used to work.
	January 2003	Frank Durda IV	uhclem at freebsd.org

Begin work on Major Release 3 here

<28>	Begin work on converting the code to run on FreeBSD 5.0.  This
	included making the driver into a loadable kernel module, and
	interfacing with the new port allocator scheme.  This code is not
	based off what was in 4.6, but comes from the last version I
	personally submitted to the project, thus eliminating some fluff that
	crept in over the years.  The conditionally-compiled sections for
	building on pre-FreeBSD 2.0 releases have been removed, as it was
	likely not useful to anyone anymore.

	I reclaimed Major device 46 for matcd - it was dropped from the 5.0
	"majors" file - but as of this date it has not been reassigned to
	some other device.
	10-Jan-2003  Frank Durda IV	uhclem at freebsd.org

<29>	Work to adapt to the new way of passing key settings around between
	functions and calls to the driver that seems to be forced on loadable
	drivers or perhaps all drivers.  This required substantial debugging
	as the port addresses identified during probing were not reaching
	all the functions that needed that information.
	14-Jan-2003  Frank Durda IV	uhclem at freebsd.org

<30>	Work continues, and matcd successfully loads and unloads itself and
	creates and cleans up its devs on repeated kldload/kldunloads.
	Notice: Because the dev_depends() kernel call for internally linking
	multiple devs to a master dev does not appear to exist prior to 5.0
 	(was not present in 4.6 or 4.7 releases), the dev create/cleanup
	code may not link on older versions, not without commenting out
	parts of that section of the driver.  See matcd_attach.
	09-Feb-2003  Frank Durda IV	uhclem at freebsd.org

<31>	Open() now does everything it is supposed to, and completes
	successfully.  It isn't clear if disable_intr/enable_intr is
	definitely going to prevent the drive commands from being issued
	within 10msec, but this seems to be the calls to now use in place
	of spl(7)/splx().
	13-Feb-2003  Frank Durda IV	uhclem at freebsd.org

<32>	Strategy code changed to deal with the newer mechanisms for handling
	request queues that exists in 5.0.  The driver will now mount and
	read an ISO filesystem.
	17-Feb-2003  Frank Durda IV	uhclem at freebsd.org

<33>	A sweep through the code to eliminate compiler warnings.  The code now
	produces just one warning in the timeout() call, and so far this one
	evades elimination.  This edit also started to remove some now-dead
	code, unneccessary includes and debug code inserted during the
	previous two edits while trying to get basic functions working again.
	22-Feb-2003  Frank Durda IV	uhclem at freebsd.org

<34>	Removal of the DIAGPORT code.  Although very useful during original
	development and for troubleshooting some problems found by others, it
	is no longer practical to keep, partly due to the way port allocation
	is now handled in FreeBSD, but mostly because of how few motherboards
	still have slots that will accomodate these diagnostic cards, most
	of which were ISA.  This presence of this code, even though ifdef-ed
	out in all earlier finished FreeBSD releases, still drove some of
	the other developers nuts for unclear reasons, but they should be
	happy now... :-)
	02-Mar-2003  Frank Durda IV	uhclem at freebsd.org

<35>	Work to make the FULLDRIVER conditional build still compile.  This
	inserts the audio and special ioctls not needed to be able to perform
	simple single-session filesystem mounts, and was used to keep the
	boot floppy kernel size down.  This work was mainly just re-working
	the way our internal data was passed around between functions (See
	edit 29), and tracking down declarations that have moved from one
	include file to another.  Since all the code for talking to the
	actual drives worked previously, only the interface back to the rest
	of the kernel and calling apps needs any modernization.
	09-Mar-2003  Frank Durda IV	uhclem at freebsd.org

<36>	Once work was complete on getting the FULLDRIVER audio functions
	compiling again, suddenly we could not mount filesystems anymore.
	This was eventually traced to a previously-not-performed-ioctl-call,
	now made by some other part of the kernel immediately after an open()
	call succeeds.  This ioctl reads the TOC for all tracks on the disc.
	Who is making this ioctl call (and what the answers are good for)
	is not immediately apparent.

	However, when FULLDRIVER wasn't present, this ioctl code was not
	available and the driver returned an error to the ioctl, but the mount
	succeeded anyway and the filesystem could be used with no issues.
	When the ioctl code was there (as a result of FULLDRIVER build), it
	was returning the information requested in MSF format, and this was
	confusing the caller.

	When matcd was originally written, returning MSF was all that the
	original CD audio players ever wanted, but this new mount-time
	query wants the answers in LBA format.  So the READTOC call has been
	updated to return either MSF or LBA formats.  Having implemented
	that, filesystems can be mounted again.
	16-Mar-2003  Frank Durda IV	uhclem at freebsd.org

<37>	Related to Edit 36, when the various ioctls were updated to return
	the offset information that was requested in either LBA or MSF
	format, that broke the cdcontrol utility, which will request
	information (via the "stat" command) in LBA or MSF based on the last
	setting of the "set" command, but cdcontrol displays bogus results if
	LBA results are returned when cdcontrol asked for them.  An
	examination of the behavior of the IDE CD-ROM driver shows that for
	SUBQ data, the IDE driver ignores the format type in the request and
	always returns MSF format, and that seems to be what cdcontrol really
	wants, never mind what it asked for.  Subsequently, a conditional
	(SUBQRETREQUESTED) has been added which will direct the driver to
	always return MSF in response to SUBQ queries (default), or to return
	what the caller actually asked for.  This probably should not be
	activated until all other CD-ROM drivers and cdcontrol are fixed
	on this point.
	22-Mar-2003  Frank Durda IV	uhclem at freebsd.org

<38>	A revisit of the compiler warning on timeout() that just refuses to go
	away.  A review of other drivers using timeout() suggests that the
	way timeout() is used within the kernel has been changed from
	traditional calling and passed parameters.  This call has no man page
	(apparently not even a reference in an include file exists) which
	makes confirmation difficult.  Further, as the timeout is not used
	predictably by the driver (sometimes needed, sometimes not), testing a
	change in this area will take some stress/duration testing.
	26-Mar-2003  Frank Durda IV	uhclem at freebsd.org

<39>	Two hardware failure conditions were identified when a eight-drive,
	two controller configuration was tested.  Some types of cable
	failure are now detected and reported by the driver.  Other failure
	modes are documented here for reference:
	1.	If the firmware version string displayed during the driver
		probing process (normally of the form) "[CR-NNNN.NN]" is
		displayed as "[]", or "[          ]" indicates that two or
		more drives are jumpered with the same drive select.  This
		condition may be accompanied by other strange random messages.

	2.	A firmware Version string "[^A^A^A^A^A^A^A^A^A^A]" indicates
		that the cable used on the Matsushita drive appears to be
		an IDE cable which has a pin pulled out of it (when this is
		done, it is usually in the key position).  A fully-pinned
		cable must be used.  A cable with shorts to ground or opens
		on numerous lines can also cause this symptom.

	3.	If the driver refuses to go into Idle state, this indicates
		a bad cable OR a power-supply under-volt condition.  The
		drives appear to stay in a Data or Status phase indefinitely
		if the supply voltages are below +5 and +12 VDC.  The drive
		BUSY light may also remain lit continuously or the eject
		button may be ignored.

	The rest of this edit was more clean-up and removal of some debug
	messages.
	02-Apr-2003  Frank Durda IV	uhclem at freebsd.org

<40>	Replaced Copyright text section in all files with the new revisionist
	(and vague) one currently required.  Everybody should be happy.  :-)
	03-Apr-2003  Frank Durda IV	uhclem at freebsd.org

<41>	Rebuilt on a stock 5.0-RELEASE platform to look for any issues
	that crept in since the late 2002 snapshot I had been using and
	to verify functionality on a dual-processor system.

	Changed the ioctl for eject so that it would work when there was
	no media in the drive.  The way it was seemed obviously wrong, but no
	one had said anything about this behavior in all the years it has
	worked that way.

	Also discovered that some of the ioctls failed to lock the device
	prior to issuing commands, and this could cause other operations
	to that same drive to not start and hang forever.  Example,
	"strings /dev" on one process while another issues an eject.  Yes,
	the "strings" will end up getting an error, but that is what should
	happen, not having "strings" simply stop and never come back.
	18-Apr-2003  Frank Durda IV	uhclem at freebsd.org

<42>	Changes to make matcd.c compile again on a 6-May-2003 pre-Beta 5.1
	cvsup, as the deck chairs continue to shift.
	o	Dropped tests for DIOCWLABEL on reads, which is now undefined.
	o	bioqdisksort() in the kernel depths changed its name from 5.0.*
	o	bounds_check_with_label() kernel call disappeared since 5.0
		with no apparent replacement.*
	o	The way the cdevsw structure is done has changed for the nth
		time.  It differs from what existed in 5.0.
	o	Dropped ioctl support for DIOCWDINFO and DIOCSDINFO.
		Although both ioctls are still defined in system include
		files, the setdisklabel() that I need to call to perform
		the ioctls has disappeared since 5.0.
	* Items marked are in the ifdef NOTEDIT42 rather than deleting
	  code that refers to kernel calls that may mysteriously return
	  in some future check-in.
	10-May-2003  Frank Durda IV	uhclem at freebsd.org

End of Edit History
---------------------------------------------------------------------------*/

/*Please match this ANSI version format:
		 		Version_d[ c](d)_dd-mmm-yyyy	*/
static char	MATCDVERSION[]="Version  3(42) 10-May-2003";

/* $FreeBSD$
*/


static char	MATCDCOPYRIGHT[] = "Matsushita CD-ROM driver, Copr. 1994,1995,2002,2003 Frank Durda IV";

/*---------------------------------------------------------------------------
	Fixed Defines - Change these values only if you are positive that
	you know what you are doing.  Values meant to be changed are in the
	file "dev/matcd/options.h".
---------------------------------------------------------------------------*/

#define	RAW_DEVICE	46		/*Major Dev number for raw device*/
#define RAW_PART        2

#define	TICKRES		10		/*Our coarse timer resolution*/
#define ISABUSKHZ	8330		/*Number of IN/OUT ops on ISA/sec*/
#define MAXTRKS		101		/*Maximum possible tracks*/

#define DRIVESPERC	4		/*Max drive selects per controller*/
#define TOTALDRIVES	NUMCTRLRS*DRIVESPERC	/*Max possible drives*/

#define MATCDBLK	2048		/*Standard block size*/
#define MATCDRBLK	2352		/*Raw and/or DA block size*/
#define	MATCD_RETRYS	5		/*Number of retries for read ops*/
#define	MATCD_READ_1	0x80		/*Read state machine defines*/
#define	MATCD_READ_2	0x90		/*Read state machine defines*/

/*	Bit equates for matcd_data.flags*/

#define	MATCDINIT	0x0001		/*Probe ran on host adapter*/
#define	MATCDLABEL	0x0004		/*Valid TOC exists*/
#define	MATCDLOCK	0x0008		/*<15>Drive door is locked*/
#define	MATCDWARN	0x0020		/*Have reported an open disc change*/

/*	Bit equates for matcd_data.partflags*/

#define	MATCDOPEN	0x0001
#define	MATCDREADRAW	0x0002

/*	Flags in the if_state array */

#define	BUSBUSY	0x01			/*<18>Bus is already busy*/

/*	Error classes returned by chk_error()*/

#define ERR_RETRY	1		/*A retry might recover this*/
#define ERR_INIT	2		/*A retry certainly will get this*/
#define ERR_FATAL	3		/*This cannot be recovered from*/


/*---------------------------------------------------------------------------
	These macros take apart the minor number and yield the
	partition, drive on controller, and controller.
	This must match the settings in /dev/MAKEDEV.
---------------------------------------------------------------------------*/

#define		matcd_partition(dev)	((minor(dev)) & 0x07)
#define		matcd_ldrive(dev)	(((minor(dev)) & 0x78) >> 3)
#define		matcd_cdrive(dev)	(((minor(dev)) & 0x18) >> 3)
#define		matcd_controller(dev)	(((minor(dev)) & 0x60) >> 5)
#ifdef LOCKDRIVE
#define		matcd_lockable(dev)	(((minor(dev)) & 0x80) >> 5)
#else /*LOCKDRIVE*/
#define		matcd_lockable(dev)	(0);	/*Behave like non-locking*/
#endif /*LOCKDRIVE*/


/*---------------------------------------------------------------------------
	Include file declarations
---------------------------------------------------------------------------*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/cdio.h>
#include <sys/disk.h>
#include <sys/bus.h>

#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>
#include <sys/disklabel.h>

#include <isa/isavar.h>

#include <dev/matcd/options.h>
#include <dev/matcd/matcd_data.h>
#include <dev/matcd/matcddrv.h>
#include <dev/matcd/creativeif.h>


/*---------------------------------------------------------------------------
	Global structures and variables
---------------------------------------------------------------------------*/

static 	struct	bio_queue_head request_head[NUMCTRLRS];	/*<18>One queue for
							each interface*/
static	int	nextcontroller=0;	/*Number of interface units probed*/
static	int	drivepresent=0; 	/*Drive has >0 drives found*/
static	int	iftype;			/*Probe/Attach I/F type relay*/

struct matcd_volinfo {
	unsigned char	type;		/*00 CD-DA or CD-ROM
					  10 CD-I
					  20 XA */
	unsigned char	trk_low;	/*Normally 1*/
	unsigned char	trk_high;	/*Highest track number*/
	unsigned char	vol_msf[3];	/*Size of disc in min/sec/frame*/
};

struct	matcd_mbx {
	short		controller;
	short		ldrive;
	short		partition;
	short		port;
	short		iftype;		/*<20>Host interface type*/
	short		retry;
	short		nblk;
	int		sz;
	u_long		skip;
	struct		bio	*bp;
	int		p_offset;
	short		count;
};

static	struct matcd_data {
	device_t	dev;
	dev_t		matcd_dev_t;
	short		config;
	short		drivemode;	/*Last state drive was set to*/
	short		flags;
	short		status;		/*Last audio-related function*/
	int		blksize;
	u_long		disksize;
	short		iobase;
	short		iftype;		/*Host interface type*/
	struct		disklabel dlabel;
	unsigned int	partflags[MAXPARTITIONS];
	unsigned int	openflags;
	struct		matcd_volinfo volinfo;
	struct		matcd_mbx mbx;
	u_char		patch[2];	/*Last known audio routing*/
	u_char		volume[2];	/*Last known volume setting*/
	struct bio_queue_head	head;
} matcd_data[TOTALDRIVES];

struct matcd_read2 {
	unsigned char	start_msf[3];
	unsigned char	end_msf[3];
};

static	unsigned char	if_state[4]={0,0,0,0};	/*State of host I/F and bus*/


/*---------------------------------------------------------------------------
	Entry points and other connections to/from kernel - see matcd_isa.c
---------------------------------------------------------------------------*/

int		matcd_probe(struct matcd_softc *sc);
int		matcd_attach(struct matcd_softc *sc);
static int	matcdopen(dev_t dev, int flags, int fmt, struct thread *ptx);
static int	matcdclose(dev_t dev, int flags, int fmt, struct thread *ptx);
static void	matcdstrategy(struct bio *bp);
static d_ioctl_t	matcdioctl;
static timeout_t	matcd_timeout;


/*---------------------------------------------------------------------------
	Internal function declarations
---------------------------------------------------------------------------*/

static	int	matcdsize(dev_t dev);
static	void	matcd_start(struct bio_queue_head *dp);
static	void	zero_cmd(char *);
static	void	matcd_pread(int port, int count, unsigned char * data);
static	int	matcd_fastcmd(int port,int ldrive,int cdrive,
			      unsigned char * cp);
static	void	matcd_slowcmd(int port,int ldrive,int cdrive,
			      unsigned char * cp);
static	void	matcd_blockread(int state);
static	void	selectdrive(int port,int drive);
static	void	doreset(int port,int cdrive);
static	int	doprobe(int port,int cdrive, struct matcd_softc *sc);
static	void	lockbus(int controller, int ldrive);
static	void	unlockbus(int controller, int ldrive);
static	int	matcd_volinfo(int ldrive);
static	void	draincmd(int port,int cdrive,int ldrive);
static	int 	get_error(int port, int ldrive, int cdrive);
static	int	chk_error(int errnum);
static	int	msf_to_blk(unsigned char * cd);
static	void	blk_to_msf(int blk, unsigned char *msf);
static	int	matcd_toc_header(int ldrive, int cdrive, int controller,
		                 struct ioc_toc_header * toc);
static	int	matcd_toc_entries(int ldrive, int cdrive,
				  int controller,
				  struct ioc_read_toc_entry *ioc_entry);
static	int	matcd_read_subq(int ldrive, int cdrive, int controller,
			        struct ioc_read_subchannel * sqp);
static	int	matcd_igot(struct ioc_capability * sqp);
static	int	waitforit(int timelimit, int state, int port,
			  char * where);
static	int	get_stat(int port, int ldrive);
static	int	media_chk(struct matcd_data *cd,int errnum,
			  int ldrive,int test);
static	int	matcd_eject(int ldrive, int cdrive, int controller);
static	int	matcd_doorclose(int ldrive, int cdrive, int controller);
static	int	matcd_dlock(int ldrive, int cdrive,
			    int controller, int action);
static	int	docmd(char * cmd, int ldrive, int cdrive,
		      int controller, int port);
static	int	matcd_setmode(int ldrive, int mode);

#ifdef	FULLDRIVER
static	int	matcd_playtracks(int ldrive, int cdrive, int controller,
				 struct ioc_play_track *pt);
static	int	matcd_playmsf(int ldrive, int cdrive, int controller,
			      struct ioc_play_msf *pt);
static	int	matcd_playblk(int ldrive, int cdrive, int controller,
			      struct ioc_play_blocks *pb);
static	int	matcd_pause(int ldrive, int cdrive, int controller,
			    int action);
static	int	matcd_stop(int ldrive, int cdrive, int controller);
static	int	matcd_level(int ldrive, int cdrive, int controller,
		            struct ioc_vol * volume, int action);
static	int	matcd_patch(int ldrive, int cdrive, int controller,
		            struct ioc_patch * routing);
static	int	matcd_route(int ldrive, int cdrive, int controller,
		            int command);
static	int	matcd_pitch(int ldrive, int cdrive, int controller,
		            struct ioc_pitch * speed);
#endif /*FULLDRIVER*/


/*---------------------------------------------------------------------------
	This Character Device Switch Table gives the rest of the kernel
	a list of what functions do and don't exist, and what to call to
	get them.  Read-only block devices have pretty much the same items.

<42>	Apparently now you don't declare things you can't do.  You used
	to say "nowhatever it was".
---------------------------------------------------------------------------*/

static struct cdevsw matcd_cdevsw = {
	.d_open =	matcdopen,	/* open */
	.d_close =	matcdclose,	/* close */
	.d_read =	physread,	/* read */
					/* write used to be here */

	.d_ioctl =	matcdioctl,	/* ioctl */

	.d_strategy =	matcdstrategy,	/* strategy */
	.d_name =	"matcd",	/* name */
	.d_maj =	RAW_DEVICE,	/* maj */
	.d_flags =	D_DISK,		/* flags */
};


/*---------------------------------------------------------------------------
	matcd_attach - Locates drives on the adapters that were located.
		If we got here, we located an interface and at least one
		drive.  Now we figure out how many drives are under that
		interface.  The Panasonic interface is too simple to call
		it a controller, but in the existing PDP model, that is
		what it would be.
---------------------------------------------------------------------------*/

int matcd_attach(struct matcd_softc *sc)
{
	int	i;
	unsigned int	z,cdrive;
	unsigned char	cmd[MAXCMDSIZ];
	unsigned char	data[12];
	struct	matcd_data *cd;
	int port = sc->port_bsh;	/*Take port ID selected in probe()*/
	dev_t	d;
	int unit=0;

	printf("matcdc%d: Host interface type %d port %x\n",
		nextcontroller,iftype,port);
	for (cdrive=0; cdrive<4; cdrive++) {	/*We're hunting drives...*/
		zero_cmd(cmd);
		cmd[0]=NOP;		/*A reasonably harmless command.
					  This command will fail after
					  power-up or after reset. It's OK*/
		unit=cdrive+(DRIVESPERC*nextcontroller);
		if (matcd_fastcmd(port,unit,cdrive,cmd)==0) {	/*Issue cmd*/
			z=get_stat(port,cdrive);/*Read status byte*/
			if ((z & MATCD_ST_ERROR)) {	/*If there was an error,
						  we must ask for error info
						  or subsequent cmds fail*/
				zero_cmd(cmd);
				cmd[0]=READERROR;	/*Inquire*/
				matcd_fastcmd(port,unit,cdrive,cmd);
				matcd_pread(port,8,data);/*Read data returned*/
				z=get_stat(port,unit);/*Read status byte*/
#ifdef DEBUGPROBE
				printf("matcd%d: Status byte %x ",unit,z);
#endif /*DEBUGPROBE*/
			}
			zero_cmd(cmd);
			cmd[0]=READID;	/*Get drive ID*/
			matcd_fastcmd(port,unit,cdrive,cmd);
			matcd_pread(port,10,data);/*Read Drive Parm*/
			get_stat(port,unit);	/*Read and toss status byte*/
			data[10]=0;	/*Build ASCIZ string*/
			printf("matcd%d: [%s]  ",unit,data);
			cd=&matcd_data[unit];
			cd->flags |= MATCDINIT;
			cd->iobase=port;
			cd->iftype=iftype;
			cd->openflags=0;
			cd->volume[0]=cd->volume[1]=DEFVOL;
					/*Match volume drive resets to*/
			cd->patch[0]=0x01;	/*Channel 0 to Left*/
			cd->patch[1]=0x02;	/*Channel 1 to Right*/
			cd->status=CD_AS_NO_STATUS;
			for (i=0; i<MAXPARTITIONS; i++) {
				cd->partflags[i]=0;
			}

/*	This section creates the devices files in the /dev directory for each
	drive that is found.   There must be a drive 0 on each controller in
	order to get any device files at all.
*/

			if (unit%4==0) {	/*First drive of controller*/
				sc->matcd_dev_t = make_dev(&matcd_cdevsw,
							   8 * unit,
							   UID_ROOT,
							   GID_OPERATOR,
						   	   0640,
							   "matcd%da", unit);
			} else {		/*Drives 1 thru N*/
				d = make_dev(&matcd_cdevsw, 8 * unit,
					     UID_ROOT, GID_OPERATOR,
				   	     0640, "matcd%da", unit);
				dev_depends(sc->matcd_dev_t,d);
			}
			d=make_dev(&matcd_cdevsw, (8 * unit)+2, UID_ROOT,
				   GID_OPERATOR, 0640, "matcd%dc", unit);
			dev_depends(sc->matcd_dev_t,d);
			d=make_dev(&matcd_cdevsw, (8 * unit)+128, UID_ROOT,
				   GID_OPERATOR, 0640, "matcd%dla", unit);
			dev_depends(sc->matcd_dev_t,d);
			d=make_dev(&matcd_cdevsw, (8 * unit)+130, UID_ROOT,
				   GID_OPERATOR, 0640, "matcd%dlc", unit);
			dev_depends(sc->matcd_dev_t,d);
		}
	}
	bioq_init(&request_head[nextcontroller]);	/*Init request queue*/
	nextcontroller++;		/*Bump ctlr assign to next number*/
	printf("\n");			/*End line of drive reports*/
	return(0);			/*Must return "NO ERROR"*/
}


/*---------------------------------------------------------------------------
	matcdopen - Open the device

	This routine actually gets called every time anybody opens
	any partition on a drive.  But the first call is the one that
	does all the work.

<15>	If LOCKDRIVE is enabled, additional minor number devices allow
<15>	the drive to be locked while being accessed.
---------------------------------------------------------------------------*/
static int	matcdopen(dev_t dev, int flags, int fmt,
		  struct thread *ptx)
{
	int cdrive,ldrive,partition,controller,lock;
	struct matcd_data *cd;
	int	i,z,port;
	unsigned char	cmd[MAXCMDSIZ];

	ldrive=matcd_ldrive(dev);
	cdrive=matcd_cdrive(dev);
	partition=matcd_partition(dev);
	controller=matcd_controller(dev);
	lock=matcd_lockable(dev);
	cd= &matcd_data[ldrive];
	port=cd->iobase;	/*and port#*/

	if (ldrive >= TOTALDRIVES) return(ENXIO);


#ifdef DEBUGOPEN
	printf("matcd%d: Open: dev %x partition %x controller %x flags %x cdrive %x\n",
	       ldrive,(int)dev,partition,controller,cd->flags,
	       matcd_cdrive(dev));
#endif /*DEBUGOPEN*/

	if (!(cd->flags & MATCDINIT)) {	/*Did probe find this drive*/
		return(ENXIO);
	}

	if (!(cd->flags & MATCDLABEL) &&
	    cd->openflags) {		/*Has drive completely closed?*/
		return(ENXIO);		/*No, all partitions must close*/
	}


/*	Now, test to see if the media is ready
*/

	lockbus(controller,ldrive);
	zero_cmd(cmd);
	cmd[0]=NOP;			/*Test drive*/
	matcd_slowcmd(port,ldrive,cdrive,cmd);
	i=waitforit(10*TICKRES,DTEN,port,"matopen");
	z=get_stat(port,ldrive);	/*Read status byte*/
#ifdef DEBUGOPEN
	printf("matcd%d Result of NOP is %x %x\n",ldrive,i,z);
#endif /*DEBUGOPEN*/
	if ((z & MATCD_ST_DSKIN)==0) {	/*Is there a disc in the drive?*/
#ifdef DEBUGOPEN
		printf("matcd%d: No Disc in open\n",ldrive);
#endif /*DEBUGOPEN*/
		unlockbus(controller, ldrive);	/*Release bus lock*/
		cd->flags &= ~MATCDLABEL;	/*Mark label as invalid*/
		if (major(dev)==RAW_DEVICE) {	/*Is the char device?*/
			return(0);		/*Allow Semi open*/
		}
		else {
			return(ENXIO);		/*Normally blow off*/
		}
	}
	if (z & MATCD_ST_ERROR) {		/*Was there an error*/
		i=get_error(port,ldrive,cdrive);/*Find out what it was*/
#ifdef DEBUGOPEN
		printf("matcd%d NOP Error was %x\n",ldrive,i);
#endif /*DEBUGOPEN*/
		if (cd->openflags) {		/*Any parts open?*/
			if (media_chk(cd,i,ldrive,0)) {	/*Was it a disc chg?*/
#ifdef DEBUGOPEN
				printf("matcd%d: Disc change detected i %x z %x\n",
				       ldrive,i,z);
#endif /*DEBUGOPEN*/
				unlockbus(controller, ldrive);	/*Free bus lck*/
				return(ENOTTY);
			}

		} else {
			media_chk(cd,i,ldrive,1);/*Was it a disc chg?*/
						 /*Clear volume info*/
		}
	}
	unlockbus(controller, ldrive);	/*Release bus lock*/

/*	Here we fill in the disklabel structure although most is hardcoded.
*/

	if ((cd->flags & MATCDLABEL)==0) {
		bzero(&cd->dlabel,sizeof(struct disklabel));


/*	Now we query the drive for the actual size of the media.
	This is where we find out if there is any media, or if the
	media isn't a Mode 1 or Mode 2/XA disc.
	See version information about Mode 2/XA support.
*/
		lockbus(controller,ldrive);
		i=matcdsize(dev);
		unlockbus(controller, ldrive);	/*Release bus lock*/
#ifdef DEBUGOPEN
		printf("matcd%d: Bus unlocked in open\n",ldrive);
#endif /*DEBUGOPEN*/
		if (i < 0) {
			printf("matcd%d: Could not read the disc size\n",ldrive);
			return(ENXIO);
		}			/*matcdsize filled in rest of dlabel*/

/*	Based on the results, fill in the variable entries in the disklabel
*/
		cd->dlabel.d_secsize=cd->blksize;
		cd->dlabel.d_ncylinders=(cd->disksize/100)+1;
		cd->dlabel.d_secperunit=cd->disksize;
		cd->dlabel.d_partitions[0].p_size=cd->disksize;
		cd->dlabel.d_checksum=dkcksum(&cd->dlabel);


/*	Now fill in the hardcoded section
*/
					     /*123456789012345678*/
		strncpy(cd->dlabel.d_typename,"Matsushita CDR ",16);
		strncpy(cd->dlabel.d_packname,"(c) 2003, fdiv ",16);
		cd->dlabel.d_magic=DISKMAGIC;
		cd->dlabel.d_magic2=DISKMAGIC;
		cd->dlabel.d_nsectors=100;
		cd->dlabel.d_secpercyl=100;
		cd->dlabel.d_ntracks=1;
		cd->dlabel.d_interleave=1;
		cd->dlabel.d_rpm=300;
		cd->dlabel.d_npartitions=1;	/*See note below*/
		cd->dlabel.d_partitions[0].p_offset=0;
		cd->dlabel.d_partitions[0].p_fstype=9;
		cd->dlabel.d_flags=D_REMOVABLE;

/*	I originally considered allowing the partition to match tracks or
	sessions on the media, but since you are allowed up to 99
	tracks in the RedBook world, this would not fit in with the older
	BSD fixed partition count scheme.
*/

		cd->flags |= MATCDLABEL;	/*Mark drive as having TOC*/
	}

#ifdef DEBUGOPEN
	printf("matcd%d open2: partition=%d disksize=%d blksize=%x flags=%x\n",
	       ldrive,partition,(int)cd->disksize,cd->blksize,cd->flags);
#endif /*DEBUGOPEN*/

#ifdef LOCKDRIVE
	if (cd->openflags==0 && lock) {
		zero_cmd(cmd);
		cmd[0]=LOCK;		/*Lock drive*/
		cmd[1]=1;
		docmd(cmd,ldrive,cdrive,controller,port);/*Issue cmd*/
		cd->flags |= MATCDLOCK;	/*Drive is now locked*/
	}
#endif /*LOCKDRIVE*/
	cd->openflags |= (1<<(partition+lock));/*Mark partition open*/

	if (partition==RAW_PART ||
	    (partition < cd->dlabel.d_npartitions &&
	     cd->dlabel.d_partitions[partition].p_fstype != FS_UNUSED)) {
		cd->partflags[partition] |= MATCDOPEN;
		if (partition == RAW_PART) {
			cd->partflags[partition] |= MATCDREADRAW;
		}
#ifdef DEBUGOPEN
		printf("matcd%d: Open is complete - openflags %x\n",
		       ldrive,cd->openflags);
#endif /*DEBUGOPEN*/
		return(0);
	}
#ifdef DEBUGOPEN
	printf("matcd%d: Open FAILED\n",ldrive);
#endif /*DEBUGOPEN*/
	return(ENXIO);
}


/*---------------------------------------------------------------------------
	matcdclose - Close the device

	Close may not do much other than clear some driver settings.
	Note that audio playback will continue.

	If you define LOCKDRIVE, and the drive has been opened using
	one of the locking minor numbers, code in close will unlock
	the drive.  See Edit 15 in Edit History.
---------------------------------------------------------------------------*/

static int matcdclose(dev_t dev, int flags, int fmt,struct thread *ptx)
{
	int	ldrive,cdrive,port,partition,controller,lock;
	struct matcd_data *cd;
#ifdef LOCKDRIVE
	unsigned char cmd[MAXCMDSIZ];
#endif /*LOCKDRIVE*/

	ldrive=matcd_ldrive(dev);
	cdrive=matcd_cdrive(dev);
	lock=matcd_lockable(dev);
	cd=matcd_data+ldrive;
	port=cd->iobase;			/*and port#*/

	if (ldrive >= TOTALDRIVES)
		return(ENXIO);

	partition = matcd_partition(dev);
	controller=matcd_controller(dev);
#ifdef DEBUGOPEN
	printf("matcd%d: Close partition=%d flags %x openflags %x partflags %x\n",
	       ldrive,partition,cd->flags,cd->openflags,
	       cd->partflags[partition]);
#endif /*DEBUGOPEN*/

	if (!(cd->flags & MATCDINIT))
		return(ENXIO);

	cd->partflags[partition] &= ~(MATCDOPEN|MATCDREADRAW);
	cd->openflags &= ~(1<<(partition+lock));
	if (cd->openflags==0) {			/*Really last close?*/
#ifdef LOCKDRIVE
		if (cd->flags & MATCDLOCK) {	/*Was drive locked?*/
			zero_cmd(cmd);		/*Yes, so unlock it*/
			cmd[0]=LOCK;		/*Unlock drive*/
			docmd(cmd,ldrive,cdrive,controller,port);
		}
#endif /*LOCKDRIVE*/
		cd->flags &= ~(MATCDWARN|MATCDLOCK);	/*<15>*/
						/*Clear warning flag*/
	}
	return(0);
}

/*---------------------------------------------------------------------------
	matcd_probe - Search for host interface/adapters

	The probe routine hunts for the first drive on the interface since
	there is no way to locate just the adapter.   It also resets the
	entire drive chain while it is there.  matcd_attach() takes care of
	the rest of the initialization.

	The probe routine can be compiled two ways.  In AUTOHUNT mode,
	the kernel config file can say "port?" and we will check all ports
	listed in the port_hint array (see above).

	Without AUTOHUNT set, the config file must list a specific port
	address to check.

	Note that specifying the explicit addresses makes boot-up a lot
	faster.

	The probe will locate Panasonic/Creative interface on the following
	Creative adapter boards:
		#1330A	Sound Blaster PRO
		#1730	Sound Blaster 16
		#1740	Sound Blaster 16 (cost reduced)
		#2230	Sound Blaster 16 (cost reduced)
		#2770	Sound Blaster 16 Value (cost reduced)
		#1810	omniCD upgrade kit adapter card (stand-alone CD)
		#3100	PhoneBlaster SB16 + Sierra 14.4K modem combo
	Creative releases a newer and cheaper-to-make Sound Blaster
	board every few months, so by the original release date of this
	software, there are probably 8 different board models called
	Sound Blaster 16.  These include "Vibra", "Value", etc.

	Please report additional part numbers and board descriptions
	and new port numbers that work to the author.

---------------------------------------------------------------------------*/

int matcd_probe(struct matcd_softc *sc)
{
	int	i,cdrive;
	unsigned char	y;
	int port = sc->port_bsh;	/*Obtain port number hint*/

	cdrive=nextcontroller;		/*Controller defined by pass for now*/
	if (nextcontroller==NUMCTRLRS) {
		device_printf(sc->dev,"matcdc%d: - Too many interfaces specified- adjust NMATCD\n",
		       nextcontroller);
		return(1);
	}
	if (nextcontroller==0) {	/*Very first time to be called*/
		for (i=0; i<TOTALDRIVES; i++) {
			matcd_data[i].drivemode=MODE_UNKNOWN;
			matcd_data[i].flags=0;
		}
	}

	i=nextcontroller*DRIVESPERC;	/*Precompute controller offset*/
	for (y=0; y<DRIVESPERC; y++) {
		matcd_data[i+y].flags=0;
	}

#ifdef DEBUGPROBE
	device_printf(sc->dev,"matcdc%d: In probe i %d y %d port %x\n",
	       nextcontroller,i,y,port);
#endif /*DEBUGPROBE*/
#ifdef AUTOHUNT
#ifdef DEBUGPROBE
	device_printf(sc->dev,"matcd%d: size of port_hints %d\n",
	       nextcontroller,sizeof(port_hints));
#endif /*DEBUGPROBE*/
	if (port==-1) {
		for(i=0;i<(sizeof(port_hints)/sizeof(short));i++) {
			port=port_hints[i];
#ifdef DEBUGPROBE
	device_printf(sc->dev,"matcdc%d: Port hint %x\n",nextcontroller,port);
#endif /*DEBUGPROBE*/
			if (port==-1) {
#ifdef DOUPDATE
				dev->id_iobase=-1;	/*Put port ? back*/
#endif
				return(1);/*Nothing left to try*/
			}
			if (port!=0) {	/*Untested port found*/
#ifdef DOUPDATE
				dev->id_iobase=port;
#endif
				port_hints[i]=0;/*Don't use that port again*/
				if (doprobe(port,cdrive,sc)==0) return(NUMPORTS);
			}
		}
#ifdef DOUPDATE
		dev->id_iobase=-1;	/*Put port ? back as it was*/
#endif
		return(1);		/*Interface not found*/

	} else {			/*Config specified a port*/
		i=0;			/*so eliminate it from the hint list*/
		for(i=0;;i++) {		/*or we might try to assign it again*/
			if (port_hints[i]== -1) break;	/*End of list*/
			if (port_hints[i]==port) {
				port_hints[i]=0;	/*Clear duplicate*/
				break;
			}
		}
		if (doprobe(port,cdrive,sc)==0) return(0);
		else return(1);
	}
#else /*AUTOHUNT*/
	if (port==-1) {
		device_printf(sc->dev,"matcdc%d: AUTOHUNT disabled but port? specified in config\n",
		       nextcontroller);
		return(1);
	}
	if (doprobe(port,cdrive,sc)==0) return(0);
	else return(1);
#endif /*AUTOHUNT*/
}

/*---------------------------------------------------------------------------
	doprobe - Common probe code that actually checks the ports we
		have decided to test.

<20>	Edit 20 changes adds code to determine if the host interface
	is one that behaves like the Creative SoundBlaster cards,
	or whether the host interface like those used by some boards
	made by Media Vision and a version known as Lasermate.
---------------------------------------------------------------------------*/

int doprobe(int port,int cdrive,struct matcd_softc *sc)
{
	unsigned char cmd[MAXCMDSIZ];
	int i;

#ifdef RESETONBOOT
	doreset(port,cdrive);		/*Reset what might be our device*/
#endif /*RESETONBOOT*/
	outb(port+PHASE,0);		/*Guarantee status phase*/
	zero_cmd(cmd);
	cmd[0]=NOP;			/*A reasonably harmless command.
				  	  This command will fail after
				  	  power-up or after reset. That's OK*/
#ifdef RESETONBOOT
	if (((inb(port+STATUS) & (DTEN|STEN)) != (DTEN|STEN)) ||
	    (inb(port+DATA) != 0xff))
		return(-1);		/*Something detected but it isn't
					      the device we wanted*/
#endif /*RESETONBOOT*/
	if (matcd_fastcmd(port,0,0,cmd)==0) {/*Issue command*/
		outb(port+PHASE,1);	/*Switch to Creative Data phase*/
		i=inb(port+CMD);	/*Read a byte in data phase*/
		outb(port+PHASE,0);	/*Switch to Creative Status phase*/
		if ((inb(port+STATUS) & (DTEN|STEN))
		    == (DTEN|STEN)) {	/*Drive went idle*/
			iftype=1;	/*It is not a Creative interface.*/
		} else {		/*Status byte still available*/
			iftype=0;	/*It is a Creative interface*/
			inb(port+CMD);	/*Read status byte*/
		}
#ifdef DEBUGPROBE
		device_printf(sc->dev,"matcdc%d: Probe found something\n",nextcontroller);
#endif /*DEBUGPROBE*/
/*------If you change the following, it makes remote support difficult-------*/
		if (drivepresent==0) {
			device_printf(sc->dev,
				      "Matsushita/Panasonic CD-ROM Driver by FDIV, %s\n",
				      MATCDVERSION);
			drivepresent++;
			if (drivepresent==0) /*make LINT happy*/
				device_printf(sc->dev,"%s\n",MATCDCOPYRIGHT);
		}
/*------If you change the preceding, it makes remote support difficult-------*/
		return(0);		/*Drive 0 detected*/
	}
#ifdef DEBUGPROBE
	device_printf(sc->dev,"matcdc%d: Probe DID NOT find something\n",nextcontroller);
#endif /*DEBUGPROBE*/
	return(1);
}


/*---------------------------------------------------------------------------
	doreset - Resets all the drives connected to a interface
---------------------------------------------------------------------------*/

void doreset(int port,int cdrive)
{
	register int	i,z;
	outb(port+RESET,0);		/*Reset what might be our device*/
					/*Although this ensures a known
					  state, it does close the drive
					  door (if open) and aborts any
					  audio playback in progress. */
	for (i=0;i<(125*ISABUSKHZ);i++){/*DELAY 500msec minimum. Worst
					  case is door open and none or
					  unreadable media */
		z=inb(port+CMD);	/*This makes the loop run at a
					  known speed.  This value is ok
					  for the 8.33MHz ISA bus*/
	}
	for (i=0;i<4;i++) {
		matcd_data[(cdrive*4)+i].drivemode=MODE_UNKNOWN;
	}
	return;
}


/*---------------------------------------------------------------------------
	matcd_fastcmd - Send a command to a drive

	This routine executed commands that return instantly (or reasonably
	quick), such as RESET, NOP, READ ERROR, etc.  The only difference
	between it and handling for slower commands, is the slower commands
	will invoke a timeout/sleep if they don't get an instant response.

	Fastcmd is mainly used in probe(), attach() and error related
	functions.  Every attempt should be made to NOT use this
	function for any command that might be executed when the system
	is up.
---------------------------------------------------------------------------*/

int matcd_fastcmd(int port,int ldrive,int cdrive,unsigned char * cp)
{
	unsigned int	i;
	unsigned char	z;

#ifdef DEBUGCMD
	unsigned char *cx;
#endif /*DEBUGCMD*/

	draincmd(port,cdrive,ldrive);	/*Make sure bus is really idle*/
#ifdef DEBUGCMD
	cx=cp;
	printf("matcd%d: Fast Send port %x sel %d command %x %x %x %x %x %x %x\n",
	       ldrive,port,cdrive,cx[0],cx[1],cx[2],cx[3],cx[4],cx[5],cx[6]);
#endif	/*DEBUGCMD*/
	selectdrive(port,cdrive);	/*Enable the desired target drive*/
	disable_intr();		/*----------------------------------------*/
	for (i=0; i<7; i++) {		/*The seven bytes of the command*/
		outb(port+CMD,*cp++);	/*must be sent within 10msec or*/
	}				/*the drive will ignore the cmd*/
	enable_intr();	/*------------------------------------------------*/

/*	Now we wait a maximum of 240msec for a response.  Only in a few rare
	cases does it take this long.  If it is longer, the command in
	question should probably be slept on rather than increasing this
	timing value.
*/

	for (i=0; i<(60*ISABUSKHZ); i++) {
		z = (inb(port+STATUS)) & (DTEN|STEN);
		if (z != (DTEN|STEN)) break;
	}

/*	We are now either in a data or status phase, OR we timed-out.*/

	if (z == (DTEN|STEN)) {
#ifdef DEBUGCMD
		printf("matcd%d: Command time-out\n",ldrive);
#endif /*DEBUGCMD*/
		return(-1);
	}
	if (z != DTEN) {
		return(1);
	}
	return(0);
}


/*---------------------------------------------------------------------------
	draincmd - Empty any pending data drive has to send

	Draincmd makes certain the bus is idle and throws away any
 	residual data from the drive if there is any.  Called as preface
	to most commands.  Added in Edit 5.

	This was added because switching drive modes causes the drive to
	emit buffers that were meant to be sent to the D-to-A to be sent
	to the host.  See setmode.
---------------------------------------------------------------------------*/
void draincmd(int port,int cdrive,int ldrive)
{
	int	i,z;

	i=inb(port+STATUS);
	if ((i & (DTEN|STEN)) == (DTEN|STEN)) return;

	printf("matcd%d: in draincmd: bus not idle %x %x - trying to fix\n",
	       ldrive,port+STATUS,inb(port+STATUS));
	if ((i & (DTEN|STEN)) == STEN) {
#ifdef DEBUGCMD
		printf("matcd%d: Data present READING - ",ldrive);
#endif /*DEBUGCMD*/
		i=0;
		outb(port+PHASE,1);	/*Enable data read*/
		while ((inb(port+STATUS) & (DTEN|STEN)) == STEN) {
			inb(port+DATA);		/*Ok for Creative*/
			inb(port+ALTDATA);	/*Ok for others*/
			i++;
		}
		outb(port+PHASE,0);
#ifdef DEBUGCMD
		printf("%d bytes read\n",i);
#endif /*DEBUGCMD*/
	}
#ifdef DEBUGCMD
	printf("matcd%d: Now read status: ",ldrive);
#endif /*DEBUGCMD*/
	i=get_stat(port,ldrive);	/*Read status byte*/
	z=inb(port+STATUS);		/*Read bus status*/
#ifdef DEBUGCMD
	printf("Data byte %x and status is now %x\n",i,z);
#endif /*DEBUGCMD*/
	if ((z & (DTEN|STEN)) != (DTEN|STEN)) {
		printf("matcd%d: Bus not idle %x - resetting\n",
		       cdrive,inb(port+STATUS));
		doreset(port,cdrive);
	}
	return;
}


/*---------------------------------------------------------------------------
	selectdrive - Swaps drive select bits

	On Creative SB/SB16/stand-alone adapters, possibly to make them
	hard to reverse engineer, the drive select signals are swapped.
---------------------------------------------------------------------------*/

void selectdrive(int port,int drive)
{
	switch(drive) {
	case 0:				/*0x00 -> 0x00*/
		outb(port+SELECT,CRDRIVE0);
		break;
	case 1:				/*0x01 -> 0x02*/
		outb(port+SELECT,CRDRIVE1);
		break;
	case 2:				/*0x02 -> 0x01*/
		outb(port+SELECT,CRDRIVE2);
		break;
	case 3:				/*0x03 -> 0x03*/
		outb(port+SELECT,CRDRIVE3);
		break;
	}
	return;
}


/*---------------------------------------------------------------------------
	get_stat - Reads status byte

	This routine should be totally unnecessary, performing the
	task with a single line of in-line code.  However in special
	cases, the drives return blocks of data that are not associated
	with the command in question.  This appears to be at least one
	firmware error and the rest of the driver makes an effort to avoid
	triggering the fault.  However, when this situation occurs, simply
	reading and throwing-away this bogus data is faster and less
	destructive than resetting all the drives on a given controller to get
	back in sync, plus not resetting leaves the other drives unaffected.
---------------------------------------------------------------------------*/

int get_stat(int port,int ldrive)
{
	int 	status,busstat;
	int	i;

	status=inb(port+DATA);		/*Read status byte, last step of cmd*/
	busstat=inb(port+STATUS);	/*Get bus status - should be 0xff*/
	while ((busstat & (DTEN|STEN)) != (DTEN|STEN)) {
		printf("matcd%d: get_stat: After reading status byte, bus didn't go idle %x %x %x - Check for bad cable\n",ldrive,status,busstat,port);
		if (( busstat & (DTEN|STEN)) == STEN) {
			i=0;
			outb(port+PHASE,1);	/*Enable data read*/
			while ((inb(port+STATUS) & (DTEN|STEN)) == STEN) {
				(void)inb(port+DATA);
				(void)inb(port+ALTDATA);
				i++;
			}
			outb(port+PHASE,0);
		}
		status=inb(port+DATA);		/*Read the status byte again*/
		busstat=inb(port+STATUS);
	}
	return(status);
}

/*---------------------------------------------------------------------------
	zero_cmd - Initialize command buffer
---------------------------------------------------------------------------*/

void zero_cmd(char * lcmd)
{
	int	i;

	for (i=0; i<MAXCMDSIZ; lcmd[i++]=0);
	return;
}


/*---------------------------------------------------------------------------
	matcd_slowcmd - Issue a command to the drive

	This routine is for commands that might take a long time, such
	as a read or seek.  The caller must determine if the command
	completes instantly or schedule a poll later on.
---------------------------------------------------------------------------*/

void matcd_slowcmd(int port,int ldrive,int cdrive,unsigned char * cp)
{
	unsigned int	i;
	int	size;
#ifdef DEBUGCMD
	unsigned char *cx;
#endif /*DEBUGCMD*/

	draincmd(port,cdrive,ldrive);	/*Make sure bus is really idle*/

#ifdef DEBUGCMD
	cx=cp;
	printf("matcd%d: Slow Send port %x sel %d command %x %x %x %x %x %x %x\n",
	       ldrive,port,cdrive,cx[0],cx[1],cx[2],cx[3],cx[4],cx[5],cx[6]);
#endif	/*DEBUGCMD*/
	selectdrive(port,cdrive);	/*Enable the desired target drive*/
	if (*cp==ABORT) size=1;
	else size=7;
	disable_intr();	/*------------------------------------------------*/
	for (i=0; i<size; i++) {	/*The seven bytes of the command*/
		outb(port+CMD,*cp++);	/*must be sent within 10msec or*/
	}				/*the drive will ignore the cmd*/
	enable_intr();	/*------------------------------------------------*/
	return;
}


/*---------------------------------------------------------------------------
	waitforit - Waits for a command started by slowcmd to complete.
---------------------------------------------------------------------------*/

int waitforit(int timelimit, int state, int port, char * where)
{
	int	i,j;

	j=0;
	i=0;
#ifdef DEBUGCMD
	printf("matcd: waitforit port %x timelimit %x hz %x\n",
	       port,timelimit,hz);
#endif /*DEBUGCMD*/
	while (i<timelimit) {
		j=inb(port+STATUS) & (STEN|DTEN);	/*Read status*/
		if (j!=(STEN|DTEN)) break;
		tsleep((caddr_t)&nextcontroller, PRIBIO, where, hz/100);
		i++;
	}
#ifdef DEBUGCMD
	printf("matcd: Count was %d\n",i);
#endif /*DEBUGCMD*/
	if (j==state) return(0);	/*Command complete*/
#ifdef DEBUGCMD
	printf("matcd: Timeout!");
#endif /*DEBUGCMD*/
	return(1);			/*Timeout occurred*/
}


/*---------------------------------------------------------------------------
	lockbus - Wait for the bus on the requested driver interface
		to go idle and acquire it.
	Created in Edit 6
---------------------------------------------------------------------------*/

void lockbus(int controller, int ldrive)
{
#ifdef DEBUGSLEEP
	printf("matcd%d: Requesting bus lock in lockbus, controller %d\n",ldrive,controller);
#endif /*DEBUGSLEEP*/
	while ((if_state[controller] & BUSBUSY)) {
#ifdef DEBUGSLEEP
		printf("matcd%d: Can't do it now - going to sleep %d\n",
		       ldrive,controller);
#endif /*DEBUGSLEEP*/
		tsleep((caddr_t)&matcd_data->status, PRIBIO,
		       "matlck", 0);
	}
	if_state[controller] |= BUSBUSY;	/*It's ours NOW*/
#ifdef DEBUGSLEEP
	printf("matcd%d: Bus now locked in lockbus, controller %d\n",ldrive,controller);
#endif /*DEBUGSLEEP*/
}


/*---------------------------------------------------------------------------
	unlockbus - Release the host interface bus we already have so
		someone else can use it.
	Created in Edit 6
---------------------------------------------------------------------------*/

void unlockbus(int controller, int ldrive)
{
	if_state[controller] &= ~BUSBUSY;
#ifdef DEBUGSLEEP
	printf("matcd%d: bus unlocked %d\n",ldrive,controller);
#endif /*DEBUGSLEEP*/
	wakeup((caddr_t)&matcd_data->status);	/*Wakeup other users*/
	matcd_start(&request_head[controller]);	/*Wake up any block I/O*/
}


/*---------------------------------------------------------------------------
	media_chk - 	Checks error for types related to media
			changes.
---------------------------------------------------------------------------*/

int media_chk(struct matcd_data *cd,int errnum,int ldrive,int test)
{
	if (errnum==NOT_READY ||
	    errnum==MEDIA_CHANGED ||
	    errnum==HARD_RESET ||
	    errnum==DISC_OUT) {
		cd->flags &= ~MATCDLABEL;	/*Mark label as invalid*/
		if (test==0) {		/*<14>Do warn by default*/

			if ((cd->flags & MATCDWARN)==0) {/*Msg seen already?*/
				printf("matcd%d: Media changed - Further I/O aborted until device closed\n",ldrive);
				cd->flags |= MATCDWARN;
			}
		}
		return(1);
	}
	if (errnum==MODE_ERROR)			/*Maybe the setting is*/
		cd->drivemode=MODE_UNKNOWN;	/*wrong so force a reset*/
	return(0);
}


/*---------------------------------------------------------------------------
	docmd - Get the bus, do the command, wait for completion,
		attempt retries, give up the bus.
		For commands that do not return data.
---------------------------------------------------------------------------*/

int docmd(char * cmd, int ldrive, int cdrive, int controller, int port)
{
	int	retries,i,z;

	lockbus(controller, ldrive);	/*Request bus*/
	retries=3;
	while(retries-- > 0) {
		matcd_slowcmd(port,ldrive,cdrive,cmd);
		i=waitforit(80*TICKRES,DTEN,port,"matcmd");
		z=get_stat(port,ldrive);/*Read status byte*/
		if ((z & MATCD_ST_ERROR)==0) break;
		i=chk_error(get_error(port,ldrive,cdrive));
		if (i!=ERR_INIT) {
			unlockbus(controller, ldrive);	/*Release bus*/
			return(EFAULT);
		}
	}
	unlockbus(controller, ldrive);	/*Release bus*/
	return(i);
}


/*---------------------------------------------------------------------------
	get_error - Read the error that aborted a command.
	Created in Edit 6
---------------------------------------------------------------------------*/

int get_error(int port, int ldrive, int cdrive)
{
	int	status,errnum;
	unsigned char	cmd1[MAXCMDSIZ];
	unsigned char	data[12];

	zero_cmd(cmd1);
	cmd1[0]=READERROR;		/*Enquire*/
	matcd_fastcmd(port,ldrive,cdrive,cmd1);
	matcd_pread(port, 8, data);	/*Read data returned*/
	errnum=data[2];			/*Caller wants it classified*/
	status=get_stat(port,ldrive);	/*Read status byte*/

#ifdef DEBUGCMD
	printf("matcd%d: Chkerror found %x on command %x addrval %x statusdata %x statusport %x\n",
		ldrive,errnum,data[1],data[0],status,inb(port+STATUS));
#endif /*DEBUGCMD*/
	return(errnum);
}


/*---------------------------------------------------------------------------
	chk_error - Classify the error that the drive reported
	Created in Edit 6
---------------------------------------------------------------------------*/

int chk_error(int errnum)
{
	switch(errnum) {

/*	These are errors we can attempt a retry for, although the drive
	has already done so.
*/
	case	UNRECV_ERROR:
	case	SEEK_ERROR:
	case	TRACK_ERROR:
	case	FOCUS_ERROR:
	case	CLV_ERROR:
	case	DATA_ERROR:
	case	MODE_ERROR:		/*Make this retryable*/
		return(ERR_RETRY);

/*	These errors usually indicate the user took the media from the
	drive while the dev was open.  We will invalidate the unit
	until it closes when we see this.
*/
	case	NOT_READY:
	case	MEDIA_CHANGED:
	case	DISC_OUT:
	case	HARD_RESET:
		return	(ERR_INIT);

/*	These errors indicate the system is confused about the drive
	or media, and point to bugs in the driver or OS.  These errors
	cannot be retried since you will always get the same error.

	case	RAM_ERROR:
	case	DIAG_ERROR:
	case	CDB_ERROR:
	case	END_ADDRESS:
	case	ILLEGAL_REQ:
	case	ADDRESS_ERROR:

	They also get the same action as default, so they are commented-out
	intentionally.  A smart compiler would do the right thing and generate
	no code if not commented-out, but we don't seem to have any smart
	compilers these days.
*/
	default:
		return	(ERR_FATAL);
	}
}


/*---------------------------------------------------------------------------
	matcd_pread - Read small blocks of control data from a drive
---------------------------------------------------------------------------*/

void matcd_pread(int port, int count, unsigned char * data)
{
	int	i;

	for (i=0; i<count; i++) {
		*data++ = inb(port+CMD);
	}
	return;
}


/*---------------------------------------------------------------------------
	matcdstrategy - Accepts I/O requests from kernel for processing

	This routine accepts a read request block pointer (historically
	but somewhat inaccurately called *bp for buffer pointer).
	Various sanity checks are performed on the request.
	When we are happy with the request and the state of the device,
	the request is added to the queue of requests for the interface
	that the drive is connected to.  We support multiple interfaces
	so there are multiple queues.  Once the request is added, we
	call the matcd_start routine to start the device in case it isn't
	doing something already.   All I/O including ioctl requests
	rely on the current request starting the next one before exiting.
---------------------------------------------------------------------------*/

static void matcdstrategy(struct bio *bp)
{
	struct matcd_data	*cd;
	struct bio_queue_head	*dp;
	int	s;
	int	ldrive,controller;

	ldrive=matcd_ldrive(bp->bio_dev);
	controller=matcd_controller(bp->bio_dev);
	cd= &matcd_data[ldrive];

#ifdef DEBUGIO
	printf("matcd%d: Strategy: buf=0x%lx, block#=%lx bcount=%ld\n",
		ldrive,(unsigned long)bp,(unsigned long)bp->bio_blkno,
		bp->bio_bcount);


	printf("ldrive %x controller %x cd %lx\n",ldrive,controller,
		(unsigned long)cd);
#endif /*DEBUGIO*/


	if (ldrive >= TOTALDRIVES || bp->bio_blkno < 0) {
		printf("matcd%d: Bogus parameters received - kernel may be corrupted\n",ldrive);
		bp->bio_error=EINVAL;
		goto bad;
	}

	if (!(cd->flags & MATCDLABEL)) {
		bp->bio_error = EIO;
		goto bad;
	}

	if (!(bp->bio_cmd & BIO_READ)) {
		bp->bio_error = EROFS;
		goto bad;
	}

	if (bp->bio_bcount==0) {	/*Request is zero-length - all done*/
		goto done;
	}

#ifdef NOTEDIT42
	if (matcd_partition(bp->bio_dev) != RAW_PART) {
		if (bounds_check_with_label(bp,&cd->dlabel,1) <= 0) {
			goto done;
		}
	}
#endif /*NOTEDIT42*/
	bp->bio_pblkno=bp->bio_blkno;
	bp->bio_resid=0;

	s=splbio();			/*Make sure we don't get intr'ed*/

	dp=&request_head[controller];	/*Pointer to controller queue*/

	bioq_disksort(dp,bp);		/*Add new request (bp) to queue (dp
					  and sort the requests in a way that
					  may not be ideal for CD-ROM media*/

	matcd_start(dp);		/*Ok, with our newly sorted queue,
					  see if we can start an I/O operation
					  right now*/
	splx(s);			/*Return priorities to normal*/
					/*All the BSD books say do this here,
					  but some drivers are doing it after
					  disksort, which seems dangerous.*/
	return;				/*All done*/

bad:	bp->bio_flags |= BIO_ERROR;	/*Request bad in some way*/
done:	bp->bio_resid = bp->bio_bcount;	/*Show amount of data un read*/
	biodone(bp);			/*Signal we have done all we plan to*/
	return;
}


/*---------------------------------------------------------------------------
	matcd_start - Pull a request from the queue and consider doing it.
---------------------------------------------------------------------------*/

static void matcd_start(struct bio_queue_head *dp)
{
	struct matcd_data *cd;
	struct bio *bp;
	struct partition *p;
	int part,ldrive,controller;
	int s;

	s=splbio();
	bp=bioq_first(dp);
	if (bp==0) {			/*Nothing on this controller queue*/
		splx(s);
		wakeup((caddr_t)&matcd_data->status);	/*Wakeup any blocked*/
		return;					/* opens, ioctls, etc*/
	}
	ldrive=matcd_ldrive(bp->bio_dev);
	controller=matcd_controller(bp->bio_dev);
	cd= &matcd_data[ldrive];

#ifdef DEBUGIO
	printf("matcd%d: In start controller %d\n",ldrive,controller);
#endif	/*DEBUGIO*/

	if (if_state[controller] & BUSBUSY) {
#ifdef DEBUGIO
		printf("matcd%d: Dropping thread in start,  controller %d\n",
	       		ldrive,controller);
#endif	/*DEBUGIO*/
		return;
	}

	bioq_remove(&request_head[controller], bp);	/*Delete off queue*/

/*	Ok, the controller is idle (not necessarily the drive) and so
	get the command to do and issue it
*/
	part=matcd_partition(bp->bio_dev);
	p=cd->dlabel.d_partitions + part;

	if_state[controller] |= BUSBUSY;/*Mark bus as busy*/
	cd->mbx.ldrive=ldrive;		/*Save current logical drive*/
	cd->mbx.controller=controller;	/*and controller*/
	cd->mbx.partition=part;		/*and partition (2048 vs 2532)*/
	cd->mbx.port=cd->iobase;	/*and port#*/
	cd->mbx.iftype=cd->iftype;	/*interface type*/
	cd->mbx.retry=MATCD_RETRYS;	/*and the retry count*/
	cd->mbx.bp=bp;			/*and the bp*/
	cd->mbx.p_offset=p->p_offset;	/*and where the data will go*/
	matcd_blockread(MATCD_READ_1+ldrive);	/*Actually start the read*/
	return;				/*Dropping thread.  matcd_blockread
					  must have scheduled a timeout or
					  we will go to sleep forever*/
}


/*---------------------------------------------------------------------------
	matcdioctl - Process things that aren't block reads

	In this driver, ioctls are used mainly to change
	the mode the drive is running in, play audio and other
	things that don't fit into the block read scheme of things.
---------------------------------------------------------------------------*/

static int matcdioctl(dev_t dev, unsigned long command,
			   caddr_t addr, int flags,
			   struct thread *td)
{
	struct	matcd_data *cd;
	int	ldrive, cdrive, partition;
	int	port, controller;
#ifdef DEBUGIOCTL
	int	i;
#endif	/*DEBUGIOCTL*/

	ldrive=matcd_ldrive(dev);
	cdrive=matcd_cdrive(dev);
	partition=matcd_partition(dev);
	controller=ldrive>>2;
	cd=&matcd_data[ldrive];
	port=cd->iobase;

#ifdef DEBUGIOCTL
	printf("matcd%d: ioctl %lx cdrive %x parms ",ldrive,command,cdrive);
	for (i=0;i<10;i++) {
		printf("%02x ",(unsigned int)addr[i]);
	}
	printf("  flags %x\n",cd->flags);
#endif	/*DEBUGIOCTL*/

	if (command==CDIOCCLOSE) { 	/*Allow close even if door open*/
		return(matcd_doorclose(ldrive, cdrive, controller));
	}
	if (command==CDIOCEJECT) {	/*Allow with tray empty or full*/
		return(matcd_eject(ldrive, cdrive, controller));
	}

	if (!(cd->flags & MATCDLABEL)) {/*Did we read TOC OK? on open?*/
		return(EIO);		/*then drive really isn't ready*/
	}

	switch(command) {
	case	DIOCGDINFO:
		*(struct disklabel *) addr = cd->dlabel;
		return(0);

#ifdef NOTEDIT42
	case	DIOCWDINFO:
	case	DIOCSDINFO:
		if ((flags & FWRITE) == 0) {
			return(EBADF);
		}
		else {
			return setdisklabel(&cd->dlabel,
				            (struct disklabel *) addr, 0);
		}
#endif /*NOTEDIT42*/

	case	CDIOCALLOW:
		return(matcd_dlock(ldrive, cdrive,
		       controller,0));

	case	CDIOCPREVENT:
		return(matcd_dlock(ldrive, cdrive,
		       controller, MATCDLOCK));

#ifdef FULLDRIVER
	case	CDIOCPLAYTRACKS:
		return(matcd_playtracks(ldrive, cdrive, controller,
		       (struct ioc_play_track *) addr));

	case	CDIOCPLAYMSF:
		return(matcd_playmsf(ldrive, cdrive, controller,
		       (struct ioc_play_msf *) addr));

	case	CDIOCPLAYBLOCKS:
		return(matcd_playblk(ldrive, cdrive, controller,
		       (struct ioc_play_blocks *) addr));

	case	CDIOCRESUME:
		return(matcd_pause(ldrive, cdrive, controller,RESUME));

	case	CDIOCPAUSE:
		return(matcd_pause(ldrive, cdrive, controller,0));

	case	CDIOCSTOP:
		return(matcd_stop(ldrive, cdrive, controller));

	case	CDIOCGETVOL:
	case	CDIOCSETVOL:
		return(matcd_level(ldrive, cdrive, controller,
		       (struct ioc_vol *) addr, command));

	case	CDIOCSETMONO:
		return(EINVAL);

					/*SRC OUT	SRC OUT*/
	case	CDIOCSETSTEREO:		/*0 -> L	1 -> R*/
	case	CDIOCSETMUTE:		/*0 -> NULL	1 -> NULL*/
	case	CDIOCSETLEFT:		/*0 -> L&R	1 -> NULL*/
	case	CDIOCSETRIGHT:		/*0 -> NULL	1 -> L&R*/
					/*Adjust audio routing*/
		return(matcd_route(ldrive, cdrive, controller,
		       command));

	case	CDIOCSETPATCH:
		return(matcd_patch(ldrive, cdrive, controller,
		       (struct ioc_patch *) addr));

	case	CDIOCPITCH:
		return(matcd_pitch(ldrive, cdrive, controller,
		       (struct ioc_pitch *) addr));

	case	CDIOCSTART:		/*Only reason this isn't*/
		return(EINVAL);		/*implemented is I can't find out*/
					/*what it should do!*/
#endif	/*FULLDRIVER*/

	case	CDIOREADTOCHEADER:
		return(matcd_toc_header(ldrive, cdrive, controller,
		       (struct ioc_toc_header *) addr));

	case	CDIOREADTOCENTRYS:
		return(matcd_toc_entries(ldrive, cdrive, controller,
		       (struct ioc_read_toc_entry *) addr));

	case	CDIOCREADSUBCHANNEL:
		return(matcd_read_subq(ldrive, cdrive, controller,
		       (struct ioc_read_subchannel *) addr));

	case	CDIOCCAPABILITY:	/*Request drive/driver capability*/
		return(matcd_igot((struct ioc_capability *) addr));

	case	CDIOCRESET:		/*There is no way to hard reset*/
		return(EINVAL);		/*just one drive*/

	default:
		return(ENOTTY);
	}
}

/*---------------------------------------------------------------------------
	matcdsize - Reports how many blocks exist on the disc.
---------------------------------------------------------------------------*/

static int	matcdsize(dev_t dev)
{
	int	size,blksize;
	int	ldrive,part;
	struct	matcd_data *cd;

	ldrive=matcd_ldrive(dev);
	part=matcd_partition(dev);
	if (part==RAW_PART)
		blksize=MATCDRBLK;	/*2352*/
	else
		blksize=MATCDBLK;	/*2048*/

	cd = &matcd_data[ldrive];

	if (matcd_volinfo(ldrive) >= 0) {
		cd->blksize=blksize;
		size=msf_to_blk((char * )&cd->volinfo.vol_msf);

		cd->disksize=size*(blksize/DEV_BSIZE);
#ifdef DEBUGOPEN
	printf("matcd%d: Media size %d\n",ldrive,size);
#endif /*DEBUGOPEN*/
		return(0);
	}
	return(-1);
}


/*---------------------------------------------------------------------------
	matcd_setmode - Configures disc to run in the desired data mode

	This routine assumes the drive is already idle.

NOTE -	Undocumented action of hardware:  If you change (or reaffirm) data
	modes with MODESELECT + BLOCKPARAM immediately after a command was
	issued that aborted a DA play operation, the drive will unexpectedly
	return 2532 bytes of data in a data phase on the first or second
	subsequent command.

	Original Symptom: drive will refuse to go idle after reading data
	and status expected for a command.  State mechanics for this are
	not fully understood.
---------------------------------------------------------------------------*/

static int matcd_setmode(int ldrive, int mode)
{
	struct	matcd_data *cd;
	int retries;
	int i,port,cdrive;
	unsigned char cmd[MAXCMDSIZ];

	cd = matcd_data + ldrive;
	retries=3;
	cdrive=ldrive&0x03;
	port=cd->iobase;
	if (cd->drivemode==mode) {
		return(0);		/*Drive already set*/
	}

/*	The drive is not in the right mode, so we need to set it.
*/

	zero_cmd(cmd);
	cmd[0]=MODESELECT;		/*Set drive transfer modes*/
/*	cmd[1]=BLOCKPARAM;		  BLOCKPARAM==0 & cmd is zero*/
	cmd[2]=mode;
	switch(mode) {
	case MODE_DATA:
		cmd[3]=0x08;		/*2048 bytes*/
		break;
	case MODE_USER:
		cmd[3]=0x09;		/*2352 bytes*/
		cmd[4]=0x30;
		break;
	case MODE_DA:
		cmd[3]=0x09;		/*2352 bytes*/
		cmd[4]=0x30;
		break;
	}
	i=0;
	while(retries-- > 0) {
		i=matcd_fastcmd(port,ldrive,cdrive,cmd);
		get_stat(port,ldrive);	/*Read and toss status byte*/
		if (i==0) {
			cd->drivemode=mode;	/*Set new mode*/
			return(i);
		}
		get_error(port,ldrive,cdrive);
	}
	cd->drivemode=MODE_UNKNOWN;	/*We failed*/
	return(i);
}


/*---------------------------------------------------------------------------
	matcd_volinfo - Read information from disc Table of Contents
---------------------------------------------------------------------------*/

static	int	matcd_volinfo(int ldrive)
{
	struct	matcd_data *cd;
	int	port,i;
	int	z,cdrive;
	unsigned char cmd[MAXCMDSIZ];
	unsigned char data[12];
	int	 retry;

	retry=10;
	cd = &matcd_data[ldrive];
	cdrive=ldrive&0x03;
	port=cd->iobase;

#ifdef DEBUGOPEN
	printf("matcd%d: In volinfo, port %x\n",ldrive,port);
#endif /*DEBUGOPEN*/

	while(retry>0) {
		zero_cmd(cmd);
		cmd[0]=READDINFO;	/*Read Disc Info*/
		matcd_slowcmd(port,ldrive,cdrive,cmd);
		i=waitforit(10*TICKRES,DTEN,port,"matvinf");
		if (i) {		/*THIS SHOULD NOT HAPPEN*/
			z=get_stat(port,ldrive);/*Read status byte*/
			printf("matcd%d: command failed, status %x\n",
			       ldrive,z);
			return(-1);
		}
		matcd_pread(port, 6, data);	/*Read data returned*/
		z=get_stat(port,ldrive);/*Read status byte*/
#ifdef DEBUGOPEN
		printf("matcd%d: Data got was %x %x %x %x %x %x   ",ldrive,
		       data[0],data[1],data[2], data[3],data[4],data[5]);
		printf("status byte %x\n",z);
#endif	/*DEBUGOPEN*/
		if ((z & MATCD_ST_ERROR)==0)
			break;		/*No Error*/

/*	If media change or other error, you have to read the error data or
	the drive will reject subsequent commands.
*/

		i=get_error(port, ldrive, cdrive);
#ifdef TAKEOUT
		if (media_chk(cd,i,ldrive,0)) {
			return(-1);
		}
#endif
		if (chk_error(i)==ERR_FATAL) {	/*Should not chg here*/
#ifdef DEBUGOPEN
			printf("matcd%d: command failed, status %x\n",
			       ldrive,z);
#endif /*DEBUGOPEN*/
			return(-1);
		}
		tsleep((caddr_t)&nextcontroller, PRIBIO, "matvi2", hz);/*<25>*/
		if ((--retry)==0) return(-1);
#ifdef DEBUGOPEN
		printf("matcd%d: Retrying err was %d",ldrive,i);
#endif /*DEBUGOPEN*/
	}
#ifdef DEBUGOPEN
	printf("matcd%d: Status port %x  \n",ldrive,inb(port+STATUS));
#endif /*DEBUGOPEN*/

	cd->volinfo.type=data[0];
	cd->volinfo.trk_high=data[2];
	cd->volinfo.trk_low=data[1];
	cd->volinfo.vol_msf[0]=data[3];
	cd->volinfo.vol_msf[1]=data[4];
	cd->volinfo.vol_msf[2]=data[5];

	if (cd->volinfo.trk_low + cd->volinfo.trk_high) {
		cd->flags |= MATCDLABEL;
		return(0);
	}
	return(-1);
}


/*---------------------------------------------------------------------------
	blk_to_msf - Convert block numbers into CD disk block ids
---------------------------------------------------------------------------*/

static void blk_to_msf(int blk, unsigned char *msf)
{
	blk=blk+150;			/*2 seconds skip required to
					  reach ISO data*/
	msf[0]=blk/4500;
	blk=blk%4500;
	msf[1]=blk/75;
	msf[2]=blk%75;
	return;
}


/*---------------------------------------------------------------------------
	msf_to_blk - Convert CD disk block ids into block numbers
---------------------------------------------------------------------------*/

static int msf_to_blk(unsigned char * cd)
{
	return(((cd[0]*60)		/*Convert MSF to*/
	       +cd[1])*75		/*Blocks minus 2*/
	       +cd[2]-150);		/*seconds*/
}


/*---------------------------------------------------------------------------
---------------------------------------------------------------------------*/

static void
matcd_timeout(void *state)
{
	matcd_blockread((int)state);
}


/*---------------------------------------------------------------------------
	matcd_blockread - Performs actual background disc I/O operations

	This routine is handed the block number to read, issues the
	command to the drive, waits for it to complete, reads the
	data or error, retries if needed, and returns the results
	to the host.
---------------------------------------------------------------------------*/

static void matcd_blockread(int state)
{
	struct	matcd_mbx *mbx;
	int	ldrive,cdrive;
	int	port;
	short	iftype;
	struct	bio *bp;
	struct	bio_queue_head *dp;
	struct	matcd_data *cd;
	int	i;
	struct	matcd_read2 rbuf;
	int	blknum;
	caddr_t addr;
	int	status;
	int	errtyp;
	int	phase;
	unsigned char cmd[MAXCMDSIZ];

	mbx=&matcd_data[state & 0x0f].mbx;
	ldrive=mbx->ldrive;		/*ldrive is logical drive #*/
	cdrive=ldrive & 0x03;		/*cdrive is drive # on a controller*/
	port=mbx->port;			/*port is base port for i/f*/
	iftype=mbx->iftype;
	bp=mbx->bp;
	cd=&matcd_data[ldrive];

	dp=&request_head[mbx->controller];

#ifdef DEBUGIO
	printf("matcd%d: Show state %x cdrive %d partition %d\n",
	       ldrive,state,cdrive,mbx->partition);
#endif /*DEBUGIO*/

loop:
#ifdef DEBUGIO
	printf("matcd%d: Top  dp %x\n",ldrive,(unsigned int)dp);
#endif /*DEBUGIO*/
	switch (state & 0xf0) {
	case	MATCD_READ_1:
#ifdef DEBUGIO
		printf("matcd%d: State 1 cd->flags %x\n",ldrive,cd->flags);
#endif /*DEBUGIO*/
		/* to check for raw/cooked mode */
		if (cd->partflags[mbx->partition] & MATCDREADRAW) {
			mbx->sz = MATCDRBLK;
			i=matcd_setmode(ldrive, MODE_DA);
#ifdef DEBUGIO
			printf("matcd%d: Set MODE_DA result %d\n",ldrive,i);
#endif /*DEBUGIO*/
		} else {
			mbx->sz = cd->blksize;
			i=matcd_setmode(ldrive, MODE_DATA);
#ifdef DEBUGIO
			printf("matcd%d: Set MODE_DATA result %d\n",ldrive,i);
#endif /*DEBUGIO*/
		}
		/*for first block*/
#ifdef DEBUGIOPLUS
		printf("matcd%d: A mbx %x bp %x b_bcount %x sz %x\n",
		       ldrive,(unsigned int)mbx,(unsigned int)bp,
		       (unsigned int)bp->b_bcount,mbx->sz);
#endif /*DEBUGIO*/
		mbx->nblk=(bp->bio_bcount+(mbx->sz-1))/mbx->sz;
		mbx->skip=0;
nextblock:
#ifdef DEBUGIO
		printf("matcd%d: at Nextblock b_blkno %d\n",
		       ldrive,(unsigned int)bp->bio_blkno);
#endif /*DEBUGIO*/

		blknum=(bp->bio_blkno/(mbx->sz/DEV_BSIZE))+
		       mbx->p_offset+mbx->skip/mbx->sz;

		blk_to_msf(blknum,rbuf.start_msf);

		zero_cmd(cmd);
		cmd[0]=READ;		/*Get drive ID*/
		cmd[1]=rbuf.start_msf[0];
		cmd[2]=rbuf.start_msf[1];
		cmd[3]=rbuf.start_msf[2];
		cmd[6]=1;		/*Xfer only one block*/
		matcd_slowcmd(port,ldrive,cdrive,cmd);

/*	Now that we have issued the command, check immediately to
	see if data is ready.   The drive has read-ahead caching, so
	it is possible the data is already in the drive buffer.

	If the data is not ready, schedule a wakeup and later on this
	code will run again to see if the data is ready then.
*/

	case MATCD_READ_2:
		state=MATCD_READ_2+ldrive;
		phase = (inb(port+STATUS)) & (DTEN|STEN);
#ifdef DEBUGIO
		printf("matcd%d: In state 2 status %x  ",ldrive,phase);
#endif /*DEBUGIO*/
		switch(phase) {
		case (DTEN|STEN):	/*DTEN==H  STEN==H*/
#ifdef DEBUGIO
			printf("matcd%d: Sleeping %x\n",ldrive,MATCD_READ_2+ldrive);
#endif /*DEBUGIO*/
			timeout(matcd_timeout,
				(caddr_t)(MATCD_READ_2+ldrive),hz/100);
			return;


		case	STEN:		/*DTEN=L STEN=H*/
		case	0:		/*DTEN=L STEN=L*/
#ifdef DEBUGIO
			printf("matcd%d: Data Phase\n",ldrive);
#endif /*DEBUGIO*/
			addr=bp->bio_data + mbx->skip;
#ifdef DEBUGIO
			printf("matcd%d: Xfer Addr %x  size %x",
			       ldrive,(unsigned int)addr,mbx->sz);
			i=0;			/*Reset read count*/
#endif /*DEBUGIO*/
			if (iftype==0) {	/*Creative host I/F*/
				outb(port+PHASE,1);	/*Enable data read*/
				while((inb(port+STATUS) &
				      (DTEN|STEN))==STEN) {
					*addr++=inb(port+DATA);
#ifdef	DEBUGIO
					i++;
#endif /*DEBUGIO*/
				}
				outb(port+PHASE,0);	/*Disable read*/
			} else {		/*Not Creative interface*/
				while((inb(port+STATUS) &
				      (DTEN|STEN))==STEN) {
					*addr++=inb(port+ALTDATA);
#ifdef	DEBUGIO
					i++;
#endif	/*DEBUGIO*/
				}
			}
#ifdef DEBUGIO
			printf("matcd%d: Read %d bytes\n",ldrive,i);
#endif /*DEBUGIO*/


/*	Now, wait for the Status phase to arrive.   This will also
	tell us if any went wrong with the request.
*/
			while((inb(port+STATUS)&(DTEN|STEN)) != DTEN);
			status=get_stat(port,ldrive);	/*Read status byte*/
#ifdef DEBUGIO
			printf("matcd%d: Status port %x byte %x  ",
			       ldrive,i,status);
#endif /*DEBUGIO*/
			if (status & MATCD_ST_ERROR) {
				i=get_error(port,ldrive,cdrive);
				printf("matcd%d: %s while reading block %d [Soft]\n",
				       ldrive,matcderrors[i],(int)bp->bio_blkno);
				media_chk(cd,i,ldrive,0);/*<14>was wrong place*/
			}

			if (--mbx->nblk > 0) {
				mbx->skip += mbx->sz;
				goto nextblock;	/*Oooooh, you flunk the course*/
			}
			bp->bio_resid=0;
			biodone(bp);	/*Signal transfer complete*/

			unlockbus(ldrive>>2, ldrive);	/*Release bus lock*/
			matcd_start(dp);/*See if other drives have work*/
			return;

/*	Here we skipped the data phase and went directly to status.
	This indicates a hard error.
*/

		case	DTEN:		/*DTEN=H STEN=L*/
			status=get_stat(port,ldrive);	/*Read status byte*/
#ifdef DEBUGIO
			printf("matcd%d: error, status was %x\n",
			       ldrive,status);
#endif /*DEBUGIO*/

/*	Ok, we need more details, so read error.  This is needed to issue
	any further commands anyway
*/

			errtyp=get_error(port,ldrive,cdrive);
			printf("matcd%d: %s while reading block %d\n",
				ldrive,matcderrors[errtyp],(int)bp->bio_blkno);

			if (media_chk(cd,errtyp,ldrive,0)==0) {
				errtyp=chk_error(errtyp);
				if (errtyp==ERR_RETRY) {/*We can retry*/
					/*<14>this error but the drive*/
					/*<14>probably has already*/
					if (mbx->retry-- > 0 ) {
						state=MATCD_READ_1+ldrive;
#ifdef DEBUGIO
						printf("matcd%d: Attempting retry\n",
						ldrive);
#endif /*DEBUGIO*/
						goto loop;
					}
				}
			}
/*
	The other error types are either something very bad or the media
	has been removed by the user.  In both cases there is no retry
	for this call.  We will invalidate the label in both cases.
*/
			bp->bio_flags |= BIO_ERROR;
			bp->bio_resid = bp->bio_bcount;
			biodone(bp);
			unlockbus(ldrive>>2, ldrive);
			matcd_start(dp);
			return;
		}
	}
}


/*---------------------------------------------------------------------------
	matcd_eject - Open drive tray
---------------------------------------------------------------------------*/

int matcd_eject(int ldrive, int cdrive, int controller)
{
	int	 i,port;
	struct	matcd_data	*cd;
	unsigned	char	cmd[MAXCMDSIZ];

	cd=&matcd_data[ldrive];
	port=cd->iobase;		/*Get I/O port base*/

#ifdef LOCKDRIVE
	if (cd->flags & MATCDLOCK) {	/*Drive was locked via open*/
		return(EINVAL);		/*so don't allow the eject*/
	}
#endif /*LOCKDRIVE*/
	zero_cmd(cmd);			/*Initialize command buffer*/
	cmd[0]=LOCK;			/*Unlock drive*/
	i=docmd(cmd,ldrive,cdrive,controller,port);	/*Issue command*/
	cmd[0]=DOOROPEN;		/*Open Door*/
	i=docmd(cmd,ldrive,cdrive,controller,port);	/*Issue command*/
	cd->flags &= ~(MATCDLABEL|MATCDLOCK);	/*Mark vol info invalid*/
	return(i);			/*Return result we got*/
}


/*---------------------------------------------------------------------------
	matcd_doorclose - Close drive tray
<16>	Added in Edit 16
---------------------------------------------------------------------------*/

int matcd_doorclose(int ldrive, int cdrive, int controller)
{
	int	 i,port;
	struct	matcd_data *cd;
	unsigned	char	cmd[MAXCMDSIZ];

	cd=&matcd_data[ldrive];
	port=cd->iobase;		/*Get I/O port base*/

	zero_cmd(cmd);			/*Initialize command buffer*/
	cmd[0]=DOORCLOSE;		/*Open Door*/
	i=docmd(cmd,ldrive,cdrive,controller,port);	/*Issue cmd impl lck*/
	cd->flags &= ~(MATCDLABEL|MATCDLOCK);	/*Mark vol info invalid*/
	tsleep((caddr_t)&nextcontroller, PRIBIO, "matclos", hz);
	return(i);			/*Return result we got*/
}


/*---------------------------------------------------------------------------
<23>	matcd_dlock - Honor/Reject drive tray requests
---------------------------------------------------------------------------*/

int matcd_dlock(int ldrive, int cdrive, int controller, int action)
{
	int	 i,port;
	struct	matcd_data	*cd;
	unsigned	char	cmd[MAXCMDSIZ];

	cd=&matcd_data[ldrive];
	port=cd->iobase;		/*Get I/O port base*/

	zero_cmd(cmd);			/*Initialize command buffer*/
	cmd[0]=LOCK;			/*Unlock drive*/

	if (action) {			/*They want to lock the door?*/
		cd->flags |= MATCDLOCK;	/*Remember we did this*/
		cmd[1]=1;		/*Lock Door command*/
	} else {
		cd->flags &= ~MATCDLOCK;/*Remember we did this*/
					/*Unlock Door command*/
	}
	i=docmd(cmd,ldrive,cdrive,controller,port);	/*Issue command*/
	return(i);			/*Return result we got*/
}


/*---------------------------------------------------------------------------
	matcd_toc_header - Return Table of Contents header to caller
<13>	New for Edit 13
---------------------------------------------------------------------------*/

static int matcd_toc_header(int ldrive, int cdrive, int controller,
		       struct ioc_toc_header * toc)
{
	struct	matcd_data *cd;
	int	i;

	lockbus(controller, ldrive);	/*Request bus*/
	i=matcd_volinfo(ldrive);	/*Check on manual change state*/
	unlockbus(controller, ldrive);	/*Release bus*/
	if (i==-1) {				/*Did media change manually*/
		return(EIO);
	}
	cd=&matcd_data[ldrive];
	if ((cd->flags & MATCDLABEL)==0)
		return(EIO);		/*Refuse after chg error*/

	toc->len=msf_to_blk(cd->volinfo.vol_msf);	/*In frames*/
	toc->starting_track=cd->volinfo.trk_low;	/*1*/
	toc->ending_track=cd->volinfo.trk_high;		/*Last track*/

	return(0);

}


/*---------------------------------------------------------------------------
	matcd_toc_entries - Read most/all of the TOC entries

	These entries are cached by the drive, but it might be worth
	the space investment to have the driver cache these as well.
	For a disc with 40 tracks, it means 41 command calls to get
	this information from the drive.
<13>	New for Edit 13

---------------------------------------------------------------------------*/

static int matcd_toc_entries(int ldrive, int cdrive, int controller,
			     struct ioc_read_toc_entry * ioc_entry)
{
	struct	matcd_data *cd;
	struct	cd_toc_entry entries[MAXTRKS+1];
	struct	cd_toc_entry *from;
	struct	cd_toc_entry *to;
	int	len,trk,i,z,port;
	unsigned char cmd[MAXCMDSIZ];
	unsigned char data[5];
	unsigned char	lowtrk, hightrk;

	cd=&matcd_data[ldrive];
	port=cd->iobase;
	lowtrk=0xff;
	hightrk=0x00;

	if ((cd->flags & MATCDLABEL)==0) {
		return(EIO);		/*Refuse after chg error*/
	}

	zero_cmd(cmd);
	cmd[0]=READTOC;

	for(trk=cd->volinfo.trk_low-1; trk<cd->volinfo.trk_high; trk++) {
		cmd[2]=trk+1;
		lockbus(controller, ldrive);	/*Request bus*/
		matcd_slowcmd(port,ldrive,cdrive,cmd);
		i=waitforit(10*TICKRES,DTEN,port,"mats1");
		matcd_pread(port, 8, data);	/*Read data returned*/
		z=get_stat(port,ldrive);	/*Read status byte*/
		if ((z & MATCD_ST_ERROR)) {	/*Something went wrong*/
			i=get_error(port, ldrive, cdrive);
			unlockbus(controller, ldrive);	/*Release bus*/
			return(EIO);
		}
		unlockbus(controller, ldrive);	/*Release bus*/

#ifdef DEBUGIOCTL
		printf("%d Track %d addr/ctrl %x  m:%d s:%d f:%d\n",trk,data[2],
		       data[1],data[4],data[5],data[6]);
#endif /*DEBUGIOCTL*/

		entries[trk].control=data[1];	/*Track type*/
		entries[trk].addr_type=ioc_entry->address_format;/*Type*/
		entries[trk].track=data[2];	/*Track #, can be Out of Order*/

		if (data[2]>hightrk) hightrk=data[2];	/*Determine min*/
		if (data[2]<lowtrk)  lowtrk=data[2];	/*and max track #*/

		if (ioc_entry->address_format == CD_MSF_FORMAT) {
			entries[trk].addr.msf.unused=0;
			entries[trk].addr.msf.minute=data[4];	/*Min*/
			entries[trk].addr.msf.second=data[5];	/*Sec*/
			entries[trk].addr.msf.frame=data[6];	/*Frame*/
		} else {
			entries[trk].addr.lba=msf_to_blk(&data[4]);
		}
	}
	entries[trk].control=data[2];	/*Copy from last valid track*/
	entries[trk].track=0xaa;	/*Lead-out*/
	entries[trk].addr_type=ioc_entry->address_format;/*Type*/
	if (ioc_entry->address_format == CD_MSF_FORMAT) {
		entries[trk].addr.msf.unused=0;	/*Fill*/
		entries[trk].addr.msf.minute=cd->volinfo.vol_msf[0];
		entries[trk].addr.msf.second=cd->volinfo.vol_msf[1];
		entries[trk].addr.msf.frame=cd->volinfo.vol_msf[2];
	} else {
		entries[trk].addr.lba=msf_to_blk((char * )&cd->volinfo.vol_msf);
	}
	trk++;				/*Bump to include leadout track*/


/*	Now that we have read all the data from the drive, copy the
	array from the kernel address space into the user address space
*/

	len=ioc_entry->data_len;
	i=ioc_entry->starting_track;	/*What did they want?*/
	if (i==0xaa) i=hightrk+1;	/*Give them lead-out info*/
	else if (i==0) i=lowtrk;	/*Give them lowest track found*/

	from=&entries[i-1];
	to=ioc_entry->data;

	while (i <= trk && len >= sizeof(struct cd_toc_entry)) {
		if (copyout(from,to,sizeof(struct cd_toc_entry)) != 0) {
			return (EFAULT);
		}
		i++;
		len -= sizeof(struct cd_toc_entry);
		from++;
		to++;
	}
	return(0);
}


/*---------------------------------------------------------------------------
	matcd_subq - Read the Sub-Q packet - (where are we?)

	This call gives a snapshot state of where the optical
	pick-up is when the command is issued.
<14>	New for Edit 14
---------------------------------------------------------------------------*/

static int matcd_read_subq(int ldrive, int cdrive, int controller,
			     struct ioc_read_subchannel * sqp)
{
	struct	matcd_data *cd;
	int	i,z,port;
	unsigned char cmd[MAXCMDSIZ];
	unsigned char data[12];
	struct cd_sub_channel_info subq;	/*Build result here*/

	cd=&matcd_data[ldrive];
	port=cd->iobase;

	if ((cd->flags & MATCDLABEL)==0) {
		return(EIO);		/*Refuse after chg error*/
	}

/*	We only support the ioctl functions we could get information
	on, so test for the things we can do
*/

	if (sqp->data_format!=CD_CURRENT_POSITION) {
		return(EINVAL);
	}

	zero_cmd(cmd);
	cmd[0]=READSUBQ;
	lockbus(controller, ldrive);	/*Request bus*/
	matcd_slowcmd(port,ldrive,cdrive,cmd);

/*	While we wait, fill in the hard-coded entries of the table.
<37>	Note that cdcontrol will request the format be LBA when that
	mode is selected, but cdcontrol really wants the replies returned
	in MSF.  Sounds like a bug in cdcontrol, but always returning the
	results in MSF for SUBQ queries is what the IDE driver does, so
	that's what we will do as well.  Set SUBQRETREQUESTED when the driver
	is to return in whatever format was requested.
*/

#ifdef SUBQRETREQUESTED
	subq.what.position.data_format=sqp->address_format;
#else /*SUBQRETREQUESTED*/
	subq.what.position.data_format=CD_MSF_FORMAT;	/*Always ret MSF*/
#endif /*SUBQRETREQUESTED*/
	subq.what.position.absaddr.msf.unused=0;
	subq.what.position.reladdr.msf.unused=0;

	i=waitforit(10*TICKRES,DTEN,port,"mats2");
	matcd_pread(port, 11, data);	/*Read data returned*/
	z=get_stat(port,ldrive);	/*Read status byte*/
	if ((z & MATCD_ST_ERROR)) {	/*Something went wrong*/
		i=get_error(port, ldrive, cdrive);
		unlockbus(controller, ldrive);	/*Release bus*/
		return(EIO);
	}
	unlockbus(controller, ldrive);	/*Release bus*/

#ifdef DEBUGIOCTL
	printf("Subq track %d index %d adr/ctl %x  abs %d:%2d:%2d  rel %d:%2d:%2d UPC %x\n",
	       data[2],data[3],data[1],data[4],data[5],data[6],
	       data[7],data[8],data[9],data[10]);
#endif /*DEBUGIOCTL*/

	if (z & MATCD_ST_AUDIOBSY) {	/*Drive playing or paused*/
		if (cd->status==CD_AS_PLAY_PAUSED) {	/*Have we issued*/
			i=cd->status;		/*a pause command?*/
		} else {
			i=CD_AS_PLAY_IN_PROGRESS;/*No, we really are playing*/
		}
	} else {
		if (cd->status==CD_AS_PLAY_IN_PROGRESS) {/*It was playing*/
			i=CD_AS_PLAY_COMPLETED;	/*so it finished*/
		} else {			/*Any other status reported*/
			i=cd->status;		/*as we get it*/
		}
	}

	subq.header.audio_status=cd->status=i;	/*Store status we selected*/

	subq.what.position.track_number=data[2];
	subq.what.position.index_number=data[3];

	if (sqp->address_format==CD_MSF_FORMAT) {	/*Req MSF format*/
		subq.what.position.absaddr.msf.minute=data[4];
		subq.what.position.absaddr.msf.second=data[5];
		subq.what.position.absaddr.msf.frame=data[6];

		subq.what.position.reladdr.msf.minute=data[7];
		subq.what.position.reladdr.msf.second=data[8];
		subq.what.position.reladdr.msf.frame=data[9];
	} else {					/*Req LBA format*/
		subq.what.position.absaddr.lba=msf_to_blk(&data[4]);
		subq.what.position.reladdr.lba=msf_to_blk(&data[7]);
	}

/*	Ok, now copy our nicely-built structure from the kernel address
	space into the user address space (we hope)
*/

	if (copyout(&subq, sqp->data,
	    min(sizeof(struct cd_sub_channel_info), sqp->data_len))!=0) {
		return(EFAULT);
	}
	return(0);
}


/*---------------------------------------------------------------------------
	matcd_igot - Like the song, report the capabilities that the
		     drive/driver has available.

	This call returns a structure of flags indicating what
	functions are available so that the application can offer
	only the functions the drive is actually capable of.
<16>	New for Edit 16
---------------------------------------------------------------------------*/

static int matcd_igot(struct ioc_capability * sqp)
{

#ifdef FULLDRIVER
	sqp->play_function=(CDDOPLAYTRK |	/*Can play trks/indx*/
			   CDDOPLAYMSF |	/*Can play msf to msf*/
			   CDDOPAUSE |		/*Can pause playback*/
			   CDDORESUME |		/*Can resume playback*/
			   CDDOSTOP |		/*Can stop playback*/
			   CDDOPITCH);		/*Can change play pitch*/

	sqp->routing_function=(CDREADVOLUME |	/*Can read volume*/
			      CDSETVOLUME |	/*Can set volume*/
			      CDSETSTEREO |	/*Can select stereo play*/
			      CDSETLEFT |	/*Can select left-only*/
			      CDSETRIGHT |	/*Can select right-only*/
			      CDSETMUTE |	/*Can mute audio*/
			      CDSETPATCH);	/*Direct patch settings*/
#else /*FULLDRIVER*/
	sqp->play_function=0;			/*No audio capability*/
	sqp->routing_function=0;		/*No audio capability*/
#endif /*FULLDRIVER*/

	sqp->special_function=(CDDOEJECT |	/*Door can be opened*/
			      CDDOCLOSE |	/*Door can be closed*/
			      CDDOLOCK |	/*Door can be locked*/
			      CDREADSUBQ |	/*Can read subchannel*/
			      CDREADENTRIES |	/*Can read TOC entries*/
			      CDREADHEADER);	/*Can read TOC*/
	return(0);
}


#ifdef FULLDRIVER
/*-----------------------------------------------------------------------------
	The following functions are related to the audio playback
	capabilities of the drive.   They can be omitted from the
	finished driver using the FULLDRIVER conditional.

	The full set of features the drive is capable of are currently
	not implemented but will be added in upcoming releases.
-----------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------
	matcd_playtracks - Plays one or more audio tracks
-----------------------------------------------------------------------------*/

static int matcd_playtracks(int ldrive, int cdrive, int controller,
			    struct ioc_play_track *pt)
{
	struct	matcd_data *cd;
	int	start,end;
	int	i,port;
	unsigned char cmd[MAXCMDSIZ];

	cd=&matcd_data[ldrive];
	port=cd->iobase;

	if ((cd->flags & MATCDLABEL)==0)
		return(EIO);		/*Refuse after chg error*/

	start=pt->start_track;
	end=pt->end_track;

	if (start < 1 ||		/*Starting track valid?*/
	    end < 1 ||			/*Ending track valid?*/
	    start > end || 		/*Start higher than end?*/
	    end > cd->volinfo.trk_high)	/*End track higher than disc size?*/
		return(ESPIPE);		/*Track out of range*/

	lockbus(controller, ldrive);	/*<16>Request bus*/
	i=matcd_setmode(ldrive, MODE_DA);/*Force drive into audio mode*/
	unlockbus(controller, ldrive);	/*<16>Release bus*/
	if (i!=0) {
		return(i);		/*Not legal for this media?*/
	}
	zero_cmd(cmd);
	cmd[0]=PLAYTRKS;		/*Play Audio Track/Index*/
	cmd[1]=start;
	cmd[2]=pt->start_index;
	cmd[3]=end;
	cmd[4]=pt->end_index;
	i=docmd(cmd,ldrive,cdrive,controller,port);	/*Issue command*/
#ifdef DEBUGIOCTL
	printf("matcd%d: Play track results %d \n",ldrive,i);
#endif /*DEBUGIOCTL*/
	if (i==0) cd->status=CD_AS_PLAY_IN_PROGRESS;	/*<14>*/
	return(i);
}


/*-----------------------------------------------------------------------------
	matcd_playmsf - Plays between a range of blocks
-----------------------------------------------------------------------------*/

static int matcd_playmsf(int ldrive, int cdrive, int controller,
			    struct ioc_play_msf *pt)
{
	struct matcd_data *cd;
	int	i,port;
	unsigned char cmd[MAXCMDSIZ];

	cd=&matcd_data[ldrive];
	port=cd->iobase;

#ifdef DEBUGIOCTL
	printf("matcd%d: playmsf %2x %2x %2x -> %2x %2x %2x\n",
	       ldrive,pt->start_m, pt->start_s, pt->start_f, pt->end_m,
	       pt->end_s,pt->end_f);
#endif /*DEBUGIOCTL*/

	if ((cd->flags & MATCDLABEL)==0)
		return(EIO);		/*Refuse after chg error*/

	if ((cd->volinfo.vol_msf[0]==0 &&
	     cd->volinfo.vol_msf[1]<2) ||	/*Must be after 0'1"75F*/
	     msf_to_blk((char *)&pt->start_m) >
	     msf_to_blk((char *)&cd->volinfo.vol_msf)) {
#ifdef DEBUGIOCTL
	printf("matcd%d: Invalid block combination\n",ldrive);
#endif /*DEBUGIOCTL*/
		return(ESPIPE);		/*Track out of range*/
	}


	lockbus(controller, ldrive);	/*<16>Request bus*/
	i=matcd_setmode(ldrive, MODE_DA);/*Force drive into audio mode*/
	unlockbus(controller, ldrive);	/*<16>Release bus*/
	if (i!=0) {
		return(i);		/*Not legal for this media?*/
	}
	zero_cmd(cmd);
	cmd[0]=PLAYBLOCKS;		/*Play Audio Blocks*/
	cmd[1]=pt->start_m;
	cmd[2]=pt->start_s;
	cmd[3]=pt->start_f;
	cmd[4]=pt->end_m;
	cmd[5]=pt->end_s;
	cmd[6]=pt->end_f;
	i=docmd(cmd,ldrive,cdrive,controller,port);	/*Issue command*/
	if (i==0) cd->status=CD_AS_PLAY_IN_PROGRESS;	/*<14>*/
	return(i);
}


/*-----------------------------------------------------------------------------
	matcd_playblk - Plays between a range of blocks
-----------------------------------------------------------------------------*/

static int matcd_playblk(int ldrive, int cdrive, int controller,
			    struct ioc_play_blocks *pb)
{
	struct matcd_data *cd;
	int	i,port;
	unsigned char cmd[MAXCMDSIZ];
	unsigned char blkstart[3];
	unsigned char blkend[3];

	cd=&matcd_data[ldrive];
	port=cd->iobase;

#ifdef DEBUGIOCTL
	printf("matcd%d: playblocks %x %d\n", ldrive,pb->blk, pb->len);
#endif /*DEBUGIOCTL*/

	if ((cd->flags & MATCDLABEL)==0)
		return(EIO);		/*Refuse after chg error*/

	i=msf_to_blk((char *)&cd->volinfo.vol_msf);
	if (pb->len <1 ||			/*Must ask for 1 or more blks*/
	    pb->blk <0 ||			/*Must not be negative*/
	    pb->blk > i ||			/*Beyond end of disc*/
	    (pb->blk+pb->len) > i) {		/*Beyond end of disc*/
#ifdef DEBUGIOCTL
	printf("matcd%d: Block request out of range\n",ldrive);
#endif /*DEBUGIOCTL*/
		return(ESPIPE);		/*Track out of range*/
	}
	blk_to_msf(pb->blk,(char *)&blkstart);
	blk_to_msf(pb->blk+pb->len,(char *)&blkend);

	lockbus(controller, ldrive);	/*<16>Request bus*/
	i=matcd_setmode(ldrive, MODE_DA);/*Force drive into audio mode*/
	unlockbus(controller, ldrive);	/*<16>Release bus*/
	if (i!=0) {
		return(i);		/*Not legal for this media?*/
	}
	zero_cmd(cmd);
	cmd[0]=PLAYBLOCKS;		/*Play Audio Blocks*/
	cmd[1]=blkstart[0];
	cmd[2]=blkstart[1];
	cmd[3]=blkstart[2];
	cmd[4]=blkend[0];
	cmd[5]=blkend[1];
	cmd[6]=blkend[2];
	i=docmd(cmd,ldrive,cdrive,controller,port);	/*Issue command*/
	if (i==0) cd->status=CD_AS_PLAY_IN_PROGRESS;	/*<14>*/
	return(i);
}


/*-----------------------------------------------------------------------------
	matcd_pause - Pause or Resume audio playback
-----------------------------------------------------------------------------*/

static int matcd_pause(int ldrive, int cdrive, int controller, int action)
{
	struct	matcd_data *cd;
	int	i,z,port;
	unsigned char cmd[MAXCMDSIZ];

	cd=&matcd_data[ldrive];
	port=cd->iobase;

	if ((cd->flags & MATCDLABEL)==0)
		return(EIO);		/*Refuse after chg error*/

	zero_cmd(cmd);
	cmd[0]=NOP;			/*<14>Just find out whats going on*/

	lockbus(controller, ldrive);	/*<16>Request bus*/
	matcd_slowcmd(port,ldrive,cdrive,cmd);	/*<14>*/
	i=waitforit(10*TICKRES,DTEN,port,"matpau");	/*<25>*/
	z=get_stat(port,ldrive);	/*<14>Read status byte*/
	if ((z & MATCD_ST_ERROR)) {	/*<14>Something went wrong*/
		i=get_error(port, ldrive, cdrive);	/*<14>*/
		unlockbus(controller, ldrive);	/*<16>Release bus*/
		return(EIO);		/*<14>*/
	}				/*<14>*/
	unlockbus(controller, ldrive);	/*<16>Release bus*/

	if ((z & MATCD_ST_AUDIOBSY)==0 &&	/*<14>If drive is idle*/
	    cd->status==CD_AS_PLAY_IN_PROGRESS) {	/*<14>but was playing*/
		cd->status=CD_AS_PLAY_COMPLETED;	/*<14>then its done*/
		return(0);
	}

	if (action) {			/*<14>Set state for subq ioctl*/
#ifndef KRYTEN
		if (cd->status==CD_AS_PLAY_IN_PROGRESS) {/*<14>Don't resume*/
			return(0);	/*<14>if already playing*/
		}			/*<14>Max Headroom sound occurs*/
#endif /*KRYTEN*/
		cd->status=CD_AS_PLAY_IN_PROGRESS;	/*<14>to read*/
	} else {			/*<14>There is no way to ask the*/
		cd->status=CD_AS_PLAY_PAUSED;/*<14>drive if it is paused*/
	}				/*<14>*/

	cmd[0]=PAUSE;			/*Pause or Resume playing audio*/
	cmd[1]=action;
	i=docmd(cmd,ldrive,cdrive,controller,port);	/*Issue command*/
#ifdef DEBUGIOCTL
	printf("matcd%d: Pause / Resume results %d \n",ldrive,i);
#endif /*DEBUGIOCTL*/
	return(i);
}



/*-----------------------------------------------------------------------------
	matcd_stop  - Stop audio playback
-----------------------------------------------------------------------------*/

static int matcd_stop(int ldrive, int cdrive, int controller)
{
	struct	matcd_data *cd;
	int	i,port;
	unsigned char cmd[MAXCMDSIZ];

	cd=&matcd_data[ldrive];
	port=cd->iobase;

	if ((cd->flags & MATCDLABEL)==0)
		return(EIO);		/*Refuse after chg error*/

	zero_cmd(cmd);
	cmd[0]=ABORT;			/*Abort playing audio*/
	i=docmd(cmd,ldrive,cdrive,controller,port);	/*Issue command*/
#ifdef DEBUGIOCTL
	printf("matcd%d: Abort results %d \n",ldrive,i);
#endif /*DEBUGIOCTL*/
	cd->status=CD_AS_PLAY_COMPLETED;/*<14>the drive if it is paused*/
	return(i);
}


/*-----------------------------------------------------------------------------
	matcd_level - Read or set the audio levels
<12>	New for Edit 12
-----------------------------------------------------------------------------*/

static int matcd_level(int ldrive, int cdrive, int controller,
		       struct ioc_vol * level, int action)
{
	struct	matcd_data *cd;
	int	i,z,port;
	unsigned char c;
	unsigned char cmd[MAXCMDSIZ];
	unsigned char data[5];

	cd=&matcd_data[ldrive];
	port=cd->iobase;

	zero_cmd(cmd);
	if (action==CDIOCSETVOL) {	/*We are setting new volume settings*/

/*	Here we set the volume levels.  Note that the same command
	also sets the patching (routing) of audio, so we have to rely
	on previously-stored settings to fill in these fields.
*/
		cmd[0]=MODESELECT;	/*Write drive settings*/
		cmd[1]=AUDIOPARM;	/*Audio/routing settings*/

/*	Although the drive allows a left and right channel volume to be
	specified separately, the drive refuses the settings if the
	values are different.
*/
		c=level->vol[0] | level->vol[1];	/*Or them together*/

		cmd[4]=cd->volume[0]=c;	/*Channel 0 (Left) volume*/
		cmd[6]=cd->volume[1]=c;	/*Channel 1 (Right) volume*/
		cmd[3]=cd->patch[0];	/*Channel 0 (Left)  patching*/
		cmd[5]=cd->patch[1];	/*Channel 1 (Right)  patching*/
		i=docmd(cmd,ldrive,cdrive,controller,port);/*Issue cmd*/
#ifdef DEBUGIOCTL
		printf("matcd%d: Volume set %d\n",ldrive,i);
#endif /*DEBUGIOCTL*/
		return(i);
	} else {			/*Read existing settings*/


/*	This code reads the settings for the drive back - note that
	volume and patching are both returned so we have to keep
	both internally.
*/
		cmd[0]=MODESENSE;	/*Read drive settings*/
		cmd[1]=AUDIOPARM;	/*Audio/routing settings*/
		lockbus(controller, ldrive);	/*<16>Request bus*/
		matcd_slowcmd(port,ldrive,cdrive,cmd);
		i=waitforit(10*TICKRES,DTEN,port,"matlvl");	/*<25>*/
		matcd_pread(port, 5, data);	/*Read data returned*/
		z=get_stat(port,ldrive);/*Read status byte*/
		unlockbus(controller, ldrive);	/*<16>Release bus*/
#ifdef DEBUGIOCTL
		printf("matcd%d: Data got was %x %x %x %x %x   ",ldrive,
		       data[0],data[1],data[2], data[3],data[4]);
		printf("status byte %x\n",z);
#endif	/*DEBUGIOCTL*/
		cd->volume[0]=level->vol[0]=	/*Channel 0 (Left) volume*/
			      data[2];
		cd->volume[1]=level->vol[1]=	/*Channel 1 (Right) volume*/
			      data[4];
		level->vol[2]=level->vol[3]=0;	/*Channel 2 & 3 not avail*/

		cd->patch[0]=data[1];	/*Channel 0 (Left) patching*/
		cd->patch[1]=data[3];	/*Channel 1 (Right) patching*/

		return(0);
	}

}


/*-----------------------------------------------------------------------------
	matcd_routing - Set the audio routing (patching)
<12>	New for Edit 12
-----------------------------------------------------------------------------*/

static int matcd_route(int ldrive, int cdrive, int controller,
		       int command)
{
	struct	matcd_data *cd;
	int	i,port;
	unsigned char l,r;
	unsigned char cmd[MAXCMDSIZ];

	cd=&matcd_data[ldrive];
	port=cd->iobase;

	zero_cmd(cmd);
	switch (command) {
	case	CDIOCSETMUTE:
		l=r=0;
		break;

	case	CDIOCSETLEFT:
		l=r=OUTLEFT;
		break;

	case	CDIOCSETRIGHT:
		l=r=OUTRIGHT;
		break;

	default:
	case	CDIOCSETSTEREO:
		l=OUTLEFT;
		r=OUTRIGHT;
		break;
	}

/*	Here we set the volume levels.  Note that the same command
	also sets the patching (routing) of audio, so we have to rely
	on previously-stored settings to fill in these fields.
*/
	cmd[0]=MODESELECT;		/*Write drive settings*/
	cmd[1]=AUDIOPARM;		/*Audio/routing settings*/


/*	Although the drive allows a left and right channel volume to be
	specified separately, the drive refuses the settings if the
	values are different.
*/
	cmd[4]=cd->volume[0];		/*Channel 0 (Left) volume*/
	cmd[6]=cd->volume[1];		/*Channel 1 (Right) volume*/
	cmd[3]=cd->patch[0]=l;		/*Channel 0 (Left)  patching*/
	cmd[5]=cd->patch[1]=r;		/*Channel 1 (Right)  patching*/
	i=docmd(cmd,ldrive,cdrive,controller,port);/*Issue cmd*/
#ifdef DEBUGIOCTL
	printf("matcd%d: Routing set %d\n",ldrive,i);
#endif /*DEBUGIOCTL*/
	return(i);

}


/*-----------------------------------------------------------------------------
	matcd_patch - Set the audio routing (patching)
<12>	New for Edit 12
-----------------------------------------------------------------------------*/

static int matcd_patch(int ldrive, int cdrive, int controller,
		       struct ioc_patch * routing)
{
	struct	matcd_data *cd;
	int	i,port;
	unsigned char cmd[MAXCMDSIZ];

	cd=&matcd_data[ldrive];
	port=cd->iobase;

	zero_cmd(cmd);

/*	Here we set the volume levels.  Note that the same command
	also sets the patching (routing) of audio, so we have to rely
	on previously-stored settings to fill in these fields.
*/
	cmd[0]=MODESELECT;		/*Write drive settings*/
	cmd[1]=AUDIOPARM;		/*Audio/routing settings*/


/*	Although the drive allows a left and right channel volume to be
	specified separately, the drive refuses the settings if the
	values are different.
*/
	cmd[4]=cd->volume[0];		/*Channel 0 (Left) volume*/
	cmd[6]=cd->volume[1];		/*Channel 1 (Right) volume*/
	cmd[3]=cd->patch[0]=		/*Channel 0 (Left)  patching*/
	       (routing->patch[0] & 0x03);
	cmd[5]=cd->patch[1]=		/*Channel 1 (Right)  patching*/
	       (routing->patch[1] & 0x03);
	i=docmd(cmd,ldrive,cdrive,controller,port);/*Issue cmd*/
#ifdef DEBUGIOCTL
	printf("matcd%d: Routing set %d\n",ldrive,i);
#endif /*DEBUGIOCTL*/
	return(i);

}

/*-----------------------------------------------------------------------------
	matcd_pitch - Change audio playback rate
		      Apart from making things sound funny, the only
		      other application might be Karaoke.  Ugh.
<12>	New for Edit 12
-----------------------------------------------------------------------------*/

static int matcd_pitch(int ldrive, int cdrive, int controller,
		       struct ioc_pitch * speed)
{
	struct	matcd_data *cd;
	short	i;
	int	z,port;
	unsigned char cmd[MAXCMDSIZ];

	cd=&matcd_data[ldrive];
	port=cd->iobase;

	zero_cmd(cmd);

/*	This function sets the audio playback rate.  In SCSI devices this is
	referred to as the logical block addresses per second parameter.
	Uh huh.  Sounds like they didn't want anyone to find it.
	Anyway, a study found that no one else has implemented this ioctl
	but the capability does exist in the SCSI standard so I am following
	the SCSI scheme even though it really doesn't fit this drive well.

	I originally defined the parameter to this ioctl as -32767 to -1
	being "play slower", 0x0000 flat and 1 to 32767 being "play faster"
	within the scale allowed by the device.  The value is scaled to fit
	the range allowed by the device and any excess is treated as being
	the positive or negative limit.  No ioctl input pitch value is
	considered invalid.

	This device has a +/- 13% playback pitch specified by a range
	-130 to +130.  The drive does a hard enforcement on this.

	SCSI defines a 16 bit LBAS count, and a "multiplier" that
	is either x1 or x(1/256).  The Matsushita drive only provides
	10 bits total for indicating pitch so the LSbits are discarded.
*/

	cmd[0]=MODESELECT;		/*Write drive settings*/
	cmd[1]=SPEEDPARM;		/*Audio speed settings*/

	i=speed->speed>>7;		/*Scale down to our usable range*/

	if (i!=0) {			/*Real pitch value*/
		if (i < -130) i=-130;	/*Force into range we support*/
		else if (i > 130) i=130;
		cmd[3]=((i>>8)&0x03) | 0x04;	/*Get upper bits*/
		cmd[4]=(i & 0xff);	/*Set lower bits*/
	}
	z=docmd(cmd,ldrive,cdrive,controller,port);/*Issue cmd*/
#ifdef DEBUGIOCTL
	printf("matcd%d: Pitch set %d\n",ldrive,i);
#endif /*DEBUGIOCTL*/
	return(z);
}

#endif	/*FULLDRIVER*/

/*End of matcd.c*/

