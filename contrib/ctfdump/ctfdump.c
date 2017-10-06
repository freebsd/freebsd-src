/*
 * Copyright (c) 2016 Martin Pieuchot <mpi@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/elf.h>
#include <sys/mman.h>
#include <sys/ctf.h>

#include <err.h>
#include <fcntl.h>
#include <locale.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef ZLIB
#include <zlib.h>
#endif /* ZLIB */

#ifndef nitems
#define nitems(_a)	(sizeof((_a)) / sizeof((_a)[0]))
#endif

#ifndef ELF_STRTAB
#define ELF_STRTAB ".strtab"
#endif
#ifndef ELF_CTF
#define ELF_CTF ".SUNW_ctf"
#endif

#define DUMP_OBJECT	(1 << 0)
#define DUMP_FUNCTION	(1 << 1)
#define DUMP_HEADER	(1 << 2)
#define DUMP_LABEL	(1 << 3)
#define DUMP_STRTAB	(1 << 4)
#define DUMP_STATISTIC	(1 << 5)
#define DUMP_TYPE	(1 << 6)

int		 dump(const char *, uint8_t);
int		 isctf(const char *, size_t);
__dead2 void	 usage(void);

int		 ctf_dump(const char *, size_t, uint8_t);
uint32_t	 ctf_dump_type(struct ctf_header *, const char *, off_t,
		     uint32_t, uint32_t);
const char	*ctf_kind2name(uint16_t);
const char	*ctf_enc2name(uint16_t);
const char	*ctf_off2name(struct ctf_header *, const char *, off_t,
		     uint32_t);

int		 elf_dump(char *, size_t, uint8_t);
const char	*elf_idx2sym(size_t *, uint8_t);

/* elf.c */
int		 iself(const char *, size_t);
int		 elf_getshstab(const char *, size_t, const char **, size_t *);
ssize_t		 elf_getsymtab(const char *, const char *, size_t,
		     const Elf_Sym **, size_t *);
ssize_t		 elf_getsection(char *, const char *, const char *,
		     size_t, const char **, size_t *);

char		*decompress(const char *, size_t, size_t);

int
main(int argc, char *argv[])
{
	const char *filename;
	uint8_t flags = 0;
	int ch, error = 0;

	setlocale(LC_ALL, "");

	while ((ch = getopt(argc, argv, "dfhlst")) != -1) {
		switch (ch) {
		case 'd':
			flags |= DUMP_OBJECT;
			break;
		case 'f':
			flags |= DUMP_FUNCTION;
			break;
		case 'h':
			flags |= DUMP_HEADER;
			break;
		case 'l':
			flags |= DUMP_LABEL;
			break;
		case 's':
			flags |= DUMP_STRTAB;
			break;
		case 't':
			flags |= DUMP_TYPE;
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc <= 0)
		usage();

	/* Dump everything by default */
	if (flags == 0)
		flags = 0xff;

	while ((filename = *argv++) != NULL)
		error |= dump(filename, flags);

	return error;
}

int
dump(const char *path, uint8_t flags)
{
	struct stat		 st;
	int			 fd, error = 1;
	char			*p;

	fd = open(path, O_RDONLY);
	if (fd == -1) {
		warn("open");
		return 1;
	}
	if (fstat(fd, &st) == -1) {
		warn("fstat");
		return 1;
	}
	if ((uintmax_t)st.st_size > SIZE_MAX) {
		warnx("file too big to fit memory");
		return 1;
	}

	p = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (p == MAP_FAILED)
		err(1, "mmap");

	if (iself(p, st.st_size)) {
		error = elf_dump(p, st.st_size, flags);
	} else if (isctf(p, st.st_size)) {
		error = ctf_dump(p, st.st_size, flags);
	}

	munmap(p, st.st_size);
	close(fd);

	return error;
}

const char		*strtab;
const Elf_Sym		*symtab;
size_t			 strtabsz, nsymb;

const char *
elf_idx2sym(size_t *idx, uint8_t type)
{
	const Elf_Sym	*st;
	size_t		 i;

	for (i = *idx + 1; i < nsymb; i++) {
		st = &symtab[i];

		if (ELF_ST_TYPE(st->st_info) != type)
			continue;

		*idx = i;
		return strtab + st->st_name;
	}

	return NULL;
}

int
elf_dump(char *p, size_t filesize, uint8_t flags)
{
	Elf_Ehdr		*eh = (Elf_Ehdr *)p;
	Elf_Shdr		*sh;
	const char		*shstab;
	size_t			 i, shstabsz;

	/* Find section header string table location and size. */
	if (elf_getshstab(p, filesize, &shstab, &shstabsz))
		return 1;

	/* Find symbol table location and number of symbols. */
	if (elf_getsymtab(p, shstab, shstabsz, &symtab, &nsymb) == -1)
		warnx("symbol table not found");

	/* Find string table location and size. */
	if (elf_getsection(p, ELF_STRTAB, shstab, shstabsz, &strtab,
	    &strtabsz) == -1)
		warnx("string table not found");

	/* Find CTF section and dump it. */
	for (i = 0; i < eh->e_shnum; i++) {
		sh = (Elf_Shdr *)(p + eh->e_shoff + i * eh->e_shentsize);

		if ((sh->sh_link >= eh->e_shnum) ||
		    (sh->sh_name >= shstabsz))
			continue;

		if (strncmp(shstab + sh->sh_name, ELF_CTF, strlen(ELF_CTF)))
			continue;

		if (!isctf(p + sh->sh_offset, sh->sh_size))
			break;

		return ctf_dump(p + sh->sh_offset, sh->sh_size, flags);
	}

	warnx("%s section not found", ELF_CTF);
	return 1;
}

int
isctf(const char *p, size_t filesize)
{
	struct ctf_header	*cth = (struct ctf_header *)p;
	size_t 			 dlen;

	if (filesize < sizeof(struct ctf_header)) {
		warnx("file too small to be CTF");
		return 0;
	}

	if (cth->cth_magic != CTF_MAGIC || cth->cth_version != CTF_VERSION)
		return 0;

	dlen = cth->cth_stroff + cth->cth_strlen;
	if (dlen > filesize && !(cth->cth_flags & CTF_F_COMPRESS)) {
		warnx("bogus file size");
		return 0;
	}

	if ((cth->cth_lbloff & 3) || (cth->cth_objtoff & 1) ||
	    (cth->cth_funcoff & 1) || (cth->cth_typeoff & 3)) {
		warnx("wrongly aligned offset");
		return 0;
	}

	if ((cth->cth_lbloff >= dlen) || (cth->cth_objtoff >= dlen) ||
	    (cth->cth_funcoff >= dlen) || (cth->cth_typeoff >= dlen)) {
		warnx("truncated file");
		return 0;
	}

	if ((cth->cth_lbloff > cth->cth_objtoff) ||
	    (cth->cth_objtoff > cth->cth_funcoff) ||
	    (cth->cth_funcoff > cth->cth_typeoff) ||
	    (cth->cth_typeoff > cth->cth_stroff)) {
		warnx("corrupted file");
		return 0;
	}

	return 1;
}

int
ctf_dump(const char *p, size_t size, uint8_t flags)
{
	struct ctf_header	*cth = (struct ctf_header *)p;
	off_t 			 dlen = cth->cth_stroff + cth->cth_strlen;
	char			*data;

	if (cth->cth_flags & CTF_F_COMPRESS) {
		data = decompress(p + sizeof(*cth), size - sizeof(*cth), dlen);
		if (data == NULL)
			return 1;
	} else {
		data = (char *)p + sizeof(*cth);
	}

	if (flags & DUMP_HEADER) {
		printf("  cth_magic    = 0x%04x\n", cth->cth_magic);
		printf("  cth_version  = %d\n", cth->cth_version);
		printf("  cth_flags    = 0x%02x\n", cth->cth_flags);
		printf("  cth_parlabel = %s\n",
		    ctf_off2name(cth, data, dlen, cth->cth_parname));
		printf("  cth_parname  = %s\n",
		    ctf_off2name(cth, data, dlen, cth->cth_parname));
		printf("  cth_lbloff   = %d\n", cth->cth_lbloff);
		printf("  cth_objtoff  = %d\n", cth->cth_objtoff);
		printf("  cth_funcoff  = %d\n", cth->cth_funcoff);
		printf("  cth_typeoff  = %d\n", cth->cth_typeoff);
		printf("  cth_stroff   = %d\n", cth->cth_stroff);
		printf("  cth_strlen   = %d\n", cth->cth_strlen);
		printf("\n");
	}

	if (flags & DUMP_LABEL) {
		uint32_t		 lbloff = cth->cth_lbloff;
		struct ctf_lblent	*ctl;

		while (lbloff < cth->cth_objtoff) {
			ctl = (struct ctf_lblent *)(data + lbloff);

			printf("  %5u %s\n", ctl->ctl_typeidx,
			    ctf_off2name(cth, data, dlen, ctl->ctl_label));

			lbloff += sizeof(*ctl);
		}
		printf("\n");
	}

	if (flags & DUMP_OBJECT) {
		uint32_t		 objtoff = cth->cth_objtoff;
		size_t			 idx = 0, i = 0;
		uint16_t		*dsp;
		const char		*s;
		int			 l;

		while (objtoff < cth->cth_funcoff) {
			dsp = (uint16_t *)(data + objtoff);

			l = printf("  [%zu] %u", i++, *dsp);
			if ((s = elf_idx2sym(&idx, STT_OBJECT)) != NULL)
				printf("%*s %s (%zu)\n", (14 - l), "", s, idx);
			else
				printf("\n");

			objtoff += sizeof(*dsp);
		}
		printf("\n");
	}

	if (flags & DUMP_FUNCTION) {
		uint16_t		*fsp, kind, vlen;
		size_t			 idx = 0, i = -1;
		const char		*s;
		int			 l;

		fsp = (uint16_t *)(data + cth->cth_funcoff);
		while (fsp < (uint16_t *)(data + cth->cth_typeoff)) {
			kind = CTF_INFO_KIND(*fsp);
			vlen = CTF_INFO_VLEN(*fsp);
			s = elf_idx2sym(&idx, STT_FUNC);
			fsp++;
			i++;

			if (kind == CTF_K_UNKNOWN && vlen == 0)
				continue;

			l = printf("  [%zu] FUNC ", i);
			if (s != NULL)
				printf("(%s)", s);
			printf(" returns: %u args: (", *fsp++);
			while (vlen-- > 0)
				printf("%u%s", *fsp++, (vlen > 0) ? ", " : "");
			printf(")\n");
		}
		printf("\n");
	}

	if (flags & DUMP_TYPE) {
		uint32_t		 idx = 1, offset = cth->cth_typeoff;

		while (offset < cth->cth_stroff) {
			offset += ctf_dump_type(cth, data, dlen, offset, idx++);
		}
		printf("\n");
	}

	if (flags & DUMP_STRTAB) {
		uint32_t		 offset = 0;
		const char		*str;

		while (offset < cth->cth_strlen) {
			str = ctf_off2name(cth, data, dlen, offset);

			printf("  [%u] ", offset);
			if (strcmp(str, "(anon)"))
				offset += printf("%s\n", str);
			else {
				printf("\\0\n");
				offset++;
			}
		}
		printf("\n");
	}

	if (cth->cth_flags & CTF_F_COMPRESS)
		free(data);

	return 0;
}

uint32_t
ctf_dump_type(struct ctf_header *cth, const char *data, off_t dlen,
    uint32_t offset, uint32_t idx)
{
	const char		*p = data + offset;
	const struct ctf_type	*ctt = (struct ctf_type *)p;
	const struct ctf_array	*cta;
	uint16_t		*argp, i, kind, vlen, root;
	uint32_t		 eob, toff;
	uint64_t		 size;
	const char		*name, *kname;

	kind = CTF_INFO_KIND(ctt->ctt_info);
	vlen = CTF_INFO_VLEN(ctt->ctt_info);
	root = CTF_INFO_ISROOT(ctt->ctt_info);
	name = ctf_off2name(cth, data, dlen, ctt->ctt_name);

	if (root)
		printf("  <%u> ", idx);
	else
		printf("  [%u] ", idx);

	if ((kname = ctf_kind2name(kind)) != NULL)
		printf("%s %s", kname, name);

	if (ctt->ctt_size <= CTF_MAX_SIZE) {
		size = ctt->ctt_size;
		toff = sizeof(struct ctf_stype);
	} else {
		size = CTF_TYPE_LSIZE(ctt);
		toff = sizeof(struct ctf_type);
	}

	switch (kind) {
	case CTF_K_UNKNOWN:
	case CTF_K_FORWARD:
		break;
	case CTF_K_INTEGER:
		eob = *((uint32_t *)(p + toff));
		toff += sizeof(uint32_t);
		printf(" encoding=%s offset=%u bits=%u",
		    ctf_enc2name(CTF_INT_ENCODING(eob)), CTF_INT_OFFSET(eob),
		    CTF_INT_BITS(eob));
		break;
	case CTF_K_FLOAT:
		eob = *((uint32_t *)(p + toff));
		toff += sizeof(uint32_t);
		printf(" encoding=0x%x offset=%u bits=%u",
		    CTF_FP_ENCODING(eob), CTF_FP_OFFSET(eob), CTF_FP_BITS(eob));
		break;
	case CTF_K_ARRAY:
		cta = (struct ctf_array *)(p + toff);
		printf(" content: %u index: %u nelems: %u\n", cta->cta_contents,
		    cta->cta_index, cta->cta_nelems);
		toff += sizeof(struct ctf_array);
		break;
	case CTF_K_FUNCTION:
		argp = (uint16_t *)(p + toff);
		printf(" returns: %u args: (%u", ctt->ctt_type, *argp);
		for (i = 1; i < vlen; i++) {
			argp++;
			printf(", %u", *argp);
		}
		printf(")");
		toff += (vlen + (vlen & 1)) * sizeof(uint16_t);
		break;
	case CTF_K_STRUCT:
	case CTF_K_UNION:
		printf(" (%lu bytes)\n", size);

		if (size < CTF_LSTRUCT_THRESH) {
			for (i = 0; i < vlen; i++) {
				struct ctf_member	*ctm;

				ctm = (struct ctf_member *)(p + toff);
				toff += sizeof(struct ctf_member);

				printf("\t%s type=%u off=%u\n",
				    ctf_off2name(cth, data, dlen,
					ctm->ctm_name),
				    ctm->ctm_type, ctm->ctm_offset);
			}
		} else {
			for (i = 0; i < vlen; i++) {
				struct ctf_lmember	*ctlm;

				ctlm = (struct ctf_lmember *)(p + toff);
				toff += sizeof(struct ctf_lmember);

				printf("\t%s type=%u off=%zu\n",
				    ctf_off2name(cth, data, dlen,
					ctlm->ctlm_name),
				    ctlm->ctlm_type, CTF_LMEM_OFFSET(ctlm));
			}
		}
		break;
	case CTF_K_ENUM:
		printf("\n");
		for (i = 0; i < vlen; i++) {
			struct ctf_enum	*cte;

			cte = (struct ctf_enum *)(p + toff);
			toff += sizeof(struct ctf_enum);

			printf("\t%s = %d\n",
			    ctf_off2name(cth, data, dlen, cte->cte_name),
			    cte->cte_value);
		}
		break;
	case CTF_K_POINTER:
	case CTF_K_TYPEDEF:
	case CTF_K_VOLATILE:
	case CTF_K_CONST:
	case CTF_K_RESTRICT:
		printf(" refers to %u", ctt->ctt_type);
		break;
	default:
		errx(1, "incorrect type %u at offset %u", kind, offset);
	}

	printf("\n");

	return toff;
}

const char *
ctf_kind2name(uint16_t kind)
{
	static const char *kind_name[] = { NULL, "INTEGER", "FLOAT", "POINTER",
	   "ARRAY", "FUNCTION", "STRUCT", "UNION", "ENUM", "FORWARD",
	   "TYPEDEF", "VOLATILE", "CONST", "RESTRICT" };

	if (kind >= nitems(kind_name))
		return NULL;

	return kind_name[kind];
}

const char *
ctf_enc2name(uint16_t enc)
{
	static const char *enc_name[] = { "SIGNED", "CHAR", "SIGNED CHAR",
	    "BOOL", "SIGNED BOOL" };
	static char invalid[7];

	if (enc == CTF_INT_VARARGS)
		return "VARARGS";

	if (enc > 0 && enc < nitems(enc_name))
		return enc_name[enc - 1];

	snprintf(invalid, sizeof(invalid), "0x%x", enc);
	return invalid;
}

const char *
ctf_off2name(struct ctf_header *cth, const char *data, off_t dlen,
    uint32_t offset)
{
	const char		*name;

	if (CTF_NAME_STID(offset) != CTF_STRTAB_0)
		return "external";

	if (CTF_NAME_OFFSET(offset) >= cth->cth_strlen)
		return "exceeds strlab";

	if (cth->cth_stroff + CTF_NAME_OFFSET(offset) >= dlen)
		return "invalid";

	name = data + cth->cth_stroff + CTF_NAME_OFFSET(offset);
	if (*name == '\0')
		return "(anon)";

	return name;
}

char *
decompress(const char *buf, size_t size, size_t len)
{
#ifdef ZLIB
	z_stream		 stream;
	char			*data;
	int			 error;

	data = malloc(len);
	if (data == NULL) {
		warn(NULL);
		return NULL;
	}

	memset(&stream, 0, sizeof(stream));
	stream.next_in = (void *)buf;
	stream.avail_in = size;
	stream.next_out = (uint8_t *)data;
	stream.avail_out = len;

	if ((error = inflateInit(&stream)) != Z_OK) {
		warnx("zlib inflateInit failed: %s", zError(error));
		goto exit;
	}

	if ((error = inflate(&stream, Z_FINISH)) != Z_STREAM_END) {
		warnx("zlib inflate failed: %s", zError(error));
		inflateEnd(&stream);
		goto exit;
	}

	if ((error = inflateEnd(&stream)) != Z_OK) {
		warnx("zlib inflateEnd failed: %s", zError(error));
		goto exit;
	}

	if (stream.total_out != len) {
		warnx("decompression failed: %zu != %zu",
		    stream.total_out, len);
		goto exit;
	}

	return data;

exit:
	free(data);
#endif /* ZLIB */
	return NULL;
}

__dead2 void
usage(void)
{
	fprintf(stderr, "usage: %s [-dfhlst] file ...\n",
	    getprogname());
	exit(1);
}
