/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <err.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/disklabel.h>
#include <sys/diskslice.h>
#ifndef PC98
#include <sys/diskmbr.h>
#endif
#include <paths.h>
#include "libdisk.h"

#define	HAVE_GEOM
#ifdef HAVE_GEOM
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#endif /*HAVE_GEOM*/

#ifndef PC98
#define DOSPTYP_EXTENDED        5
#define DOSPTYP_ONTRACK         84
#endif

#ifdef DEBUG
#define	DPRINT(x)	warn x
#define	DPRINTX(x)	warnx x
#else
#define	DPRINT(x)
#define	DPRINTX(x)
#endif

const char *chunk_n[] = {
	"whole",
	"unknown",
	"fat",
	"freebsd",
	"extended",
	"part",
	"unused",
	NULL
};

struct disk *
Open_Disk(const char *name)
{
	return Int_Open_Disk(name, 0);
}

#ifndef PC98
static u_int32_t
Read_Int32(u_int32_t *p)
{
    u_int8_t *bp = (u_int8_t *)p;
    return bp[0] | (bp[1] << 8) | (bp[2] << 16) | (bp[3] << 24);
}
#endif

#ifdef HAVE_GEOM
/*
 * XXX BEGIN HACK XXX
 * Scan/parse the XML geom data to retrieve what we need to
 * carry out the work of Int_Open_Disk.  This is a total hack
 * and should be replaced with a real XML parser.
 */
typedef enum {
	XML_MESH,
	XML_MESH_END,
	XML_CLASS,
	XML_CLASS_END,
	XML_GEOM,
	XML_GEOM_END,
	XML_CONFIG,
	XML_CONFIG_END,
	XML_PROVIDER,
	XML_PROVIDER_END,
	XML_NAME,
	XML_NAME_END,
	XML_INDEX,
	XML_INDEX_END,
	XML_SECLENGTH,
	XML_SECLENGTH_END,
	XML_SECOFFSET,
	XML_SECOFFSET_END,
	XML_TYPE,
	XML_TYPE_END,
	XML_MEDIASIZE,
	XML_MEDIASIZE_END,
	XML_SECTORSIZE,
	XML_SECTORSIZE_END,
	XML_FWHEADS,
	XML_FWHEADS_END,
	XML_FWSECTORS,
	XML_FWSECTORS_END,

	XML_OTHER,
	XML_OTHER_END
} XMLToken;

const struct {
	XMLToken	t;
	const char*	token;
	const char*	name;
} xmltokens[] = {
	{ XML_MESH,		"mesh",		"XML_MESH" },
	{ XML_CLASS,		"class",	"XML_CLASS" },
	{ XML_GEOM,		"geom",		"XML_GEOM" },
	{ XML_CONFIG,		"config",	"XML_CONFIG" },
	{ XML_PROVIDER,		"provider",	"XML_PROVIDE" },
	{ XML_NAME,		"name",		"XML_NAME" },
	{ XML_INDEX,		"index",	"XML_INDEX" },
	{ XML_SECLENGTH,	"seclength",	"XML_SECLENGTH" },
	{ XML_SECOFFSET,	"secoffset",	"XML_SECOFFSET" },
	{ XML_TYPE,		"type",		"XML_TYPE" },
	{ XML_FWHEADS,		"fwheads",	"XML_FWHEADS" },
	{ XML_FWSECTORS,	"fwsectors",	"XML_FWSECTORS" },
	{ XML_MEDIASIZE,	"mediasize",	"XML_MEDIASIZE" },
	{ XML_SECTORSIZE,	"sectorsize",	"XML_SECTORSIZE" },
	/* NB: this must be last */
	{ XML_OTHER,		NULL,		"XML_OTHER" },
};
#define	N(x)	(sizeof (x) / sizeof (x[0]))

#ifdef DEBUG
static const char*
xmltokenname(XMLToken t)
{
	int i;

	for (i = 0; i < N(xmltokens); i++) {
		if (t == xmltokens[i].t)
			return xmltokens[i].name;
		if ((t-1) == xmltokens[i].t) {
			static char tbuf[80];
			snprintf(tbuf, sizeof (tbuf), "%s_END",
				xmltokens[i].name);
			return tbuf;
		}
	}
	return "???";
}
#endif /*DEBUG*/

/*
 * Parse the next XML token delimited by <..>.  If the token
 * has a "builtin terminator" (<... />) then just skip it and
 * go the next token.
 */
static int
xmltoken(const char *start, const char **next, XMLToken *t)
{
	const char *cp = start;
	const char *token;
	int i;

again:
	while (*cp != '<') {
		if (*cp == '\0') {
			*next = cp;
			DPRINTX(("xmltoken: EOD"));
			return 0;
		}
		cp++;
	}
	token = ++cp;
	for (; *cp && *cp != '>' && !isspace(*cp); cp++)
		;
	if (*cp == '\0') {
		*next = cp;
		DPRINTX(("xmltoken: EOD"));
		return 0;
	}
	*t = (*token == '/');
	if (*t)
		token++;
	for (i = 0; xmltokens[i].token != NULL; i++)
		if (strncasecmp(token, xmltokens[i].token, cp-token) == 0)
			break;
	*t += xmltokens[i].t;
	/* now collect the remainder of the string */
	for (; *cp != '>' && *cp != '\0'; cp++)
		;
	if (*cp == '\0') {
		*next = cp;
		DPRINTX(("xmltoken: EOD"));
		return 0;
	}
	if (cp > token && cp[-1] == '/') {
		/* e.g. <geom ref="0xc1c8c100"/> */
		start = cp+1;
		goto again;
	}
	*next = cp+1;
	DPRINTX(("xmltoken: %s \"%.*s\"", xmltokenname(*t), cp-token, token));
	return 1;
}

/*
 * Parse and discard XML up to the token terminator.
 */
static int
discardxml(const char **next, XMLToken terminator)
{
	const char *xml = *next;
	XMLToken t;

	DPRINTX(("discard XML up to %s", xmltokenname(terminator)));
	for (;;) {
		if (xmltoken(xml, next, &t) == 0)
			return EINVAL;
		if (t == terminator)
			break;
		if ((t & 1) == 0) {
			int error = discardxml(next, t+1);
			if (error)
				return error;
		}
		xml = *next;
	}
	return 0;
}

/*
 * Parse XML from between a range of markers; e.g. <mesh> ... </mesh>.
 * When the specified class name is located we descend looking for the
 * geometry information given by diskname.  Once inside there we process
 * tags calling f back for each useful one.  The arg is passed into f
 * for use in storing the parsed data.
 */
static int
parsexmlpair(
	const char *xml,
	const char **next,
	const char *classname,
	XMLToken terminator,
	const char *diskname,
	int (*f)(void *, XMLToken, u_int *, u_int64_t),
	void *arg
)
{
	const char *cp;
	XMLToken t;
	int error;
	u_int ix = (u_int) -1;

	DPRINTX(("parse XML up to %s", xmltokenname(terminator)));
	do {
		if (xmltoken(xml, next, &t) == 0) {
			error = EINVAL;
			break;
		}
		if (t == terminator) {
			error = 0;
			break;
		}
		if (t & 1) {		/* </mumble> w/o matching <mumble> */
			DPRINTX(("Unexpected token %s", xmltokenname(t)));
			error = EINVAL;
			break;
		}
		switch ((int) t) {
		case XML_NAME:
			for (cp = *next; *cp && *cp != '<'; cp++)
				;
			if (*cp == '\0') {
				DPRINTX(("parsexmlpair: EOD"));
				error = EINVAL;
				goto done;
			    }
			DPRINTX(("parsexmlpair: \"%.*s\"", cp-*next, *next));
			switch ((int) terminator) {
			case XML_CLASS_END:
				if (strncasecmp(*next, classname, cp-*next))
					return discardxml(next, terminator);
				break;
			case XML_GEOM_END:
				if (strncasecmp(*next, diskname, cp-*next))
					return discardxml(next, terminator);
				break;
			}
			break;
		case XML_SECOFFSET:
		case XML_SECLENGTH:
		case XML_TYPE:
			if (ix == (u_int) -1) {
				DPRINTX(("parsexmlpair: slice data w/o "
					"preceding index"));
				error = EINVAL;
				goto done;
			}
			/* fall thru... */
		case XML_INDEX:
		case XML_FWHEADS:
		case XML_FWSECTORS:
		case XML_MEDIASIZE:
		case XML_SECTORSIZE:
			if (terminator != XML_CONFIG_END &&
			    terminator != XML_PROVIDER_END) {
				DPRINTX(("parsexmlpair: %s in unexpected "
				      "context: terminator %s",
				      xmltokenname(t),
				      xmltokenname(terminator)));
				error = EINVAL;
				goto done;
			}
			error = (*f)(arg, t, &ix, strtoull(*next, NULL, 10));
			if (error)
				goto done;
			break;
		}
		error = parsexmlpair(*next, &xml, classname,
				     t+1, diskname, f, arg);
	} while (error == 0);
done:
	return error;
}

/*
 * XML parser.  Just barely smart enough to handle the
 * gibberish that geom passed back from the kernel.
 */
static int
xmlparse(
	const char *confxml,
	const char *classname,
	const char *diskname,
	int (*f)(void *, XMLToken, u_int *, u_int64_t),
	void *arg
)
{
	const char *next;
	XMLToken t;
	int error;

	next = confxml;
	while (xmltoken(next, &next, &t) && t != XML_MESH)
		;
	if (t == XML_MESH)
		error = parsexmlpair(next, &next, classname, XML_MESH_END, diskname, f, arg);
	else {
		DPRINTX(("xmlparse: expecting mesh token, got %s",
		      xmltokenname(t)));
		error = EINVAL;
	}

	return (error ? -1 : 0);
}

/*
 * Callback to collect slice-related data.
 */
static int
assignToSlice(void *arg, XMLToken t, u_int *slice, u_int64_t v)
{
	struct diskslices *ds = (struct diskslices *) arg;

	switch ((int) t) {
	case XML_INDEX:
		*slice = BASE_SLICE + (u_int) v;
		if (*slice >= MAX_SLICES) {
			DPRINTX(("assignToSlice: invalid slice index %u > max %u",
			      *slice, MAX_SLICES));
			return EINVAL;
		}
		if (*slice >= ds->dss_nslices)
			ds->dss_nslices = (*slice)+1;
		break;
	case XML_SECOFFSET:
		ds->dss_slices[*slice].ds_offset = (u_long) v;
		break;
	case XML_SECLENGTH:
		ds->dss_slices[*slice].ds_size = (u_long) v;
		break;
	case XML_TYPE:
		ds->dss_slices[*slice].ds_type = (int) v;
		break;
	}
	return 0;
}

/*
 * Callback to collect disk-related data.
 */
static int
assignToDisk(void *arg, XMLToken t, u_int *slice, u_int64_t v)
{
	struct disklabel *dl = (struct disklabel *) arg;

	switch ((int) t) {
	case XML_FWHEADS:
		dl->d_ntracks = (u_int32_t) v;
	case XML_FWSECTORS:
		dl->d_nsectors = (u_int32_t) v;
		break;
	case XML_MEDIASIZE:
		/* store this temporarily; it gets moved later */
		dl->d_secpercyl = v >> 32;
		dl->d_secperunit = v & 0xffffffff;
		break;
	case XML_SECTORSIZE:
		dl->d_secsize = (u_int32_t) v;
		break;
	}
	return 0;
}

/*
 * Callback to collect partition-related data.
 */
static int
assignToPartition(void *arg, XMLToken t, u_int *part, u_int64_t v)
{
	struct disklabel *dl = (struct disklabel *) arg;

	switch ((int) t) {
	case XML_INDEX:
		*part = (u_int) v;
		if (*part >= MAXPARTITIONS) {
			DPRINTX(("assignToPartition: invalid partition index %u > max %u",
			      *part, MAXPARTITIONS));
			return EINVAL;
		}
		if (*part >= dl->d_npartitions)
			dl->d_npartitions = (*part)+1;
		break;
	case XML_SECOFFSET:
		dl->d_partitions[*part].p_offset = (u_int32_t) v;
		break;
	case XML_SECLENGTH:
		dl->d_partitions[*part].p_size = (u_int32_t) v;
		break;
	case XML_TYPE:
		dl->d_partitions[*part].p_fstype = (u_int8_t) v;
		break;
	}
	return 0;
}
#undef N
#endif /*HAVE_GEOM*/

struct disk *
Int_Open_Disk(const char *name, u_long size)
{
	int i;
	int fd = -1;
	struct diskslices ds;
	struct disklabel dl;
	char device[64];
	struct disk *d;
#ifdef PC98
	unsigned char *p;
#else
	struct dos_partition *dp;
	void *p;
#endif
	u_long offset = 0;
#ifdef HAVE_GEOM
	char *confxml = NULL;
	size_t xmlsize;
	u_int64_t mediasize;
	int error;
#else
	u_long sector_size;
	char *buf;
#endif /*HAVE_GEOM*/

	strlcpy(device, _PATH_DEV, sizeof(device));
	strlcat(device, name, sizeof(device));

	d = (struct disk *)malloc(sizeof *d);
	if(!d) return NULL;
	memset(d, 0, sizeof *d);

	fd = open(device, O_RDONLY);
	if (fd < 0) {
		DPRINT(("open(%s) failed", device));
		goto bad;
	}

	memset(&dl, 0, sizeof dl);
	memset(&ds, 0, sizeof ds);
#ifdef HAVE_GEOM
	/*
	 * Read and hack-parse the XML that provides the info we need.
	 */
	error = sysctlbyname("kern.geom.confxml", NULL, &xmlsize, NULL, 0);
	if (error) {
		warn("kern.geom.confxml sysctl not available, giving up!");
		goto bad;
	}
	confxml = (char *) malloc(xmlsize+1);
	if (confxml == NULL) {
		DPRINT(("cannot malloc memory for confxml"));
		goto bad;
	}
	error = sysctlbyname("kern.geom.confxml", confxml, &xmlsize, NULL, 0);
	if (error) {
		DPRINT(("error reading kern.geom.confxml from the system"));
		goto bad;
	}
	confxml[xmlsize] = '\0';	/* in case kernel bug is still there */

	if (xmlparse(confxml, "MBR", name, assignToSlice, &ds) != 0) {
		DPRINTX(("Error parsing MBR geometry specification."));
		goto bad;
	}
	if (xmlparse(confxml, "DISK", name, assignToDisk, &dl) != 0) {
		DPRINTX(("Error parsing DISK geometry specification."));
		goto bad;
	}
	if (dl.d_nsectors == 0) {
		DPRINTX(("No (zero) sector information in DISK geometry"));
		goto bad;
	}
	if (dl.d_ntracks == 0) {
		DPRINTX(("No (zero) track information in DISK geometry"));
		goto bad;
	}
	if (dl.d_secsize == 0) {
		DPRINTX(("No (zero) sector size information in DISK geometry"));
		goto bad;
	}
	if (dl.d_secpercyl == 0 && dl.d_secperunit == 0) {
		DPRINTX(("No (zero) media size information in DISK geometry"));
		goto bad;
	}
	/*
	 * Now patch up disklabel and diskslice.
	 */
	d->sector_size = dl.d_secsize;
	/* NB: media size was stashed in two parts while parsing */
	mediasize = (((u_int64_t) dl.d_secpercyl) << 32) + dl.d_secperunit;
	dl.d_secpercyl = 0;
	dl.d_secperunit = 0;
	size = mediasize / d->sector_size;
	dl.d_ncylinders = size / (dl.d_ntracks * dl.d_nsectors);
	/* "whole disk" slice maintained for compatibility */
	ds.dss_slices[WHOLE_DISK_SLICE].ds_size = size;
#else /* !HAVE_GEOM */
	if (ioctl(fd, DIOCGDINFO, &dl) < 0) {
		DPRINT(("DIOCGDINFO(%s) failed", device));
		goto bad;
	}
	i = ioctl(fd, DIOCGSLICEINFO, &ds);
	if (i < 0) {
		DPRINT(("DIOCGSLICEINFO(%s) failed", device));
		goto bad;
	}

#ifdef DEBUG
	for(i = 0; i < ds.dss_nslices; i++)
		if(ds.dss_slices[i].ds_openmask)
			printf("  open(%d)=0x%2x",
				i, ds.dss_slices[i].ds_openmask);
	printf("\n");
#endif

/* XXX --- ds.dss_slice[WHOLE_DISK_SLICE].ds.size of MO disk is wrong!!! */
#ifdef PC98
	if (!size)
		size = dl.d_ncylinders * dl.d_ntracks * dl.d_nsectors;
#else
	if (!size)
		size = ds.dss_slices[WHOLE_DISK_SLICE].ds_size;
#endif

	/* determine media sector size */
	if ((buf = malloc(MAX_SEC_SIZE)) == NULL)
		return NULL;
	for (sector_size = MIN_SEC_SIZE; sector_size <= MAX_SEC_SIZE; sector_size *= 2) {
		if (read(fd, buf, sector_size) == sector_size) {
			d->sector_size = sector_size;
			break;
		}
	}
	free (buf);
	if (sector_size > MAX_SEC_SIZE) {
		DPRINT(("Int_Open_Disk: could not determine sector size, "
		     "calculated %u, max %u\n", sector_size, MAX_SEC_SIZE));
		/* could not determine sector size */
		goto bad;
	}
#endif /*HAVE_GEOM*/

#ifdef PC98
	p = (unsigned char*)read_block(fd, 1, d->sector_size);
#else
	p = read_block(fd, 0, d->sector_size);
	dp = (struct dos_partition*)(p + DOSPARTOFF);
	for (i = 0; i < NDOSPART; i++) {
		if (Read_Int32(&dp->dp_start) >= size)
		    continue;
		if (Read_Int32(&dp->dp_start) + Read_Int32(&dp->dp_size) >= size)
		    continue;
		if (!Read_Int32(&dp->dp_size))
		    continue;

		if (dp->dp_typ == DOSPTYP_ONTRACK) {
			d->flags |= DISK_ON_TRACK;
			offset = 63;
		}

	}
	free(p);
#endif

	d->bios_sect = dl.d_nsectors;
	d->bios_hd = dl.d_ntracks;

	d->name = strdup(name);


	if (dl.d_ntracks && dl.d_nsectors)
		d->bios_cyl = size / (dl.d_ntracks * dl.d_nsectors);

#ifdef PC98
	if (Add_Chunk(d, -offset, size, name, whole, 0, 0, "-"))
#else
	if (Add_Chunk(d, -offset, size, name, whole, 0, 0))
#endif
		DPRINT(("Failed to add 'whole' chunk"));

#ifdef __i386__
#ifdef PC98
	/* XXX -- Quick Hack!
	 * Check MS-DOS MO
	 */
	if ((*p == 0xf0 || *p == 0xf8) &&
	    (*(p+1) == 0xff) &&
	    (*(p+2) == 0xff)) {
		Add_Chunk(d, 0, size, name, fat, 0xa0a0, 0, name);
	    free(p);
	    goto pc98_mo_done;
	}
	free(p);
#endif /* PC98 */
	for(i=BASE_SLICE;i<ds.dss_nslices;i++) {
		char sname[20];
		char pname[20];
		chunk_e ce;
		u_long flags=0;
		int subtype=0;
		int j;

		if (! ds.dss_slices[i].ds_size)
			continue;
		ds.dss_slices[i].ds_offset -= offset;
		snprintf(sname, sizeof(sname), "%ss%d", name, i - 1);
#ifdef PC98
		subtype = ds.dss_slices[i].ds_type |
			ds.dss_slices[i].ds_subtype << 8;
		switch (ds.dss_slices[i].ds_type & 0x7f) {
			case 0x14:
				ce = freebsd;
				break;
			case 0x20:
			case 0x21:
			case 0x22:
			case 0x23:
			case 0x24:
				ce = fat;
				break;
#else /* IBM-PC */
		subtype = ds.dss_slices[i].ds_type;
		switch (ds.dss_slices[i].ds_type) {
			case 0xa5:
				ce = freebsd;
				break;
			case 0x1:
			case 0x6:
			case 0x4:
			case 0xb:
			case 0xc:
			case 0xe:
				ce = fat;
				break;
			case DOSPTYP_EXTENDED:
			case 0xf:
				ce = extended;
				break;
#endif
			default:
				ce = unknown;
				break;
		}
#ifdef PC98
		if (Add_Chunk(d, ds.dss_slices[i].ds_offset,
			ds.dss_slices[i].ds_size, sname, ce, subtype, flags,
			ds.dss_slices[i].ds_name))
#else
		if (Add_Chunk(d, ds.dss_slices[i].ds_offset,
			ds.dss_slices[i].ds_size, sname, ce, subtype, flags))
#endif
			DPRINT(("failed to add chunk for slice %d", i - 1));

#ifdef PC98
		if ((ds.dss_slices[i].ds_type & 0x7f) != 0x14)
#else
		if (ds.dss_slices[i].ds_type != 0xa5)
#endif
			continue;
#ifdef HAVE_GEOM
		if (xmlparse(confxml, "BSD", sname, assignToPartition, &dl) != 0) {
			DPRINTX(("Error parsing MBR geometry specification."));
			goto bad;
		}
#else
		{
		struct disklabel dl;
		int k;

		strlcpy(pname, _PATH_DEV, sizeof(pname));
		strlcat(pname, sname, sizeof(pname));
		j = open(pname, O_RDONLY);
		if (j < 0) {
			DPRINT(("open(%s)", pname));
			continue;
		}
		k = ioctl(j, DIOCGDINFO, &dl);
		if (k < 0) {
			DPRINT(("ioctl(%s, DIOCGDINFO)", pname));
			close(j);
			continue;
		}
		close(j);
		}
#endif /*HAVE_GEOM*/

		for(j = 0; j <= dl.d_npartitions; j++) {
			if (j == RAW_PART)
				continue;
			if (j == 3)
				continue;
			if (j == dl.d_npartitions) {
				j = 3;
				dl.d_npartitions = 0;
			}
			if (!dl.d_partitions[j].p_size)
				continue;
			if (dl.d_partitions[j].p_size +
			    dl.d_partitions[j].p_offset >
			    ds.dss_slices[i].ds_size)
				continue;
			snprintf(pname, sizeof(pname), "%s%c", sname, j + 'a');
			if (Add_Chunk(d,
				dl.d_partitions[j].p_offset +
				ds.dss_slices[i].ds_offset,
				dl.d_partitions[j].p_size,
				pname,part,
				dl.d_partitions[j].p_fstype,
#ifdef PC98
				0,
				ds.dss_slices[i].ds_name) && j != 3)
#else
				0) && j != 3)
#endif
				DPRINT((
			"Failed to add chunk for partition %c [%lu,%lu]",
			j + 'a', dl.d_partitions[j].p_offset,
			dl.d_partitions[j].p_size));
		}
	}
#endif /* __i386__ */
#ifdef __alpha__
	{
		struct disklabel dl;
		char pname[20];
		int j,k;

		strlcpy(pname, _PATH_DEV, sizeof(pname));
		strlcat(pname, name, sizeof(pname));
		j = open(pname, O_RDONLY);
		if (j < 0) {
			DPRINT(("open(%s)", pname));
			goto nolabel;
		}
		k = ioctl(j, DIOCGDINFO, &dl);
		if (k < 0) {
			DPRINT(("ioctl(%s, DIOCGDINFO)", pname));
			close(j);
			goto nolabel;
		}
		close(j);
		All_FreeBSD(d, 1);

		for(j = 0; j <= dl.d_npartitions; j++) {
			if (j == RAW_PART)
				continue;
			if (j == 3)
				continue;
			if (j == dl.d_npartitions) {
				j = 3;
				dl.d_npartitions = 0;
			}
			if (!dl.d_partitions[j].p_size)
				continue;
			if (dl.d_partitions[j].p_size +
			    dl.d_partitions[j].p_offset >
			    ds.dss_slices[WHOLE_DISK_SLICE].ds_size)
				continue;
			snprintf(pname, sizeof(pname), "%s%c", name, j + 'a');
			if (Add_Chunk(d,
				      dl.d_partitions[j].p_offset,
				      dl.d_partitions[j].p_size,
				      pname,part,
				      dl.d_partitions[j].p_fstype,
				      0) && j != 3)
				DPRINT((
					"Failed to add chunk for partition %c [%lu,%lu]",
					j + 'a', dl.d_partitions[j].p_offset,
					dl.d_partitions[j].p_size));
		}
	nolabel:;
	}
#endif /* __alpha__ */
#ifdef PC98
pc98_mo_done:
#endif
	close(fd);
	Fixup_Names(d);
	return d;
bad:
	if (confxml != NULL)
		free(confxml);
	if (fd >= 0)
		close(fd);
	return NULL;
}

void
Debug_Disk(struct disk *d)
{
	printf("Debug_Disk(%s)", d->name);
	printf("  flags=%lx", d->flags);
#if 0
	printf("  real_geom=%lu/%lu/%lu", d->real_cyl, d->real_hd, d->real_sect);
#endif
	printf("  bios_geom=%lu/%lu/%lu = %lu\n",
		d->bios_cyl, d->bios_hd, d->bios_sect,
		d->bios_cyl * d->bios_hd * d->bios_sect);
#if defined(PC98)
	printf("  boot1=%p, boot2=%p, bootipl=%p, bootmenu=%p\n",
		d->boot1, d->boot2, d->bootipl, d->bootmenu);
#elif defined(__i386__)
	printf("  boot1=%p, boot2=%p, bootmgr=%p\n",
		d->boot1, d->boot2, d->bootmgr);
#elif defined(__alpha__)
	printf("  boot1=%p, bootmgr=%p\n",
		d->boot1, d->bootmgr);
#endif
	Debug_Chunk(d->chunks);
}

void
Free_Disk(struct disk *d)
{
	if(d->chunks) Free_Chunk(d->chunks);
	if(d->name) free(d->name);
#ifdef PC98
	if(d->bootipl) free(d->bootipl);
	if(d->bootmenu) free(d->bootmenu);
#else
	if(d->bootmgr) free(d->bootmgr);
#endif
	if(d->boot1) free(d->boot1);
#if defined(__i386__)
	if(d->boot2) free(d->boot2);
#endif
	free(d);
}

struct disk *
Clone_Disk(struct disk *d)
{
	struct disk *d2;

	d2 = (struct disk*) malloc(sizeof *d2);
	if(!d2) return NULL;
	*d2 = *d;
	d2->name = strdup(d2->name);
	d2->chunks = Clone_Chunk(d2->chunks);
#ifdef PC98
	if(d2->bootipl) {
		d2->bootipl = malloc(d2->bootipl_size);
		memcpy(d2->bootipl, d->bootipl, d2->bootipl_size);
	}
	if(d2->bootmenu) {
		d2->bootmenu = malloc(d2->bootmenu_size);
		memcpy(d2->bootmenu, d->bootmenu, d2->bootmenu_size);
	}
#else
	if(d2->bootmgr) {
		d2->bootmgr = malloc(d2->bootmgr_size);
		memcpy(d2->bootmgr, d->bootmgr, d2->bootmgr_size);
	}
#endif
#if defined(__i386__)
	if(d2->boot1) {
		d2->boot1 = malloc(512);
		memcpy(d2->boot1, d->boot1, 512);
	}
	if(d2->boot2) {
		d2->boot2 = malloc(512 * 15);
		memcpy(d2->boot2, d->boot2, 512 * 15);
	}
#elif defined(__alpha__)
	if(d2->boot1) {
		d2->boot1 = malloc(512 * 15);
		memcpy(d2->boot1, d->boot1, 512 * 15);
	}
#endif
	return d2;
}

#if 0
void
Collapse_Disk(struct disk *d)
{

	while(Collapse_Chunk(d, d->chunks))
		;
}
#endif

#ifdef PC98
static char * device_list[] = {"wd", "aacd", "ad", "da", "afd", "fla", "idad", "mlxd", "amrd", "twed", "ar", "fd", 0};
#else
static char * device_list[] = {"aacd", "ad", "da", "afd", "fla", "idad", "mlxd", "amrd", "twed", "ar", "fd", 0};
#endif

int qstrcmp(const void* a, const void* b) {

	char *str1 = *(char**)a;
	char *str2 = *(char**)b;
	return strcmp(str1, str2);

}

char **
Disk_Names()
{
    int i,j,disk_cnt;
    char disk[25];
    char diskname[25];
    struct stat st;
    struct diskslices ds;
    int fd;
    static char **disks;
    int error;
    size_t listsize;
    char *disklist;

    disks = malloc(sizeof *disks * (1 + MAX_NO_DISKS));
    if (disks == NULL)
	    return NULL;
    memset(disks,0,sizeof *disks * (1 + MAX_NO_DISKS));
    error = sysctlbyname("kern.disks", NULL, &listsize, NULL, 0);
    if (!error) {
	    disklist = (char *)malloc(listsize+1);
	    if (disklist == NULL) {
		    free(disks);
		    return NULL;
	    }
	    memset(disklist, 0, listsize+1);
	    error = sysctlbyname("kern.disks", disklist, &listsize, NULL, 0);
	    if (error) {
		    free(disklist);
		    free(disks);
		    return NULL;
	    }
	    for (disk_cnt = 0; disk_cnt < MAX_NO_DISKS; disk_cnt++) {
		    disks[disk_cnt] = strsep(&disklist, " ");
		    if (disks[disk_cnt] == NULL)
			    break;
											    }
    } else {
    warn("kern.disks sysctl not available");
    disk_cnt = 0;
	for (j = 0; device_list[j]; j++) {
		if(disk_cnt >= MAX_NO_DISKS)
			break;
		for (i = 0; i < MAX_NO_DISKS; i++) {
			snprintf(diskname, sizeof(diskname), "%s%d",
				device_list[j], i);
			snprintf(disk, sizeof(disk), _PATH_DEV"%s", diskname);
			if (stat(disk, &st) || !(st.st_mode & S_IFCHR))
				continue;
			if ((fd = open(disk, O_RDWR)) == -1)
				continue;
			if (ioctl(fd, DIOCGSLICEINFO, &ds) == -1) {
				DPRINT(("DIOCGSLICEINFO %s", disk));
				close(fd);
				continue;
			}
			close(fd);
			disks[disk_cnt++] = strdup(diskname);
			if(disk_cnt >= MAX_NO_DISKS)
				break;
		}
	}
    }
    qsort(disks, disk_cnt, sizeof(char*), qstrcmp);
    
    return disks;
}

#ifdef PC98
void
Set_Boot_Mgr(struct disk *d, const u_char *bootipl, const size_t bootipl_size,
	     const u_char *bootmenu, const size_t bootmenu_size)
#else
void
Set_Boot_Mgr(struct disk *d, const u_char *b, const size_t s)
#endif
{
#ifdef PC98
	if (bootipl_size % d->sector_size != 0)
		return;
	if (d->bootipl)
		free(d->bootipl);
	if (!bootipl) {
		d->bootipl = NULL;
	} else {
		d->bootipl_size = bootipl_size;
		d->bootipl = malloc(bootipl_size);
		if(!d->bootipl) return;
		memcpy(d->bootipl, bootipl, bootipl_size);
	}

	if (bootmenu_size % d->sector_size != 0)
		return;
	if (d->bootmenu)
		free(d->bootmenu);
	if (!bootmenu) {
		d->bootmenu = NULL;
	} else {
		d->bootmenu_size = bootmenu_size;
		d->bootmenu = malloc(bootmenu_size);
		if(!d->bootmenu) return;
		memcpy(d->bootmenu, bootmenu, bootmenu_size);
	}
#else
	if (s % d->sector_size != 0)
		return;
	if (d->bootmgr)
		free(d->bootmgr);
	if (!b) {
		d->bootmgr = NULL;
	} else {
		d->bootmgr_size = s;
		d->bootmgr = malloc(s);
		if(!d->bootmgr) return;
		memcpy(d->bootmgr, b, s);
	}
#endif
}

int
Set_Boot_Blocks(struct disk *d, const u_char *b1, const u_char *b2)
{
#if defined(__i386__)
	if (d->boot1) free(d->boot1);
	d->boot1 = malloc(512);
	if(!d->boot1) return -1;
	memcpy(d->boot1, b1, 512);
	if (d->boot2) free(d->boot2);
	d->boot2 = malloc(15 * 512);
	if(!d->boot2) return -1;
	memcpy(d->boot2, b2, 15 * 512);
#elif defined(__alpha__)
	if (d->boot1) free(d->boot1);
	d->boot1 = malloc(15 * 512);
	if(!d->boot1) return -1;
	memcpy(d->boot1, b1, 15 * 512);
#endif
	return 0;
}

const char *
slice_type_name( int type, int subtype )
{
	switch (type) {
		case 0:		return "whole";
#ifndef	PC98
		case 1:		switch (subtype) {
					case 1:		return "fat (12-bit)";
					case 2:		return "XENIX /";
					case 3:		return "XENIX /usr";
					case 4:         return "fat (16-bit,<=32Mb)";
					case 5:		return "extended DOS";
					case 6:         return "fat (16-bit,>32Mb)";
					case 7:         return "NTFS/HPFS/QNX";
					case 8:         return "AIX bootable";
					case 9:         return "AIX data";
					case 10:	return "OS/2 bootmgr";
					case 11:        return "fat (32-bit)";
					case 12:        return "fat (32-bit,LBA)";
					case 14:        return "fat (16-bit,>32Mb,LBA)";
					case 15:        return "extended DOS, LBA";
					case 18:        return "Compaq Diagnostic";
					case 84:	return "OnTrack diskmgr";
					case 100:	return "Netware 2.x";
					case 101:	return "Netware 3.x";
					case 115:	return "SCO UnixWare";
					case 128:	return "Minix 1.1";
					case 129:	return "Minix 1.5";
					case 130:	return "linux_swap";
					case 131:	return "ext2fs";
					case 166:	return "OpenBSD FFS";	/* 0xA6 */
					case 169:	return "NetBSD FFS";	/* 0xA9 */
					case 182:	return "OpenBSD";		/* dedicated */
					case 183:	return "bsd/os";
					case 184:	return "bsd/os swap";
					case 238:	return "EFI GPT";
					case 239:	return "EFI Sys. Part.";
					default:	return "unknown";
				}
#endif
		case 2:		return "fat";
		case 3:		switch (subtype) {
#ifdef	PC98
					case 0xc494:	return "freebsd";
#else
					case 165:	return "freebsd";
#endif
					default:	return "unknown";
				}
#ifndef	PC98
		case 4:		return "extended";
		case 5:		return "part";
		case 6:		return "unused";
#endif
		default:	return "unknown";
	}
}
