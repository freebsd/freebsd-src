/*
 * support.c -  Specific support functions
 *
 * Copyright (C) 1997 Martin von Löwis
 * Copyright (C) 1997 Régis Duchesne
 * Copyright (C) 2001 Anton Altaparmakov (AIA)
 */

#include "ntfstypes.h"
#include "struct.h"
#include "support.h"

#include <stdarg.h>
#include <linux/slab.h>
#include <linux/locks.h>
#include <linux/fs.h>
#include "util.h"
#include "inode.h"
#include "macros.h"
#include <linux/nls.h>

static char print_buf[1024];

#ifdef DEBUG
#include "sysctl.h"
#include <linux/kernel.h>

/* Debugging output */
void ntfs_debug(int mask, const char *fmt, ...)
{
	va_list ap;

	/* Filter it with the debugging level required */
	if (ntdebug & mask) {
		va_start(ap,fmt);
		strcpy(print_buf, KERN_DEBUG "NTFS: ");
		vsprintf(print_buf + 9, fmt, ap);
		printk(print_buf);
		va_end(ap);
	}
}

#ifndef ntfs_malloc
/* Verbose kmalloc */
void *ntfs_malloc(int size)
{
	void *ret;

	ret = kmalloc(size, GFP_KERNEL);
	ntfs_debug(DEBUG_MALLOC, "Allocating %x at %p\n", size, ret);

	return ret;
}
#endif

#ifndef ntfs_free
/* Verbose kfree() */
void ntfs_free(void *block)
{
        ntfs_debug(DEBUG_MALLOC, "Freeing memory at %p\n", block);
	kfree(block);
}
#endif
#else /* End of DEBUG functions. Normal ones below... */

#ifndef ntfs_malloc
void *ntfs_malloc(int size)
{
	return kmalloc(size, GFP_KERNEL);
}
#endif

#ifndef ntfs_free
void ntfs_free(void *block)
{
	kfree(block);
}
#endif
#endif /* DEBUG */

void ntfs_bzero(void *s, int n)
{
	memset(s, 0, n);
}

/* These functions deliberately return no value. It is dest, anyway,
   and not used anywhere in the NTFS code.  */

void ntfs_memcpy(void *dest, const void *src, ntfs_size_t n)
{
	memcpy(dest, src, n);
}

void ntfs_memmove(void *dest, const void *src, ntfs_size_t n)
{
	memmove(dest, src, n);
}

/* Warn that an error occurred. */
void ntfs_error(const char *fmt,...)
{
        va_list ap;

        va_start(ap, fmt);
        strcpy(print_buf, KERN_ERR "NTFS: ");
        vsprintf(print_buf + 9, fmt, ap);
        printk(print_buf);
        va_end(ap);
}

int ntfs_read_mft_record(ntfs_volume *vol, int mftno, char *buf)
{
	int error;
	ntfs_io io;

	ntfs_debug(DEBUG_OTHER, "read_mft_record 0x%x\n", mftno);
	if (mftno == FILE_Mft)
	{
		ntfs_memcpy(buf, vol->mft, vol->mft_record_size);
		return 0;
	}
	if (!vol->mft_ino)
	{
		printk(KERN_ERR "NTFS: mft_ino is NULL. Something is terribly "
				"wrong here!\n");
		return -ENODATA;
	}
 	io.fn_put = ntfs_put;
	io.fn_get = 0;
	io.param = buf;
	io.size = vol->mft_record_size;
	ntfs_debug(DEBUG_OTHER, "read_mft_record: calling ntfs_read_attr with: "
		"mftno = 0x%x, vol->mft_record_size_bits = 0x%x, "
		"mftno << vol->mft_record_size_bits = 0x%Lx\n", mftno,
		vol->mft_record_size_bits,
		(__s64)mftno << vol->mft_record_size_bits);
	error = ntfs_read_attr(vol->mft_ino, vol->at_data, NULL,
				(__s64)mftno << vol->mft_record_size_bits, &io);
	if (error || (io.size != vol->mft_record_size)) {
		ntfs_debug(DEBUG_OTHER, "read_mft_record: read 0x%x failed "
				   	"(%d,%d,%d)\n", mftno, error, io.size,
				   	vol->mft_record_size);
		return error ? error : -ENODATA;
	}
	ntfs_debug(DEBUG_OTHER, "read_mft_record: finished read 0x%x\n", mftno);
	if (!ntfs_check_mft_record(vol, buf)) {
		/* FIXME: This is incomplete behaviour. We might be able to
		 * recover at this stage. ntfs_check_mft_record() is too
		 * conservative at aborting it's operations. It is OK for
		 * now as we just can't handle some on disk structures
		 * this way. (AIA) */
		printk(KERN_WARNING "NTFS: Invalid MFT record for 0x%x\n", mftno);
		return -EIO;
	}
	ntfs_debug(DEBUG_OTHER, "read_mft_record: Done 0x%x\n", mftno);
	return 0;
}

int ntfs_getput_clusters(ntfs_volume *vol, int cluster, ntfs_size_t start_offs,
		ntfs_io *buf)
{
	struct super_block *sb = NTFS_SB(vol);
	struct buffer_head *bh;
	int length = buf->size;
	int error = 0;
	ntfs_size_t to_copy;

	ntfs_debug(DEBUG_OTHER, "%s_clusters %d %d %d\n", 
		   buf->do_read ? "get" : "put", cluster, start_offs, length);
	to_copy = vol->cluster_size - start_offs;
	while (length) {
		if (!(bh = sb_bread(sb, cluster))) {
			ntfs_debug(DEBUG_OTHER, "%s failed\n",
				   buf->do_read ? "Reading" : "Writing");
			error = -EIO;
			goto error_ret;
		}
		if (to_copy > length)
			to_copy = length;
		lock_buffer(bh);
		if (buf->do_read) {
			buf->fn_put(buf, bh->b_data + start_offs, to_copy);
			unlock_buffer(bh);
		} else {
			buf->fn_get(bh->b_data + start_offs, buf, to_copy);
			mark_buffer_dirty(bh);
			unlock_buffer(bh);
			/*
			 * Note: We treat synchronous IO on a per volume basis
			 * disregarding flags of individual inodes. This can
			 * lead to some strange write ordering effects upon a
			 * remount with a change in the sync flag but it should
			 * not break anything. [Except if the system crashes
			 * at that point in time but there would be more thigs
			 * to worry about than that in that case...]. (AIA)
			 */
			if (sb->s_flags & MS_SYNCHRONOUS) {
				ll_rw_block(WRITE, 1, &bh);
				wait_on_buffer(bh);
				if (buffer_req(bh) && !buffer_uptodate(bh)) {
					printk(KERN_ERR "IO error syncing NTFS "
					       "cluster [%s:%i]\n",
					       bdevname(sb->s_dev), cluster);
					brelse(bh);
					error = -EIO;
					goto error_ret;
				}
			}
		}
		brelse(bh);
		length -= to_copy;
		start_offs = 0;
		to_copy = vol->cluster_size;
		cluster++;
	}
error_ret:
	return error;
}

ntfs_time64_t ntfs_now(void)
{
	return ntfs_unixutc2ntutc(CURRENT_TIME);
}

int ntfs_dupuni2map(ntfs_volume *vol, ntfs_u16 *in, int in_len, char **out,
		int *out_len)
{
	int i, o, chl, chi;
	char *result, *buf, charbuf[NLS_MAX_CHARSET_SIZE];
	struct nls_table *nls = vol->nls_map;

	result = ntfs_malloc(in_len + 1);
	if (!result)
		return -ENOMEM;
	*out_len = in_len;
	for (i = o = 0; i < in_len; i++) {
		/* FIXME: Byte order? */
		wchar_t uni = in[i];
		if ((chl = nls->uni2char(uni, charbuf,
				NLS_MAX_CHARSET_SIZE)) > 0) {
			/* Adjust result buffer. */
			if (chl > 1) {
				buf = ntfs_malloc(*out_len + chl);
				if (!buf) {
					i = -ENOMEM;
					goto err_ret;
				}
				memcpy(buf, result, o);
				ntfs_free(result);
				result = buf;
				*out_len += (chl - 1);
			}
			for (chi = 0; chi < chl; chi++)
				result[o++] = charbuf[chi];
		} else {
			/* Invalid character. */
			printk(KERN_ERR "NTFS: Unicode name contains a "
					"character that cannot be converted "
					"to chosen character set. Remount "
					"with utf8 encoding and this should "
					"work.\n");
			i = -EILSEQ;
			goto err_ret;
		}
	}
	result[*out_len] = '\0';
	*out = result;
	return 0;
err_ret:
	ntfs_free(result);
	*out_len = 0;
	*out = NULL;
	return i;
}

int ntfs_dupmap2uni(ntfs_volume *vol, char* in, int in_len, ntfs_u16 **out,
		int *out_len)
{
	int i, o;
	ntfs_u16 *result;
	struct nls_table *nls = vol->nls_map;

	*out = result = ntfs_malloc(2 * in_len);
	if (!result) {
		*out_len = 0;
		return -ENOMEM;
	}
	*out_len = in_len;
	for (i = o = 0; i < in_len; i++, o++) {
		wchar_t uni;
		int charlen;

		charlen = nls->char2uni(&in[i], in_len - i, &uni);
		if (charlen < 0) {
			i = charlen;
			goto err_ret;
		}
		*out_len -= charlen - 1;
		i += charlen - 1;
		/* FIXME: Byte order? */
		result[o] = uni;
		if (!result[o]) {
			i = -EILSEQ;
			goto err_ret;
		}
	}
	return 0;
err_ret:
	printk(KERN_ERR "NTFS: Name contains a character that cannot be "
			"converted to Unicode.\n");
	ntfs_free(result);
	*out_len = 0;
	*out = NULL;
	return i;
}

