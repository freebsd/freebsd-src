/*
 * Copyright (c) 1999, Boris Popov
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
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#include <sys/param.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <strings.h>

#include <netncp/ncp_lib.h>
#include <netncp/ncp_file.h>
#include <nwfs/nwfs.h>

int
ncp_read(NWCONN_HANDLE connid, ncp_fh *fh, off_t offset, size_t count, char *target) {
	int result;
	struct ncp_rw rwrq;
	DECLARE_RQ;

	ncp_init_request(conn);
	ncp_add_byte(conn, NCP_CONN_READ);
	rwrq.nrw_fh = *fh;
	rwrq.nrw_base = target;
	rwrq.nrw_cnt = count;
	rwrq.nrw_offset = offset;
	ncp_add_mem(conn, &rwrq, sizeof(rwrq));
	if ((result = ncp_conn_request(connid, conn)) < 0) 
		return -1;
	return result;
}

int
ncp_write(NWCONN_HANDLE connid, ncp_fh *fh, off_t offset, size_t count, char *source)
{
	int result;
	struct ncp_rw rwrq;
	DECLARE_RQ;

	ncp_init_request(conn);
	ncp_add_byte(conn, NCP_CONN_WRITE);
	rwrq.nrw_fh = *fh;
	rwrq.nrw_base = source;
	rwrq.nrw_cnt = count;
	rwrq.nrw_offset = offset;
	ncp_add_mem(conn, &rwrq, sizeof(rwrq));

	if ((result = ncp_conn_request(connid, conn)) < 0)
		return -1;
	return result;
}

int
ncp_geteinfo(char *path, struct nw_entry_info *fi) {
	int d, error;

	if ((d = open(path, O_RDONLY)) < 0) return errno;
	if ((error = ioctl(d, NWFSIOC_GETEINFO, fi)) != 0) return errno;
	close(d);
	return 0;
}


int
ncp_AllocTempDirHandle(char *path, NWDIR_HANDLE *pdh) {
	int d;

	if ((d = open(path, O_RDONLY)) < 0) return errno;
	*pdh = d;
	return 0;
}

int
ncp_DeallocateDirHandle(NWDIR_HANDLE dh) {
	close(dh);
	return 0;
}

int
ncp_GetNSEntryInfo(NWDIR_HANDLE dh, struct nw_entry_info *fi, int *ns) {
	int error;

	if ((error = ioctl(dh, NWFSIOC_GETEINFO, fi)) != 0) return errno;
	if ((error = ioctl(dh, NWFSIOC_GETNS, ns)) != 0) return errno;
	return 0;
}

NWCCODE
ncp_ScanForDeletedFiles(NWCONN_HANDLE cH, pnuint32 iterHandle, 
	pnuint32 volNum, pnuint32 dirBase, nuint8 ns,
	NWDELETED_INFO *entryInfo)
{
	int error;
	struct nw_entry_info *pfi;
	DECLARE_RQ;
#define	UNITEDT(d,t)	(((d) << 16) | (t))

	bzero(entryInfo, sizeof(NWDELETED_INFO));
	ncp_init_request(conn);
	ncp_add_byte(conn, 16);
	ncp_add_byte(conn, ns);
	ncp_add_byte(conn, 0);		/* data stream */
	ncp_add_dword_lh(conn, IM_ALL & ~(IM_SPACE_ALLOCATED | IM_TOTAL_SIZE | IM_EA | IM_DIRECTORY));
	ncp_add_dword_lh(conn, *iterHandle);

	ncp_add_byte(conn, *volNum);
	ncp_add_dword_lh(conn, *dirBase);
	ncp_add_byte(conn, NCP_HF_DIRBASE);	/* dirBase */
	ncp_add_byte(conn, 0);			/* no component */
	if ((error = ncp_request(cH, 87, conn)) != 0) {
		return error;
	}
	if (conn->rpsize < 0x61) {
		return EBADRPC;	/* EACCES ? */
	}
	*iterHandle = entryInfo->sequence = ncp_reply_dword_lh(conn, 0x00);
	entryInfo->deletedTime = ncp_reply_word_lh(conn, 0x04);
	entryInfo->deletedDateAndTime = UNITEDT(ncp_reply_word_lh(conn, 0x06), entryInfo->deletedTime);
	entryInfo->deletorID = ncp_reply_dword_hl(conn, 0x08);
	*volNum = ncp_reply_dword_lh(conn, 0x0C);
	*dirBase = ncp_reply_dword_lh(conn, 0x10);
	entryInfo->parent = ncp_reply_dword_lh(conn, 0x10);
	pfi = (struct nw_entry_info*) ncp_reply_data(conn, 0x14);
	entryInfo->nameLength = pfi->nameLen;
	memcpy(entryInfo->name, pfi->entryName, pfi->nameLen);
	return error;
}

NWCCODE
ncp_PurgeDeletedFile(NWCONN_HANDLE cH, nuint32 iterHandle, 
	nuint32 volNum, nuint32 dirBase, nuint8 ns)
{
	DECLARE_RQ;

	ncp_init_request(conn);
	ncp_add_byte(conn, 18);
	ncp_add_byte(conn, ns);
	ncp_add_byte(conn, 0);		/* reserved */
	ncp_add_dword_lh(conn, iterHandle);
	ncp_add_dword_lh(conn, volNum);
	ncp_add_dword_lh(conn, dirBase);
	return ncp_request(cH, 87, conn);
}


static void 
ncp_extract_entryInfo(char *data, NW_ENTRY_INFO *entry) {
	u_char l;
	const int info_struct_size = sizeof(NW_ENTRY_INFO) - 257;

	memcpy(entry, data, info_struct_size);
	data += info_struct_size;
	l = *data++;
	entry->nameLen = l;
	memcpy(entry->entryName, data, l);
	entry->entryName[l] = '\0';
	return;
}

NWCCODE
ncp_ScanNSEntryInfo(NWCONN_HANDLE cH,
	nuint8 namSpc, nuint16 attrs, SEARCH_SEQUENCE *seq,
	pnstr8 searchPattern, nuint32 retInfoMask, NW_ENTRY_INFO *entryInfo) 
{
	int error, l;
	DECLARE_RQ;

	if (seq->searchDirNumber == -1) {
		seq->searchDirNumber = 0;
		ncp_init_request(conn);
		ncp_add_byte(conn, 2);
		ncp_add_byte(conn, namSpc);
		ncp_add_byte(conn, 0);
		ncp_add_handle_path(conn, seq->volNumber, seq->dirNumber, 
		    NCP_HF_DIRBASE, NULL);
		error = ncp_request(cH, 87, conn);
		if (error) return error;
		memcpy(seq, ncp_reply_data(conn, 0), 9);
	}
	ncp_init_request(conn);
	ncp_add_byte(conn, 3);
	ncp_add_byte(conn, namSpc);
	ncp_add_byte(conn, 0);		/* dataStream */
	ncp_add_word_lh(conn, attrs);	/* SearchAttributes */
	ncp_add_dword_lh(conn, retInfoMask);
	ncp_add_mem(conn, seq, sizeof(*seq));
	l = strlen(searchPattern);
	ncp_add_byte(conn, l);
	ncp_add_mem(conn, searchPattern, l);
	error = ncp_request(cH, 87, conn);
	if (error) return error;
	memcpy(seq, ncp_reply_data(conn, 0), sizeof(*seq));
	ncp_extract_entryInfo(ncp_reply_data(conn, 10), entryInfo);
	return 0;
}

int
ncp_NSEntryInfo(NWCONN_HANDLE cH, nuint8 ns, nuint8 vol, nuint32 dirent,
    NW_ENTRY_INFO *entryInfo)
{
	DECLARE_RQ;
	int error;

	ncp_init_request(conn);
	ncp_add_byte(conn, 6);
	ncp_add_byte(conn, ns);
	ncp_add_byte(conn, ns);	/* DestNameSpace */
	ncp_add_word_lh(conn, htons(0xff00));	/* get all */
	ncp_add_dword_lh(conn, IM_ALL);
	ncp_add_handle_path(conn, vol, dirent, NCP_HF_DIRBASE, NULL);
	error = ncp_request(cH, 87, conn);
	if (error) return error;
	ncp_extract_entryInfo(ncp_reply_data(conn, 0), entryInfo);
	return 0;
}

NWCCODE
NWGetVolumeName(NWCONN_HANDLE cH, u_char volume, char *name) {
	int error, len;
	DECLARE_RQ;

	ncp_init_request_s(conn, 44);
	ncp_add_byte(conn, volume);
	error = ncp_request(cH, 22, conn);
	if (error) return error;
	len = ncp_reply_byte(conn, 29);
	if (len == 0)
		return ENOENT;
	bcopy(ncp_reply_data(conn, 30), name, len);
	name[len] = 0;
	return 0;
}
