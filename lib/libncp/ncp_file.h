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

#ifndef _NCP_NCP_FILE_H_
#define _NCP_NCP_FILE_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   nuint32  sequence;
   nuint32  parent;
   nuint32  attributes;
   nuint8   uniqueID;
   nuint8   flags;
   nuint8   nameSpace;
   nuint8   nameLength;
   nuint8   name [256];
   nuint32  creationDateAndTime;
   nuint32  ownerID;
   nuint32  lastArchiveDateAndTime;
   nuint32  lastArchiverID;
   nuint32  updateDateAndTime;
   nuint32  updatorID;
   nuint32  fileSize;
   nuint8   reserved[44];
   nuint16  inheritedRightsMask;
   nuint16  lastAccessDate;
   nuint32  deletedTime;
   nuint32  deletedDateAndTime;
   nuint32  deletorID;
   nuint8   reserved3 [16];
} __attribute__((packed)) NWDELETED_INFO;

int	ncp_AllocTempDirHandle(char *path, NWDIR_HANDLE *pdh);
int	ncp_DeallocateDirHandle(NWDIR_HANDLE dh);
int	ncp_GetNSEntryInfo(NWDIR_HANDLE dh, struct nw_entry_info *fi, int *ns);

NWCCODE	ncp_ScanNSEntryInfo(NWCONN_HANDLE cH, nuint8 namSpc, nuint16 attrs,
	SEARCH_SEQUENCE *seq, pnstr8 searchPattern, nuint32 retInfoMask, 
	NW_ENTRY_INFO *entryInfo);

NWCCODE ncp_PurgeDeletedFile(NWCONN_HANDLE cH, nuint32 iterHandle, 
	nuint32 volNum, nuint32 dirBase, nuint8 ns);

NWCCODE NWRecoverDeletedFile(NWCONN_HANDLE conn, NWDIR_HANDLE dirHandle,
	nuint32 iterHandle, 
	nuint32 volNum, nuint32 dirBase, 
	pnstr8  delFileName, pnstr8 rcvrFileName);

NWCCODE ncp_ScanForDeletedFiles(NWCONN_HANDLE cH, pnuint32 iterHandle, 
	pnuint32 volNum, pnuint32 dirBase, nuint8 ns,
	NWDELETED_INFO *entryInfo);


#ifdef __cplusplus
}
#endif

#endif /* _NCP_NCP_FILE_ */
