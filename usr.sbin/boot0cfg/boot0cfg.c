/*
 * Copyright (c) 1999 Robert Nordier
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
	"$Id: boot0cfg.c,v 1.3 1999/02/26 14:57:17 rnordier Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/disklabel.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MBRSIZE         512     /* master boot record size */

#define OFF_DRIVE	0x1ba	/* offset: setdrv drive */
#define OFF_FLAGS       0x1bb   /* offset: option flags */
#define OFF_TICKS       0x1bc   /* offset: clock ticks */
#define OFF_PTBL        0x1be   /* offset: partition table */
#define OFF_MAGIC       0x1fe   /* offset: magic number */

#define cv2(p)  ((p)[0] | (p)[1] << 010)

#define mk2(p, x)                               \
    (p)[0] = (u_int8_t)(x),                     \
    (p)[1] = (u_int8_t)((x) >> 010)

static const struct {
    const char *tok;
    int def;
} opttbl[] = {
    {"packet", 0},
    {"update", 1},
    {"setdrv", 0}
};
static const int nopt = sizeof(opttbl) / sizeof(opttbl[0]);

static const char fmt0[] = "#   flag     start chs   type"
    "       end chs       offset         size\n";

static const char fmt1[] = "%d   0x%02x   %4u:%3u:%2u   0x%02x"
    "   %4u:%3u:%2u   %10u   %10u\n";

static void stropt(const char *, int *, int *);
static char *mkrdev(const char *);
static int argtoi(const char *, int, int, int);
static void usage(void);

int
main(int argc, char *argv[])
{
    u_int8_t buf[MBRSIZE];
    struct dos_partition part[4];
    const char *bpath, *fpath, *disk;
    ssize_t n;
    int B_flag, v_flag, o_flag;
    int d_arg, m_arg, t_arg;
    int o_and, o_or;
    int fd, fd1, up, c, i;

    bpath = "/boot/boot0";
    fpath = NULL;
    B_flag = v_flag = o_flag = 0;
    d_arg = m_arg = t_arg = -1;
    o_and = 0xff;
    o_or = 0;
    while ((c = getopt(argc, argv, "Bvb:d:f:m:o:t:")) != -1)
        switch (c) {
        case 'B':
            B_flag = 1;
            break;
        case 'v':
            v_flag = 1;
            break;
        case 'b':
            bpath = optarg;
            break;
        case 'd':
            d_arg = argtoi(optarg, 0, 0xff, 'd');
            break;
        case 'f':
            fpath = optarg;
            break;
        case 'm':
            m_arg = argtoi(optarg, 0, 0xf, 'm');
            break;
        case 'o':
            stropt(optarg, &o_and, &o_or);
            o_flag = 1;
            break;
        case 't':
            t_arg = argtoi(optarg, 1, 0xffff, 't');
            break;
        default:
            usage();
        }
    argc -= optind;
    argv += optind;
    if (argc != 1)
        usage();
    disk = mkrdev(*argv);
    up = B_flag || d_arg != -1 || o_flag || t_arg != -1;
    if ((fd = open(disk, up ? O_RDWR : O_RDONLY)) == -1)
        err(1, "%s", disk);
    if ((n = read(fd, buf, MBRSIZE)) == -1)
        err(1, "%s", disk);
    if (n != MBRSIZE)
        errx(1, "%s: short read", disk);
    if (cv2(buf + OFF_MAGIC) != 0xaa55)
        errx(1, "%s: bad magic", disk);
    if (fpath) {
        if ((fd1 = open(fpath, O_WRONLY | O_CREAT | O_TRUNC,
                        0666)) == -1 ||
            (n = write(fd1, buf, MBRSIZE)) == -1 || close(fd1))
            err(1, "%s", fpath);
        if (n != MBRSIZE)
            errx(1, "%s: short write", fpath);
    }
    memcpy(part, buf + OFF_PTBL, sizeof(part));
    if (B_flag) {
        if ((fd1 = open(bpath, O_RDONLY)) == -1 ||
            (n = read(fd1, buf, MBRSIZE)) == -1 || close(fd1))
            err(1, "%s", bpath);
        if (n != MBRSIZE)
            errx(1, "%s: short read", bpath);
        if (cv2(buf + OFF_MAGIC) != 0xaa55)
            errx(1, "%s: bad magic", bpath);
        memcpy(buf + OFF_PTBL, part, sizeof(part));
    }
    if (d_arg != -1)
	buf[OFF_DRIVE] = d_arg;
    if (m_arg != -1) {
	buf[OFF_FLAGS] &= 0xf0;
	buf[OFF_FLAGS] |= m_arg;
    }
    if (o_flag) {
        buf[OFF_FLAGS] &= o_and;
        buf[OFF_FLAGS] |= o_or;
    }
    if (t_arg != -1)
        mk2(buf + OFF_TICKS, t_arg);
    if (up) {
        if (lseek(fd, 0, SEEK_SET) == -1 ||
            (n = write(fd, buf, MBRSIZE)) == -1 || close(fd))
            err(1, "%s", disk);
        if (n != MBRSIZE)
            errx(1, "%s: short write", disk);
    }
    if (v_flag) {
        printf(fmt0);
        for (i = 0; i < 4; i++)
            if (part[i].dp_typ) {
                printf(fmt1,
                       1 + i,
                       part[i].dp_flag,
                  part[i].dp_scyl + ((part[i].dp_ssect & 0xc0) << 2),
                       part[i].dp_shd,
                       part[i].dp_ssect & 0x3f,
                       part[i].dp_typ,
                  part[i].dp_ecyl + ((part[i].dp_esect & 0xc0) << 2),
                       part[i].dp_ehd,
                       part[i].dp_esect & 0x3f,
                       part[i].dp_start,
                       part[i].dp_size);
            }
        printf("\n");
        printf("drive=0x%x  mask=0x%x  options=", buf[OFF_DRIVE],
	       buf[OFF_FLAGS] & 0xf);
        for (i = 0; i < nopt; i++) {
            if (i)
                printf(",");
            if (!(buf[OFF_FLAGS] & 1 << (7 - i)) ^ opttbl[i].def)
                printf("no");
            printf("%s", opttbl[i].tok);
        }
        printf("  ticks=%u\n", cv2(buf + OFF_TICKS));
    }
    return 0;
}

static void
stropt(const char *arg, int *xa, int *xo)
{
    const char *q;
    char *s, *s1;
    int inv, i, x;

    if (!(s = strdup(arg)))
        err(1, NULL);
    for (s1 = s; (q = strtok(s1, ",")); s1 = NULL) {
        if ((inv = !strncmp(q, "no", 2)))
            q += 2;
        for (i = 0; i < nopt; i++)
            if (!strcmp(q, opttbl[i].tok))
                break;
        if (i == nopt)
            errx(1, "%s: Unknown -o option", q);
        if (opttbl[i].def)
            inv ^= 1;
        x = 1 << (7 - i);
        if (inv)
            *xa &= ~x;
        else
            *xo |= x;
    }
    free(s);
}

static char *
mkrdev(const char *fname)
{
    char buf[MAXPATHLEN];
    struct stat sb;
    char *s;

    s = (char *) fname;
    if (!strchr(fname, '/')) {
        snprintf(buf, sizeof(buf), "%sr%s", _PATH_DEV, fname);
        if (stat(buf, &sb))
            snprintf(buf, sizeof(buf), "%s%s", _PATH_DEV, fname);
        if (!(s = strdup(buf)))
            err(1, NULL);
    }
    return s;
}

static int
argtoi(const char *arg, int lo, int hi, int opt)
{
    char *s;
    long x;

    errno = 0;
    x = strtol(arg, &s, 0);
    if (errno || !*arg || *s || x < lo || x > hi)
        errx(1, "%s: Bad argument to -%c option", arg, opt);
    return x;
}

static void
usage(void)
{
    fprintf(stderr, "%s\n%s\n",
    "usage: boot0cfg [-Bv] [-b boot0] [-d drive] [-f file] [-m mask]",
    "                [-o options] [-t ticks] disk");
    exit(1);
}
