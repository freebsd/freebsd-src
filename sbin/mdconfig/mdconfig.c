/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
 *
 */
#include <sys/param.h>
#include <sys/devicestat.h>
#include <sys/ioctl.h>
#include <sys/linker.h>
#include <sys/mdioctl.h>
#include <sys/module.h>
#include <sys/resource.h>
#include <sys/stat.h>

#include <assert.h>
#include <devstat.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libgeom.h>
#include <libutil.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


static struct md_ioctl mdio;
static enum {UNSET, ATTACH, DETACH, LIST} action = UNSET;
static int nflag;

static void usage(void);
static int md_find(char *, const char *);
static int md_query(char *name);
static int md_list(char *units, int opt);
static char *geom_config_get(struct gconf *g, const char *name);
static void md_prthumanval(char *length);

#define OPT_VERBOSE	0x01
#define OPT_UNIT	0x02
#define OPT_DONE	0x04
#define OPT_LIST	0x10

#define CLASS_NAME_MD	"MD"

static void
usage(void)
{
	fprintf(stderr,
"usage: mdconfig -a -t type [-n] [-o [no]option] ... [-f file]\n"
"                [-s size] [-S sectorsize] [-u unit]\n"
"                [-x sectors/track] [-y heads/cylinder]\n"
"       mdconfig -d -u unit [-o [no]force]\n"
"       mdconfig -l [-v] [-n] [-u unit]\n");
	fprintf(stderr, "\t\ttype = {malloc, preload, vnode, swap}\n");
	fprintf(stderr, "\t\toption = {cluster, compress, reserve}\n");
	fprintf(stderr, "\t\tsize = %%d (512 byte blocks), %%db (B),\n");
	fprintf(stderr, "\t\t       %%dk (kB), %%dm (MB), %%dg (GB) or\n");
	fprintf(stderr, "\t\t       %%dt (TB)\n");
	exit(1);
}

int
main(int argc, char **argv)
{
	int ch, fd, i, vflag;
	char *p;
	int cmdline = 0;
	char *mdunit = NULL;

	bzero(&mdio, sizeof(mdio));
	mdio.md_file = malloc(PATH_MAX);
	if (mdio.md_file == NULL)
		err(1, "could not allocate memory");
	vflag = 0;
	bzero(mdio.md_file, PATH_MAX);
	for (;;) {
		ch = getopt(argc, argv, "ab:df:lno:s:S:t:u:vx:y:");
		if (ch == -1)
			break;
		switch (ch) {
		case 'a':
			if (cmdline != 0)
				usage();
			action = ATTACH;
			cmdline = 1;
			break;
		case 'd':
			if (cmdline != 0)
				usage();
			action = DETACH;
			mdio.md_options = MD_AUTOUNIT;
			cmdline = 3;
			break;
		case 'l':
			if (cmdline != 0)
				usage();
			action = LIST;
			mdio.md_options = MD_AUTOUNIT;
			cmdline = 3;
			break;
		case 'n':
			nflag = 1;
			break;
		case 't':
			if (cmdline != 1)
				usage();
			if (!strcmp(optarg, "malloc")) {
				mdio.md_type = MD_MALLOC;
				mdio.md_options = MD_AUTOUNIT | MD_COMPRESS;
			} else if (!strcmp(optarg, "preload")) {
				mdio.md_type = MD_PRELOAD;
				mdio.md_options = 0;
			} else if (!strcmp(optarg, "vnode")) {
				mdio.md_type = MD_VNODE;
				mdio.md_options = MD_CLUSTER | MD_AUTOUNIT | MD_COMPRESS;
			} else if (!strcmp(optarg, "swap")) {
				mdio.md_type = MD_SWAP;
				mdio.md_options = MD_CLUSTER | MD_AUTOUNIT | MD_COMPRESS;
			} else {
				usage();
			}
			cmdline=2;
			break;
		case 'f':
			if (cmdline == 0) {
				action = ATTACH;
				cmdline = 1;
			}
			if (cmdline == 1) {
				/* Imply ``-t vnode'' */
				mdio.md_type = MD_VNODE;
				mdio.md_options = MD_CLUSTER | MD_AUTOUNIT | MD_COMPRESS;
				cmdline = 2;
			}
 			if (cmdline != 2)
 				usage();
			if (realpath(optarg, mdio.md_file) == NULL) {
				err(1, "could not find full path for %s",
				    optarg);
			}
			fd = open(mdio.md_file, O_RDONLY);
			if (fd < 0)
				err(1, "could not open %s", optarg);
			else if (mdio.md_mediasize == 0) {
				struct stat sb;

				if (fstat(fd, &sb) == -1)
					err(1, "could not stat %s", optarg);
				mdio.md_mediasize = sb.st_size;
			}
			close(fd);
			break;
		case 'o':
			if (action == DETACH) {
				if (!strcmp(optarg, "force"))
					mdio.md_options |= MD_FORCE;
				else if (!strcmp(optarg, "noforce"))
					mdio.md_options &= ~MD_FORCE;
				else
					errx(1, "Unknown option: %s.", optarg);
				break;
			}

			if (cmdline != 2)
				usage();
			if (!strcmp(optarg, "async"))
				mdio.md_options |= MD_ASYNC;
			else if (!strcmp(optarg, "noasync"))
				mdio.md_options &= ~MD_ASYNC;
			else if (!strcmp(optarg, "cluster"))
				mdio.md_options |= MD_CLUSTER;
			else if (!strcmp(optarg, "nocluster"))
				mdio.md_options &= ~MD_CLUSTER;
			else if (!strcmp(optarg, "compress"))
				mdio.md_options |= MD_COMPRESS;
			else if (!strcmp(optarg, "nocompress"))
				mdio.md_options &= ~MD_COMPRESS;
			else if (!strcmp(optarg, "force"))
				mdio.md_options |= MD_FORCE;
			else if (!strcmp(optarg, "noforce"))
				mdio.md_options &= ~MD_FORCE;
			else if (!strcmp(optarg, "readonly"))
				mdio.md_options |= MD_READONLY;
			else if (!strcmp(optarg, "noreadonly"))
				mdio.md_options &= ~MD_READONLY;
			else if (!strcmp(optarg, "reserve"))
				mdio.md_options |= MD_RESERVE;
			else if (!strcmp(optarg, "noreserve"))
				mdio.md_options &= ~MD_RESERVE;
			else
				errx(1, "Unknown option: %s.", optarg);
			break;
		case 'S':
			if (cmdline != 2)
				usage();
			mdio.md_sectorsize = strtoul(optarg, &p, 0);
			break;
		case 's':
			if (cmdline == 0) {
				/* Imply ``-a'' */
				action = ATTACH;
				cmdline = 1;
			}
			if (cmdline == 1) {
				/* Imply ``-t swap'' */
				mdio.md_type = MD_SWAP;
				mdio.md_options = MD_CLUSTER | MD_AUTOUNIT | MD_COMPRESS;
				cmdline = 2;
			}
			if (cmdline != 2)
				usage();
			mdio.md_mediasize = (off_t)strtoumax(optarg, &p, 0);
			if (p == NULL || *p == '\0')
				mdio.md_mediasize *= DEV_BSIZE;
			else if (*p == 'b' || *p == 'B')
				; /* do nothing */
			else if (*p == 'k' || *p == 'K')
				mdio.md_mediasize <<= 10;
			else if (*p == 'm' || *p == 'M')
				mdio.md_mediasize <<= 20;
			else if (*p == 'g' || *p == 'G')
				mdio.md_mediasize <<= 30;
			else if (*p == 't' || *p == 'T') {
				mdio.md_mediasize <<= 30;
				mdio.md_mediasize <<= 10;
			} else
				errx(1, "Unknown suffix on -s argument");
			break;
		case 'u':
			if (cmdline != 2 && cmdline != 3)
				usage();
			if (!strncmp(optarg, "/dev/", 5))
				optarg += 5;
			if (!strncmp(optarg, MD_NAME, sizeof(MD_NAME) - 1))
				optarg += sizeof(MD_NAME) - 1;
			mdio.md_unit = strtoul(optarg, &p, 0);
			if (mdio.md_unit == (unsigned)ULONG_MAX || *p != '\0')
				errx(1, "bad unit: %s", optarg);
			mdunit = optarg;
			mdio.md_options &= ~MD_AUTOUNIT;
			break;
		case 'v':
			if (cmdline != 3)
				usage();
			vflag = OPT_VERBOSE;
			break;
		case 'x':
			if (cmdline != 2)
				usage();
			mdio.md_fwsectors = strtoul(optarg, &p, 0);
			break;
		case 'y':
			if (cmdline != 2)
				usage();
			mdio.md_fwheads = strtoul(optarg, &p, 0);
			break;
		default:
			usage();
		}
	}
	mdio.md_version = MDIOVERSION;

	if (!kld_isloaded("g_md") && kld_load("geom_md") == -1)
		err(1, "failed to load geom_md module");

	fd = open("/dev/" MDCTL_NAME, O_RDWR, 0);
	if (fd < 0)
		err(1, "open(/dev/%s)", MDCTL_NAME);
	if (cmdline == 2
	    && (mdio.md_type == MD_MALLOC || mdio.md_type == MD_SWAP))
		if (mdio.md_mediasize == 0)
			errx(1, "must specify -s for -t malloc or -t swap");
	if (cmdline == 2 && mdio.md_type == MD_VNODE)
		if (mdio.md_file[0] == '\0')
			errx(1, "must specify -f for -t vnode");
	if (mdio.md_type == MD_VNODE &&
	    (mdio.md_options & MD_READONLY) == 0) {
		if (access(mdio.md_file, W_OK) < 0 &&
		    (errno == EACCES || errno == EPERM || errno == EROFS)) {
			fprintf(stderr,
			    "WARNING: opening backing store: %s readonly\n",
			    mdio.md_file);
			mdio.md_options |= MD_READONLY;
		}
	}
	if (action == LIST) {
		if (mdio.md_options & MD_AUTOUNIT) {
			/* 
			 * Listing all devices. This is why we pass NULL
			 * together with OPT_LIST.
			 */
			md_list(NULL, OPT_LIST | vflag);
		} else {
			return (md_query(mdunit));
		}
	} else if (action == ATTACH) {
		if (cmdline < 2)
			usage();
		i = ioctl(fd, MDIOCATTACH, &mdio);
		if (i < 0)
			err(1, "ioctl(/dev/%s)", MDCTL_NAME);
		if (mdio.md_options & MD_AUTOUNIT)
			printf("%s%d\n", nflag ? "" : MD_NAME, mdio.md_unit);
	} else if (action == DETACH) {
		if (mdio.md_options & MD_AUTOUNIT)
			usage();
		i = ioctl(fd, MDIOCDETACH, &mdio);
		if (i < 0)
			err(1, "ioctl(/dev/%s)", MDCTL_NAME);
	} else
		usage();
	close (fd);
	return (0);
}

/*
 * Lists md(4) disks. Is used also as a query routine, since it handles XML
 * interface. 'units' can be NULL for listing memory disks. It might be
 * coma-separated string containing md(4) disk names. 'opt' distinguished
 * between list and query mode.
 */
static int
md_list(char *units, int opt)
{
	struct gmesh gm;
	struct gprovider *pp;
	struct gconf *gc;
	struct gident *gid;
	struct devstat *gsp;
	struct ggeom *gg;
	struct gclass *gcl;
	void *sq;
	int retcode, found;
	char *type, *file, *length;

	type = file = length = NULL;

	retcode = geom_gettree(&gm);
	if (retcode != 0)
		return (-1);
	retcode = geom_stats_open();
	if (retcode != 0)
		return (-1);
	sq = geom_stats_snapshot_get();
	if (sq == NULL)
		return (-1);

	found = 0;
	while ((gsp = geom_stats_snapshot_next(sq)) != NULL) {
		gid = geom_lookupid(&gm, gsp->id);
		if (gid == NULL)
			continue;
		if (gid->lg_what == ISPROVIDER) {
			pp = gid->lg_ptr;
			gg = pp->lg_geom;
			gcl = gg->lg_class;
			if (strcmp(gcl->lg_name, CLASS_NAME_MD) != 0)
				continue;
			if ((opt & OPT_UNIT) && (units != NULL)) {
				retcode = md_find(units, pp->lg_name);
				if (retcode != 1)
					continue;
				else
					found = 1;
			}
			gc = &pp->lg_config;
			printf("%s", nflag ? pp->lg_name + 2 : pp->lg_name);
			if (opt & OPT_VERBOSE || opt & OPT_UNIT) {
				type = geom_config_get(gc, "type");
				if (strcmp(type, "vnode") == 0)
					file = geom_config_get(gc, "file");
				length = geom_config_get(gc, "length");
				printf("\t%s\t", type);
				if (length != NULL)
					md_prthumanval(length);
				if (file != NULL) {
					printf("\t%s", file);
					file = NULL;
				}
			}
			opt |= OPT_DONE;
			if ((opt & OPT_LIST) && !(opt & OPT_VERBOSE))
				printf(" ");
			else
				printf("\n");
		}
	}
	if ((opt & OPT_LIST) && (opt & OPT_DONE) && !(opt & OPT_VERBOSE))
		printf("\n");
	/* XXX: Check if it's enough to clean everything. */
	geom_stats_snapshot_free(sq);
	if ((opt & OPT_UNIT) && found)
		return (0);
	else
		return (-1);
}

/*
 * Returns value of 'name' from gconfig structure.
 */
static char *
geom_config_get(struct gconf *g, const char *name)
{
	struct gconfig *gce;

	LIST_FOREACH(gce, g, lg_config) {
		if (strcmp(gce->lg_name, name) == 0)
			return (gce->lg_val);
	}
	return (NULL);
}

/*
 * List is comma separated list of MD disks. name is a
 * device name we look for.  Returns 1 if found and 0
 * otherwise.
 */
static int
md_find(char *list, const char *name)
{
	int ret;
	char num[16];
	char *ptr, *p, *u;

	ret = 0;
	ptr = strdup(list);
	if (ptr == NULL)
		return (-1);
	for (p = ptr; (u = strsep(&p, ",")) != NULL;) {
		if (strncmp(u, "/dev/", 5) == 0)
			u += 5;
		/* Just in case user specified number instead of full name */
		snprintf(num, sizeof(num), "md%s", u);
		if (strcmp(u, name) == 0 || strcmp(num, name) == 0) {
			ret = 1;
			break;
		}
	}
	free(ptr);
	return (ret);
}

static void
md_prthumanval(char *length)
{
	char buf[6];
	uintmax_t bytes;
	char *endptr;

	errno = 0;
	bytes = strtoumax(length, &endptr, 10);
	if (errno != 0 || *endptr != '\0' || bytes > INT64_MAX)
		return;
	humanize_number(buf, sizeof(buf), (int64_t)bytes, "",
	    HN_AUTOSCALE, HN_B | HN_NOSPACE | HN_DECIMAL);
	(void)printf("%6s", buf);
}

int
md_query(char *name)
{
	return (md_list(name, OPT_UNIT));
}
