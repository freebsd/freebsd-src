/*
 * util.h - Header file for util.c
 *
 * Copyright (C) 1997 Régis Duchesne
 * Copyright (C) 2001 Anton Altaparmakov (AIA)
 */

/* The first 16 inodes correspond to NTFS special files. */
typedef enum {
	FILE_Mft	= 0,
	FILE_MftMirr	= 1,
	FILE_LogFile	= 2,
	FILE_Volume	= 3,
	FILE_AttrDef	= 4,
	FILE_root	= 5,
	FILE_BitMap	= 6,
	FILE_Boot	= 7,
	FILE_BadClus	= 8,
	FILE_Secure	= 9,
	FILE_UpCase	= 10,
	FILE_Extend	= 11,
	FILE_Reserved12	= 12,
	FILE_Reserved13	= 13,
	FILE_Reserved14	= 14,
	FILE_Reserved15	= 15,
} NTFS_SYSTEM_FILES;
 
/* Memory management */
void *ntfs_calloc(int size);

/* String operations */
/*  Copy Unicode <-> ASCII */
void ntfs_ascii2uni(short int *to, char *from, int len);

/*  Comparison */
int ntfs_uni_strncmp(short int* a, short int *b, int n);
int ntfs_ua_strncmp(short int* a, char* b, int n);

/* Same address space copies */
void ntfs_put(ntfs_io *dest, void *src, ntfs_size_t n);
void ntfs_get(void* dest, ntfs_io *src, ntfs_size_t n);

/* Charset conversion */
int ntfs_encodeuni(ntfs_volume *vol, ntfs_u16 *in, int in_len, char **out,
		   int *out_len);
int ntfs_decodeuni(ntfs_volume *vol, char *in, int in_len, ntfs_u16 **out,
		   int *out_len);

/* Time conversion */
/*  NT <-> Unix */
ntfs_time_t ntfs_ntutc2unixutc(ntfs_time64_t ntutc);
ntfs_time64_t ntfs_unixutc2ntutc(ntfs_time_t t);

/* Attribute names */
void ntfs_indexname(char *buf, int type);

