/*	$NetBSD: arch.c,v 1.223 2025/06/28 22:39:27 rillig Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
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

/*
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

/*
 * Manipulate libraries, archives and their members.
 *
 * The first time an archive is referenced, all of its members' headers are
 * read and cached and the archive closed again.  All cached archives are kept
 * on a list which is searched each time an archive member is referenced.
 *
 * The interface to this module is:
 *
 *	Arch_Init	Initialize this module.
 *
 *	Arch_End	Clean up this module.
 *
 *	Arch_ParseArchive
 *			Parse an archive specification such as
 *			"archive.a(member1 member2)".
 *
 *	Arch_Touch	Alter the modification time of the archive
 *			member described by the given node to be
 *			the time when make was started.
 *
 *	Arch_TouchLib	Update the modification time of the library
 *			described by the given node. This is special
 *			because it also updates the modification time
 *			of the library's table of contents.
 *
 *	Arch_UpdateMTime
 *			Find the modification time of a member of
 *			an archive *in the archive* and place it in the
 *			member's GNode.
 *
 *	Arch_UpdateMemberMTime
 *			Find the modification time of a member of
 *			an archive. Called when the member doesn't
 *			already exist. Looks in the archive for the
 *			modification time. Returns the modification
 *			time.
 *
 *	Arch_FindLib	Search for a library along a path. The
 *			library name in the GNode should be in
 *			-l<name> format.
 *
 *	Arch_LibOODate	Decide if a library node is out-of-date.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/param.h>
#ifdef HAVE_AR_H
#include <ar.h>
#else
struct ar_hdr {
        char ar_name[16];               /* name */
        char ar_date[12];               /* modification time */
        char ar_uid[6];                 /* user id */
        char ar_gid[6];                 /* group id */
        char ar_mode[8];                /* octal file permissions */
        char ar_size[10];               /* size in bytes */
#ifndef ARFMAG
#define ARFMAG  "`\n"
#endif
        char ar_fmag[2];                /* consistency check */
};
#endif
#if defined(HAVE_RANLIB_H) && !(defined(__ELF__) || defined(NO_RANLIB))
#include <ranlib.h>
#endif
#ifdef HAVE_UTIME_H
#include <utime.h>
#endif

#include "make.h"
#include "dir.h"

/*	"@(#)arch.c	8.2 (Berkeley) 1/2/94"	*/
MAKE_RCSID("$NetBSD: arch.c,v 1.223 2025/06/28 22:39:27 rillig Exp $");

typedef struct List ArchList;
typedef struct ListNode ArchListNode;

static ArchList archives;	/* The archives we've already examined */

typedef struct Arch {
	char *name;
	HashTable members;	/* All the members of the archive described
				 * by <name, struct ar_hdr *> key/value pairs */
	char *fnametab;		/* Extended name table strings */
	size_t fnamesize;	/* Size of the string table */
} Arch;

static FILE *ArchFindMember(const char *, const char *,
			    struct ar_hdr *, const char *);
#if defined(__svr4__) || defined(__SVR4) || defined(__ELF__)
#define SVR4ARCHIVES
static int ArchSVR4Entry(Arch *, char *, size_t, FILE *);
#endif


#if defined(_AIX)
# define AR_NAME _ar_name.ar_name
# define AR_FMAG _ar_name.ar_fmag
# define SARMAG  SAIAMAG
# define ARMAG   AIAMAG
# define ARFMAG  AIAFMAG
#endif
#ifndef  AR_NAME
# define AR_NAME ar_name
#endif
#ifndef  AR_DATE
# define AR_DATE ar_date
#endif
#ifndef  AR_SIZE
# define AR_SIZE ar_size
#endif
#ifndef  AR_FMAG
# define AR_FMAG ar_fmag
#endif
#ifndef ARMAG
# define ARMAG	"!<arch>\n"
#endif
#ifndef SARMAG
# define SARMAG	8
#endif


#ifdef CLEANUP
static void
ArchFree(Arch *a)
{
	HashIter hi;

	HashIter_Init(&hi, &a->members);
	while (HashIter_Next(&hi))
		free(hi.entry->value);

	free(a->name);
	free(a->fnametab);
	HashTable_Done(&a->members);
	free(a);
}
#endif

/* Return "archive(member)". */
MAKE_ATTR_NOINLINE static char *
FullName(const char *archive, const char *member)
{
	Buffer buf;
	Buf_Init(&buf);
	Buf_AddStr(&buf, archive);
	Buf_AddStr(&buf, "(");
	Buf_AddStr(&buf, member);
	Buf_AddStr(&buf, ")");
	return Buf_DoneData(&buf);
}

/*
 * Parse an archive specification such as "archive.a(member1 member2.${EXT})",
 * adding nodes for the expanded members to gns.  If successful, advance pp
 * beyond the archive specification and any trailing whitespace.
 */
bool
Arch_ParseArchive(char **pp, GNodeList *gns, GNode *scope)
{
	char *spec;		/* For modifying some bytes of *pp */
	const char *cp;		/* Pointer into line */
	GNode *gn;		/* New node */
	FStr lib;		/* Library-part of specification */
	FStr mem;		/* Member-part of specification */
	char saveChar;		/* Ending delimiter of member-name */
	bool expandLib;		/* Whether the parsed lib contains
				 * expressions that need to be expanded */

	spec = *pp;
	lib = FStr_InitRefer(spec);
	expandLib = false;

	for (cp = lib.str; *cp != '(' && *cp != '\0';) {
		if (*cp == '$') {
			/* Expand nested expressions. */
			/* XXX: This code can probably be shortened. */
			const char *nested_p = cp;
			FStr result;
			bool isError;

			/* XXX: is expanded twice: once here and once below */
			result = Var_Parse(&nested_p, scope,
			    VARE_EVAL_DEFINED);
			/* TODO: handle errors */
			isError = result.str == var_Error;
			FStr_Done(&result);
			if (isError)
				return false;

			expandLib = true;
			cp += nested_p - cp;
		} else
			cp++;
	}

	spec[cp++ - spec] = '\0';
	if (expandLib)
		Var_Expand(&lib, scope, VARE_EVAL_DEFINED);

	for (;;) {
		/*
		 * First skip to the start of the member's name, mark that
		 * place and skip to the end of it (either white-space or
		 * a close paren).
		 */
		bool doSubst = false;

		cpp_skip_whitespace(&cp);

		mem = FStr_InitRefer(cp);
		while (*cp != '\0' && *cp != ')' && !ch_isspace(*cp)) {
			if (*cp == '$') {
				/* Expand nested expressions. */
				/*
				 * XXX: This code can probably be shortened.
				 */
				FStr result;
				bool isError;
				const char *nested_p = cp;

				result = Var_Parse(&nested_p, scope,
				    VARE_EVAL_DEFINED);
				/* TODO: handle errors */
				isError = result.str == var_Error;
				FStr_Done(&result);

				if (isError)
					return false;

				doSubst = true;
				cp += nested_p - cp;
			} else {
				cp++;
			}
		}

		if (*cp == '\0') {
			Parse_Error(PARSE_FATAL,
			    "Missing \")\" in archive specification");
			return false;
		}

		if (cp == mem.str)
			break;

		saveChar = *cp;
		spec[cp - spec] = '\0';

		/*
		 * XXX: This should be taken care of intelligently by
		 * SuffExpandChildren, both for the archive and the member
		 * portions.
		 */
		/*
		 * If member contains variables, try and substitute for them.
		 * This slows down archive specs with dynamic sources, since
		 * they are (non-)substituted three times, but we need to do
		 * this since SuffExpandChildren calls us, otherwise we could
		 * assume the substitutions would be taken care of later.
		 */
		if (doSubst) {
			char *fullName;
			char *p;
			const char *unexpandedMem = mem.str;

			Var_Expand(&mem, scope, VARE_EVAL_DEFINED);

			/*
			 * Now form an archive spec and recurse to deal with
			 * nested variables and multi-word variable values.
			 */
			fullName = FullName(lib.str, mem.str);
			p = fullName;

			if (strcmp(mem.str, unexpandedMem) == 0) {
				/*
				 * Must contain dynamic sources, so we can't
				 * deal with it now. Just create an ARCHV node
				 * and let SuffExpandChildren handle it.
				 */
				gn = Targ_GetNode(fullName);
				gn->type |= OP_ARCHV;
				Lst_Append(gns, gn);

			} else if (!Arch_ParseArchive(&p, gns, scope)) {
				/* Error in nested call. */
				free(fullName);
				/* XXX: does unexpandedMemName leak? */
				return false;
			}
			free(fullName);
			/* XXX: does unexpandedMemName leak? */

		} else if (Dir_HasWildcards(mem.str)) {
			StringList members = LST_INIT;
			SearchPath_Expand(&dirSearchPath, mem.str, &members);

			while (!Lst_IsEmpty(&members)) {
				char *member = Lst_Dequeue(&members);
				char *fullname = FullName(lib.str, member);
				free(member);

				gn = Targ_GetNode(fullname);
				free(fullname);

				gn->type |= OP_ARCHV;
				Lst_Append(gns, gn);
			}
			Lst_Done(&members);

		} else {
			char *fullname = FullName(lib.str, mem.str);
			gn = Targ_GetNode(fullname);
			free(fullname);

			gn->type |= OP_ARCHV;
			Lst_Append(gns, gn);
		}
		FStr_Done(&mem);

		spec[cp - spec] = saveChar;
	}

	FStr_Done(&lib);

	cp++;			/* skip the ')' */
	cpp_skip_whitespace(&cp);
	*pp += cp - *pp;
	return true;
}

/*
 * Locate a member in an archive.
 *
 * See ArchFindMember for an almost identical copy of this code.
 */
static struct ar_hdr *
ArchStatMember(const char *archive, const char *member, bool addToCache)
{
#define AR_MAX_NAME_LEN (sizeof arh.AR_NAME - 1)
	FILE *arch;
	size_t size;		/* Size of archive member */
	char magic[SARMAG];
	ArchListNode *ln;
	Arch *ar;
	struct ar_hdr arh;
	char memName[MAXPATHLEN + 1];
	/* Current member name while hashing. */

	member = str_basename(member);

	for (ln = archives.first; ln != NULL; ln = ln->next) {
		const Arch *a = ln->datum;
		if (strcmp(a->name, archive) == 0)
			break;
	}

	if (ln != NULL) {
		struct ar_hdr *hdr;

		ar = ln->datum;
		hdr = HashTable_FindValue(&ar->members, member);
		if (hdr != NULL)
			return hdr;

		{
			/* Try truncated name */
			char copy[AR_MAX_NAME_LEN + 1];
			size_t len = strlen(member);

			if (len > AR_MAX_NAME_LEN) {
				snprintf(copy, sizeof copy, "%s", member);
				hdr = HashTable_FindValue(&ar->members, copy);
			}
			return hdr;
		}
	}

	if (!addToCache) {
		/*
		 * Since the archive is not to be cached, assume there's no
		 * need to allocate the header, so just declare it static.
		 */
		static struct ar_hdr sarh;

		arch = ArchFindMember(archive, member, &sarh, "r");
		if (arch == NULL)
			return NULL;

		fclose(arch);
		return &sarh;
	}

	arch = fopen(archive, "r");
	if (arch == NULL)
		return NULL;

	if (fread(magic, SARMAG, 1, arch) != 1 ||
	    strncmp(magic, ARMAG, SARMAG) != 0) {
		(void)fclose(arch);
		return NULL;
	}

	ar = bmake_malloc(sizeof *ar);
	ar->name = bmake_strdup(archive);
	ar->fnametab = NULL;
	ar->fnamesize = 0;
	HashTable_Init(&ar->members);
	memName[AR_MAX_NAME_LEN] = '\0';

	while (fread(&arh, sizeof arh, 1, arch) == 1) {
		char *nameend;

		if (strncmp(arh.AR_FMAG, ARFMAG, sizeof arh.AR_FMAG) != 0)
			goto bad_archive;

		arh.AR_SIZE[sizeof arh.AR_SIZE - 1] = '\0';
		size = (size_t)strtol(arh.AR_SIZE, NULL, 10);

		memcpy(memName, arh.AR_NAME, sizeof arh.AR_NAME);
		nameend = memName + AR_MAX_NAME_LEN;
		while (nameend > memName && *nameend == ' ')
			nameend--;
		nameend[1] = '\0';

#ifdef SVR4ARCHIVES
		/*
		 * svr4 names are slash-terminated.
		 * Also svr4 extended the AR format.
		 */
		if (memName[0] == '/') {
			/* svr4 magic mode; handle it */
			switch (ArchSVR4Entry(ar, memName, size, arch)) {
			case -1:	/* Invalid data */
				goto bad_archive;
			case 0:		/* List of files entry */
				continue;
			default:	/* Got the entry */
				break;
			}
		} else {
			if (nameend[0] == '/')
				nameend[0] = '\0';
		}
#endif

#ifdef AR_EFMT1
		/*
		 * BSD 4.4 extended AR format: #1/<namelen>, with name as the
		 * first <namelen> bytes of the file
		 */
		if (strncmp(memName, AR_EFMT1, sizeof AR_EFMT1 - 1) == 0 &&
		    ch_isdigit(memName[sizeof AR_EFMT1 - 1])) {

			size_t elen = (size_t)atoi(
			    memName + sizeof AR_EFMT1 - 1);

			if (elen > MAXPATHLEN)
				goto bad_archive;
			if (fread(memName, elen, 1, arch) != 1)
				goto bad_archive;
			memName[elen] = '\0';
			if (fseek(arch, -(long)elen, SEEK_CUR) != 0)
				goto bad_archive;
			if (DEBUG(ARCH) || DEBUG(MAKE))
				debug_printf(
				    "ArchStatMember: "
				    "Extended format entry for %s\n",
				    memName);
		}
#endif

		{
			struct ar_hdr *cached_hdr = bmake_malloc(
			    sizeof *cached_hdr);
			memcpy(cached_hdr, &arh, sizeof arh);
			HashTable_Set(&ar->members, memName, cached_hdr);
		}

		/* Files are padded with newlines to an even-byte boundary. */
		if (fseek(arch, ((long)size + 1) & ~1, SEEK_CUR) != 0)
			goto bad_archive;
	}

	fclose(arch);

	Lst_Append(&archives, ar);

	return HashTable_FindValue(&ar->members, member);

bad_archive:
	fclose(arch);
	HashTable_Done(&ar->members);
	free(ar->fnametab);
	free(ar);
	return NULL;
}

#ifdef SVR4ARCHIVES
/*
 * Parse an SVR4 style entry that begins with a slash.
 * If it is "//", then load the table of filenames.
 * If it is "/<offset>", then try to substitute the long file name
 * from offset of a table previously read.
 * If a table is read, the file pointer is moved to the next archive member.
 *
 * Results:
 *	-1: Bad data in archive
 *	 0: A table was loaded from the file
 *	 1: Name was successfully substituted from table
 *	 2: Name was not successfully substituted from table
 */
static int
ArchSVR4Entry(Arch *ar, char *inout_name, size_t size, FILE *arch)
{
#define ARLONGNAMES1 "//"
#define ARLONGNAMES2 "/ARFILENAMES"
	size_t entry;
	char *ptr, *eptr;

	if (strncmp(inout_name, ARLONGNAMES1, sizeof ARLONGNAMES1 - 1) == 0 ||
	    strncmp(inout_name, ARLONGNAMES2, sizeof ARLONGNAMES2 - 1) == 0) {

		if (ar->fnametab != NULL) {
			DEBUG0(ARCH,
			    "Attempted to redefine an SVR4 name table\n");
			return -1;
		}

		/*
		 * This is a table of archive names, so we build one for
		 * ourselves
		 */
		ar->fnametab = bmake_malloc(size);
		ar->fnamesize = size;

		if (fread(ar->fnametab, size, 1, arch) != 1) {
			DEBUG0(ARCH, "Reading an SVR4 name table failed\n");
			return -1;
		}
		eptr = ar->fnametab + size;
		for (entry = 0, ptr = ar->fnametab; ptr < eptr; ptr++)
			if (*ptr == '/') {
				entry++;
				*ptr = '\0';
			}
		DEBUG1(ARCH,
		    "Found svr4 archive name table with %lu entries\n",
		    (unsigned long)entry);
		return 0;
	}

	if (inout_name[1] == ' ' || inout_name[1] == '\0')
		return 2;

	entry = (size_t)strtol(&inout_name[1], &eptr, 0);
	if ((*eptr != ' ' && *eptr != '\0') || eptr == &inout_name[1]) {
		DEBUG1(ARCH, "Could not parse SVR4 name %s\n", inout_name);
		return 2;
	}
	if (entry >= ar->fnamesize) {
		DEBUG2(ARCH, "SVR4 entry offset %s is greater than %lu\n",
		    inout_name, (unsigned long)ar->fnamesize);
		return 2;
	}

	DEBUG2(ARCH, "Replaced %s with %s\n", inout_name, &ar->fnametab[entry]);

	snprintf(inout_name, MAXPATHLEN + 1, "%s", &ar->fnametab[entry]);
	return 1;
}
#endif


static bool
ArchiveMember_HasName(const struct ar_hdr *hdr,
		      const char *name, size_t namelen)
{
	const size_t ar_name_len = sizeof hdr->AR_NAME;
	const char *ar_name = hdr->AR_NAME;

	if (strncmp(ar_name, name, namelen) != 0)
		return false;

	if (namelen >= ar_name_len)
		return namelen == ar_name_len;

	/* hdr->AR_NAME is space-padded to the right. */
	if (ar_name[namelen] == ' ')
		return true;

	/*
	 * In archives created by GNU binutils 2.27, the member names end
	 * with a slash.
	 */
	if (ar_name[namelen] == '/' && ar_name[namelen + 1] == ' ')
		return true;

	return false;
}

/*
 * Load the header of an archive member.  The mode is "r" for read-only
 * access, "r+" for read-write access.
 *
 * Upon successful return, the archive file is positioned at the start of the
 * member's struct ar_hdr.  In case of a failure or if the member doesn't
 * exist, return NULL.
 *
 * See ArchStatMember for an almost identical copy of this code.
 */
static FILE *
ArchFindMember(const char *archive, const char *member,
	       struct ar_hdr *out_arh, const char *mode)
{
	FILE *arch;
	int size;		/* Size of archive member */
	char magic[SARMAG];
	size_t len;

	arch = fopen(archive, mode);
	if (arch == NULL)
		return NULL;

	if (fread(magic, SARMAG, 1, arch) != 1 ||
	    strncmp(magic, ARMAG, SARMAG) != 0) {
		fclose(arch);
		return NULL;
	}

	/* Files are archived using their basename, not the entire path. */
	member = str_basename(member);
	len = strlen(member);

	while (fread(out_arh, sizeof *out_arh, 1, arch) == 1) {

		if (strncmp(out_arh->AR_FMAG, ARFMAG,
			    sizeof out_arh->AR_FMAG) != 0) {
			fclose(arch);
			return NULL;
		}

		DEBUG5(ARCH, "Reading archive %s member %.*s mtime %.*s\n",
		    archive,
		    (int)sizeof out_arh->AR_NAME, out_arh->AR_NAME,
		    (int)sizeof out_arh->ar_date, out_arh->ar_date);

		if (ArchiveMember_HasName(out_arh, member, len)) {
			if (fseek(arch, -(long)sizeof *out_arh, SEEK_CUR) !=
			    0) {
				fclose(arch);
				return NULL;
			}
			return arch;
		}

#ifdef AR_EFMT1
		/*
		 * BSD 4.4 extended AR format: #1/<namelen>, with name as the
		 * first <namelen> bytes of the file
		 */
		if (strncmp(out_arh->AR_NAME, AR_EFMT1, sizeof AR_EFMT1 - 1) ==
		    0 &&
		    (ch_isdigit(out_arh->AR_NAME[sizeof AR_EFMT1 - 1]))) {
			size_t elen = (size_t)atoi(
			    &out_arh->AR_NAME[sizeof AR_EFMT1 - 1]);
			char ename[MAXPATHLEN + 1];

			if (elen > MAXPATHLEN) {
				fclose(arch);
				return NULL;
			}
			if (fread(ename, elen, 1, arch) != 1) {
				fclose(arch);
				return NULL;
			}
			ename[elen] = '\0';
			if (DEBUG(ARCH) || DEBUG(MAKE))
				debug_printf(
				    "ArchFindMember: "
				    "Extended format entry for %s\n",
				    ename);
			if (strncmp(ename, member, len) == 0) {
				/* Found as extended name */
				if (fseek(arch,
				    -(long)(sizeof(struct ar_hdr) - elen),
				    SEEK_CUR) != 0) {
					fclose(arch);
					return NULL;
				}
				return arch;
			}
			if (fseek(arch, -(long)elen, SEEK_CUR) != 0) {
				fclose(arch);
				return NULL;
			}
		}
#endif

		/* Advance to the next member. */
		out_arh->AR_SIZE[sizeof out_arh->AR_SIZE - 1] = '\0';
		size = (int)strtol(out_arh->AR_SIZE, NULL, 10);
		/* Files are padded with newlines to an even-byte boundary. */
		if (fseek(arch, (size + 1) & ~1L, SEEK_CUR) != 0) {
			fclose(arch);
			return NULL;
		}
	}

	fclose(arch);
	return NULL;
}

/*
 * Update the ar_date of the member of an archive, on disk but not in the
 * GNode.  Update the st_mtime of the entire archive as well.  For a library,
 * it may be required to run ranlib after this.
 */
void
Arch_Touch(GNode *gn)
{
	FILE *f;
	struct ar_hdr arh;

	f = ArchFindMember(GNode_VarArchive(gn), GNode_VarMember(gn), &arh,
	    "r+");
	if (f == NULL)
		return;

	snprintf(arh.ar_date, sizeof arh.ar_date, "%-ld", (unsigned long)now);
	(void)fwrite(&arh, sizeof arh, 1, f);
	fclose(f);		/* TODO: handle errors */
}

/*
 * Given a node which represents a library, touch the thing, making sure that
 * the table of contents is also touched.
 *
 * Both the modification time of the library and of the RANLIBMAG member are
 * set to 'now'.
 */
void
Arch_TouchLib(GNode *gn MAKE_ATTR_UNUSED)
{
#ifdef RANLIBMAG
	FILE *f;
	struct ar_hdr arh;	/* Header describing table of contents */
	struct utimbuf times;

	f = ArchFindMember(gn->path, RANLIBMAG, &arh, "r+");
	if (f == NULL)
		return;

	snprintf(arh.ar_date, sizeof arh.ar_date, "%-ld", (unsigned long)now);
	(void)fwrite(&arh, sizeof arh, 1, f);
	fclose(f);		/* TODO: handle errors */

	times.actime = times.modtime = now;
	utime(gn->path, &times);	/* TODO: handle errors */
#endif
}

/*
 * Update the mtime of the GNode with the mtime from the archive member on
 * disk (or in the cache).
 */
void
Arch_UpdateMTime(GNode *gn)
{
	struct ar_hdr *arh;

	arh = ArchStatMember(GNode_VarArchive(gn), GNode_VarMember(gn), true);
	if (arh != NULL)
		gn->mtime = (time_t)strtol(arh->ar_date, NULL, 10);
	else
		gn->mtime = 0;
}

/*
 * Given a nonexistent archive member's node, update gn->mtime from its
 * archived form, if it exists.
 */
void
Arch_UpdateMemberMTime(GNode *gn)
{
	GNodeListNode *ln;

	for (ln = gn->parents.first; ln != NULL; ln = ln->next) {
		GNode *pgn = ln->datum;

		if (pgn->type & OP_ARCHV) {
			/*
			 * If the parent is an archive specification and is
			 * being made and its member's name matches the name
			 * of the node we were given, record the modification
			 * time of the parent in the child. We keep searching
			 * its parents in case some other parent requires this
			 * child to exist.
			 */
			const char *nameStart = strchr(pgn->name, '(') + 1;
			const char *nameEnd = strchr(nameStart, ')');
			size_t nameLen = (size_t)(nameEnd - nameStart);

			if (pgn->flags.remake &&
			    strncmp(nameStart, gn->name, nameLen) == 0) {
				Arch_UpdateMTime(pgn);
				gn->mtime = pgn->mtime;
			}
		} else if (pgn->flags.remake) {
			/*
			 * Something which isn't a library depends on the
			 * existence of this target, so it needs to exist.
			 */
			gn->mtime = 0;
			break;
		}
	}
}

/*
 * Search for a library along the given search path.
 *
 * The node's 'path' field is set to the found path (including the
 * actual file name, not -l...). If the system can handle the -L
 * flag when linking (or we cannot find the library), we assume that
 * the user has placed the .LIBS variable in the final linking
 * command (or the linker will know where to find it) and set the
 * TARGET variable for this node to be the node's name. Otherwise,
 * we set the TARGET variable to be the full path of the library,
 * as returned by Dir_FindFile.
 */
void
Arch_FindLib(GNode *gn, SearchPath *path)
{
	char *libName = str_concat3("lib", gn->name + 2, ".a");
	gn->path = Dir_FindFile(libName, path);
	free(libName);

	Var_Set(gn, TARGET, gn->name);
}

static bool
RanlibOODate(const GNode *gn MAKE_ATTR_UNUSED)
{
#ifdef RANLIBMAG
	struct ar_hdr *arh;	/* Header for __.SYMDEF */
	int tocModTime;		/* The table-of-contents' mod time */

	arh = ArchStatMember(gn->path, RANLIBMAG, false);

	if (arh == NULL) {
		/* A library without a table of contents is out-of-date. */
		if (DEBUG(ARCH) || DEBUG(MAKE))
			debug_printf("no toc...");
		return true;
	}

	tocModTime = (int)strtol(arh->ar_date, NULL, 10);

	if (DEBUG(ARCH) || DEBUG(MAKE))
		debug_printf("%s modified %s...",
		    RANLIBMAG, Targ_FmtTime(tocModTime));
	return gn->youngestChild == NULL ||
	       gn->youngestChild->mtime > tocModTime;
#else
	return false;
#endif
}

/*
 * Decide if a node with the OP_LIB attribute is out-of-date.
 * The library is cached if it hasn't been already.
 *
 * There are several ways for a library to be out-of-date that are not
 * available to ordinary files.  In addition, there are ways that are open to
 * regular files that are not available to libraries.
 *
 * A library that is only used as a source is never considered out-of-date by
 * itself.  This does not preclude the library's modification time from making
 * its parent be out-of-date.  A library will be considered out-of-date for
 * any of these reasons, given that it is a target on a dependency line
 * somewhere:
 *
 *	Its modification time is less than that of one of its sources
 *	(gn->mtime < gn->youngestChild->mtime).
 *
 *	Its modification time is greater than the time at which the make
 *	began (i.e. it's been modified in the course of the make, probably
 *	by archiving).
 *
 *	The modification time of one of its sources is greater than the one
 *	of its RANLIBMAG member (i.e. its table of contents is out-of-date).
 *	We don't compare the archive time vs. TOC time because they can be
 *	too close. In my opinion we should not bother with the TOC at all
 *	since this is used by 'ar' rules that affect the data contents of the
 *	archive, not by ranlib rules, which affect the TOC.
 */
bool
Arch_LibOODate(GNode *gn)
{

	if (gn->type & OP_PHONY)
		return true;
	if (!GNode_IsTarget(gn) && Lst_IsEmpty(&gn->children))
		return false;
	if ((!Lst_IsEmpty(&gn->children) && gn->youngestChild == NULL) ||
		   (gn->mtime > now) ||
		   (gn->youngestChild != NULL &&
		    gn->mtime < gn->youngestChild->mtime))
		return true;
	return RanlibOODate(gn);
}

/* Initialize the archives module. */
void
Arch_Init(void)
{
	Lst_Init(&archives);
}

#ifdef CLEANUP
/* Clean up the archives module. */
void
Arch_End(void)
{
	ArchListNode *ln;

	for (ln = archives.first; ln != NULL; ln = ln->next)
		ArchFree(ln->datum);
	Lst_Done(&archives);
}
#endif

bool
Arch_IsLib(GNode *gn)
{
	char buf[8];
	int fd;
	bool isLib;

	if ((fd = open(gn->path, O_RDONLY)) == -1)
		return false;
	isLib = read(fd, buf, sizeof buf) == sizeof buf
	    && memcmp(buf, "!<arch>\n", sizeof buf) == 0;
	(void)close(fd);
	return isLib;
}
