/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: media_strategy.c,v 1.9 1995/05/21 19:51:50 gpalmer Exp $
 *
 * Copyright (c) 1995
 *	Jordan Hubbard.  All rights reserved.
 * Copyright (c) 1995
 * 	Gary J Palmer. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer, 
 *    verbatim and that no modifications are made prior to this 
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jordan Hubbard
 *	for the FreeBSD Project.
 * 4. The name of Jordan Hubbard or the FreeBSD project may not be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JORDAN HUBBARD ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL JORDAN HUBBARD OR HIS PETS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include "sysinstall.h"
#include <ctype.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/dkbad.h>
#include <sys/mman.h>

#define MSDOSFS
#define CD9660
#define NFS
#include <sys/mount.h>
#undef MSDOSFS
#undef CD9660
#undef NFS

#define MAX_ATTRIBS	20
#define MAX_NAME	511
#define MAX_VALUE	4095

struct attribs {
    char 		*name;
    char 		*value;
};

static int 		lno;
static int		num_attribs;

static int
attr_parse(struct attribs **attr, char *file)
{
    char hold_n[MAX_NAME+1];
    char hold_v[MAX_VALUE+1];
    int n, v, ch = 0;
    enum { LOOK, COMMENT, NAME, VALUE, COMMIT } state;
    FILE *fp;
    
    num_attribs = n = v = lno = 0;
    state = LOOK;
    
    if ((fp=fopen(file, "r")) == NULL)
    {
	msgConfirm("Cannot open the information file `%s': %s (%d)", file, strerror(errno), errno);
	return 0;
    }

    while (state == COMMIT || (ch = fgetc(fp)) != EOF) {
	/* Count lines */
	if (ch == '\n')
	    ++lno;
	switch(state) {
	case LOOK:
	    if (isspace(ch))
		continue;
	    /* Allow shell or lisp style comments */
	    else if (ch == '#' || ch == ';') {
		state = COMMENT;
		continue;
	    }
	    else if (isalpha(ch)) {
		hold_n[n++] = ch;
		state = NAME;
	    }
	    else
		msgFatal("Invalid character '%c' at line %d\n", ch, lno);
	    break;
	    
	case COMMENT:
	    if (ch == '\n')
		state = LOOK;
	    break;
	    
	case NAME:
	    if (ch == '\n') {
		hold_n[n] = '\0';
		hold_v[v = 0] = '\0';
		state = COMMIT;
	    }
	    else if (isspace(ch))
		continue;
	    else if (ch == '=') {
		hold_n[n] = '\0';
		state = VALUE;
	    }
	    else
		hold_n[n++] = ch;
	    break;
	    
	case VALUE:
	    if (v == 0 && isspace(ch))
		continue;
	    else if (ch == '{') {
		/* multiline value */
		while ((ch = fgetc(fp)) != '}') {
		    if (ch == EOF)
			msgFatal("Unexpected EOF on line %d", lno);
		    else {
		    	if (v == MAX_VALUE)
			    msgFatal("Value length overflow at line %d", lno);
		        hold_v[v++] = ch;
		    }
		}
		hold_v[v] = '\0';
		state = COMMIT;
	    }
	    else if (ch == '\n') {
		hold_v[v] = '\0';
		state = COMMIT;
	    }
	    else {
		if (v == MAX_VALUE)
		    msgFatal("Value length overflow at line %d", lno);
		else
		    hold_v[v++] = ch;
	    }
	    break;
	    
	case COMMIT:
	    (*attr)[num_attribs].name = strdup(hold_n);
	    (*attr)[num_attribs++].value = strdup(hold_v);
	    state = LOOK;
	    v = n = 0;
	    break;

	default:
	    msgFatal("Unknown state at line %d??\n", lno);
	}
    }
    return 1;
}

static const char *
attr_match(struct attribs *attr, char *name)
{
    int n = 0;

    while((strcmp(attr[n].name, name)!=0) && (n < num_attribs) && (n < 20))
	n++;

    if (strcmp(attr[n].name, name)==0)
	return((const char *) attr[n].value);

    return NULL;
}

static int
genericGetDist(char *path, struct attribs *dist_attrib)
{
    int 	fd;
    char 	buf[512];
    struct stat	sb;
    int		pfd[2], pid, numchunks;
    const char *tmp;

    snprintf(buf, 512, "%s.tgz", path);

    if (stat(buf, &sb) == 0)
    {
	fd = open(buf, O_RDONLY, 0);
	return(fd);
    }

    snprintf(buf, 512, "%s.aa", path);
    if (stat(buf, &sb) != 0)
    {
	msgConfirm("Cannot find file(s) for distribution in ``%s''!\n", path);
	return -1;
    }

    tmp = attr_match(dist_attrib, "pieces");
    numchunks = atoi(tmp);
    msgDebug("Attempting to extract distribution from %u files\n", numchunks);
    pipe(pfd);
    pid = fork();
    if (!pid)
    {
	caddr_t		memory;
	int		chunk = 0;
	int		retval;

	dup2(pfd[1], 1); close(pfd[1]);
	close(pfd[0]);

	while (chunk < numchunks)
	{
	    int		fd;

	    snprintf(buf, 512, "%s.%c%c", path, (chunk / 26) + 'a', (chunk % 26) + 'a');
	    if ((fd = open(buf, O_RDONLY)) == -1)
		msgFatal("Cannot find file `%s'!\n", buf);

	    fstat(fd, &sb);
	    msgDebug("mmap()ing %s (%d)\n", buf, fd);
	    memory = mmap(0, sb.st_size, PROT_READ, MAP_SHARED, fd, (off_t) 0);
	    if (memory == (caddr_t) -1)
		msgFatal("mmap error: %s\n", strerror(errno));

 	    retval = write(1, memory, sb.st_size);
	    if (retval != sb.st_size)
	    {
		msgConfirm("write didn't write out the complete file!\n
(wrote %d bytes of %d bytes)\n", retval, sb.st_size);
		exit(1);
	    }

	    retval = munmap(memory, sb.st_size);
	    if (retval != 0)
	    {
		msgConfirm("munmap() returned %d\n", retval);
		exit(1);
	    }
	    close(fd);
	    ++chunk;
	}
	close(1);
	msgDebug("Extract of %s finished!!!\n", path);
	exit(0);
    }
    close(pfd[1]);
    return(pfd[0]);
}

/* Various media "strategy" routines */

static Boolean cdromMounted;

Boolean
mediaInitCDROM(Device *dev)
{
    struct iso_args	args;
    struct stat		sb;

    if (cdromMounted)
	return TRUE;

    if (Mkdir("/cdrom", NULL))
	return FALSE;

    args.fspec = dev->devname;
    args.flags = 0;

    if (mount(MOUNT_CD9660, "/cdrom", MNT_RDONLY, (caddr_t) &args) == -1)
    {
	msgConfirm("Error mounting %s on /cdrom: %s (%u)\n",
		   dev, strerror(errno), errno);
	return FALSE;
    }

    /* Do a very simple check to see if this looks roughly like a 2.0.5 CDROM
       Unfortunately FreeBSD won't let us read the ``label'' AFAIK, which is one
       sure way of telling the disc version :-( */
    if (stat("/cdrom/dists", &sb))
    {
	if (errno == ENOENT)
	{
	    msgConfirm("Couldn't locate the directory `dists' on the cdrom\n\
Is this a 2.0.5 CDROM?\n");
	    return FALSE;
	} else {
	    msgConfirm("Couldn't stat directory %s: %s", "/cdrom/dists", strerror(errno));
	    return FALSE;
	}
    }
    cdromMounted = TRUE;
    return TRUE;
}

Boolean
mediaGetCDROM(char *dist)
{
    char		buf[PATH_MAX];
    struct attribs	*dist_attr;
    int			retval;

    dist_attr = safe_malloc(sizeof(struct attribs) * MAX_ATTRIBS);

    snprintf(buf, PATH_MAX, "/stand/info/%s.inf", dist);

    if (attr_parse(&dist_attr, buf) == 0)
    {
	msgConfirm("Cannot load information file for distribution\n");
	return FALSE;
    }
   
    snprintf(buf, PATH_MAX, "/cdrom/dists/%s", dist);

    retval = genericGetDist(buf, dist_attr);
    free(dist_attr);
    return retval;
}

void
mediaCloseCDROM(Device *dev)
{
    if (unmount("/cdrom", 0) != 0)
	msgConfirm("Could not unmount the CDROM: %s\n", strerror(errno));
    cdromMounted = FALSE;
    return;
}

Boolean
mediaInitFloppy(Device *dev)
{
    if (Mkdir("/mnt", NULL))
	return FALSE;

    return TRUE;
}

Boolean
mediaGetFloppy(char *dist)
{
    char		buf[PATH_MAX];
    struct attribs	*dist_attr;
    int			retval;

    dist_attr = safe_malloc(sizeof(struct attribs) * MAX_ATTRIBS);

    snprintf(buf, PATH_MAX, "/stand/info/%s.inf", dist);
    if (attr_parse(&dist_attr, buf) == 0)
    {
	msgConfirm("Cannot load information file for distribution\n");
	return FALSE;
    }
   
    snprintf(buf, PATH_MAX, "/mnt/%s", dist);

    retval = genericGetDist(buf, dist_attr);
    free(dist_attr);

    return retval;
}

void
mediaCloseFloppy(Device *dev)
{
    return;
}

Boolean
mediaInitTape(Device *dev)
{
    return TRUE;
}

Boolean
mediaInitNetwork(Device *dev)
{
    return TRUE;
}

Boolean
mediaGetTape(char *dist)
{
    return TRUE;
}

void
mediaCloseTape(Device *dev)
{
    return;
}

void
mediaCloseNetwork(Device *dev)
{
    return;
}

Boolean
mediaInitFTP(Device *dev)
{
    return TRUE;
}

Boolean
mediaGetFTP(char *dist)
{
    return TRUE;
}

void
mediaCloseFTP(Device *dev)
{
}

Boolean
mediaInitUFS(Device *dev)
{
    return TRUE;
}

Boolean
mediaGetUFS(char *dist)
{
    return TRUE;
}

/* UFS has no close routine since this is handled at the device level */


Boolean
mediaInitDOS(Device *dev)
{
    return TRUE;
}

Boolean
mediaGetDOS(char *dist)
{
    return TRUE;
}

void
mediaCloseDOS(Device *dev)
{
}
