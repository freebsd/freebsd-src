/*
 * @(#)dlfcn.c	1.11 revision of 96/04/10  20:12:51
 * This is an unpublished work copyright (c) 1992 HELIOS Software GmbH
 * 30159 Hannover, Germany
 */

/*
 * Changes marked with `--jwe' were made on April 7 1996 by John W. Eaton
 * <jwe@bevo.che.wisc.edu> to support g++ and/or use with Octave.
 */

/*
 * This makes my life easier with Octave.  --jwe
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ldr.h>
#include <a.out.h>
#include <ldfcn.h>
#include "dlfcn.h"

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
 * xlC uses the following structure to list its constructors and
 * destructors. This is gleaned from the output of munch.
 */
typedef struct {
	void (*init)(void);		/* call static constructors */
	void (*term)(void);		/* call static destructors */
} Cdtor, *CdtorPtr;

typedef void (*GccCDtorPtr)(void);

/*
 * The void * handle returned from dlopen is actually a ModulePtr.
 */
typedef struct Module {
	struct Module	*next;
	char		*name;		/* module name for refcounting */
	int		refCnt;		/* the number of references */
	void		*entry;		/* entry point from load */
	struct dl_info	*info;		/* optional init/terminate functions */
	CdtorPtr	cdtors;		/* optional C++ constructors */
	GccCDtorPtr	gcc_ctor;	/* g++ constructors  --jwe */
	GccCDtorPtr	gcc_dtor;	/* g++ destructors  --jwe */
	int		nExports;	/* the number of exports found */
	ExportPtr	exports;	/* the array of exports */
} Module, *ModulePtr;

/*
 * We keep a list of all loaded modules to be able to call the fini
 * handlers and destructors at atexit() time.
 */
static ModulePtr modList;

/*
 * The last error from one of the dl* routines is kept in static
 * variables here. Each error is returned only once to the caller.
 */
static char errbuf[BUFSIZ];
static int errvalid;

/*
 * The `fixed' gcc header files on AIX 3.2.5 provide a prototype for
 * strdup().  --jwe
 */
#ifndef HAVE_STRDUP
extern char *strdup(const char *);
#endif
static void caterr(char *);
static int readExports(ModulePtr);
static void terminate(void);
static void *findMain(void);

void *dlopen(const char *path, int mode)
{
	ModulePtr mp;
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
	 * Scan the list of modules if we have the module already loaded.
	 */
	for (mp = modList; mp; mp = mp->next)
		if (strcmp(mp->name, path) == 0) {
			mp->refCnt++;
			return mp;
		}
	if ((mp = (ModulePtr)calloc(1, sizeof(*mp))) == NULL) {
		errvalid++;
		snprintf (errbuf, "calloc: %s", strerror(errno));
		return NULL;
	}
	if ((mp->name = strdup(path)) == NULL) {
		errvalid++;
		snprintf (errbuf, "strdup: %s", strerror(errno));
		free(mp);
		return NULL;
	}
	/*
	 * load should be declared load(const char *...). Thus we
	 * cast the path to a normal char *. Ugly.
	 */
	if ((mp->entry = (void *)load((char *)path, L_NOAUTODEFER, NULL)) == NULL) {
		free(mp->name);
		free(mp);
		errvalid++;
		snprintf (errbuf, sizeof(errbuf),
			  "dlopen: %s: ", path);
		/*
		 * If AIX says the file is not executable, the error
		 * can be further described by querying the loader about
		 * the last error.
		 */
		if (errno == ENOEXEC) {
			char *tmp[BUFSIZ/sizeof(char *)];
			if (loadquery(L_GETMESSAGES, tmp, sizeof(tmp)) == -1)
				strlcpy(errbuf,
						strerror(errno),
						sizeof(errbuf));
			else {
				char **p;
				for (p = tmp; *p; p++)
					caterr(*p);
			}
		} else
			strlcat(errbuf,
					strerror(errno),
					sizeof(errbuf));
		return NULL;
	}
	mp->refCnt = 1;
	mp->next = modList;
	modList = mp;
	if (loadbind(0, mainModule, mp->entry) == -1) {
		dlclose(mp);
		errvalid++;
		snprintf (errbuf, sizeof(errbuf),
			  "loadbind: %s", strerror(errno));
		return NULL;
	}
	/*
	 * If the user wants global binding, loadbind against all other
	 * loaded modules.
	 */
	if (mode & RTLD_GLOBAL) {
		ModulePtr mp1;
		for (mp1 = mp->next; mp1; mp1 = mp1->next)
			if (loadbind(0, mp1->entry, mp->entry) == -1) {
				dlclose(mp);
				errvalid++;
				snprintf (errbuf, sizeof(errbuf),
					  "loadbind: %s",
					  strerror(errno));
				return NULL;
			}
	}
	if (readExports(mp) == -1) {
		dlclose(mp);
		return NULL;
	}
	/*
	 * If there is a dl_info structure, call the init function.
	 */
	if (mp->info = (struct dl_info *)dlsym(mp, "dl_info")) {
		if (mp->info->init)
			(*mp->info->init)();
	} else
		errvalid = 0;
	/*
	 * If the shared object was compiled using xlC we will need
	 * to call static constructors (and later on dlclose destructors).
	 */
	if (mp->cdtors = (CdtorPtr)dlsym(mp, "__cdtors")) {
		CdtorPtr cp = mp->cdtors;
		while (cp->init || cp->term) {
			if (cp->init && cp->init != (void (*)(void))0xffffffff)
				(*cp->init)();
			cp++;
		}
	/*
	 * If the shared object was compiled using g++, we will need
	 * to call global constructors using the _GLOBAL__DI function,
	 * and later, global destructors using the _GLOBAL_DD
	 * funciton.  --jwe
	 */
	} else if (mp->gcc_ctor = (GccCDtorPtr)dlsym(mp, "_GLOBAL__DI")) {
		(*mp->gcc_ctor)();
		mp->gcc_dtor = (GccCDtorPtr)dlsym(mp, "_GLOBAL__DD"); 
	} else
		errvalid = 0;
	return mp;
}

/*
 * Attempt to decipher an AIX loader error message and append it
 * to our static error message buffer.
 */
static void caterr(char *s)
{
	char *p = s;

	while (*p >= '0' && *p <= '9')
		p++;
	switch(atoi(s)) {
	case L_ERROR_TOOMANY:
		strlcat(errbuf, "to many errors", sizeof(errbuf));
		break;
	case L_ERROR_NOLIB:
		strlcat(errbuf, "can't load library", sizeof(errbuf));
		strlcat(errbuf, p, sizeof(errbuf));
		break;
	case L_ERROR_UNDEF:
		strlcat(errbuf, "can't find symbol", sizeof(errbuf));
		strlcat(errbuf, p, sizeof(errbuf));
		break;
	case L_ERROR_RLDBAD:
		strlcat(errbuf, "bad RLD", sizeof(errbuf));
		strlcat(errbuf, p, sizeof(errbuf));
		break;
	case L_ERROR_FORMAT:
		strlcat(errbuf, "bad exec format in", sizeof(errbuf));
		strlcat(errbuf, p, sizeof(errbuf));
		break;
	case L_ERROR_ERRNO:
		strlcat(errbuf, strerror(atoi(++p)), sizeof(errbuf));
		break;
	default:
		strlcat(errbuf, s, sizeof(errbuf));
		break;
	}
}

void *dlsym(void *handle, const char *symbol)
{
	ModulePtr mp = (ModulePtr)handle;
	ExportPtr ep;
	int i;

	/*
	 * Could speed up the search, but I assume that one assigns
	 * the result to function pointers anyways.
	 */
	for (ep = mp->exports, i = mp->nExports; i; i--, ep++)
		if (strcmp(ep->name, symbol) == 0)
			return ep->addr;
	errvalid++;
	snprintf (errbuf, sizeof(errbuf),
		  "dlsym: undefined symbol %s", symbol);		  
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
	ModulePtr mp = (ModulePtr)handle;
	int result;
	ModulePtr mp1;

	if (--mp->refCnt > 0)
		return 0;
	if (mp->info && mp->info->fini)
		(*mp->info->fini)();
	if (mp->cdtors) {
		CdtorPtr cp = mp->cdtors;
		while (cp->init || cp->term) {
			if (cp->term && cp->init != (void (*)(void))0xffffffff)
				(*cp->term)();
			cp++;
		}
	/*
	 * If the function to handle global destructors for g++
	 * exists, call it.  --jwe
	 */
	} else if (mp->gcc_dtor) {
	        (*mp->gcc_dtor)();
	}
	result = unload(mp->entry);
	if (result == -1) {
		errvalid++;
		snprintf (errbuf, sizeof(errbuf),
			  "%s", strerror(errno));
	}
	if (mp->exports) {
		ExportPtr ep;
		int i;
		for (ep = mp->exports, i = mp->nExports; i; i--, ep++)
			if (ep->name)
				free(ep->name);
		free(mp->exports);
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
	free(mp->name);
	free(mp);
	return result;
}

static void terminate(void)
{
	while (modList)
		dlclose(modList);
}

/*
 * Build the export table from the XCOFF .loader section.
 */
static int readExports(ModulePtr mp)
{
	LDFILE *ldp = NULL;
	SCNHDR sh, shdata;
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
			snprintf(errbuf, sizeof(errbuf),
				 "readExports: %s",
				 strerror(errno));
			return -1;
		}
		/*
		 * The module might be loaded due to the LIBPATH
		 * environment variable. Search for the loaded
		 * module using L_GETINFO.
		 */
		if ((buf = malloc(size)) == NULL) {
			errvalid++;
			snprintf(errbuf, sizeof(errbuf),
				 "readExports: %s",
				 strerror(errno));
			return -1;
		}
		while ((i = loadquery(L_GETINFO, buf, size)) == -1 && errno == ENOMEM) {
			free(buf);
			size += 4*1024;
			if ((buf = malloc(size)) == NULL) {
				errvalid++;
				snprintf(errbuf, sizeof(errbuf),
					 "readExports: %s",
					 strerror(errno));
				return -1;
			}
		}
		if (i == -1) {
			errvalid++;
			snprintf(errbuf, sizeof(errbuf),
				 "readExports: %s",
				 strerror(errno));
			free(buf);
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
		free(buf);
		if (!ldp) {
			errvalid++;
			snprintf (errbuf, sizeof(errbuf),
				  "readExports: %s", strerror(errno));
			return -1;
		}
	}
	if (TYPE(ldp) != U802TOCMAGIC) {
		errvalid++;
		snprintf(errbuf, sizeof(errbuf), "readExports: bad magic");
		while(ldclose(ldp) == FAILURE)
			;
		return -1;
	}
	/*
	 * Get the padding for the data section. This is needed for
	 * AIX 4.1 compilers. This is used when building the final
	 * function pointer to the exported symbol.
	 */
	if (ldnshread(ldp, _DATA, &shdata) != SUCCESS) {
		errvalid++;
		snprintf(errbuf, sizeof(errbuf),
			 "readExports: cannot read data section header");
		while(ldclose(ldp) == FAILURE)
			;
		return -1;
	}
	if (ldnshread(ldp, _LOADER, &sh) != SUCCESS) {
		errvalid++;
		snprintf(errbuf, sizeof(errbuf),
			 "readExports: cannot read loader section header");
		while(ldclose(ldp) == FAILURE)
			;
		return -1;
	}
	/*
	 * We read the complete loader section in one chunk, this makes
	 * finding long symbol names residing in the string table easier.
	 */
	if ((ldbuf = (char *)malloc(sh.s_size)) == NULL) {
		errvalid++;
		snprintf (errbuf, sizeof(errbuf),
			  "readExports: %s", strerror(errno));
		while(ldclose(ldp) == FAILURE)
			;
		return -1;
	}
	if (FSEEK(ldp, sh.s_scnptr, BEGINNING) != OKFSEEK) {
		errvalid++;
		snprintf(errbuf, sizeof(errbuf),
			 "readExports: cannot seek to loader section");
		free(ldbuf);
		while(ldclose(ldp) == FAILURE)
			;
		return -1;
	}
	if (FREAD(ldbuf, sh.s_size, 1, ldp) != 1) {
		errvalid++;
		snprintf(errbuf, sizeof(errbuf),
			 "readExports: cannot read loader section");
		free(ldbuf);
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
	if ((mp->exports = (ExportPtr)calloc(mp->nExports, sizeof(*mp->exports))) == NULL) {
		errvalid++;
		snprintf (errbuf, sizeof(errbuf),
			  "readExports: %s", strerror(errno));
		free(ldbuf);
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
		char tmpsym[SYMNMLEN+1];
		if (!LDR_EXPORT(*ls))
			continue;
		if (ls->l_zeroes == 0)
			symname = ls->l_offset+lhp->l_stoff+ldbuf;
		else {
			/*
			 * The l_name member is not zero terminated, we
			 * must copy the first SYMNMLEN chars and make
			 * sure we have a zero byte at the end.
			 */
		        strlcpy (tmpsym, ls->l_name,
					 SYMNMLEN + 1);
			symname = tmpsym;
		}
		ep->name = strdup(symname);
		ep->addr = (void *)((unsigned long)mp->entry +
					ls->l_value - shdata.s_vaddr);
		ep++;
	}
	free(ldbuf);
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

	if ((buf = malloc(size)) == NULL) {
		errvalid++;
		snprintf (errbuf, sizeof(errbuf),
			  "findMail: %s", strerror(errno));
		return NULL;
	}
	while ((i = loadquery(L_GETINFO, buf, size)) == -1 && errno == ENOMEM) {
		free(buf);
		size += 4*1024;
		if ((buf = malloc(size)) == NULL) {
			errvalid++;
			snprintf (errbuf, sizeof(errbuf),
				  "findMail: %s", strerror(errno));
			return NULL;
		}
	}
	if (i == -1) {
		errvalid++;
		snprintf (errbuf, sizeof(errbuf),
			  "findMail: %s", strerror(errno));
		free(buf);
		return NULL;
	}
	/*
	 * The first entry is the main module. The entry point
	 * returned by load() does actually point to the data
	 * segment origin.
	 */
	lp = (struct ld_info *)buf;
	ret = lp->ldinfo_dataorg;
	free(buf);
	return ret;
}
