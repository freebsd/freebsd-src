/*-
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *
 * @(#)arch.c	8.2 (Berkeley) 1/2/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*-
 * arch.c --
 *	Functions to manipulate libraries, archives and their members.
 *
 *	Once again, cacheing/hashing comes into play in the manipulation
 * of archives. The first time an archive is referenced, all of its members'
 * headers are read and hashed and the archive closed again. All hashed
 * archives are kept on a list which is searched each time an archive member
 * is referenced.
 *
 * The interface to this module is:
 *	Arch_ParseArchive	Given an archive specification, return a list
 *				of GNode's, one for each member in the spec.
 *				FAILURE is returned if the specification is
 *				invalid for some reason.
 *
 *	Arch_Touch		Alter the modification time of the archive
 *				member described by the given node to be
 *				the current time.
 *
 *	Arch_TouchLib		Update the modification time of the library
 *				described by the given node. This is special
 *				because it also updates the modification time
 *				of the library's table of contents.
 *
 *	Arch_MTime		Find the modification time of a member of
 *				an archive *in the archive*. The time is also
 *				placed in the member's GNode. Returns the
 *				modification time.
 *
 *	Arch_MemTime		Find the modification time of a member of
 *				an archive. Called when the member doesn't
 *				already exist. Looks in the archive for the
 *				modification time. Returns the modification
 *				time.
 *
 *	Arch_FindLib		Search for a library along a path. The
 *				library name in the GNode should be in
 *				-l<name> format.
 *
 *	Arch_LibOODate		Special function to decide if a library node
 *				is out-of-date.
 *
 *	Arch_Init		Initialize this module.
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <ar.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <regex.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <utime.h>

#include "arch.h"
#include "buf.h"
#include "config.h"
#include "dir.h"
#include "globals.h"
#include "GNode.h"
#include "hash.h"
#include "make.h"
#include "targ.h"
#include "util.h"
#include "var.h"

typedef struct Arch {
	char		*name;		/* Name of archive */

	/*
	 * All the members of the archive described
	 * by <name, struct ar_hdr *> key/value pairs
	 */
	Hash_Table	members;

	TAILQ_ENTRY(Arch) link;		/* link all cached archives */
} Arch;

/* Lst of archives we've already examined */
static TAILQ_HEAD(, Arch) archives = TAILQ_HEAD_INITIALIZER(archives);


/* size of the name field in the archive member header */
#define	AR_NAMSIZ	sizeof(((struct ar_hdr *)0)->ar_name)

/*
 * This structure is used while reading/writing an archive
 */
struct arfile {
	FILE		*fp;		/* archive file */
	char		*fname;		/* name of the file */
	struct ar_hdr	hdr;		/* current header */
	char		sname[AR_NAMSIZ + 1]; /* short name */
	char		*member;	/* (long) member name */
	size_t		mlen;		/* size of the above */
	char		*nametab;	/* name table */
	size_t		nametablen;	/* size of the table */
	int64_t		time;		/* from ar_date */
	uint64_t	size;		/* from ar_size */
	off_t		pos;		/* header pos of current entry */
};

/*
 * Name of the symbol table. The original BSD used "__.SYMDEF". Rumours go
 * that this name may have a slash appended sometimes. Actually FreeBSD
 * uses "/" which probably came from SVR4.
 */
#define	SVR4_RANLIBMAG	"/"
#define	BSD_RANLIBMAG	"__.SYMDEF"

/*
 * Name of the filename table. The 4.4BSD ar format did not use this, but
 * puts long filenames directly between the member header and the object
 * file.
 */
#define	SVR4_NAMEMAG	"//"
#define	BSD_NAMEMAG	"ARFILENAMES/"

/*
 * 44BSD long filename key. Use a local define here instead of relying
 * on ar.h because we want this to continue working even when the
 * definition is removed from ar.h.
 */
#define	BSD_EXT1	"#1/"
#define	BSD_EXT1LEN	3

/* if this is TRUE make archive errors fatal */
Boolean arch_fatal = TRUE;

/**
 * ArchError
 *	An error happend while handling an archive. BSDmake traditionally
 *	ignored these errors. Now this is dependend on the global arch_fatal
 *	which, if true, makes these errors fatal and, if false, just emits an
 *	error message.
 */
#define	ArchError(ARGS) do {					\
	if (arch_fatal)						\
		Fatal ARGS;					\
	else							\
		Error ARGS;					\
    } while (0)

/*-
 *-----------------------------------------------------------------------
 * Arch_ParseArchive --
 *	Parse the archive specification in the given line and find/create
 *	the nodes for the specified archive members, placing their nodes
 *	on the given list, given the pointer to the start of the
 *	specification, a Lst on which to place the nodes, and a context
 *	in which to expand variables.
 *
 * Results:
 *	SUCCESS if it was a valid specification. The linePtr is updated
 *	to point to the first non-space after the archive spec. The
 *	nodes for the members are placed on the given list.
 *
 * Side Effects:
 *	Some nodes may be created. The given list is extended.
 *
 *-----------------------------------------------------------------------
 */
ReturnStatus
Arch_ParseArchive(char **linePtr, Lst *nodeLst, GNode *ctxt)
{
	char	*cp;		/* Pointer into line */
	GNode	*gn;		/* New node */
	char	*libName;	/* Library-part of specification */
	char	*memName;	/* Member-part of specification */
	char	*nameBuf;	/* temporary place for node name */
	char	saveChar;	/* Ending delimiter of member-name */
	Boolean	subLibName;	/* TRUE if libName should have/had
				 * variable substitution performed on it */

	libName = *linePtr;

	subLibName = FALSE;

	for (cp = libName; *cp != '(' && *cp != '\0'; cp++) {
		if (*cp == '$') {
			/*
			 * Variable spec, so call the Var module to parse the
			 * puppy so we can safely advance beyond it...
			 */
			size_t	length = 0;
			Boolean	freeIt;
			char	*result;

			result = Var_Parse(cp, ctxt, TRUE, &length, &freeIt);
			if (result == var_Error) {
				return (FAILURE);
			}
			subLibName = TRUE;

			if (freeIt) {
				free(result);
			}
			cp += length - 1;
		}
	}

	*cp++ = '\0';
	if (subLibName) {
		libName = Buf_Peel(Var_Subst(libName, ctxt, TRUE));
	}

	for (;;) {
		/*
		 * First skip to the start of the member's name, mark that
		 * place and skip to the end of it (either white-space or
		 * a close paren).
		 */

		/*
		 * TRUE if need to substitute in memName
		 */
		Boolean	doSubst = FALSE;

		while (*cp != '\0' && *cp != ')' &&
		    isspace((unsigned char)*cp)) {
			cp++;
		}

		memName = cp;
		while (*cp != '\0' && *cp != ')' &&
		    !isspace((unsigned char)*cp)) {
			if (*cp == '$') {
				/*
				 * Variable spec, so call the Var module to
				 * parse the puppy so we can safely advance
				 * beyond it...
				 */
				size_t	length = 0;
				Boolean	freeIt;
				char	*result;

				result = Var_Parse(cp, ctxt, TRUE,
				    &length, &freeIt);
				if (result == var_Error) {
					return (FAILURE);
				}
				doSubst = TRUE;

				if (freeIt) {
					free(result);
				}
				cp += length;
			} else {
				cp++;
			}
		}

		/*
		 * If the specification ends without a closing parenthesis,
		 * chances are there's something wrong (like a missing
		 * backslash), so it's better to return failure than allow
		 * such things to happen
		 */
		if (*cp == '\0') {
			printf("No closing parenthesis in archive "
			    "specification\n");
			return (FAILURE);
		}

		/*
		 * If we didn't move anywhere, we must be done
		 */
		if (cp == memName) {
			break;
		}

		saveChar = *cp;
		*cp = '\0';

		/*
		 * XXX: This should be taken care of intelligently by
		 * SuffExpandChildren, both for the archive and the member
		 * portions.
		 */
		/*
		 * If member contains variables, try and substitute for them.
		 * This will slow down archive specs with dynamic sources, of
		 * course, since we'll be (non-)substituting them three times,
		 * but them's the breaks -- we need to do this since
		 * SuffExpandChildren calls us, otherwise we could assume the
		 * thing would be taken care of later.
		 */
		if (doSubst) {
			char	*buf;
			char	*sacrifice;
			char	*oldMemName = memName;
			size_t	sz;
			Buffer	*buf1;

			/*
			 * Now form an archive spec and recurse to deal with
			 * nested variables and multi-word variable values....
			 * The results are just placed at the end of the
			 * nodeLst we're returning.
			 */
			buf1 = Var_Subst(memName, ctxt, TRUE);
			memName = Buf_Data(buf1);

			sz = strlen(memName) + strlen(libName) + 3;
			buf = emalloc(sz);

			snprintf(buf, sz, "%s(%s)", libName, memName);

			sacrifice = buf;

			if (strchr(memName, '$') &&
			    strcmp(memName, oldMemName) == 0) {
				/*
				 * Must contain dynamic sources, so we can't
				 * deal with it now.
				 * Just create an ARCHV node for the thing and
				 * let SuffExpandChildren handle it...
				 */
				gn = Targ_FindNode(buf, TARG_CREATE);

				if (gn == NULL) {
					free(buf);
					Buf_Destroy(buf1, FALSE);
					return (FAILURE);
				}
				gn->type |= OP_ARCHV;
				Lst_AtEnd(nodeLst, (void *)gn);
			} else if (Arch_ParseArchive(&sacrifice, nodeLst,
			    ctxt) != SUCCESS) {
				/*
				 * Error in nested call -- free buffer and
				 * return FAILURE ourselves.
				 */
				free(buf);
				Buf_Destroy(buf1, FALSE);
				return (FAILURE);
			}

			/* Free buffer and continue with our work. */
			free(buf);
			Buf_Destroy(buf1, FALSE);

		} else if (Dir_HasWildcards(memName)) {
			Lst	members = Lst_Initializer(members);
			char	*member;
			size_t	sz = MAXPATHLEN;
			size_t	nsz;

			nameBuf = emalloc(sz);

			Path_Expand(memName, &dirSearchPath, &members);
			while (!Lst_IsEmpty(&members)) {
				member = Lst_DeQueue(&members);
				nsz = strlen(libName) + strlen(member) + 3;
				if (nsz > sz) {
					sz = nsz * 2;
					nameBuf = erealloc(nameBuf, sz);
				}

				snprintf(nameBuf, sz, "%s(%s)",
				    libName, member);
				free(member);
				gn = Targ_FindNode(nameBuf, TARG_CREATE);
				if (gn == NULL) {
					free(nameBuf);
					/* XXXHB Lst_Destroy(&members) */
					return (FAILURE);
				}
				/*
				 * We've found the node, but have to make sure
				 * the rest of the world knows it's an archive
				 * member, without having to constantly check
				 * for parentheses, so we type the thing with
				 * the OP_ARCHV bit before we place it on the
				 * end of the provided list.
				 */
				gn->type |= OP_ARCHV;
				Lst_AtEnd(nodeLst, gn);
			}
			free(nameBuf);
		} else {
			size_t	sz = strlen(libName) + strlen(memName) + 3;

			nameBuf = emalloc(sz);
			snprintf(nameBuf, sz, "%s(%s)", libName, memName);
			gn = Targ_FindNode(nameBuf, TARG_CREATE);
			free(nameBuf);
			if (gn == NULL) {
				return (FAILURE);
			}
			/*
			 * We've found the node, but have to make sure the
			 * rest of the world knows it's an archive member,
			 * without having to constantly check for parentheses,
			 * so we type the thing with the OP_ARCHV bit before
			 * we place it on the end of the provided list.
			 */
			gn->type |= OP_ARCHV;
			Lst_AtEnd(nodeLst, gn);
		}
		if (doSubst) {
			free(memName);
		}

		*cp = saveChar;
	}

	/*
	 * If substituted libName, free it now, since we need it no longer.
	 */
	if (subLibName) {
		free(libName);
	}

	/*
	 * We promised the pointer would be set up at the next non-space, so
	 * we must advance cp there before setting *linePtr... (note that on
	 * entrance to the loop, cp is guaranteed to point at a ')')
	 */
	do {
		cp++;
	} while (*cp != '\0' && isspace((unsigned char)*cp));

	*linePtr = cp;
	return (SUCCESS);
}

/*
 * Close an archive file an free all resources
 */
static void
ArchArchiveClose(struct arfile *ar)
{

	if (ar->nametab != NULL)
		free(ar->nametab);
	free(ar->member);
	if (ar->fp != NULL) {
		if (fclose(ar->fp) == EOF)
			ArchError(("%s: close error", ar->fname));
	}
	free(ar->fname);
	free(ar);
}

/*
 * Open an archive file.
 */
static struct arfile *
ArchArchiveOpen(const char *archive, const char *mode)
{
	struct arfile *ar;
	char	magic[SARMAG];

	ar = emalloc(sizeof(*ar));
	ar->fname = estrdup(archive);
	ar->mlen = 100;
	ar->member = emalloc(ar->mlen);
	ar->nametab = NULL;
	ar->nametablen = 0;

	if ((ar->fp = fopen(ar->fname, mode)) == NULL) {
		DEBUGM(ARCH, ("%s", ar->fname));
		ArchArchiveClose(ar);
		return (NULL);
	}

	/* read MAGIC */
	if (fread(magic, SARMAG, 1, ar->fp) != 1 ||
	    strncmp(magic, ARMAG, SARMAG) != 0) {
		ArchError(("%s: bad archive magic\n", ar->fname));
		ArchArchiveClose(ar);
		return (NULL);
	}

	ar->pos = 0;
	return (ar);
}

/*
 * Read the next header from the archive. The return value will be +1 if
 * the header is read successfully, 0 on EOF and -1 if an error happend.
 * On a successful return sname contains the truncated member name and
 * member the full name. hdr contains the member header. For the symbol table
 * names of length 0 are returned. The entry for the file name table is never
 * returned.
 */
static int
ArchArchiveNext(struct arfile *ar)
{
	char	*end;
	int	have_long_name;
	u_long	offs;
	char	*ptr;
	size_t	ret;
	char	buf[MAX(sizeof(ar->hdr.ar_size), sizeof(ar->hdr.ar_date)) + 1];

  next:
	/*
	 * Seek to the next header.
	 */
	if (ar->pos == 0) {
		ar->pos = SARMAG;
	} else {
		ar->pos += sizeof(ar->hdr) + ar->size;
		if (ar->size % 2 == 1)
			ar->pos++;
	}

	if (fseeko(ar->fp, ar->pos, SEEK_SET) == -1) {
		ArchError(("%s: cannot seek to %jd: %s", ar->fname,
		    (intmax_t)ar->pos, strerror(errno)));
		return (-1);
	}

	/*
	 * Read next member header
	 */
	ret = fread(&ar->hdr, sizeof(ar->hdr), 1, ar->fp);
	if (ret != 1) {
		if (feof(ar->fp))
			return (0);
		ArchError(("%s: error reading member header: %s", ar->fname,
		    strerror(errno)));
		return (-1);
	}
	if (strncmp(ar->hdr.ar_fmag, ARFMAG, sizeof(ar->hdr.ar_fmag)) != 0) {
		ArchError(("%s: bad entry magic", ar->fname));
		return (-1);
	}

	/*
	 * looks like a member - get name by stripping trailing spaces
	 * and NUL terminating.
	 */
	strncpy(ar->sname, ar->hdr.ar_name, AR_NAMSIZ);
	ar->sname[AR_NAMSIZ] = '\0';
	for (ptr = ar->sname + AR_NAMSIZ; ptr > ar->sname; ptr--)
		if (ptr[-1] != ' ')
			break;

	*ptr = '\0';

	/*
	 * Parse the size. All entries need to have a size. Be careful
	 * to not allow buffer overruns.
	 */
	strncpy(buf, ar->hdr.ar_size, sizeof(ar->hdr.ar_size));
	buf[sizeof(ar->hdr.ar_size)] = '\0';

	errno = 0;
	ar->size = strtoumax(buf, &end, 10);
	if (errno != 0 || strspn(end, " ") != strlen(end)) {
		ArchError(("%s: bad size format in archive '%s'",
		    ar->fname, buf));
		return (-1);
	}

	/*
	 * Look for the extended name table. Do this before parsing
	 * the date because this table doesn't need a date.
	 */
	if (strcmp(ar->sname, BSD_NAMEMAG) == 0 ||
	    strcmp(ar->sname, SVR4_NAMEMAG) == 0) {
		/* filename table - read it in */
		ar->nametablen = ar->size;
		ar->nametab = emalloc(ar->nametablen);

		ret = fread(ar->nametab, 1, ar->nametablen, ar->fp);
		if (ret != ar->nametablen) {
			if (ferror(ar->fp)) {
				ArchError(("%s: cannot read nametab: %s",
				    ar->fname, strerror(errno)));
			} else {
				ArchError(("%s: cannot read nametab: "
				    "short read", ar->fname, strerror(errno)));
			}
			return (-1);
		}

		/*
		 * NUL terminate the entries. Entries are \n terminated
		 * and may have a trailing / or \.
		 */
		ptr = ar->nametab;
		while (ptr < ar->nametab + ar->nametablen) {
			if (*ptr == '\n') {
				if (ptr[-1] == '/' || ptr[-1] == '\\')
					ptr[-1] = '\0';
				*ptr = '\0';
			}
			ptr++;
		}

		/* get next archive entry */
		goto next;
	}

	/*
	 * Now parse the modification date. Be careful to not overrun
	 * buffers.
	 */
	strncpy(buf, ar->hdr.ar_date, sizeof(ar->hdr.ar_date));
	buf[sizeof(ar->hdr.ar_date)] = '\0';

	errno = 0;
	ar->time = (int64_t)strtoll(buf, &end, 10);
	if (errno != 0 || strspn(end, " ") != strlen(end)) {
		ArchError(("%s: bad date format in archive '%s'",
		    ar->fname, buf));
		return (-1);
	}

	/*
	 * Now check for the symbol table. This should really be the first
	 * entry, but we don't check this.
	 */
	if (strcmp(ar->sname, BSD_RANLIBMAG) == 0 ||
	    strcmp(ar->sname, SVR4_RANLIBMAG) == 0) {
		/* symbol table - return a zero length name */
		ar->member[0] = '\0';
		ar->sname[0] = '\0';
		return (1);
	}

	have_long_name = 0;

	/*
	 * Look whether this is a long name. There are several variants
	 * of long names:
	 *	"#1/12           "	- 12 length of following filename
	 *	"/17             "	- index into name table
	 *	" 17             "	- index into name table
	 * Note that in the last case we must also check that there is no
	 * slash in the name because of filenames with leading spaces:
	 *	" 777.o/           "	- filename 777.o
	 */
	if (ar->sname[0] == '/' || (ar->sname[0] == ' ' &&
	    strchr(ar->sname, '/') == NULL)) {
		/* SVR4 extended name */
		errno = 0;
		offs = strtoul(ar->sname + 1, &end, 10);
		if (errno != 0 || *end != '\0' || offs >= ar->nametablen ||
		    end == ar->sname + 1) {
			ArchError(("%s: bad extended name '%s'", ar->fname,
			    ar->sname));
			return (-1);
		}

		/* fetch the name */
		if (ar->mlen <= strlen(ar->nametab + offs)) {
			ar->mlen = strlen(ar->nametab + offs) + 1;
			ar->member = erealloc(ar->member, ar->mlen);
		}
		strcpy(ar->member, ar->nametab + offs);

		have_long_name = 1;

	} else if (strncmp(ar->sname, BSD_EXT1, BSD_EXT1LEN) == 0 &&
	    isdigit(ar->sname[BSD_EXT1LEN])) {
		/* BSD4.4 extended name */
		errno = 0;
		offs = strtoul(ar->sname + BSD_EXT1LEN, &end, 10);
		if (errno != 0 || *end != '\0' ||
		    end == ar->sname + BSD_EXT1LEN) {
			ArchError(("%s: bad extended name '%s'", ar->fname,
			    ar->sname));
			return (-1);
		}

		/* read it from the archive */
		if (ar->mlen <= offs) {
			ar->mlen = offs + 1;
			ar->member = erealloc(ar->member, ar->mlen);
		}
		ret = fread(ar->member, 1, offs, ar->fp);
		if (ret != offs) {
			if (ferror(ar->fp)) {
				ArchError(("%s: reading extended name: %s",
				    ar->fname, strerror(errno)));
			} else {
				ArchError(("%s: reading extended name: "
				    "short read", ar->fname));
			}
			return (-1);
		}
		ar->member[offs] = '\0';

		have_long_name = 1;
	}

	/*
	 * Now remove the trailing slash that Svr4 puts at
	 * the end of the member name to support trailing spaces in names.
	 */
	if (ptr > ar->sname && ptr[-1] == '/')
		*--ptr = '\0';

	if (!have_long_name) {
		if (strlen(ar->sname) >= ar->mlen) {
			ar->mlen = strlen(ar->sname) + 1;
			ar->member = erealloc(ar->member, ar->mlen);
		}
		strcpy(ar->member, ar->sname);
	}

	return (1);
}

/*
 * Touch the current archive member by writing a new header with an
 * updated timestamp. The return value is 0 for success and -1 for errors.
 */
static int
ArchArchiveTouch(struct arfile *ar, int64_t ts)
{

	/* seek to our header */
	if (fseeko(ar->fp, ar->pos, SEEK_SET) == -1) {
		ArchError(("%s: cannot seek to %jd: %s", ar->fname,
		    (intmax_t)ar->pos, strerror(errno)));
		return (-1);
	}

	/*
	 * change timestamp, be sure to not NUL-terminated it, but
	 * to fill with spaces.
	 */
	snprintf(ar->hdr.ar_date, sizeof(ar->hdr.ar_date), "%lld", ts);
	memset(ar->hdr.ar_date + strlen(ar->hdr.ar_date),
	    ' ', sizeof(ar->hdr.ar_date) - strlen(ar->hdr.ar_date));

	if (fwrite(&ar->hdr, sizeof(ar->hdr), 1, ar->fp) != 1) {
		ArchError(("%s: cannot touch: %s", ar->fname, strerror(errno)));
		return (-1);
	}
	return (0);
}

/*-
 *-----------------------------------------------------------------------
 * ArchFindMember --
 *	Locate a member of an archive, given the path of the archive and
 *	the path of the desired member. If the archive is to be modified,
 *	the mode should be "r+", if not, it should be "r".  The archive
 *	file is returned positioned at the correct header.
 *
 * Results:
 *	A struct arfile *, opened for reading and, possibly writing,
 *	positioned at the member's header, or NULL if the member was
 *	nonexistent.
 *
 *-----------------------------------------------------------------------
 */
static struct arfile *
ArchFindMember(const char *archive, const char *member, const char *mode)
{
	struct arfile	*ar;
	const char	*cp;	/* Useful character pointer */

	if ((ar = ArchArchiveOpen(archive, mode)) == NULL)
		return (NULL);

	/*
	 * Because of space constraints and similar things, files are archived
	 * using their final path components, not the entire thing, so we need
	 * to point 'member' to the final component, if there is one, to make
	 * the comparisons easier...
	 */
	if (member != NULL) {
		cp = strrchr(member, '/');
		if (cp != NULL) {
			member = cp + 1;
		}
	}

	while (ArchArchiveNext(ar) > 0) {
		/*
		 * When comparing there are actually three cases:
		 * (1) the name fits into the limit og af_name,
		 * (2) the name is longer and the archive supports long names,
		 * (3) the name is longer and the archive doesn't support long
		 * names.
		 * Because we don't know whether the archive supports long
		 * names or not we need to be carefull.
		 */
		if (member == NULL) {
			/* special case - symbol table */
			if (ar->member[0] == '\0')
				return (ar);
		} else if (strlen(member) <= AR_NAMSIZ) {
			/* case (1) */
			if (strcmp(ar->member, member) == 0)
				return (ar);
		} else if (strcmp(ar->member, member) == 0) {
			/* case (3) */
			return (ar);
		} else {
			/* case (2) */
			if (strlen(ar->member) == AR_NAMSIZ &&
			    strncmp(member, ar->member, AR_NAMSIZ) == 0)
				return (ar);
		}
	}

	/* not found */
	ArchArchiveClose(ar);
	return (NULL);
}

/*-
 *-----------------------------------------------------------------------
 * ArchStatMember --
 *	Locate a member of an archive, given the path of the archive and
 *	the path of the desired member, and a boolean representing whether
 *	or not the archive should be hashed (if not already hashed).
 *
 * Results:
 *	A pointer to the current struct ar_hdr structure for the member. Note
 *	That no position is returned, so this is not useful for touching
 *	archive members. This is mostly because we have no assurances that
 *	The archive will remain constant after we read all the headers, so
 *	there's not much point in remembering the position...
 *
 * Side Effects:
 *
 *-----------------------------------------------------------------------
 */
static int64_t
ArchStatMember(const char *archive, const char *member, Boolean hash)
{
	struct arfile	*arf;
	int64_t		ret;
	int		t;
	char		*cp;	/* Useful character pointer */
	Arch		*ar;	/* Archive descriptor */
	Hash_Entry	*he;	/* Entry containing member's description */
	char		copy[AR_NAMSIZ + 1];

	/*
	 * Because of space constraints and similar things, files are archived
	 * using their final path components, not the entire thing, so we need
	 * to point 'member' to the final component, if there is one, to make
	 * the comparisons easier...
	 */
	if (member != NULL) {
		cp = strrchr(member, '/');
		if (cp != NULL)
			member = cp + 1;
	}

	TAILQ_FOREACH(ar, &archives, link) {
		if (strcmp(archive, ar->name) == 0)
			break;
	}
	if (ar == NULL) {
		/* archive not found */
		if (!hash) {
			/*
			 * Caller doesn't want the thing hashed, just use
			 * ArchFindMember to read the header for the member
			 * out and close down the stream again.
			 */
			arf = ArchFindMember(archive, member, "r");
			if (arf == NULL) {
				return (INT64_MIN);
			}
			ret = arf->time;
			ArchArchiveClose(arf);
			return (ret);
		}

		/*
		 * We don't have this archive on the list yet, so we want to
		 * find out everything that's in it and cache it so we can get
		 * at it quickly.
		 */
		arf = ArchArchiveOpen(archive, "r");
		if (arf == NULL) {
			return (INT64_MIN);
		}

		/* create archive data structure */
		ar = emalloc(sizeof(*ar));
		ar->name = estrdup(archive);
		Hash_InitTable(&ar->members, -1);

		while ((t = ArchArchiveNext(arf)) > 0) {
			he = Hash_CreateEntry(&ar->members, arf->member, NULL);
			Hash_SetValue(he, emalloc(sizeof(int64_t)));
			*(int64_t *)Hash_GetValue(he) = arf->time;
		}

		ArchArchiveClose(arf);

		if (t < 0) {
			/* error happend - throw away everything */
			Hash_DeleteTable(&ar->members);
			free(ar->name);
			free(ar);
			return (INT64_MIN);
		}

		TAILQ_INSERT_TAIL(&archives, ar, link);
	}

	/*
	 * Now that the archive has been read and cached, we can look into
	 * the hash table to find the desired member's header.
	 */
	he = Hash_FindEntry(&ar->members, member);
	if (he != NULL)
		return (*(int64_t *)Hash_GetValue (he));

	if (member != NULL && strlen(member) > AR_NAMSIZ) {
		/* Try truncated name */
		strncpy(copy, member, AR_NAMSIZ);
		copy[AR_NAMSIZ] = '\0';

		if ((he = Hash_FindEntry(&ar->members, copy)) != NULL)
			return (*(int64_t *)Hash_GetValue(he));
	}

	return (INT64_MIN);
}

/*-
 *-----------------------------------------------------------------------
 * Arch_Touch --
 *	Touch a member of an archive.
 *
 * Results:
 *	The 'time' field of the member's header is updated.
 *
 * Side Effects:
 *	The modification time of the entire archive is also changed.
 *	For a library, this could necessitate the re-ranlib'ing of the
 *	whole thing.
 *
 *-----------------------------------------------------------------------
 */
void
Arch_Touch(GNode *gn)
{
	struct arfile	*ar;
	char		*p1, *p2;

	ar = ArchFindMember(Var_Value(ARCHIVE, gn, &p1),
	    Var_Value(TARGET, gn, &p2), "r+");
	free(p1);
	free(p2);

	if (ar != NULL) {
		ArchArchiveTouch(ar, (int64_t)now);
		ArchArchiveClose(ar);
	}
}

/*-
 *-----------------------------------------------------------------------
 * Arch_TouchLib --
 *	Given a node which represents a library, touch the thing, making
 *	sure that the table of contents also is touched.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Both the modification time of the library and of the RANLIBMAG
 *	member are set to 'now'.
 *
 *-----------------------------------------------------------------------
 */
void
Arch_TouchLib(GNode *gn)
{
	struct arfile	*ar;	/* Open archive */
	struct utimbuf	times;	/* Times for utime() call */

	ar = ArchFindMember(gn->path, NULL, "r+");
	if (ar != NULL) {
		ArchArchiveTouch(ar, (int64_t)now);
		ArchArchiveClose(ar);

		times.actime = times.modtime = now;
		utime(gn->path, &times);
	}
}

/*-
 *-----------------------------------------------------------------------
 * Arch_MTime --
 *	Return the modification time of a member of an archive, given its
 *	name.
 *
 * Results:
 *	The modification time(seconds).
 *	XXXHB this should be a long.
 *
 * Side Effects:
 *	The mtime field of the given node is filled in with the value
 *	returned by the function.
 *
 *-----------------------------------------------------------------------
 */
int
Arch_MTime(GNode *gn)
{
	int64_t	mtime;
	char	*p1, *p2;

	mtime = ArchStatMember(Var_Value(ARCHIVE, gn, &p1),
	    Var_Value(TARGET, gn, &p2), TRUE);
	free(p1);
	free(p2);

	if (mtime == INT_MIN) {
		mtime = 0;
	}
	gn->mtime = (int)mtime;			/* XXX */
	return (gn->mtime);
}

/*-
 *-----------------------------------------------------------------------
 * Arch_MemMTime --
 *	Given a non-existent archive member's node, get its modification
 *	time from its archived form, if it exists.
 *
 * Results:
 *	The modification time.
 *
 * Side Effects:
 *	The mtime field is filled in.
 *
 *-----------------------------------------------------------------------
 */
int
Arch_MemMTime(GNode *gn)
{
	LstNode	*ln;
	GNode	*pgn;
	char	*nameStart;
	char	*nameEnd;

	for (ln = Lst_First(&gn->parents); ln != NULL; ln = Lst_Succ(ln)) {
		pgn = Lst_Datum(ln);

		if (pgn->type & OP_ARCHV) {
			/*
			 * If the parent is an archive specification and is
			 * being made and its member's name matches the name of
			 * the node we were given, record the modification time
			 * of the parent in the child. We keep searching its
			 * parents in case some other parent requires this
			 * child to exist...
			 */
			nameStart = strchr(pgn->name, '(') + 1;
			nameEnd = strchr(nameStart, ')');

			if (pgn->make && strncmp(nameStart, gn->name,
			    nameEnd - nameStart) == 0) {
				gn->mtime = Arch_MTime(pgn);
			}
		} else if (pgn->make) {
			/*
			 * Something which isn't a library depends on the
			 * existence of this target, so it needs to exist.
			 */
			gn->mtime = 0;
			break;
		}
	}
	return (gn->mtime);
}

/*-
 *-----------------------------------------------------------------------
 * Arch_FindLib --
 *	Search for a named library along the given search path.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The node's 'path' field is set to the found path (including the
 *	actual file name, not -l...). If the system can handle the -L
 *	flag when linking (or we cannot find the library), we assume that
 *	the user has placed the .LIBRARIES variable in the final linking
 *	command (or the linker will know where to find it) and set the
 *	TARGET variable for this node to be the node's name. Otherwise,
 *	we set the TARGET variable to be the full path of the library,
 *	as returned by Dir_FindFile.
 *
 *-----------------------------------------------------------------------
 */
void
Arch_FindLib(GNode *gn, struct Path *path)
{
	char	*libName;	/* file name for archive */
	size_t	sz;

	sz = strlen(gn->name) + 4;
	libName = emalloc(sz);
	snprintf(libName, sz, "lib%s.a", &gn->name[2]);

	gn->path = Path_FindFile(libName, path);

	free(libName);

#ifdef LIBRARIES
	Var_Set(TARGET, gn->name, gn);
#else
	Var_Set(TARGET, gn->path == NULL ? gn->name : gn->path, gn);
#endif /* LIBRARIES */
}

/*-
 *-----------------------------------------------------------------------
 * Arch_LibOODate --
 *	Decide if a node with the OP_LIB attribute is out-of-date. Called
 *	from Make_OODate to make its life easier, with the library's
 *	graph node.
 *
 *	There are several ways for a library to be out-of-date that are
 *	not available to ordinary files. In addition, there are ways
 *	that are open to regular files that are not available to
 *	libraries. A library that is only used as a source is never
 *	considered out-of-date by itself. This does not preclude the
 *	library's modification time from making its parent be out-of-date.
 *	A library will be considered out-of-date for any of these reasons,
 *	given that it is a target on a dependency line somewhere:
 *	    Its modification time is less than that of one of its
 *		  sources (gn->mtime < gn->cmtime).
 *	    Its modification time is greater than the time at which the
 *		  make began (i.e. it's been modified in the course
 *		  of the make, probably by archiving).
 *	    The modification time of one of its sources is greater than
 *		  the one of its RANLIBMAG member (i.e. its table of contents
 *		  is out-of-date). We don't compare of the archive time
 *		  vs. TOC time because they can be too close. In my
 *		  opinion we should not bother with the TOC at all since
 *		  this is used by 'ar' rules that affect the data contents
 *		  of the archive, not by ranlib rules, which affect the
 *		  TOC.
 *
 * Results:
 *	TRUE if the library is out-of-date. FALSE otherwise.
 *
 * Side Effects:
 *	The library will be hashed if it hasn't been already.
 *
 *-----------------------------------------------------------------------
 */
Boolean
Arch_LibOODate(GNode *gn)
{
	int64_t	mtime;	/* The table-of-contents's mod time */

	if (OP_NOP(gn->type) && Lst_IsEmpty(&gn->children)) {
		return (FALSE);
	}
	if (gn->mtime > now || gn->mtime < gn->cmtime) {
		return (TRUE);
	}

	mtime = ArchStatMember(gn->path, NULL, FALSE);
	if (mtime == INT64_MIN) {
		/*
		 * Not found. A library w/o a table of contents is out-of-date
		 */
		if (DEBUG(ARCH) || DEBUG(MAKE)) {
			Debug("No TOC...");
		}
		return (TRUE);
	}

	/* XXX choose one. */
	if (DEBUG(ARCH) || DEBUG(MAKE)) {
		Debug("TOC modified %s...", Targ_FmtTime(mtime));
	}
	return (gn->cmtime > mtime);
}
