/*
 *
 *	$OBJRECDEF
 *	Generated automatically by "vms_struct Version 1.00"
 *	Created from VMS definition file "objrecdef.mar"
 *	Mon Oct 14 14:01:29 1985
 *
 */
struct OBJREC {
	unsigned char	obj$b_rectyp;
	unsigned char	obj$b_subtyp;
	unsigned char	obj$b_mhd_strlv;
	unsigned char	obj$b_mhd_recsz[2];
	unsigned char	obj$t_mhd_name[1];
	};

#define	OBJ$C_HDR	0
#define	OBJ$C_HDR_MHD	0
#define	OBJ$C_HDR_LNM	1
#define	OBJ$C_HDR_SRC	2
#define	OBJ$C_HDR_TTL	3
#define	OBJ$C_HDR_CPR	4
#define	OBJ$C_HDR_MTC	5
#define	OBJ$C_HDR_GTX	6
#define	OBJ$C_GSD	1
#define	OBJ$C_GSD_PSC	0
#define	OBJ$C_GSD_SYM	1
#define	OBJ$C_GSD_EPM	2
#define	OBJ$C_GSD_PRO	3
#define	OBJ$C_GSD_SYMW	4
#define	OBJ$C_GSD_EPMW	5
#define	OBJ$C_GSD_PROW	6
#define	OBJ$C_GSD_IDC	7
#define	OBJ$C_GSD_ENV	8
#define	OBJ$C_GSD_LSY	9
#define	OBJ$C_GSD_LEPM	10
#define	OBJ$C_GSD_LPRO	11
#define	OBJ$C_GSD_SPSC	12
#define	OBJ$C_TIR	2
#define	OBJ$C_EOM	3
#define	OBJ$C_DBG	4
#define	OBJ$C_TBT	5
#define	OBJ$C_LNK	6
#define	OBJ$C_EOMW	7
#define	OBJ$C_MAXRECTYP	7
#define	OBJ$K_SUBTYP	1
#define	OBJ$C_SUBTYP	1
#define	OBJ$C_MAXRECSIZ	2048
#define	OBJ$C_STRLVL	0
#define	OBJ$C_SYMSIZ	31
#define	OBJ$C_STOREPLIM	-1
#define	OBJ$C_PSCALILIM	9

#define	MHD$C_MHD	0
#define	MHD$C_LNM	1
#define	MHD$C_SRC	2
#define	MHD$C_TTL	3
#define	MHD$C_CPR	4
#define	MHD$C_MTC	5
#define	MHD$C_GTX	6
#define	MHD$C_MAXHDRTYP	6

#define	GSD$K_ENTRIES	1
#define	GSD$C_ENTRIES	1
#define	GSD$C_PSC	0
#define	GSD$C_SYM	1
#define	GSD$C_EPM	2
#define	GSD$C_PRO	3
#define	GSD$C_SYMW	4
#define	GSD$C_EPMW	5
#define	GSD$C_PROW	6
#define	GSD$C_IDC	7
#define	GSD$C_ENV	8
#define	GSD$C_LSY	9
#define	GSD$C_LEPM	10
#define	GSD$C_LPRO	11
#define	GSD$C_SPSC	12
#define	GSD$C_SYMV	13
#define	GSD$C_EPMV	14
#define	GSD$C_PROV	15
#define	GSD$C_MAXRECTYP	15

#define	GSY$M_WEAK	1
#define	GSY$M_DEF	2
#define	GSY$M_UNI	4
#define	GSY$M_REL	8

#define	GPS$M_PIC	1
#define	GPS$M_LIB	2
#define	GPS$M_OVR	4
#define	GPS$M_REL	8
#define	GPS$M_GBL	16
#define	GPS$M_SHR	32
#define	GPS$M_EXE	64
#define	GPS$M_RD	128
#define	GPS$M_WRT	256
#define	GPS$M_VEC	512
#define	GPS$K_NAME	9
#define	GPS$C_NAME	9

#define	TIR$C_STA_GBL	0
#define	TIR$C_STA_SB	1
#define	TIR$C_STA_SW	2
#define	TIR$C_STA_LW	3
#define	TIR$C_STA_PB	4
#define	TIR$C_STA_PW	5
#define	TIR$C_STA_PL	6
#define	TIR$C_STA_UB	7
#define	TIR$C_STA_UW	8
#define	TIR$C_STA_BFI	9
#define	TIR$C_STA_WFI	10
#define	TIR$C_STA_LFI	11
#define	TIR$C_STA_EPM	12
#define	TIR$C_STA_CKARG	13
#define	TIR$C_STA_WPB	14
#define	TIR$C_STA_WPW	15
#define	TIR$C_STA_WPL	16
#define	TIR$C_STA_LSY	17
#define	TIR$C_STA_LIT	18
#define	TIR$C_STA_LEPM	19
#define	TIR$C_MAXSTACOD	19
#define	TIR$C_MINSTOCOD	20
#define	TIR$C_STO_SB	20
#define	TIR$C_STO_SW	21
#define	TIR$C_STO_L	22
#define	TIR$C_STO_BD	23
#define	TIR$C_STO_WD	24
#define	TIR$C_STO_LD	25
#define	TIR$C_STO_LI	26
#define	TIR$C_STO_PIDR	27
#define	TIR$C_STO_PICR	28
#define	TIR$C_STO_RSB	29
#define	TIR$C_STO_RSW	30
#define	TIR$C_STO_RL	31
#define	TIR$C_STO_VPS	32
#define	TIR$C_STO_USB	33
#define	TIR$C_STO_USW	34
#define	TIR$C_STO_RUB	35
#define	TIR$C_STO_RUW	36
#define	TIR$C_STO_B	37
#define	TIR$C_STO_W	38
#define	TIR$C_STO_RB	39
#define	TIR$C_STO_RW	40
#define	TIR$C_STO_RIVB	41
#define	TIR$C_STO_PIRR	42
#define	TIR$C_MAXSTOCOD	42
#define	TIR$C_MINOPRCOD	50
#define	TIR$C_OPR_NOP	50
#define	TIR$C_OPR_ADD	51
#define	TIR$C_OPR_SUB	52
#define	TIR$C_OPR_MUL	53
#define	TIR$C_OPR_DIV	54
#define	TIR$C_OPR_AND	55
#define	TIR$C_OPR_IOR	56
#define	TIR$C_OPR_EOR	57
#define	TIR$C_OPR_NEG	58
#define	TIR$C_OPR_COM	59
#define	TIR$C_OPR_INSV	60
#define	TIR$C_OPR_ASH	61
#define	TIR$C_OPR_USH	62
#define	TIR$C_OPR_ROT	63
#define	TIR$C_OPR_SEL	64
#define	TIR$C_OPR_REDEF	65
#define	TIR$C_OPR_DFLIT	66
#define	TIR$C_MAXOPRCOD	66
#define	TIR$C_MINCTLCOD	80
#define	TIR$C_CTL_SETRB	80
#define	TIR$C_CTL_AUGRB	81
#define	TIR$C_CTL_DFLOC	82
#define	TIR$C_CTL_STLOC	83
#define	TIR$C_CTL_STKDL	84
#define	TIR$C_MAXCTLCOD	84

/*
 *	Debugger symbol definitions:  These are done by hand, as no
 *					machine-readable version seems
 *					to be available.
 */
#define	DST$C_C		7		/* Language == "C"	*/
#define DST$C_VERSION	153
#define	DST$C_SOURCE	155		/* Source file		*/
#define DST$C_PROLOG	162
#define	DST$C_BLKBEG	176		/* Beginning of block	*/
#define	DST$C_BLKEND	177		/* End of block	*/
#define DST$C_ENTRY	181
#define DST$C_PSECT	184
#define	DST$C_LINE_NUM	185		/* Line Number		*/
#define DST$C_LBLORLIT	186
#define DST$C_LABEL	187
#define	DST$C_MODBEG	188		/* Beginning of module	*/
#define	DST$C_MODEND	189		/* End of module	*/
#define	DST$C_RTNBEG	190		/* Beginning of routine	*/
#define	DST$C_RTNEND	191		/* End of routine	*/
#define	DST$C_DELTA_PC_W	1		/* Incr PC	*/
#define	DST$C_INCR_LINUM	2		/* Incr Line #	*/
#define	DST$C_INCR_LINUM_W	3		/* Incr Line #	*/
#define DST$C_SET_LINUM_INCR	4
#define DST$C_SET_LINUM_INCR_W	5
#define DST$C_RESET_LINUM_INCR	6
#define DST$C_BEG_STMT_MODE	7
#define DST$C_END_STMT_MODE	8
#define	DST$C_SET_LINE_NUM	9		/* Set Line #	*/
#define DST$C_SET_PC		10
#define DST$C_SET_PC_W		11
#define DST$C_SET_PC_L		12
#define DST$C_SET_STMTNUM	13
#define DST$C_TERM		14		/* End of lines	*/
#define DST$C_TERM_W		15		/* End of lines	*/
#define	DST$C_SET_ABS_PC	16		/* Set PC	*/
#define	DST$C_DELTA_PC_L	17		/* Incr PC	*/
#define DST$C_INCR_LINUM_L	18		/* Incr Line #	*/
#define DST$C_SET_LINUM_B	19		/* Set Line #	*/
#define DST$C_SET_LINUM_L	20		/* Set Line #	*/
#define	DST$C_TERM_L		21		/* End of lines	*/
/* these are used with DST$C_SOURCE */
#define	DST$C_SRC_FORMFEED	16		/* ^L counts	*/
#define	DST$C_SRC_DECLFILE	1		/* Declare file	*/
#define	DST$C_SRC_SETFILE	2		/* Set file	*/
#define	DST$C_SRC_SETREC_L	3		/* Set record	*/
#define	DST$C_SRC_DEFLINES_W	10		/* # of line	*/
/* the following are the codes for the various data types.  Anything not on
 * the list is included under 'advanced_type'
 */
#define DBG$C_UCHAR		0x02
#define DBG$C_USINT		0x03
#define DBG$C_ULINT		0x04
#define DBG$C_SCHAR		0x06
#define DBG$C_SSINT		0x07
#define DBG$C_SLINT		0x08
#define DBG$C_REAL4		0x0a
#define DBG$C_REAL8		0x0b
#define DBG$C_FUNCTION_ADDR	0x17
#define DBG$C_ADVANCED_TYPE	0xa3
/*  These are the codes that are used to generate the definitions of struct
 *  union and enum records
 */
#define DBG$C_ENUM_ITEM			0xa4
#define DBG$C_ENUM_START		0xa5
#define DBG$C_ENUM_END			0xa6
#define DBG$C_STRUCT_START		0xab
#define DBG$C_STRUCT_ITEM		0xff
#define DBG$C_STRUCT_END		0xac
/*  These are the codes that are used in the suffix records to determine the
 *  actual data type
 */
#define DBG$C_BASIC			0x01
#define DBG$C_BASIC_ARRAY		0x02
#define DBG$C_STRUCT			0x03
#define DBG$C_POINTER			0x04
#define DBG$C_VOID			0x05
#define DBG$C_COMPLEX_ARRAY		0x07
/* These codes are used in the generation of the symbol definition records
 */
#define DBG$C_FUNCTION_PARAMETER	0xc9
#define DBG$C_LOCAL_SYM			0xd9
