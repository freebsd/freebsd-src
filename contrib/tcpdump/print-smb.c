/*
 * Copyright (C) Andrew Tridgell 1995-1999
 *
 * This software may be distributed either under the terms of the
 * BSD-style license that accompanies tcpdump or the GNU GPL version 2
 * or later
 */

/* \summary: SMB/CIFS printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include <string.h>

#include "netdissect.h"
#include "extract.h"
#include "smb.h"


static int request = 0;
static int unicodestr = 0;

extern const u_char *startbuf;

const u_char *startbuf = NULL;

struct smbdescript {
    const char *req_f1;
    const char *req_f2;
    const char *rep_f1;
    const char *rep_f2;
    void (*fn)(netdissect_options *, const u_char *, const u_char *, const u_char *, const u_char *);
};

struct smbdescriptint {
    const char *req_f1;
    const char *req_f2;
    const char *rep_f1;
    const char *rep_f2;
    void (*fn)(netdissect_options *, const u_char *, const u_char *, u_int, u_int);
};

struct smbfns
{
    int id;
    const char *name;
    int flags;
    struct smbdescript descript;
};

struct smbfnsint
{
    int id;
    const char *name;
    int flags;
    struct smbdescriptint descript;
};

#define DEFDESCRIPT	{ NULL, NULL, NULL, NULL, NULL }

#define FLG_CHAIN	(1 << 0)

static const struct smbfns *
smbfind(int id, const struct smbfns *list)
{
    int sindex;

    for (sindex = 0; list[sindex].name; sindex++)
	if (list[sindex].id == id)
	    return(&list[sindex]);

    return(&list[0]);
}

static const struct smbfnsint *
smbfindint(int id, const struct smbfnsint *list)
{
    int sindex;

    for (sindex = 0; list[sindex].name; sindex++)
	if (list[sindex].id == id)
	    return(&list[sindex]);

    return(&list[0]);
}

static void
trans2_findfirst(netdissect_options *ndo,
                 const u_char *param, const u_char *data, u_int pcnt, u_int dcnt)
{
    const char *fmt;

    if (request)
	fmt = "Attribute=[A]\nSearchCount=[u]\nFlags=[w]\nLevel=[uP4]\nFile=[S]\n";
    else
	fmt = "Handle=[w]\nCount=[u]\nEOS=[w]\nEoffset=[u]\nLastNameOfs=[w]\n";

    smb_fdata(ndo, param, fmt, param + pcnt, unicodestr);
    if (dcnt) {
	ND_PRINT("data:\n");
	smb_data_print(ndo, data, dcnt);
    }
}

static void
trans2_qfsinfo(netdissect_options *ndo,
               const u_char *param, const u_char *data, u_int pcnt, u_int dcnt)
{
    static u_int level = 0;
    const char *fmt="";

    if (request) {
	level = GET_LE_U_2(param);
	fmt = "InfoLevel=[u]\n";
	smb_fdata(ndo, param, fmt, param + pcnt, unicodestr);
    } else {
	switch (level) {
	case 1:
	    fmt = "idFileSystem=[W]\nSectorUnit=[U]\nUnit=[U]\nAvail=[U]\nSectorSize=[u]\n";
	    break;
	case 2:
	    fmt = "CreationTime=[T2]VolNameLength=[lb]\nVolumeLabel=[c]\n";
	    break;
	case 0x105:
	    fmt = "Capabilities=[W]\nMaxFileLen=[U]\nVolNameLen=[lU]\nVolume=[C]\n";
	    break;
	default:
	    fmt = "UnknownLevel\n";
	    break;
	}
	smb_fdata(ndo, data, fmt, data + dcnt, unicodestr);
    }
    if (dcnt) {
	ND_PRINT("data:\n");
	smb_data_print(ndo, data, dcnt);
    }
}

static const struct smbfnsint trans2_fns[] = {
    { 0, "TRANSACT2_OPEN", 0,
	{ "Flags2=[w]\nMode=[w]\nSearchAttrib=[A]\nAttrib=[A]\nTime=[T2]\nOFun=[w]\nSize=[U]\nRes=([w, w, w, w, w])\nPath=[S]",
	  NULL,
	  "Handle=[u]\nAttrib=[A]\nTime=[T2]\nSize=[U]\nAccess=[w]\nType=[w]\nState=[w]\nAction=[w]\nInode=[W]\nOffErr=[u]\n|EALength=[u]\n",
	  NULL, NULL }},
    { 1, "TRANSACT2_FINDFIRST", 0,
	{ NULL, NULL, NULL, NULL, trans2_findfirst }},
    { 2, "TRANSACT2_FINDNEXT", 0, DEFDESCRIPT },
    { 3, "TRANSACT2_QFSINFO", 0,
	{ NULL, NULL, NULL, NULL, trans2_qfsinfo }},
    { 4, "TRANSACT2_SETFSINFO", 0, DEFDESCRIPT },
    { 5, "TRANSACT2_QPATHINFO", 0, DEFDESCRIPT },
    { 6, "TRANSACT2_SETPATHINFO", 0, DEFDESCRIPT },
    { 7, "TRANSACT2_QFILEINFO", 0, DEFDESCRIPT },
    { 8, "TRANSACT2_SETFILEINFO", 0, DEFDESCRIPT },
    { 9, "TRANSACT2_FSCTL", 0, DEFDESCRIPT },
    { 10, "TRANSACT2_IOCTL", 0, DEFDESCRIPT },
    { 11, "TRANSACT2_FINDNOTIFYFIRST", 0, DEFDESCRIPT },
    { 12, "TRANSACT2_FINDNOTIFYNEXT", 0, DEFDESCRIPT },
    { 13, "TRANSACT2_MKDIR", 0, DEFDESCRIPT },
    { -1, NULL, 0, DEFDESCRIPT }
};


static void
print_trans2(netdissect_options *ndo,
             const u_char *words, const u_char *dat, const u_char *buf, const u_char *maxbuf)
{
    u_int bcc;
    static const struct smbfnsint *fn = &trans2_fns[0];
    const u_char *data, *param;
    const u_char *w = words + 1;
    const char *f1 = NULL, *f2 = NULL;
    u_int pcnt, dcnt;

    ND_TCHECK_1(words);
    if (request) {
	ND_TCHECK_2(w + (14 * 2));
	pcnt = GET_LE_U_2(w + 9 * 2);
	param = buf + GET_LE_U_2(w + 10 * 2);
	dcnt = GET_LE_U_2(w + 11 * 2);
	data = buf + GET_LE_U_2(w + 12 * 2);
	fn = smbfindint(GET_LE_U_2(w + 14 * 2), trans2_fns);
    } else {
	if (GET_U_1(words) == 0) {
	    ND_PRINT("%s\n", fn->name);
	    ND_PRINT("Trans2Interim\n");
	    return;
	}
	ND_TCHECK_2(w + (7 * 2));
	pcnt = GET_LE_U_2(w + 3 * 2);
	param = buf + GET_LE_U_2(w + 4 * 2);
	dcnt = GET_LE_U_2(w + 6 * 2);
	data = buf + GET_LE_U_2(w + 7 * 2);
    }

    ND_PRINT("%s param_length=%u data_length=%u\n", fn->name, pcnt, dcnt);

    if (request) {
	if (GET_U_1(words) == 8) {
	    smb_fdata(ndo, words + 1,
		"Trans2Secondary\nTotParam=[u]\nTotData=[u]\nParamCnt=[u]\nParamOff=[u]\nParamDisp=[u]\nDataCnt=[u]\nDataOff=[u]\nDataDisp=[u]\nHandle=[u]\n",
		maxbuf, unicodestr);
	    return;
	} else {
	    smb_fdata(ndo, words + 1,
		"TotParam=[u]\nTotData=[u]\nMaxParam=[u]\nMaxData=[u]\nMaxSetup=[b][P1]\nFlags=[w]\nTimeOut=[D]\nRes1=[w]\nParamCnt=[u]\nParamOff=[u]\nDataCnt=[u]\nDataOff=[u]\nSetupCnt=[b][P1]\n",
		words + 1 + 14 * 2, unicodestr);
	}
	f1 = fn->descript.req_f1;
	f2 = fn->descript.req_f2;
    } else {
	smb_fdata(ndo, words + 1,
	    "TotParam=[u]\nTotData=[u]\nRes1=[w]\nParamCnt=[u]\nParamOff=[u]\nParamDisp[u]\nDataCnt=[u]\nDataOff=[u]\nDataDisp=[u]\nSetupCnt=[b][P1]\n",
	    words + 1 + 10 * 2, unicodestr);
	f1 = fn->descript.rep_f1;
	f2 = fn->descript.rep_f2;
    }

    bcc = GET_LE_U_2(dat);
    ND_PRINT("smb_bcc=%u\n", bcc);
    if (fn->descript.fn)
	(*fn->descript.fn)(ndo, param, data, pcnt, dcnt);
    else {
	smb_fdata(ndo, param, f1 ? f1 : "Parameters=\n", param + pcnt, unicodestr);
	smb_fdata(ndo, data, f2 ? f2 : "Data=\n", data + dcnt, unicodestr);
    }
    return;
trunc:
    nd_print_trunc(ndo);
}

static void
print_browse(netdissect_options *ndo,
             const u_char *param, u_int paramlen, const u_char *data, u_int datalen)
{
    const u_char *maxbuf = data + datalen;
    u_int command;

    command = GET_U_1(data);

    smb_fdata(ndo, param, "BROWSE PACKET\n|Param ", param+paramlen, unicodestr);

    switch (command) {
    case 0xF:
	data = smb_fdata(ndo, data,
	    "BROWSE PACKET:\nType=[B] (LocalMasterAnnouncement)\nUpdateCount=[w]\nRes1=[B]\nAnnounceInterval=[u]\nName=[n2]\nMajorVersion=[B]\nMinorVersion=[B]\nServerType=[W]\nElectionVersion=[w]\nBrowserConstant=[w]\n",
	    maxbuf, unicodestr);
	break;

    case 0x1:
	data = smb_fdata(ndo, data,
	    "BROWSE PACKET:\nType=[B] (HostAnnouncement)\nUpdateCount=[w]\nRes1=[B]\nAnnounceInterval=[u]\nName=[n2]\nMajorVersion=[B]\nMinorVersion=[B]\nServerType=[W]\nElectionVersion=[w]\nBrowserConstant=[w]\n",
	    maxbuf, unicodestr);
	break;

    case 0x2:
	data = smb_fdata(ndo, data,
	    "BROWSE PACKET:\nType=[B] (AnnouncementRequest)\nFlags=[B]\nReplySystemName=[S]\n",
	    maxbuf, unicodestr);
	break;

    case 0xc:
	data = smb_fdata(ndo, data,
	    "BROWSE PACKET:\nType=[B] (WorkgroupAnnouncement)\nUpdateCount=[w]\nRes1=[B]\nAnnounceInterval=[u]\nName=[n2]\nMajorVersion=[B]\nMinorVersion=[B]\nServerType=[W]\nCommentPointer=[W]\nServerName=[S]\n",
	    maxbuf, unicodestr);
	break;

    case 0x8:
	data = smb_fdata(ndo, data,
	    "BROWSE PACKET:\nType=[B] (ElectionFrame)\nElectionVersion=[B]\nOSSummary=[W]\nUptime=[(W, W)]\nServerName=[S]\n",
	    maxbuf, unicodestr);
	break;

    case 0xb:
	data = smb_fdata(ndo, data,
	    "BROWSE PACKET:\nType=[B] (BecomeBackupBrowser)\nName=[S]\n",
	    maxbuf, unicodestr);
	break;

    case 0x9:
	data = smb_fdata(ndo, data,
	    "BROWSE PACKET:\nType=[B] (GetBackupList)\nListCount?=[B]\nToken=[W]\n",
	    maxbuf, unicodestr);
	break;

    case 0xa:
	data = smb_fdata(ndo, data,
	    "BROWSE PACKET:\nType=[B] (BackupListResponse)\nServerCount?=[B]\nToken=[W]\n*Name=[S]\n",
	    maxbuf, unicodestr);
	break;

    case 0xd:
	data = smb_fdata(ndo, data,
	    "BROWSE PACKET:\nType=[B] (MasterAnnouncement)\nMasterName=[S]\n",
	    maxbuf, unicodestr);
	break;

    case 0xe:
	data = smb_fdata(ndo, data,
	    "BROWSE PACKET:\nType=[B] (ResetBrowser)\nOptions=[B]\n", maxbuf, unicodestr);
	break;

    default:
	data = smb_fdata(ndo, data, "Unknown Browser Frame ", maxbuf, unicodestr);
	break;
    }
}


static void
print_ipc(netdissect_options *ndo,
          const u_char *param, u_int paramlen, const u_char *data, u_int datalen)
{
    if (paramlen)
	smb_fdata(ndo, param, "Command=[w]\nStr1=[S]\nStr2=[S]\n", param + paramlen,
	    unicodestr);
    if (datalen)
	smb_fdata(ndo, data, "IPC ", data + datalen, unicodestr);
}


static void
print_trans(netdissect_options *ndo,
            const u_char *words, const u_char *data1, const u_char *buf, const u_char *maxbuf)
{
    u_int bcc;
    const char *f1, *f2, *f3, *f4;
    const u_char *data, *param;
    const u_char *w = words + 1;
    u_int datalen, paramlen;

    if (request) {
	ND_TCHECK_2(w + (12 * 2));
	paramlen = GET_LE_U_2(w + 9 * 2);
	param = buf + GET_LE_U_2(w + 10 * 2);
	datalen = GET_LE_U_2(w + 11 * 2);
	data = buf + GET_LE_U_2(w + 12 * 2);
	f1 = "TotParamCnt=[u]\nTotDataCnt=[u]\nMaxParmCnt=[u]\nMaxDataCnt=[u]\nMaxSCnt=[u]\nTransFlags=[w]\nRes1=[w]\nRes2=[w]\nRes3=[w]\nParamCnt=[u]\nParamOff=[u]\nDataCnt=[u]\nDataOff=[u]\nSUCnt=[u]\n";
	f2 = "|Name=[S]\n";
	f3 = "|Param ";
	f4 = "|Data ";
    } else {
	ND_TCHECK_2(w + (7 * 2));
	paramlen = GET_LE_U_2(w + 3 * 2);
	param = buf + GET_LE_U_2(w + 4 * 2);
	datalen = GET_LE_U_2(w + 6 * 2);
	data = buf + GET_LE_U_2(w + 7 * 2);
	f1 = "TotParamCnt=[u]\nTotDataCnt=[u]\nRes1=[u]\nParamCnt=[u]\nParamOff=[u]\nRes2=[u]\nDataCnt=[u]\nDataOff=[u]\nRes3=[u]\nLsetup=[u]\n";
	f2 = "|Unknown ";
	f3 = "|Param ";
	f4 = "|Data ";
    }

    smb_fdata(ndo, words + 1, f1,
              ND_MIN(words + 1 + 2 * GET_U_1(words), maxbuf),
              unicodestr);

    bcc = GET_LE_U_2(data1);
    ND_PRINT("smb_bcc=%u\n", bcc);
    if (bcc > 0) {
	smb_fdata(ndo, data1 + 2, f2, maxbuf - (paramlen + datalen), unicodestr);

#define MAILSLOT_BROWSE_STR "\\MAILSLOT\\BROWSE"
	ND_TCHECK_LEN(data1 + 2, strlen(MAILSLOT_BROWSE_STR) + 1);
	if (strcmp((const char *)(data1 + 2), MAILSLOT_BROWSE_STR) == 0) {
	    print_browse(ndo, param, paramlen, data, datalen);
	    return;
	}
#undef MAILSLOT_BROWSE_STR

#define PIPE_LANMAN_STR "\\PIPE\\LANMAN"
	ND_TCHECK_LEN(data1 + 2, strlen(PIPE_LANMAN_STR) + 1);
	if (strcmp((const char *)(data1 + 2), PIPE_LANMAN_STR) == 0) {
	    print_ipc(ndo, param, paramlen, data, datalen);
	    return;
	}
#undef PIPE_LANMAN_STR

	if (paramlen)
	    smb_fdata(ndo, param, f3, ND_MIN(param + paramlen, maxbuf), unicodestr);
	if (datalen)
	    smb_fdata(ndo, data, f4, ND_MIN(data + datalen, maxbuf), unicodestr);
    }
    return;
trunc:
    nd_print_trunc(ndo);
}


static void
print_negprot(netdissect_options *ndo,
              const u_char *words, const u_char *data, const u_char *buf _U_, const u_char *maxbuf)
{
    u_int wct, bcc;
    const char *f1 = NULL, *f2 = NULL;

    wct = GET_U_1(words);
    if (request)
	f2 = "*|Dialect=[Y]\n";
    else {
	if (wct == 1)
	    f1 = "Core Protocol\nDialectIndex=[u]";
	else if (wct == 17)
	    f1 = "NT1 Protocol\nDialectIndex=[u]\nSecMode=[B]\nMaxMux=[u]\nNumVcs=[u]\nMaxBuffer=[U]\nRawSize=[U]\nSessionKey=[W]\nCapabilities=[W]\nServerTime=[T3]TimeZone=[u]\nCryptKey=";
	else if (wct == 13)
	    f1 = "Coreplus/Lanman1/Lanman2 Protocol\nDialectIndex=[u]\nSecMode=[w]\nMaxXMit=[u]\nMaxMux=[u]\nMaxVcs=[u]\nBlkMode=[w]\nSessionKey=[W]\nServerTime=[T1]TimeZone=[u]\nRes=[W]\nCryptKey=";
    }

    if (f1)
	smb_fdata(ndo, words + 1, f1, ND_MIN(words + 1 + wct * 2, maxbuf),
	    unicodestr);
    else
	smb_data_print(ndo, words + 1, ND_MIN(wct * 2, ND_BYTES_BETWEEN(maxbuf, words + 1)));

    bcc = GET_LE_U_2(data);
    ND_PRINT("smb_bcc=%u\n", bcc);
    if (bcc > 0) {
	if (f2)
	    smb_fdata(ndo, data + 2, f2, ND_MIN(data + 2 + GET_LE_U_2(data),
                                             maxbuf), unicodestr);
	else
	    smb_data_print(ndo, data + 2,
                           ND_MIN(GET_LE_U_2(data), ND_BYTES_BETWEEN(maxbuf, data + 2)));
    }
}

static void
print_sesssetup(netdissect_options *ndo,
                const u_char *words, const u_char *data, const u_char *buf _U_, const u_char *maxbuf)
{
    u_int wct, bcc;
    const char *f1 = NULL, *f2 = NULL;

    wct = GET_U_1(words);
    if (request) {
	if (wct == 10)
	    f1 = "Com2=[w]\nOff2=[u]\nBufSize=[u]\nMpxMax=[u]\nVcNum=[u]\nSessionKey=[W]\nPassLen=[u]\nCryptLen=[u]\nCryptOff=[u]\nPass&Name=\n";
	else
	    f1 = "Com2=[B]\nRes1=[B]\nOff2=[u]\nMaxBuffer=[u]\nMaxMpx=[u]\nVcNumber=[u]\nSessionKey=[W]\nCaseInsensitivePasswordLength=[u]\nCaseSensitivePasswordLength=[u]\nRes=[W]\nCapabilities=[W]\nPass1&Pass2&Account&Domain&OS&LanMan=\n";
    } else {
	if (wct == 3) {
	    f1 = "Com2=[w]\nOff2=[u]\nAction=[w]\n";
	} else if (wct == 13) {
	    f1 = "Com2=[B]\nRes=[B]\nOff2=[u]\nAction=[w]\n";
	    f2 = "NativeOS=[S]\nNativeLanMan=[S]\nPrimaryDomain=[S]\n";
	}
    }

    if (f1)
	smb_fdata(ndo, words + 1, f1, ND_MIN(words + 1 + wct * 2, maxbuf),
	    unicodestr);
    else
	smb_data_print(ndo, words + 1, ND_MIN(wct * 2, ND_BYTES_BETWEEN(maxbuf, words + 1)));

    bcc = GET_LE_U_2(data);
    ND_PRINT("smb_bcc=%u\n", bcc);
    if (bcc > 0) {
	if (f2)
	    smb_fdata(ndo, data + 2, f2, ND_MIN(data + 2 + GET_LE_U_2(data),
                                             maxbuf), unicodestr);
	else
	    smb_data_print(ndo, data + 2,
                           ND_MIN(GET_LE_U_2(data), ND_BYTES_BETWEEN(maxbuf, data + 2)));
    }
}

static void
print_lockingandx(netdissect_options *ndo,
                  const u_char *words, const u_char *data, const u_char *buf _U_, const u_char *maxbuf)
{
    u_int wct, bcc;
    const u_char *maxwords;
    const char *f1 = NULL, *f2 = NULL;

    wct = GET_U_1(words);
    if (request) {
	f1 = "Com2=[w]\nOff2=[u]\nHandle=[u]\nLockType=[w]\nTimeOut=[D]\nUnlockCount=[u]\nLockCount=[u]\n";
	if (GET_U_1(words + 7) & 0x10)
	    f2 = "*Process=[u]\n[P2]Offset=[M]\nLength=[M]\n";
	else
	    f2 = "*Process=[u]\nOffset=[D]\nLength=[U]\n";
    } else {
	f1 = "Com2=[w]\nOff2=[u]\n";
    }

    maxwords = ND_MIN(words + 1 + wct * 2, maxbuf);
    if (wct)
	smb_fdata(ndo, words + 1, f1, maxwords, unicodestr);

    bcc = GET_LE_U_2(data);
    ND_PRINT("smb_bcc=%u\n", bcc);
    if (bcc > 0) {
	if (f2)
	    smb_fdata(ndo, data + 2, f2, ND_MIN(data + 2 + GET_LE_U_2(data),
                                             maxbuf), unicodestr);
	else
	    smb_data_print(ndo, data + 2,
                           ND_MIN(GET_LE_U_2(data), ND_BYTES_BETWEEN(maxbuf, data + 2)));
    }
}


static const struct smbfns smb_fns[] = {
    { -1, "SMBunknown", 0, DEFDESCRIPT },

    { SMBtcon, "SMBtcon", 0,
	{ NULL, "Path=[Z]\nPassword=[Z]\nDevice=[Z]\n",
	  "MaxXmit=[u]\nTreeId=[u]\n", NULL,
	  NULL } },

    { SMBtdis, "SMBtdis", 0, DEFDESCRIPT },
    { SMBexit,  "SMBexit", 0, DEFDESCRIPT },
    { SMBioctl, "SMBioctl", 0, DEFDESCRIPT },

    { SMBecho, "SMBecho", 0,
	{ "ReverbCount=[u]\n", NULL,
	  "SequenceNum=[u]\n", NULL,
	  NULL } },

    { SMBulogoffX, "SMBulogoffX", FLG_CHAIN, DEFDESCRIPT },

    { SMBgetatr, "SMBgetatr", 0,
	{ NULL, "Path=[Z]\n",
	  "Attribute=[A]\nTime=[T2]Size=[U]\nRes=([w,w,w,w,w])\n", NULL,
	  NULL } },

    { SMBsetatr, "SMBsetatr", 0,
	{ "Attribute=[A]\nTime=[T2]Res=([w,w,w,w,w])\n", "Path=[Z]\n",
	  NULL, NULL, NULL } },

    { SMBchkpth, "SMBchkpth", 0,
       { NULL, "Path=[Z]\n", NULL, NULL, NULL } },

    { SMBsearch, "SMBsearch", 0,
	{ "Count=[u]\nAttrib=[A]\n",
	  "Path=[Z]\nBlkType=[B]\nBlkLen=[u]\n|Res1=[B]\nMask=[s11]\nSrv1=[B]\nDirIndex=[u]\nSrv2=[w]\nRes2=[W]\n",
	  "Count=[u]\n",
	  "BlkType=[B]\nBlkLen=[u]\n*\nRes1=[B]\nMask=[s11]\nSrv1=[B]\nDirIndex=[u]\nSrv2=[w]\nRes2=[W]\nAttrib=[a]\nTime=[T1]Size=[U]\nName=[s13]\n",
	  NULL } },

    { SMBopen, "SMBopen", 0,
	{ "Mode=[w]\nAttribute=[A]\n", "Path=[Z]\n",
	  "Handle=[u]\nOAttrib=[A]\nTime=[T2]Size=[U]\nAccess=[w]\n",
	  NULL, NULL } },

    { SMBcreate, "SMBcreate", 0,
	{ "Attrib=[A]\nTime=[T2]", "Path=[Z]\n", "Handle=[u]\n", NULL, NULL } },

    { SMBmknew, "SMBmknew", 0,
	{ "Attrib=[A]\nTime=[T2]", "Path=[Z]\n", "Handle=[u]\n", NULL, NULL } },

    { SMBunlink, "SMBunlink", 0,
	{ "Attrib=[A]\n", "Path=[Z]\n", NULL, NULL, NULL } },

    { SMBread, "SMBread", 0,
	{ "Handle=[u]\nByteCount=[u]\nOffset=[D]\nCountLeft=[u]\n", NULL,
	  "Count=[u]\nRes=([w,w,w,w])\n", NULL, NULL } },

    { SMBwrite, "SMBwrite", 0,
	{ "Handle=[u]\nByteCount=[u]\nOffset=[D]\nCountLeft=[u]\n", NULL,
	  "Count=[u]\n", NULL, NULL } },

    { SMBclose, "SMBclose", 0,
	{ "Handle=[u]\nTime=[T2]", NULL, NULL, NULL, NULL } },

    { SMBmkdir, "SMBmkdir", 0,
	{ NULL, "Path=[Z]\n", NULL, NULL, NULL } },

    { SMBrmdir, "SMBrmdir", 0,
	{ NULL, "Path=[Z]\n", NULL, NULL, NULL } },

    { SMBdskattr, "SMBdskattr", 0,
	{ NULL, NULL,
	  "TotalUnits=[u]\nBlocksPerUnit=[u]\nBlockSize=[u]\nFreeUnits=[u]\nMedia=[w]\n",
	  NULL, NULL } },

    { SMBmv, "SMBmv", 0,
	{ "Attrib=[A]\n", "OldPath=[Z]\nNewPath=[Z]\n", NULL, NULL, NULL } },

    /*
     * this is a Pathworks specific call, allowing the
     * changing of the root path
     */
    { pSETDIR, "SMBsetdir", 0, { NULL, "Path=[Z]\n", NULL, NULL, NULL } },

    { SMBlseek, "SMBlseek", 0,
	{ "Handle=[u]\nMode=[w]\nOffset=[D]\n", "Offset=[D]\n", NULL, NULL, NULL } },

    { SMBflush, "SMBflush", 0, { "Handle=[u]\n", NULL, NULL, NULL, NULL } },

    { SMBsplopen, "SMBsplopen", 0,
	{ "SetupLen=[u]\nMode=[w]\n", "Ident=[Z]\n", "Handle=[u]\n",
	  NULL, NULL } },

    { SMBsplclose, "SMBsplclose", 0,
	{ "Handle=[u]\n", NULL, NULL, NULL, NULL } },

    { SMBsplretq, "SMBsplretq", 0,
	{ "MaxCount=[u]\nStartIndex=[u]\n", NULL,
	  "Count=[u]\nIndex=[u]\n",
	  "*Time=[T2]Status=[B]\nJobID=[u]\nSize=[U]\nRes=[B]Name=[s16]\n",
	  NULL } },

    { SMBsplwr, "SMBsplwr", 0,
	{ "Handle=[u]\n", NULL, NULL, NULL, NULL } },

    { SMBlock, "SMBlock", 0,
	{ "Handle=[u]\nCount=[U]\nOffset=[D]\n", NULL, NULL, NULL, NULL } },

    { SMBunlock, "SMBunlock", 0,
	{ "Handle=[u]\nCount=[U]\nOffset=[D]\n", NULL, NULL, NULL, NULL } },

    /* CORE+ PROTOCOL FOLLOWS */

    { SMBreadbraw, "SMBreadbraw", 0,
	{ "Handle=[u]\nOffset=[D]\nMaxCount=[u]\nMinCount=[u]\nTimeOut=[D]\nRes=[u]\n",
	  NULL, NULL, NULL, NULL } },

    { SMBwritebraw, "SMBwritebraw", 0,
	{ "Handle=[u]\nTotalCount=[u]\nRes=[w]\nOffset=[D]\nTimeOut=[D]\nWMode=[w]\nRes2=[W]\n|DataSize=[u]\nDataOff=[u]\n",
	  NULL, "WriteRawAck", NULL, NULL } },

    { SMBwritec, "SMBwritec", 0,
	{ NULL, NULL, "Count=[u]\n", NULL, NULL } },

    { SMBwriteclose, "SMBwriteclose", 0,
	{ "Handle=[u]\nCount=[u]\nOffset=[D]\nTime=[T2]Res=([w,w,w,w,w,w])",
	  NULL, "Count=[u]\n", NULL, NULL } },

    { SMBlockread, "SMBlockread", 0,
	{ "Handle=[u]\nByteCount=[u]\nOffset=[D]\nCountLeft=[u]\n", NULL,
	  "Count=[u]\nRes=([w,w,w,w])\n", NULL, NULL } },

    { SMBwriteunlock, "SMBwriteunlock", 0,
	{ "Handle=[u]\nByteCount=[u]\nOffset=[D]\nCountLeft=[u]\n", NULL,
	  "Count=[u]\n", NULL, NULL } },

    { SMBreadBmpx, "SMBreadBmpx", 0,
	{ "Handle=[u]\nOffset=[D]\nMaxCount=[u]\nMinCount=[u]\nTimeOut=[D]\nRes=[w]\n",
	  NULL,
	  "Offset=[D]\nTotCount=[u]\nRemaining=[u]\nRes=([w,w])\nDataSize=[u]\nDataOff=[u]\n",
	  NULL, NULL } },

    { SMBwriteBmpx, "SMBwriteBmpx", 0,
	{ "Handle=[u]\nTotCount=[u]\nRes=[w]\nOffset=[D]\nTimeOut=[D]\nWMode=[w]\nRes2=[W]\nDataSize=[u]\nDataOff=[u]\n", NULL,
	  "Remaining=[u]\n", NULL, NULL } },

    { SMBwriteBs, "SMBwriteBs", 0,
	{ "Handle=[u]\nTotCount=[u]\nOffset=[D]\nRes=[W]\nDataSize=[u]\nDataOff=[u]\n",
	  NULL, "Count=[u]\n", NULL, NULL } },

    { SMBsetattrE, "SMBsetattrE", 0,
	{ "Handle=[u]\nCreationTime=[T2]AccessTime=[T2]ModifyTime=[T2]", NULL,
	  NULL, NULL, NULL } },

    { SMBgetattrE, "SMBgetattrE", 0,
	{ "Handle=[u]\n", NULL,
	  "CreationTime=[T2]AccessTime=[T2]ModifyTime=[T2]Size=[U]\nAllocSize=[U]\nAttribute=[A]\n",
	  NULL, NULL } },

    { SMBtranss, "SMBtranss", 0, DEFDESCRIPT },
    { SMBioctls, "SMBioctls", 0, DEFDESCRIPT },

    { SMBcopy, "SMBcopy", 0,
	{ "TreeID2=[u]\nOFun=[w]\nFlags=[w]\n", "Path=[S]\nNewPath=[S]\n",
	  "CopyCount=[u]\n",  "|ErrStr=[S]\n",  NULL } },

    { SMBmove, "SMBmove", 0,
	{ "TreeID2=[u]\nOFun=[w]\nFlags=[w]\n", "Path=[S]\nNewPath=[S]\n",
	  "MoveCount=[u]\n",  "|ErrStr=[S]\n",  NULL } },

    { SMBopenX, "SMBopenX", FLG_CHAIN,
	{ "Com2=[w]\nOff2=[u]\nFlags=[w]\nMode=[w]\nSearchAttrib=[A]\nAttrib=[A]\nTime=[T2]OFun=[w]\nSize=[U]\nTimeOut=[D]\nRes=[W]\n",
	  "Path=[S]\n",
	  "Com2=[w]\nOff2=[u]\nHandle=[u]\nAttrib=[A]\nTime=[T2]Size=[U]\nAccess=[w]\nType=[w]\nState=[w]\nAction=[w]\nFileID=[W]\nRes=[w]\n",
	  NULL, NULL } },

    { SMBreadX, "SMBreadX", FLG_CHAIN,
	{ "Com2=[w]\nOff2=[u]\nHandle=[u]\nOffset=[D]\nMaxCount=[u]\nMinCount=[u]\nTimeOut=[D]\nCountLeft=[u]\n",
	  NULL,
	  "Com2=[w]\nOff2=[u]\nRemaining=[u]\nRes=[W]\nDataSize=[u]\nDataOff=[u]\nRes=([w,w,w,w])\n",
	  NULL, NULL } },

    { SMBwriteX, "SMBwriteX", FLG_CHAIN,
	{ "Com2=[w]\nOff2=[u]\nHandle=[u]\nOffset=[D]\nTimeOut=[D]\nWMode=[w]\nCountLeft=[u]\nRes=[w]\nDataSize=[u]\nDataOff=[u]\n",
	  NULL,
	  "Com2=[w]\nOff2=[u]\nCount=[u]\nRemaining=[u]\nRes=[W]\n",
	  NULL, NULL } },

    { SMBffirst, "SMBffirst", 0,
	{ "Count=[u]\nAttrib=[A]\n",
	  "Path=[Z]\nBlkType=[B]\nBlkLen=[u]\n|Res1=[B]\nMask=[s11]\nSrv1=[B]\nDirIndex=[u]\nSrv2=[w]\n",
	  "Count=[u]\n",
	  "BlkType=[B]\nBlkLen=[u]\n*\nRes1=[B]\nMask=[s11]\nSrv1=[B]\nDirIndex=[u]\nSrv2=[w]\nRes2=[W]\nAttrib=[a]\nTime=[T1]Size=[U]\nName=[s13]\n",
	  NULL } },

    { SMBfunique, "SMBfunique", 0,
	{ "Count=[u]\nAttrib=[A]\n",
	  "Path=[Z]\nBlkType=[B]\nBlkLen=[u]\n|Res1=[B]\nMask=[s11]\nSrv1=[B]\nDirIndex=[u]\nSrv2=[w]\n",
	  "Count=[u]\n",
	  "BlkType=[B]\nBlkLen=[u]\n*\nRes1=[B]\nMask=[s11]\nSrv1=[B]\nDirIndex=[u]\nSrv2=[w]\nRes2=[W]\nAttrib=[a]\nTime=[T1]Size=[U]\nName=[s13]\n",
	  NULL } },

    { SMBfclose, "SMBfclose", 0,
	{ "Count=[u]\nAttrib=[A]\n",
	  "Path=[Z]\nBlkType=[B]\nBlkLen=[u]\n|Res1=[B]\nMask=[s11]\nSrv1=[B]\nDirIndex=[u]\nSrv2=[w]\n",
	  "Count=[u]\n",
	  "BlkType=[B]\nBlkLen=[u]\n*\nRes1=[B]\nMask=[s11]\nSrv1=[B]\nDirIndex=[u]\nSrv2=[w]\nRes2=[W]\nAttrib=[a]\nTime=[T1]Size=[U]\nName=[s13]\n",
	  NULL } },

    { SMBfindnclose, "SMBfindnclose", 0,
	{ "Handle=[u]\n", NULL, NULL, NULL, NULL } },

    { SMBfindclose, "SMBfindclose", 0,
	{ "Handle=[u]\n", NULL, NULL, NULL, NULL } },

    { SMBsends, "SMBsends", 0,
	{ NULL, "Source=[Z]\nDest=[Z]\n", NULL, NULL, NULL } },

    { SMBsendstrt, "SMBsendstrt", 0,
	{ NULL, "Source=[Z]\nDest=[Z]\n", "GroupID=[u]\n", NULL, NULL } },

    { SMBsendend, "SMBsendend", 0,
	{ "GroupID=[u]\n", NULL, NULL, NULL, NULL } },

    { SMBsendtxt, "SMBsendtxt", 0,
	{ "GroupID=[u]\n", NULL, NULL, NULL, NULL } },

    { SMBsendb, "SMBsendb", 0,
	{ NULL, "Source=[Z]\nDest=[Z]\n", NULL, NULL, NULL } },

    { SMBfwdname, "SMBfwdname", 0, DEFDESCRIPT },
    { SMBcancelf, "SMBcancelf", 0, DEFDESCRIPT },
    { SMBgetmac, "SMBgetmac", 0, DEFDESCRIPT },

    { SMBnegprot, "SMBnegprot", 0,
	{ NULL, NULL, NULL, NULL, print_negprot } },

    { SMBsesssetupX, "SMBsesssetupX", FLG_CHAIN,
	{ NULL, NULL, NULL, NULL, print_sesssetup } },

    { SMBtconX, "SMBtconX", FLG_CHAIN,
	{ "Com2=[w]\nOff2=[u]\nFlags=[w]\nPassLen=[u]\nPasswd&Path&Device=\n",
	  NULL, "Com2=[w]\nOff2=[u]\n", "ServiceType=[R]\n", NULL } },

    { SMBlockingX, "SMBlockingX", FLG_CHAIN,
	{ NULL, NULL, NULL, NULL, print_lockingandx } },

    { SMBtrans2, "SMBtrans2", 0, { NULL, NULL, NULL, NULL, print_trans2 } },

    { SMBtranss2, "SMBtranss2", 0, DEFDESCRIPT },
    { SMBctemp, "SMBctemp", 0, DEFDESCRIPT },
    { SMBreadBs, "SMBreadBs", 0, DEFDESCRIPT },
    { SMBtrans, "SMBtrans", 0, { NULL, NULL, NULL, NULL, print_trans } },

    { SMBnttrans, "SMBnttrans", 0, DEFDESCRIPT },
    { SMBnttranss, "SMBnttranss", 0, DEFDESCRIPT },

    { SMBntcreateX, "SMBntcreateX", FLG_CHAIN,
	{ "Com2=[w]\nOff2=[u]\nRes=[b]\nNameLen=[lu]\nFlags=[W]\nRootDirectoryFid=[U]\nAccessMask=[W]\nAllocationSize=[L]\nExtFileAttributes=[W]\nShareAccess=[W]\nCreateDisposition=[W]\nCreateOptions=[W]\nImpersonationLevel=[W]\nSecurityFlags=[b]\n",
	  "Path=[C]\n",
	  "Com2=[w]\nOff2=[u]\nOplockLevel=[b]\nFid=[u]\nCreateAction=[W]\nCreateTime=[T3]LastAccessTime=[T3]LastWriteTime=[T3]ChangeTime=[T3]ExtFileAttributes=[W]\nAllocationSize=[L]\nEndOfFile=[L]\nFileType=[w]\nDeviceState=[w]\nDirectory=[b]\n",
	  NULL, NULL } },

    { SMBntcancel, "SMBntcancel", 0, DEFDESCRIPT },

    { -1, NULL, 0, DEFDESCRIPT }
};


/*
 * print a SMB message
 */
static void
print_smb(netdissect_options *ndo,
          const u_char *buf, const u_char *maxbuf)
{
    uint16_t flags2;
    u_int nterrcodes;
    u_int command;
    uint32_t nterror;
    const u_char *words, *maxwords, *data;
    const struct smbfns *fn;
    const char *fmt_smbheader =
        "[P4]SMB Command   =  [B]\nError class   =  [BP1]\nError code    =  [u]\nFlags1        =  [B]\nFlags2        =  [B][P13]\nTree ID       =  [u]\nProc ID       =  [u]\nUID           =  [u]\nMID           =  [u]\nWord Count    =  [b]\n";
    u_int smboffset;

    ndo->ndo_protocol = "smb";

    request = (GET_U_1(buf + 9) & 0x80) ? 0 : 1;
    startbuf = buf;

    command = GET_U_1(buf + 4);

    fn = smbfind(command, smb_fns);

    if (ndo->ndo_vflag > 1)
	ND_PRINT("\n");

    ND_PRINT("SMB PACKET: %s (%s)", fn->name, request ? "REQUEST" : "REPLY");

    if (ndo->ndo_vflag < 2)
	return;

    ND_PRINT("\n");
    flags2 = GET_LE_U_2(buf + 10);
    unicodestr = flags2 & 0x8000;
    nterrcodes = flags2 & 0x4000;

    /* print out the header */
    smb_fdata(ndo, buf, fmt_smbheader, buf + 33, unicodestr);

    if (nterrcodes) {
	nterror = GET_LE_U_4(buf + 5);
	if (nterror)
	    ND_PRINT("NTError = %s\n", nt_errstr(nterror));
    } else {
	if (GET_U_1(buf + 5))
	    ND_PRINT("SMBError = %s\n", smb_errstr(GET_U_1(buf + 5),
                                                   GET_LE_U_2(buf + 7)));
    }

    smboffset = 32;

    for (;;) {
	const char *f1, *f2;
	int wct;
	u_int bcc;
	u_int newsmboffset;

	words = buf + smboffset;
	wct = GET_U_1(words);
	data = words + 1 + wct * 2;
	maxwords = ND_MIN(data, maxbuf);

	if (request) {
	    f1 = fn->descript.req_f1;
	    f2 = fn->descript.req_f2;
	} else {
	    f1 = fn->descript.rep_f1;
	    f2 = fn->descript.rep_f2;
	}

	smb_reset();
	if (fn->descript.fn)
	    (*fn->descript.fn)(ndo, words, data, buf, maxbuf);
	else {
	    if (wct) {
		if (f1)
		    smb_fdata(ndo, words + 1, f1, words + 1 + wct * 2, unicodestr);
		else {
		    u_int i;
		    u_int v;

		    for (i = 0; words + 1 + 2 * i < maxwords; i++) {
			v = GET_LE_U_2(words + 1 + 2 * i);
			ND_PRINT("smb_vwv[%u]=%u (0x%X)\n", i, v, v);
		    }
		}
	    }

	    bcc = GET_LE_U_2(data);
	    ND_PRINT("smb_bcc=%u\n", bcc);
	    if (f2) {
		if (bcc > 0)
		    smb_fdata(ndo, data + 2, f2, data + 2 + bcc, unicodestr);
	    } else {
		if (bcc > 0) {
		    ND_PRINT("smb_buf[]=\n");
		    smb_data_print(ndo, data + 2, ND_MIN(bcc, ND_BYTES_BETWEEN(maxbuf, data + 2)));
		}
	    }
	}

	if ((fn->flags & FLG_CHAIN) == 0)
	    break;
	if (wct == 0)
	    break;
	command = GET_U_1(words + 1);
	if (command == 0xFF)
	    break;
	newsmboffset = GET_LE_U_2(words + 3);

	fn = smbfind(command, smb_fns);

	ND_PRINT("\nSMB PACKET: %s (%s) (CHAINED)\n",
	    fn->name, request ? "REQUEST" : "REPLY");
	if (newsmboffset <= smboffset) {
	    ND_PRINT("Bad andX offset: %u <= %u\n", newsmboffset, smboffset);
	    break;
	}
	smboffset = newsmboffset;
    }
}


/*
 * print a NBT packet received across tcp on port 139
 */
void
nbt_tcp_print(netdissect_options *ndo,
              const u_char *data, u_int length)
{
    u_int caplen;
    u_int type;
    u_int nbt_len;
    const u_char *maxbuf;

    ndo->ndo_protocol = "nbt_tcp";
    if (length < 4)
	goto trunc;
    if (ndo->ndo_snapend < data)
	goto trunc;
    caplen = ND_BYTES_AVAILABLE_AFTER(data);
    if (caplen < 4)
	goto trunc;
    maxbuf = data + caplen;
    type = GET_U_1(data);
    nbt_len = GET_BE_U_2(data + 2);
    length -= 4;
    caplen -= 4;

    startbuf = data;

    if (ndo->ndo_vflag < 2) {
	ND_PRINT(" NBT Session Packet: ");
	switch (type) {
	case 0x00:
	    ND_PRINT("Session Message");
	    break;

	case 0x81:
	    ND_PRINT("Session Request");
	    break;

	case 0x82:
	    ND_PRINT("Session Granted");
	    break;

	case 0x83:
	  {
	    u_int ecode;

	    if (nbt_len < 4)
		goto trunc;
	    if (length < 4)
		goto trunc;
	    if (caplen < 4)
		goto trunc;
	    ecode = GET_U_1(data + 4);

	    ND_PRINT("Session Reject, ");
	    switch (ecode) {
	    case 0x80:
		ND_PRINT("Not listening on called name");
		break;
	    case 0x81:
		ND_PRINT("Not listening for calling name");
		break;
	    case 0x82:
		ND_PRINT("Called name not present");
		break;
	    case 0x83:
		ND_PRINT("Called name present, but insufficient resources");
		break;
	    default:
		ND_PRINT("Unspecified error 0x%X", ecode);
		break;
	    }
	  }
	    break;

	case 0x85:
	    ND_PRINT("Session Keepalive");
	    break;

	default:
	    data = smb_fdata(ndo, data, "Unknown packet type [rB]", maxbuf, 0);
	    break;
	}
    } else {
	ND_PRINT("\n>>> NBT Session Packet\n");
	switch (type) {
	case 0x00:
	    data = smb_fdata(ndo, data, "[P1]NBT Session Message\nFlags=[B]\nLength=[ru]\n",
		data + 4, 0);
	    if (data == NULL)
		break;
	    if (nbt_len >= 4 && caplen >= 4 && memcmp(data,"\377SMB",4) == 0) {
		if (nbt_len > caplen) {
		    if (nbt_len > length)
			ND_PRINT("WARNING: Packet is continued in later TCP segments\n");
		    else
			ND_PRINT("WARNING: Short packet. Try increasing the snap length by %u\n",
			    nbt_len - caplen);
		}
		print_smb(ndo, data, maxbuf > data + nbt_len ? data + nbt_len : maxbuf);
	    } else
		ND_PRINT("Session packet:(raw data or continuation?)\n");
	    break;

	case 0x81:
	    data = smb_fdata(ndo, data,
		"[P1]NBT Session Request\nFlags=[B]\nLength=[ru]\nDestination=[n1]\nSource=[n1]\n",
		maxbuf, 0);
	    break;

	case 0x82:
	    data = smb_fdata(ndo, data, "[P1]NBT Session Granted\nFlags=[B]\nLength=[ru]\n", maxbuf, 0);
	    break;

	case 0x83:
	  {
	    const u_char *origdata;
	    u_int ecode;

	    origdata = data;
	    data = smb_fdata(ndo, data, "[P1]NBT SessionReject\nFlags=[B]\nLength=[ru]\nReason=[B]\n",
		maxbuf, 0);
	    if (data == NULL)
		break;
	    if (nbt_len >= 1 && caplen >= 1) {
		ecode = GET_U_1(origdata + 4);
		switch (ecode) {
		case 0x80:
		    ND_PRINT("Not listening on called name\n");
		    break;
		case 0x81:
		    ND_PRINT("Not listening for calling name\n");
		    break;
		case 0x82:
		    ND_PRINT("Called name not present\n");
		    break;
		case 0x83:
		    ND_PRINT("Called name present, but insufficient resources\n");
		    break;
		default:
		    ND_PRINT("Unspecified error 0x%X\n", ecode);
		    break;
		}
	    }
	  }
	    break;

	case 0x85:
	    data = smb_fdata(ndo, data, "[P1]NBT Session Keepalive\nFlags=[B]\nLength=[ru]\n", maxbuf, 0);
	    break;

	default:
	    data = smb_fdata(ndo, data, "NBT - Unknown packet type\nType=[B]\n", maxbuf, 0);
	    break;
	}
    }
    return;
trunc:
    nd_print_trunc(ndo);
}

static const struct tok opcode_str[] = {
	{ 0,  "QUERY"                   },
	{ 5,  "REGISTRATION"            },
	{ 6,  "RELEASE"                 },
	{ 7,  "WACK"                    },
	{ 8,  "REFRESH(8)"              },
	{ 9,  "REFRESH"                 },
	{ 15, "MULTIHOMED REGISTRATION" },
	{ 0, NULL }
};

/*
 * print a NBT packet received across udp on port 137
 */
void
nbt_udp137_print(netdissect_options *ndo,
                 const u_char *data, u_int length)
{
    const u_char *maxbuf = data + length;
    u_int name_trn_id, response, opcode, nm_flags, rcode;
    u_int qdcount, ancount, nscount, arcount;
    const u_char *p;
    u_int total, i;

    ndo->ndo_protocol = "nbt_udp137";
    name_trn_id = GET_BE_U_2(data);
    response = (GET_U_1(data + 2) >> 7);
    opcode = (GET_U_1(data + 2) >> 3) & 0xF;
    nm_flags = ((GET_U_1(data + 2) & 0x7) << 4) + (GET_U_1(data + 3) >> 4);
    rcode = GET_U_1(data + 3) & 0xF;
    qdcount = GET_BE_U_2(data + 4);
    ancount = GET_BE_U_2(data + 6);
    nscount = GET_BE_U_2(data + 8);
    arcount = GET_BE_U_2(data + 10);
    startbuf = data;

    if (maxbuf <= data)
	return;

    if (ndo->ndo_vflag > 1)
	ND_PRINT("\n>>> ");

    ND_PRINT("NBT UDP PACKET(137): %s", tok2str(opcode_str, "OPUNKNOWN", opcode));
    if (response) {
        ND_PRINT("; %s", rcode ? "NEGATIVE" : "POSITIVE");
    }
    ND_PRINT("; %s; %s", response ? "RESPONSE" : "REQUEST",
              (nm_flags & 1) ? "BROADCAST" : "UNICAST");

    if (ndo->ndo_vflag < 2)
	return;

    ND_PRINT("\nTrnID=0x%X\nOpCode=%u\nNmFlags=0x%X\nRcode=%u\nQueryCount=%u\nAnswerCount=%u\nAuthorityCount=%u\nAddressRecCount=%u\n",
	name_trn_id, opcode, nm_flags, rcode, qdcount, ancount, nscount,
	arcount);

    p = data + 12;

    total = ancount + nscount + arcount;

    if (qdcount > 100 || total > 100) {
	ND_PRINT("Corrupt packet??\n");
	return;
    }

    if (qdcount) {
	ND_PRINT("QuestionRecords:\n");
	for (i = 0; i < qdcount; i++) {
	    p = smb_fdata(ndo, p,
		"|Name=[n1]\nQuestionType=[rw]\nQuestionClass=[rw]\n#",
		maxbuf, 0);
	    if (p == NULL)
		goto out;
	}
    }

    if (total) {
	ND_PRINT("\nResourceRecords:\n");
	for (i = 0; i < total; i++) {
	    u_int rdlen;
	    u_int restype;

	    p = smb_fdata(ndo, p, "Name=[n1]\n#", maxbuf, 0);
	    if (p == NULL)
		goto out;
	    restype = GET_BE_U_2(p);
	    p = smb_fdata(ndo, p, "ResType=[rw]\nResClass=[rw]\nTTL=[rU]\n", p + 8, 0);
	    if (p == NULL)
		goto out;
	    rdlen = GET_BE_U_2(p);
	    ND_PRINT("ResourceLength=%u\nResourceData=\n", rdlen);
	    p += 2;
	    if (rdlen == 6) {
		p = smb_fdata(ndo, p, "AddrType=[rw]\nAddress=[b.b.b.b]\n", p + rdlen, 0);
		if (p == NULL)
		    goto out;
	    } else {
		if (restype == 0x21) {
		    u_int numnames;

		    numnames = GET_U_1(p);
		    p = smb_fdata(ndo, p, "NumNames=[B]\n", p + 1, 0);
		    if (p == NULL)
			goto out;
		    while (numnames) {
			p = smb_fdata(ndo, p, "Name=[n2]\t#", maxbuf, 0);
			if (p == NULL)
			    goto out;
			ND_TCHECK_1(p);
			if (p >= maxbuf)
			    goto out;
			if (GET_U_1(p) & 0x80)
			    ND_PRINT("<GROUP> ");
			switch (GET_U_1(p) & 0x60) {
			case 0x00: ND_PRINT("B "); break;
			case 0x20: ND_PRINT("P "); break;
			case 0x40: ND_PRINT("M "); break;
			case 0x60: ND_PRINT("_ "); break;
			}
			if (GET_U_1(p) & 0x10)
			    ND_PRINT("<DEREGISTERING> ");
			if (GET_U_1(p) & 0x08)
			    ND_PRINT("<CONFLICT> ");
			if (GET_U_1(p) & 0x04)
			    ND_PRINT("<ACTIVE> ");
			if (GET_U_1(p) & 0x02)
			    ND_PRINT("<PERMANENT> ");
			ND_PRINT("\n");
			p += 2;
			numnames--;
		    }
		} else {
		    if (p >= maxbuf)
		        goto out;
		    smb_data_print(ndo, p, ND_MIN(rdlen, length - ND_BYTES_BETWEEN(p, data)));
		    p += rdlen;
		}
	    }
	}
    }

    if (p < maxbuf)
	smb_fdata(ndo, p, "AdditionalData:\n", maxbuf, 0);

out:
    return;
trunc:
    nd_print_trunc(ndo);
}

/*
 * Print an SMB-over-TCP packet received across tcp on port 445
 */
void
smb_tcp_print(netdissect_options *ndo,
              const u_char * data, u_int length)
{
    u_int caplen;
    u_int smb_len;
    const u_char *maxbuf;

    ndo->ndo_protocol = "smb_tcp";
    if (length < 4)
	goto trunc;
    if (ndo->ndo_snapend < data)
	goto trunc;
    caplen = ND_BYTES_AVAILABLE_AFTER(data);
    if (caplen < 4)
	goto trunc;
    maxbuf = data + caplen;
    smb_len = GET_BE_U_3(data + 1);
    length -= 4;
    caplen -= 4;

    startbuf = data;
    data += 4;

    if (smb_len >= 4 && caplen >= 4 && memcmp(data,"\377SMB",4) == 0) {
	if (smb_len > caplen) {
	    if (smb_len > length)
		ND_PRINT(" WARNING: Packet is continued in later TCP segments\n");
	    else
		ND_PRINT(" WARNING: Short packet. Try increasing the snap length by %u\n",
		    smb_len - caplen);
	} else
	    ND_PRINT(" ");
	print_smb(ndo, data, maxbuf > data + smb_len ? data + smb_len : maxbuf);
    } else
	ND_PRINT(" SMB-over-TCP packet:(raw data or continuation?)\n");
    return;
trunc:
    nd_print_trunc(ndo);
}

/*
 * print a NBT packet received across udp on port 138
 */
void
nbt_udp138_print(netdissect_options *ndo,
                 const u_char *data, u_int length)
{
    const u_char *maxbuf = data + length;

    ndo->ndo_protocol = "nbt_udp138";
    if (maxbuf > ndo->ndo_snapend)
	maxbuf = ndo->ndo_snapend;
    if (maxbuf <= data)
	return;
    startbuf = data;

    if (ndo->ndo_vflag < 2) {
	ND_PRINT("NBT UDP PACKET(138)");
	return;
    }

    data = smb_fdata(ndo, data,
	"\n>>> NBT UDP PACKET(138) Res=[rw] ID=[rw] IP=[b.b.b.b] Port=[ru] Length=[ru] Res2=[rw]\nSourceName=[n1]\nDestName=[n1]\n#",
	maxbuf, 0);

    if (data != NULL) {
	/* If there isn't enough data for "\377SMB", don't check for it. */
	if ((data + 3) >= maxbuf)
	    goto out;

	if (memcmp(data, "\377SMB",4) == 0)
	    print_smb(ndo, data, maxbuf);
    }
out:
    return;
}


/*
   print netbeui frames
*/
static struct nbf_strings {
	const char	*name;
	const char	*nonverbose;
	const char	*verbose;
} nbf_strings[0x20] = {
	{ "Add Group Name Query", ", [P23]Name to add=[n2]#",
	  "[P5]ResponseCorrelator=[w]\n[P16]Name to add=[n2]\n" },
	{ "Add Name Query", ", [P23]Name to add=[n2]#",
	  "[P5]ResponseCorrelator=[w]\n[P16]Name to add=[n2]\n" },
	{ "Name In Conflict", NULL, NULL },
	{ "Status Query", NULL, NULL },
	{ NULL, NULL, NULL },	/* not used */
	{ NULL, NULL, NULL },	/* not used */
	{ NULL, NULL, NULL },	/* not used */
	{ "Terminate Trace", NULL, NULL },
	{ "Datagram", NULL,
	  "[P7]Destination=[n2]\nSource=[n2]\n" },
	{ "Broadcast Datagram", NULL,
	  "[P7]Destination=[n2]\nSource=[n2]\n" },
	{ "Name Query", ", [P7]Name=[n2]#",
	  "[P1]SessionNumber=[B]\nNameType=[B][P2]\nResponseCorrelator=[w]\nName=[n2]\nName of sender=[n2]\n" },
	{ NULL, NULL, NULL },	/* not used */
	{ NULL, NULL, NULL },	/* not used */
	{ "Add Name Response", ", [P1]GroupName=[w] [P4]Destination=[n2] Source=[n2]#",
	  "AddNameInProcess=[B]\nGroupName=[w]\nTransmitCorrelator=[w][P2]\nDestination=[n2]\nSource=[n2]\n" },
	{ "Name Recognized", NULL,
	  "[P1]Data2=[w]\nTransmitCorrelator=[w]\nResponseCorelator=[w]\nDestination=[n2]\nSource=[n2]\n" },
	{ "Status Response", NULL, NULL },
	{ NULL, NULL, NULL },	/* not used */
	{ NULL, NULL, NULL },	/* not used */
	{ NULL, NULL, NULL },	/* not used */
	{ "Terminate Trace", NULL, NULL },
	{ "Data Ack", NULL,
	  "[P3]TransmitCorrelator=[w][P2]\nRemoteSessionNumber=[B]\nLocalSessionNumber=[B]\n" },
	{ "Data First/Middle", NULL,
	  "Flags=[{RECEIVE_CONTINUE|NO_ACK||PIGGYBACK_ACK_INCLUDED|}]\nResyncIndicator=[w][P2]\nResponseCorelator=[w]\nRemoteSessionNumber=[B]\nLocalSessionNumber=[B]\n" },
	{ "Data Only/Last", NULL,
	  "Flags=[{|NO_ACK|PIGGYBACK_ACK_ALLOWED|PIGGYBACK_ACK_INCLUDED|}]\nResyncIndicator=[w][P2]\nResponseCorelator=[w]\nRemoteSessionNumber=[B]\nLocalSessionNumber=[B]\n" },
	{ "Session Confirm", NULL,
	  "Data1=[B]\nData2=[w]\nTransmitCorrelator=[w]\nResponseCorelator=[w]\nRemoteSessionNumber=[B]\nLocalSessionNumber=[B]\n" },
	{ "Session End", NULL,
	  "[P1]Data2=[w][P4]\nRemoteSessionNumber=[B]\nLocalSessionNumber=[B]\n" },
	{ "Session Initialize", NULL,
	  "Data1=[B]\nData2=[w]\nTransmitCorrelator=[w]\nResponseCorelator=[w]\nRemoteSessionNumber=[B]\nLocalSessionNumber=[B]\n" },
	{ "No Receive", NULL,
	  "Flags=[{|SEND_NO_ACK}]\nDataBytesAccepted=[b][P4]\nRemoteSessionNumber=[B]\nLocalSessionNumber=[B]\n" },
	{ "Receive Outstanding", NULL,
	  "[P1]DataBytesAccepted=[b][P4]\nRemoteSessionNumber=[B]\nLocalSessionNumber=[B]\n" },
	{ "Receive Continue", NULL,
	  "[P2]TransmitCorrelator=[w]\n[P2]RemoteSessionNumber=[B]\nLocalSessionNumber=[B]\n" },
	{ NULL, NULL, NULL },	/* not used */
	{ NULL, NULL, NULL },	/* not used */
	{ "Session Alive", NULL, NULL }
};

void
netbeui_print(netdissect_options *ndo,
              u_short control, const u_char *data, u_int length)
{
    const u_char *maxbuf = data + length;
    u_int len;
    u_int command;
    const u_char *data2;
    int is_truncated = 0;

    ndo->ndo_protocol = "netbeui";
    if (maxbuf > ndo->ndo_snapend)
	maxbuf = ndo->ndo_snapend;
    len = GET_LE_U_2(data);
    command = GET_U_1(data + 4);
    data2 = data + len;
    if (data2 >= maxbuf) {
	data2 = maxbuf;
	is_truncated = 1;
    }

    startbuf = data;

    if (ndo->ndo_vflag < 2) {
	ND_PRINT("NBF Packet: ");
	data = smb_fdata(ndo, data, "[P5]#", maxbuf, 0);
    } else {
	ND_PRINT("\n>>> NBF Packet\nType=0x%X ", control);
	data = smb_fdata(ndo, data, "Length=[u] Signature=[w] Command=[B]\n#", maxbuf, 0);
    }
    if (data == NULL)
	goto out;

    if (command > 0x1f || nbf_strings[command].name == NULL) {
	if (ndo->ndo_vflag < 2)
	    data = smb_fdata(ndo, data, "Unknown NBF Command#", data2, 0);
	else
	    data = smb_fdata(ndo, data, "Unknown NBF Command\n", data2, 0);
    } else {
	if (ndo->ndo_vflag < 2) {
	    ND_PRINT("%s", nbf_strings[command].name);
	    if (nbf_strings[command].nonverbose != NULL)
		data = smb_fdata(ndo, data, nbf_strings[command].nonverbose, data2, 0);
	} else {
	    ND_PRINT("%s:\n", nbf_strings[command].name);
	    if (nbf_strings[command].verbose != NULL)
		data = smb_fdata(ndo, data, nbf_strings[command].verbose, data2, 0);
	    else
		ND_PRINT("\n");
	}
    }

    if (ndo->ndo_vflag < 2)
	return;

    if (data == NULL)
	goto out;

    if (is_truncated) {
	/* data2 was past the end of the buffer */
	goto out;
    }

    /* If this isn't a command that would contain an SMB message, quit. */
    if (command != 0x08 && command != 0x09 && command != 0x15 &&
        command != 0x16)
	goto out;

    /* If there isn't enough data for "\377SMB", don't look for it. */
    if ((data2 + 3) >= maxbuf)
	goto out;

    if (memcmp(data2, "\377SMB",4) == 0)
	print_smb(ndo, data2, maxbuf);
    else {
	u_int i;
	for (i = 0; i < 128; i++) {
	    if ((data2 + i + 3) >= maxbuf)
		break;
	    if (memcmp(data2 + i, "\377SMB", 4) == 0) {
		ND_PRINT("found SMB packet at %u\n", i);
		print_smb(ndo, data2 + i, maxbuf);
		break;
	    }
	}
    }

out:
    return;
}


/*
 * print IPX-Netbios frames
 */
void
ipx_netbios_print(netdissect_options *ndo,
                  const u_char *data, u_int length)
{
    /*
     * this is a hack till I work out how to parse the rest of the
     * NetBIOS-over-IPX stuff
     */
    u_int i;
    const u_char *maxbuf;

    ndo->ndo_protocol = "ipx_netbios";
    maxbuf = data + length;
    /* Don't go past the end of the captured data in the packet. */
    if (maxbuf > ndo->ndo_snapend)
	maxbuf = ndo->ndo_snapend;
    startbuf = data;
    for (i = 0; i < 128; i++) {
	if ((data + i + 4) > maxbuf)
	    break;
	if (memcmp(data + i, "\377SMB", 4) == 0) {
	    smb_fdata(ndo, data, "\n>>> IPX transport ", data + i, 0);
	    print_smb(ndo, data + i, maxbuf);
	    break;
	}
    }
    if (i == 128)
	smb_fdata(ndo, data, "\n>>> Unknown IPX ", maxbuf, 0);
}
