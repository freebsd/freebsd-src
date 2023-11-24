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
#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/disk.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/wait.h>

#include <assert.h>
#include <capsicum_helpers.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <paths.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include <arpa/inet.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/netdump/netdump.h>

#ifdef HAVE_CRYPTO
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#endif

static int	verbose;

static void _Noreturn
usage(void)
{
	fprintf(stderr,
    "usage: dumpon [-i index] [-r] [-v] [-k <pubkey>] [-Zz] <device>\n"
    "       dumpon [-i index] [-r] [-v] [-k <pubkey>] [-Zz]\n"
    "              [-g <gateway>] -s <server> -c <client> <iface>\n"
    "       dumpon [-v] off\n"
    "       dumpon [-v] -l\n");
	exit(EX_USAGE);
}

/*
 * Look for a default route on the specified interface.
 */
static char *
find_gateway(const char *ifname)
{
	struct ifaddrs *ifa, *ifap;
	struct rt_msghdr *rtm;
	struct sockaddr *sa;
	struct sockaddr_dl *sdl;
	struct sockaddr_in *dst, *mask, *gw;
	char *buf, *next, *ret;
	size_t sz;
	int error, i, ifindex, mib[7];

	/* First look up the interface index. */
	if (getifaddrs(&ifap) != 0)
		err(EX_OSERR, "getifaddrs");
	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family != AF_LINK)
			continue;
		if (strcmp(ifa->ifa_name, ifname) == 0) {
			sdl = (struct sockaddr_dl *)(void *)ifa->ifa_addr;
			ifindex = sdl->sdl_index;
			break;
		}
	}
	if (ifa == NULL)
		errx(1, "couldn't find interface index for '%s'", ifname);
	freeifaddrs(ifap);

	/* Now get the IPv4 routing table. */
	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_DUMP;
	mib[5] = 0;
	mib[6] = -1; /* FIB */

	for (;;) {
		if (sysctl(mib, nitems(mib), NULL, &sz, NULL, 0) != 0)
			err(EX_OSERR, "sysctl(NET_RT_DUMP)");
		buf = malloc(sz);
		error = sysctl(mib, nitems(mib), buf, &sz, NULL, 0);
		if (error == 0)
			break;
		if (errno != ENOMEM)
			err(EX_OSERR, "sysctl(NET_RT_DUMP)");
		free(buf);
	}

	ret = NULL;
	for (next = buf; next < buf + sz; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)(void *)next;
		if (rtm->rtm_version != RTM_VERSION)
			continue;
		if ((rtm->rtm_flags & RTF_GATEWAY) == 0 ||
		    rtm->rtm_index != ifindex)
			continue;

		dst = gw = mask = NULL;
		sa = (struct sockaddr *)(rtm + 1);
		for (i = 0; i < RTAX_MAX; i++) {
			if ((rtm->rtm_addrs & (1 << i)) != 0) {
				switch (i) {
				case RTAX_DST:
					dst = (void *)sa;
					break;
				case RTAX_GATEWAY:
					gw = (void *)sa;
					break;
				case RTAX_NETMASK:
					mask = (void *)sa;
					break;
				}
			}
			sa = (struct sockaddr *)((char *)sa + SA_SIZE(sa));
		}

		if (dst->sin_addr.s_addr == INADDR_ANY &&
		    mask->sin_addr.s_addr == 0) {
			ret = inet_ntoa(gw->sin_addr);
			break;
		}
	}
	free(buf);
	return (ret);
}

static void
check_link_status(const char *ifname)
{
	struct ifaddrs *ifap, *ifa;

	if (getifaddrs(&ifap) != 0)
		err(EX_OSERR, "getifaddrs");

	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		if (strcmp(ifname, ifa->ifa_name) != 0)
			continue;
		if ((ifa->ifa_flags & IFF_UP) == 0) {
			warnx("warning: %s's link is down", ifname);
		}
		break;
	}
	freeifaddrs(ifap);
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
	if ((uintmax_t)mediasize < (uintmax_t)physmem)
		errx(EX_IOERR, "%s is smaller than physical memory", fn);
}

#ifdef HAVE_CRYPTO
static void
_genkey(const char *pubkeyfile, struct diocskerneldump_arg *kdap)
{
	FILE *fp;
	RSA *pubkey;

	assert(pubkeyfile != NULL);
	assert(kdap != NULL);

	fp = NULL;
	pubkey = NULL;

	fp = fopen(pubkeyfile, "r");
	if (fp == NULL)
		err(1, "Unable to open %s", pubkeyfile);

	/*
	 * Obsolescent OpenSSL only knows about /dev/random, and needs to
	 * pre-seed before entering cap mode.  For whatever reason,
	 * RSA_pub_encrypt uses the internal PRNG.
	 */
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	{
		unsigned char c[1];
		RAND_bytes(c, 1);
	}
#endif

	if (caph_enter() < 0)
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
		errx(1, "Unable to read data from %s: %s", pubkeyfile,
		    ERR_error_string(ERR_get_error(), NULL));

	/*
	 * RSA keys under ~1024 bits are trivially factorable (2018).  OpenSSL
	 * provides an API for RSA keys to estimate the symmetric-cipher
	 * "equivalent" bits of security (defined in NIST SP800-57), which as
	 * of this writing equates a 2048-bit RSA key to 112 symmetric cipher
	 * bits.
	 *
	 * Use this API as a seatbelt to avoid suggesting to users that their
	 * privacy is protected by encryption when the key size is insufficient
	 * to prevent compromise via factoring.
	 *
	 * Future work: Sanity check for weak 'e', and sanity check for absence
	 * of 'd' (i.e., the supplied key is a public key rather than a full
	 * keypair).
	 */
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	if (RSA_security_bits(pubkey) < 112)
#else
	if (RSA_size(pubkey) * 8 < 2048)
#endif
		errx(1, "Small RSA keys (you provided: %db) can be "
		    "factored cheaply.  Please generate a larger key.",
		    RSA_size(pubkey) * 8);

	kdap->kda_encryptedkeysize = RSA_size(pubkey);
	if (kdap->kda_encryptedkeysize > KERNELDUMP_ENCKEY_MAX_SIZE) {
		errx(1, "Public key has to be at most %db long.",
		    8 * KERNELDUMP_ENCKEY_MAX_SIZE);
	}

	kdap->kda_encryptedkey = calloc(1, kdap->kda_encryptedkeysize);
	if (kdap->kda_encryptedkey == NULL)
		err(1, "Unable to allocate encrypted key");

	/*
	 * If no cipher was specified, choose a reasonable default.
	 */
	if (kdap->kda_encryption == KERNELDUMP_ENC_NONE)
		kdap->kda_encryption = KERNELDUMP_ENC_CHACHA20;
	else if (kdap->kda_encryption == KERNELDUMP_ENC_AES_256_CBC &&
	    kdap->kda_compression != KERNELDUMP_COMP_NONE)
		errx(EX_USAGE, "Unpadded AES256-CBC mode cannot be used "
		    "with compression.");

	arc4random_buf(kdap->kda_key, sizeof(kdap->kda_key));
	if (RSA_public_encrypt(sizeof(kdap->kda_key), kdap->kda_key,
	    kdap->kda_encryptedkey, pubkey,
	    RSA_PKCS1_OAEP_PADDING) != (int)kdap->kda_encryptedkeysize) {
		errx(1, "Unable to encrypt the one-time key: %s",
		    ERR_error_string(ERR_get_error(), NULL));
	}
	RSA_free(pubkey);
}

/*
 * Run genkey() in a child so it can use capability mode without affecting
 * the rest of the runtime.
 */
static void
genkey(const char *pubkeyfile, struct diocskerneldump_arg *kdap)
{
	pid_t pid;
	int error, filedes[2], status;
	ssize_t bytes;

	if (pipe2(filedes, O_CLOEXEC) != 0)
		err(1, "pipe");
	pid = fork();
	switch (pid) {
	case -1:
		err(1, "fork");
		break;
	case 0:
		close(filedes[0]);
		_genkey(pubkeyfile, kdap);
		/* Write the new kdap back to the parent. */
		bytes = write(filedes[1], kdap, sizeof(*kdap));
		if (bytes != sizeof(*kdap))
			err(1, "genkey pipe write");
		bytes = write(filedes[1], kdap->kda_encryptedkey,
		    kdap->kda_encryptedkeysize);
		if (bytes != (ssize_t)kdap->kda_encryptedkeysize)
			err(1, "genkey pipe write kda_encryptedkey");
		_exit(0);
	}
	close(filedes[1]);
	/* Read in the child's genkey() result into kdap. */
	bytes = read(filedes[0], kdap, sizeof(*kdap));
	if (bytes != sizeof(*kdap))
		errx(1, "genkey pipe read");
	if (kdap->kda_encryptedkeysize > KERNELDUMP_ENCKEY_MAX_SIZE)
		errx(1, "Public key has to be at most %db long.",
		    8 * KERNELDUMP_ENCKEY_MAX_SIZE);
	kdap->kda_encryptedkey = calloc(1, kdap->kda_encryptedkeysize);
	if (kdap->kda_encryptedkey == NULL)
		err(1, "Unable to allocate encrypted key");
	bytes = read(filedes[0], kdap->kda_encryptedkey,
	    kdap->kda_encryptedkeysize);
	if (bytes != (ssize_t)kdap->kda_encryptedkeysize)
		errx(1, "genkey pipe read kda_encryptedkey");
	error = waitpid(pid, &status, WEXITED);
	if (error == -1)
		err(1, "waitpid");
	if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
		errx(1, "genkey child exited with status %d",
		    WEXITSTATUS(status));
	else if (WIFSIGNALED(status))
		errx(1, "genkey child exited with signal %d",
		    WTERMSIG(status));
	close(filedes[0]);
}
#endif

static void
listdumpdev(void)
{
	static char ip[200];

	char dumpdev[PATH_MAX];
	struct diocskerneldump_arg ndconf;
	size_t len;
	const char *sysctlname = "kern.shutdown.dumpdevname";
	int fd;

	len = sizeof(dumpdev);
	if (sysctlbyname(sysctlname, &dumpdev, &len, NULL, 0) != 0) {
		if (errno == ENOMEM) {
			err(EX_OSERR, "Kernel returned too large of a buffer for '%s'\n",
				sysctlname);
		} else {
			err(EX_OSERR, "Sysctl get '%s'\n", sysctlname);
		}
	}
	if (strlen(dumpdev) == 0)
		(void)strlcpy(dumpdev, _PATH_DEVNULL, sizeof(dumpdev));

	if (verbose) {
		char *ctx, *dd;
		unsigned idx;

		printf("kernel dumps on priority: device\n");
		idx = 0;
		ctx = dumpdev;
		while ((dd = strsep(&ctx, ",")) != NULL)
			printf("%u: %s\n", idx++, dd);
	} else
		printf("%s\n", dumpdev);

	/* If netdump is enabled, print the configuration parameters. */
	if (verbose) {
		fd = open(_PATH_NETDUMP, O_RDONLY);
		if (fd < 0) {
			if (errno != ENOENT)
				err(EX_OSERR, "opening %s", _PATH_NETDUMP);
			return;
		}
		if (ioctl(fd, DIOCGKERNELDUMP, &ndconf) != 0) {
			if (errno != ENXIO)
				err(EX_OSERR, "ioctl(DIOCGKERNELDUMP)");
			(void)close(fd);
			return;
		}

		printf("server address: %s\n",
		    inet_ntop(ndconf.kda_af, &ndconf.kda_server, ip,
			sizeof(ip)));
		printf("client address: %s\n",
		    inet_ntop(ndconf.kda_af, &ndconf.kda_client, ip,
			sizeof(ip)));
		printf("gateway address: %s\n",
		    inet_ntop(ndconf.kda_af, &ndconf.kda_gateway, ip,
			sizeof(ip)));
		(void)close(fd);
	}
}

static int
opendumpdev(const char *arg, char *dumpdev)
{
	int fd, i;

	if (strncmp(arg, _PATH_DEV, sizeof(_PATH_DEV) - 1) == 0)
		strlcpy(dumpdev, arg, PATH_MAX);
	else {
		i = snprintf(dumpdev, PATH_MAX, "%s%s", _PATH_DEV, arg);
		if (i < 0)
			err(EX_OSERR, "%s", arg);
		if (i >= PATH_MAX)
			errc(EX_DATAERR, EINVAL, "%s", arg);
	}

	fd = open(dumpdev, O_RDONLY);
	if (fd < 0)
		err(EX_OSFILE, "%s", dumpdev);
	return (fd);
}

int
main(int argc, char *argv[])
{
	char dumpdev[PATH_MAX];
	struct diocskerneldump_arg ndconf, *kdap;
	struct addrinfo hints, *res;
	const char *dev, *pubkeyfile, *server, *client, *gateway;
	int ch, error, fd, cipher;
	bool gzip, list, netdump, zstd, insert, rflag;
	uint8_t ins_idx;

	gzip = list = netdump = zstd = insert = rflag = false;
	kdap = NULL;
	pubkeyfile = NULL;
	server = client = gateway = NULL;
	ins_idx = KDA_APPEND;
	cipher = KERNELDUMP_ENC_NONE;

	while ((ch = getopt(argc, argv, "C:c:g:i:k:lrs:vZz")) != -1)
		switch ((char)ch) {
		case 'C':
			if (strcasecmp(optarg, "chacha") == 0 ||
			    strcasecmp(optarg, "chacha20") == 0)
				cipher = KERNELDUMP_ENC_CHACHA20;
			else if (strcasecmp(optarg, "aes-cbc") == 0 ||
			    strcasecmp(optarg, "aes256-cbc") == 0)
				cipher = KERNELDUMP_ENC_AES_256_CBC;
			else
				errx(EX_USAGE, "Unrecognized cipher algorithm "
				    "'%s'", optarg);
			break;
		case 'c':
			client = optarg;
			break;
		case 'g':
			gateway = optarg;
			break;
		case 'i':
			{
			int i;

			i = atoi(optarg);
			if (i < 0 || i >= KDA_APPEND - 1)
				errx(EX_USAGE,
				    "-i index must be between zero and %d.",
				    (int)KDA_APPEND - 2);
			insert = true;
			ins_idx = i;
			}
			break;
		case 'k':
			pubkeyfile = optarg;
			break;
		case 'l':
			list = true;
			break;
		case 'r':
			rflag = true;
			break;
		case 's':
			server = optarg;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'Z':
			zstd = true;
			break;
		case 'z':
			gzip = true;
			break;
		default:
			usage();
		}

	if (gzip && zstd)
		errx(EX_USAGE, "The -z and -Z options are mutually exclusive.");

	if (insert && rflag)
		errx(EX_USAGE, "The -i and -r options are mutually exclusive.");

	argc -= optind;
	argv += optind;

	if (list) {
		listdumpdev();
		exit(EX_OK);
	}

	if (argc != 1)
		usage();

#ifdef HAVE_CRYPTO
	if (cipher != KERNELDUMP_ENC_NONE && pubkeyfile == NULL) {
		errx(EX_USAGE, "-C option requires a public key file.");
	} else if (pubkeyfile != NULL) {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
		ERR_load_crypto_strings();
#else
		if (!OPENSSL_init_crypto(0, NULL))
			errx(EX_UNAVAILABLE, "Unable to initialize OpenSSL");
#endif
	}
#else
	if (pubkeyfile != NULL)
		errx(EX_UNAVAILABLE,"Unable to use the public key."
				    " Recompile dumpon with OpenSSL support.");
#endif

	if (server != NULL && client != NULL) {
		dev = _PATH_NETDUMP;
		netdump = true;
	} else if (server == NULL && client == NULL && argc > 0) {
		if (strcmp(argv[0], "off") == 0) {
			rflag = true;
			dev = _PATH_DEVNULL;
		} else
			dev = argv[0];
		netdump = false;

		if (strcmp(dev, _PATH_DEVNULL) == 0) {
			/*
			 * Netdump has its own configuration tracking that
			 * is not removed when using /dev/null.
			 */
			fd = open(_PATH_NETDUMP, O_RDONLY);
			if (fd != -1) {
				bzero(&ndconf, sizeof(ndconf));
				ndconf.kda_index = KDA_REMOVE_ALL;
				ndconf.kda_af = AF_INET;
				error = ioctl(fd, DIOCSKERNELDUMP, &ndconf);
				if (error != 0)
					err(1, "ioctl(%s, DIOCSKERNELDUMP)",
					    _PATH_NETDUMP);
				close(fd);
			}
		}
	} else
		usage();

	fd = opendumpdev(dev, dumpdev);
	if (!netdump && !gzip && !zstd && !rflag)
		check_size(fd, dumpdev);

	kdap = &ndconf;
	bzero(kdap, sizeof(*kdap));

	if (rflag)
		kdap->kda_index = KDA_REMOVE;
	else
		kdap->kda_index = ins_idx;

	kdap->kda_compression = KERNELDUMP_COMP_NONE;
	if (zstd)
		kdap->kda_compression = KERNELDUMP_COMP_ZSTD;
	else if (gzip)
		kdap->kda_compression = KERNELDUMP_COMP_GZIP;

	if (netdump) {
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_protocol = IPPROTO_UDP;
		res = NULL;
		error = getaddrinfo(server, NULL, &hints, &res);
		if (error != 0) {
			if (error == EAI_SYSTEM)
				err(EX_OSERR, "%s", gai_strerror(error));
			errx(EX_NOHOST, "%s", gai_strerror(error));
		}
		server = inet_ntoa(
		    ((struct sockaddr_in *)(void *)res->ai_addr)->sin_addr);
		freeaddrinfo(res);

		if (strlcpy(ndconf.kda_iface, argv[0],
		    sizeof(ndconf.kda_iface)) >= sizeof(ndconf.kda_iface))
			errx(EX_USAGE, "invalid interface name '%s'", argv[0]);
		if (inet_aton(server, &ndconf.kda_server.in4) == 0)
			errx(EX_USAGE, "invalid server address '%s'", server);
		if (inet_aton(client, &ndconf.kda_client.in4) == 0)
			errx(EX_USAGE, "invalid client address '%s'", client);

		if (gateway == NULL) {
			gateway = find_gateway(argv[0]);
			if (gateway == NULL) {
				if (verbose)
					printf(
				    "failed to look up gateway for %s\n",
					    server);
				gateway = server;
			}
		}
		if (inet_aton(gateway, &ndconf.kda_gateway.in4) == 0)
			errx(EX_USAGE, "invalid gateway address '%s'", gateway);
		ndconf.kda_af = AF_INET;
	}

#ifdef HAVE_CRYPTO
	if (pubkeyfile != NULL) {
		kdap->kda_encryption = cipher;
		genkey(pubkeyfile, kdap);
	}
#endif
	error = ioctl(fd, DIOCSKERNELDUMP, kdap);
	if (error != 0)
		error = errno;
	if (error == EINVAL && (gzip || zstd)) {
		/* Retry without compression in case kernel lacks support. */
		kdap->kda_compression = KERNELDUMP_COMP_NONE;
		error = ioctl(fd, DIOCSKERNELDUMP, kdap);
		if (error == 0)
			warnx("Compression disabled; kernel may lack gzip or zstd support.");
		else
			error = errno;
	}
	/* Emit a warning if the user configured a downed interface. */
	if (error == 0 && netdump)
		check_link_status(kdap->kda_iface);
	explicit_bzero(kdap->kda_encryptedkey, kdap->kda_encryptedkeysize);
	free(kdap->kda_encryptedkey);
	explicit_bzero(kdap, sizeof(*kdap));
	if (error != 0) {
		if (netdump) {
			/*
			 * Be slightly less user-hostile for some common
			 * errors, especially as users don't have any great
			 * discoverability into which NICs support netdump.
			 */
			if (error == ENODEV)
				errx(EX_OSERR, "Unable to configure netdump "
				    "because the interface driver does not yet "
				    "support netdump.");
		}
		errc(EX_OSERR, error, "ioctl(DIOCSKERNELDUMP)");
	}

	if (verbose)
		listdumpdev();

	exit(EX_OK);
}
