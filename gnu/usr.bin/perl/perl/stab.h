/* $RCSfile: stab.h,v $$Revision: 1.1.1.1 $$Date: 1993/08/23 21:29:39 $
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log: stab.h,v $
 * Revision 1.1.1.1  1993/08/23  21:29:39  nate
 * PERL!
 *
 * Revision 4.0.1.3  92/06/08  15:33:44  lwall
 * patch20: fixed confusion between a *var's real name and its effective name
 * patch20: ($<,$>) = ... didn't work on some architectures
 * 
 * Revision 4.0.1.2  91/11/05  18:36:15  lwall
 * patch11: length($x) was sometimes wrong for numeric $x
 * 
 * Revision 4.0.1.1  91/06/07  11:56:35  lwall
 * patch4: new copyright notice
 * patch4: length($`), length($&), length($') now optimized to avoid string copy
 * 
 * Revision 4.0  91/03/20  01:39:49  lwall
 * 4.0 baseline.
 * 
 */

struct stabptrs {
    char        stbp_magic[4];
    STR		*stbp_val;	/* scalar value */
    struct stio *stbp_io;	/* filehandle value */
    FCMD	*stbp_form;	/* format value */
    ARRAY	*stbp_array;	/* array value */
    HASH	*stbp_hash;	/* associative array value */
    STAB	*stbp_stab;	/* effective stab, if *glob */
    SUBR	*stbp_sub;	/* subroutine value */
    int		stbp_lastexpr;	/* used by nothing_in_common() */
    line_t	stbp_line;	/* line first declared at (for -w) */
    char	stbp_flags;
};

#if defined(CRIPPLED_CC) && (defined(iAPX286) || defined(M_I286) || defined(I80286))
#define MICROPORT
#endif

#define stab_magic(stab)	(((STBP*)(stab->str_ptr))->stbp_magic)
#define stab_val(stab)		(((STBP*)(stab->str_ptr))->stbp_val)
#define stab_io(stab)		(((STBP*)(stab->str_ptr))->stbp_io)
#define stab_form(stab)		(((STBP*)(stab->str_ptr))->stbp_form)
#define stab_xarray(stab)	(((STBP*)(stab->str_ptr))->stbp_array)
#ifdef	MICROPORT	/* Microport 2.4 hack */
ARRAY *stab_array();
#else
#define stab_array(stab)	(((STBP*)(stab->str_ptr))->stbp_array ? \
				 ((STBP*)(stab->str_ptr))->stbp_array : \
				 ((STBP*)(aadd(stab)->str_ptr))->stbp_array)
#endif
#define stab_xhash(stab)	(((STBP*)(stab->str_ptr))->stbp_hash)
#ifdef	MICROPORT	/* Microport 2.4 hack */
HASH *stab_hash();
#else
#define stab_hash(stab)		(((STBP*)(stab->str_ptr))->stbp_hash ? \
				 ((STBP*)(stab->str_ptr))->stbp_hash : \
				 ((STBP*)(hadd(stab)->str_ptr))->stbp_hash)
#endif			/* Microport 2.4 hack */
#define stab_sub(stab)		(((STBP*)(stab->str_ptr))->stbp_sub)
#define stab_lastexpr(stab)	(((STBP*)(stab->str_ptr))->stbp_lastexpr)
#define stab_line(stab)		(((STBP*)(stab->str_ptr))->stbp_line)
#define stab_flags(stab)	(((STBP*)(stab->str_ptr))->stbp_flags)

#define stab_stab(stab)		(stab->str_magic->str_u.str_stab)
#define stab_estab(stab)	(((STBP*)(stab->str_ptr))->stbp_stab)

#define stab_name(stab)		(stab->str_magic->str_ptr)
#define stab_ename(stab)	stab_name(stab_estab(stab))

#define stab_stash(stab)	(stab->str_magic->str_u.str_stash)
#define stab_estash(stab)	stab_stash(stab_estab(stab))

#define SF_VMAGIC 1		/* call routine to dereference STR val */
#define SF_MULTI 2		/* seen more than once */

struct stio {
    FILE	*ifp;		/* ifp and ofp are normally the same */
    FILE	*ofp;		/* but sockets need separate streams */
#ifdef HAS_READDIR
    DIR		*dirp;		/* for opendir, readdir, etc */
#endif
    long	lines;		/* $. */
    long	page;		/* $% */
    long	page_len;	/* $= */
    long	lines_left;	/* $- */
    char	*top_name;	/* $^ */
    STAB	*top_stab;	/* $^ */
    char	*fmt_name;	/* $~ */
    STAB	*fmt_stab;	/* $~ */
    short	subprocess;	/* -| or |- */
    char	type;
    char	flags;
};

#define IOF_ARGV 1	/* this fp iterates over ARGV */
#define IOF_START 2	/* check for null ARGV and substitute '-' */
#define IOF_FLUSH 4	/* this fp wants a flush after write op */

struct sub {
    CMD		*cmd;
    int		(*usersub)();
    int		userindex;
    STAB	*filestab;
    long	depth;	/* >= 2 indicates recursive call */
    ARRAY	*tosave;
};

#define Nullstab Null(STAB*)

STRLEN stab_len();

#define STAB_STR(s) (tmpstab = (s), stab_flags(tmpstab) & SF_VMAGIC ? stab_str(stab_val(tmpstab)->str_magic) : stab_val(tmpstab))
#define STAB_LEN(s) (tmpstab = (s), stab_flags(tmpstab) & SF_VMAGIC ? stab_len(stab_val(tmpstab)->str_magic) : str_len(stab_val(tmpstab)))
#define STAB_GET(s) (tmpstab = (s), str_get(stab_flags(tmpstab) & SF_VMAGIC ? stab_str(tmpstab->str_magic) : stab_val(tmpstab)))
#define STAB_GNUM(s) (tmpstab = (s), str_gnum(stab_flags(tmpstab) & SF_VMAGIC ? stab_str(tmpstab->str_magic) : stab_val(tmpstab)))

EXT STAB *tmpstab;

EXT STAB *stab_index[128];

EXT unsigned short statusvalue;

EXT int delaymagic INIT(0);
#define DM_UID   0x003
#define DM_RUID   0x001
#define DM_EUID   0x002
#define DM_GID   0x030
#define DM_RGID   0x010
#define DM_EGID   0x020
#define DM_DELAY 0x100

STAB *aadd();
STAB *hadd();
STAB *fstab();
void stabset();
void stab_fullname();
void stab_efullname();
void stab_check();
