/*
 * Copyright (c) 1999 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 *	$Id: bf_torek.h,v 8.6 1999/11/04 19:31:25 ca Exp $
 *
 * Contributed by Exactis.com, Inc.
 *
 */

#ifndef BF_TOREK_H
#define BF_TOREK_H 1
/*
**  Data structure for storing information about each buffered file
*/

struct bf
{
	bool	bf_committed;	/* Has this buffered file been committed? */
	bool	bf_ondisk;	/* On disk: committed or buffer overflow */
	int	bf_flags;
	int	bf_disk_fd;	/* If on disk, associated file descriptor */
	char	*bf_buf;	/* Memory buffer */
	int	bf_bufsize;	/* Length of above buffer */
	int	bf_buffilled;	/* Bytes of buffer actually filled */
	char	*bf_filename;	/* Name of buffered file, if ever committed */
	mode_t	bf_filemode;	/* Mode of buffered file, if ever committed */
	fpos_t	bf_offset;	/* Currect file offset */
	int	bf_size;	/* Total current size of file */
	int	bf_refcount;	/* Reference count */
};

/* Our lower-level I/O routines */
extern int	_bfclose __P((void *));
extern int	_bfread __P((void *, char *, int));
extern fpos_t	_bfseek __P((void *, fpos_t, int));
extern int	_bfwrite __P((void *, const char *, int));
#endif /* BF_TOREK_H */
