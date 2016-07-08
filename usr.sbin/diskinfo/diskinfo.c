/*-
 * Copyright (c) 2003 Poul-Henning Kamp
 * Copyright (c) 2015 Spectra Logic Corporation
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
#include <stdint.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <libutil.h>
#include <paths.h>
#include <err.h>
#include <sys/disk.h>
#include <sys/param.h>
#include <sys/time.h>

static void
usage(void)
{
	fprintf(stderr, "usage: diskinfo [-ctv] disk ...\n");
	exit (1);
}

static int opt_c, opt_t, opt_v;

static void speeddisk(int fd, off_t mediasize, u_int sectorsize);
static void commandtime(int fd, off_t mediasize, u_int sectorsize);
static int zonecheck(int fd, uint32_t *zone_mode, char *zone_str,
		     size_t zone_str_len);

int
main(int argc, char **argv)
{
	int i, ch, fd, error, exitval = 0;
	char buf[BUFSIZ], ident[DISK_IDENT_SIZE], physpath[MAXPATHLEN];
	char zone_desc[64];
	off_t	mediasize, stripesize, stripeoffset;
	u_int	sectorsize, fwsectors, fwheads, zoned = 0;
	uint32_t zone_mode;

	while ((ch = getopt(argc, argv, "ctv")) != -1) {
		switch (ch) {
		case 'c':
			opt_c = 1;
			opt_v = 1;
			break;
		case 't':
			opt_t = 1;
			opt_v = 1;
			break;
		case 'v':
			opt_v = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();

	for (i = 0; i < argc; i++) {
		fd = open(argv[i], O_RDONLY);
		if (fd < 0 && errno == ENOENT && *argv[i] != '/') {
			sprintf(buf, "%s%s", _PATH_DEV, argv[i]);
			fd = open(buf, O_RDONLY);
		}
		if (fd < 0) {
			warn("%s", argv[i]);
			exitval = 1;
			goto out;
		}
		error = ioctl(fd, DIOCGMEDIASIZE, &mediasize);
		if (error) {
			warnx("%s: ioctl(DIOCGMEDIASIZE) failed, probably not a disk.", argv[i]);
			exitval = 1;
			goto out;
		}
		error = ioctl(fd, DIOCGSECTORSIZE, &sectorsize);
		if (error) {
			warnx("%s: ioctl(DIOCGSECTORSIZE) failed, probably not a disk.", argv[i]);
			exitval = 1;
			goto out;
		}
		error = ioctl(fd, DIOCGFWSECTORS, &fwsectors);
		if (error)
			fwsectors = 0;
		error = ioctl(fd, DIOCGFWHEADS, &fwheads);
		if (error)
			fwheads = 0;
		error = ioctl(fd, DIOCGSTRIPESIZE, &stripesize);
		if (error)
			stripesize = 0;
		error = ioctl(fd, DIOCGSTRIPEOFFSET, &stripeoffset);
		if (error)
			stripeoffset = 0;
		error = zonecheck(fd, &zone_mode, zone_desc, sizeof(zone_desc));
		if (error == 0)
			zoned = 1;
		if (!opt_v) {
			printf("%s", argv[i]);
			printf("\t%u", sectorsize);
			printf("\t%jd", (intmax_t)mediasize);
			printf("\t%jd", (intmax_t)mediasize/sectorsize);
			printf("\t%jd", (intmax_t)stripesize);
			printf("\t%jd", (intmax_t)stripeoffset);
			if (fwsectors != 0 && fwheads != 0) {
				printf("\t%jd", (intmax_t)mediasize /
				    (fwsectors * fwheads * sectorsize));
				printf("\t%u", fwheads);
				printf("\t%u", fwsectors);
			} 
		} else {
			humanize_number(buf, 5, (int64_t)mediasize, "",
			    HN_AUTOSCALE, HN_B | HN_NOSPACE | HN_DECIMAL);
			printf("%s\n", argv[i]);
			printf("\t%-12u\t# sectorsize\n", sectorsize);
			printf("\t%-12jd\t# mediasize in bytes (%s)\n",
			    (intmax_t)mediasize, buf);
			printf("\t%-12jd\t# mediasize in sectors\n",
			    (intmax_t)mediasize/sectorsize);
			printf("\t%-12jd\t# stripesize\n", stripesize);
			printf("\t%-12jd\t# stripeoffset\n", stripeoffset);
			if (fwsectors != 0 && fwheads != 0) {
				printf("\t%-12jd\t# Cylinders according to firmware.\n", (intmax_t)mediasize /
				    (fwsectors * fwheads * sectorsize));
				printf("\t%-12u\t# Heads according to firmware.\n", fwheads);
				printf("\t%-12u\t# Sectors according to firmware.\n", fwsectors);
			} 
			if (ioctl(fd, DIOCGIDENT, ident) == 0)
				printf("\t%-12s\t# Disk ident.\n", ident);
			if (ioctl(fd, DIOCGPHYSPATH, physpath) == 0)
				printf("\t%-12s\t# Physical path\n", physpath);
			if (zoned != 0)
				printf("\t%-12s\t# Zone Mode\n", zone_desc);
		}
		printf("\n");
		if (opt_c)
			commandtime(fd, mediasize, sectorsize);
		if (opt_t)
			speeddisk(fd, mediasize, sectorsize);
out:
		close(fd);
	}
	exit (exitval);
}


static char sector[65536];
static char mega[1024 * 1024];

static void
rdsect(int fd, off_t blockno, u_int sectorsize)
{
	int error;

	lseek(fd, (off_t)blockno * sectorsize, SEEK_SET);
	error = read(fd, sector, sectorsize);
	if (error == -1)
		err(1, "read");
	if (error != (int)sectorsize)
		errx(1, "disk too small for test.");
}

static void
rdmega(int fd)
{
	int error;

	error = read(fd, mega, sizeof(mega));
	if (error == -1)
		err(1, "read");
	if (error != sizeof(mega))
		errx(1, "disk too small for test.");
}

static struct timeval tv1, tv2;

static void
T0(void)
{

	fflush(stdout);
	sync();
	sleep(1);
	sync();
	sync();
	gettimeofday(&tv1, NULL);
}

static void
TN(int count)
{
	double dt;

	gettimeofday(&tv2, NULL);
	dt = (tv2.tv_usec - tv1.tv_usec) / 1e6;
	dt += (tv2.tv_sec - tv1.tv_sec);
	printf("%5d iter in %10.6f sec = %8.3f msec\n",
		count, dt, dt * 1000.0 / count);
}

static void
TR(double count)
{
	double dt;

	gettimeofday(&tv2, NULL);
	dt = (tv2.tv_usec - tv1.tv_usec) / 1e6;
	dt += (tv2.tv_sec - tv1.tv_sec);
	printf("%8.0f kbytes in %10.6f sec = %8.0f kbytes/sec\n",
		count, dt, count / dt);
}

static void
speeddisk(int fd, off_t mediasize, u_int sectorsize)
{
	int bulk, i;
	off_t b0, b1, sectorcount, step;

	sectorcount = mediasize / sectorsize;
	step = 1ULL << (flsll(sectorcount / (4 * 200)) - 1);
	if (step > 16384)
		step = 16384;
	bulk = mediasize / (1024 * 1024);
	if (bulk > 100)
		bulk = 100;

	printf("Seek times:\n");
	printf("\tFull stroke:\t");
	b0 = 0;
	b1 = sectorcount - step;
	T0();
	for (i = 0; i < 125; i++) {
		rdsect(fd, b0, sectorsize);
		b0 += step;
		rdsect(fd, b1, sectorsize);
		b1 -= step;
	}
	TN(250);

	printf("\tHalf stroke:\t");
	b0 = sectorcount / 4;
	b1 = b0 + sectorcount / 2;
	T0();
	for (i = 0; i < 125; i++) {
		rdsect(fd, b0, sectorsize);
		b0 += step;
		rdsect(fd, b1, sectorsize);
		b1 += step;
	}
	TN(250);
	printf("\tQuarter stroke:\t");
	b0 = sectorcount / 4;
	b1 = b0 + sectorcount / 4;
	T0();
	for (i = 0; i < 250; i++) {
		rdsect(fd, b0, sectorsize);
		b0 += step;
		rdsect(fd, b1, sectorsize);
		b1 += step;
	}
	TN(500);

	printf("\tShort forward:\t");
	b0 = sectorcount / 2;
	T0();
	for (i = 0; i < 400; i++) {
		rdsect(fd, b0, sectorsize);
		b0 += step;
	}
	TN(400);

	printf("\tShort backward:\t");
	b0 = sectorcount / 2;
	T0();
	for (i = 0; i < 400; i++) {
		rdsect(fd, b0, sectorsize);
		b0 -= step;
	}
	TN(400);

	printf("\tSeq outer:\t");
	b0 = 0;
	T0();
	for (i = 0; i < 2048; i++) {
		rdsect(fd, b0, sectorsize);
		b0++;
	}
	TN(2048);

	printf("\tSeq inner:\t");
	b0 = sectorcount - 2048;
	T0();
	for (i = 0; i < 2048; i++) {
		rdsect(fd, b0, sectorsize);
		b0++;
	}
	TN(2048);

	printf("Transfer rates:\n");
	printf("\toutside:     ");
	rdsect(fd, 0, sectorsize);
	T0();
	for (i = 0; i < bulk; i++) {
		rdmega(fd);
	}
	TR(bulk * 1024);

	printf("\tmiddle:      ");
	b0 = sectorcount / 2 - bulk * (1024*1024 / sectorsize) / 2 - 1;
	rdsect(fd, b0, sectorsize);
	T0();
	for (i = 0; i < bulk; i++) {
		rdmega(fd);
	}
	TR(bulk * 1024);

	printf("\tinside:      ");
	b0 = sectorcount - bulk * (1024*1024 / sectorsize) - 1;
	rdsect(fd, b0, sectorsize);
	T0();
	for (i = 0; i < bulk; i++) {
		rdmega(fd);
	}
	TR(bulk * 1024);

	printf("\n");
	return;
}

static void
commandtime(int fd, off_t mediasize, u_int sectorsize)
{	
	double dtmega, dtsector;
	int i;

	printf("I/O command overhead:\n");
	i = mediasize;
	rdsect(fd, 0, sectorsize);
	T0();
	for (i = 0; i < 10; i++)
		rdmega(fd);
	gettimeofday(&tv2, NULL);
	dtmega = (tv2.tv_usec - tv1.tv_usec) / 1e6;
	dtmega += (tv2.tv_sec - tv1.tv_sec);

	printf("\ttime to read 10MB block    %10.6f sec\t= %8.3f msec/sector\n",
		dtmega, dtmega*100/2048);

	rdsect(fd, 0, sectorsize);
	T0();
	for (i = 0; i < 20480; i++)
		rdsect(fd, 0, sectorsize);
	gettimeofday(&tv2, NULL);
	dtsector = (tv2.tv_usec - tv1.tv_usec) / 1e6;
	dtsector += (tv2.tv_sec - tv1.tv_sec);

	printf("\ttime to read 20480 sectors %10.6f sec\t= %8.3f msec/sector\n",
		dtsector, dtsector*100/2048);
	printf("\tcalculated command overhead\t\t\t= %8.3f msec/sector\n",
		(dtsector - dtmega)*100/2048);

	printf("\n");
	return;
}

static int
zonecheck(int fd, uint32_t *zone_mode, char *zone_str, size_t zone_str_len)
{
	struct disk_zone_args zone_args;
	int error;

	bzero(&zone_args, sizeof(zone_args));

	zone_args.zone_cmd = DISK_ZONE_GET_PARAMS;
	error = ioctl(fd, DIOCZONECMD, &zone_args);

	if (error == 0) {
		*zone_mode = zone_args.zone_params.disk_params.zone_mode;

		switch (*zone_mode) {
		case DISK_ZONE_MODE_NONE:
			snprintf(zone_str, zone_str_len, "Not_Zoned");
			break;
		case DISK_ZONE_MODE_HOST_AWARE:
			snprintf(zone_str, zone_str_len, "Host_Aware");
			break;
		case DISK_ZONE_MODE_DRIVE_MANAGED:
			snprintf(zone_str, zone_str_len, "Drive_Managed");
			break;
		case DISK_ZONE_MODE_HOST_MANAGED:
			snprintf(zone_str, zone_str_len, "Host_Managed");
			break;
		default:
			snprintf(zone_str, zone_str_len, "Unknown_zone_mode_%u",
			    *zone_mode);
			break;
		}
	}
	return (error);
}
