/*
 * Copyright (c) 2003
 *	Bill Paul <wpaul@windriver.com>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <net/if.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>
#include <err.h>
#include <ctype.h>

#include <compat/ndis/pe_var.h>

#include "inf.h"

static int insert_padding(void **, int *);
extern const char *__progname;

/*
 * Sections in object code files can be sparse. That is, the
 * section may occupy more space in memory that it does when
 * stored in a disk file. In Windows PE files, each section header
 * has a 'virtual size' and 'raw data size' field. The latter
 * specifies the amount of section data actually stored in the
 * disk file, and the former describes how much space the section
 * should actually occupy in memory. If the vsize is larger than
 * the rsize, we need to allocate some extra storage and fill
 * it with zeros. (Think BSS.)
 *
 * The typical method of loading an executable file involves
 * reading each segment into memory using the vaddr/vsize from
 * each section header. We try to make a small optimization however
 * and only pad/move segments when it's absolutely necessary, i.e.
 * if the vsize is larger than the rsize. This conserves a little
 * bit of memory, at the cost of having to fixup some of the values
 * in the section headers.
 */

#define ROUND_UP(x, y)	\
	(((x) + (y)) - ((x) % (y)))

#define SET_HDRS(x)	\
	dos_hdr = (image_dos_header *)x;				\
	nt_hdr = (image_nt_header *)(x + dos_hdr->idh_lfanew);		\
	sect_hdr = (image_section_header *)((vm_offset_t)nt_hdr +	\
	    sizeof(image_nt_header));

static
int insert_padding(imgbase, imglen)
	void			**imgbase;
	int			*imglen;
{
        image_section_header	*sect_hdr;
        image_dos_header	*dos_hdr;
        image_nt_header		*nt_hdr;
	image_optional_header	opt_hdr;
        int			i = 0, sections, curlen = 0;
	int			offaccum = 0, diff, oldraddr, oldrlen;
	uint8_t			*newimg, *tmp;

	newimg = malloc(*imglen);

	if (newimg == NULL)
		return(ENOMEM);

	bcopy(*imgbase, newimg, *imglen);
	curlen = *imglen;

	if (pe_get_optional_header((vm_offset_t)newimg, &opt_hdr))
		return(0);

        sections = pe_numsections((vm_offset_t)newimg);

	SET_HDRS(newimg);

	for (i = 0; i < sections; i++) {
		/*
		 * If we have accumulated any padding offset,
		 * add it to the raw data address of this segment.
		 */
		oldraddr = sect_hdr->ish_rawdataaddr;
		oldrlen = sect_hdr->ish_rawdatasize;
		if (offaccum)
			sect_hdr->ish_rawdataaddr += offaccum;
		if (sect_hdr->ish_misc.ish_vsize >
		    sect_hdr->ish_rawdatasize) {
			diff = ROUND_UP(sect_hdr->ish_misc.ish_vsize -
			    sect_hdr->ish_rawdatasize,
			    opt_hdr.ioh_filealign);
			offaccum += ROUND_UP(diff -
			    (sect_hdr->ish_misc.ish_vsize -
			    sect_hdr->ish_rawdatasize),
			    opt_hdr.ioh_filealign);
			sect_hdr->ish_rawdatasize =
			    ROUND_UP(sect_hdr->ish_rawdatasize,
			    opt_hdr.ioh_filealign);
			tmp = realloc(newimg, *imglen + offaccum);
			if (tmp == NULL) {
				free(newimg);
				return(ENOMEM);
			}
			newimg = tmp;
			SET_HDRS(newimg);
			sect_hdr += i;
		}
		bzero(newimg + sect_hdr->ish_rawdataaddr,
		    ROUND_UP(sect_hdr->ish_misc.ish_vsize,
		    opt_hdr.ioh_filealign));
		bcopy((uint8_t *)(*imgbase) + oldraddr,
		    newimg + sect_hdr->ish_rawdataaddr, oldrlen);
		sect_hdr++;
	}

	free(*imgbase);

	*imgbase = newimg;
	*imglen += offaccum;

	return(0);
}

static void
usage(void)
{
	fprintf(stderr, "Usage: %s [-O] [-i <inffile>] -s <sysfile> "
	    "[-n devname] [-o outfile]\n", __progname);
	fprintf(stderr, "       %s -f <firmfile>\n", __progname);

	exit(1);
}

static void
bincvt(char *sysfile, char *outfile, void *img, int fsize)
{
	char			*ptr;
	char			tname[] = "/tmp/ndiscvt.XXXXXX";
	char			sysbuf[1024];
	FILE			*binfp;

	mkstemp(tname);

	binfp = fopen(tname, "a+");
	if (binfp == NULL)
		err(1, "opening %s failed", tname);

	if (fwrite(img, fsize, 1, binfp) != 1)
		err(1, "failed to output binary image");

	fclose(binfp);

	outfile = strdup(basename(outfile));
	if (strchr(outfile, '.'))
		*strchr(outfile, '.') = '\0';

	snprintf(sysbuf, sizeof(sysbuf),
	    "objcopy -I binary -O elf32-i386-freebsd -B i386 %s %s.o\n",
	    tname, outfile);
	printf("%s", sysbuf);
	system(sysbuf);
	unlink(tname);

	ptr = tname;
	while (*ptr) {
		if (*ptr == '/' || *ptr == '.')
			*ptr = '_';
		ptr++;
	}

	snprintf(sysbuf, sizeof(sysbuf),
	    "objcopy --redefine-sym _binary_%s_start=%s_drv_data_start "
	    "--strip-symbol _binary_%s_size "
	    "--redefine-sym _binary_%s_end=%s_drv_data_end %s.o %s.o\n",
	    tname, sysfile, tname, tname, sysfile, outfile, outfile);
	printf("%s", sysbuf);
	system(sysbuf);

	return;
}
   
static void
firmcvt(char *firmfile)
{
	char			*basefile, *outfile, *ptr;
	char			sysbuf[1024];

	outfile = basename(firmfile);
	basefile = strdup(outfile);

	snprintf(sysbuf, sizeof(sysbuf),
	    "objcopy -I binary -O elf32-i386-freebsd -B i386 %s %s.o\n",
	    firmfile, outfile);
	printf("%s", sysbuf);
	system(sysbuf);

	ptr = firmfile;
	while (*ptr) {
		if (*ptr == '/' || *ptr == '.')
			*ptr = '_';
		ptr++;
	}
	ptr = basefile;
	while (*ptr) {
		if (*ptr == '/' || *ptr == '.')
			*ptr = '_';
		else
			*ptr = tolower(*ptr);
		ptr++;
	}

	snprintf(sysbuf, sizeof(sysbuf),
	    "objcopy --redefine-sym _binary_%s_start=%s_start "
	    "--strip-symbol _binary_%s_size "
	    "--redefine-sym _binary_%s_end=%s_end %s.o %s.o\n",
	    firmfile, basefile, firmfile, firmfile,
	    basefile, outfile, outfile);
	ptr = sysbuf;
	printf("%s", sysbuf);
	system(sysbuf);

	snprintf(sysbuf, sizeof(sysbuf),
	    "ld -Bshareable -d -warn-common -o %s.ko %s.o\n",
	    outfile, outfile);
	printf("%s", sysbuf);
	system(sysbuf);

	free(basefile);

	exit(0);
}

int
main(int argc, char *argv[])
{
	FILE			*fp, *outfp;
	int			i, bin = 0;
	void			*img;
	int			n, fsize, cnt;
	unsigned char		*ptr;
	char			*inffile = NULL, *sysfile = NULL;
	char			*outfile = NULL, *firmfile = NULL;
	char			*dname = NULL;
	int			ch;

	while((ch = getopt(argc, argv, "i:s:o:n:f:O")) != -1) {
		switch(ch) {
		case 'f':
			firmfile = optarg;
			break;
		case 'i':
			inffile = optarg;
			break;
		case 's':
			sysfile = optarg;
			break;
		case 'o':
			outfile = optarg;
			break;
		case 'n':
			dname = optarg;
			break;
		case 'O':
			bin = 1;
			break;
		default:
			usage();
			break;
		}
	}

	if (firmfile != NULL)
		firmcvt(firmfile);

	if (sysfile == NULL)
		usage();

	/* Open the .SYS file and load it into memory */
	fp = fopen(sysfile, "r");
	if (fp == NULL)
		err(1, "opening .SYS file '%s' failed", sysfile);
	fseek (fp, 0L, SEEK_END);
	fsize = ftell (fp);
	rewind (fp);
	img = calloc(fsize, 1);
	n = fread (img, fsize, 1, fp);

	fclose(fp);

	if (insert_padding(&img, &fsize)) {
		fprintf(stderr, "section relocation failed\n");
		exit(1);
	}

	if (outfile == NULL || strcmp(outfile, "-") == 0)
		outfp = stdout;
	else {
		outfp = fopen(outfile, "w");
		if (outfp == NULL)
			err(1, "opening output file '%s' failed", outfile);
	}

	fprintf(outfp, "\n/*\n");
	fprintf(outfp, " * Generated from %s and %s (%d bytes)\n",
	    inffile == NULL ? "<notused>" : inffile, sysfile, fsize);
	fprintf(outfp, " */\n\n");

	if (dname != NULL) {
		if (strlen(dname) > IFNAMSIZ)
			err(1, "selected device name '%s' is "
			    "too long (max chars: %d)", dname, IFNAMSIZ);
		fprintf (outfp, "#define NDIS_DEVNAME \"%s\"\n", dname);
		fprintf (outfp, "#define NDIS_MODNAME %s\n\n", dname);
	}

	if (inffile == NULL) {
		fprintf (outfp, "#ifdef NDIS_REGVALS\n");
		fprintf (outfp, "ndis_cfg ndis_regvals[] = {\n");
        	fprintf (outfp, "\t{ NULL, NULL, { 0 }, 0 }\n");
		fprintf (outfp, "#endif /* NDIS_REGVALS */\n");

		fprintf (outfp, "};\n\n");
	} else {
		fp = fopen(inffile, "r");
		if (fp == NULL)
			err(1, "opening .INF file '%s' failed", inffile);


		inf_parse(fp, outfp);
		fclose(fp);
	}

	fprintf(outfp, "\n#ifdef NDIS_IMAGE\n");

	if (bin) {
		sysfile = strdup(basename(sysfile));
		ptr = sysfile;
		while (*ptr) {
			if (*ptr == '.')
				*ptr = '_';
			ptr++;
		}
		fprintf(outfp,
		    "\nextern unsigned char %s_drv_data_start[];\n",
		    sysfile);
		fprintf(outfp, "static unsigned char *drv_data = "
		    "%s_drv_data_start;\n\n", sysfile);
		bincvt(sysfile, outfile, img, fsize);
		goto done;
	}


	fprintf(outfp, "\nextern unsigned char drv_data[];\n\n");

	fprintf(outfp, "__asm__(\".data\");\n");
	fprintf(outfp, "__asm__(\".type   drv_data, @object\");\n");
	fprintf(outfp, "__asm__(\".size   drv_data, %d\");\n", fsize);
	fprintf(outfp, "__asm__(\"drv_data:\");\n");

	ptr = img;
	cnt = 0;
	while(cnt < fsize) {
		fprintf (outfp, "__asm__(\".byte ");
		for (i = 0; i < 10; i++) {
			cnt++;
			if (cnt == fsize) {
				fprintf(outfp, "0x%.2X\");\n", ptr[i]);
				goto done;
			} else {
				if (i == 9)
					fprintf(outfp, "0x%.2X\");\n", ptr[i]);
				else
					fprintf(outfp, "0x%.2X, ", ptr[i]);
			}
		}
		ptr += 10;
	}

done:

	fprintf(outfp, "#endif /* NDIS_IMAGE */\n");

	if (fp != NULL)
		fclose(fp);
	fclose(outfp);
	free(img);
	exit(0);
}
