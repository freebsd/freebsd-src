/*
 * Copyright (c) 1993 Atsushi Murai (amurai@spec.co.jp)
 * All rights reserved.
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
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Atsushi Murai(amurai@spec.co.jp)``AS IS'' AND
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
 *	$Id: isofs_rrip.c,v 1.6 1994/06/13 20:19:35 jkh Exp $
 */

#include "param.h"
#include "systm.h"
#include "namei.h"
#include "buf.h"
#include "file.h"
#include "vnode.h"
#include "mount.h"
#include "kernel.h"

#include "sys/time.h"

#include "iso.h"
#include "isofs_node.h"
#include "isofs_rrip.h"
#include "iso_rrip.h"

/*
 * POSIX file attribute
 */
static int isofs_rrip_attr( p, ana )
	ISO_RRIP_ATTR	 *p;
	ISO_RRIP_ANALYZE *ana;
{
	ana->inode.iso_mode  = isonum_731(p->mode_l);
	ana->inode.iso_uid   = (uid_t)isonum_731(p->uid_l);
	ana->inode.iso_gid   = (gid_t)isonum_731(p->gid_l);
/*	ana->inode.iso_links = isonum_731(p->links_l); */
	return 0;
}

int isofs_rrip_defattr(  isodir, ana )
	struct iso_directory_record 	*isodir;
	ISO_RRIP_ANALYZE 		*ana;
{
	ana->inode.iso_mode  = (VREAD|VEXEC|(VREAD|VEXEC)>>3|(VREAD|VEXEC)>>6);
	ana->inode.iso_uid   = (uid_t)0;
	ana->inode.iso_gid   = (gid_t)0;
	return 0;
}

/*
 * POSIX device modes
 */
static int isofs_rrip_device( p, ana )
	ISO_RRIP_DEVICE  *p;
	ISO_RRIP_ANALYZE *ana;
{
	char   buf[3];

	buf[0] = p->h.type[0];
	buf[1] = p->h.type[1];
	buf[2]	= 0x00;

	printf("isofs:%s[%d] high=0x%08x, low=0x%08x\n",
				buf,
				isonum_711(p->h.length),
				isonum_731(p->dev_t_high_l),
				isonum_731(p->dev_t_low_l)
					 );
	return 0;
}

/*
 * Symbolic Links
 */
static int isofs_rrip_slink( p, ana )
	ISO_RRIP_SLINK  *p;
	ISO_RRIP_ANALYZE *ana;
{
	return 0;
}

/*
 * Alternate name
 */
static int isofs_rrip_altname( p, ana )
	ISO_RRIP_ALTNAME *p;
	ISO_RRIP_ANALYZE *ana;
{
	return 0;
}

/*
 * Child Link
 */
static int isofs_rrip_clink( p, ana )
	ISO_RRIP_CLINK  *p;
	ISO_RRIP_ANALYZE *ana;
{
	char   buf[3];
	buf[0] = p->h.type[0];
	buf[1] = p->h.type[1];
	buf[2]	= 0x00;
	printf("isofs:%s[%d] loc=%d\n",
				buf,
				isonum_711(p->h.length),
				isonum_733(p->dir_loc)
							);
	ana->inode.iso_cln = isonum_733(p->dir_loc);
	return 0;
}

/*
 * Parent Link
 */
static int isofs_rrip_plink( p, ana )
ISO_RRIP_PLINK  *p;
ISO_RRIP_ANALYZE *ana;
{

	char   buf[3];
	buf[0] = p->h.type[0];
	buf[1] = p->h.type[1];
	buf[2]	= 0x00;
	printf("isofs:%s[%d] loc=%d\n",
				buf,
				isonum_711(p->h.length),
				isonum_733(p->dir_loc)
							);
	ana->inode.iso_pln = isonum_733(p->dir_loc);
	return 0;
}

/*
 * Relocated directory
 */
static int isofs_rrip_reldir( p, ana )
ISO_RRIP_RELDIR  *p;
ISO_RRIP_ANALYZE *ana;
{
	char   buf[3];

	buf[0] = p->h.type[0];
	buf[1] = p->h.type[1];
	buf[2]	= 0x00;

	printf("isofs:%s[%d]\n",buf, isonum_711(p->h.length) );
	return 0;
}

/*
 * Time stamp 
 */
static void isofs_rrip_tstamp_conv7(pi, pu)
char	         *pi;
struct timeval   *pu;
{
	int	 i;
	int	 crtime,days;
	int	 year,month,day,hour,minute,second,tz;

	year   = pi[0] - 70;
	month  = pi[1];
	day    = pi[2];
	hour   = pi[3];
	minute = pi[4];
	second = pi[5];
	tz     = pi[6];

	if (year < 0) {
		crtime = 0;
	} else {
		static int monlen[12] = {31,28,31,30,31,30,31,31,30,31,30,31};

		days = year * 365;
		if (year > 2)
			days += (year+2) / 4;
		for (i = 1; i < month; i++)
			days += monlen[i-1];
		if (((year+2) % 4) == 0 && month > 2)
			days++;
		days += day - 1;
		crtime = ((((days * 24) + hour) * 60 + minute) * 60)
			+ second;

		/* sign extend */
		if (tz & 0x80)
			tz |= (-1 << 8);

		/* timezone offset is unreliable on some disks */
		if (-48 <= tz && tz <= 52)
			crtime -= tz * 15 * 60;
	}
	pu->tv_sec  = crtime;
	pu->tv_usec = 0;
}


static unsigned isofs_chars2ui( begin, end )
unsigned char *begin;
unsigned char *end;
{
	unsigned rc=0;
	int 	 len; 
	int 	 wlen;
	static   int pow[]={ 1, 10, 100, 1000 };

	len = end - begin;
	wlen= len;
	for (; len >= 0; len -- ) {
		rc += ( *(begin+len) * pow[wlen - len] ); 
	}
	return( rc );
}
	
static void isofs_rrip_tstamp_conv17(pi, pu)
unsigned char *pi;
struct timeval   *pu;
{
	unsigned char	buf[7];

        /* year:"0001"-"9999" -> -1900  */
	buf[0]  =  (unsigned char)(isofs_chars2ui( &pi[0], &pi[3]) - 1900 );

	/* month: " 1"-"12"      -> 1 - 12 */
	buf[1]  =  (unsigned char)isofs_chars2ui( &pi[4], &pi[5]);

	/* day:   " 1"-"31"      -> 1 - 31 */
	buf[2]  =  isofs_chars2ui( &pi[6], &pi[7]);

  	/* hour:  " 0"-"23"      -> 0 - 23 */
	buf[3]  =  isofs_chars2ui( &pi[8], &pi[9]);

	/* minute:" 0"-"59"      -> 0 - 59 */
	buf[4]  =  isofs_chars2ui( &pi[10], &pi[11] );

	/* second:" 0"-"59"      -> 0 - 59 */
	buf[5]  =  isofs_chars2ui( &pi[12], &pi[13] );

	/* difference of GMT */
	buf[6]  =  pi[16];

	isofs_rrip_tstamp_conv7(buf, pu);
}

static int isofs_rrip_tstamp( p, ana )
ISO_RRIP_TSTAMP  *p;
ISO_RRIP_ANALYZE *ana;
{
	unsigned char *ptime;	

	ptime = p->time;

	/* Check a format of time stamp (7bytes/17bytes) */
	if ( !(*p->flags & ISO_SUSP_TSTAMP_FORM17 ) ) {
        		isofs_rrip_tstamp_conv7(ptime,    &ana->inode.iso_ctime );

			if ( *p->flags & ISO_SUSP_TSTAMP_MODIFY )
        			isofs_rrip_tstamp_conv7(ptime+7,    &ana->inode.iso_mtime );
			else
        			ana->inode.iso_mtime = ana->inode.iso_ctime;

			if ( *p->flags & ISO_SUSP_TSTAMP_ACCESS )
        			isofs_rrip_tstamp_conv7(ptime+14,   &ana->inode.iso_atime );
			else
        			ana->inode.iso_atime = ana->inode.iso_ctime;
	} else {
        		isofs_rrip_tstamp_conv17(ptime,    &ana->inode.iso_ctime );
			
			if ( *p->flags & ISO_SUSP_TSTAMP_MODIFY )
        			isofs_rrip_tstamp_conv17(ptime+17, &ana->inode.iso_mtime );
			else
        			ana->inode.iso_mtime = ana->inode.iso_ctime;

			if ( *p->flags & ISO_SUSP_TSTAMP_ACCESS )
        			isofs_rrip_tstamp_conv17(ptime+34, &ana->inode.iso_atime );
			else
        			ana->inode.iso_atime = ana->inode.iso_ctime;
	}
	return 0;
}

int isofs_rrip_deftstamp( isodir, ana )
	struct iso_directory_record  *isodir;
	ISO_RRIP_ANALYZE *ana;
{
       	isofs_rrip_tstamp_conv7(isodir->date, &ana->inode.iso_ctime );
	ana->inode.iso_atime = ana->inode.iso_ctime;
	ana->inode.iso_mtime = ana->inode.iso_ctime;
	return 0;
}


/*
 * Flag indicating
 *   Nothing to do....
 */
static int isofs_rrip_idflag( p, ana )
ISO_RRIP_IDFLAG  *p;
ISO_RRIP_ANALYZE *ana;
{
	char   buf[3];

	buf[0] = p->h.type[0];
	buf[1] = p->h.type[1];
	buf[2]	= 0x00;

	printf("isofs:%s[%d] idflag=0x%x\n",
				buf,
				isonum_711(p->h.length),
				p->flags );
	return 0;
}

/*
 * Extension reference
 *   Nothing to do....
 */
static int isofs_rrip_exflag( p, ana )
ISO_RRIP_EXFLAG  *p;
ISO_RRIP_ANALYZE *ana;
{
	char   buf[3];

	buf[0] = p->h.type[0];
	buf[1] = p->h.type[1];
	buf[2]	= 0x00;

	printf("isofs:%s[%d] exflag=0x%x",
				buf,
				isonum_711(p->h.length),
				p->flags );
	return 0;
}

/*
 * Unknown ...
 *   Nothing to do....
 */
static int isofs_rrip_unknown( p, ana )
	ISO_RRIP_EXFLAG  *p;
	ISO_RRIP_ANALYZE *ana;
{
	return 0;
}

typedef struct {
	char	 type[2];
	int	 (*func)();
	int	 (*func2)();
	int	 result;
} RRIP_TABLE;

static RRIP_TABLE rrip_table [] = {
             { 'P', 'X', isofs_rrip_attr,   isofs_rrip_defattr,   ISO_SUSP_ATTR    },
	     { 'T', 'F', isofs_rrip_tstamp, isofs_rrip_deftstamp, ISO_SUSP_TSTAMP  },
	     { 'N', 'M', isofs_rrip_altname,0,                    ISO_SUSP_ALTNAME },
	     { 'C', 'L', isofs_rrip_clink,  0,                    ISO_SUSP_CLINK   },
	     { 'P', 'L', isofs_rrip_plink,  0,                    ISO_SUSP_PLINK   },
	     { 'S', 'L', isofs_rrip_slink,  0,                    ISO_SUSP_SLINK   },
	     { 'R', 'E', isofs_rrip_reldir, 0,                    ISO_SUSP_RELDIR  },
	     { 'P', 'N', isofs_rrip_device, 0,                    ISO_SUSP_DEVICE  },
	     { 'R', 'R', isofs_rrip_idflag, 0,                    ISO_SUSP_IDFLAG  },
	     { 'E', 'R', isofs_rrip_exflag, 0,                    ISO_SUSP_EXFLAG  },
	     { 'S', 'P', isofs_rrip_unknown,0,                    ISO_SUSP_UNKNOWN },
	     { 'C', 'E', isofs_rrip_unknown,0,                    ISO_SUSP_UNKNOWN }
};

int isofs_rrip_analyze ( isodir, analyze )
struct iso_directory_record 	*isodir;
ISO_RRIP_ANALYZE		*analyze;
{	
	register RRIP_TABLE	 *ptable;
	register ISO_SUSP_HEADER *phead;
	register ISO_SUSP_HEADER *pend;
	int	found;
	int	i;
	char	*pwhead;
	int	result;

	/*
         * Note: If name length is odd,
	 *       it will be padding 1 byte  after the name
	 */
	pwhead 	=  isodir->name + isonum_711(isodir->name_len);
	if ( !(isonum_711(isodir->name_len) & 0x0001) )
		pwhead ++;
	phead 	= (ISO_SUSP_HEADER *)pwhead;
	pend 	= (ISO_SUSP_HEADER *)( (char *)isodir + isonum_711(isodir->length) );

	result = 0;
	if ( pend == phead ) {
		goto setdefault;
	}
	/*
	 * Note: "pend" should be more than one SUSP header
	 */ 
	while ( pend >= phead + 1) {
		found   = 0;
		for ( ptable=&rrip_table[0];ptable < &rrip_table[sizeof(rrip_table)/sizeof(RRIP_TABLE)]; ptable ++) {
			if ( bcmp( phead->type, ptable->type, 2 ) == 0 ) {
				found = 1;
				ptable->func( phead, analyze );
				result |= ptable->result;
				break;
			}
		}
		if ( found == 0 ) {
			printf("isofs: name '");
			for ( i =0; i < isonum_711(isodir->name_len) ;i++) {
				printf("%c", *(isodir->name + i) );
			}
			printf("'");
			printf(" - type %c%c [%08x/%08x]...not found\n",
				phead->type[0], phead->type[1], phead, pend );
			isofs_hexdump( phead, (int)( (char *)pend - (char *)phead ) );
		 	break;
		}

		/*
		 * move to next SUSP
		 */
		phead  = (ISO_SUSP_HEADER *) ((unsigned)isonum_711(phead->length) + (char *)phead);
	}

setdefault:
	/*
	 * If we don't find the Basic SUSP stuffs, just set default value
         *   ( attribute/time stamp )
	 */
	for ( ptable=&rrip_table[0];ptable < &rrip_table[2]; ptable ++) {
		if ( ptable->func2 != 0 && !(ptable->result & result) ) {
			ptable->func2( isodir, analyze );
		}
	}
	return ( result );
}

/* 
 * Get Alternate Name from 'AL' record 
 * If either no AL record nor 0 lenght, 
 *    it will be return the translated ISO9660 name,
 */
int	isofs_rrip_getname( isodir, outbuf, outlen )
	struct iso_directory_record *isodir;
	char *outbuf;
	int *outlen;
{
	ISO_SUSP_HEADER  *phead, *pend;
	ISO_RRIP_ALTNAME *p;
	char		 *pwhead;
	int		 found;

	/*
         * Note: If name length is odd,
	 *       it will be padding 1 byte  after the name
	 */
	pwhead 	=  isodir->name + isonum_711(isodir->name_len);
	if ( !(isonum_711(isodir->name_len) & 0x0001) )
		pwhead ++;
	phead 	= (ISO_SUSP_HEADER *)pwhead;
	pend 	= (ISO_SUSP_HEADER *)( (char *)isodir + isonum_711(isodir->length) );

	found   = 0;
	if ( pend != phead ) {
		while ( pend >= phead + 1) {
			if ( bcmp( phead->type, "NM", 2 ) == 0 ) {
				found = 1;
				break;
			}
			phead  = (ISO_SUSP_HEADER *) ((unsigned)isonum_711(phead->length) + (char *)phead);
		}
	}
	if ( found == 1 ) {
		p = (ISO_RRIP_ALTNAME *)phead;
		*outlen = isonum_711( p->h.length ) - sizeof( ISO_RRIP_ALTNAME );
		bcopy( (char *)( &p->flags + 1 ), outbuf, *outlen );
	} else {
		isofntrans(isodir->name, isonum_711(isodir->name_len), outbuf, outlen );
		if ( *outlen == 1) {
			switch ( outbuf[0] ) {
			case 0:
				outbuf[0] = '.';
				break;
			case 1:
				outbuf[0] = '.';
				outbuf[1] = '.';
				*outlen = 2;
			}
		}
	}
	return( found );
}

/* 
 * Get Symbolic Name from 'SL' record 
 *
 * Note: isodir should contains SL record!
 */
int	isofs_rrip_getsymname( vp, isodir, outbuf, outlen )
struct vnode			*vp;
struct iso_directory_record 	*isodir;
char				*outbuf;
int				*outlen;
{
	register ISO_RRIP_SLINK_COMPONENT *pcomp;
	register ISO_SUSP_HEADER  *phead, *pend;
	register ISO_RRIP_SLINK_COMPONENT *pcompe;
	ISO_RRIP_SLINK   *p;
	char		 *pwhead;
	int		 found;
	int		 slash;
	int 	  	 wlen;

	/*
         * Note: If name length is odd,
	 *       it will be padding 1 byte  after the name
	 */
	pwhead 	=  isodir->name + isonum_711(isodir->name_len);
	if ( !(isonum_711(isodir->name_len) & 0x0001) )
		pwhead ++;
	phead 	= (ISO_SUSP_HEADER *)pwhead;
	pend 	= (ISO_SUSP_HEADER *)( (char *)isodir + isonum_711(isodir->length) );

	found   = 0;
	if ( pend != phead ) {
		while ( pend >= phead + 1) {
			if ( bcmp( phead->type, "SL", 2 ) == 0 ) {
				found = 1;
				break;
			}
			phead  = (ISO_SUSP_HEADER *) ((unsigned)isonum_711(phead->length) + (char *)phead);
		}
	}
	if ( found == 0 ) {
		*outlen = 0;
		return( found );
	} 

	p	= (ISO_RRIP_SLINK *)phead;
	pcomp	= (ISO_RRIP_SLINK_COMPONENT *)p->component;
	pcompe  = (ISO_RRIP_SLINK_COMPONENT *)((char *)p + isonum_711(p->h.length));
	
	/*
         * Gathering a Symbolic name from each component with path
         */
	*outlen = 0; 
	slash   = 0;
	while ( pcomp < pcompe ) {

		/* Inserting Current */
		if ( pcomp->cflag[0] & ISO_SUSP_CFLAG_CURRENT ) {
			bcopy("./",	outbuf+*outlen, 2);
			*outlen += 2;
			slash   = 0;
		}

		/* Inserting Parent */
		if ( pcomp->cflag[0] & ISO_SUSP_CFLAG_PARENT ) {
			bcopy("../",	outbuf+*outlen, 3);
			*outlen += 3;
			slash   = 0;
		}

		/* Inserting slash for ROOT */
		if ( pcomp->cflag[0] & ISO_SUSP_CFLAG_ROOT ) {
			bcopy("/",	outbuf+*outlen, 1);
			*outlen += 1;
			slash   = 0;
		}

		/* Inserting a mount point i.e. "/cdrom" */
		if ( pcomp->cflag[0] & ISO_SUSP_CFLAG_VOLROOT ) {
                        wlen = strlen(vp->v_mount->mnt_stat.f_mntonname);
			bcopy(vp->v_mount->mnt_stat.f_mntonname,outbuf+*outlen, wlen);
			*outlen += wlen;
			slash   = 1;
		}

		/* Inserting hostname i.e. "tama:" */
		if ( pcomp->cflag[0] & ISO_SUSP_CFLAG_HOST ) {
			bcopy(hostname, outbuf+*outlen, hostnamelen);
			*(outbuf+hostnamelen) = ':';
			*outlen += (hostnamelen + 1);
			slash   = 0;	/* Uuum Should we insert a slash ? */
		}
	
		/* Inserting slash for next component */
		if ( slash == 1 ) {
			outbuf[*outlen] = '/';
			*outlen  += 1;
			slash   =  0;
		}

		/* Inserting component */
		wlen = isonum_711(pcomp->clen);
		if ( wlen != 0 ) {
			bcopy( pcomp->name, outbuf + *outlen, wlen );
			*outlen += wlen;
			slash   = 1;
		}
			
		/*
                 * Move to next component...
		 */	
		pcomp = (ISO_RRIP_SLINK_COMPONENT *)((char *)pcomp
			+ sizeof(ISO_RRIP_SLINK_COMPONENT) - 1 
			+ isonum_711(pcomp->clen));
	}
	return( found );
}
/* Hexdump routine for debug*/
int isofs_hexdump( p, size )
	unsigned char *p;
	int  size;
{	
	int i,j,k;
	unsigned char *wp;

	for ( i = 0; i < size; i += 16 ) {
		printf("isofs: ");
		wp = p;
		k = ( (size - i) > 16 ? 16 : size - i );
		for ( j =0; j < k; j ++ ){
			printf("%02x ", *p );
			p++;
		}
		printf(" : ");
		p = wp;
		for ( j =0; j < k; j ++ ){
			if ( (*p > 0x20) && (*p < 0x7f) ) 
				printf("%c", *p );
			else
				printf(" ");
			p++;
		}
		printf("\n");
	}
	printf("\n");
	return 0;
}
