/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)names.c	5.2 (Berkeley) 6/4/91
 */

#if !defined(hp300) && !defined(tahoe) && !defined(vax) && !defined(__386BSD__)
char *defdrives[] = { 0 };

void read_names()
{
}
#endif

#ifdef __386BSD__
/*
 * 386BSD support added by Rodney W. Grimes, rgrimes@agora.rain.com 3/24/93
 */
#include <i386/isa/isa_device.h>

char *defdrives[] = { "fd0", "fd1", "wd0", "wd1",
	              "as0", "as1", "sd0", "sd1", 0 };

void
read_names()
{
	register char *p;
	register u_long isa_bio;
	static char buf[BUFSIZ];
	struct isa_device dev;
	struct isa_driver drv;
	char name[10];
	int i = 0;
	int dummydk = 0;
	int fdunit = 0;
	int wdunit = 0;
	int ahaunit = 0;

	isa_bio = nl[X_ISA_BIO].n_value;
	if (isa_bio == 0) {
		(void) fprintf(stderr,
		    "vmstat: disk init info not in namelist\n");
		exit(1);
	}
		
	p = buf;
	for (;; isa_bio += sizeof dev) {
		(void)kvm_read((void *)isa_bio, &dev, sizeof dev);
		if (dev.id_driver == 0)
			break;
		if (dev.id_alive == 0)
			continue;
		(void)kvm_read(dev.id_driver, &drv, sizeof drv);
		(void)kvm_read(drv.name, name, sizeof name);

		/*
		 * 386bsd is kinda brain dead about dk_units, or at least
		 * I can't figure out how to get the real unit mappings
		 */
		if (strcmp(name, "fd") == 0) dummydk = fdunit++;
		if (strcmp(name, "wd") == 0) dummydk = wdunit++;
		if (strcmp(name, "aha") == 0) dummydk = ahaunit++;

		dr_name[i] = p;
		p += sprintf(p, "%s%d", name, dummydk) + 1;
		i++;
	}
}
#endif /* __386BSD__ */

#ifdef hp300
#include <hp300/dev/device.h>

char *defdrives[] = { "sd0", "sd1", "sd2", "rd0", "rd1", "rd2", 0 };

void
read_names()
{
	register char *p;
	register u_long hp;
	static char buf[BUFSIZ];
	struct hp_device hdev;
	struct driver hdrv;
	char name[10];

	hp = nl[X_HPDINIT].n_value;
	if (hp == 0) {
		(void) fprintf(stderr,
		    "vmstat: disk init info not in namelist\n");
		exit(1);
	}
	p = buf;
	for (;; hp += sizeof hdev) {
		(void)kvm_read((void *)hp, &hdev, sizeof hdev);
		if (hdev.hp_driver == 0)
			break;
		if (hdev.hp_dk < 0 || hdev.hp_alive == 0 ||
		    hdev.hp_cdriver == 0)
			continue;
		(void)kvm_read(hdev.hp_driver, &hdrv, sizeof hdrv);
		(void)kvm_read(hdrv.d_name, name, sizeof name);
		dr_name[hdev.hp_dk] = p;
		p += sprintf(p, "%s%d", name, hdev.hp_unit) + 1;
	}
}
#endif /* hp300 */

#ifdef tahoe
#include <tahoe/vba/vbavar.h>

char *defdrives[] = { "dk0", "dk1", "dk2", 0 };

void
read_names()
{
	register char *p;
	struct vba_device udev, *up;
	struct vba_driver udrv;
	char name[10];
	static char buf[BUFSIZ];

	up = (struct vba_device *) nl[X_VBDINIT].n_value;
	if (up == 0) {
		(void) fprintf(stderr,
		    "vmstat: disk init info not in namelist\n");
		exit(1);
	}
	p = buf;
	for (;; up += sizeof udev) {
		(void)kvm_read(up, &udev, sizeof udev);
		if (udev.ui_driver == 0)
			break;
		if (udev.ui_dk < 0 || udev.ui_alive == 0)
			continue;
		(void)kvm_read(udev.ui_driver, &udrv, sizeof udrv);
		(void)kvm_read(udrv.ud_dname, name, sizeof name);
		dr_name[udev.ui_dk] = p;
		p += sprintf(p, "%s%d", name, udev.ui_unit);
	}
}
#endif /* tahoe */

#ifdef vax
#include <vax/uba/ubavar.h>
#include <vax/mba/mbavar.h>

char *defdrives[] = { "hp0", "hp1", "hp2", 0 };

void
read_names()
{
	register char *p;
	unsigned long mp, up;
	struct mba_device mdev;
	struct mba_driver mdrv;
	struct uba_device udev;
	struct uba_driver udrv;
	char name[10];
	static char buf[BUFSIZ];

	mp = nl[X_MBDINIT].n_value;
	up = nl[X_UBDINIT].n_value;
	if (mp == 0 && up == 0) {
		(void) fprintf(stderr,
		    "vmstat: disk init info not in namelist\n");
		exit(1);
	}
	p = buf;
	if (mp) for (;; mp += sizeof mdev) {
		(void)kvm_read(mp, &mdev, sizeof mdev);
		if (mdev.mi_driver == 0)
			break;
		if (mdev.mi_dk < 0 || mdev.mi_alive == 0)
			continue;
		(void)kvm_read(mdev.mi_driver, &mdrv, sizeof mdrv);
		(void)kvm_read(mdrv.md_dname, name, sizeof name);
		dr_name[mdev.mi_dk] = p;
		p += sprintf(p, "%s%d", name, mdev.mi_unit);
	}
	if (up) for (;; up += sizeof udev) {
		(void)kvm_read(up, &udev, sizeof udev);
		if (udev.ui_driver == 0)
			break;
		if (udev.ui_dk < 0 || udev.ui_alive == 0)
			continue;
		(void)kvm_read(udev.ui_driver, &udrv, sizeof udrv);
		(void)kvm_read(udrv.ud_dname, name, sizeof name);
		dr_name[udev.ui_dk] = p;
		p += sprintf(p, "%s%d", name, udev.ui_unit);
	}
}
#endif /* vax */
