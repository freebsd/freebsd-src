/* 
   Copyright (C) Andrew Tridgell 1995-1999

   This software may be distributed either under the terms of the
   BSD-style license that accompanies tcpdump or the GNU GPL version 2
   or later */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef lint
static const char rcsid[] =
     "@(#) $Header: /tcpdump/master/tcpdump/print-smb.c,v 1.7 2000/12/05 06:42:47 guy Exp $";
#endif

#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "interface.h"
#include "smb.h"

static int request=0;

const uchar *startbuf=NULL;

struct smbdescript
{
  char *req_f1;
  char *req_f2;
  char *rep_f1;
  char *rep_f2;
  void (*fn)(); /* sometimes (u_char *, u_char *, u_char *, u_char *)
		and sometimes (u_char *, u_char *, int, int) */
};

struct smbfns
{
  int id;
  char *name;
  int flags;
  struct smbdescript descript;
};

#define DEFDESCRIPT  {NULL,NULL,NULL,NULL,NULL}

#define FLG_CHAIN (1<<0)

static struct smbfns *smbfind(int id,struct smbfns *list)
{
  int sindex;

  for (sindex=0;list[sindex].name;sindex++) 
    if (list[sindex].id == id) return(&list[sindex]);

  return(&list[0]);
}

static void trans2_findfirst(uchar *param,uchar *data,int pcnt,int dcnt)
{
  char *fmt;

  if (request) {
    fmt = "Attribute=[A]\nSearchCount=[d]\nFlags=[w]\nLevel=[dP5]\nFile=[S]\n";
  } else {
    fmt = "Handle=[w]\nCount=[d]\nEOS=[w]\nEoffset=[d]\nLastNameOfs=[w]\n";
  }

  fdata(param,fmt,param+pcnt);
  if (dcnt) {
    printf("data:\n");
    print_data(data,dcnt);
  }
}

static void trans2_qfsinfo(uchar *param,uchar *data,int pcnt,int dcnt)
{
  static int level=0;
  char *fmt="";

  if (request) {
    level = SVAL(param,0);
    fmt = "InfoLevel=[d]\n";
    fdata(param,fmt,param+pcnt);
  } else {
    switch (level) {
    case 1:
      fmt = "idFileSystem=[W]\nSectorUnit=[D]\nUnit=[D]\nAvail=[D]\nSectorSize=[d]\n";
      break;
    case 2:
      fmt = "CreationTime=[T2]VolNameLength=[B]\nVolumeLabel=[s12]\n";
      break;
    case 0x105:
      fmt = "Capabilities=[W]\nMaxFileLen=[D]\nVolNameLen=[D]\nVolume=[S]\n";
      break;
    default:
      fmt = "UnknownLevel\n";
    }
    fdata(data,fmt,data+dcnt);
  }
  if (dcnt) {
    printf("data:\n");
    print_data(data,dcnt);
  }
}

struct smbfns trans2_fns[] = {
{0,"TRANSACT2_OPEN",0,
   {"Flags2=[w]\nMode=[w]\nSearchAttrib=[A]\nAttrib=[A]\nTime=[T2]\nOFun=[w]\nSize=[D]\nRes=([w,w,w,w,w])\nPath=[S]",NULL,
    "Handle=[d]\nAttrib=[A]\nTime=[T2]\nSize=[D]\nAccess=[w]\nType=[w]\nState=[w]\nAction=[w]\nInode=[W]\nOffErr=[d]\n|EALength=[d]\n",NULL,NULL}},

{1,"TRANSACT2_FINDFIRST",0,
   {NULL,NULL,NULL,NULL,trans2_findfirst}},

{2,"TRANSACT2_FINDNEXT",0,DEFDESCRIPT},

{3,"TRANSACT2_QFSINFO",0,
   {NULL,NULL,NULL,NULL,trans2_qfsinfo}},

{4,"TRANSACT2_SETFSINFO",0,DEFDESCRIPT},
{5,"TRANSACT2_QPATHINFO",0,DEFDESCRIPT},
{6,"TRANSACT2_SETPATHINFO",0,DEFDESCRIPT},
{7,"TRANSACT2_QFILEINFO",0,DEFDESCRIPT},
{8,"TRANSACT2_SETFILEINFO",0,DEFDESCRIPT},
{9,"TRANSACT2_FSCTL",0,DEFDESCRIPT},
{10,"TRANSACT2_IOCTL",0,DEFDESCRIPT},
{11,"TRANSACT2_FINDNOTIFYFIRST",0,DEFDESCRIPT},
{12,"TRANSACT2_FINDNOTIFYNEXT",0,DEFDESCRIPT},
{13,"TRANSACT2_MKDIR",0,DEFDESCRIPT},
{-1,NULL,0,DEFDESCRIPT}};


static void print_trans2(uchar *words,uchar *dat,uchar *buf,uchar *maxbuf)
{
  static struct smbfns *fn = &trans2_fns[0];
  uchar *data,*param;
  uchar *f1=NULL,*f2=NULL;
  int pcnt,dcnt;

  if (request) {
    fn = smbfind(SVAL(words+1,14*2),trans2_fns);
    data = buf+SVAL(words+1,12*2);
    param = buf+SVAL(words+1,10*2);
    pcnt = SVAL(words+1,9*2);
    dcnt = SVAL(words+1,11*2);
  } else {
    data = buf+SVAL(words+1,7*2);
    param = buf+SVAL(words+1,4*2);
    pcnt = SVAL(words+1,3*2);
    dcnt = SVAL(words+1,6*2);
  }

  printf("%s param_length=%d data_length=%d\n",
	 fn->name,pcnt,dcnt);

  if (request) {
    if (CVAL(words,0) == 8) {
      fdata(words+1,"Trans2Secondary\nTotParam=[d]\nTotData=[d]\nParamCnt=[d]\nParamOff=[d]\nParamDisp=[d]\nDataCnt=[d]\nDataOff=[d]\nDataDisp=[d]\nHandle=[d]\n",maxbuf);
      return;	    
    } else {
      fdata(words+1,"TotParam=[d]\nTotData=[d]\nMaxParam=[d]\nMaxData=[d]\nMaxSetup=[d]\nFlags=[w]\nTimeOut=[D]\nRes1=[w]\nParamCnt=[d]\nParamOff=[d]\nDataCnt=[d]\nDataOff=[d]\nSetupCnt=[d]\n",words+1+14*2);
      fdata(data+1,"TransactionName=[S]\n%",maxbuf);
    }
    f1 = fn->descript.req_f1;
    f2 = fn->descript.req_f2;
  } else {
    if (CVAL(words,0) == 0) {
      printf("Trans2Interim\n");
      return;
    } else {
      fdata(words+1,"TotParam=[d]\nTotData=[d]\nRes1=[w]\nParamCnt=[d]\nParamOff=[d]\nParamDisp[d]\nDataCnt=[d]\nDataOff=[d]\nDataDisp=[d]\nSetupCnt=[d]\n",words+1+10*2);
    }
    f1 = fn->descript.rep_f1;
    f2 = fn->descript.rep_f2;
  }

  if (fn->descript.fn) {
    fn->descript.fn(param,data,pcnt,dcnt);
  } else {
    fdata(param,f1?f1:(uchar*)"Paramaters=\n",param+pcnt);
    fdata(data,f2?f2:(uchar*)"Data=\n",data+dcnt);      
  }
}


static void print_browse(uchar *param,int paramlen,const uchar *data,int datalen)
{
  const uchar *maxbuf = data + datalen;
  int command = CVAL(data,0);

  fdata(param,"BROWSE PACKET\n|Param ",param+paramlen);

  switch (command) {
  case 0xF:
    data = fdata(data,"BROWSE PACKET:\nType=[B] (LocalMasterAnnouncement)\nUpdateCount=[w]\nRes1=[B]\nAnnounceInterval=[d]\nName=[n2]\nMajorVersion=[B]\nMinorVersion=[B]\nServerType=[W]\nElectionVersion=[w]\nBrowserConstant=[w]\n",maxbuf);
    break;
    
  case 0x1:
    data = fdata(data,"BROWSE PACKET:\nType=[B] (HostAnnouncement)\nUpdateCount=[w]\nRes1=[B]\nAnnounceInterval=[d]\nName=[n2]\nMajorVersion=[B]\nMinorVersion=[B]\nServerType=[W]\nElectionVersion=[w]\nBrowserConstant=[w]\n",maxbuf);
    break;
    
  case 0x2:
    data = fdata(data,"BROWSE PACKET:\nType=[B] (AnnouncementRequest)\nFlags=[B]\nReplySystemName=[S]\n",maxbuf);
    break;
    
  case 0xc:
    data = fdata(data,"BROWSE PACKET:\nType=[B] (WorkgroupAnnouncement)\nUpdateCount=[w]\nRes1=[B]\nAnnounceInterval=[d]\nName=[n2]\nMajorVersion=[B]\nMinorVersion=[B]\nServerType=[W]\nCommentPointer=[W]\nServerName=[S]\n",maxbuf);
    break;

  case 0x8:
    data = fdata(data,"BROWSE PACKET:\nType=[B] (ElectionFrame)\nElectionVersion=[B]\nOSSummary=[W]\nUptime=[(W,W)]\nServerName=[S]\n",maxbuf);
    break;
    
  case 0xb:
    data = fdata(data,"BROWSE PACKET:\nType=[B] (BecomeBackupBrowser)\nName=[S]\n",maxbuf);
    break;
    
  case 0x9:
    data = fdata(data,"BROWSE PACKET:\nType=[B] (GetBackupList)\nListCount?=[B]\nToken?=[B]\n",maxbuf);
    break;
    
  case 0xa:
    data = fdata(data,"BROWSE PACKET:\nType=[B] (BackupListResponse)\nServerCount?=[B]\nToken?=[B]*Name=[S]\n",maxbuf);
    break;
    
  case 0xd:
    data = fdata(data,"BROWSE PACKET:\nType=[B] (MasterAnnouncement)\nMasterName=[S]\n",maxbuf);
    break;
    
  case 0xe:
    data = fdata(data,"BROWSE PACKET:\nType=[B] (ResetBrowser)\nOptions=[B]\n",maxbuf);
    break;
    
  default:
    data = fdata(data,"Unknown Browser Frame ",maxbuf);
    break;
  }
}


static void print_ipc(uchar *param,int paramlen,uchar *data,int datalen)
{
  if (paramlen)
    fdata(param,"Command=[w]\nStr1=[S]\nStr2=[S]\n",param+paramlen);
  if (datalen)
    fdata(data,"IPC ",data+datalen);
}


static void print_trans(uchar *words,uchar *data1,uchar *buf,uchar *maxbuf)
{
  uchar *f1,*f2,*f3,*f4;
  uchar *data,*param;
  int datalen,paramlen;

  if (request) {
    paramlen = SVAL(words+1,9*2);
    param = buf + SVAL(words+1,10*2);
    datalen = SVAL(words+1,11*2);
    data = buf + SVAL(words+1,12*2);
    f1 = "TotParamCnt=[d] \nTotDataCnt=[d] \nMaxParmCnt=[d] \nMaxDataCnt=[d]\nMaxSCnt=[d] \nTransFlags=[w] \nRes1=[w] \nRes2=[w] \nRes3=[w]\nParamCnt=[d] \nParamOff=[d] \nDataCnt=[d] \nDataOff=[d] \nSUCnt=[d]\n";
    f2 = "|Name=[S]\n";
    f3 = "|Param ";
    f4 = "|Data ";
  } else {
    paramlen = SVAL(words+1,3*2);
    param = buf + SVAL(words+1,4*2);
    datalen = SVAL(words+1,6*2);
    data = buf + SVAL(words+1,7*2);
    f1 = "TotParamCnt=[d] \nTotDataCnt=[d] \nRes1=[d]\nParamCnt=[d] \nParamOff=[d] \nRes2=[d] \nDataCnt=[d] \nDataOff=[d] \nRes3=[d]\nLsetup=[d]\n";
    f2 = "|Unknown ";
    f3 = "|Param ";
    f4 = "|Data ";
  }

  fdata(words+1,f1,MIN(words+1+2*CVAL(words,0),maxbuf));
  fdata(data1+2,f2,maxbuf - (paramlen + datalen));

  if (!strcmp(data1+2,"\\MAILSLOT\\BROWSE")) {
    print_browse(param,paramlen,data,datalen);
    return;
  }

  if (!strcmp(data1+2,"\\PIPE\\LANMAN")) {
    print_ipc(param,paramlen,data,datalen);
    return;
  }

  if (paramlen) fdata(param,f3,MIN(param+paramlen,maxbuf));
  if (datalen) fdata(data,f4,MIN(data+datalen,maxbuf));
}



static void print_negprot(uchar *words,uchar *data,uchar *buf,uchar *maxbuf)
{
  uchar *f1=NULL,*f2=NULL;

  if (request) {
    f2 = "*|Dialect=[Z]\n";
  } else {
    if (CVAL(words,0) == 1) {
      f1 = "Core Protocol\nDialectIndex=[d]";
    } else if (CVAL(words,0) == 17) {
      f1 = "NT1 Protocol\nDialectIndex=[d]\nSecMode=[B]\nMaxMux=[d]\nNumVcs=[d]\nMaxBuffer=[D]\nRawSize=[D]\nSessionKey=[W]\nCapabilities=[W]\nServerTime=[T3]TimeZone=[d]\nCryptKey=";
    } else if (CVAL(words,0) == 13) {
      f1 = "Coreplus/Lanman1/Lanman2 Protocol\nDialectIndex=[d]\nSecMode=[w]\nMaxXMit=[d]\nMaxMux=[d]\nMaxVcs=[d]\nBlkMode=[w]\nSessionKey=[W]\nServerTime=[T1]TimeZone=[d]\nRes=[W]\nCryptKey=";
    }
  }

  if (f1) 
    fdata(words+1,f1,MIN(words + 1 + CVAL(words,0)*2,maxbuf));
  else
    print_data(words+1,MIN(CVAL(words,0)*2,PTR_DIFF(maxbuf,words+1)));
  
  if (f2) 
    fdata(data+2,f2,MIN(data + 2 + SVAL(data,0),maxbuf));
  else
    print_data(data+2,MIN(SVAL(data,0),PTR_DIFF(maxbuf,data+2)));
    
}

static void print_sesssetup(uchar *words,uchar *data,uchar *buf,uchar *maxbuf)
{
  int wcnt = CVAL(words,0);
  uchar *f1=NULL,*f2=NULL;

  if (request) {
    if (wcnt==10) {
      f1 = "Com2=[w]\nOff2=[d]\nBufSize=[d]\nMpxMax=[d]\nVcNum=[d]\nSessionKey=[W]\nPassLen=[d]\nCryptLen=[d]\nCryptOff=[d]\nPass&Name=\n";
    } else {
      f1 = "Com2=[B]\nRes1=[B]\nOff2=[d]\nMaxBuffer=[d]\nMaxMpx=[d]\nVcNumber=[d]\nSessionKey=[W]\nCaseInsensitivePasswordLength=[d]\nCaseSensitivePasswordLength=[d]\nRes=[W]\nCapabilities=[W]\nPass1&Pass2&Account&Domain&OS&LanMan=\n";
    }
  } else {
    if (CVAL(words,0) == 3) {
      f1 = "Com2=[w]\nOff2=[d]\nAction=[w]\n";
    } else if (CVAL(words,0) == 13) {
      f1 = "Com2=[B]\nRes=[B]\nOff2=[d]\nAction=[w]\n";
      f2 = "NativeOS=[S]\nNativeLanMan=[S]\nPrimaryDomain=[S]\n";
    }
  }

  if (f1) 
    fdata(words+1,f1,MIN(words + 1 + CVAL(words,0)*2,maxbuf));
  else
    print_data(words+1,MIN(CVAL(words,0)*2,PTR_DIFF(maxbuf,words+1)));
  
  if (f2) 
    fdata(data+2,f2,MIN(data + 2 + SVAL(data,0),maxbuf));
  else
    print_data(data+2,MIN(SVAL(data,0),PTR_DIFF(maxbuf,data+2))); 
}


static struct smbfns smb_fns[] = 
{
{-1,"SMBunknown",0,DEFDESCRIPT},

{SMBtcon,"SMBtcon",0,
   {NULL,"Path=[Z]\nPassword=[Z]\nDevice=[Z]\n",
    "MaxXmit=[d]\nTreeId=[d]\n",NULL,
    NULL}},


{SMBtdis,"SMBtdis",0,DEFDESCRIPT},
{SMBexit,"SMBexit",0,DEFDESCRIPT},
{SMBioctl,"SMBioctl",0,DEFDESCRIPT},

{SMBecho,"SMBecho",0,
   {"ReverbCount=[d]\n",NULL,
    "SequenceNum=[d]\n",NULL,
    NULL}},

{SMBulogoffX, "SMBulogoffX",FLG_CHAIN,DEFDESCRIPT},

{SMBgetatr,"SMBgetatr",0,
   {NULL,"Path=[Z]\n",
    "Attribute=[A]\nTime=[T2]Size=[D]\nRes=([w,w,w,w,w])\n",NULL,
    NULL}},

{SMBsetatr,"SMBsetatr",0,
   {"Attribute=[A]\nTime=[T2]Res=([w,w,w,w,w])\n","Path=[Z]\n",
    NULL,NULL,NULL}},

{SMBchkpth,"SMBchkpth",0,
   {NULL,"Path=[Z]\n",NULL,NULL,NULL}},

{SMBsearch,"SMBsearch",0,
{"Count=[d]\nAttrib=[A]\n","Path=[Z]\nBlkType=[B]\nBlkLen=[d]\n|Res1=[B]\nMask=[s11]\nSrv1=[B]\nDirIndex=[d]\nSrv2=[w]\nRes2=[W]\n",
"Count=[d]\n","BlkType=[B]\nBlkLen=[d]\n*\nRes1=[B]\nMask=[s11]\nSrv1=[B]\nDirIndex=[d]\nSrv2=[w]\nRes2=[W]\nAttrib=[a]\nTime=[T1]Size=[D]\nName=[s13]\n",NULL}},


{SMBopen,"SMBopen",0,
   {"Mode=[w]\nAttribute=[A]\n","Path=[Z]\n",
    "Handle=[d]\nOAttrib=[A]\nTime=[T2]Size=[D]\nAccess=[w]\n",NULL,
    NULL}},

{SMBcreate,"SMBcreate",0,
   {"Attrib=[A]\nTime=[T2]","Path=[Z]\n",
    "Handle=[d]\n",NULL,
    NULL}},

{SMBmknew,"SMBmknew",0,
   {"Attrib=[A]\nTime=[T2]","Path=[Z]\n",
    "Handle=[d]\n",NULL,
    NULL}},

{SMBunlink,"SMBunlink",0,
   {"Attrib=[A]\n","Path=[Z]\n",NULL,NULL,NULL}},

{SMBread,"SMBread",0,
   {"Handle=[d]\nByteCount=[d]\nOffset=[D]\nCountLeft=[d]\n",NULL,
    "Count=[d]\nRes=([w,w,w,w])\n",NULL,NULL}},

{SMBwrite,"SMBwrite",0,
   {"Handle=[d]\nByteCount=[d]\nOffset=[D]\nCountLeft=[d]\n",NULL,
    "Count=[d]\n",NULL,NULL}},

{SMBclose,"SMBclose",0,
   {"Handle=[d]\nTime=[T2]",NULL,NULL,NULL,NULL}},

{SMBmkdir,"SMBmkdir",0,
   {NULL,"Path=[Z]\n",NULL,NULL,NULL}},

{SMBrmdir,"SMBrmdir",0,
   {NULL,"Path=[Z]\n",NULL,NULL,NULL}},

{SMBdskattr,"SMBdskattr",0,
{NULL,NULL,
"TotalUnits=[d]\nBlocksPerUnit=[d]\nBlockSize=[d]\nFreeUnits=[d]\nMedia=[w]\n",
NULL,NULL}},

{SMBmv,"SMBmv",0,
   {"Attrib=[A]\n","OldPath=[Z]\nNewPath=[Z]\n",NULL,NULL,NULL}},

/* this is a Pathworks specific call, allowing the 
   changing of the root path */
{pSETDIR,"SMBsetdir",0,
   {NULL,"Path=[Z]\n",NULL,NULL,NULL}},

{SMBlseek,"SMBlseek",0,
   {"Handle=[d]\nMode=[w]\nOffset=[D]\n","Offset=[D]\n",NULL,NULL}},

{SMBflush,"SMBflush",0,
   {"Handle=[d]\n",NULL,NULL,NULL,NULL}},

{SMBsplopen,"SMBsplopen",0,
   {"SetupLen=[d]\nMode=[w]\n","Ident=[Z]\n","Handle=[d]\n",NULL,NULL}},

{SMBsplclose,"SMBsplclose",0,
   {"Handle=[d]\n",NULL,NULL,NULL,NULL}},

{SMBsplretq,"SMBsplretq",0,
   {"MaxCount=[d]\nStartIndex=[d]\n",NULL,
    "Count=[d]\nIndex=[d]\n",
    "*Time=[T2]Status=[B]\nJobID=[d]\nSize=[D]\nRes=[B]Name=[s16]\n",
    NULL}},

{SMBsplwr,"SMBsplwr",0,
   {"Handle=[d]\n",NULL,NULL,NULL,NULL}},

{SMBlock,"SMBlock",0,
   {"Handle=[d]\nCount=[D]\nOffset=[D]\n",NULL,NULL,NULL,NULL}},

{SMBunlock,"SMBunlock",0,
   {"Handle=[d]\nCount=[D]\nOffset=[D]\n",NULL,NULL,NULL,NULL}},

/* CORE+ PROTOCOL FOLLOWS */

{SMBreadbraw,"SMBreadbraw",0,
{"Handle=[d]\nOffset=[D]\nMaxCount=[d]\nMinCount=[d]\nTimeOut=[D]\nRes=[d]\n",
 NULL,NULL,NULL,NULL}},

{SMBwritebraw,"SMBwritebraw",0,
{"Handle=[d]\nTotalCount=[d]\nRes=[w]\nOffset=[D]\nTimeOut=[D]\nWMode=[w]\nRes2=[W]\n|DataSize=[d]\nDataOff=[d]\n",
NULL,"WriteRawAck",NULL,NULL}},

{SMBwritec,"SMBwritec",0,
   {NULL,NULL,"Count=[d]\n",NULL,NULL}},

{SMBwriteclose,"SMBwriteclose",0,
   {"Handle=[d]\nCount=[d]\nOffset=[D]\nTime=[T2]Res=([w,w,w,w,w,w])",NULL,
    "Count=[d]\n",NULL,NULL}},

{SMBlockread,"SMBlockread",0,
   {"Handle=[d]\nByteCount=[d]\nOffset=[D]\nCountLeft=[d]\n",NULL,
    "Count=[d]\nRes=([w,w,w,w])\n",NULL,NULL}},

{SMBwriteunlock,"SMBwriteunlock",0,
   {"Handle=[d]\nByteCount=[d]\nOffset=[D]\nCountLeft=[d]\n",NULL,
    "Count=[d]\n",NULL,NULL}},

{SMBreadBmpx,"SMBreadBmpx",0,
{"Handle=[d]\nOffset=[D]\nMaxCount=[d]\nMinCount=[d]\nTimeOut=[D]\nRes=[w]\n",
NULL,
"Offset=[D]\nTotCount=[d]\nRemaining=[d]\nRes=([w,w])\nDataSize=[d]\nDataOff=[d]\n",
NULL,NULL}},

{SMBwriteBmpx,"SMBwriteBmpx",0,
{"Handle=[d]\nTotCount=[d]\nRes=[w]\nOffset=[D]\nTimeOut=[D]\nWMode=[w]\nRes2=[W]\nDataSize=[d]\nDataOff=[d]\n",NULL,
"Remaining=[d]\n",NULL,NULL}},

{SMBwriteBs,"SMBwriteBs",0,
   {"Handle=[d]\nTotCount=[d]\nOffset=[D]\nRes=[W]\nDataSize=[d]\nDataOff=[d]\n",NULL,
    "Count=[d]\n",NULL,NULL}},

{SMBsetattrE,"SMBsetattrE",0,
   {"Handle=[d]\nCreationTime=[T2]AccessTime=[T2]ModifyTime=[T2]",NULL,
      NULL,NULL,NULL}},

{SMBgetattrE,"SMBgetattrE",0,
{"Handle=[d]\n",NULL,
 "CreationTime=[T2]AccessTime=[T2]ModifyTime=[T2]Size=[D]\nAllocSize=[D]\nAttribute=[A]\n",NULL,NULL}},

{SMBtranss,"SMBtranss",0,DEFDESCRIPT},
{SMBioctls,"SMBioctls",0,DEFDESCRIPT},

{SMBcopy,"SMBcopy",0,
   {"TreeID2=[d]\nOFun=[w]\nFlags=[w]\n","Path=[S]\nNewPath=[S]\n",
    "CopyCount=[d]\n","|ErrStr=[S]\n",NULL}},

{SMBmove,"SMBmove",0,
   {"TreeID2=[d]\nOFun=[w]\nFlags=[w]\n","Path=[S]\nNewPath=[S]\n",
    "MoveCount=[d]\n","|ErrStr=[S]\n",NULL}},

{SMBopenX,"SMBopenX",FLG_CHAIN,
{"Com2=[w]\nOff2=[d]\nFlags=[w]\nMode=[w]\nSearchAttrib=[A]\nAttrib=[A]\nTime=[T2]OFun=[w]\nSize=[D]\nTimeOut=[D]\nRes=[W]\n","Path=[S]\n",
"Com2=[w]\nOff2=[d]\nHandle=[d]\nAttrib=[A]\nTime=[T2]Size=[D]\nAccess=[w]\nType=[w]\nState=[w]\nAction=[w]\nFileID=[W]\nRes=[w]\n",NULL,NULL}},

{SMBreadX,"SMBreadX",FLG_CHAIN,
{"Com2=[w]\nOff2=[d]\nHandle=[d]\nOffset=[D]\nMaxCount=[d]\nMinCount=[d]\nTimeOut=[D]\nCountLeft=[d]\n",NULL,
"Com2=[w]\nOff2=[d]\nRemaining=[d]\nRes=[W]\nDataSize=[d]\nDataOff=[d]\nRes=([w,w,w,w])\n",NULL,NULL}},

{SMBwriteX,"SMBwriteX",FLG_CHAIN,
{"Com2=[w]\nOff2=[d]\nHandle=[d]\nOffset=[D]\nTimeOut=[D]\nWMode=[w]\nCountLeft=[d]\nRes=[w]\nDataSize=[d]\nDataOff=[d]\n",NULL,
"Com2=[w]\nOff2=[d]\nCount=[d]\nRemaining=[d]\nRes=[W]\n",NULL,NULL}},

{SMBlockingX,"SMBlockingX",FLG_CHAIN,
{"Com2=[w]\nOff2=[d]\nHandle=[d]\nLockType=[w]\nTimeOut=[D]\nUnlockCount=[d]\nLockCount=[d]\n",
"*Process=[d]\nOffset=[D]\nLength=[D]\n",
"Com2=[w]\nOff2=[d]\n"}},

{SMBffirst,"SMBffirst",0,
{"Count=[d]\nAttrib=[A]\n","Path=[Z]\nBlkType=[B]\nBlkLen=[d]\n|Res1=[B]\nMask=[s11]\nSrv1=[B]\nDirIndex=[d]\nSrv2=[w]\n",
"Count=[d]\n","BlkType=[B]\nBlkLen=[d]\n*\nRes1=[B]\nMask=[s11]\nSrv1=[B]\nDirIndex=[d]\nSrv2=[w]\nRes2=[W]\nAttrib=[a]\nTime=[T1]Size=[D]\nName=[s13]\n",NULL}},

{SMBfunique,"SMBfunique",0,
{"Count=[d]\nAttrib=[A]\n","Path=[Z]\nBlkType=[B]\nBlkLen=[d]\n|Res1=[B]\nMask=[s11]\nSrv1=[B]\nDirIndex=[d]\nSrv2=[w]\n",
"Count=[d]\n","BlkType=[B]\nBlkLen=[d]\n*\nRes1=[B]\nMask=[s11]\nSrv1=[B]\nDirIndex=[d]\nSrv2=[w]\nRes2=[W]\nAttrib=[a]\nTime=[T1]Size=[D]\nName=[s13]\n",NULL}},

{SMBfclose,"SMBfclose",0,
{"Count=[d]\nAttrib=[A]\n","Path=[Z]\nBlkType=[B]\nBlkLen=[d]\n|Res1=[B]\nMask=[s11]\nSrv1=[B]\nDirIndex=[d]\nSrv2=[w]\n",
"Count=[d]\n","BlkType=[B]\nBlkLen=[d]\n*\nRes1=[B]\nMask=[s11]\nSrv1=[B]\nDirIndex=[d]\nSrv2=[w]\nRes2=[W]\nAttrib=[a]\nTime=[T1]Size=[D]\nName=[s13]\n",NULL}},

{SMBfindnclose, "SMBfindnclose", 0,
   {"Handle=[d]\n",NULL,NULL,NULL,NULL}},

{SMBfindclose, "SMBfindclose", 0,
   {"Handle=[d]\n",NULL,NULL,NULL,NULL}},

{SMBsends,"SMBsends",0,
   {NULL,"Source=[Z]\nDest=[Z]\n",NULL,NULL,NULL}},

{SMBsendstrt,"SMBsendstrt",0,
   {NULL,"Source=[Z]\nDest=[Z]\n","GroupID=[d]\n",NULL,NULL}},
   
{SMBsendend,"SMBsendend",0,
   {"GroupID=[d]\n",NULL,NULL,NULL,NULL}},

{SMBsendtxt,"SMBsendtxt",0,
   {"GroupID=[d]\n",NULL,NULL,NULL,NULL}},

{SMBsendb,"SMBsendb",0,
   {NULL,"Source=[Z]\nDest=[Z]\n",NULL,NULL,NULL}},

{SMBfwdname,"SMBfwdname",0,DEFDESCRIPT},
{SMBcancelf,"SMBcancelf",0,DEFDESCRIPT},
{SMBgetmac,"SMBgetmac",0,DEFDESCRIPT},

{SMBnegprot,"SMBnegprot",0,
   {NULL,NULL,NULL,NULL,print_negprot}},

{SMBsesssetupX,"SMBsesssetupX",FLG_CHAIN,
   {NULL,NULL,NULL,NULL,print_sesssetup}},

{SMBtconX,"SMBtconX",FLG_CHAIN,
{"Com2=[w]\nOff2=[d]\nFlags=[w]\nPassLen=[d]\nPasswd&Path&Device=\n",NULL,
 "Com2=[w]\nOff2=[d]\n","ServiceType=[S]\n",NULL}},

{SMBtrans2, "SMBtrans2",0,{NULL,NULL,NULL,NULL,print_trans2}},

{SMBtranss2, "SMBtranss2", 0,DEFDESCRIPT},
{SMBctemp,"SMBctemp",0,DEFDESCRIPT},
{SMBreadBs,"SMBreadBs",0,DEFDESCRIPT},
{SMBtrans,"SMBtrans",0,{NULL,NULL,NULL,NULL,print_trans}},

{SMBnttrans,"SMBnttrans", 0, DEFDESCRIPT},
{SMBnttranss,"SMBnttranss", 0, DEFDESCRIPT},

{SMBntcreateX,"SMBntcreateX", FLG_CHAIN, 
{"Com2=[w]\nOff2=[d]\nRes=[b]\nNameLen=[d]\nFlags=[W]\nRootDirectoryFid=[D]\nAccessMask=[W]\nAllocationSize=[L]\nExtFileAttributes=[W]\nShareAccess=[W]\nCreateDisposition=[W]\nCreateOptions=[W]\nImpersonationLevel=[W]\nSecurityFlags=[b]\n","Path=[S]\n",
	 "Com2=[w]\nOff2=[d]\nOplockLevel=[b]\nFid=[d]\nCreateAction=[W]\nCreateTime=[T3]LastAccessTime=[T3]LastWriteTime=[T3]ChangeTime=[T3]ExtFileAttributes=[W]\nAllocationSize=[L]\nEndOfFile=[L]\nFileType=[w]\nDeviceState=[w]\nDirectory=[b]\n", NULL}},

{SMBntcancel,"SMBntcancel", 0, DEFDESCRIPT},

{-1,NULL,0,DEFDESCRIPT}};


/*******************************************************************
print a SMB message
********************************************************************/
static void print_smb(const uchar *buf, const uchar *maxbuf)
{
  int command;
  const uchar *words, *data;
  struct smbfns *fn;
  char *fmt_smbheader = 
"[P4]SMB Command   =  [B]\nError class   =  [BP1]\nError code    =  [d]\nFlags1        =  [B]\nFlags2        =  [B][P13]\nTree ID       =  [d]\nProc ID       =  [d]\nUID           =  [d]\nMID           =  [d]\nWord Count    =  [b]\n";

  request = (CVAL(buf,9)&0x80)?0:1;

  command = CVAL(buf,4);

  fn = smbfind(command,smb_fns);

  printf("\nSMB PACKET: %s (%s)\n",fn->name,request?"REQUEST":"REPLY");

  if (vflag == 0) return;

  /* print out the header */
  fdata(buf,fmt_smbheader,buf+33);

  if (CVAL(buf,5)) {
    int class = CVAL(buf,5);
    int num = SVAL(buf,7);
    printf("SMBError = %s\n",smb_errstr(class,num));
  }

  words = buf+32;
  data = words + 1 + CVAL(words,0)*2;


  while (words && data)
    {
      char *f1,*f2;
      int wct = CVAL(words,0);

      if (request) {
	f1 = fn->descript.req_f1;
	f2 = fn->descript.req_f2;
      } else {
	f1 = fn->descript.rep_f1;
	f2 = fn->descript.rep_f2;
      }

      if (fn->descript.fn) {
	fn->descript.fn(words,data,buf,maxbuf);
      } else {
	if (f1) {
	  printf("smbvwv[]=\n");
	  fdata(words+1,f1,words + 1 + wct*2);
	} else if (wct) {
	  int i;
	  int v;
	  printf("smbvwv[]=\n");
	  for (i=0;i<wct;i++) {
	    v = SVAL(words+1,2*i);
	    printf("smb_vwv[%d]=%d (0x%X)\n",i,v,v);
	  }
	}
	
	if (f2) {
	  printf("smbbuf[]=\n");
	  fdata(data+2,f2,maxbuf);
	} else {
	  int bcc = SVAL(data,0);
	  printf("smb_bcc=%d\n",bcc);
	  if (bcc>0) {
	    printf("smb_buf[]=\n");
	    print_data(data + 2, MIN(bcc,PTR_DIFF(maxbuf,data+2)));
	  }
	}
      }

      if ((fn->flags & FLG_CHAIN) && CVAL(words,0) && SVAL(words,1)!=0xFF) {
	command = SVAL(words,1);
	words = buf + SVAL(words,3);
	data = words + 1 + CVAL(words,0)*2;

	fn = smbfind(command,smb_fns);

	printf("\nSMB PACKET: %s (%s) (CHAINED)\n",fn->name,request?"REQUEST":"REPLY");
      } else {
	words = data = NULL;
      }
    }

  printf("\n");  
}


/*
   print a NBT packet received across tcp on port 139
*/
void nbt_tcp_print(const uchar *data,int length)
{
  const uchar *maxbuf = data + length;
  int flags = CVAL(data,0);
  int nbt_len = RSVAL(data,2);

  startbuf = data;
  if (maxbuf <= data) return;

  printf("\n>>> NBT Packet\n");

  switch (flags) {
  case 1:    
    printf("flags=0x%x\n", flags);
  case 0:    
    data = fdata(data,"NBT Session Packet\nFlags=[rw]\nLength=[rd]\n",data+4);
    if (data == NULL)
      break;
    if (memcmp(data,"\377SMB",4)==0) {
      if (nbt_len>PTR_DIFF(maxbuf,data))
	printf("WARNING: Short packet. Try increasing the snap length (%ld)\n",
	       PTR_DIFF(maxbuf,data));
      print_smb(data,maxbuf>data+nbt_len?data+nbt_len:maxbuf);
    } else {
	    printf("Session packet:(raw data?)\n");
    }
    break;

  case 0x81:
    data = fdata(data,"NBT Session Request\nFlags=[rW]\nDestination=[n1]\nSource=[n1]\n",maxbuf);
    break;

  case 0x82:
    data = fdata(data,"NBT Session Granted\nFlags=[rW]\n",maxbuf);
    break;

  case 0x83:
    {
      int ecode = CVAL(data,4);
      data = fdata(data,"NBT SessionReject\nFlags=[rW]\nReason=[B]\n",maxbuf);
      switch (ecode) {
      case 0x80: 
	printf("Not listening on called name\n"); 
	break;
      case 0x81: 
	printf("Not listening for calling name\n"); 
	break;
      case 0x82: 
	printf("Called name not present\n"); 
	break;
      case 0x83: 
	printf("Called name present, but insufficient resources\n"); 
	break;
      default:
	printf("Unspecified error 0x%X\n",ecode); 
	break;	  
      }
    }
    break;

  case 0x85:
    data = fdata(data,"NBT Session Keepalive\nFlags=[rW]\n",maxbuf);
    break;

  default:
    printf("flags=0x%x\n", flags);
    data = fdata(data,"NBT - Unknown packet type\nType=[rW]\n",maxbuf);
  }
  printf("\n");
  fflush(stdout);
}


/*
   print a NBT packet received across udp on port 137
*/
void nbt_udp137_print(const uchar *data, int length)
{
  const uchar *maxbuf = data + length;
  int name_trn_id = RSVAL(data,0);
  int response = (CVAL(data,2)>>7);
  int opcode = (CVAL(data,2) >> 3) & 0xF;
  int nm_flags = ((CVAL(data,2) & 0x7) << 4) + (CVAL(data,3)>>4);
  int rcode = CVAL(data,3) & 0xF;
  int qdcount = RSVAL(data,4);
  int ancount = RSVAL(data,6);
  int nscount = RSVAL(data,8);
  int arcount = RSVAL(data,10);
  char *opcodestr;  
  const char *p;

  startbuf = data;

  if (maxbuf <= data) return;

  printf("\n>>> NBT UDP PACKET(137): ");

  switch (opcode) {
  case 0: opcodestr = "QUERY"; break;
  case 5: opcodestr = "REGISTRATION"; break;
  case 6: opcodestr = "RELEASE"; break;
  case 7: opcodestr = "WACK"; break;
  case 8: opcodestr = "REFRESH(8)"; break;
  case 9: opcodestr = "REFRESH"; break;
  default: opcodestr = "OPUNKNOWN"; break;
  }
  printf("%s", opcodestr);
  if (response) {
    if (rcode)
      printf("; NEGATIVE");
    else
      printf("; POSITIVE");
  }
    
  if (response) 
    printf("; RESPONSE");
  else
    printf("; REQUEST");

  if (nm_flags&1)
    printf("; BROADCAST");
  else
    printf("; UNICAST");
  
  if (vflag == 0) return;

  printf("\nTrnID=0x%X\nOpCode=%d\nNmFlags=0x%X\nRcode=%d\nQueryCount=%d\nAnswerCount=%d\nAuthorityCount=%d\nAddressRecCount=%d\n",
	 name_trn_id,opcode,nm_flags,rcode,qdcount,ancount,nscount,arcount);

  p = data + 12;

  {
    int total = ancount+nscount+arcount;
    int i;

    if (qdcount>100 || total>100) {
      printf("Corrupt packet??\n");
      return;
    }

    if (qdcount) {
      printf("QuestionRecords:\n");
      for (i=0;i<qdcount;i++)
	p = fdata(p,"|Name=[n1]\nQuestionType=[rw]\nQuestionClass=[rw]\n#",maxbuf);
	if (p == NULL)
	  goto out;
    }

    if (total) {
      printf("\nResourceRecords:\n");
      for (i=0;i<total;i++) {	  
	int rdlen;
	int restype;
	p = fdata(p,"Name=[n1]\n#",maxbuf);
	if (p == NULL)
	  goto out;
	restype = RSVAL(p,0);
	p = fdata(p,"ResType=[rw]\nResClass=[rw]\nTTL=[rD]\n",p+8);
	if (p == NULL)
	  goto out;
	rdlen = RSVAL(p,0);
	printf("ResourceLength=%d\nResourceData=\n",rdlen);
	p += 2;
	if (rdlen == 6) {
	  p = fdata(p,"AddrType=[rw]\nAddress=[b.b.b.b]\n",p+rdlen);
	  if (p == NULL)
	    goto out;
	} else {
	  if (restype == 0x21) {
	    int numnames = CVAL(p,0);
	    p = fdata(p,"NumNames=[B]\n",p+1);
	    if (p == NULL)
	      goto out;
	    while (numnames--) {
	      p = fdata(p,"Name=[n2]\t#",maxbuf);
	      if (p[0] & 0x80) printf("<GROUP> ");
	      switch (p[0] & 0x60) {
	      case 0x00: printf("B "); break;
	      case 0x20: printf("P "); break;
	      case 0x40: printf("M "); break;
	      case 0x60: printf("_ "); break;
	      }
	      if (p[0] & 0x10) printf("<DEREGISTERING> ");
	      if (p[0] & 0x08) printf("<CONFLICT> ");
	      if (p[0] & 0x04) printf("<ACTIVE> ");
	      if (p[0] & 0x02) printf("<PERMANENT> ");
	      printf("\n");
	      p += 2;
	    }
	  } else {
	    print_data(p,rdlen);
	    p += rdlen;
	  }
	}
      }
    }
  }

  if ((uchar*)p < maxbuf) {
    fdata(p,"AdditionalData:\n",maxbuf);    
  }      
  
out:
  printf("\n");
  fflush(stdout);
}



/*
   print a NBT packet received across udp on port 138
*/
void nbt_udp138_print(const uchar *data, int length)
{
  const uchar *maxbuf = data + length;
  startbuf = data;
  if (maxbuf <= data) return;

  data = fdata(data,"\n>>> NBT UDP PACKET(138) Res=[rw] ID=[rw] IP=[b.b.b.b] Port=[rd] Length=[rd] Res2=[rw]\nSourceName=[n1]\nDestName=[n1]\n#",maxbuf);

  if (data != NULL)
    print_smb(data,maxbuf);
  
  printf("\n");
  fflush(stdout);
}



/*
   print netbeui frames 
*/
void netbeui_print(u_short control, const uchar *data, const uchar *maxbuf)
{
  int len = SVAL(data,0);
  int command = CVAL(data,4);
  const uchar *data2 = data + len;
  int is_truncated = 0;

  if (data2 >= maxbuf) {
    data2 = maxbuf;
    is_truncated = 1;
  }

  startbuf = data;

  printf("\n>>> NetBeui Packet\nType=0x%X ", control);
  data = fdata(data,"Length=[d] Signature=[w] Command=[B]\n#",maxbuf);
  if (data == NULL)
    goto out;

  switch (command) {
  case 0xA: 
    data = fdata(data,"NameQuery:[P1]\nSessionNumber=[B]\nNameType=[B][P2]\nResponseCorrelator=[w]\nDestination=[n2]\nSource=[n2]\n",data2);
    break;

  case 0x8:
    data = fdata(data,"NetbiosDataGram:[P7]\nDestination=[n2]\nSource=[n2]\n",data2);
    break;

  case 0xE:
    data = fdata(data,"NameRecognise:\n[P1]\nData2=[w]\nTransmitCorrelator=[w]\nResponseCorelator=[w]\nDestination=[n2]\nSource=[n2]\n",data2);
    break;

  case 0x19:
    data = fdata(data,"SessionInitialise:\nData1=[B]\nData2=[w]\nTransmitCorrelator=[w]\nResponseCorelator=[w]\nRemoteSessionNumber=[B]\nLocalSessionNumber=[B]\n",data2);
    break;

  case 0x17:
    data = fdata(data,"SessionConfirm:\nData1=[B]\nData2=[w]\nTransmitCorrelator=[w]\nResponseCorelator=[w]\nRemoteSessionNumber=[B]\nLocalSessionNumber=[B]\n",data2);
    break;

  case 0x16:
    data = fdata(data,"NetbiosDataOnlyLast:\nFlags=[{|NO_ACK|PIGGYBACK_ACK_ALLOWED|PIGGYBACK_ACK_INCLUDED|}]\nResyncIndicator=[w][P2]\nResponseCorelator=[w]\nRemoteSessionNumber=[B]\nLocalSessionNumber=[B]\n",data2);
    break;

  case 0x14:
    data = fdata(data,"NetbiosDataAck:\n[P3]TransmitCorrelator=[w][P2]\nRemoteSessionNumber=[B]\nLocalSessionNumber=[B]\n",data2);
    break;

  case 0x18:
    data = fdata(data,"SessionEnd:\n[P1]Data2=[w][P4]\nRemoteSessionNumber=[B]\nLocalSessionNumber=[B]\n",data2);
    break;

  case 0x1f:
    data = fdata(data,"SessionAlive\n",data2);
    break;

  default:
    data = fdata(data,"Unknown Netbios Command ",data2);
    break;
  }
  if (data == NULL)
    goto out;

  if (is_truncated) {
    /* data2 was past the end of the buffer */
    goto out;
  }

  if (memcmp(data2,"\377SMB",4)==0) {
    print_smb(data2,maxbuf);
  } else {
    int i;
    for (i=0;i<128;i++) {
      if (&data2[i] >= maxbuf)
        break;
      if (memcmp(&data2[i],"\377SMB",4)==0) {
	printf("found SMB packet at %d\n", i);
	print_smb(&data2[i],maxbuf);
	break;
      }
    }
  }

out:
  printf("\n");
}


/*
   print IPX-Netbios frames 
*/
void ipx_netbios_print(const uchar *data, const uchar *maxbuf)
{
  /* this is a hack till I work out how to parse the rest of the IPX stuff */
  int i;
  startbuf = data;
  for (i=0;i<128;i++)
    if (memcmp(&data[i],"\377SMB",4)==0) {
      fdata(data,"\n>>> IPX transport ",&data[i]);
      if (data != NULL)
	print_smb(&data[i],maxbuf);
      printf("\n");
      fflush(stdout);
      break;
    }
  if (i==128)
    fdata(data,"\n>>> Unknown IPX ",maxbuf);
}
