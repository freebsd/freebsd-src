/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: ctm.h,v 1.8 1996/02/05 16:06:47 phk Exp $
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <md5.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>

#define VERSION "2.0"
#define MAXSIZE (1024*1024*10)

#define SUBSUFF ".ctm"
#define TMPSUFF ".ctmtmp"

/* The fields... */
#define CTM_F_MASK		0xff
#define CTM_F_Name		0x01
#define CTM_F_Uid		0x02
#define CTM_F_Gid		0x03
#define CTM_F_Mode		0x04
#define CTM_F_MD5		0x05
#define CTM_F_Count		0x06
#define CTM_F_Bytes		0x07

/* The qualifiers... */
#define CTM_Q_MASK		0xff00
#define CTM_Q_Name_File		0x0100
#define CTM_Q_Name_Dir		0x0200
#define CTM_Q_Name_New		0x0400
#define CTM_Q_Name_Subst	0x0800
#define CTM_Q_MD5_After		0x0100
#define CTM_Q_MD5_Before	0x0200
#define CTM_Q_MD5_Chunk		0x0400
#define CTM_Q_MD5_Force		0x0800

struct CTM_Syntax {
    char	*Key;
    int		*List;
    };

extern struct CTM_Syntax Syntax[];

#define Malloc malloc
#define Free free
#define Delete(foo) if (!foo) ; else {Free(foo); foo = 0; }
#define String(foo) strdup(foo)

#ifndef EXTERN
#  define EXTERN extern
#endif
EXTERN u_char *Version;
EXTERN u_char *Name;
EXTERN u_char *Nbr;
EXTERN u_char *TimeStamp;
EXTERN u_char *Prefix;
EXTERN u_char *FileName;
EXTERN u_char *TmpDir;
EXTERN u_char *CatPtr;
EXTERN u_char *Buffer;

/*
 * Paranoid -- Just in case they should be after us...
 *  0 not at all.
 *  1 normal.
 *  2 somewhat.
 *  3 you bet!.
 *
 * Verbose -- What to tell mom...
 *  0 Nothing which wouldn't surprise.
 *  1 Normal.
 *  2 Show progress '.'.
 *  3 Show progress names, and actions.
 *  4 even more...
 *  and so on
 *
 * ExitCode -- our Epitaph
 *  0 Perfect, all input digested, no problems
 *  1 Bad input, no point in retrying.
 *  2 Pilot error, commandline problem &c
 *  4 Out of resources.
 *  8 Destination-tree not correct.
 * 16 Destination-tree not correct, can force.
 * 32 Internal problems.
 *
 */

EXTERN int Paranoid;
EXTERN int Verbose;
EXTERN int Exit;
EXTERN int Force;
EXTERN int CheckIt;
EXTERN int SetTime;
EXTERN struct timeval Times[2];

#define Exit_OK		0
#define Exit_Garbage	1
#define Exit_Pilot	2
#define Exit_Broke	4
#define Exit_NotOK	8
#define Exit_Forcible	16
#define Exit_Mess	32
#define Exit_Done	64
#define Exit_Version	128

void Fatal_(int ln, char *fn, char *kind);
#define Fatal(foo) Fatal_(__LINE__,__FILE__,foo)
#define Assert() Fatal_(__LINE__,__FILE__,"Assert failed.")
#define WRONG {Assert(); return Exit_Mess;}

u_char * Ffield(FILE *fd, MD5_CTX *ctx,u_char term);
u_char * Fname(FILE *fd, MD5_CTX *ctx,u_char term,int qual, int verbose);

int Fbytecnt(FILE *fd, MD5_CTX *ctx, u_char term);

u_char * Fdata(FILE *fd, int u_chars, MD5_CTX *ctx);

#define GETFIELD(p,q) if(!((p)=Ffield(fd,&ctx,(q)))) return BADREAD
#define GETFIELDCOPY(p,q) if(!((p)=Ffield(fd,&ctx,(q)))) return BADREAD; else p=String(p)
#define GETBYTECNT(p,q) if(0 >((p)= Fbytecnt(fd,&ctx,(q)))) return BADREAD
#define GETDATA(p,q) if(!((p) = Fdata(fd,(q),&ctx))) return BADREAD
#define GETNAMECOPY(p,q,r,v) if(!((p)=Fname(fd,&ctx,(q),(r),(v)))) return BADREAD; else p=String(p)

int Pass1(FILE *fd, unsigned applied);
int Pass2(FILE *fd);
int Pass3(FILE *fd);

int ctm_edit(u_char *script, int length, char *filein, char *fileout);
