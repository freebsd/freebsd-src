/*
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* To build a stand-alone command-line utility to create and dismantal
 * these theme files, build this thusly:
 *
 * gcc -I../ -DNSTHEME -o themetool container.c
 *
 * [needs a c99 compiler]
 *
 * then for instance to create a theme file called mythemefilename
 * ./themetool --verbose --create -n"My theme name" mythemefilename\
 * --author "Myname" /path/to/directory/containing/theme/files/
 */

/** \file
 * Container format handling for themes etc. */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include "utils/config.h"
#include "utils/container.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"

#ifdef WITH_MMAP
#include <sys/mman.h>
#endif

#ifdef NSTHEME
bool verbose_log = true;
#endif

struct container_dirent {
	unsigned char	filename[64];
	u_int32_t	startoffset;
	u_int32_t	len;
	u_int32_t	flags1;
	u_int32_t	flags2;
};

struct container_header {
	u_int32_t	magic;	/* 0x4d54534e little endian */
	u_int32_t	parser;
	unsigned char	name[32];
	unsigned char	author[64];
	u_int32_t	diroffset;
};

struct container_ctx {
	FILE		*fh;
	bool		creating;
	bool		processed;
	struct container_header	header;
	unsigned int	entries;
	unsigned char 	*data;
	struct container_dirent *directory;
};

inline static size_t container_filelen(FILE *fd)
{
	long o;
	long a;

	o = ftell(fd);
	if (o == -1) {
		LOG(("Could not get current stream position"));
		return 0;
	}

	fseek(fd, 0, SEEK_END);
	a = ftell(fd);

	fseek(fd, o, SEEK_SET);
	if (a == -1) {
		LOG(("could not ascertain size of file in theme container; omitting"));
		return 0;
	}
	if (((unsigned long) a) > SIZE_MAX) {
		LOG(("overlarge file in theme container; possible truncation"));
		return SIZE_MAX;
	}
	return (size_t) a;
}

static void container_add_to_dir(struct container_ctx *ctx,
					const unsigned char *entryname,
					const u_int32_t offset,
					const u_int32_t length)
{
	struct container_dirent *temp;
	temp = realloc(ctx->directory, ctx->entries * 
			sizeof(struct container_dirent));
	if (temp == NULL) {
		printf("error adding entry for %s to theme container\n", entryname);
		return;
	}
	ctx->entries += 1;
	ctx->directory = temp;

	strncpy((char *)ctx->directory[ctx->entries - 1].filename,
				(char *)entryname, sizeof(ctx->directory[
				ctx->entries - 1].filename));
	ctx->directory[ctx->entries - 1].startoffset = offset;
	ctx->directory[ctx->entries - 1].len = length;
	ctx->directory[ctx->entries - 1].flags1 = 0;
	ctx->directory[ctx->entries - 1].flags2 = 0;
}

struct container_ctx *container_open(const char *filename)
{
	size_t val;
	struct container_ctx *ctx = calloc(sizeof(struct container_ctx), 1);

	ctx->fh = fopen(filename, "rb");

	if (ctx->fh == NULL) {
		free(ctx);
		return NULL;
	}

	/* we don't actually load any of the data (including directory)
	 * until we need to, such that _get_name and _get_author are as quick
	 * as possible.  When we have, this gets set to true.
	 */
	ctx->processed = false;

	val = fread(&ctx->header.magic, 4, 1, ctx->fh);
	if (val == 0)
		LOG(("empty read magic"));
	ctx->header.magic = ntohl(ctx->header.magic);

	val = fread(&ctx->header.parser, 4, 1, ctx->fh);
	if (val == 0)
		LOG(("empty read parser"));	
	ctx->header.parser = ntohl(ctx->header.parser);

	val = fread(ctx->header.name, 32, 1, ctx->fh);
	if (val == 0)
		LOG(("empty read name"));
	val = fread(ctx->header.author, 64, 1, ctx->fh);
	if (val == 0)
		LOG(("empty read author"));

	val = fread(&ctx->header.diroffset, 4, 1, ctx->fh);
	if (val == 0)
		LOG(("empty read diroffset"));
	ctx->header.diroffset = ntohl(ctx->header.diroffset);

	if (ctx->header.magic != 0x4e53544d || ctx->header.parser != 3) {
		fclose(ctx->fh);
		free(ctx);
		return NULL;
	}

	return ctx;
}

static void container_process(struct container_ctx *ctx)
{
	size_t val;
	unsigned char filename[64];
	u_int32_t start, len, flags1, flags2;

#ifdef WITH_MMAP
	ctx->data = mmap(NULL, ctx->header.diroffset, PROT_READ, MAP_PRIVATE,
				fileno(ctx->fh), 0);
#else
	ctx->data = malloc(ctx->header.diroffset);
	fseek(ctx->fh, 0, SEEK_SET);
	val = fread(ctx->data, ctx->header.diroffset, 1, ctx->fh);
	if (val == 0)
		LOG(("empty read diroffset"));
#endif
	fseek(ctx->fh, ctx->header.diroffset, SEEK_SET);
	/* now work through the directory structure taking it apart into
	 * our structure */
#define BEREAD(x) do { val = fread(&(x), 4, 1, ctx->fh); if (val == 0)\
		LOG(("empty read"));(x) = ntohl((x)); } while (0)
	do {
		val = fread(filename, 64, 1, ctx->fh);
		if (val == 0)
			LOG(("empty read filename"));
		BEREAD(start);
		BEREAD(len);
		BEREAD(flags1);
		BEREAD(flags2);
		if (filename[0] != '\0')
			container_add_to_dir(ctx, filename, start, len);
	} while (filename[0] != '\0');
#undef BEREAD
	ctx->processed = true;
}

static const struct container_dirent *container_lookup(
					struct container_ctx *ctx,
					const unsigned char *entryname)
{
	unsigned int i;

	for (i = 1; i <= ctx->entries; i++) {
		struct container_dirent *e = ctx->directory + i - 1;
		if (strcmp((char *)e->filename, (char *)entryname) == 0)
			return e;
	}

	return NULL;
}

const unsigned char *container_get(struct container_ctx *ctx,
					const unsigned char *entryname,
					u_int32_t *size)
{
	const struct container_dirent *e;

	if (ctx->processed == false)
		container_process(ctx);

	e = container_lookup(ctx, entryname);

	if (e == NULL)
		return NULL;

	*size = e->len;

	return &ctx->data[e->startoffset];
}

const unsigned char *container_iterate(struct container_ctx *ctx, int *state)
{
	struct container_dirent *e;
	unsigned char *r;

	if (ctx->processed == false)
		container_process(ctx);

	e = ctx->directory + *state;

	r = e->filename;

	if (r == NULL || r[0] == '\0')
		r = NULL;

	*state += 1;

	return r;
}

const unsigned char *container_get_name(struct container_ctx *ctx)
{
	return ctx->header.name;
}

const unsigned char *container_get_author(struct container_ctx *ctx)
{
	return ctx->header.author;
}


static void container_write_dir(struct container_ctx *ctx)
{
	size_t val;
	unsigned int i;
	u_int32_t tmp;
#define BEWRITE(x) do {tmp = htonl((x)); val = fwrite(&tmp, 4, 1, ctx->fh);\
		if (val == 0) LOG(("empty write")); } while(0)
	for (i = 1; i <= ctx->entries; i++) {
		struct container_dirent *e = ctx->directory + i - 1;
		val = fwrite(e->filename, 64, 1, ctx->fh);
		if (val == 0)
			LOG(("empty write filename"));
		BEWRITE(e->startoffset);
		BEWRITE(e->len);
		BEWRITE(e->flags1);
		BEWRITE(e->flags2);
	}
#undef BEWRITE
	/* empty entry signifies end of directory */
	tmp = 0;
	val = fwrite(&tmp, 4, 8, ctx->fh);
	if (val == 0)
		LOG(("empty write end"));
}

struct container_ctx *container_create(const char *filename,
					const unsigned char *name,
					const unsigned char *author)
{
	size_t val;
	struct container_ctx *ctx = calloc(sizeof(struct container_ctx), 1);

	ctx->fh = fopen(filename, "wb");

	if (ctx->fh == NULL) {
		free(ctx);
		return NULL;
	}

	ctx->creating = true;
	ctx->entries = 0;
	ctx->directory = NULL;
	ctx->header.parser = htonl(3);
	strncpy((char *)ctx->header.name, (char *)name, 32);
	strncpy((char *)ctx->header.author, (char *)author, 64);

	val = fwrite("NSTM", 4, 1, ctx->fh);
	if (val == 0)
		LOG(("empty write NSTM"));
	val = fwrite(&ctx->header.parser, 4, 1, ctx->fh);
	if (val == 0)
		LOG(("empty write parser"));
	val = fwrite(ctx->header.name, 32, 1, ctx->fh);
	if (val == 0)
		LOG(("empty write name"));
	val = fwrite(ctx->header.author, 64, 1, ctx->fh);
	if (val == 0)
		LOG(("empty write author"));

	ctx->header.diroffset = 108;

	/* skip over the directory offset for now, and fill it in later.
	 * we don't know where it'll be yet!
	 */

	fseek(ctx->fh, 108, SEEK_SET);

	return ctx;
}

void container_add(struct container_ctx *ctx, const unsigned char *entryname,
					const unsigned char *data,
					const u_int32_t datalen)
{
	size_t val;
	container_add_to_dir(ctx, entryname, ftell(ctx->fh), datalen);
	val = fwrite(data, datalen, 1, ctx->fh);
	if (val == 0)
		LOG(("empty write add file"));
}

void container_close(struct container_ctx *ctx)
{
	if (ctx->creating == true) {
		size_t flen, nflen, val;

		/* discover where the directory's going to go. */
		flen = container_filelen(ctx->fh);
		flen = (flen + 3) & (~3); /* round up to nearest 4 bytes */

		/* write this location to the header */
		fseek(ctx->fh, 104, SEEK_SET);
		nflen = htonl(flen);
		val = fwrite(&nflen, 4, 1, ctx->fh);
		if (val == 0)
			LOG(("empty write directory location"));

		/* seek to where the directory will be, and write it */
		fseek(ctx->fh, flen, SEEK_SET);
		container_write_dir(ctx);

	} else if (ctx->processed) {
#ifdef WITH_MMAP
		munmap(ctx->data, ctx->header.diroffset);
#else
		free(ctx->data);
#endif
	}

	fclose(ctx->fh);
	free(ctx);
}

#ifdef WITH_THEME_INSTALL

/**
 * install theme from container
 * \param themefile a file containing the containerized theme
 * \param dirbasename a directory basename including trailing path sep; the
 * full path of the theme is then a subdirectory of that
 * caller owns reference to returned string, NULL for error
 */

char *container_extract_theme(const char *themefile, const char *dirbasename)
{
	struct stat statbuf;
	struct container_ctx *cctx;
	FILE *fh;
	size_t val;
	const unsigned char *e, *d;
	char *themename, *dirname;
	char path[PATH_MAX];
	int state = 0;
	unsigned int i;
	u_int32_t flen;

	cctx = container_open(themefile);
	if (cctx == NULL) {
		warn_user("FileOpenError", themefile);
		return NULL;
	}
	themename = strdup((const char *)container_get_name(cctx));
	if (themename == NULL) {
		warn_user("NoMemory", 0);
		container_close(cctx);
		return NULL;
	}
	LOG(("theme name: %s", themename));
	LOG(("theme author: %s", container_get_author(cctx)));
	
	dirname = malloc(strlen(dirbasename) + strlen(themename) + 2);
	if (dirname == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		free(themename);
		container_close(cctx);
		return NULL;
	}
	strcpy(dirname, dirbasename);
	strcat(dirname, themename);
	if (stat(dirname, &statbuf) != -1) {
		warn_user("DirectoryError", dirname);
		container_close(cctx);
		free(dirname);
		free(themename);
		return NULL;
	}
	mkdir(dirname, S_IRWXU);

	for (e = container_iterate(cctx, &state), i = 0; i < cctx->entries;
			e = container_iterate(cctx, &state), i++) {
		LOG(("extracting %s", e));
		snprintf(path, PATH_MAX, "%s/%s", dirname, e);
		fh = fopen(path, "wb");
		if (fh == NULL) {
			warn_user("FileOpenError", (char *)e);
		} else {
			d = container_get(cctx, e, &flen);
			val = fwrite(d, flen, 1, fh);
			if (val == 0)
				LOG(("empty write"));
			fclose(fh);
		}
	}
	LOG(("theme container unpacked"));
	container_close(cctx);
	free(dirname);
	return themename;

}

#endif

#ifdef TEST_RIG
int main(int argc, char *argv[])
{
	struct container_ctx *ctx = container_create("test.theme", "Test theme",
				"Rob Kendrick");
	u_int32_t size;
	int state = 0;
	char *n;

	container_add(ctx, "CHEESE", "This is a test of some cheese.", sizeof("This is a test of some cheese."));
	container_add(ctx, "FOO", "This is a test of some cheese.", sizeof("This is a test of some cheese."));

	container_close(ctx);

	ctx = container_open("test.theme");

	printf("Theme name: %s\n", container_get_name(ctx));
	printf("Theme author: %s\n", container_get_author(ctx));

	printf("Test string: %s\n", container_get(ctx, "CHEESE", &size));
	printf("Length of text: %d\n", size);

	while ( (n = container_iterate(ctx, &state)) ) {
		printf("%s\n", n);
	}

	container_close(ctx);

	exit(0);
}
#endif

#ifdef NSTHEME
	/* code to implement a simple container creator/extractor */
#include <getopt.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>

static bool verbose = false;

static void show_usage(const char *argv0)
{
	fprintf(stderr, "%s [options] <theme file> <directory>\n", argv0);
	fprintf(stderr, " --help       This text\n");
	fprintf(stderr, " --create     Create theme file from directory\n");
	fprintf(stderr, " --extract    Extract theme file into directory\n");
	fprintf(stderr, " --name x     Set theme's name when creating\n");
	fprintf(stderr, " --author x   Set theme's author when creating\n");
	fprintf(stderr, " --verbose    Print progress information\n");
	fprintf(stderr, "\nOne and only one of --create or --extract must be specified.\n");
}

static void extract_theme(const char *themefile, const char *dirname)
{
	struct stat statbuf;
	struct container_ctx *cctx;
	FILE *fh;
	const unsigned char *e, *d;
	char path[PATH_MAX];
	int i, state = 0;
	u_int32_t flen;


	if (stat(dirname, &statbuf) != -1) {
		fprintf(stderr, "error: directory '%s' already exists.\n",
			dirname);
		exit(1);
	}

	mkdir(dirname, S_IRWXU);

	cctx = container_open(themefile);
	if (cctx == NULL) {
		fprintf(stderr, "error: unable to open theme file '%s'\n",
			themefile);
		exit(1);
	}

	if (verbose == true) {
		printf("theme name: %s\n", container_get_name(cctx));
		printf("theme author: %s\n", container_get_author(cctx));
	}

	for (e = container_iterate(cctx, &state), i = 0; i < cctx->entries;
			e = container_iterate(cctx, &state), i++) {
		if (verbose == true)
			printf("extracting %s\n", e);
		snprintf(path, PATH_MAX, "%s/%s", dirname, e);
		fh = fopen(path, "wb");
		if (fh == NULL) {
			perror("warning: unable to open file for output");
		} else {
			d = container_get(cctx, e, &flen);
			fwrite(d, flen, 1, fh);
			fclose(fh);
		}
	}

	container_close(cctx);

}

static void create_theme(const char *themefile, const char *dirname,
				const unsigned char *name,
				const unsigned char *author)
{
	DIR *dir = opendir(dirname);
	FILE *fh;
	struct dirent *e;
	struct stat statbuf;
	struct container_ctx *cctx;
	unsigned char *data;
	char path[PATH_MAX];
	size_t flen;
	int t;

	if (dir == NULL) {
		perror("error: unable to open directory");
		exit(1);
	}

	cctx = container_create(themefile, name, author);

	errno = 0;	/* to distinguish between end of dir and err */

	while ((e = readdir(dir)) != NULL) {
		if (strcmp(e->d_name, ".") != 0 &&
			strcmp(e->d_name, "..") != 0) {
			/* not the metadirs, so we want to process this. */
			if (verbose == true)
				printf("adding %s\n", e->d_name);
			if (strlen(e->d_name) > 63) {
				fprintf(stderr,
			"warning: name truncated to length 63.\n");
			}

			snprintf(path, PATH_MAX, "%s/%s", dirname, e->d_name);

			stat(path, &statbuf);
			if (S_ISDIR(statbuf.st_mode)) {
				fprintf(stderr,
					"warning: skipping directory '%s'\n",
					e->d_name);
				continue;
			}

			fh = fopen(path, "rb");
			if (fh == NULL) {
				fprintf(stderr,
					"warning: unable to open, skipping.");
			} else {
				flen = statbuf.st_size;
				data = malloc(flen);
				t = fread(data, flen, 1, fh);
				fclose(fh);
				container_add(cctx, (unsigned char *)e->d_name,
						data, flen);
				free(data);
			}
		}
		errno = 0;
	}

	if (errno != 0) {
		perror("error: couldn't enumerate directory");
		closedir(dir);
		container_close(cctx);
		exit(1);
	}

	closedir(dir);
	container_close(cctx);
}

int main(int argc, char *argv[])
{
	static struct option l_opts[] = {
		{ "help", 0, 0, 'h' },
		{ "create", 0, 0, 'c' },
		{ "extract", 0, 0, 'x' },
		{ "name", 1, 0, 'n' },
		{ "author", 1, 0, 'a' },
		{ "verbose", 0, 0, 'v' },

		{ NULL, 0, 0, 0 }
	};

	static char *s_opts = "hcxn:a:v";
	int optch, optidx;
	bool creating = false, extracting = false;
	unsigned char name[32] = { '\0' }, author[64] = { '\0' };
	char *themefile, *dirname;

	while ((optch = getopt_long(argc, argv, s_opts, l_opts, &optidx)) != -1)
		switch (optch) {
		case 'h':
			show_usage(argv[0]);
			exit(0);
			break;
		case 'c':
			creating = true;
			break;
		case 'x':
			extracting = true;
			break;
		case 'n':
			strncpy((char *)name, optarg, 31);
			if (strlen(optarg) > 32)
				fprintf(stderr, "warning: theme name truncated to 32 characters.\n");
			break;
		case 'a':
			strncpy((char *)author, optarg, 63);
			if (strlen(optarg) > 64)
				fprintf(stderr, "warning: theme author truncated to 64 characters.\n");
			break;
		case 'v':
			verbose = true;
			break;
		default:
			show_usage(argv[0]);
			exit(1);
			break;
		}

	if (creating == extracting) {
		show_usage(argv[0]);
		exit(1);
	}

	if ((argc - optind) < 2) {
		show_usage(argv[0]);
		exit(1);
	}

	if (creating == true &&
		(strlen((char *)name) == 0 || strlen((char *)author) == 0)) {
		fprintf(stderr, "No theme name and/or author specified.\n");
		show_usage(argv[0]);
		exit(1);
	}

	themefile = strdup(argv[optind]);
	dirname = strdup(argv[optind + 1]);

	if (verbose == true)
		printf("%s '%s' %s directory '%s'\n",
			creating ? "creating" : "extracting", themefile,
			creating ? "from" : "to", dirname);

	if (creating) {
		if (verbose == true)
			printf("name = %s, author = %s\n", name, author);
		create_theme(themefile, dirname, name, author);
	} else {
		extract_theme(themefile, dirname);
	}

	return 0;
}
#endif
