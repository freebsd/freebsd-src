/*-
 * Copyright (c) 2017 Juniper Networks, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * 9P Protocol Support Code
 * This file provides the standard for the FS interactions with the server
 * interface as it can understand only this protocol. The details of the
 * protocol can be found here
 * XXX (link to protocol details page on FreeBSD wiki)
 */

#include <sys/types.h>
#include <fs/p9fs/p9_client.h>
#include <fs/p9fs/p9_debug.h>
#include <fs/p9fs/p9_protocol.h>

#define P9FS_MAXLEN 255

static int p9_buf_writef(struct p9_buffer *buf, int proto_version,
    const char *fmt, ...);
static void stat_free(struct p9_wstat *sbuf);

static void
stat_free(struct p9_wstat *stbuf)
{

	free(stbuf->name, M_TEMP);
	free(stbuf->uid, M_TEMP);
	free(stbuf->gid, M_TEMP);
	free(stbuf->muid, M_TEMP);
	free(stbuf->extension, M_TEMP);
}

static size_t
buf_read(struct p9_buffer *buf, void *data, size_t size)
{
	size_t len;

	len = min(buf->size - buf->offset, size);

	memcpy(data, &buf->sdata[buf->offset], len);
	buf->offset += len;

	return (size - len);
}

static size_t
buf_write(struct p9_buffer *buf, const void *data, size_t size)
{
	size_t len;

	len = min(buf->capacity - buf->size, size);

	memcpy(&buf->sdata[buf->size], data, len);
	buf->size += len;

	return (size - len);
}

/*
 * Main buf_read routine. This copies the data from the buffer into the
 * respective values based on the data type.
 * Here
 *	  b - int8_t
 *	  w - int16_t
 *	  d - int32_t
 *	  q - int64_t
 *	  s - string
 *	  u - uid
 *	  g - gid
 *	  Q - qid
 *	  S - stat
 *	  A - getattr (9P2000.L)
 *	  D - data blob (int32_t size followed by void *, results are not freed)
 *	  T - array of strings (int16_t count, followed by strings)
 *	  R - array of qids (int16_t count, followed by qids)
 *	  ? - return if version is not .u or .l
 */
static int
p9_buf_vreadf(struct p9_buffer *buf, int proto_version, const char *fmt,
    va_list ap)
{
	const char *ptr;
	int error;

	error = 0;

	for (ptr = fmt; *ptr; ptr++) {
		switch (*ptr) {
		case 'b':
		{
			int8_t *val = va_arg(ap, int8_t *);

			if (buf_read(buf, val, sizeof(*val)))
				error = EFAULT;
			break;
		}
		case 'w':
		{
			int16_t *val = va_arg(ap, int16_t *);

			if (buf_read(buf, val, sizeof(*val)))
				error = EFAULT;
			break;
		}
		case 'd':
		{
			int32_t *val = va_arg(ap, int32_t *);

			if (buf_read(buf, val, sizeof(*val)))
				error = EFAULT;
			break;
		}
		case 'q':
		{
			int64_t *val = va_arg(ap, int64_t *);

			if (buf_read(buf, val, sizeof(*val)))
				error = EFAULT;
			break;
		}
		case 's':
		{
			char **sptr_p = va_arg(ap, char **);
			uint16_t len;
			char *sptr;

			error = buf_read(buf, &len, sizeof(uint16_t));
			if (error)
				break;

			sptr = malloc(len + 1, M_TEMP, M_NOWAIT | M_ZERO);

			if (buf_read(buf, sptr, len)) {
				error = EFAULT;
				free(sptr, M_TEMP);
				sptr = NULL;
			} else {
				(sptr)[len] = 0;
				*sptr_p = sptr;
			}
			break;
		}
		case 'u':
		{
			uid_t *val = va_arg(ap, uid_t *);

			if (buf_read(buf, val, sizeof(*val)))
				error = EFAULT;
			break;

		}
		case 'g':
		{
			gid_t *val = va_arg(ap, gid_t *);

			if (buf_read(buf, val, sizeof(*val)))
				error = EFAULT;
			break;

		}
		case 'Q':
		{
			struct p9_qid *qid = va_arg(ap, struct p9_qid *);

			error = p9_buf_readf(buf, proto_version, "bdq",
			    &qid->type, &qid->version, &qid->path);

			break;
		}
		case 'S':
		{
			struct p9_wstat *stbuf = va_arg(ap, struct p9_wstat *);

			error = p9_buf_readf(buf, proto_version, "wwdQdddqssss?sddd",
			    &stbuf->size, &stbuf->type, &stbuf->dev, &stbuf->qid,
			    &stbuf->mode, &stbuf->atime, &stbuf->mtime, &stbuf->length,
			    &stbuf->name, &stbuf->uid, &stbuf->gid, &stbuf->muid,
			    &stbuf->extension, &stbuf->n_uid, &stbuf->n_gid, &stbuf->n_muid);

			if (error != 0)
				stat_free(stbuf);
			break;
		}
		case 'A':
		{
			struct p9_stat_dotl *stbuf = va_arg(ap, struct p9_stat_dotl *);

			error = p9_buf_readf(buf, proto_version, "qQdugqqqqqqqqqqqqqqq",
			   &stbuf->st_result_mask, &stbuf->qid, &stbuf->st_mode,
			   &stbuf->st_uid,&stbuf->st_gid, &stbuf->st_nlink,
			   &stbuf->st_rdev, &stbuf->st_size, &stbuf->st_blksize,
			   &stbuf->st_blocks, &stbuf->st_atime_sec,
			   &stbuf->st_atime_nsec, &stbuf->st_mtime_sec,
			   &stbuf->st_mtime_nsec, &stbuf->st_ctime_sec,
			   &stbuf->st_ctime_nsec, &stbuf->st_btime_sec,
			   &stbuf->st_btime_nsec, &stbuf->st_gen,
			   &stbuf->st_data_version);

			break;
		}
		case 'D':
		{
			uint32_t *count = va_arg(ap, uint32_t *);
			void **data = va_arg(ap, void **);

			error = buf_read(buf, count, sizeof(uint32_t));
			if (error == 0) {
				*count = MIN(*count, buf->size - buf->offset);
				*data = &buf->sdata[buf->offset];
			}
			break;
		}
		case 'T':
		{
			uint16_t *nwname_p = va_arg(ap, uint16_t *);
			char ***wnames_p = va_arg(ap, char ***);
			uint16_t nwname;
			char **wnames;
			int i;

			error = buf_read(buf, nwname_p, sizeof(uint16_t));
			if (error != 0)
				break;

			nwname = *nwname_p;
			wnames = malloc(sizeof(char *) * nwname, M_TEMP, M_NOWAIT | M_ZERO);

			for (i = 0; i < nwname && (error == 0); i++)
				error = p9_buf_readf(buf, proto_version, "s", &wnames[i]);

			if (error != 0) {
				for (i = 0; i < nwname; i++)
					free((wnames)[i], M_TEMP);
				free(wnames, M_TEMP);
			} else
				*wnames_p = wnames;
			break;
		}
		case 'R':
		{
			uint16_t *nwqid_p = va_arg(ap, uint16_t *);
			struct p9_qid **wqids_p = va_arg(ap, struct p9_qid **);
			uint16_t nwqid;
			struct p9_qid *wqids;
			int i;

			wqids = NULL;
			error = buf_read(buf, nwqid_p, sizeof(uint16_t));
			if (error != 0)
				break;

			nwqid = *nwqid_p;
			wqids = malloc(nwqid * sizeof(struct p9_qid), M_TEMP, M_NOWAIT | M_ZERO);
			if (wqids == NULL) {
				error = ENOMEM;
				break;
			}
			for (i = 0; i < nwqid && (error == 0); i++)
				error = p9_buf_readf(buf, proto_version, "Q", &(wqids)[i]);

			if (error != 0) {
				free(wqids, M_TEMP);
			} else
				*wqids_p = wqids;

			break;
		}
		case '?':
		{
			if ((proto_version != p9_proto_2000u) && (proto_version != p9_proto_2000L))
				return (0);
			break;
		}
		default:
			break;
		}

		if (error != 0)
			break;
	}

	return (error);
}

/*
 * Main buf_write routine. This copies the data into the buffer from the
 * respective values based on the data type.
 * Here
 *	  b - int8_t
 *	  w - int16_t
 *	  d - int32_t
 *	  q - int64_t
 *	  s - string
 *	  u - uid
 *	  g - gid
 *	  Q - qid
 *	  S - stat
 *	  D - data blob (int32_t size followed by void *, results are not freed)
 *	  T - array of strings (int16_t count, followed by strings)
 *	  W - string of a specific length
 *	  R - array of qids (int16_t count, followed by qids)
 *	  A - setattr (9P2000.L)
 *	  ? - return if version is not .u or .l
 */

int
p9_buf_vwritef(struct p9_buffer *buf, int proto_version, const char *fmt,
	va_list ap)
{
	const char *ptr;
	int error;

	error = 0;

	for (ptr = fmt; *ptr; ptr++) {
		switch (*ptr) {
		case 'b':
		{
			int8_t val = va_arg(ap, int);

			if (buf_write(buf, &val, sizeof(val)))
				error = EFAULT;
			break;
		}
		case 'w':
		{
			int16_t val = va_arg(ap, int);

			if (buf_write(buf, &val, sizeof(val)))
				error = EFAULT;
			break;
		}
		case 'd':
		{
			int32_t val = va_arg(ap, int32_t);

			if (buf_write(buf, &val, sizeof(val)))
				error = EFAULT;
			break;
		}
		case 'q':
		{
			int64_t val = va_arg(ap, int64_t);

			if (buf_write(buf, &val, sizeof(val)))
				error = EFAULT;

			break;
		}
		case 's':
		{
			const char *sptr = va_arg(ap, const char *);
		        uint16_t len = 0;

	                if (sptr)
			    len = MIN(strlen(sptr), P9FS_MAXLEN);

			error = buf_write(buf, &len, sizeof(uint16_t));
			if (error == 0 && buf_write(buf, sptr, len))
				error = EFAULT;
			break;
		}
		case 'u':
		{
			uid_t val = va_arg(ap, uid_t);

			if (buf_write(buf, &val, sizeof(val)))
				error = EFAULT;
			break;

		}
		case 'g':
		{
			gid_t val = va_arg(ap, gid_t);

			if (buf_write(buf, &val, sizeof(val)))
				error = EFAULT;
			break;

		}
		case 'Q':
		{
			const struct p9_qid *qid = va_arg(ap, const struct p9_qid *);

			error = p9_buf_writef(buf, proto_version, "bdq",
			    qid->type, qid->version, qid->path);
			break;
		}
		case 'S':
		{
			struct p9_wstat *stbuf = va_arg(ap, struct p9_wstat *);

			error = p9_buf_writef(buf, proto_version,
			    "wwdQdddqssss?sddd", stbuf->size, stbuf->type, stbuf->dev, &stbuf->qid,
			    stbuf->mode, stbuf->atime, stbuf->mtime, stbuf->length, stbuf->name,
			    stbuf->uid, stbuf->gid, stbuf->muid, stbuf->extension, stbuf->n_uid,
			    stbuf->n_gid, stbuf->n_muid);

			if (error != 0)
				stat_free(stbuf);

			break;
		}
		case 'D':
		{
			uint32_t count = va_arg(ap, uint32_t);
			void *data = va_arg(ap, void *);

			error = buf_write(buf, &count, sizeof(uint32_t));
			if ((error == 0) && buf_write(buf, data, count))
				error = EFAULT;

			break;
		}
		case 'T':
		{
                        char **wnames = va_arg(ap, char **);
                        uint16_t nwnames = va_arg(ap, int);

			error = buf_write(buf, &nwnames, sizeof(uint16_t));
			if (error == 0) {
				int i = 0;
				for (i = 0; i < nwnames; i++) {
					error = p9_buf_writef(buf, proto_version, "s", wnames[i]);
					if (error != 0)
						break;
				}
			}
			break;
		}
                case 'W':
                {
                        const char *sptr = va_arg(ap, const char*);
                        uint16_t len = va_arg(ap, int);

			error = buf_write(buf, &len, sizeof(uint16_t));
			if (error == 0 && buf_write(buf, sptr, len))
				error = EFAULT;
			break;

                }
		case 'R':
		{
			uint16_t nwqid = va_arg(ap, int);
			struct p9_qid *wqids = va_arg(ap, struct p9_qid *);
			int i;

			error = buf_write(buf, &nwqid, sizeof(uint16_t));
			if (error == 0) {

				for (i = 0; i < nwqid; i++) {
					error = p9_buf_writef(buf, proto_version, "Q", &wqids[i]);
					if (error != 0)
						break;
				}
			}
			break;
		}
		case 'A':
		{
			struct p9_iattr_dotl *p9attr = va_arg(ap, struct p9_iattr_dotl *);

			error = p9_buf_writef(buf, proto_version, "ddugqqqqq",
			    p9attr->valid, p9attr->mode, p9attr->uid,
			    p9attr->gid, p9attr->size, p9attr->atime_sec,
			    p9attr->atime_nsec, p9attr->mtime_sec,
			    p9attr->mtime_nsec);

			break;
		}
		case '?':
		{
			if ((proto_version != p9_proto_2000u) && (proto_version != p9_proto_2000L))
				return (0);
			break;
		}
		default:
			break;
		}

		if (error != 0)
			break;
	}

	return (error);
}

/* Variadic form of buf_read */
int
p9_buf_readf(struct p9_buffer *buf, int proto_version, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = p9_buf_vreadf(buf, proto_version, fmt, ap);
	va_end(ap);

	return (ret);
}

/* Variadic form of buf_write */
static int
p9_buf_writef(struct p9_buffer *buf, int proto_version, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = p9_buf_vwritef(buf, proto_version, fmt, ap);
	va_end(ap);

	return (ret);
}

/* File stats read routine for P9 to get attributes of files */
int
p9stat_read(struct p9_client *clnt, char *buf, size_t len, struct p9_wstat *st)
{
	struct p9_buffer msg_buf;
	int ret;

	msg_buf.size = len;
	msg_buf.capacity = len;
	msg_buf.sdata = buf;
	msg_buf.offset = 0;

	ret = p9_buf_readf(&msg_buf, clnt->proto_version, "S", st);
	if (ret) {
		P9_DEBUG(ERROR, "%s: failed: %d\n", __func__, ret);
	}

	return (ret);
}

/*
 * P9_header preparation routine. All p9 buffers have to have this header(QEMU_HEADER) at the
 * front of the buffer.
 */
int
p9_buf_prepare(struct p9_buffer *buf, int8_t type)
{
	buf->id = type;
	return (p9_buf_writef(buf, 0, "dbw", 0, type, buf->tag));
}

/*
 * Final write to the buffer, this is the total size of the buffer. Since the buffer length can
 * vary with request, this is computed at the end just before sending the request to the driver
 */
int
p9_buf_finalize(struct p9_client *clnt, struct p9_buffer *buf)
{
	int size;
	int error;

	size = buf->size;
	buf->size = 0;
	error = p9_buf_writef(buf, 0, "d", size);
	buf->size = size;

	P9_DEBUG(LPROTO, "%s: size=%d type: %d tag: %d\n",
	    __func__, buf->size, buf->id, buf->tag);

	return (error);
}

/* Reset values of the buffer */
void
p9_buf_reset(struct p9_buffer *buf)
{

	buf->offset = 0;
	buf->size = 0;
}

/*
 * Directory entry read with the buf we have. Call this once we have the buf to parse.
 * This buf, obtained from the server, is parsed to make dirent in readdir.
 */
int
p9_dirent_read(struct p9_client *clnt, char *buf, int start, int len,
	struct p9_dirent *dent)
{
	struct p9_buffer msg_buf;
	int ret;
	char *nameptr;
	uint16_t sle;

	msg_buf.size = len;
	msg_buf.capacity = len;
	msg_buf.sdata = buf;
	msg_buf.offset = start;

	ret = p9_buf_readf(&msg_buf, clnt->proto_version, "Qqbs", &dent->qid,
	    &dent->d_off, &dent->d_type, &nameptr);
	if (ret) {
		P9_DEBUG(ERROR, "%s: failed: %d\n", __func__, ret);
		goto out;
	}

	sle = strlen(nameptr);
	strncpy(dent->d_name, nameptr, sle);
	dent->len = sle;
	free(nameptr, M_TEMP);
out:
	return (msg_buf.offset);
}
