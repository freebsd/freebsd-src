#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <sys/types.h>
#ifdef __linux__
#   include <asm/page.h>
#endif
#if defined(HAS_MSG) || defined(HAS_SEM) || defined(HAS_SHM)
#ifndef HAS_SEM
#   include <sys/ipc.h>
#endif
#   ifdef HAS_MSG
#       include <sys/msg.h>
#   endif
#   ifdef HAS_SHM
#       if defined(PERL_SCO) || defined(PERL_ISC)
#           include <sys/sysmacros.h>	/* SHMLBA */
#       endif
#      include <sys/shm.h>
#      ifndef HAS_SHMAT_PROTOTYPE
           extern Shmat_t shmat (int, char *, int);
#      endif
#      if defined(__sparc__) && (defined(__NetBSD__) || defined(__OpenBSD__))
#          undef  SHMLBA /* not static: determined at boot time */
#          define SHMLBA getpagesize()
#      endif
#   endif
#endif

/* Required to get 'struct pte' for SHMLBA on ULTRIX. */
#if defined(__ultrix) || defined(__ultrix__) || defined(ultrix)
#include <machine/pte.h>
#endif

/* Required in BSDI to get PAGE_SIZE definition for SHMLBA.
 * Ugly.  More beautiful solutions welcome.
 * Shouting at BSDI sounds quite beautiful. */
#ifdef __bsdi__
#   include <vm/vm_param.h>	/* move upwards under HAS_SHM? */
#endif

#ifndef S_IRWXU
#   ifdef S_IRUSR
#       define S_IRWXU (S_IRUSR|S_IWUSR|S_IWUSR)
#       define S_IRWXG (S_IRGRP|S_IWGRP|S_IWGRP)
#       define S_IRWXO (S_IROTH|S_IWOTH|S_IWOTH)
#   else
#       define S_IRWXU 0700
#       define S_IRWXG 0070
#       define S_IRWXO 0007
#   endif
#endif

MODULE=IPC::SysV	PACKAGE=IPC::Msg::stat

PROTOTYPES: ENABLE

void
pack(obj)
    SV	* obj
PPCODE:
{
#ifdef HAS_MSG
    SV *sv;
    struct msqid_ds ds;
    AV *list = (AV*)SvRV(obj);
    sv = *av_fetch(list,0,TRUE); ds.msg_perm.uid = SvIV(sv);
    sv = *av_fetch(list,1,TRUE); ds.msg_perm.gid = SvIV(sv);
    sv = *av_fetch(list,4,TRUE); ds.msg_perm.mode = SvIV(sv);
    sv = *av_fetch(list,6,TRUE); ds.msg_qbytes = SvIV(sv);
    ST(0) = sv_2mortal(newSVpvn((char *)&ds,sizeof(ds)));
    XSRETURN(1);
#else
    croak("System V msgxxx is not implemented on this machine");
#endif
}

void
unpack(obj,buf)
    SV * obj
    SV * buf
PPCODE:
{
#ifdef HAS_MSG
    STRLEN len;
    SV **sv_ptr;
    struct msqid_ds *ds = (struct msqid_ds *)SvPV(buf,len);
    AV *list = (AV*)SvRV(obj);
    if (len != sizeof(*ds)) {
	croak("Bad arg length for %s, length is %d, should be %d",
		    "IPC::Msg::stat",
		    len, sizeof(*ds));
    }
    sv_ptr = av_fetch(list,0,TRUE);
    sv_setiv(*sv_ptr, ds->msg_perm.uid);
    sv_ptr = av_fetch(list,1,TRUE);
    sv_setiv(*sv_ptr, ds->msg_perm.gid);
    sv_ptr = av_fetch(list,2,TRUE);
    sv_setiv(*sv_ptr, ds->msg_perm.cuid);
    sv_ptr = av_fetch(list,3,TRUE);
    sv_setiv(*sv_ptr, ds->msg_perm.cgid);
    sv_ptr = av_fetch(list,4,TRUE);
    sv_setiv(*sv_ptr, ds->msg_perm.mode);
    sv_ptr = av_fetch(list,5,TRUE);
    sv_setiv(*sv_ptr, ds->msg_qnum);
    sv_ptr = av_fetch(list,6,TRUE);
    sv_setiv(*sv_ptr, ds->msg_qbytes);
    sv_ptr = av_fetch(list,7,TRUE);
    sv_setiv(*sv_ptr, ds->msg_lspid);
    sv_ptr = av_fetch(list,8,TRUE);
    sv_setiv(*sv_ptr, ds->msg_lrpid);
    sv_ptr = av_fetch(list,9,TRUE);
    sv_setiv(*sv_ptr, ds->msg_stime);
    sv_ptr = av_fetch(list,10,TRUE);
    sv_setiv(*sv_ptr, ds->msg_rtime);
    sv_ptr = av_fetch(list,11,TRUE);
    sv_setiv(*sv_ptr, ds->msg_ctime);
    XSRETURN(1);
#else
    croak("System V msgxxx is not implemented on this machine");
#endif
}

MODULE=IPC::SysV	PACKAGE=IPC::Semaphore::stat

void
unpack(obj,ds)
    SV * obj
    SV * ds
PPCODE:
{
#ifdef HAS_SEM
    STRLEN len;
    AV *list = (AV*)SvRV(obj);
    struct semid_ds *data = (struct semid_ds *)SvPV(ds,len);
    if(!sv_isa(obj, "IPC::Semaphore::stat"))
	croak("method %s not called a %s object",
		"unpack","IPC::Semaphore::stat");
    if (len != sizeof(*data)) {
	croak("Bad arg length for %s, length is %d, should be %d",
		    "IPC::Semaphore::stat",
		    len, sizeof(*data));
    }
    sv_setiv(*av_fetch(list,0,TRUE), data[0].sem_perm.uid);
    sv_setiv(*av_fetch(list,1,TRUE), data[0].sem_perm.gid);
    sv_setiv(*av_fetch(list,2,TRUE), data[0].sem_perm.cuid);
    sv_setiv(*av_fetch(list,3,TRUE), data[0].sem_perm.cgid);
    sv_setiv(*av_fetch(list,4,TRUE), data[0].sem_perm.mode);
    sv_setiv(*av_fetch(list,5,TRUE), data[0].sem_ctime);
    sv_setiv(*av_fetch(list,6,TRUE), data[0].sem_otime);
    sv_setiv(*av_fetch(list,7,TRUE), data[0].sem_nsems);
    XSRETURN(1);
#else
    croak("System V semxxx is not implemented on this machine");
#endif
}

void
pack(obj)
    SV	* obj
PPCODE:
{
#ifdef HAS_SEM
    SV **sv_ptr;
    SV *sv;
    struct semid_ds ds;
    AV *list = (AV*)SvRV(obj);
    if(!sv_isa(obj, "IPC::Semaphore::stat"))
	croak("method %s not called a %s object",
		"pack","IPC::Semaphore::stat");
    if((sv_ptr = av_fetch(list,0,TRUE)) && (sv = *sv_ptr))
	ds.sem_perm.uid = SvIV(*sv_ptr);
    if((sv_ptr = av_fetch(list,1,TRUE)) && (sv = *sv_ptr))
	ds.sem_perm.gid = SvIV(*sv_ptr);
    if((sv_ptr = av_fetch(list,2,TRUE)) && (sv = *sv_ptr))
	ds.sem_perm.cuid = SvIV(*sv_ptr);
    if((sv_ptr = av_fetch(list,3,TRUE)) && (sv = *sv_ptr))
	ds.sem_perm.cgid = SvIV(*sv_ptr);
    if((sv_ptr = av_fetch(list,4,TRUE)) && (sv = *sv_ptr))
	ds.sem_perm.mode = SvIV(*sv_ptr);
    if((sv_ptr = av_fetch(list,5,TRUE)) && (sv = *sv_ptr))
	ds.sem_ctime = SvIV(*sv_ptr);
    if((sv_ptr = av_fetch(list,6,TRUE)) && (sv = *sv_ptr))
	ds.sem_otime = SvIV(*sv_ptr);
    if((sv_ptr = av_fetch(list,7,TRUE)) && (sv = *sv_ptr))
	ds.sem_nsems = SvIV(*sv_ptr);
    ST(0) = sv_2mortal(newSVpvn((char *)&ds,sizeof(ds)));
    XSRETURN(1);
#else
    croak("System V semxxx is not implemented on this machine");
#endif
}

MODULE=IPC::SysV	PACKAGE=IPC::SysV

void
ftok(path, id)
        char *          path
        int             id
    CODE:
#if defined(HAS_SEM) || defined(HAS_SHM)
        key_t k = ftok(path, id);
        ST(0) = k == (key_t) -1 ? &PL_sv_undef : sv_2mortal(newSViv(k));
#else
        DIE(aTHX_ PL_no_func, "ftok");
#endif

void
SHMLBA()
    CODE:
#ifdef SHMLBA
    ST(0) = sv_2mortal(newSViv(SHMLBA));
#else
    croak("SHMLBA is not defined on this architecture");
#endif

BOOT:
{
    HV *stash = gv_stashpvn("IPC::SysV", 9, TRUE);
    /*
     * constant subs for IPC::SysV
     */
     struct { char *n; I32 v; } IPC__SysV__const[] = {
#ifdef GETVAL
        {"GETVAL", GETVAL},
#endif
#ifdef GETPID
        {"GETPID", GETPID},
#endif
#ifdef GETNCNT
        {"GETNCNT", GETNCNT},
#endif
#ifdef GETZCNT
        {"GETZCNT", GETZCNT},
#endif
#ifdef GETALL
        {"GETALL", GETALL},
#endif
#ifdef IPC_ALLOC
        {"IPC_ALLOC", IPC_ALLOC},
#endif
#ifdef IPC_CREAT
        {"IPC_CREAT", IPC_CREAT},
#endif
#ifdef IPC_EXCL
        {"IPC_EXCL", IPC_EXCL},
#endif
#ifdef IPC_GETACL
        {"IPC_GETACL", IPC_EXCL},
#endif
#ifdef IPC_LOCKED
        {"IPC_LOCKED", IPC_LOCKED},
#endif
#ifdef IPC_M
        {"IPC_M", IPC_M},
#endif
#ifdef IPC_NOERROR
        {"IPC_NOERROR", IPC_NOERROR},
#endif
#ifdef IPC_NOWAIT
        {"IPC_NOWAIT", IPC_NOWAIT},
#endif
#ifdef IPC_PRIVATE
        {"IPC_PRIVATE", IPC_PRIVATE},
#endif
#ifdef IPC_R
        {"IPC_R", IPC_R},
#endif
#ifdef IPC_RMID
        {"IPC_RMID", IPC_RMID},
#endif
#ifdef IPC_SET
        {"IPC_SET", IPC_SET},
#endif
#ifdef IPC_SETACL
        {"IPC_SETACL", IPC_SETACL},
#endif
#ifdef IPC_SETLABEL
        {"IPC_SETLABEL", IPC_SETLABEL},
#endif
#ifdef IPC_STAT
        {"IPC_STAT", IPC_STAT},
#endif
#ifdef IPC_W
        {"IPC_W", IPC_W},
#endif
#ifdef IPC_WANTED
        {"IPC_WANTED", IPC_WANTED},
#endif
#ifdef MSG_NOERROR
        {"MSG_NOERROR", MSG_NOERROR},
#endif
#ifdef MSG_FWAIT
        {"MSG_FWAIT", MSG_FWAIT},
#endif
#ifdef MSG_LOCKED
        {"MSG_LOCKED", MSG_LOCKED},
#endif
#ifdef MSG_MWAIT
        {"MSG_MWAIT", MSG_MWAIT},
#endif
#ifdef MSG_WAIT
        {"MSG_WAIT", MSG_WAIT},
#endif
#ifdef MSG_R
        {"MSG_R", MSG_R},
#endif
#ifdef MSG_RWAIT
        {"MSG_RWAIT", MSG_RWAIT},
#endif
#ifdef MSG_STAT
        {"MSG_STAT", MSG_STAT},
#endif
#ifdef MSG_W
        {"MSG_W", MSG_W},
#endif
#ifdef MSG_WWAIT
        {"MSG_WWAIT", MSG_WWAIT},
#endif
#ifdef SEM_A
        {"SEM_A", SEM_A},
#endif
#ifdef SEM_ALLOC
        {"SEM_ALLOC", SEM_ALLOC},
#endif
#ifdef SEM_DEST
        {"SEM_DEST", SEM_DEST},
#endif
#ifdef SEM_ERR
        {"SEM_ERR", SEM_ERR},
#endif
#ifdef SEM_R
        {"SEM_R", SEM_R},
#endif
#ifdef SEM_ORDER
        {"SEM_ORDER", SEM_ORDER},
#endif
#ifdef SEM_UNDO
        {"SEM_UNDO", SEM_UNDO},
#endif
#ifdef SETVAL
        {"SETVAL", SETVAL},
#endif
#ifdef SETALL
        {"SETALL", SETALL},
#endif
#ifdef SHM_CLEAR
        {"SHM_CLEAR", SHM_CLEAR},
#endif
#ifdef SHM_COPY
        {"SHM_COPY", SHM_COPY},
#endif
#ifdef SHM_DCACHE
        {"SHM_DCACHE", SHM_DCACHE},
#endif
#ifdef SHM_DEST
        {"SHM_DEST", SHM_DEST},
#endif
#ifdef SHM_ECACHE
        {"SHM_ECACHE", SHM_ECACHE},
#endif
#ifdef SHM_FMAP
        {"SHM_FMAP", SHM_FMAP},
#endif
#ifdef SHM_ICACHE
        {"SHM_ICACHE", SHM_ICACHE},
#endif
#ifdef SHM_INIT
        {"SHM_INIT", SHM_INIT},
#endif
#ifdef SHM_LOCK
        {"SHM_LOCK", SHM_LOCK},
#endif
#ifdef SHM_LOCKED
        {"SHM_LOCKED", SHM_LOCKED},
#endif
#ifdef SHM_MAP
        {"SHM_MAP", SHM_MAP},
#endif
#ifdef SHM_NOSWAP
        {"SHM_NOSWAP", SHM_NOSWAP},
#endif
#ifdef SHM_RDONLY
        {"SHM_RDONLY", SHM_RDONLY},
#endif
#ifdef SHM_REMOVED
        {"SHM_REMOVED", SHM_REMOVED},
#endif
#ifdef SHM_RND
        {"SHM_RND", SHM_RND},
#endif
#ifdef SHM_SHARE_MMU
        {"SHM_SHARE_MMU", SHM_SHARE_MMU},
#endif
#ifdef SHM_SHATTR
        {"SHM_SHATTR", SHM_SHATTR},
#endif
#ifdef SHM_SIZE
        {"SHM_SIZE", SHM_SIZE},
#endif
#ifdef SHM_UNLOCK
        {"SHM_UNLOCK", SHM_UNLOCK},
#endif
#ifdef SHM_W
        {"SHM_W", SHM_W},
#endif
#ifdef S_IRUSR
        {"S_IRUSR", S_IRUSR},
#endif
#ifdef S_IWUSR
        {"S_IWUSR", S_IWUSR},
#endif
#ifdef S_IRWXU
        {"S_IRWXU", S_IRWXU},
#endif
#ifdef S_IRGRP
        {"S_IRGRP", S_IRGRP},
#endif
#ifdef S_IWGRP
        {"S_IWGRP", S_IWGRP},
#endif
#ifdef S_IRWXG
        {"S_IRWXG", S_IRWXG},
#endif
#ifdef S_IROTH
        {"S_IROTH", S_IROTH},
#endif
#ifdef S_IWOTH
        {"S_IWOTH", S_IWOTH},
#endif
#ifdef S_IRWXO
        {"S_IRWXO", S_IRWXO},
#endif
	{Nullch,0}};
    char *name;
    int i;

    for(i = 0 ; (name = IPC__SysV__const[i].n) ; i++) {
	newCONSTSUB(stash,name, newSViv(IPC__SysV__const[i].v));
    }
}

