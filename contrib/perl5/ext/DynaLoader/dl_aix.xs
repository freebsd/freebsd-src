/* dl_aix.xs
 *
 * Written: 8/31/94 by Wayne Scott (wscott@ichips.intel.com)
 *
 *  All I did was take Jens-Uwe Mager's libdl emulation library for
 *  AIX and merged it with the dl_dlopen.xs file to create a dynamic library
 *  package that works for AIX.
 *
 *  I did change all malloc's, free's, strdup's, calloc's to use the perl
 *  equilvant.  I also removed some stuff we will not need.  Call fini()
 *  on statup...   It can probably be trimmed more.
 */

/*
 * @(#)dlfcn.c	1.5 revision of 93/02/14  20:14:17
 * This is an unpublished work copyright (c) 1992 Helios Software GmbH
 * 3000 Hannover 1, Germany
 */
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ldr.h>
#include <a.out.h>
#include <ldfcn.h>

/*
 * AIX 4.3 does remove some useful definitions from ldfcn.h. Define
 * these here to compensate for that lossage.
 */
#ifndef BEGINNING
# define BEGINNING SEEK_SET
#endif
#ifndef FSEEK
# define FSEEK(ldptr,o,p)	fseek(IOPTR(ldptr),(p==BEGINNING)?(OFFSET(ldptr) +o):o,p)
#endif
#ifndef FREAD
# define FREAD(p,s,n,ldptr)	fread(p,s,n,IOPTR(ldptr))
#endif

/* If using PerlIO, redefine these macros from <ldfcn.h> */
#ifdef USE_PERLIO
#define FSEEK(ldptr,o,p)        PerlIO_seek(IOPTR(ldptr),(p==BEGINNING)?(OFFSET(ldptr)+o):o,p)
#define FREAD(p,s,n,ldptr)      PerlIO_read(IOPTR(ldptr),p,s*n)
#endif

/*
 * We simulate dlopen() et al. through a call to load. Because AIX has
 * no call to find an exported symbol we read the loader section of the
 * loaded module and build a list of exported symbols and their virtual
 * address.
 */

typedef struct {
	char		*name;		/* the symbols's name */
	void		*addr;		/* its relocated virtual address */
} Export, *ExportPtr;

/*
 * The void * handle returned from dlopen is actually a ModulePtr.
 */
typedef struct Module {
	struct Module	*next;
	char		*name;		/* module name for refcounting */
	int		refCnt;		/* the number of references */
	void		*entry;		/* entry point from load */
	int		nExports;	/* the number of exports found */
	ExportPtr	exports;	/* the array of exports */
} Module, *ModulePtr;

/*
 * We keep a list of all loaded modules to be able to call the fini
 * handlers at atexit() time.
 */
static ModulePtr modList;

/*
 * The last error from one of the dl* routines is kept in static
 * variables here. Each error is returned only once to the caller.
 */
static char errbuf[BUFSIZ];
static int errvalid;

static void caterr(char *);
static int readExports(ModulePtr);
static void terminate(void);
static void *findMain(void);

static char *strerror_failed   = "(strerror failed)";
static char *strerror_r_failed = "(strerror_r failed)";

char *strerrorcat(char *str, int err) {
    int strsiz = strlen(str);
    int msgsiz;
    char *msg;

#ifdef USE_THREADS
    char *buf = malloc(BUFSIZ);

    if (buf == 0)
      return 0;
    if (strerror_r(err, buf, sizeof(buf)) == 0)
      msg = buf;
    else
      msg = strerror_r_failed;
    msgsiz = strlen(msg);
    if (strsiz + msgsiz < BUFSIZ)
      strcat(str, msg);
    free(buf);
#else
    if ((msg = strerror(err)) == 0)
      msg = strerror_failed;
    msgsiz = strlen(msg);		/* Note msg = buf and free() above. */
    if (strsiz + msgsiz < BUFSIZ)	/* Do not move this after #endif. */
      strcat(str, msg);
#endif

    return str;
}

char *strerrorcpy(char *str, int err) {
    int msgsiz;
    char *msg;

#ifdef USE_THREADS
    char *buf = malloc(BUFSIZ);

    if (buf == 0)
      return 0;
    if (strerror_r(err, buf, sizeof(buf)) == 0)
      msg = buf;
    else
      msg = strerror_r_failed;
    msgsiz = strlen(msg);
    if (msgsiz < BUFSIZ)
      strcpy(str, msg);
    free(buf);
#else
    if ((msg = strerror(err)) == 0)
      msg = strerror_failed;
    msgsiz = strlen(msg);	/* Note msg = buf and free() above. */
    if (msgsiz < BUFSIZ)	/* Do not move this after #endif. */
      strcpy(str, msg);
#endif

    return str;
}
  
/* ARGSUSED */
void *dlopen(char *path, int mode)
{
	register ModulePtr mp;
	static void *mainModule;

	/*
	 * Upon the first call register a terminate handler that will
	 * close all libraries. Also get a reference to the main module
	 * for use with loadbind.
	 */
	if (!mainModule) {
		if ((mainModule = findMain()) == NULL)
			return NULL;
		atexit(terminate);
	}
	/*
	 * Scan the list of modules if have the module already loaded.
	 */
	for (mp = modList; mp; mp = mp->next)
		if (strcmp(mp->name, path) == 0) {
			mp->refCnt++;
			return mp;
		}
	Newz(1000,mp,1,Module);
	if (mp == NULL) {
		errvalid++;
		strcpy(errbuf, "Newz: ");
		strerrorcat(errbuf, errno);
		return NULL;
	}
	
	if ((mp->name = savepv(path)) == NULL) {
		errvalid++;
		strcpy(errbuf, "savepv: ");
		strerrorcat(errbuf, errno);
		safefree(mp);
		return NULL;
	}
	/*
	 * load should be declared load(const char *...). Thus we
	 * cast the path to a normal char *. Ugly.
	 */
	if ((mp->entry = (void *)load((char *)path, L_NOAUTODEFER, NULL)) == NULL) {
		safefree(mp->name);
		safefree(mp);
		errvalid++;
		strcpy(errbuf, "dlopen: ");
		strcat(errbuf, path);
		strcat(errbuf, ": ");
		/*
		 * If AIX says the file is not executable, the error
		 * can be further described by querying the loader about
		 * the last error.
		 */
		if (errno == ENOEXEC) {
			char *tmp[BUFSIZ/sizeof(char *)];
			if (loadquery(L_GETMESSAGES, tmp, sizeof(tmp)) == -1)
				strerrorcpy(errbuf, errno);
			else {
				char **p;
				for (p = tmp; *p; p++)
					caterr(*p);
			}
		} else
			strerrorcat(errbuf, errno);
		return NULL;
	}
	mp->refCnt = 1;
	mp->next = modList;
	modList = mp;
	if (loadbind(0, mainModule, mp->entry) == -1) {
		dlclose(mp);
		errvalid++;
		strcpy(errbuf, "loadbind: ");
		strerrorcat(errbuf, errno);
		return NULL;
	}
	if (readExports(mp) == -1) {
		dlclose(mp);
		return NULL;
	}
	return mp;
}

/*
 * Attempt to decipher an AIX loader error message and append it
 * to our static error message buffer.
 */
static void caterr(char *s)
{
	register char *p = s;

	while (*p >= '0' && *p <= '9')
		p++;
	switch(atoi(s)) {
	case L_ERROR_TOOMANY:
		strcat(errbuf, "to many errors");
		break;
	case L_ERROR_NOLIB:
		strcat(errbuf, "can't load library");
		strcat(errbuf, p);
		break;
	case L_ERROR_UNDEF:
		strcat(errbuf, "can't find symbol");
		strcat(errbuf, p);
		break;
	case L_ERROR_RLDBAD:
		strcat(errbuf, "bad RLD");
		strcat(errbuf, p);
		break;
	case L_ERROR_FORMAT:
		strcat(errbuf, "bad exec format in");
		strcat(errbuf, p);
		break;
	case L_ERROR_ERRNO:
		strerrorcat(errbuf, atoi(++p));
		break;
	default:
		strcat(errbuf, s);
		break;
	}
}

void *dlsym(void *handle, const char *symbol)
{
	register ModulePtr mp = (ModulePtr)handle;
	register ExportPtr ep;
	register int i;

	/*
	 * Could speed up search, but I assume that one assigns
	 * the result to function pointers anyways.
	 */
	for (ep = mp->exports, i = mp->nExports; i; i--, ep++)
		if (strcmp(ep->name, symbol) == 0)
			return ep->addr;
	errvalid++;
	strcpy(errbuf, "dlsym: undefined symbol ");
	strcat(errbuf, symbol);
	return NULL;
}

char *dlerror(void)
{
	if (errvalid) {
		errvalid = 0;
		return errbuf;
	}
	return NULL;
}

int dlclose(void *handle)
{
	register ModulePtr mp = (ModulePtr)handle;
	int result;
	register ModulePtr mp1;

	if (--mp->refCnt > 0)
		return 0;
	result = unload(mp->entry);
	if (result == -1) {
		errvalid++;
		strerrorcpy(errbuf, errno);
	}
	if (mp->exports) {
		register ExportPtr ep;
		register int i;
		for (ep = mp->exports, i = mp->nExports; i; i--, ep++)
			if (ep->name)
				safefree(ep->name);
		safefree(mp->exports);
	}
	if (mp == modList)
		modList = mp->next;
	else {
		for (mp1 = modList; mp1; mp1 = mp1->next)
			if (mp1->next == mp) {
				mp1->next = mp->next;
				break;
			}
	}
	safefree(mp->name);
	safefree(mp);
	return result;
}

static void terminate(void)
{
	while (modList)
		dlclose(modList);
}

/* Added by Wayne Scott 
 * This is needed because the ldopen system call calls
 * calloc to allocated a block of date.  The ldclose call calls free.
 * Without this we get this system calloc and perl's free, resulting
 * in a "Bad free" message.  This way we always use perl's malloc.
 */
void *calloc(size_t ne, size_t sz) 
{
  void *out;

  out = (void *) safemalloc(ne*sz);
  memzero(out, ne*sz);
  return(out);
}
 
/*
 * Build the export table from the XCOFF .loader section.
 */
static int readExports(ModulePtr mp)
{
	LDFILE *ldp = NULL;
	SCNHDR sh;
	LDHDR *lhp;
	char *ldbuf;
	LDSYM *ls;
	int i;
	ExportPtr ep;

	if ((ldp = ldopen(mp->name, ldp)) == NULL) {
		struct ld_info *lp;
		char *buf;
		int size = 4*1024;
		if (errno != ENOENT) {
			errvalid++;
			strcpy(errbuf, "readExports: ");
			strerrorcat(errbuf, errno);
			return -1;
		}
		/*
		 * The module might be loaded due to the LIBPATH
		 * environment variable. Search for the loaded
		 * module using L_GETINFO.
		 */
		if ((buf = safemalloc(size)) == NULL) {
			errvalid++;
			strcpy(errbuf, "readExports: ");
			strerrorcat(errbuf, errno);
			return -1;
		}
		while ((i = loadquery(L_GETINFO, buf, size)) == -1 && errno == ENOMEM) {
			safefree(buf);
			size += 4*1024;
			if ((buf = safemalloc(size)) == NULL) {
				errvalid++;
				strcpy(errbuf, "readExports: ");
				strerrorcat(errbuf, errno);
				return -1;
			}
		}
		if (i == -1) {
			errvalid++;
			strcpy(errbuf, "readExports: ");
			strerrorcat(errbuf, errno);
			safefree(buf);
			return -1;
		}
		/*
		 * Traverse the list of loaded modules. The entry point
		 * returned by load() does actually point to the data
		 * segment origin.
		 */
		lp = (struct ld_info *)buf;
		while (lp) {
			if (lp->ldinfo_dataorg == mp->entry) {
				ldp = ldopen(lp->ldinfo_filename, ldp);
				break;
			}
			if (lp->ldinfo_next == 0)
				lp = NULL;
			else
				lp = (struct ld_info *)((char *)lp + lp->ldinfo_next);
		}
		safefree(buf);
		if (!ldp) {
			errvalid++;
			strcpy(errbuf, "readExports: ");
			strerrorcat(errbuf, errno);
			return -1;
		}
	}
	if (TYPE(ldp) != U802TOCMAGIC) {
		errvalid++;
		strcpy(errbuf, "readExports: bad magic");
		while(ldclose(ldp) == FAILURE)
			;
		return -1;
	}
	if (ldnshread(ldp, _LOADER, &sh) != SUCCESS) {
		errvalid++;
		strcpy(errbuf, "readExports: cannot read loader section header");
		while(ldclose(ldp) == FAILURE)
			;
		return -1;
	}
	/*
	 * We read the complete loader section in one chunk, this makes
	 * finding long symbol names residing in the string table easier.
	 */
	if ((ldbuf = (char *)safemalloc(sh.s_size)) == NULL) {
		errvalid++;
		strcpy(errbuf, "readExports: ");
		strerrorcat(errbuf, errno);
		while(ldclose(ldp) == FAILURE)
			;
		return -1;
	}
	if (FSEEK(ldp, sh.s_scnptr, BEGINNING) != OKFSEEK) {
		errvalid++;
		strcpy(errbuf, "readExports: cannot seek to loader section");
		safefree(ldbuf);
		while(ldclose(ldp) == FAILURE)
			;
		return -1;
	}
/* This first case is a hack, since it assumes that the 3rd parameter to
   FREAD is 1. See the redefinition of FREAD above to see how this works. */
#ifdef USE_PERLIO
	if (FREAD(ldbuf, sh.s_size, 1, ldp) != sh.s_size) {
#else
	if (FREAD(ldbuf, sh.s_size, 1, ldp) != 1) {
#endif
		errvalid++;
		strcpy(errbuf, "readExports: cannot read loader section");
		safefree(ldbuf);
		while(ldclose(ldp) == FAILURE)
			;
		return -1;
	}
	lhp = (LDHDR *)ldbuf;
	ls = (LDSYM *)(ldbuf+LDHDRSZ);
	/*
	 * Count the number of exports to include in our export table.
	 */
	for (i = lhp->l_nsyms; i; i--, ls++) {
		if (!LDR_EXPORT(*ls))
			continue;
		mp->nExports++;
	}
	Newz(1001, mp->exports, mp->nExports, Export);
	if (mp->exports == NULL) {
		errvalid++;
		strcpy(errbuf, "readExports: ");
		strerrorcat(errbuf, errno);
		safefree(ldbuf);
		while(ldclose(ldp) == FAILURE)
			;
		return -1;
	}
	/*
	 * Fill in the export table. All entries are relative to
	 * the entry point we got from load.
	 */
	ep = mp->exports;
	ls = (LDSYM *)(ldbuf+LDHDRSZ);
	for (i = lhp->l_nsyms; i; i--, ls++) {
		char *symname;
		if (!LDR_EXPORT(*ls))
			continue;
		if (ls->l_zeroes == 0)
			symname = ls->l_offset+lhp->l_stoff+ldbuf;
		else
			symname = ls->l_name;
		ep->name = savepv(symname);
		ep->addr = (void *)((unsigned long)mp->entry + ls->l_value);
		ep++;
	}
	safefree(ldbuf);
	while(ldclose(ldp) == FAILURE)
		;
	return 0;
}

/*
 * Find the main modules entry point. This is used as export pointer
 * for loadbind() to be able to resolve references to the main part.
 */
static void * findMain(void)
{
	struct ld_info *lp;
	char *buf;
	int size = 4*1024;
	int i;
	void *ret;

	if ((buf = safemalloc(size)) == NULL) {
		errvalid++;
		strcpy(errbuf, "findMain: ");
		strerrorcat(errbuf, errno);
		return NULL;
	}
	while ((i = loadquery(L_GETINFO, buf, size)) == -1 && errno == ENOMEM) {
		safefree(buf);
		size += 4*1024;
		if ((buf = safemalloc(size)) == NULL) {
			errvalid++;
			strcpy(errbuf, "findMain: ");
			strerrorcat(errbuf, errno);
			return NULL;
		}
	}
	if (i == -1) {
		errvalid++;
		strcpy(errbuf, "findMain: ");
		strerrorcat(errbuf, errno);
		safefree(buf);
		return NULL;
	}
	/*
	 * The first entry is the main module. The entry point
	 * returned by load() does actually point to the data
	 * segment origin.
	 */
	lp = (struct ld_info *)buf;
	ret = lp->ldinfo_dataorg;
	safefree(buf);
	return ret;
}

/* dl_dlopen.xs
 * 
 * Platform:	SunOS/Solaris, possibly others which use dlopen.
 * Author:	Paul Marquess (pmarquess@bfsec.bt.co.uk)
 * Created:	10th July 1994
 *
 * Modified:
 * 15th July 1994   - Added code to explicitly save any error messages.
 * 3rd August 1994  - Upgraded to v3 spec.
 * 9th August 1994  - Changed to use IV
 * 10th August 1994 - Tim Bunce: Added RTLD_LAZY, switchable debugging,
 *                    basic FreeBSD support, removed ClearError
 *
 */

/* Porting notes:

	see dl_dlopen.xs

*/

#include "dlutils.c"	/* SaveError() etc	*/


static void
dl_private_init()
{
    (void)dl_generic_private_init();
}
 
MODULE = DynaLoader     PACKAGE = DynaLoader

BOOT:
    (void)dl_private_init();


void *
dl_load_file(filename, flags=0)
	char *	filename
	int	flags
	CODE:
	DLDEBUG(1,PerlIO_printf(PerlIO_stderr(), "dl_load_file(%s,%x):\n", filename,flags));
	if (flags & 0x01)
	    warn("Can't make loaded symbols global on this platform while loading %s",filename);
	RETVAL = dlopen(filename, 1) ;
	DLDEBUG(2,PerlIO_printf(PerlIO_stderr(), " libref=%x\n", RETVAL));
	ST(0) = sv_newmortal() ;
	if (RETVAL == NULL)
	    SaveError("%s",dlerror()) ;
	else
	    sv_setiv( ST(0), (IV)RETVAL);


void *
dl_find_symbol(libhandle, symbolname)
	void *		libhandle
	char *		symbolname
	CODE:
	DLDEBUG(2,PerlIO_printf(PerlIO_stderr(), "dl_find_symbol(handle=%x, symbol=%s)\n",
		libhandle, symbolname));
	RETVAL = dlsym(libhandle, symbolname);
	DLDEBUG(2,PerlIO_printf(PerlIO_stderr(), "  symbolref = %x\n", RETVAL));
	ST(0) = sv_newmortal() ;
	if (RETVAL == NULL)
	    SaveError("%s",dlerror()) ;
	else
	    sv_setiv( ST(0), (IV)RETVAL);


void
dl_undef_symbols()
	PPCODE:



# These functions should not need changing on any platform:

void
dl_install_xsub(perl_name, symref, filename="$Package")
    char *	perl_name
    void *	symref 
    char *	filename
    CODE:
    DLDEBUG(2,PerlIO_printf(PerlIO_stderr(), "dl_install_xsub(name=%s, symref=%x)\n",
	perl_name, symref));
    ST(0)=sv_2mortal(newRV((SV*)newXS(perl_name, (void(*)())symref, filename)));


char *
dl_error()
    CODE:
    RETVAL = LastError ;
    OUTPUT:
    RETVAL

# end.
