/*
 *  Copyright (c) 1993 Steve Gerakines
 *
 *  This is freely redistributable software.  You may do anything you
 *  wish with it, so long as the above notice stays intact.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS
 *  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT,
 *  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 *  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 *  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 *  ftape.h - QIC-40/80 floppy tape driver functions
 *  10/30/93 v0.3
 *  Set up constant values.  Added support to get hardware info.
 *
 *  08/07/93 v0.2
 *  Header file that sits in /sys/sys, first revision.  Support for
 *  ioctl functions added.
 */

#ifndef	_FTAPE_H_
#define	_FTAPE_H_

#ifndef _IOCTL_H_
#include <sys/ioctl.h>
#endif

/* Miscellaneous constant values */
#define QCV_BLKSIZE	1024		/* Size of a block */
#define QCV_SEGSIZE	32768		/* Size of a segment */
#define QCV_BLKSEG	32		/* Blocks per segment */
#define QCV_ECCSIZE	3072		/* Bytes ecc eats */
#define QCV_ECCBLKS	3		/* Blocks ecc eats */
#define QCV_NFMT	3		/* Number of tape formats */
#define QCV_NLEN	5		/* Number of tape lengths */
#define QCV_HDRMAGIC	0xaa55aa55	/* Magic for header segment */
#define QCV_FSMAGIC	0x33cc33cc	/* Magic for fileset */

#define UCHAR		unsigned char
#define USHORT		unsigned short
#define ULONG		unsigned long

/* Segment request structure. */
typedef struct qic_segment {
	ULONG sg_trk;		/* Track number */
	ULONG sg_seg;		/* Segment number */
	ULONG sg_crcmap;	/* Returned bitmap of CRC errors */
	ULONG sg_badmap;	/* Map of known bad sectors */
	UCHAR *sg_data;		/* Segment w/bad blocks discarded */
} QIC_Segment;

/* Tape geometry structure. */
typedef struct qic_geom {
	int g_fmtno;			/* Format number */
	int g_lenno;			/* Length number */
	char g_fmtdesc[16];		/* Format text description */
	char g_lendesc[16];		/* Length text description */
	int g_trktape;			/* Number of tracks per tape */
	int g_segtrk;			/* Number of segments per track */
	int g_blktrk;			/* Number of blocks per track */
	int g_fdtrk;			/* Floppy disk tracks */
	int g_fdside;			/* Floppy disk sectors/side */
} QIC_Geom;

/* Tape hardware info */
typedef struct qic_hwinfo {
	int hw_make;			/* 10-bit drive make */
	int hw_model;			/* 6-bit model */
	int hw_rombeta;			/* TRUE if beta rom */
	int hw_romid;			/* 8-bit rom ID */
} QIC_HWInfo;

/* Various ioctl() routines. */
#define QIOREAD		_IOWR('q', 1, QIC_Segment)	/* Read segment     */
#define QIOWRITE	_IOW('q', 2, QIC_Segment)	/* Write segment    */
#define QIOREWIND	_IO('q', 3)			/* Rewind tape      */
#define QIOBOT		_IO('q', 4)			/* Seek beg of trk  */
#define QIOEOT		_IO('q', 5)			/* Seek end of trk  */
#define QIOTRACK	_IOW('q', 6, int)		/* Seek to track    */
#define QIOSEEKLP	_IO('q', 7)			/* Seek load point  */
#define QIOFORWARD	_IO('q', 8)			/* Move tape fwd    */
#define QIOSTOP		_IO('q', 9)			/* Stop tape	    */
#define QIOPRIMARY	_IO('q', 10)			/* Primary mode     */
#define QIOFORMAT	_IO('q', 11)			/* Format mode      */
#define QIOVERIFY	_IO('q', 12)			/* Verify mode      */
#define QIOWRREF	_IO('q', 13)			/* Write ref burst  */
#define QIOSTATUS	_IOR('q', 14, int)		/* Get drive status */
#define QIOCONFIG	_IOR('q', 15, int)		/* Get tape config  */
#define QIOGEOM		_IOR('q', 16, QIC_Geom)		/* Get geometry	    */
#define QIOHWINFO	_IOR('q', 17, QIC_HWInfo)	/* Get hardware inf */
#define QIOSENDHDR	_IOW('q', 18, QIC_Segment)	/* Send header      */
#define QIORECVHDR	_IOWR('q', 19, QIC_Segment)	/* Receive header   */

/* QIC drive status bits. */
#define QS_READY			0x01	/* Drive ready */
#define QS_ERROR			0x02	/* Error detected */
#define QS_CART				0x04	/* Tape in drive */
#define QS_RDONLY			0x08	/* Write protect */
#define QS_NEWCART			0x10	/* New tape inserted */
#define QS_FMTOK			0x20	/* Tape is formatted */
#define QS_BOT				0x40	/* Tape at beginning */
#define QS_EOT				0x80	/* Tape at end */

/* QIC configuration bits. */
#define QCF_RTMASK			0x18	/* Rate mask */
#define QCF_RT250			0x00	/* 250K bps */
#define QCF_RT2				0x01	/* 2M bps */
#define QCF_RT500			0x02	/* 500K bps */
#define QCF_RT1				0x03	/* 1M bps */
#define QCF_EXTRA			0x40	/* Extra length tape */
#define QCF_QIC80			0x80	/* QIC-80 detected */

/* QIC tape status bits. */
#define QTS_FMMASK			0x0f	/* Tape format mask */
#define QTS_LNMASK			0xf0	/* Tape length mask */
#define QTS_QIC40			0x01	/* QIC-40 tape */
#define QTS_QIC80			0x02	/* QIC-80 tape */
#define QTS_QIC500			0x03	/* QIC-500 tape */
#define QTS_LEN1			0x10	/* 205 ft/550 Oe */
#define QTS_LEN2			0x20	/* 307.5 ft/550 Oe */
#define QTS_LEN3			0x30	/* 295 ft/900 Oe */
#define QTS_LEN4			0x40	/* 1100 ft/550 Oe */
#define QTS_LEN5			0x50	/* 1100 ft/900 Oe */

/* Tape header segment structure */
typedef struct qic_header {
	ULONG qh_sig;		/* Header signature 0x55aa55aa */
	UCHAR qh_fmtc;		/* Format code */
	UCHAR qh_unused1;
	USHORT qh_hseg;		/* Header segment number */
	USHORT qh_dhseg;	/* Duplicate header segment number */
	USHORT qh_first;	/* First logical area data segment */
	USHORT qh_last;		/* Last logical area data segment */
	UCHAR qh_fmtdate[4];	/* Most recent format date */
	UCHAR qh_chgdate[4];	/* Most recent tape change date */
	UCHAR qh_unused2[2];
	USHORT qh_tstrk;	/* Tape segments per track */
	UCHAR qh_ttcart;	/* Tape tracks per cartridge */
	UCHAR qh_mfside;	/* Max floppy sides */
	UCHAR qh_mftrk;		/* Max floppy tracks */
	UCHAR qh_mfsect;	/* Max floppy sector */
	char qh_tname[44];	/* Tape name (ASCII, space filled) */
	UCHAR qh_namdate[4];	/* Date tape was given a name */
	USHORT qh_cprseg;	/* Compression map start segment */
	UCHAR qh_unused3[48];
	UCHAR qh_refmt;		/* Re-format flag */
	UCHAR qh_unused4;
	UCHAR qh_iocount[4];	/* I/O count for life of tape */
	UCHAR qh_unused5[4];
	UCHAR qh_ffmtdate[4];	/* Date first formatted */
	USHORT qh_fmtcount;	/* Number of times formatted */
	USHORT qh_badsect;	/* Failed sector count */
	char qh_mfname[44];	/* Manufacturer name if pre-formatted */
	char qh_mflot[44];	/* Manufacturer lot code */
	UCHAR qh_unused6[22];
	ULONG qh_fail[448];	/* Failed sector log */
	ULONG qh_badmap[6912];	/* Bad sector map */
} QIC_Header;

/* Volume table of contents entry structure. */
typedef struct qic_vtbl {
	UCHAR vt_sig[4];	/* Signature "VTBL" if entry used */
	USHORT vt_first;	/* Starting segment */
	USHORT vt_last;		/* Ending segment */
	char vt_vname[44];	/* Set name */
	UCHAR vt_savdate[4];	/* Date saved */
	UCHAR vt_flags;		/* Volume flags */
	UCHAR vt_multi;		/* Multi cartidge sequence no. */
	UCHAR vt_vext[26];	/* Extension data */
	char vt_passwd[8];	/* Password for volume */
	UCHAR vt_dirsize[4];	/* Directory section size */
	UCHAR vt_dtasize[4];	/* Data section size */
	USHORT vt_osver;	/* Operating System version */
	char vt_label[16];	/* Source drive label */
	UCHAR vt_ldev;		/* Logical device origin */
	UCHAR vt_pdev;		/* Physical device origin */
	UCHAR vt_cprtype;	/* Compression type */
	UCHAR vt_ostype;	/* Operating System type */
	UCHAR vt_ostype2;	/* Always zero ?? */
	UCHAR vt_isocpr;	/* ISO compression type */
	UCHAR vt_unused1[4];
} QIC_VTbl;

/* Data compression map structure. */
typedef struct qic_dcmap {
	UCHAR dc_sig[4];	/* Siguature "DCMS" */
	USHORT dc_mlen;		/* Total map length */
	UCHAR dc_unused1[6];
	ULONG dc_offset[7421];	/* Byte offsets to segments */
} QIC_DCMap;

/* System specific file set structures - Unix */
typedef struct qic_unix_set {
	UCHAR fsu_perm;		/* Permissions */
	UCHAR fsu_attr2;	/* More attributes */
	UCHAR fsu_ctime[4];	/* Creation time */
	UCHAR fsu_atime[4];	/* Last access time */
	UCHAR fsu_inode[4];	/* i-node number */
	UCHAR fsu_user[4];	/* User number */
	UCHAR fsu_group[4];	/* Group number */
	UCHAR fsu_major;	/* Major device number */
	UCHAR fsu_minor;	/* Minor device number */
	UCHAR fsu_nsize;	/* Name size */
	UCHAR fsu_name;		/* Entry name starts here */
} QIC_Unix_Set;

/* File set structure */
typedef struct qic_fileset {
	UCHAR fs_size;		/* Size of fixed + system size - 1 */
	UCHAR fs_attr;		/* Attributes */
	UCHAR fs_mtime;		/* Modification time */
	UCHAR fs_dsize[4];	/* Data size */
} QIC_FileSet;

#endif	/* _FTAPE_H_ */
