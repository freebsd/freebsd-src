/*
 * Copyright (c) 1992, 1993, 1996
 *	Berkeley Software Design, Inc.  All rights reserved.
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
 *	This product includes software developed by Berkeley Software
 *	Design, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY Berkeley Software Design, Inc. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Berkeley Software Design, Inc. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI intff.c,v 2.2 1996/04/08 19:32:56 bostic Exp
 *
 * $FreeBSD$
 */

#include "doscmd.h"
#include <sys/param.h>
#include <ctype.h>

#include "dispatch.h"

static LOL	*lol = 0;		/* DOS list-of-lists */
static SDA	*sda = 0;		/* DOS swappable data area */

/******************************************************************************
** redirector functions
**
**
** These are set up on entry to the redirector each time, and are referenced
** by the functions here.
*/
static int	r_drive,n_drive = 0;
static CDS	*r_cds;
static SFT	*r_sft;


/*
** 2f:11:0
**
** Installation check
*/
static int
int2f11_00(regcontext_t *REGS)
{
    R_AL = 0xff;
    R_AH = 'U';		/* and why not? 8) */
    return(0);
}

/*
** 2f:11:1 2f:11:2 2f:11:3 2f:11:4 2f:11:5 2f:11:11 2f:11:13
**
** Directory functions
*/
static int
int2f11_dirfn(regcontext_t *REGS)
{
    char	fname[PATH_MAX], tname[PATH_MAX];
    int		error;

    error = translate_filename(sda->filename1, fname, &r_drive);
    if (error)
	return(error);
    
    if (dos_readonly(r_drive) && (R_AL != 0x05))
	return(WRITE_PROT_DISK);

    switch(R_AL) {
    case 0x01:				/* rmdir */
    case 0x02:
	debug(D_REDIR,"rmdir(%s)\n",fname);
	error = rmdir(fname);
	break;
    case 0x03:				/* mkdir */
    case 0x04:
	debug(D_REDIR,"mkdir(%s)\n",fname);
	error = mkdir(fname,0777);
	break;
    case 0x05:				/* chdir */
	debug(D_REDIR,"chdir(%s)\n",fname);
	/* Note returns DOS error directly */
	return(dos_setcwd(sda->filename1));
	
    case 0x11:				/* rename */
	error = translate_filename(sda->filename2, tname, &r_drive);
	if (!error) {
	    debug(D_REDIR,"rename(%s,%s)\n",fname,tname);
	    error = rename(fname, tname);
	}
	break;

    case 0x13:				/* unlink */
	debug(D_REDIR,"unlink(%s)\n",fname);
	error = unlink(fname);
	break;
	
    default:
	fatal("called int2f11_dirfn on unknown function %x\n",R_AL);
    }
    
    if (error < 0) {
	switch(errno) {
	case ENOTDIR:
	case ENOENT:
	    return(PATH_NOT_FOUND);
	case EXDEV:
	    return(NOT_SAME_DEV);
	default:
	    return(ACCESS_DENIED);
	}
    }
    return(0);
}

/*
** 2f:11:6
**
** Close
*/
static int
int2f11_close(regcontext_t *REGS)
{
    int		fd;

    fd = r_sft->fd;
    debug(D_REDIR, "close(%d)\n", fd);
    
    r_sft->nfiles--;
    if (r_sft->nfiles) {
	debug(D_REDIR, "not last close\n");
	return(0);
    }
    if (close(fd) < 0)
	return(HANDLE_INVALID);
    return(0);
}

/*
** 2f:11:8 2f:11:9
**
** read/write
*/
static int
int2f11_rdwr(regcontext_t *REGS)
{
    int		fd;
    char	*addr;
    int		nbytes;
    int		n;

    fd = r_sft->fd;
    if (lseek(fd, r_sft->offset, SEEK_SET) < 0)
	return(SEEK_ERROR);

    addr = (char *)MAKEPTR(sda->dta_seg, sda->dta_off);
    nbytes = R_CX;
    
    switch(R_AL) {
    case 0x08:				/* read */
	debug(D_REDIR, "read(%d, %d)\n", fd, nbytes);
	n = read(fd, addr, nbytes);
	if (n < 0)
	    return(READ_FAULT);
	break;
    case 0x09:
	debug(D_REDIR, "write(%d, %d)\n", fd, nbytes);
	n = write(fd, addr, nbytes);
	if (n < 0)
	    return(WRITE_FAULT);
	break;
    default:
	fatal("called int2f11_rdwr on unknown function %x\n",R_AL);
    }
    
    R_CX = n;				/* report count */
    r_sft->offset += n;
    if (r_sft->offset > r_sft->size)
	r_sft->size = r_sft->offset;
    debug(D_REDIR, "offset now %d\n", r_sft->offset);
    return(0);
}

/*
** 2f:11:c
**
** Get free space (like 21:36)
*/
static int
int2f11_free(regcontext_t *REGS)
{
    fsstat_t	fs;
    int		error;
    
    error = get_space(r_drive, &fs);
    if (error)
	return (error);
    R_AX = fs.sectors_cluster;
    R_BX = fs.total_clusters;
    R_CX = fs.bytes_sector;
    R_DX = fs.avail_clusters;
    return(0);
}

/*
** 2f:11:f
**
** get size and mode
*/
static int
int2f11_stat(regcontext_t *REGS)
{
    char	fname[PATH_MAX];
    struct stat	sb;
    int		error;
    
    error = translate_filename(sda->filename1, fname, &r_drive);
    if (error)
	return(error);
    
    if (stat(fname, &sb) < 0)
	return(FILE_NOT_FOUND);
    
    R_AX = to_dos_attr(sb.st_mode);
    R_BX = sb.st_size >> 16;
    R_DI = sb.st_size & 0xffff;
    return(0);
}

/*
** 2f:11:16 2f:11:17 2f:11:18 2f:11:2e
**
** Open/create a file, closely resembles int21_open.
*/
static int
int2f11_open(regcontext_t *REGS)
{
    char	fname[PATH_MAX];
    struct stat sb;
    int		error;
    int		mode;			/* open mode */
    int		attr;			/* attributes of created file */
    int		action;			/* what to do about file */
    u_char	*p, *e;
    int		i;
    int		omode;			/* mode to say we opened in */
    int		status;
    int		fd;
    
    error = translate_filename(sda->filename1, fname, &r_drive);
    if (error)
	return(error);

    /* 
    ** get attributes/access mode off stack : low byte is attribute, high
    ** byte is (sometimes) used in conjunction with 'action'
    */
    attr = *(u_short *)MAKEPTR(R_SS, R_SP) & 0xff;

    /* which style? */
    switch(R_AL) {
    case 0x16:				/* open */
	action = 0x01;			/* fail if does not exist */
	switch (sda->open_mode & 3) {
	case 0:
	    mode = O_RDONLY;
	    break;
	case 1:
	    mode = O_WRONLY;
	    break;
	case 2:
	    mode = O_RDWR;
	    break;
	default:
	    return (FUNC_NUM_IVALID);
	}
	omode = sda->open_mode & 0x7f;
	debug(D_REDIR,"open");
	break;

    case 0x17:				/* creat/creat new */
    case 0x18:				/* creat/creat new (no CDS, but we don't care)*/
	mode = O_RDWR;
	omode = 3;
	if (attr & 0x100) {		/* creat new */
	    action = 0x10;		/* create if not exist, fail if exists */
	    debug(D_REDIR, "creat_new");
	} else {			/* creat */
	    action = 0x12;		/* create and destroy */
	    debug(D_REDIR, "creat");
	}
	break;

    case 0x2e:				/* multipurpose */
	attr = sda->ext_attr;
	action = sda->ext_action;
	switch (sda->ext_mode & 3) {
	case 0:
	    mode = O_RDONLY;
	    break;
	case 1:
	    mode = O_WRONLY;
	    break;
	case 2:
	    mode = O_RDWR;
	    break;
	default:
	    return (FUNC_NUM_IVALID);
	}
	omode = sda->ext_mode & 0x7f;
	debug(D_REDIR,"mopen");
	break;

    default:
	fatal("called int2f11_open for unknown function %x\n",R_AL);
    }
    if (action & 0x02)	/* replace/open mode */
	mode |= O_TRUNC;
    debug(D_REDIR, "(%s) action 0x%x  mode 0x%x  attr 0x%x omode 0x%x \n", 
	  fname, action, mode, attr, omode);
	
    if (ustat(fname, &sb) < 0) {			/* file does not exist */
	if ((action & 0x10) || (attr & 0x100)) {	/* create it */
	    sb.st_ino = 0;
	    mode |= O_CREAT;				/* have to create as we go */
	    status = 0x02;				/* file created */
	} else {
	    return(FILE_NOT_FOUND);			/* fail */
	}
    } else {
	if (S_ISDIR(sb.st_mode))
	    return(ACCESS_DENIED);
	if ((action & 0x03) && !(attr & 0x100)) {	/* exists, work with it */
	    if (action & 0x02) {
		if (!S_ISREG(sb.st_mode)) {		/* only allowed for files */
		    debug(D_FILE_OPS,"attempt to truncate non-regular file\n");
		    return(ACCESS_DENIED);
		}
		status = 0x03;				/* we're going to truncate it */
	    } else {
		status = 0x01;				/* just open it */
	    }
	} else {
	    return(FILE_ALREADY_EXISTS);		/* exists, fail */
	}
    }

    if ((fd = open(fname, mode, from_dos_attr(attr))) < 0) {
	debug(D_FILE_OPS,"failed to open %s : %s\n",fname,strerror(errno));
	return (ACCESS_DENIED);
    }

    if (R_AL == 0x2e)			/* extended wants status returned */
	R_CX = status;

    /* black magic to populate the SFT */

    e = p = sda->filename1 + 2;		/* parse name */
    while (*p) {
	if (*p++ == '\\')		/* look for beginning of filename */
	    e = p;
    }

    for (i = 0; i < 8; ++i) {		/* copy name and pad with spaces */
	if (*e && *e != '.')
	    r_sft->name[i] = *e++;
	else
	    r_sft->name[i] = ' ';
    }

    if (*e == '.')			/* skip period on short names */
	++e;
    
    for (i = 0; i < 3; ++i) {		/* copy extension and pad with spaces */
	if (*e)
	    r_sft->ext[i] = *e++;
	else
	    r_sft->ext[i] = ' ';
    }

    if (ustat(fname, &sb) < 0)		/* re-stat to be accurate */
	return(WRITE_FAULT);		/* any better ideas?! */

    r_sft->open_mode = omode;		/* file open mode */
    *(u_long *)r_sft->ddr_dpb = 0;	/* no parameter block */
    r_sft->size = sb.st_size;		/* current size */
    r_sft->fd = fd;			/* our fd for it (hidden in starting cluster number) */
    r_sft->offset = 0;			/* current offset is 0 */
    *(u_short *)r_sft->dir_sector = 0;	/* not local file, ignored */
    r_sft->dir_entry = 0;		/* not local file, ignored */
    r_sft->attribute = attr & 0xff;	/* file attributes as requested */
    r_sft->info = r_drive + 0x8040;	/* hide drive number here for later reference */
    encode_dos_file_time(sb.st_mtime, &r_sft->date, &r_sft->time);
    debug(D_REDIR,"success, fd %d  status %x\n", fd, status);
    return(0);
}

/*
** 2f:11:19 2f:11:1b
**
** find first
*/
static int
int2f11_findfirst(regcontext_t *REGS)
{
    return(find_first(sda->filename1,sda->attrmask,
		      (dosdir_t *)sda->foundentry,
		      (find_block_t *)sda->findfirst));
}

/*
** 2f:11:1c
**
** find next
*/
static int
int2f11_findnext(regcontext_t *REGS)
{
    return(find_next((dosdir_t *)sda->foundentry,
		     (find_block_t *)sda->findfirst));
}

/*
** 2f:11:21
**
** lseek
*/
static int
int2f11_lseek(regcontext_t *REGS)
{
    int		fd;
    off_t	offset;

    fd = r_sft->fd;
    offset = (off_t) ((int) ((R_CX << 16) + R_DX));
    
    debug(D_REDIR,"lseek(%d, 0x%qx, SEEK_END)\n", fd, offset);
    
    if ((offset = lseek(fd, offset, SEEK_END)) < 0) {
	if (errno == EBADF)
	    return(HANDLE_INVALID);
	else
	    return(SEEK_ERROR);
    }
    r_sft->offset = offset;		/* update offset in SFT */
    R_DX = offset >> 16;
    R_AX = offset;
    return(0);
}

/*
** 2f:11:23
**
** qualify filename
*/
static int
int2f11_fnqual(regcontext_t *REGS)
{
    char	*fname,*tname;
    int		savedrive;
    int		error;

    return(PATH_NOT_FOUND);
    
    savedrive = diskdrive;		/* to get CWD for network drive */
    diskdrive = n_drive;
    fname = (char *)MAKEPTR(R_DS, R_SI);	/* path pointers */
    tname = (char *)MAKEPTR(R_ES, R_DI);
    
    error = dos_makepath(fname, tname);
    if (error)
	tname = "(failed)";

    diskdrive = savedrive;		/* restore correct drive */
    
    debug(D_REDIR, "qualify '%s' -> '%s'\n", fname, tname);
    return(error);
}

/*
** 2f:11:??
**
** Null function - we know about it but do nothing
*/
static int
int2f11_NULLFUNC(regcontext_t *REGS)
{
    return(0);
}

/*
** 2f:11:??
**
** no function - not handled here (error)
*/
static int
int2f11_NOFUNC(regcontext_t *REGS)
{
    return(-1);
}

struct intfunc_table int2f11_table[] = {
    { 0x00,	IFT_NOSUBFUNC,	int2f11_00,		"installation check"},
    { 0x01,	IFT_NOSUBFUNC,	int2f11_dirfn,		"rmdir"},
    { 0x02,	IFT_NOSUBFUNC,	int2f11_dirfn,		"rmdir"},
    { 0x03,	IFT_NOSUBFUNC,	int2f11_dirfn,		"mkdir"},
    { 0x04,	IFT_NOSUBFUNC,	int2f11_dirfn,		"mkdir"},
    { 0x05,	IFT_NOSUBFUNC,	int2f11_dirfn,		"chdir"},
    { 0x06,	IFT_NOSUBFUNC,	int2f11_close,		"close"},
    { 0x07,	IFT_NOSUBFUNC,	int2f11_NULLFUNC,	"commit file"},
    { 0x08,	IFT_NOSUBFUNC,	int2f11_rdwr,		"read"},
    { 0x09,	IFT_NOSUBFUNC,	int2f11_rdwr,		"write"},
    { 0x0a,	IFT_NOSUBFUNC,	int2f11_NULLFUNC,	"lock region"},
    { 0x0b,	IFT_NOSUBFUNC,	int2f11_NULLFUNC,	"unlock region"},
    { 0x0c,	IFT_NOSUBFUNC,	int2f11_free,		"free space"},
    { 0x0e,	IFT_NOSUBFUNC,	int2f11_NULLFUNC,	"chmod"},
    { 0x0f,	IFT_NOSUBFUNC,	int2f11_stat,		"stat"},
    { 0x11,	IFT_NOSUBFUNC,	int2f11_dirfn,		"rename"},
    { 0x13,	IFT_NOSUBFUNC,	int2f11_dirfn,		"unlink"},
    { 0x16,	IFT_NOSUBFUNC,	int2f11_open,		"open"},
    { 0x17,	IFT_NOSUBFUNC,	int2f11_open,		"creat"},
    { 0x18,	IFT_NOSUBFUNC,	int2f11_open,		"creat"},
    { 0x19,	IFT_NOSUBFUNC,	int2f11_findfirst,	"find first"},
    { 0x1b,	IFT_NOSUBFUNC,	int2f11_findfirst,	"find first"},
    { 0x1c,	IFT_NOSUBFUNC,	int2f11_findnext,	"find next"},
    { 0x1d,	IFT_NOSUBFUNC,	int2f11_NULLFUNC,	"close all (abort)"},
    { 0x1e,	IFT_NOSUBFUNC,	int2f11_NULLFUNC,	"do redirection"},
    { 0x1f,	IFT_NOSUBFUNC,	int2f11_NULLFUNC,	"printer setup"},
    { 0x20,	IFT_NOSUBFUNC,	int2f11_NULLFUNC,	"flush all buffers"},
    { 0x21,	IFT_NOSUBFUNC,	int2f11_lseek,		"lseek"},
    { 0x22,	IFT_NOSUBFUNC,	int2f11_NULLFUNC,	"process terminated"},
    { 0x23,	IFT_NOSUBFUNC,	int2f11_fnqual,		"qualify filename"},
    { 0x24,	IFT_NOSUBFUNC,	int2f11_NOFUNC,		"turn off printer"},
    { 0x25,	IFT_NOSUBFUNC,	int2f11_NOFUNC,		"printer mode"},
    { 0x2d,	IFT_NOSUBFUNC,	int2f11_NOFUNC,		"extended attributes"},
    { 0x2e,	IFT_NOSUBFUNC,	int2f11_open,		"extended open/create"},
    { -1,	0,		NULL,			NULL}
};

static int int2f11_fastlookup[256];

/******************************************************************************
** 2f:11
**
** The DOS redirector interface.
*/

/*
** Verify that the drive being referenced is one we are handling, and
** establish some state for upcoming functions.
** 
** Returns 1 if we should handle this request.
**
** XXX this is rather inefficient, but much easier to read than the previous
** incarnation 8(
*/
static int
int2f11_validate(regcontext_t *REGS)
{
    int		func = R_AL;
    char	*path = NULL;
    int		doit = 0;

    /* defaults may help trap problems */
    r_cds = NULL;
    r_sft = NULL;
    r_drive = -1;
    
    /* some functions we accept regardless */
    switch (func) {
    case 0x00:		/* installation check */
    case 0x23:		/* qualify path */
    case 0x1c:		/* XXX really only valid if a search already started... */
	return(1);
    }
    
    /* Where's the CDS? */
    switch(func) {
    case 0x01:		/* referenced by the SDA */
    case 0x02:
    case 0x03:
    case 0x04:
    case 0x05:
    case 0x0e:
    case 0x0f:
    case 0x11:
    case 0x13:
    case 0x17:
    case 0x1b:
	r_cds = (CDS *)MAKEPTR(sda->cds_seg, sda->cds_off);
	break;

    case 0x0c:		/* in es:di */
    case 0x1c:
	r_cds = (CDS *)MAKEPTR(R_ES, R_DI);
	break;
    }

    /* Where's the SFT? */
    switch(func) {
    case 0x06:		/* in es:di */
    case 0x07:
    case 0x08:
    case 0x09:
    case 0x0a:
    case 0x0b:
    case 0x16:
    case 0x17:
    case 0x18:
    case 0x21:
    case 0x2d:
    case 0x2e:
	r_sft = (SFT *)MAKEPTR(R_ES, R_DI);
	break;
    }
    
    /* What drive? */
    switch(func) {
    case 0x01:		/* get drive from fully-qualified path in SDA */
    case 0x02:
    case 0x03:
    case 0x04:
    case 0x05:
    case 0x0c:
    case 0x0e:
    case 0x0f:
    case 0x11:
    case 0x13:
    case 0x16:
    case 0x17:
    case 0x18:
    case 0x19:
    case 0x1b:
    case 0x2e:
	path = sda->filename1;
	break;
	
    case 0x06:		/* get drive from SFT (we put it here when file was opened) */
    case 0x07:
    case 0x08:
    case 0x09:
    case 0x0a:
    case 0x0b:
    case 0x21:
    case 0x2d:
	r_drive = r_sft->info & 0x1f;
	break;
    }

    if (path) {		/* we have a path and need to determine the drive it refers to */

	if (path[1] != ':') {	/* must be fully qualified; we cannot handle this */
	    debug(D_REDIR,"attempt to work non-absolute path %s\n",path);
	    return(0);
	}

	/* translate letter to drive number */
	r_drive = drlton(path[0]);
    } else {
	path = "(no path)";
    }
        
    /* do we handle this drive? */
    if (dos_getcwd(r_drive)) {
	n_drive = r_drive;		/* XXX GROSTIC HACK ALERT */
	doit = 1;
    }
    
    debug(D_REDIR,"%s -> drive %c func %x (%sus)\n",
	  path, drntol(r_drive), func, doit?"":"not ");
    
    /* so do we deal with this one? */
    return(doit);
}

    
int 
int2f_11(regcontext_t *REGS)
{
    int		index;
    int		error;
    char	*fname;
	
    
    if (!sda) {		/* not initialised yet */
	error = FUNC_NUM_IVALID;
    } else {

	index = intfunc_find(int2f11_table, int2f11_fastlookup, R_AL, 0);
	if (index == -1) {
	    debug(D_ALWAYS,"no handler for int2f:11:%x\n", R_AL);
	    return(0);
	}
	reset_poll();
	

	if (!int2f11_validate(REGS)) {		/* determine whether we handle this request */
	    error = -1;				/* not handled by us */
	} else {
	    debug(D_REDIR, "REDIR: %02x (%s)\n", 
		      int2f11_table[index].func, int2f11_table[index].desc);
	    /* call the handler */
	    error = int2f11_table[index].handler(REGS);
	    if (error != -1)
		debug(D_REDIR, "REDIR: returns %d (%s)\n", 
		      error, ((error >= 0) && (error <= dos_ret_size)) ? dos_return[error] : "unknown");
	}
    }

    if (error == -1)
	return (0);
    if (error) {
	R_AX = error;
	R_FLAGS |= PSL_C;
    } else
	R_FLAGS &= ~PSL_C;
    return (1);
}

/******************************************************************************
** intff handler.
**
** intff is the (secret, proprietary, internal, evil) call to 
** initialise the redirector.
*/
static void
install_drive(int drive, u_char *path)
{
    CDS *cds;

    /* check that DOS considers this a valid drive */
    if (drive < 0 || drive >= lol->lastdrive) {
	debug(D_REDIR, "Drive %c beyond limit of %c\n",
	      drntol(drive), drntol(lol->lastdrive - 1));
	return;
    }

    /* get the CDS for this drive */
    cds = (CDS *)MAKEPTR(lol->cds_seg, lol->cds_offset);
    cds += drive;

#if 0	/* XXX looks OK to me - mjs */
    if (cds->flag & (CDS_remote | CDS_ready)) {
	debug(D_REDIR, "Drive %c already installed\n", drntol(drive));
	return;
    }
#endif

    debug(D_REDIR, "Installing %c: as %s\n", drntol(drive), path);

    cds->flag |= CDS_remote | CDS_ready | CDS_notnet;
    cds->path[0] = drntol(drive);
    cds->path[1] = ':';
    cds->path[2] = '\\';
    cds->path[3] = '\0';
    cds->offset = 2;	/* offset of \ in current path field */
}

static void
init_drives(void)
{
    int drive;
    u_char *path;

    /* for all possible drives */
    for (drive = 0; drive < 26; ++drive) {
	if (path = dos_getpath(drive))		/* assigned to a path? */
	    install_drive(drive, path);		/* make it visible to DOS */
    }	
}


void
intff(regcontext_t *REGS)
{

    if (lol && sda) {				/* already been called? */
	debug(D_REDIR, "redirector duplicate install ignored");
	return;
    }
    lol = (LOL *)MAKEPTR(R_BX, R_DX);	/* where DOS keeps its goodies */
    sda = (SDA *)MAKEPTR(R_DI, R_SI);
    init_drives();
    
    /* initialise dispatcher */
    intfunc_init(int2f11_table, int2f11_fastlookup);
}
