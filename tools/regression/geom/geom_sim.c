/*-
 * Copyright (c) 2002 Poul-Henning Kamp
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Poul-Henning Kamp
 * and NAI Labs, the Security Research Division of Network Associates, Inc.
 * under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <err.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/sbuf.h>
#include <geom/geom.h>

int
thread_sim(void *ptr)
{
	struct thread *tp = ptr;
	struct g_consumer *cp;

	printf("Running %s\n", tp->name);
	rattle();
	g_topology_lock();
	printf("--- g_simdisk_init();\n");
	g_simdisk_init();
	printf("--- g_bsd_init();\n");
	g_bsd_init();
	printf("--- g_mbr_init();\n");
	g_mbr_init();
	g_mbrext_init();
	printf("--- g_dev_init();\n");
	g_dev_init(NULL);
	g_topology_unlock();
	rattle();

#if 0
	g_simdisk_new("ad0", "/dev/ad0");
#else
	g_simdisk_xml_load("ad0", "Data/disk.typo.ad0.xml");
	// g_simdisk_xml_load("ad0", "Data/disk.msdos.ext.xml");
	// g_simdisk_xml_load("ad0", "Data/disk.far.ad0.xml");
#endif
	rattle();
	dumpf("1");
	conff("1");
	sdumpf("2");
	g_simdisk_xml_save("ad0", "_ad0");
	g_simdisk_destroy("ad0");
	rattle();
	dumpf("1");
	conff("1");
	sdumpf("2");

	printf("Done\n");
	exit (0);

#if 0
	printf("--- g_dev_finddev(\"ad0s1a\");\n");
	cp = g_dev_finddev("ad0s1a");
	if (cp == NULL)
		errx(1, "argh!");
	printf("--- g_access_rel(cp, 1, 1, 1);\n");
	g_access_rel(cp, 1, 1, 1);
	sleep(3);
	dumpf("1");
	conff("1");
	sdumpf("2");

	printf("--- g_access_rel(cp, -1, -1, -1);\n");
	g_access_rel(cp, -1, -1, -1);
	sleep(3);
	dumpf("1");
	conff("1");
	sdumpf("2");

	printf("--- g_dev_finddev(\"ad0\");\n");
	cp = g_dev_finddev("ad0");
	if (cp == NULL)
		errx(1, "argh!");
	printf("--- g_access_rel(cp, 1, 1, 1);\n");
	g_access_rel(cp, 1, 1, 1);
	sleep(3);
	dumpf("1");
	conff("1");
	sdumpf("2");

	printf("--- g_access_rel(cp, -1, -1, -1);\n");
	g_access_rel(cp, -1, -1, -1);

	sleep (3);
	dumpf("1");
	conff("1");
	sdumpf("2");
#if 0
	printf("--- Simulation done...\n");
	for (;;)
		sleep(1);
	exit (0);
#endif

#endif
#if 1
	g_simdisk_new("../Disks/typo.freebsd.dk:ad0", "ad1");
#else
	g_simdisk_xml_load("ad0", "disk.critter.ad0.xml");
#endif
	sleep (3);
	dumpf("1");
	conff("1");
	sdumpf("2");
	g_simdisk_xml_save("ad1", "_ad1");

#if 1
	sleep (3);
	g_simdisk_new("/home/phk/phk2001/msdos_6_2-1.flp", "fd0");
	sleep (3);
	dumpf("1");
	conff("1");
	sdumpf("2");
	g_simdisk_xml_save("fd0", "_fd0");

	sleep (3);
	g_simdisk_new("/home/phk/phk2001/kern.flp", "fd1");
	sleep (3);
	dumpf("1");
	conff("1");
	sdumpf("2");
	g_simdisk_xml_save("fd1", "_fd1");

	sleep (3);
	g_simdisk_new("../Disks/far:ad0", "ad2");
	sleep (3);
	dumpf("1");
	conff("1");
	sdumpf("2");
	g_simdisk_xml_save("ad2", "_ad2");

#endif
	sleep (3);
	g_simdisk_destroy("ad1");
	sleep (5);
	dumpf("1");
	conff("1");
	sdumpf("2");

	printf("Simulation done...\n");

	exit (0);
}

