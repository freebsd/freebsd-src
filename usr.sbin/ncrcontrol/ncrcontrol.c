/**************************************************************************
**
**  $Id: ncrcontrol.c,v 1.19 1997/10/02 11:46:53 charnier Exp $
**
**  Utility for NCR 53C810 device driver.
**
**  386bsd / FreeBSD / NetBSD
**
**-------------------------------------------------------------------------
**
**  Written for 386bsd and FreeBSD by
**	wolf@dentaro.gun.de	Wolfgang Stanglmeier
**	se@mi.Uni-Koeln.de	Stefan Esser
**
**  Ported to NetBSD by
**	mycroft@gnu.ai.mit.edu
**
**-------------------------------------------------------------------------
**
** Copyright (c) 1994 Wolfgang Stanglmeier.  All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
**
***************************************************************************
*/

#include <sys/file.h>
#include <sys/types.h>
#ifdef __NetBSD__
#include <sys/device.h>
#endif
#include <err.h>
#include <errno.h>
#include <kvm.h>
#include <limits.h>
#include <nlist.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pci/ncr.c>

/*
**	used external functions
*/

#if defined(__NetBSD__) || (__FreeBSD__ >= 2)
kvm_t	*kvm;
#define	KVM_NLIST(n)		(kvm_nlist(kvm, (n)) >= 0)
#define	KVM_READ(o, p, l)	(kvm_read(kvm, (o), (void*)(p), (l)) == (l))
#else
#define	KVM_NLIST(n)		(kvm_nlist((n)) >= 0)
#define	KVM_READ(o, p, l)	(kvm_read((void*)(o), (p), (l)) == (l))
#endif


/*===========================================================
**
**	Global variables.
**
**===========================================================
*/

u_long	verbose;
u_long  wizard;



struct nlist nl[] = {
#define	N_NCR_VERSION	0
	{ "_ncr_version" },
#ifdef __NetBSD__
#define	N_NCRCD	1
	{ "_ncrcd" },
#else
#define	N_NCRP	1
	{ "_ncrp" },
#define	N_NNCR	2
	{ "_nncr" },
#endif
	{ 0 }
};


const char *vmunix = NULL;
char	*kmemf  = NULL;

int	kvm_isopen;

u_long  ncr_base;
u_long	lcb_base;
u_long	ccb_base;

u_long  ncr_unit;
#ifdef __NetBSD__
struct	cfdriver ncrcd;
#else
u_long	ncr_units;
#endif

struct	ncb ncr;
struct	lcb lcb;
struct	ccb ccb;

u_long  target_mask;
u_long  global_lun_mask;
u_long  lun_mask;
u_long  interval;

static void usage __P((void));


/*===========================================================
**
**	Accessing kernel memory via kvm library.
**
**===========================================================
*/

void read_ccb(u_long base)
{
	ccb_base = base;
	if (!KVM_READ (
		base,
		&ccb,
		sizeof (struct ccb)))
		errx(1, "bad kvm read at %x", base);
}

void read_lcb(u_long base)
{
	lcb_base = base;
	if (!KVM_READ (
		base,
		&lcb,
		sizeof (struct lcb)))
		errx(1, "bad kvm read at %x", base);
}

void read_ncr()
{
	if (!KVM_READ (
		ncr_base,
		&ncr,
		sizeof (ncr)))
		errx(1, "bad kvm read at %x", ncr_base);
}

void open_kvm(int flags)
{
	int i;
	u_long	kernel_version;
#if defined(__NetBSD__) || (__FreeBSD__ >= 2)
	char 	errbuf[_POSIX2_LINE_MAX];
#endif

	if (kvm_isopen) return;

#if defined(__NetBSD__) || (__FreeBSD__ >= 2)
	/*
	 * Discard setgid privileges if not the running kernel so that bad
	 * guys can't print interesting stuff from kernel memory.
	 */
	if (vmunix != NULL || kmemf != NULL)
		setgid(getgid());
	else {
#if (__FreeBSD__ >= 2)
		vmunix = getbootfile();
#else
		vmunix = _PATH_UNIX;
#endif
	}

	kvm = kvm_openfiles(vmunix, kmemf, NULL, flags, errbuf);
	if (kvm == NULL)
		errx(1, "kvm_openfiles: %s", errbuf);
#else
	if (vmunix != NULL) {
#if (__FreeBSD__ >= 2)
		vmunix = getbootfile();
#else
		vmunix = _PATH_UNIX;
#endif
	}
	if (kvm_openfiles(vmunix, kmemf, NULL) == -1)
		errx(1, "kvm_openfiles: %s", kvm_geterr());
#endif

	if (!KVM_NLIST(nl))
		errx(2, "no symbols in \"%s\"", vmunix);

	for (i=0; nl[i].n_name; i++)
		if (nl[i].n_type == 0)
			errx(1, "no symbol \"%s\" in \"%s\"",
				nl[i].n_name, vmunix);

	if (!KVM_READ (
		nl[N_NCR_VERSION].n_value,
		&kernel_version,
		sizeof (kernel_version)))
		errx(1, "bad kvm read");

	if (kernel_version != ncr_version)
		errx(1, "incompatible with kernel. Rebuild!");

#ifdef __NetBSD__

	if (!KVM_READ (
		nl[N_NCRCD].n_value,
		&ncrcd,
		sizeof (ncrcd)))
		errx(1, "bad kvm read");

	if (ncr_unit >= ncrcd.cd_ndevs)
		errx(1, "bad unit number (valid range: 0-%d)",
			ncrcd.cd_ndevs-1);

	if (!KVM_READ (
		ncrcd.cd_devs+4*ncr_unit,
		&ncr_base,
		sizeof (ncr_base)))
		errx(1, "bad kvm read");

	if (!ncr_base)
		errx(1,
		"control structure not allocated (not found in autoconfig?)");

#else /* !__NetBSD__ */

	if (!KVM_READ (
		nl[N_NNCR].n_value,
		&ncr_units,
		sizeof (ncr_units)))
		errx(1, "bad kvm read");

	if (ncr_unit >= ncr_units)
		errx(1, "bad unit number (valid range: 0-%d)",
			ncr_units-1);

	if (!KVM_READ (
		nl[N_NCRP].n_value+4*ncr_unit,
		&ncr_base,
		sizeof (ncr_base))) {
		errx(1, "bad kvm read");
	};

	if (!ncr_base)
		errx(1,
		"control structure not allocated (not found in autoconfig?)");

#endif /* !__NetBSD__ */

	read_ncr();

	if (!ncr.vaddr)
		errx(1, "53c810 not mapped (not found in autoconfig?)");

	kvm_isopen = 1;
}




void set_target_mask(void)
{
	int t;
	if (target_mask) return;
	for (t=0; t<MAX_TARGET; t++)
		if (ncr.target[t].jump_tcb.l_cmd) target_mask |= (1<<t);
}

void set_lun_mask(struct tcb * tp)
{
	int l;
	lun_mask = global_lun_mask;
	if (lun_mask) return;
	for (l=0; l<MAX_LUN; l++)
		if (tp->lp[l]) lun_mask |= (1<<l);
}

void printc (u_char*p, int l)
{
	for (;l>0;l--) {
		char c=*p++;
		printf ("%c", c?c:'_');
	}
}

/*================================================================
**
**
**	system info
**
**
**================================================================
*/

static double syncmhz(int negoval)
{
	switch (negoval) {
	case 0:
		return 0.0;
	case 10:
		return 40.0;
	case 11:
		return 33.0;
	case 12:
		return 20.0;
	}
	return 250.0 / negoval;
}

void do_info(void)
{
	int t,l,i,d,f,fl;
	struct tcb * tip;
	open_kvm(O_RDONLY);

	if (verbose>=3)
	printf ("ncr unit=%d  data@%x  register@%x  (pci@%x)\n\n",
		ncr_unit, ncr_base, ncr.vaddr, ncr.paddr);

	set_target_mask();

	printf ("T:L  Vendor   Device           Rev  Speed   Max Wide Tags\n");
	for (t=0; t<MAX_TARGET;t++) {
		if (!((target_mask>>t)&1)) continue;
		tip = &ncr.target[t];

		set_lun_mask(tip);
		if (!lun_mask) lun_mask=1;
		fl=1;

		for (l=0; l<MAX_LUN; l++) {
			if (!((lun_mask>>l)&1)) continue;

			printf ("%d:%d  ", t, l);

			if (!tip->jump_tcb.l_cmd) break;

			if (fl) {
				fl=0;
				printc (&tip->inqdata[ 8], 8);printf(" ");
				printc (&tip->inqdata[16],16);printf(" ");
				printc (&tip->inqdata[32], 4);printf("  ");

				if (tip->period==0xffff) {
					printf ("asyn");
				} else if (tip->period) {
					printf ("%4.1f", 10000.0 / tip->period);
				} else {
					printf ("   ?");
				}

				printf ("  ");

				if (tip->minsync==255) {
					printf ("asyn");
				} else if (tip->minsync) {
					printf ("%4.1f", syncmhz(tip->minsync));
				} else {
					printf ("   ?");
				}
			} else printf ("%42s", "");

			if (!tip->lp[l]) {
				printf ("   no\n");
				continue;
			};
			read_lcb ((u_long) tip->lp[l]);

			switch (tip->widedone) {
			case 1:
				printf ("   8");
				break;
			case 2:
				printf ("  16");
				break;
			case 3:
				printf ("  32");
				break;
			default:
				printf ("   ?");
			};

			if (lcb.usetags)
				printf ("%5d", lcb.actlink);
			else
				printf ("    -");

			printf ("\n");

		};

		if (!tip->jump_tcb.l_cmd) {
			printf (" --- no target.\n");
			continue;
		};

		if (verbose<1) continue;

		for (i=0; i<8; i++) {
			char* (class[10])={
			"disk","tape","printer","processor",
			"worm", "cdrom", "scanner", "optical disk",
			"media changer", "communication device"};
			d = tip->inqdata[i];
			printf ("[%02x]: ",d);

                        switch (i) {

			case 0:
				f = d & 0x1f;
				if (f<10) printf (class[f]);
				else	printf ("unknown (%x)", f);
				break;
			case 1:
				f = (d>>7) & 1;
				if (f) printf ("removable media");
				else	printf ("fixed media");
				break;

			case 2:
				f = d & 7;
				switch (f) {
				case 0:	printf ("SCSI-1");
					break;
				case 1: printf ("SCSI-1 with CCS");
					break;
				case 2:	printf ("SCSI-2");
					break;
				default:
					printf ("unknown ansi version (%d)",
						f);
				}
				break;

			case 3:
				if (d&0xc0) printf ("capabilities:");
				if (d&0x80) printf (" AEN");
				if (d&0x40) printf (" TERMINATE-I/O");
				break;

			case 7:
				if (d&0xfb) printf ("capabilities:");
				if (d&0x80) printf (" relative");
				if (d&0x40) printf (" wide32");
				if (d&0x20) printf (" wide");
				if (d&0x10) printf (" synch");
				if (d&0x08) printf (" link");
				if (d&0x02) printf (" tags");
				if (d&0x01) printf (" soft-reset");
			};
			printf ("\n");
		};
		printf ("\n");
	};
	printf ("\n");
}

/*================================================================
**
**
**	profiling
**
**
**================================================================
*/

void do_profile(void)
{
#define old  backup.profile
#define new  ncr.profile

	struct ncb backup;
	struct profile diff;
	int tra,line,t;

	open_kvm(O_RDONLY);

	set_target_mask();

	if (interval<1) interval=1;
	for (;;) {
		/*
		**	Header Line 1
		*/
		printf ("  total  ");

		for (t=0; t<MAX_TARGET; t++) {
			if (!((target_mask>>t)&1)) continue;
			printf (" ");
			printc (&ncr.target[t].inqdata[16],8);
		};

		printf (" transf.  disconn interru");

		if (verbose>=1) printf ("  ---- ms/transfer ----");

		printf ("\n");

		/*
		**	Header Line 2
		*/

		printf ("t/s kb/s ");

		for (t=0; t<MAX_TARGET; t++) {
			if (!((target_mask>>t)&1)) continue;
			printf (" t/s kb/s");
		};

		printf (" length   exp une fly brk");

		if (verbose>=1) printf ("  total  pre post  disc");

		printf ("\n");

		/*
		**	Data
		*/

		for(line=0;line<20;line++) {
			backup = ncr;
			read_ncr();
			diff.num_trans	= new.num_trans - old.num_trans;
			diff.num_bytes	= new.num_bytes - old.num_bytes;
			diff.num_fly    = new.num_fly   - old.num_fly  ;
			diff.num_int    = new.num_int   - old.num_int  ;
			diff.ms_setup	= new.ms_setup - old.ms_setup;
			diff.ms_data	= new.ms_data - old.ms_data;
			diff.ms_disc	= new.ms_disc - old.ms_disc;
			diff.ms_post	= new.ms_post - old.ms_post;
			diff.num_disc	= new.num_disc - old.num_disc;
			diff.num_break	= new.num_break - old.num_break;

			tra = diff.num_trans;
			if (!tra) tra=1;

			printf ("%3.0f %4.0f ",
				(1.0 * diff.num_trans) / interval,
				(1.0 * diff.num_bytes) / (1024*interval));


			for (t=0; t<MAX_TARGET; t++) {
				if (!((target_mask>>t)&1)) continue;
				printf (" %3.0f %4.0f",
					((ncr.target[t].transfers-
					backup.target[t].transfers)*1.0)
					/interval,
					((ncr.target[t].bytes-
					backup.target[t].bytes)*1.0)
					/(1024*interval));
			};

			printf ("%7.0f ", (diff.num_bytes*1.0) / tra);

			printf (" %4.0f", (1.0*(diff.num_disc-diff.num_break))
					/interval);

			printf ("%4.0f", (1.0*diff.num_break)/interval);

			printf ("%4.0f", (1.0*diff.num_fly) / interval);

			printf ("%4.0f", (1.0*diff.num_int) / interval);

				if (verbose >= 1) {
				printf ("%7.1f",
					(diff.ms_disc+diff.ms_data+diff.ms_setup+diff.ms_post)
					* 1.0  / tra);

					printf ("%5.1f%5.1f%6.1f",
					1.0 * diff.ms_setup / tra,
					1.0 * diff.ms_post  / tra,
					1.0 * diff.ms_disc  / tra);
			};

			printf ("\n");
			fflush (stdout);
			sleep (interval);
		};
	};
}

/*================================================================
**
**
**	Port access
**
**
**================================================================
*/

static	int kernelwritefile;
static	char* kernelwritefilename = _PATH_KMEM;

void openkernelwritefile(void)
{
	if (kernelwritefile) return;

	kernelwritefile = open (kernelwritefilename, O_WRONLY);
	if (kernelwritefile<3)
		err(1, "%s", kernelwritefilename);
}

void out (u_char reg, u_char val)
{
	u_long addr = ncr.vaddr + reg;
	openkernelwritefile();
	if (lseek (kernelwritefile, addr, 0) != addr)
		err(1, "%s", kernelwritefilename);
	if (write (kernelwritefile, &val, 1) < 0)
		err(1, "%s", kernelwritefilename);
}

u_char in (u_char reg)
{
	u_char res;
	if (!KVM_READ (
		(ncr.vaddr + reg),
		&res,
		1))
		errx(1, "bad kvm read");
	return (res);
}

/*================================================================
**
**
**	Setting of driver parameters
**
**
**================================================================
*/

void do_set (char * arg)
{
	struct usrcmd user;
	u_long addr;
	int i;

	if (!strcmp(arg, "?")) { printf (
"async:         disable synchronous transfers.\n"
"sync=value:    set the maximal synchronous transfer rate (MHz).\n"
"fast:          set FAST SCSI-2.\n"
"\n"
"wide=value:    set the bus width (0=8bit 1=16bit).\n"
"\n"
"tags=value:    use this number of tags.\n"
"orderedtag:    use ordered tags only.\n"
"simpletag:     use simple tags only.\n"
"orderedwrite:  use simple tags for read, else ordered tags.\n"
"\n"
"debug=value:   set debug mode.\n"
"\n");
		return;
	};

	open_kvm(O_RDWR);
	addr = ncr_base + offsetof (struct ncb, user);

	for (i=3; i; i--) {
		if (!KVM_READ (
			(addr),
			&user,
			sizeof (user)))
			errx(1, "bad kvm read");
		if (!user.cmd) break;
		sleep (1);
	}
	if (user.cmd)
		errx(1, "ncb.user busy");

	set_target_mask();

	user.target = target_mask;
	user.lun    = lun_mask;
	user.data   = 0;
	user.cmd    = 0;


	if (!strcmp(arg, "async")) {
		user.data = 255;
		user.cmd  = UC_SETSYNC;
	};

	if (!strcmp(arg, "fast")) {
		user.data = 25;
		user.cmd  = UC_SETSYNC;
	};

	if (!strncmp(arg, "sync=", 5)) {
		double f = strtod (arg+5, NULL);
		if (f>=4.0 && f<=10.0) {
			user.data = 250.0 / f;
			user.cmd  = UC_SETSYNC;
		};
	};

	if (!strncmp(arg, "wide=", 5)) {
		u_char t = strtoul (arg+5, (char**)0, 0);
		if (t<=1) {
			user.data = t;
			user.cmd  = UC_SETWIDE;
		};
	};

	if (!strncmp(arg, "tags=", 5)) {
		u_char t = strtoul (arg+5, (char**)0, 0);
		if (t<=MAX_TAGS) {
			user.data = t;
			user.cmd  = UC_SETTAGS;
		};
	};

	if (!strncmp(arg, "flags=", 6)) {
		u_char t = strtoul (arg+6, (char**)0, 0);
		if (t<=0xff) {
			user.data = t;
			user.cmd  = UC_SETFLAG;
		};
	};

	if (!strncmp(arg, "debug=", 6)) {
		user.data = strtoul (arg+6, (char**)0, 0);
		user.cmd  = UC_SETDEBUG;
	};

	if (!strcmp(arg, "orderedtag")) {
		user.data = M_ORDERED_TAG;
		user.cmd  = UC_SETORDER;
	};

	if (!strcmp(arg, "simpletag")) {
		user.data = M_SIMPLE_TAG;
		user.cmd  = UC_SETORDER;
	};

	if (!strcmp(arg, "orderedwrite")) {
		user.data = 0;
		user.cmd  = UC_SETORDER;
	};

	if (user.cmd) {
		openkernelwritefile();

		if (lseek (kernelwritefile, addr, 0) != addr)
			err(1, "%s", kernelwritefilename);
		if (write (kernelwritefile, &user, sizeof (user)) < 0)
			errx(1, "%s", kernelwritefilename);

		return;
	};

	warnx ("do_set \"%s\" not (yet) implemented", arg);
}

/*================================================================
**
**
**	D O _ K I L L
**
**
**================================================================
*/

void do_kill(char * arg)
{
	open_kvm(O_RDWR);

	if (!strcmp(arg, "?")) { printf (
"scsireset:     force SCSI bus reset.\n"
"scriptabort:   send an abort cmd to the script processor.\n"
"scriptstart:   start script processind (set SIGP bit).\n"
"evenparity:    force even parity.\n"
"oddparity:     force odd parity.\n"
"noreselect:    disable reselect (force timeouts).\n"
"doreselect:    enable reselect.\n"
"\n");
		return;
	};

	if (!wizard)
		errx(2, "you are NOT a wizard!");

	if (!strcmp(arg, "scsireset")) {
		out (0x01, 0x08);
		out (0x01, 0x00);
		return;
	};
	if (!strcmp(arg, "scriptabort")) {
		out (0x14, 0x80);
		out (0x14, 0x20);
		return;
	};
	if (!strcmp(arg, "scriptstart")) {
		out (0x14, 0x20);
		return;
	};
	if (!strcmp(arg, "evenparity")) {
		out (0x01, 0x04);
		return;
	};
	if (!strcmp(arg, "oddparity")) {
		out (0x01, 0x00);
		return;
	};
	if (!strcmp(arg, "noreselect")) {
		out (0x04, in (0x04) & ~RRE);
		return;
        };
	if (!strcmp(arg, "doreselect")) {
		out (0x04, in (0x04) | RRE);
		return;
        };
        warnx ("do_kill \"%s\" not (yet) implemented", arg);
}

/*================================================================
**
**
**	Write debug info: utilities:     write symbolname.
**
**
**================================================================
*/

static const char * sn (u_long a)
{
	static	char buffer[100];

	const char * s="";
	u_long d,m;

	a -= ncr.p_script;
	m = sizeof (struct script);

	if ((d=a-offsetof(struct script, start))<m) m=d, s="<start>";
	if ((d=a-offsetof(struct script, start1))<m) m=d, s="<start1>";
	if ((d=a-offsetof(struct script, startpos))<m) m=d, s="<startpos>";
	if ((d=a-offsetof(struct scripth, tryloop))<m) m=d, s="<tryloop>";
	if ((d=a-offsetof(struct script, trysel))<m) m=d, s="<trysel>";
	if ((d=a-offsetof(struct script, skip))<m) m=d, s="<skip>";
	if ((d=a-offsetof(struct script, skip2))<m) m=d, s="<skip2>";
	if ((d=a-offsetof(struct script, idle))<m) m=d, s="<idle>";
	if ((d=a-offsetof(struct script, select))<m) m=d, s="<select>";
	if ((d=a-offsetof(struct script, prepare))<m) m=d, s="<prepare>";
	if ((d=a-offsetof(struct script, loadpos))<m) m=d, s="<loadpos>";
	if ((d=a-offsetof(struct script, prepare2))<m) m=d, s="<prepare2>";
	if ((d=a-offsetof(struct script, setmsg))<m) m=d, s="<setmsg>";
	if ((d=a-offsetof(struct script, clrack))<m) m=d, s="<clrack>";
	if ((d=a-offsetof(struct script, dispatch))<m) m=d, s="<dispatch>";
	if ((d=a-offsetof(struct script, checkatn))<m) m=d, s="<checkatn>";
	if ((d=a-offsetof(struct script, command))<m) m=d, s="<command>";
	if ((d=a-offsetof(struct script, status))<m) m=d, s="<status>";
	if ((d=a-offsetof(struct script, msg_in))<m) m=d, s="<msg_in>";
	if ((d=a-offsetof(struct script, msg_bad))<m) m=d, s="<msg_bad>";
	if ((d=a-offsetof(struct scripth, msg_parity))<m) m=d, s="<msg_parity>";
	if ((d=a-offsetof(struct scripth, msg_reject))<m) m=d, s="<msg_reject>";
	if ((d=a-offsetof(struct scripth, msg_extended))<m) m=d, s="<msg_extended>";
	if ((d=a-offsetof(struct scripth, msg_sdtr))<m) m=d, s="<msg_sdtr>";
	if ((d=a-offsetof(struct script, complete))<m) m=d, s="<complete>";
	if ((d=a-offsetof(struct script, cleanup))<m) m=d, s="<cleanup>";
	if ((d=a-offsetof(struct script, cleanup0))<m) m=d, s="<cleanup>";
	if ((d=a-offsetof(struct script, signal))<m) m=d, s="<signal>";
	if ((d=a-offsetof(struct script, save_dp))<m) m=d, s="<save_dp>";
	if ((d=a-offsetof(struct script, restore_dp))<m) m=d, s="<restore_dp>";
	if ((d=a-offsetof(struct script, disconnect))<m) m=d, s="<disconnect>";
	if ((d=a-offsetof(struct script, msg_out))<m) m=d, s="<msg_out>";
	if ((d=a-offsetof(struct script, msg_out_done))<m) m=d, s="<msg_out_done>";
	if ((d=a-offsetof(struct scripth, msg_out_abort))<m) m=d, s="<msg_out_abort>";
	if ((d=a-offsetof(struct scripth, getcc))<m) m=d, s="<getcc>";
	if ((d=a-offsetof(struct scripth, getcc1))<m) m=d, s="<getcc1>";
	if ((d=a-offsetof(struct scripth, getcc2))<m) m=d, s="<getcc2>";
	if ((d=a-offsetof(struct script, badgetcc))<m) m=d, s="<badgetcc>";
	if ((d=a-offsetof(struct script, reselect))<m) m=d, s="<reselect>";
	if ((d=a-offsetof(struct script, reselect2))<m) m=d, s="<reselect2>";
	if ((d=a-offsetof(struct script, resel_tmp))<m) m=d, s="<resel_tmp>";
	if ((d=a-offsetof(struct script, resel_lun))<m) m=d, s="<resel_lun>";
	if ((d=a-offsetof(struct script, resel_tag))<m) m=d, s="<resel_tag>";
	if ((d=a-offsetof(struct script, data_in))<m) m=d, s="<data_in>";
	if ((d=a-offsetof(struct script, data_out))<m) m=d, s="<data_out>";
	if ((d=a-offsetof(struct script, no_data))<m) m=d, s="<no_data>";
	if ((d=a-offsetof(struct scripth, aborttag))<m) m=d, s="<aborttag>";
	if ((d=a-offsetof(struct scripth, abort))<m) m=d, s="<abort>";

	if (!*s) return s;

	sprintf (buffer, "%s:%d%c", s, m/4, 0);
	return (buffer);
}

/*================================================================
**
**
**	Write debug info: utilities:     write misc. fields.
**
**
**================================================================
*/

static void printm (u_char * msg, int len)
{
	u_char l;
	do {
		if (*msg==M_EXTENDED)
			l=msg[1]+2;
		else if ((*msg & 0xf0)==0x20)
			l=2;
		else l=1;
		len-=l;

		printf (" %x",*msg++);
		while (--l>0) printf ("-%x",*msg++);
	} while (len>0);
}

void dump_table (const char * str, struct scr_tblmove * p, int l)
{
	int i;
	for (i=0;l>0;i++,p++,l--) if (p->size) {
	printf ("    %s[%d]: %5d @ 0x%08x\n",
		str, i, p->size, p->addr);
	};
}

void dump_link (const char* name, struct link * link)
{
	printf ("%s: cmd=%08x pa=%08x %s\n",
	name, link->l_cmd, link->l_paddr, sn(link->l_paddr));
}

/*================================================================
**
**
**	Write debug info: utilities:     write time fields.
**
**
**================================================================
*/

void dump_tstamp (const char* name, struct tstamp * p)
#define P(id,fld)\
	if (p->fld) \
		printf ("%s: "id" at %d hz", name,&p->fld);
{
	P ("started     ", start);
	P ("ended       ", end  );
	P ("selected    ", select);
	P ("command     ", command);
	P ("data        ", data);
	P ("status      ", status);
	P ("disconnected", disconnect);
	P ("reselected  ", reselect);
	printf ("\n");
}




void dump_profile (const char* name, struct profile * p)
{
	printf ("%s: %10d transfers.\n"        ,name,p->num_trans);
	printf ("%s: %10d bytes transferred.\n",name,p->num_bytes);
	printf ("%s: %10d disconnects.\n"      ,name,p->num_disc);
	printf ("%s: %10d short transfers.\n"  ,name,p->num_break);
	printf ("%s: %10d interrupts.\n"       ,name,p->num_int);
	printf ("%s: %10d on the fly ints.\n"  ,name,p->num_fly);
	printf ("%s: %10d ms setup time.\n"    ,name,p->ms_setup);
	printf ("%s: %10d ms data transfer.\n" ,name,p->ms_data);
	printf ("%s: %10d ms disconnected.\n"  ,name,p->ms_disc);
	printf ("%s: %10d ms postprocessing.\n",name,p->ms_post);
	printf ("\n");
}

/*================================================================
**
**
**	Write debug info: utilities:     write script registers.
**
**
**================================================================
*/

static void dump_reg(struct ncr_reg * rp)
{
	u_char *reg = (u_char*) rp;
#define l(i)  (reg[i]+(reg[i+1]<<8ul)+(reg[i+2]<<16ul)+(reg[i+3]<<24ul))
	int ad;

	char*(phasename[8])={"DATA-OUT","DATA-IN","COMMAND","STATUS",
				"ILG-OUT","ILG-IN","MESSAGE-OUT","MESSAGE-IN"};
	for (ad=0x00;ad<0x80;ad++) {
		switch (ad % 16) {

		case 0:
			printf ("        %02x:\t",ad);
			break;
		case 8:
			printf (" :  ");
			break;
		default:
			printf (" ");
		};
		printf ("%02x", reg[ad]);
		if (ad % 16 == 15) printf ("\n");
	};
	printf ("\n");
	printf ("        DSP  %08x %-20s  CMD %08x DSPS %08x %s\n",
		l(0x2c),sn(l(0x2c)),l(0x24),l(0x30), sn(l(0x30)));
	printf ("        TEMP %08x %-20s  DSA %08x\n",
		l(0x1c),sn(l(0x1c)),l(0x10));
	printf ("\n");
	printf ("        Busstatus: ");
	if ((reg[0x0b]>>7)&1) printf (" Req");
	if ((reg[0x0b]>>6)&1) printf (" Ack");
	if ((reg[0x0b]>>5)&1) printf (" Bsy");
	if ((reg[0x0b]>>4)&1) printf (" Sel");
	if ((reg[0x0b]>>3)&1) printf (" Atn");
	printf (" %s\n", phasename[reg[0x0b]&7]);

        printf ("        Dmastatus: ");
	if ((reg[0x0c]>>7)&1) printf (" FifoEmpty");
	if ((reg[0x0c]>>6)&1) printf (" MasterParityError");
	if ((reg[0x0c]>>5)&1) printf (" BusFault");
	if ((reg[0x0c]>>4)&1) printf (" Aborted");
	if ((reg[0x0c]>>3)&1) printf (" SingleStep");
	if ((reg[0x0c]>>2)&1) printf (" Interrupt");
	if ((reg[0x0c]>>0)&1) printf (" IllegalInstruction");
	printf ("\n");
	printf ("        Intstatus: ");
	if ((reg[0x14]>>7)&1) printf (" Abort");
	if ((reg[0x14]>>6)&1) printf (" SoftwareReset");
	if ((reg[0x14]>>5)&1) printf (" SignalProcess");
	if ((reg[0x14]>>4)&1) printf (" Semaphore");
	if ((reg[0x14]>>3)&1) printf (" Connected");
	if ((reg[0x14]>>2)&1) printf (" IntOnTheFly");
	if ((reg[0x14]>>1)&1) printf (" SCSI-Interrupt");
	if ((reg[0x14]>>0)&1) printf (" DMA-Interrupt");
	printf ("\n");
	printf ("        ScsiIstat: ");
	if ((reg[0x42]>>7)&1) printf (" PhaseMismatch");
	if ((reg[0x42]>>6)&1) printf (" Complete");
	if ((reg[0x42]>>5)&1) printf (" Selected");
	if ((reg[0x42]>>4)&1) printf (" Reselected");
	if ((reg[0x42]>>3)&1) printf (" GrossError");
	if ((reg[0x42]>>2)&1) printf (" UnexpectedDisconnect");
	if ((reg[0x42]>>1)&1) printf (" ScsiReset");
	if ((reg[0x42]>>0)&1) printf (" ParityError");
	if ((reg[0x43]>>2)&1) printf (" SelectionTimeout");
	if ((reg[0x43]>>1)&1) printf (" TimerExpired");
	if ((reg[0x43]>>0)&1) printf (" HandshakeTimeout");
	printf ("\n");
	printf ("        ID=%d  DEST-ID=%d  RESEL-ID=%d\n", reg[4]&7, reg[6]&7, reg[0xa]&7);
	printf ("\n");
}

/*================================================================
**
**
**	Write debug info: utilities:     write header.
**
**
**================================================================
*/

char * debug_opt;

void dump_head (struct head * hp)
{
	dump_link ("      launch", & hp->launch);
	printf    ("       savep: %08x %s\n",
				hp->savep, sn((u_long) hp->savep));
	printf    ("          cp: %08x %s\n",
				hp->cp, sn((u_long)hp->cp));
	if (strchr (debug_opt, 'y')) {
		printf ("\n");
		dump_tstamp ("   timestamp", &hp->stamp);
	};

	printf    ("      status: %x %x %x %x %x %x %x %x\n",
		hp->status[0], hp->status[1], hp->status[2], hp->status[3],
		hp->status[4], hp->status[5], hp->status[6], hp->status[7]);

	printf ("\n");
}

/*================================================================
**
**
**	Write debug info: utilities:     write ccb.
**
**
**================================================================
*/

void dump_ccb (struct ccb * cp, u_long base)
{
	printf ("----------------------\n");
	printf ("struct ccb @ %08x:\n", base);
	printf ("----------------------\n");

	dump_link ("        next", &cp->jump_ccb);
	dump_link ("        call", &cp->call_tmp);

	dump_head (&cp->phys.header);

	if (strchr (debug_opt, 's')) {
		dump_table(" smsg", &cp->phys.smsg,   1);
		dump_table("smsg2", &cp->phys.smsg2,  1);
		dump_table("  cmd", &cp->phys.cmd,    1);
		dump_table(" data", &cp->phys.data[0],MAX_SCATTER);
		dump_table("sense", &cp->phys.sense,  1);
	};

	if (strchr (debug_opt, 'a')) {
		int i;
		for (i=0; i<8; i++)
			printf ("    patch[%d]: %08x\n", i, cp->patch[i]);
	};

	if (strchr (debug_opt, 'x')) {
		printf ("        xfer: -- dump not yet implemented.\n");
	};

	if (strchr (debug_opt, 'm')) {
		printf ("        smsg:");
		printm (cp->scsi_smsg, cp->phys.smsg.size);
		printf ("\n");
		printf ("       smsg2:");
		printm (cp->scsi_smsg2, cp->phys.smsg2.size);
		printf ("\n");
	};

	printf ("       magic: %x\n", cp->magic);
	if (cp->tlimit)
	printf ("  timeout at: %s", ctime((time_t*)&cp->tlimit));
	printf ("    link_ccb: %08x\n", (u_long) cp->link_ccb);
	printf ("    next_ccb: %08x\n", (u_long) cp->next_ccb);
	printf ("         tag: %d\n", cp->tag);
	printf ("\n");
}

/*================================================================
**
**
**	Write debug info:	struct lcb
**
**
**================================================================
*/

static void dump_lcb (u_long base)
{
	struct lcb l;
	struct ccb c;
	u_long cp,cn;

	printf ("----------------------\n");
	printf ("struct lcb @ %08x:\n", base);
	printf ("----------------------\n");

	if (!KVM_READ (
		base,
		&l,
		sizeof (struct lcb)))
		errx(1, "bad kvm read");
	printf   ("     reqccbs: %d\n", l.reqccbs);
	printf   ("     actccbs: %d\n", l.actccbs);
	printf   ("     reqlink: %d\n", l.reqlink);
	printf   ("     actlink: %d\n", l.actlink);
	printf   ("     usetags: %d\n", l.usetags);
	dump_link ("    jump_lcb", &l.jump_lcb);
	dump_link ("    call_tag", &l.call_tag);
	dump_link ("    jump_ccb", &l.jump_ccb);
	printf ("\n");
	cp = (u_long) l.next_ccb;
	cn = 0;
	while (cp) {
		cn++;
		printf ("ccb #%d:\n", cn);
		if (!KVM_READ (
			cp,
			&c,
			sizeof (struct ccb)))
			errx(1, "bad kvm read");
		dump_ccb (&c, cp);
		cp= (u_long) c.next_ccb;
	};
}

/*================================================================
**
**
**	Write debug info:	struct tcb
**
**
**================================================================
*/

static void dump_tip (struct tcb * tip)
{
	int i;
	u_long lp;

	printf ("----------------------\n");
	printf ("struct tcb:\n");
	printf ("----------------------\n");

	printf ("   transfers:%10d.\n", tip->transfers);
	printf ("       bytes:%10d.\n", tip->bytes    );
	printf (" user limits: usrsync=%d  usrwide=%d  usrtags=%d.\n",
			tip->usrsync, tip->usrwide, tip->usrtags);
	printf ("        sync: minsync=%d, maxoffs=%d, period=%d ns, sval=%x.\n",
			tip->minsync, tip->maxoffs, tip->period, tip->sval);
	printf ("	wide: widedone=%d, wval=%x.\n",
			tip->widedone, tip->wval);

	printf   ("     hold_cp: %x\n", tip->hold_cp);
	dump_link ("    jump_tcb", &tip->jump_tcb);
	dump_link ("    call_lun", &tip->call_lun);
	dump_link ("    jump_lcb", &tip->jump_lcb);
	if (tip->hold_cp) printf ("     hold_cp: @ %x\n", tip->hold_cp);
	printf ("\n");

	if (strchr (debug_opt, 'l')) {
		for (i=0;i<MAX_LUN;i++) {
			lp= (u_long) tip->lp[i];
			printf ("logic unit #%d:\n", i);
			if (lp) dump_lcb (lp);
		};
	}
}

/*================================================================
**
**
**	Write debug info:	struct ncb
**
**
**================================================================
*/


static void dump_ncr (void)
{
	int i;

	printf ("----------------------\n");
	printf ("struct ncb @ %x:\n", ncr_base);
	printf ("----------------------\n");

	dump_link ("    jump_tcb", &ncr.jump_tcb);
	printf    ("    register: @ %x (p=%x)\n", ncr.vaddr, ncr.paddr);

	if (wizard && strchr (debug_opt, 'r')) {
		struct ncr_reg reg;

		if (!KVM_READ (
			ncr.vaddr,
			&reg,
			sizeof (reg)))
			errx(1, "bad kvm read");

		printf ("\n");
		dump_reg (&reg);
	};

	printf    ("      script: @ %x (p=%x)\n", ncr.script, ncr.p_script);

	printf ("hostscsiaddr: %d\n", ncr.myaddr);
	printf ("     minsync: %d\n", ncr.minsync);
	printf ("      scntl3: 0x%02x\n", ncr.rv_scntl3);
	printf ("\n");

	/* sc_link not dumped */

	if (strchr (debug_opt, 'u')) {
		printf ("     usercmd: cmd=%x data=%x target=%x lun=%x\n",
			ncr.user.cmd,
			ncr.user.data,
			ncr.user.target,
			ncr.user.lun);
	};

	printf ("     actccbs: %d\n", ncr.actccbs);

	if (strchr (debug_opt, 'q')) {

		u_long	startpos;

		if (!KVM_READ (
			((u_long)ncr.script
				+offsetof(struct script, startpos)),
			&startpos,
			sizeof (startpos)))
			errx(1, "bad kvm read");

		printf ("    startpos: %x\n", startpos);
		printf ("        slot: %d\n", (startpos-
			(ncr.p_script+offsetof(struct scripth, tryloop)))/20);
		printf ("    squeuput: %d\n", ncr.squeueput);
		for (i=0; i<MAX_START; i++)
			printf ("%12d: %08x %s\n", i,
			ncr.squeue[i], sn(ncr.squeue[i]));

		printf ("\n");
	};

	printf ("       ticks: %d ms\n", ncr.ticks * 10);
	printf ("   heartbeat: %s", ctime ((time_t*)&ncr.heartbeat));
	printf ("    lasttime: %s", ctime ((time_t*)&ncr.lasttime));
	printf ("\n");

	if (wizard && strchr (debug_opt, 'd') && ncr.regtime.tv_sec) {
		printf ("     regdump: %s", ctime (&ncr.regtime.tv_sec));
		dump_reg (&ncr.regdump);
	};

	if (strchr (debug_opt, 'p')) {
		printf ("\n");
		dump_profile ("     profile", &ncr.profile);
	};

	if (strchr (debug_opt, 'h')) {
		printf ("\n");
		dump_head ( &ncr.header);
	};

	if (strchr (debug_opt, 'c')) {
		dump_ccb  (ncr.ccb, ncr_base + offsetof (struct ncb, ccb));
	};

	if (strchr (debug_opt, 'm')) {
		printf ("      msgout:"); printm (ncr.msgout,0); printf ("\n");
		printf ("      msg in:"); printm (ncr.msgin,0);  printf ("\n");
		printf ("\n");
	};

	if (strchr (debug_opt, 't')) {
		struct tcb * tip;
		for (i=0;i<MAX_TARGET;i++) {
			tip = &ncr.target[i];
			if (!tip->jump_tcb.l_cmd) continue;
			printf ("target #%d:\n", i);
			dump_tip (tip);
		}
	}
}

/*================================================================
**
**
**	D O _ D E B U G
**
**
**================================================================
*/


void do_debug(char * arg)
{
	open_kvm(O_RDONLY);
	debug_opt = arg;
	if (strchr (debug_opt, '?')) printf (
"'?': list debug options [sic].\n"
"'a': show patchfields in ccbs (requires c).\n"
"'c': show ccbs.\n"
"'d': show register dump.\n"
"'h': show header information.\n"
"'m': show message buffers.\n"
"'n': show ncr main control block.\n"
"'p': show profiling information.\n"
"'q': show start queue.\n"
"'r': show registers (*DANGEROUS*).\n"
"'s': show scatter/gather info.\n"
"'t': show target control blocks.\n"
"'u': show user cmd field.\n"
"'x': show generic xfer structure.\n"
"'y': show timestamps.\n"
"\n"
	);

	if (strchr (debug_opt, 'n')) dump_ncr ();

	if (!wizard)
		errx(2, "you are NOT a wizard!");
	if (strchr (debug_opt, 'r')) {
		struct ncr_reg reg;
		if (!KVM_READ (
			ncr.vaddr,
			&reg,
			sizeof (reg)))
			errx(1, "bad kvm read");
		dump_reg (&reg);
	};
}


/*================================================================
**
**
**	Main function
**
**
**================================================================
*/

int main(argc, argv)
	int argc;
	char **argv;
{
	int usageflg=0;
	char * charp;
	int ch;
	int i;

	while ((ch = getopt(argc, argv, "M:N:u:f:t:l:p:s:k:d:vwhin:?")) != -1)
	switch((char)ch) {
	case 'M':
		if (kvm_isopen)
			errx(1, "-M: kernel file already open");
		kmemf = optarg;
		break;
	case 'N':
		if (kvm_isopen)
			errx(1, "-N: symbol table already open");
		vmunix = optarg;
		break;
#ifdef OPT_F
	case 'f':
		errx(1, "-f: option not yet implemented");
#endif

        case 'u':
		i = strtoul (optarg, &charp, 0);
		if (!*optarg || *charp || (i<0))
			errx(1, "bad unit number \"%s\"", optarg);
		ncr_unit = i;
		break;
	case 't':
		i = strtoul (optarg, &charp, 0);
		if (!*optarg || *charp || (i<0) || (i>=MAX_TARGET))
			errx(1, "bad target number \"%s\" (valid range: 0-%d)",
				optarg, MAX_TARGET-1);
		target_mask |= 1ul << i;
		break;
	case 'n':
		open_kvm(O_RDONLY);
		i = strtoul (optarg, &charp, 0);
		printf ("addr %d (0x%x) has label %s.\n",
			i,i,sn(i));
		break;
	case 'l':
		i = strtoul (optarg, &charp, 0);
		if (!*optarg || *charp || (i<0) || (i>=MAX_LUN))
			errx(1,
			"bad logic unit number \"%s\" (valid range: 0-%d)",
				optarg, MAX_LUN);
		global_lun_mask |= 1ul << i;
		break;
	case 'p':
		i = strtoul (optarg, &charp, 0);
		if (!*optarg || *charp || (i<1) || (i>60))
			errx(1, "bad interval \"%s\"", optarg);
		interval = i;
		do_profile();
		break;

        case 'w':
		if(geteuid()==0)
			wizard=1;
		break;
	case 'v':
		verbose++;
		break;
	case 'i':
		do_info();
		break;

	case 's':
		do_set(optarg);
		break;
	case 'd':
		do_debug(optarg);
		break;
	case 'k':
		do_kill(optarg);
		break;
	case 'h':
	case '?':
		usageflg++;
		break;
	default:
		warnx("illegal option \"%c\"", ch);
		usageflg++;
	}

	argv += optind;
	argc -= optind;

	if (argc)
		warnx("rest of line starting with \"%s\" ignored", *argv);

	if (verbose&&!kvm_isopen) usageflg++;
	if (usageflg) {
		usage();
		if (verbose) fprintf (stderr, ident);
		exit (1);
	}

	if (!kvm_isopen) {
		do_info();
		do_profile();
	};
	exit (0);
}


static void
usage (void)
{
	fprintf (stderr, "%s\n%s\n%s\n%s\n%s\n",
"usage: ncrcontrol [-M core] [-N system] [-u unit] [-v] [-v] -i",
"       ncrcontrol [-N system] [-u unit] [-p wait]",
"       ncrcontrol [-N system] [-u unit] [-t target] -s name=value",
"       ncrcontrol [-M core] [-N system] [-u unit] [-t target] -d debug",
"       ncrcontrol [-N system] [-u unit] -w -k torture");
}
