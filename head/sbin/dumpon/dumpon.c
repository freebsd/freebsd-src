/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#if 0
#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "From: @(#)swapon.c	8.1 (Berkeley) 6/5/93";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/disk.h>
#include <sys/sysctl.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#ifdef HAVE_CRYPTO
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#endif

static int	verbose;

static void
usage(void)
{
	fprintf(stderr, "%s\n%s\n%s\n",
	    "usage: dumpon [-v] [-k public_key_file] [-z] special_file",
	    "       dumpon [-v] off",
	    "       dumpon [-v] -l");
	exit(EX_USAGE);
}

static void
check_size(int fd, const char *fn)
{
	int name[] = { CTL_HW, HW_PHYSMEM };
	size_t namelen = nitems(name);
	unsigned long physmem;
	size_t len;
	off_t mediasize;
	int minidump;

	len = sizeof(minidump);
	if (sysctlbyname("debug.minidump", &minidump, &len, NULL, 0) == 0 &&
	    minidump == 1)
		return;
	len = sizeof(physmem);
	if (sysctl(name, namelen, &physmem, &len, NULL, 0) != 0)
		err(EX_OSERR, "can't get memory size");
	if (ioctl(fd, DIOCGMEDIASIZE, &mediasize) != 0)
		err(EX_OSERR, "%s: can't get size", fn);
	if ((uintmax_t)mediasize < (uintmax_t)physmem) {
		if (verbose)
			printf("%s is smaller than physical memory\n", fn);
		exit(EX_IOERR);
	}
}

#ifdef HAVE_CRYPTO
static void
genkey(const char *pubkeyfile, struct diocskerneldump_arg *kda)
{
	FILE *fp;
	RSA *pubkey;

	assert(pubkeyfile != NULL);
	assert(kda != NULL);

	fp = NULL;
	pubkey = NULL;

	fp = fopen(pubkeyfile, "r");
	if (fp == NULL)
		err(1, "Unable to open %s", pubkeyfile);

	if (cap_enter() < 0 && errno != ENOSYS)
		err(1, "Unable to enter capability mode");

	pubkey = RSA_new();
	if (pubkey == NULL) {
		errx(1, "Unable to allocate an RSA structure: %s",
		    ERR_error_string(ERR_get_error(), NULL));
	}

	pubkey = PEM_read_RSA_PUBKEY(fp, &pubkey, NULL, NULL);
	fclose(fp);
	fp = NULL;
	if (pubkey == NULL)
		errx(1, "Unable to read data from %s.", pubkeyfile);

	kda->kda_encryptedkeysize = RSA_size(pubkey);
	if (kda->kda_encryptedkeysize > KERNELDUMP_ENCKEY_MAX_SIZE) {
		errx(1, "Public key has to be at most %db long.",
		    8 * KERNELDUMP_ENCKEY_MAX_SIZE);
	}

	kda->kda_encryptedkey = calloc(1, kda->kda_encryptedkeysize);
	if (kda->kda_encryptedkey == NULL)
		err(1, "Unable to allocate encrypted key");

	kda->kda_encryption = KERNELDUMP_ENC_AES_256_CBC;
	arc4random_buf(kda->kda_key, sizeof(kda->kda_key));
	if (RSA_public_encrypt(sizeof(kda->kda_key), kda->kda_key,
	    kda->kda_encryptedkey, pubkey,
	    RSA_PKCS1_PADDING) != (int)kda->kda_encryptedkeysize) {
		errx(1, "Unable to encrypt the one-time key.");
	}
	RSA_free(pubkey);
}
#endif

static void
listdumpdev(void)
{
	char dumpdev[PATH_MAX];
	size_t len;
	const char *sysctlname = "kern.shutdown.dumpdevname";

	len = sizeof(dumpdev);
	if (sysctlbyname(sysctlname, &dumpdev, &len, NULL, 0) != 0) {
		if (errno == ENOMEM) {
			err(EX_OSERR, "Kernel returned too large of a buffer for '%s'\n",
				sysctlname);
		} else {
			err(EX_OSERR, "Sysctl get '%s'\n", sysctlname);
		}
	}
	if (verbose) {
		printf("kernel dumps on ");
	}
	if (strlen(dumpdev) == 0) {
		printf("%s\n", _PATH_DEVNULL);
	} else {
		printf("%s\n", dumpdev);
	}
}

int
main(int argc, char *argv[])
{
	struct diocskerneldump_arg kda;
	const char *pubkeyfile;
	int ch;
	int i, fd;
	int do_listdumpdev = 0;
	bool enable, gzip;

	gzip = false;
	pubkeyfile = NULL;

	while ((ch = getopt(argc, argv, "k:lvz")) != -1)
		switch((char)ch) {
		case 'k':
			pubkeyfile = optarg;
			break;
		case 'l':
			do_listdumpdev = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'z':
			gzip = true;
			break;
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (do_listdumpdev) {
		listdumpdev();
		exit(EX_OK);
	}

	if (argc != 1)
		usage();

	enable = (strcmp(argv[0], "off") != 0);
#ifndef HAVE_CRYPTO
	if (pubkeyfile != NULL) {
		enable = false;
		warnx("Unable to use the public key. Recompile dumpon with OpenSSL support.");
	}
#endif

	if (enable) {
		char tmp[PATH_MAX];
		char *dumpdev;

		if (strncmp(argv[0], _PATH_DEV, sizeof(_PATH_DEV) - 1) == 0) {
			dumpdev = argv[0];
		} else {
			i = snprintf(tmp, PATH_MAX, "%s%s", _PATH_DEV, argv[0]);
			if (i < 0) {
				err(EX_OSERR, "%s", argv[0]);
			} else if (i >= PATH_MAX) {
				errno = EINVAL;
				err(EX_DATAERR, "%s", argv[0]);
			}
			dumpdev = tmp;
		}
		fd = open(dumpdev, O_RDONLY);
		if (fd < 0)
			err(EX_OSFILE, "%s", dumpdev);

		if (!gzip)
			check_size(fd, dumpdev);

		bzero(&kda, sizeof(kda));
		kda.kda_enable = 0;
		i = ioctl(fd, DIOCSKERNELDUMP, &kda);
		explicit_bzero(&kda, sizeof(kda));

#ifdef HAVE_CRYPTO
		if (pubkeyfile != NULL)
			genkey(pubkeyfile, &kda);
#endif

		kda.kda_enable = 1;
		kda.kda_compression = gzip ? KERNELDUMP_COMP_GZIP :
		    KERNELDUMP_COMP_NONE;
		i = ioctl(fd, DIOCSKERNELDUMP, &kda);
		explicit_bzero(kda.kda_encryptedkey, kda.kda_encryptedkeysize);
		free(kda.kda_encryptedkey);
		explicit_bzero(&kda, sizeof(kda));
		if (i == 0 && verbose)
			printf("kernel dumps on %s\n", dumpdev);
	} else {
		fd = open(_PATH_DEVNULL, O_RDONLY);
		if (fd < 0)
			err(EX_OSFILE, "%s", _PATH_DEVNULL);

		kda.kda_enable = 0;
		i = ioctl(fd, DIOCSKERNELDUMP, &kda);
		explicit_bzero(&kda, sizeof(kda));
		if (i == 0 && verbose)
			printf("kernel dumps disabled\n");
	}
	if (i < 0)
		err(EX_OSERR, "ioctl(DIOCSKERNELDUMP)");

	exit (0);
}
