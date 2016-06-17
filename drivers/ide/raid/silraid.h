struct signature {
	char unknown[0x36];  		/* 0x00 to 0x35 */
	char diskname[32];	     	/* 0x36 to 0x56 */
	char unknown2[0x6c-86];		/* 0x57 to 0x6B */	
	unsigned int array_sectors;	/* 0x6C to 0x6F */
	char unknown2b[8];		/* 0x70 to 0x77 */
	unsigned int thisdisk_sectors;	/* 0x78 to 0x7B */
	char unknown2c[0xFF-0x7B];	/* 0x7C to 0xFF */
	char unknown3[4];		/* 0x100 to 0x103 */
	unsigned short PCI_DEV_ID;	/* 0x104 and 0x105 */
	unsigned short PCI_VEND_ID;	/* 0x106 and 0x107 */
	char unknown4[4];		/* 0x108 to 0x10B */
	unsigned char seconds;		/* 0x10C */
	unsigned char minutes;		/* 0x10D */
	unsigned char hour;		/* 0x10E */
	unsigned char day;		/* 0x10F */
	unsigned char month;		/* 0x110 */
	unsigned char year;		/* 0x111 */
	unsigned short raid0_sectors_per_stride; /* 0x112 */
	char unknown6[2];		/* 0x113 - 0x115 */
	unsigned char disk_in_set;	/* 0x116 */
	unsigned char raidlevel;	/* 0x117 */
	unsigned char disks_in_set;	/* 0x118 */
	char unknown7[0x12a - 0x118];   /* 0x118 - 0x12a */
	unsigned char idechannel;	/* 0x12b */
	char unknown8[0x13D-0x12B];	/* 0x12c - 0x13d */
	unsigned short checksum1;	/* 0x13e and 0x13f */	
	char assumed_zeros[509-0x13f];
	unsigned short checksum2;	/* 0x1FE and 0x1FF */
} __attribute__((packed));
