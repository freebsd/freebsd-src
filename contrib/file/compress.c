/*
 * compress routines:
 *	zmagic() - returns 0 if not recognized, uncompresses and prints
 *		   information if recognized
 *	uncompress(method, old, n, newch) - uncompress old into new, 
 *					    using method, return sizeof new
 */
#include "file.h"
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_LIBZ
#include <zlib.h>
#endif

#ifndef lint
FILE_RCSID("@(#)$Id: compress.c,v 1.25 2002/07/03 18:26:37 christos Exp $")
#endif


static struct {
	const char *magic;
	int   maglen;
	const char *const argv[3];
	int	 silent;
} compr[] = {
	{ "\037\235", 2, { "gzip", "-cdq", NULL }, 1 },		/* compressed */
	/* Uncompress can get stuck; so use gzip first if we have it
	 * Idea from Damien Clark, thanks! */
	{ "\037\235", 2, { "uncompress", "-c", NULL }, 1 },	/* compressed */
	{ "\037\213", 2, { "gzip", "-cdq", NULL }, 1 },		/* gzipped */
	{ "\037\236", 2, { "gzip", "-cdq", NULL }, 1 },		/* frozen */
	{ "\037\240", 2, { "gzip", "-cdq", NULL }, 1 },		/* SCO LZH */
	/* the standard pack utilities do not accept standard input */
	{ "\037\036", 2, { "gzip", "-cdq", NULL }, 0 },		/* packed */
	{ "BZh",      3, { "bzip2", "-cd", NULL }, 1 },		/* bzip2-ed */
};

static int ncompr = sizeof(compr) / sizeof(compr[0]);


static int swrite(int, const void *, size_t);
static int sread(int, void *, size_t);
static int uncompressbuf(int, const unsigned char *, unsigned char **, int);
#ifdef HAVE_LIBZ
static int uncompressgzipped(const unsigned char *, unsigned char **, int);
#endif

int
zmagic(const char *fname, unsigned char *buf, int nbytes)
{
	unsigned char *newbuf;
	int newsize;
	int i;

	for (i = 0; i < ncompr; i++) {
		if (nbytes < compr[i].maglen)
			continue;
		if (memcmp(buf, compr[i].magic, compr[i].maglen) == 0 &&
		    (newsize = uncompressbuf(i, buf, &newbuf, nbytes)) != 0) {
			tryit(fname, newbuf, newsize, 1);
			free(newbuf);
			printf(" (");
			tryit(fname, buf, nbytes, 0);
			printf(")");
			return 1;
		}
	}

	if (i == ncompr)
		return 0;

	return 1;
}

/*
 * `safe' write for sockets and pipes.
 */
static int
swrite(int fd, const void *buf, size_t n)
{
	int rv;
	size_t rn = n;

	do
		switch (rv = write(fd, buf, n)) {
		case -1:
			if (errno == EINTR)
				continue;
			return -1;
		default:
			n -= rv;
			buf = ((const char *)buf) + rv;
			break;
		}
	while (n > 0);
	return rn;
}


/*
 * `safe' read for sockets and pipes.
 */
static int
sread(int fd, void *buf, size_t n)
{
	int rv;
	size_t rn = n;

	do
		switch (rv = read(fd, buf, n)) {
		case -1:
			if (errno == EINTR)
				continue;
			return -1;
		case 0:
			return rn - n;
		default:
			n -= rv;
			buf = ((char *)buf) + rv;
			break;
		}
	while (n > 0);
	return rn;
}

int
pipe2file(int fd, void *startbuf, size_t nbytes)
{
	char buf[4096];
	int r, tfd;

	(void)strcpy(buf, "/tmp/file.XXXXXX");
#ifndef HAVE_MKSTEMP
	{
		char *ptr = mktemp(buf);
		tfd = open(ptr, O_RDWR|O_TRUNC|O_EXCL|O_CREAT, 0600);
		r = errno;
		(void)unlink(ptr);
		errno = r;
	}
#else
	tfd = mkstemp(buf);
	r = errno;
	(void)unlink(buf);
	errno = r;
#endif
	if (tfd == -1) {
		error("Can't create temporary file for pipe copy (%s)\n",
		    strerror(errno));
		/*NOTREACHED*/
	}

	if (swrite(tfd, startbuf, nbytes) != nbytes)
		r = 1;
	else {
		while ((r = sread(fd, buf, sizeof(buf))) > 0)
			if (swrite(tfd, buf, r) != r)
				break;
	}

	switch (r) {
	case -1:
		error("Error copying from pipe to temp file (%s)\n",
		    strerror(errno));
		/*NOTREACHED*/
	case 0:
		break;
	default:
		error("Error while writing to temp file (%s)\n",
		    strerror(errno));
		/*NOTREACHED*/
	}

	/*
	 * We duplicate the file descriptor, because fclose on a
	 * tmpfile will delete the file, but any open descriptors
	 * can still access the phantom inode.
	 */
	if ((fd = dup2(tfd, fd)) == -1) {
		error("Couldn't dup destcriptor for temp file(%s)\n",
		    strerror(errno));
		/*NOTREACHED*/
	}
	(void)close(tfd);
	if (lseek(fd, (off_t)0, SEEK_SET) == (off_t)-1) {
		error("Couldn't seek on temp file (%s)\n", strerror(errno));
		/*NOTREACHED*/
	}
	return fd;
}

#ifdef HAVE_LIBZ

#define FHCRC		(1 << 1)
#define FEXTRA		(1 << 2)
#define FNAME		(1 << 3)
#define FCOMMENT	(1 << 4)

static int
uncompressgzipped(const unsigned char *old, unsigned char **newch, int n)
{
	unsigned char flg = old[3];
	int data_start = 10;
	z_stream z;
	int rc;

	if (flg & FEXTRA)
		data_start += 2 + old[data_start] + old[data_start + 1] * 256;
	if (flg & FNAME) {
		while(old[data_start])
			data_start++;
		data_start++;
	}
	if(flg & FCOMMENT) {
		while(old[data_start])
			data_start++;
		data_start++;
	}
	if(flg & FHCRC)
		data_start += 2;

	if ((*newch = (unsigned char *)malloc(HOWMANY + 1)) == NULL) {
		return 0;
	}
	
	z.next_in = (Bytef *)(old + data_start);
	z.avail_in = n - data_start;
	z.next_out = *newch;
	z.avail_out = HOWMANY;
	z.zalloc = Z_NULL;
	z.zfree = Z_NULL;
	z.opaque = Z_NULL;

	rc = inflateInit2(&z, -15);
	if (rc != Z_OK) {
		(void) fprintf(stderr,"%s: zlib: %s\n", progname, z.msg);
		return 0;
	}

	rc = inflate(&z, Z_SYNC_FLUSH);
	if (rc != Z_OK && rc != Z_STREAM_END) {
		fprintf(stderr,"%s: zlib: %s\n", progname, z.msg);
		return 0;
	}

	n = z.total_out;
	inflateEnd(&z);
	
	/* let's keep the nul-terminate tradition */
	(*newch)[n++] = '\0';

	return n;
}
#endif

static int
uncompressbuf(int method, const unsigned char *old, unsigned char **newch,
	      int n)
{
	int fdin[2], fdout[2];

	/* The buffer is NUL terminated, and we don't need that. */
	n--;
	 
#ifdef HAVE_LIBZ
	if (method == 2)
		return uncompressgzipped(old,newch,n);
#endif

	if (pipe(fdin) == -1 || pipe(fdout) == -1) {
		error("cannot create pipe (%s).\n", strerror(errno));	
		/*NOTREACHED*/
	}
	switch (fork()) {
	case 0:	/* child */
		(void) close(0);
		(void) dup(fdin[0]);
		(void) close(fdin[0]);
		(void) close(fdin[1]);

		(void) close(1);
		(void) dup(fdout[1]);
		(void) close(fdout[0]);
		(void) close(fdout[1]);
		if (compr[method].silent)
			(void) close(2);

		execvp(compr[method].argv[0],
		       (char *const *)compr[method].argv);
		exit(1);
		/*NOTREACHED*/
	case -1:
		error("could not fork (%s).\n", strerror(errno));
		/*NOTREACHED*/

	default: /* parent */
		(void) close(fdin[0]);
		(void) close(fdout[1]);
		if (swrite(fdin[1], old, n) != n) {
			n = 0;
			goto err;
		}
		(void) close(fdin[1]);
		fdin[1] = -1;
		if ((*newch = (unsigned char *) malloc(HOWMANY + 1)) == NULL) {
			n = 0;
			goto err;
		}
		if ((n = sread(fdout[0], *newch, HOWMANY)) <= 0) {
			free(*newch);
			n = 0;
			goto err;
		}
 		/* NUL terminate, as every buffer is handled here. */
 		(*newch)[n++] = '\0';
err:
		if (fdin[1] != -1)
			(void) close(fdin[1]);
		(void) close(fdout[0]);
		(void) wait(NULL);
		return n;
	}
}
