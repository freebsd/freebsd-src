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
#include <sys/types.h>
#include <ar.h>
#include <ctype.h>
#include <regex.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <utime.h>

#include "arch.h"
#include "config.h"
#include "dir.h"
#include "globals.h"
#include "GNode.h"
#include "hash.h"
#include "make.h"
#include "targ.h"
#include "util.h"
#include "var.h"

/* Lst of archives we've already examined */
static Lst archives = Lst_Initializer(archives);

typedef struct Arch {
    char	*name;		/* Name of archive */
    Hash_Table	members;	/* All the members of the archive described
				 * by <name, struct ar_hdr *> key/value pairs */
    char	*fnametab;	/* Extended name table strings */
    size_t	fnamesize;	/* Size of the string table */
} Arch;

static struct ar_hdr *ArchStatMember(const char *, const char *, Boolean);
static FILE *ArchFindMember(const char *, const char *, struct ar_hdr *,
	const char *);
#if defined(__svr4__) || defined(__SVR4) || defined(__ELF__)
#define	SVR4ARCHIVES
static int ArchSVR4Entry(Arch *, char *, size_t, FILE *);
#endif

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
	     * Variable spec, so call the Var module to parse the puppy
	     * so we can safely advance beyond it...
	     */
	    size_t length;
	    Boolean	freeIt;
	    char	*result;

	    result = Var_Parse(cp, ctxt, TRUE, &length, &freeIt);
	    if (result == var_Error) {
		return (FAILURE);
	    } else {
		subLibName = TRUE;
	    }

	    if (freeIt) {
		free(result);
	    }
	    cp += length - 1;
	}
    }

    *cp++ = '\0';
    if (subLibName) {
	libName = Var_Subst(NULL, libName, ctxt, TRUE);
    }


    for (;;) {
	/*
	 * First skip to the start of the member's name, mark that
	 * place and skip to the end of it (either white-space or
	 * a close paren).
	 */
	Boolean	doSubst = FALSE; /* TRUE if need to substitute in memName */

	while (*cp != '\0' && *cp != ')' && isspace((unsigned char)*cp)) {
	    cp++;
	}
	memName = cp;
	while (*cp != '\0' && *cp != ')' && !isspace((unsigned char)*cp)) {
	    if (*cp == '$') {
		/*
		 * Variable spec, so call the Var module to parse the puppy
		 * so we can safely advance beyond it...
		 */
		size_t length;
		Boolean	freeIt;
		char	*result;

		result = Var_Parse(cp, ctxt, TRUE, &length, &freeIt);
		if (result == var_Error) {
		    return (FAILURE);
		} else {
		    doSubst = TRUE;
		}

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
	 * chances are there's something wrong (like a missing backslash),
	 * so it's better to return failure than allow such things to happen
	 */
	if (*cp == '\0') {
	    printf("No closing parenthesis in archive specification\n");
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
	 * SuffExpandChildren, both for the archive and the member portions.
	 */
	/*
	 * If member contains variables, try and substitute for them.
	 * This will slow down archive specs with dynamic sources, of course,
	 * since we'll be (non-)substituting them three times, but them's
	 * the breaks -- we need to do this since SuffExpandChildren calls
	 * us, otherwise we could assume the thing would be taken care of
	 * later.
	 */
	if (doSubst) {
	    char	*buf;
	    char	*sacrifice;
	    char	*oldMemName = memName;
	    size_t	sz;

	    memName = Var_Subst(NULL, memName, ctxt, TRUE);

	    /*
	     * Now form an archive spec and recurse to deal with nested
	     * variables and multi-word variable values.... The results
	     * are just placed at the end of the nodeLst we're returning.
	     */

	    sz = strlen(memName) + strlen(libName) + 3;
	    buf = sacrifice = emalloc(sz);

	    snprintf(buf, sz, "%s(%s)", libName, memName);

	    if (strchr(memName, '$') && strcmp(memName, oldMemName) == 0) {
		/*
		 * Must contain dynamic sources, so we can't deal with it now.
		 * Just create an ARCHV node for the thing and let
		 * SuffExpandChildren handle it...
		 */
		gn = Targ_FindNode(buf, TARG_CREATE);

		if (gn == NULL) {
		    free(buf);
		    return (FAILURE);
		} else {
		    gn->type |= OP_ARCHV;
		    Lst_AtEnd(nodeLst, (void *)gn);
		}
	    } else if (Arch_ParseArchive(&sacrifice, nodeLst, ctxt) != SUCCESS) {
		/*
		 * Error in nested call -- free buffer and return FAILURE
		 * ourselves.
		 */
		free(buf);
		return (FAILURE);
	    }
	    /*
	     * Free buffer and continue with our work.
	     */
	    free(buf);
	} else if (Dir_HasWildcards(memName)) {
	    Lst members = Lst_Initializer(members);
	    char  *member;
	    size_t sz = MAXPATHLEN;
	    size_t nsz;

	    nameBuf = emalloc(sz);

	    Dir_Expand(memName, &dirSearchPath, &members);
	    while (!Lst_IsEmpty(&members)) {
		member = Lst_DeQueue(&members);
		nsz = strlen(libName) + strlen(member) + 3;
		if (nsz > sz) {
			sz = nsz * 2;
			nameBuf = erealloc(nameBuf, sz);
		}

		snprintf(nameBuf, sz, "%s(%s)", libName, member);
		free(member);
		gn = Targ_FindNode(nameBuf, TARG_CREATE);
		if (gn == NULL) {
		    free(nameBuf);
		    /* XXXHB Lst_Destroy(&members) */
		    return (FAILURE);
		} else {
		    /*
		     * We've found the node, but have to make sure the rest of
		     * the world knows it's an archive member, without having
		     * to constantly check for parentheses, so we type the
		     * thing with the OP_ARCHV bit before we place it on the
		     * end of the provided list.
		     */
		    gn->type |= OP_ARCHV;
		    Lst_AtEnd(nodeLst, (void *)gn);
		}
	    }
	    free(nameBuf);
	} else {
	    size_t sz = strlen(libName) + strlen(memName) + 3;
	    nameBuf = emalloc(sz);
	    snprintf(nameBuf, sz, "%s(%s)", libName, memName);
	    gn = Targ_FindNode(nameBuf, TARG_CREATE);
	    free(nameBuf);
	    if (gn == NULL) {
		return (FAILURE);
	    } else {
		/*
		 * We've found the node, but have to make sure the rest of the
		 * world knows it's an archive member, without having to
		 * constantly check for parentheses, so we type the thing with
		 * the OP_ARCHV bit before we place it on the end of the
		 * provided list.
		 */
		gn->type |= OP_ARCHV;
		Lst_AtEnd(nodeLst, gn);
	    }
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

/*-
 *-----------------------------------------------------------------------
 * ArchFindArchive --
 *	See if the given archive is the one we are looking for. Called
 *	From ArchStatMember and ArchFindMember via Lst_Find with the
 *	current list element and the name we want.
 *
 * Results:
 *	0 if it is, non-zero if it isn't.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
static int
ArchFindArchive(const void *ar, const void *archName)
{

	return (strcmp(archName, ((const Arch *)ar)->name));
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
static struct ar_hdr *
ArchStatMember(const char *archive, const char *member, Boolean hash)
{
#define	AR_MAX_NAME_LEN	    (sizeof(arh.ar_name) - 1)
    FILE	*arch;	/* Stream to archive */
    int		size;	/* Size of archive member */
    char	*cp;	/* Useful character pointer */
    char	magic[SARMAG];
    LstNode	*ln;	/* Lst member containing archive descriptor */
    Arch	*ar;	/* Archive descriptor */
    Hash_Entry	*he;	/* Entry containing member's description */
    struct ar_hdr arh;	/* archive-member header for reading archive */
    char	memName[MAXPATHLEN + 1]; /* Current member name while hashing. */

    /*
     * Because of space constraints and similar things, files are archived
     * using their final path components, not the entire thing, so we need
     * to point 'member' to the final component, if there is one, to make
     * the comparisons easier...
     */
    cp = strrchr(member, '/');
    if ((cp != NULL) && (strcmp(member, RANLIBMAG) != 0))
	member = cp + 1;

    ln = Lst_Find(&archives, archive, ArchFindArchive);
    if (ln != NULL) {
	ar = Lst_Datum(ln);

	he = Hash_FindEntry(&ar->members, member);

	if (he != NULL) {
	    return ((struct ar_hdr *)Hash_GetValue (he));
	} else {
	    /* Try truncated name */
	    char copy[AR_MAX_NAME_LEN + 1];
	    size_t len = strlen(member);

	    if (len > AR_MAX_NAME_LEN) {
		len = AR_MAX_NAME_LEN;
		strncpy(copy, member, AR_MAX_NAME_LEN);
		copy[AR_MAX_NAME_LEN] = '\0';
	    }
	    if ((he = Hash_FindEntry(&ar->members, copy)) != NULL)
		return (Hash_GetValue(he));
	    return (NULL);
	}
    }

    if (!hash) {
	/*
	 * Caller doesn't want the thing hashed, just use ArchFindMember
	 * to read the header for the member out and close down the stream
	 * again. Since the archive is not to be hashed, we assume there's
	 * no need to allocate extra room for the header we're returning,
	 * so just declare it static.
	 */
	 static struct ar_hdr	sarh;

	 arch = ArchFindMember(archive, member, &sarh, "r");

	if (arch == NULL) {
	    return (NULL);
	} else {
	    fclose(arch);
	    return (&sarh);
	}
    }

    /*
     * We don't have this archive on the list yet, so we want to find out
     * everything that's in it and cache it so we can get at it quickly.
     */
    arch = fopen(archive, "r");
    if (arch == NULL) {
	return (NULL);
    }

    /*
     * We use the ARMAG string to make sure this is an archive we
     * can handle...
     */
    if ((fread(magic, SARMAG, 1, arch) != 1) ||
	(strncmp(magic, ARMAG, SARMAG) != 0)) {
	    fclose(arch);
	    return (NULL);
    }

    ar = emalloc(sizeof(Arch));
    ar->name = estrdup(archive);
    ar->fnametab = NULL;
    ar->fnamesize = 0;
    Hash_InitTable(&ar->members, -1);
    memName[AR_MAX_NAME_LEN] = '\0';

    while (fread(&arh, sizeof(struct ar_hdr), 1, arch) == 1) {
	if (strncmp(arh.ar_fmag, ARFMAG, sizeof(arh.ar_fmag)) != 0) {
	    /*
	     * The header is bogus, so the archive is bad
	     * and there's no way we can recover...
	     */
	    goto badarch;
	} else {
	    /*
	     * We need to advance the stream's pointer to the start of the
	     * next header. Files are padded with newlines to an even-byte
	     * boundary, so we need to extract the size of the file from the
	     * 'size' field of the header and round it up during the seek.
	     */
	    arh.ar_size[sizeof(arh.ar_size) - 1] = '\0';
	    size = (int)strtol(arh.ar_size, NULL, 10);

	    strncpy(memName, arh.ar_name, sizeof(arh.ar_name));
	    for (cp = &memName[AR_MAX_NAME_LEN]; *cp == ' '; cp--) {
		continue;
	    }
	    cp[1] = '\0';

#ifdef SVR4ARCHIVES
	    /*
	     * svr4 names are slash terminated. Also svr4 extended AR format.
	     */
	    if (memName[0] == '/') {
		/*
		 * svr4 magic mode; handle it
		 */
		switch (ArchSVR4Entry(ar, memName, size, arch)) {
		case -1:  /* Invalid data */
		    goto badarch;
		case 0:	  /* List of files entry */
		    continue;
		default:  /* Got the entry */
		    break;
		}
	    }
	    else {
		if (cp[0] == '/')
		    cp[0] = '\0';
	    }
#endif

#ifdef AR_EFMT1
	    /*
	     * BSD 4.4 extended AR format: #1/<namelen>, with name as the
	     * first <namelen> bytes of the file
	     */
	    if (strncmp(memName, AR_EFMT1, sizeof(AR_EFMT1) - 1) == 0 &&
		isdigit(memName[sizeof(AR_EFMT1) - 1])) {

		unsigned int elen = atoi(&memName[sizeof(AR_EFMT1)-1]);

		if (elen > MAXPATHLEN)
			goto badarch;
		if (fread(memName, elen, 1, arch) != 1)
			goto badarch;
		memName[elen] = '\0';
		fseek(arch, -elen, SEEK_CUR);
		/* XXX Multiple levels may be asked for, make this conditional
		 * on one, and use DEBUGF.
		 */
		if (DEBUG(ARCH) || DEBUG(MAKE)) {
		    fprintf(stderr, "ArchStat: Extended format entry for %s\n", memName);
		}
	    }
#endif

	    he = Hash_CreateEntry(&ar->members, memName, NULL);
	    Hash_SetValue(he, emalloc(sizeof(struct ar_hdr)));
	    memcpy(Hash_GetValue(he), &arh, sizeof(struct ar_hdr));
	}
	fseek(arch, (size + 1) & ~1, SEEK_CUR);
    }

    fclose(arch);

    Lst_AtEnd(&archives, ar);

    /*
     * Now that the archive has been read and cached, we can look into
     * the hash table to find the desired member's header.
     */
    he = Hash_FindEntry(&ar->members, member);

    if (he != NULL) {
	return (Hash_GetValue (he));
    } else {
	return (NULL);
    }

badarch:
    fclose(arch);
    Hash_DeleteTable(&ar->members);
    free(ar->fnametab);
    free(ar);
    return (NULL);
}

#ifdef SVR4ARCHIVES
/*-
 *-----------------------------------------------------------------------
 * ArchSVR4Entry --
 *	Parse an SVR4 style entry that begins with a slash.
 *	If it is "//", then load the table of filenames
 *	If it is "/<offset>", then try to substitute the long file name
 *	from offset of a table previously read.
 *
 * Results:
 *	-1: Bad data in archive
 *	 0: A table was loaded from the file
 *	 1: Name was successfully substituted from table
 *	 2: Name was not successfully substituted from table
 *
 * Side Effects:
 *	If a table is read, the file pointer is moved to the next archive
 *	member
 *
 *-----------------------------------------------------------------------
 */
static int
ArchSVR4Entry(Arch *ar, char *name, size_t size, FILE *arch)
{
#define	ARLONGNAMES1 "//"
#define	ARLONGNAMES2 "/ARFILENAMES"
    size_t entry;
    char *ptr, *eptr;

    if (strncmp(name, ARLONGNAMES1, sizeof(ARLONGNAMES1) - 1) == 0 ||
	strncmp(name, ARLONGNAMES2, sizeof(ARLONGNAMES2) - 1) == 0) {

	if (ar->fnametab != NULL) {
	    DEBUGF(ARCH, ("Attempted to redefine an SVR4 name table\n"));
	    return (-1);
	}

	/*
	 * This is a table of archive names, so we build one for
	 * ourselves
	 */
	ar->fnametab = emalloc(size);
	ar->fnamesize = size;

	if (fread(ar->fnametab, size, 1, arch) != 1) {
	    DEBUGF(ARCH, ("Reading an SVR4 name table failed\n"));
	    return (-1);
	}
	eptr = ar->fnametab + size;
	for (entry = 0, ptr = ar->fnametab; ptr < eptr; ptr++)
	    switch (*ptr) {
	    case '/':
		entry++;
		*ptr = '\0';
		break;

	    case '\n':
		break;

	    default:
		break;
	    }
	DEBUGF(ARCH, ("Found svr4 archive name table with %zu entries\n", entry));
	return (0);
    }

    if (name[1] == ' ' || name[1] == '\0')
	return (2);

    entry = (size_t)strtol(&name[1], &eptr, 0);
    if ((*eptr != ' ' && *eptr != '\0') || eptr == &name[1]) {
	DEBUGF(ARCH, ("Could not parse SVR4 name %s\n", name));
	return (2);
    }
    if (entry >= ar->fnamesize) {
	DEBUGF(ARCH, ("SVR4 entry offset %s is greater than %zu\n",
	       name, ar->fnamesize));
	return (2);
    }

    DEBUGF(ARCH, ("Replaced %s with %s\n", name, &ar->fnametab[entry]));

    strncpy(name, &ar->fnametab[entry], MAXPATHLEN);
    name[MAXPATHLEN] = '\0';
    return (1);
}
#endif


/*-
 *-----------------------------------------------------------------------
 * ArchFindMember --
 *	Locate a member of an archive, given the path of the archive and
 *	the path of the desired member. If the archive is to be modified,
 *	the mode should be "r+", if not, it should be "r".  arhPtr is a
 *	poitner to the header structure to fill in.
 *
 * Results:
 *	An FILE *, opened for reading and writing, positioned at the
 *	start of the member's struct ar_hdr, or NULL if the member was
 *	nonexistent. The current struct ar_hdr for member.
 *
 * Side Effects:
 *	The passed struct ar_hdr structure is filled in.
 *
 *-----------------------------------------------------------------------
 */
static FILE *
ArchFindMember(const char *archive, const char *member, struct ar_hdr *arhPtr,
    const char *mode)
{
    FILE	*arch;	/* Stream to archive */
    int		size;	/* Size of archive member */
    const char	*cp;	/* Useful character pointer */
    char	magic[SARMAG];
    size_t	len, tlen;

    arch = fopen(archive, mode);
    if (arch == NULL) {
	return (NULL);
    }

    /*
     * We use the ARMAG string to make sure this is an archive we
     * can handle...
     */
    if ((fread(magic, SARMAG, 1, arch) != 1) ||
	(strncmp(magic, ARMAG, SARMAG) != 0)) {
	    fclose(arch);
	    return (NULL);
    }

    /*
     * Because of space constraints and similar things, files are archived
     * using their final path components, not the entire thing, so we need
     * to point 'member' to the final component, if there is one, to make
     * the comparisons easier...
     */
    cp = strrchr(member, '/');
    if ((cp != NULL) && (strcmp(member, RANLIBMAG) != 0)) {
	member = cp + 1;
    }
    len = tlen = strlen(member);
    if (len > sizeof(arhPtr->ar_name)) {
	tlen = sizeof(arhPtr->ar_name);
    }

    while (fread(arhPtr, sizeof(struct ar_hdr), 1, arch) == 1) {
	if (strncmp(arhPtr->ar_fmag, ARFMAG, sizeof(arhPtr->ar_fmag) ) != 0) {
	     /*
	      * The header is bogus, so the archive is bad
	      * and there's no way we can recover...
	      */
	     fclose(arch);
	     return (NULL);
	} else if (strncmp(member, arhPtr->ar_name, tlen) == 0) {
	    /*
	     * If the member's name doesn't take up the entire 'name' field,
	     * we have to be careful of matching prefixes. Names are space-
	     * padded to the right, so if the character in 'name' at the end
	     * of the matched string is anything but a space, this isn't the
	     * member we sought.
	     */
	    if (tlen != sizeof(arhPtr->ar_name) && arhPtr->ar_name[tlen] != ' '){
		goto skip;
	    } else {
		/*
		 * To make life easier, we reposition the file at the start
		 * of the header we just read before we return the stream.
		 * In a more general situation, it might be better to leave
		 * the file at the actual member, rather than its header, but
		 * not here...
		 */
		fseek(arch, -sizeof(struct ar_hdr), SEEK_CUR);
		return (arch);
	    }
	} else
#ifdef AR_EFMT1
		/*
		 * BSD 4.4 extended AR format: #1/<namelen>, with name as the
		 * first <namelen> bytes of the file
		 */
	    if (strncmp(arhPtr->ar_name, AR_EFMT1,
					sizeof(AR_EFMT1) - 1) == 0 &&
		isdigit(arhPtr->ar_name[sizeof(AR_EFMT1) - 1])) {

		unsigned int elen = atoi(&arhPtr->ar_name[sizeof(AR_EFMT1)-1]);
		char ename[MAXPATHLEN];

		if (elen > MAXPATHLEN) {
			fclose(arch);
			return NULL;
		}
		if (fread(ename, elen, 1, arch) != 1) {
			fclose(arch);
			return NULL;
		}
		ename[elen] = '\0';
		/*
		 * XXX choose one.
		 */
		if (DEBUG(ARCH) || DEBUG(MAKE)) {
		    printf("ArchFind: Extended format entry for %s\n", ename);
		}
		if (strncmp(ename, member, len) == 0) {
			/* Found as extended name */
			fseek(arch, -sizeof(struct ar_hdr) - elen, SEEK_CUR);
			return (arch);
		}
		fseek(arch, -elen, SEEK_CUR);
		goto skip;
	} else
#endif
	{
skip:
	    /*
	     * This isn't the member we're after, so we need to advance the
	     * stream's pointer to the start of the next header. Files are
	     * padded with newlines to an even-byte boundary, so we need to
	     * extract the size of the file from the 'size' field of the
	     * header and round it up during the seek.
	     */
	    arhPtr->ar_size[sizeof(arhPtr->ar_size) - 1] = '\0';
	    size = (int)strtol(arhPtr->ar_size, NULL, 10);
	    fseek(arch, (size + 1) & ~1, SEEK_CUR);
	}
    }

    /*
     * We've looked everywhere, but the member is not to be found. Close the
     * archive and return NULL -- an error.
     */
    fclose(arch);
    return (NULL);
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
    FILE *	  arch;	  /* Stream open to archive, positioned properly */
    struct ar_hdr arh;	  /* Current header describing member */
    char *p1, *p2;

    arch = ArchFindMember(Var_Value(ARCHIVE, gn, &p1),
			  Var_Value(TARGET, gn, &p2),
			  &arh, "r+");
    free(p1);
    free(p2);
    snprintf(arh.ar_date, sizeof(arh.ar_date), "%-12ld", (long)now);

    if (arch != NULL) {
	fwrite(&arh, sizeof(struct ar_hdr), 1, arch);
	fclose(arch);
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
#ifdef RANLIBMAG
    FILE		*arch;	/* Stream open to archive */
    struct ar_hdr	arh;	/* Header describing table of contents */
    struct utimbuf	times;	/* Times for utime() call */

    arch = ArchFindMember(gn->path, RANLIBMAG, &arh, "r+");
    snprintf(arh.ar_date, sizeof(arh.ar_date), "%-12ld", (long) now);

    if (arch != NULL) {
	fwrite(&arh, sizeof(struct ar_hdr), 1, arch);
	fclose(arch);

	times.actime = times.modtime = now;
	utime(gn->path, &times);
    }
#endif
}

/*-
 *-----------------------------------------------------------------------
 * Arch_MTime --
 *	Return the modification time of a member of an archive, given its
 *	name.
 *
 * Results:
 *	The modification time(seconds).
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
    struct ar_hdr *arhPtr;    /* Header of desired member */
    int		  modTime;    /* Modification time as an integer */
    char *p1, *p2;

    arhPtr = ArchStatMember(Var_Value(ARCHIVE, gn, &p1),
			     Var_Value(TARGET, gn, &p2),
			     TRUE);
    free(p1);
    free(p2);

    if (arhPtr != NULL) {
	modTime = (int)strtol(arhPtr->ar_date, NULL, 10);
    } else {
	modTime = 0;
    }

    gn->mtime = modTime;
    return (modTime);
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
	     * If the parent is an archive specification and is being made
	     * and its member's name matches the name of the node we were
	     * given, record the modification time of the parent in the
	     * child. We keep searching its parents in case some other
	     * parent requires this child to exist...
	     */
	    nameStart = strchr(pgn->name, '(') + 1;
	    nameEnd = strchr(nameStart, ')');

	    if (pgn->make &&
		strncmp(nameStart, gn->name, nameEnd - nameStart) == 0) {
				     gn->mtime = Arch_MTime(pgn);
	    }
	} else if (pgn->make) {
	    /*
	     * Something which isn't a library depends on the existence of
	     * this target, so it needs to exist.
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
Arch_FindLib(GNode *gn, Lst *path)
{
    char	    *libName;   /* file name for archive */
    size_t	    sz;

    sz = strlen(gn->name) + 4;
    libName = emalloc(sz);
    snprintf(libName, sz, "lib%s.a", &gn->name[2]);

    gn->path = Dir_FindFile(libName, path);

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
    Boolean	  oodate;

    if (OP_NOP(gn->type) && Lst_IsEmpty(&gn->children)) {
	oodate = FALSE;
    } else if ((gn->mtime > now) || (gn->mtime < gn->cmtime)) {
	oodate = TRUE;
    } else {
#ifdef RANLIBMAG
	struct ar_hdr	*arhPtr;	/* Header for __.SYMDEF */
	int		modTimeTOC;	/* The table-of-contents's mod time */

	arhPtr = ArchStatMember(gn->path, RANLIBMAG, FALSE);

	if (arhPtr != NULL) {
	    modTimeTOC = (int)strtol(arhPtr->ar_date, NULL, 10);

	    /* XXX choose one. */
	    if (DEBUG(ARCH) || DEBUG(MAKE)) {
		printf("%s modified %s...", RANLIBMAG, Targ_FmtTime(modTimeTOC));
	    }
	    oodate = (gn->cmtime > modTimeTOC);
	} else {
	    /*
	     * A library w/o a table of contents is out-of-date
	     */
	    if (DEBUG(ARCH) || DEBUG(MAKE)) {
		printf("No t.o.c....");
	    }
	    oodate = TRUE;
	}
#else
	oodate = (gn->mtime == 0); /* out-of-date if not present */
#endif
    }
    return (oodate);
}

/*-
 *-----------------------------------------------------------------------
 * Arch_Init --
 *	Initialize things for this module.
 *
 * Results:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
void
Arch_Init(void)
{
}
