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
 *	BSDI dos.h,v 2.2 1996/04/08 19:32:28 bostic Exp
 *
 * $Id: dos.h,v 1.7 1996/09/23 09:59:24 miff Exp $
 */

/*
 * DOS Error codes
 */
/* MS-DOS version 2 error codes */
#define FUNC_NUM_IVALID		0x01
#define FILE_NOT_FOUND		0x02
#define PATH_NOT_FOUND		0x03
#define TOO_MANY_OPEN_FILES	0x04
#define ACCESS_DENIED		0x05
#define HANDLE_INVALID		0x06
#define MEM_CB_DEST		0x07
#define INSUF_MEM		0x08
#define MEM_BLK_ADDR_IVALID	0x09
#define ENV_INVALID		0x0a
#define FORMAT_INVALID		0x0b
#define ACCESS_CODE_INVALID	0x0c
#define DATA_INVALID		0x0d
#define UNKNOWN_UNIT		0x0e
#define DISK_DRIVE_INVALID	0x0f
#define ATT_REM_CUR_DIR		0x10
#define NOT_SAME_DEV		0x11
#define NO_MORE_FILES		0x12
/* mappings to critical-error codes */
#define WRITE_PROT_DISK		0x13
#define UNKNOWN_UNIT_CERR	0x14
#define DRIVE_NOT_READY		0x15
#define UNKNOWN_COMMAND		0x16
#define DATA_ERROR_CRC		0x17
#define BAD_REQ_STRUCT_LEN	0x18
#define SEEK_ERROR		0x19
#define UNKNOWN_MEDIA_TYPE	0x1a
#define SECTOR_NOT_FOUND	0x1b
#define PRINTER_OUT_OF_PAPER	0x1c
#define WRITE_FAULT		0x1d
#define READ_FAULT		0x1e
#define GENERAL_FAILURE		0x1f

/* MS-DOS version 3 and later extended error codes */
#define SHARING_VIOLATION	0x20
#define FILE_LOCK_VIOLATION	0x21
#define DISK_CHANGE_INVALID	0x22
#define FCB_UNAVAILABLE		0x23
#define SHARING_BUF_EXCEEDED	0x24

#define NETWORK_NAME_NOT_FOUND	0x35

#define FILE_ALREADY_EXISTS	0x50

#define DUPLICATE_REDIR		0x55

/*
 * dos attribute byte flags
 */
#define REGULAR_FILE    0x00    
#define READ_ONLY_FILE  0x01    
#define HIDDEN_FILE     0x02    
#define SYSTEM_FILE     0x04    
#define VOLUME_LABEL    0x08    
#define DIRECTORY       0x10    
#define ARCHIVE_NEEDED  0x20    

/*
 * Internal structure used for get_space()
 */
typedef struct {
    long	bytes_sector;
    long	sectors_cluster;
    long	total_clusters;
    long	avail_clusters;
} fsstat_t;

/*
 * Several DOS structures used by the file redirector
 */

typedef struct {
    DIR         *dp;
    u_char      *searchend;
    u_char      searchdir[1024];
} search_t;

/*
 * This is really the format of the DTA.  The file redirector will only
 * use the first 21 bytes.
 */
typedef struct {
    u_char	drive		__attribute__ ((packed));
    u_char      pattern[11]	__attribute__ ((packed));
    u_char      flag		__attribute__ ((packed));
    u_char      reserved1[4]	__attribute__ ((packed));
    search_t    *searchptr	__attribute__ ((packed));
    u_char      attr		__attribute__ ((packed));
    u_short     time		__attribute__ ((packed));
    u_short     date		__attribute__ ((packed));
    u_long	size		__attribute__ ((packed));
    u_char      name[13]	__attribute__ ((packed));
}/*  __attribute__((__packed__))*/ find_block_t;

/*
 * DOS directory entry structure
 */
typedef struct {
    u_char      name[8]		__attribute__ ((packed));
    u_char      ext[3]		__attribute__ ((packed));
    u_char      attr		__attribute__ ((packed));
    u_char      reserved[10]	__attribute__ ((packed));
    u_short     time		__attribute__ ((packed));
    u_short     date		__attribute__ ((packed));
    u_short     start		__attribute__ ((packed));
    u_long      size		__attribute__ ((packed));
} dosdir_t;

/*
 * The Current Drive Structure
 */
typedef struct {
	u_char	path[0x43]		__attribute__ ((packed));
	u_short	flag			__attribute__ ((packed));
	u_short	dpb_off			__attribute__ ((packed));
	u_short	dpb_seg			__attribute__ ((packed));
	u_short	redirector_off		__attribute__ ((packed));
	u_short	redirector_seg		__attribute__ ((packed));
	u_char	paramter_int21[2]	__attribute__ ((packed));
        u_short	offset			__attribute__ ((packed));
	u_char	dummy			__attribute__ ((packed));
	u_char	ifs_driver[4]		__attribute__ ((packed));
	u_char	dummy2[2]		__attribute__ ((packed));
}/* __attribute__((__packed__))*/ CDS;

#define	CDS_remote	0x8000
#define	CDS_ready	0x4000
#define	CDS_joined	0x2000
#define	CDS_substed	0x1000

#define	CDS_notnet	0x0080

/* 
 * The List of Lists (used to get the CDS and a few other numbers)
 */
typedef struct {
	u_char	dummy1[0x16]	__attribute__ ((packed));
	u_short	cds_offset	__attribute__ ((packed));
	u_short	cds_seg		__attribute__ ((packed));
	u_char  dummy2[6]	__attribute__ ((packed));
	u_char	numberbdev	__attribute__ ((packed));
	u_char	lastdrive	__attribute__ ((packed));
} LOL;

/*
 * The System File Table
 */
typedef struct {
/*00*/	u_short	nfiles		__attribute__ ((packed));	/* Number file handles referring to this file */
/*02*/	u_short	open_mode	__attribute__ ((packed));	/* Open mode (bit 15 -> by FCB) */
/*04*/	u_char	attribute	__attribute__ ((packed));
/*05*/	u_short	info		__attribute__ ((packed));	/* 15 -> remote, 14 ->  dont set date */
/*07*/	u_char	ddr_dpb[4]	__attribute__ ((packed));	/* Device Driver Header/Drive Paramter Block */
/*0b*/	u_short	fd		__attribute__ ((packed));
/*0d*/	u_short	time		__attribute__ ((packed));
/*0f*/	u_short	date		__attribute__ ((packed));
/*11*/	u_long	size		__attribute__ ((packed));
/*15*/	u_long	offset		__attribute__ ((packed));
/*19*/	u_short	rel_cluster	__attribute__ ((packed));
/*1b*/	u_short	abs_cluster	__attribute__ ((packed));
/*1d*/	u_char	dir_sector[2]	__attribute__ ((packed));
/*1f*/	u_char	dir_entry	__attribute__ ((packed));
/*20*/	u_char	name[8]		__attribute__ ((packed));
/*28*/	u_char	ext[3]		__attribute__ ((packed));
/*2b*/	u_char	sharesft[4]	__attribute__ ((packed));
/*2f*/	u_char	sharenet[2]	__attribute__ ((packed));
/*31*/	u_short	psp		__attribute__ ((packed));
/*33*/	u_char	share_off[2]	__attribute__ ((packed));
/*35*/	u_char	local_end[2]	__attribute__ ((packed));
/*37*/	u_char	ifd_driver[4]	__attribute__ ((packed)); 
} /*__attribute__((__packed__))*/ SFT;

/*
 * Format of PCDOS 4.01 swappable data area
 * (Sorry, but you need a wide screen to make this look nice)
 */
typedef struct {
    u_char	err_crit	__attribute__ ((packed));	/*   00h    BYTE    critical error flag */
    u_char	InDOS		__attribute__ ((packed));	/*   01h    BYTE    InDOS flag (count of active INT 21 calls) */
    u_char	err_drive	__attribute__ ((packed));	/*   02h    BYTE    ??? drive number or FFh */
    u_char	err_locus	__attribute__ ((packed));	/*   03h    BYTE    locus of last error */
    u_short	err_code	__attribute__ ((packed));	/*   04h    WORD    extended error code of last error */
    u_char	err_suggest	__attribute__ ((packed));	/*   06h    BYTE    suggested action for last error */
    u_char	err_class	__attribute__ ((packed));	/*   07h    BYTE    class of last error */
    u_short	err_di		__attribute__ ((packed));
    u_short	err_es		__attribute__ ((packed));	/*   08h    DWORD   ES:DI pointer for last error */
    u_short	dta_off		__attribute__ ((packed));
    u_short	dta_seg		__attribute__ ((packed));	/*   0Ch    DWORD   current DTA */
    u_short	psp		__attribute__ ((packed));	/*   10h    WORD    current PSP */
    u_short	int_23_sp	__attribute__ ((packed));	/*   12h    WORD    stores SP across an INT 23 */
    u_short	wait_status	__attribute__ ((packed));	/*   14h    WORD    return code from last process termination (zerod after reading with AH=4Dh) */
    u_char	current_drive	__attribute__ ((packed));	/*   16h    BYTE    current drive */
    u_char	break_flag	__attribute__ ((packed));	/*   17h    BYTE    extended break flag */
    u_char	unknown1[2]	__attribute__ ((packed));	/*   18h  2 BYTEs   ??? */
    u_short	int_21_ax	__attribute__ ((packed));	/*   1Ah    WORD    value of AX on call to INT 21 */
    u_short	net_psp		__attribute__ ((packed));	/*   1Ch    WORD    PSP segment for sharing/network */
    u_short	net_number	__attribute__ ((packed));	/*   1Eh    WORD    network machine number for sharing/network (0000h = us) */
    u_short	first_mem	__attribute__ ((packed));	/*   20h    WORD    first usable memory block found when allocating memory */
    u_short	best_mem	__attribute__ ((packed));	/*   22h    WORD    best usable memory block found when allocating memory */
    u_short	last_mem	__attribute__ ((packed));	/*   24h    WORD    last usable memory block found when allocating memory */
    u_char	unknown[10]	__attribute__ ((packed));	/*   26h  2 BYTEs   ??? (don't seem to be referenced) */
    u_char	monthday	__attribute__ ((packed));	/*   30h    BYTE    day of month */
    u_char	month		__attribute__ ((packed));	/*   31h    BYTE    month */
    u_short	year		__attribute__ ((packed));	/*   32h    WORD    year - 1980 */
    u_short	days		__attribute__ ((packed));	/*   34h    WORD    number of days since 1-1-1980 */
    u_char	weekday		__attribute__ ((packed));	/*   36h    BYTE    day of week (0 = Sunday) */
    u_char	unknown2[3]	__attribute__ ((packed));	/*   37h    BYTE    ??? */
    u_char	ddr_head[30]	__attribute__ ((packed));	/*   38h 30 BYTEs   device driver request header */
    u_short	ddre_ip		__attribute__ ((packed));
    u_short	ddre_cs		__attribute__ ((packed));	/*   58h    DWORD   pointer to device driver entry point (used in calling driver) */
    u_char	ddr_head2[22]	__attribute__ ((packed));	/*   5Ch 22 BYTEs   device driver request header */
    u_char	ddr_head3[30]	__attribute__ ((packed));	/*   72h 30 BYTEs   device driver request header */
    u_char	unknown3[6]	__attribute__ ((packed));	/*   90h  6 BYTEs   ??? */
    u_char	clock_xfer[6]	__attribute__ ((packed));	/*   96h  6 BYTEs   CLOCK$ transfer record (see AH=52h) */
    u_char	unknown4[2]	__attribute__ ((packed));	/*   9Ch  2 BYTEs   ??? */
    u_char	filename1[128]	__attribute__ ((packed));	/*   9Eh 128 BYTEs  buffer for filename */
    u_char	filename2[128]	__attribute__ ((packed));	/*  11Eh 128 BYTEs  buffer for filename */
    u_char	findfirst[21]	__attribute__ ((packed));	/*  19Eh 21 BYTEs   findfirst/findnext search data block (see AH=4Eh) */
    u_char	foundentry[32]	__attribute__ ((packed));	/*  1B3h 32 BYTEs   directory entry for found file */
    u_char	cds[88]		__attribute__ ((packed));	/*  1D3h 88 BYTEs   copy of current directory structure for drive being accessed */
    u_char	fcbname[11]	__attribute__ ((packed));	/*  22Bh 11 BYTEs   ??? FCB-format filename */
    u_char	unknown5	__attribute__ ((packed));	/*  236h    BYTE    ??? */
    u_char	wildcard[11]	__attribute__ ((packed));	/*  237h 11 BYTEs   wildcard destination specification for rename (FCB format) */
    u_char	unknown6[11]	__attribute__ ((packed));	/*  242h  2 BYTEs   ??? */
    u_char	attrmask	__attribute__ ((packed));	/*  24Dh    BYTE    attribute mask for directory search??? */
    u_char	open_mode	__attribute__ ((packed));	/*  24Eh    BYTE    open mode */
    u_char	unknown7[3]	__attribute__ ((packed));	/*  24fh    BYTE    ??? */
    u_char	virtual_dos	__attribute__ ((packed));	/*  252h    BYTE    flag indicating how DOS function was invoked (00h = direct INT 20/INT 21, FFh = server call AX=5D00h) */
    u_char	unknown8[9]	__attribute__ ((packed));	/*  253h    BYTE    ??? */
    u_char	term_type	__attribute__ ((packed));	/*  25Ch    BYTE    type of process termination (00h-03h) */
    u_char	unknown9[3]	__attribute__ ((packed));	/*  25Dh    BYTE    ??? */
    u_short	dpb_off		__attribute__ ((packed));
    u_short	dpb_seg		__attribute__ ((packed));	/*  260h    DWORD   pointer to Drive Parameter Block for critical error invocation */
    u_short	int21_sf_off	__attribute__ ((packed));
    u_short	int21_sf_seg	__attribute__ ((packed));	/*  264h    DWORD   pointer to stack frame containing user registers on INT 21 */
    u_short	store_sp	__attribute__ ((packed));	/*  268h    WORD    stores SP??? */
    u_short	dosdpb_off	__attribute__ ((packed));
    u_short	dosdpb_seg	__attribute__ ((packed));	/*  26Ah    DWORD   pointer to DOS Drive Parameter Block for ??? */
    u_short	disk_buf_seg	__attribute__ ((packed));	/*  26Eh    WORD    segment of disk buffer */
    u_short	unknown10[4]	__attribute__ ((packed));	/*  270h    WORD    ??? */
    u_char	media_id	__attribute__ ((packed));	/*  278h    BYTE    Media ID byte returned by AH=1Bh,1Ch */
    u_char	unknown11	__attribute__ ((packed));	/*  279h    BYTE    ??? (doesn't seem to be referenced) */
    u_short	unknown12[2]	__attribute__ ((packed));	/*  27Ah    DWORD   pointer to ??? */
    u_short	sft_off		__attribute__ ((packed));
    u_short	sft_seg		__attribute__ ((packed));	/*  27Eh    DWORD   pointer to current SFT */
    u_short	cds_off		__attribute__ ((packed));
    u_short	cds_seg		__attribute__ ((packed));	/*  282h    DWORD   pointer to current directory structure for drive being accessed */
    u_short	fcb_off		__attribute__ ((packed));
    u_short	fcb_seg		__attribute__ ((packed));	/*  286h    DWORD   pointer to caller's FCB */
    u_short	unknown13[2]	__attribute__ ((packed));	/*  28Ah    WORD    ??? */
    u_short	jft_off		__attribute__ ((packed));
    u_short	jft_seg		__attribute__ ((packed));	/*  28Eh    DWORD   pointer to a JFT entry in process handle table (see AH=26h) */
    u_short	filename1_off	__attribute__ ((packed));	/*  292h    WORD    offset in DOS CS of first filename argument */
    u_short	filename2_off	__attribute__ ((packed));	/*  294h    WORD    offset in DOS CS of second filename argument */
    u_short	unknown14[12]	__attribute__ ((packed));	/*  296h    WORD    ??? */
    u_short	file_offset_lo	__attribute__ ((packed));
    u_short	file_offset_hi	__attribute__ ((packed));	/*  2AEh    DWORD   offset in file??? */
    u_short	unknown15	__attribute__ ((packed));	/*  2B2h    WORD    ??? */
    u_short	partial_bytes	__attribute__ ((packed));	/*  2B4h    WORD    bytes in partial sector */
    u_short	number_sectors	__attribute__ ((packed));	/*  2B6h    WORD    number of sectors */
    u_short	unknown16[3]	__attribute__ ((packed));	/*  2B8h    WORD    ??? */
    u_short	nbytes_lo	__attribute__ ((packed));
    u_short	nbytes_hi	__attribute__ ((packed));	/*  2BEh    DWORD   number of bytes appended to file */
    u_short	qpdb_off	__attribute__ ((packed));
    u_short	qpdb_seg	__attribute__ ((packed));	/*  2C2h    DWORD   pointer to ??? disk buffer */
    u_short	asft_off	__attribute__ ((packed));
    u_short	asft_seg	__attribute__ ((packed));	/*  2C6h    DWORD   pointer to ??? SFT */
    u_short	int21_bx	__attribute__ ((packed));	/*  2CAh    WORD    used by INT 21 dispatcher to store caller's BX */
    u_short	int21_ds	__attribute__ ((packed));	/*  2CCh    WORD    used by INT 21 dispatcher to store caller's DS */
    u_short	temporary	__attribute__ ((packed));	/*  2CEh    WORD    temporary storage while saving/restoring caller's registers */
    u_short	prevcall_off	__attribute__ ((packed));
    u_short	prevcall_seg	__attribute__ ((packed));	/*  2D0h    DWORD   pointer to prev call frame (offset 264h) if INT 21 reentered also switched to for duration of INT 24 */
    u_char	unknown17[9]	__attribute__ ((packed));	/*  2D4h    WORD    ??? */
    u_short	ext_action	__attribute__ ((packed));	/*  2DDh    WORD    multipurpose open action */
    u_short	ext_attr	__attribute__ ((packed));	/*  2DFh    WORD    multipurpose attribute */
    u_short	ext_mode	__attribute__ ((packed));	/*  2E1h    WORD    multipurpose mode */
    u_char	unknown17a[9]	__attribute__ ((packed));
    u_short	lol_ds		__attribute__ ((packed));	/*  2ECh    WORD    stores DS during call to [List-of-Lists + 37h] */
    u_char	unknown18[5]	__attribute__ ((packed));	/*  2EEh    WORD    ??? */
    u_char	usernameptr[4]	__attribute__ ((packed));	/*  2F3h    DWORD   pointer to user-supplied filename */
    u_char	unknown19[4]	__attribute__ ((packed));	/*  2F7h    DWORD   pointer to ??? */
    u_char	lol_ss[2]	__attribute__ ((packed));	/*  2FBh    WORD    stores SS during call to [List-of-Lists + 37h] */
    u_char	lol_sp[2]	__attribute__ ((packed));	/*  2FDh    WORD    stores SP during call to [List-of-Lists + 37h] */
    u_char	lol_flag	__attribute__ ((packed));	/*  2FFh    BYTE    flag, nonzero if stack switched in calling [List-of-Lists+37h] */
    u_char	searchdata[21]	__attribute__ ((packed));	/*  300h 21 BYTEs   FindFirst search data for source file(s) of a rename operation (see AH=4Eh) */
    u_char	renameentry[32]	__attribute__ ((packed));	/*  315h 32 BYTEs   directory entry for file being renamed */
    u_char	errstack[331]	__attribute__ ((packed));	/*  335h 331 BYTEs  critical error stack */
    u_char	diskstack[384]	__attribute__ ((packed));	/*  480h 384 BYTEs  disk stack (functions greater than 0Ch, INT 25, INT 26) */
    u_char	iostack[384]	__attribute__ ((packed));	/*  600h 384 BYTEs  character I/O stack (functions 01h through 0Ch) */
    u_char	int_21_08_flag	__attribute__ ((packed));	/*  780h    BYTE    flag affecting AH=08h (see AH=64h) */
    u_char	unknown20[11]	__attribute__ ((packed));	/*  781h    BYTE    ??? looks like a drive number */
} /*__attribute__((__packed__))*/ SDA;

struct exehdr {
	u_short magic;
	u_short bytes_on_last_page;
	u_short size; /* 512 byte blocks */
	u_short nreloc;
	u_short hdr_size; /* paragraphs */
	u_short min_memory; /* paragraphs */
	u_short max_memory; /* pargraphs */
	u_short init_ss;
	u_short init_sp;
	u_short checksum;
	u_short init_ip;
	u_short init_cs;
	u_short reloc_offset;
	u_short overlay_num;
};

struct reloc_entry {
	u_short off;
	u_short seg;
};


/*
** DOS-related shrapnel
*/

static inline int
from_dos_attr(int attr)
{
    return((attr & READ_ONLY_FILE) ? 0444 : 0666);
}

static inline int
to_dos_attr(int mode)
{
    int		attr;

    attr = (mode & 0200) ? 0:READ_ONLY_FILE;
    attr |= S_ISDIR(mode) ? DIRECTORY:0;
    return(attr);
}

/* prototypes */

extern char		*dos_return[];	/* names of DOS return codes */
extern const int	dos_ret_size;	/* length of above */
extern char		*InDOS;
extern int		diskdrive;	/* current drive */
unsigned long		disk_transfer_addr;

extern void		encode_dos_file_time (time_t, u_short *, u_short *);
extern time_t		decode_dos_file_time(u_short dosdate, u_short dostime);
extern int		translate_filename(u_char *dname, u_char *uname, int *drivep);
extern int		parse_filename(int flag, char *str, char *fcb, int *nb);
extern void		dos_init(void);

/* from exe.c */
extern int	pspseg;			/* segment # of PSP */
extern int	curpsp;

extern void	exec_command(regcontext_t *REGS, int run, int fd, char *cmdname, u_short *param);
extern void	load_overlay(int fd, int start_segment, int reloc_segment);
extern void 	load_command(regcontext_t *REGS, int run, int fd, char *cmdname, 
			     u_short *param, char **argv, char **envs);
extern void	exec_return(regcontext_t *REGS, int code);
extern int	get_env(void);

/* from setver.c */
extern void	setver(char *cmd, short version);
extern short	getver(char *cmd);
