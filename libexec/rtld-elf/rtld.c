/*-
 * Copyright 1996, 1997, 1998, 1999, 2000 John D. Polstra.
 * Copyright 2003 Alexander Kabaev <kan@FreeBSD.ORG>.
 * Copyright 2009, 2010, 2011 Konstantin Belousov <kib@FreeBSD.ORG>.
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
 * $FreeBSD$
 */

/*
 * Dynamic linker for ELF.
 *
 * John Polstra <jdp@polstra.com>.
 */

#ifndef __GNUC__
#error "GCC is needed to compile this file"
#endif

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#include <sys/ktrace.h>

#include <dlfcn.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"
#include "rtld.h"
#include "libmap.h"
#include "rtld_tls.h"

#ifndef COMPAT_32BIT
#define PATH_RTLD	"/libexec/ld-elf.so.1"
#else
#define PATH_RTLD	"/libexec/ld-elf32.so.1"
#endif

/* Types. */
typedef void (*func_ptr_type)();
typedef void * (*path_enum_proc) (const char *path, size_t len, void *arg);

/*
 * Function declarations.
 */
static const char *basename(const char *);
static void die(void) __dead2;
static void digest_dynamic1(Obj_Entry *, int, const Elf_Dyn **,
    const Elf_Dyn **);
static void digest_dynamic2(Obj_Entry *, const Elf_Dyn *, const Elf_Dyn *);
static void digest_dynamic(Obj_Entry *, int);
static Obj_Entry *digest_phdr(const Elf_Phdr *, int, caddr_t, const char *);
static Obj_Entry *dlcheck(void *);
static Obj_Entry *dlopen_object(const char *name, Obj_Entry *refobj,
    int lo_flags, int mode);
static Obj_Entry *do_load_object(int, const char *, char *, struct stat *, int);
static int do_search_info(const Obj_Entry *obj, int, struct dl_serinfo *);
static bool donelist_check(DoneList *, const Obj_Entry *);
static void errmsg_restore(char *);
static char *errmsg_save(void);
static void *fill_search_info(const char *, size_t, void *);
static char *find_library(const char *, const Obj_Entry *);
static const char *gethints(void);
static void init_dag(Obj_Entry *);
static void init_rtld(caddr_t, Elf_Auxinfo **);
static void initlist_add_neededs(Needed_Entry *, Objlist *);
static void initlist_add_objects(Obj_Entry *, Obj_Entry **, Objlist *);
static void linkmap_add(Obj_Entry *);
static void linkmap_delete(Obj_Entry *);
static void load_filtees(Obj_Entry *, int flags, RtldLockState *);
static void unload_filtees(Obj_Entry *);
static int load_needed_objects(Obj_Entry *, int);
static int load_preload_objects(void);
static Obj_Entry *load_object(const char *, const Obj_Entry *, int);
static void map_stacks_exec(RtldLockState *);
static Obj_Entry *obj_from_addr(const void *);
static void objlist_call_fini(Objlist *, Obj_Entry *, RtldLockState *);
static void objlist_call_init(Objlist *, RtldLockState *);
static void objlist_clear(Objlist *);
static Objlist_Entry *objlist_find(Objlist *, const Obj_Entry *);
static void objlist_init(Objlist *);
static void objlist_push_head(Objlist *, Obj_Entry *);
static void objlist_push_tail(Objlist *, Obj_Entry *);
static void objlist_remove(Objlist *, Obj_Entry *);
static void *path_enumerate(const char *, path_enum_proc, void *);
static int relocate_objects(Obj_Entry *, bool, Obj_Entry *, RtldLockState *);
static int rtld_dirname(const char *, char *);
static int rtld_dirname_abs(const char *, char *);
static void rtld_exit(void);
static char *search_library_path(const char *, const char *);
static const void **get_program_var_addr(const char *, RtldLockState *);
static void set_program_var(const char *, const void *);
static int symlook_default(SymLook *, const Obj_Entry *refobj);
static int symlook_global(SymLook *, DoneList *);
static void symlook_init_from_req(SymLook *, const SymLook *);
static int symlook_list(SymLook *, const Objlist *, DoneList *);
static int symlook_needed(SymLook *, const Needed_Entry *, DoneList *);
static int symlook_obj1(SymLook *, const Obj_Entry *);
static void trace_loaded_objects(Obj_Entry *);
static void unlink_object(Obj_Entry *);
static void unload_object(Obj_Entry *);
static void unref_dag(Obj_Entry *);
static void ref_dag(Obj_Entry *);
static int origin_subst_one(char **, const char *, const char *,
  const char *, char *);
static char *origin_subst(const char *, const char *);
static int  rtld_verify_versions(const Objlist *);
static int  rtld_verify_object_versions(Obj_Entry *);
static void object_add_name(Obj_Entry *, const char *);
static int  object_match_name(const Obj_Entry *, const char *);
static void ld_utrace_log(int, void *, void *, size_t, int, const char *);
static void rtld_fill_dl_phdr_info(const Obj_Entry *obj,
    struct dl_phdr_info *phdr_info);

void r_debug_state(struct r_debug *, struct link_map *);

/*
 * Data declarations.
 */
static char *error_message;	/* Message for dlerror(), or NULL */
struct r_debug r_debug;		/* for GDB; */
static bool libmap_disable;	/* Disable libmap */
static bool ld_loadfltr;	/* Immediate filters processing */
static char *libmap_override;	/* Maps to use in addition to libmap.conf */
static bool trust;		/* False for setuid and setgid programs */
static bool dangerous_ld_env;	/* True if environment variables have been
				   used to affect the libraries loaded */
static char *ld_bind_now;	/* Environment variable for immediate binding */
static char *ld_debug;		/* Environment variable for debugging */
static char *ld_library_path;	/* Environment variable for search path */
static char *ld_preload;	/* Environment variable for libraries to
				   load first */
static char *ld_elf_hints_path;	/* Environment variable for alternative hints path */
static char *ld_tracing;	/* Called from ldd to print libs */
static char *ld_utrace;		/* Use utrace() to log events. */
static Obj_Entry *obj_list;	/* Head of linked list of shared objects */
static Obj_Entry **obj_tail;	/* Link field of last object in list */
static Obj_Entry *obj_main;	/* The main program shared object */
static Obj_Entry obj_rtld;	/* The dynamic linker shared object */
static unsigned int obj_count;	/* Number of objects in obj_list */
static unsigned int obj_loads;	/* Number of objects in obj_list */

static Objlist list_global =	/* Objects dlopened with RTLD_GLOBAL */
  STAILQ_HEAD_INITIALIZER(list_global);
static Objlist list_main =	/* Objects loaded at program startup */
  STAILQ_HEAD_INITIALIZER(list_main);
static Objlist list_fini =	/* Objects needing fini() calls */
  STAILQ_HEAD_INITIALIZER(list_fini);

Elf_Sym sym_zero;		/* For resolving undefined weak refs. */

#define GDB_STATE(s,m)	r_debug.r_state = s; r_debug_state(&r_debug,m);

extern Elf_Dyn _DYNAMIC;
#pragma weak _DYNAMIC
#ifndef RTLD_IS_DYNAMIC
#define	RTLD_IS_DYNAMIC()	(&_DYNAMIC != NULL)
#endif

int osreldate, pagesize;

static int stack_prot = PROT_READ | PROT_WRITE | RTLD_DEFAULT_STACK_EXEC;
static int max_stack_flags;

/*
 * Global declarations normally provided by crt1.  The dynamic linker is
 * not built with crt1, so we have to provide them ourselves.
 */
char *__progname;
char **environ;

/*
 * Globals to control TLS allocation.
 */
size_t tls_last_offset;		/* Static TLS offset of last module */
size_t tls_last_size;		/* Static TLS size of last module */
size_t tls_static_space;	/* Static TLS space allocated */
int tls_dtv_generation = 1;	/* Used to detect when dtv size changes  */
int tls_max_index = 1;		/* Largest module index allocated */

/*
 * Fill in a DoneList with an allocation large enough to hold all of
 * the currently-loaded objects.  Keep this as a macro since it calls
 * alloca and we want that to occur within the scope of the caller.
 */
#define donelist_init(dlp)					\
    ((dlp)->objs = alloca(obj_count * sizeof (dlp)->objs[0]),	\
    assert((dlp)->objs != NULL),				\
    (dlp)->num_alloc = obj_count,				\
    (dlp)->num_used = 0)

#define	UTRACE_DLOPEN_START		1
#define	UTRACE_DLOPEN_STOP		2
#define	UTRACE_DLCLOSE_START		3
#define	UTRACE_DLCLOSE_STOP		4
#define	UTRACE_LOAD_OBJECT		5
#define	UTRACE_UNLOAD_OBJECT		6
#define	UTRACE_ADD_RUNDEP		7
#define	UTRACE_PRELOAD_FINISHED		8
#define	UTRACE_INIT_CALL		9
#define	UTRACE_FINI_CALL		10

struct utrace_rtld {
	char sig[4];			/* 'RTLD' */
	int event;
	void *handle;
	void *mapbase;			/* Used for 'parent' and 'init/fini' */
	size_t mapsize;
	int refcnt;			/* Used for 'mode' */
	char name[MAXPATHLEN];
};

#define	LD_UTRACE(e, h, mb, ms, r, n) do {			\
	if (ld_utrace != NULL)					\
		ld_utrace_log(e, h, mb, ms, r, n);		\
} while (0)

static void
ld_utrace_log(int event, void *handle, void *mapbase, size_t mapsize,
    int refcnt, const char *name)
{
	struct utrace_rtld ut;

	ut.sig[0] = 'R';
	ut.sig[1] = 'T';
	ut.sig[2] = 'L';
	ut.sig[3] = 'D';
	ut.event = event;
	ut.handle = handle;
	ut.mapbase = mapbase;
	ut.mapsize = mapsize;
	ut.refcnt = refcnt;
	bzero(ut.name, sizeof(ut.name));
	if (name)
		strlcpy(ut.name, name, sizeof(ut.name));
	utrace(&ut, sizeof(ut));
}

/*
 * Main entry point for dynamic linking.  The first argument is the
 * stack pointer.  The stack is expected to be laid out as described
 * in the SVR4 ABI specification, Intel 386 Processor Supplement.
 * Specifically, the stack pointer points to a word containing
 * ARGC.  Following that in the stack is a null-terminated sequence
 * of pointers to argument strings.  Then comes a null-terminated
 * sequence of pointers to environment strings.  Finally, there is a
 * sequence of "auxiliary vector" entries.
 *
 * The second argument points to a place to store the dynamic linker's
 * exit procedure pointer and the third to a place to store the main
 * program's object.
 *
 * The return value is the main program's entry point.
 */
func_ptr_type
_rtld(Elf_Addr *sp, func_ptr_type *exit_proc, Obj_Entry **objp)
{
    Elf_Auxinfo *aux_info[AT_COUNT];
    int i;
    int argc;
    char **argv;
    char **env;
    Elf_Auxinfo *aux;
    Elf_Auxinfo *auxp;
    const char *argv0;
    Objlist_Entry *entry;
    Obj_Entry *obj;
    Obj_Entry **preload_tail;
    Objlist initlist;
    RtldLockState lockstate;

    /*
     * On entry, the dynamic linker itself has not been relocated yet.
     * Be very careful not to reference any global data until after
     * init_rtld has returned.  It is OK to reference file-scope statics
     * and string constants, and to call static and global functions.
     */

    /* Find the auxiliary vector on the stack. */
    argc = *sp++;
    argv = (char **) sp;
    sp += argc + 1;	/* Skip over arguments and NULL terminator */
    env = (char **) sp;
    while (*sp++ != 0)	/* Skip over environment, and NULL terminator */
	;
    aux = (Elf_Auxinfo *) sp;

    /* Digest the auxiliary vector. */
    for (i = 0;  i < AT_COUNT;  i++)
	aux_info[i] = NULL;
    for (auxp = aux;  auxp->a_type != AT_NULL;  auxp++) {
	if (auxp->a_type < AT_COUNT)
	    aux_info[auxp->a_type] = auxp;
    }

    /* Initialize and relocate ourselves. */
    assert(aux_info[AT_BASE] != NULL);
    init_rtld((caddr_t) aux_info[AT_BASE]->a_un.a_ptr, aux_info);

    __progname = obj_rtld.path;
    argv0 = argv[0] != NULL ? argv[0] : "(null)";
    environ = env;

    trust = !issetugid();

    ld_bind_now = getenv(LD_ "BIND_NOW");
    /* 
     * If the process is tainted, then we un-set the dangerous environment
     * variables.  The process will be marked as tainted until setuid(2)
     * is called.  If any child process calls setuid(2) we do not want any
     * future processes to honor the potentially un-safe variables.
     */
    if (!trust) {
        if (unsetenv(LD_ "PRELOAD") || unsetenv(LD_ "LIBMAP") ||
	    unsetenv(LD_ "LIBRARY_PATH") || unsetenv(LD_ "LIBMAP_DISABLE") ||
	    unsetenv(LD_ "DEBUG") || unsetenv(LD_ "ELF_HINTS_PATH") ||
	    unsetenv(LD_ "LOADFLTR")) {
		_rtld_error("environment corrupt; aborting");
		die();
	}
    }
    ld_debug = getenv(LD_ "DEBUG");
    libmap_disable = getenv(LD_ "LIBMAP_DISABLE") != NULL;
    libmap_override = getenv(LD_ "LIBMAP");
    ld_library_path = getenv(LD_ "LIBRARY_PATH");
    ld_preload = getenv(LD_ "PRELOAD");
    ld_elf_hints_path = getenv(LD_ "ELF_HINTS_PATH");
    ld_loadfltr = getenv(LD_ "LOADFLTR") != NULL;
    dangerous_ld_env = libmap_disable || (libmap_override != NULL) ||
	(ld_library_path != NULL) || (ld_preload != NULL) ||
	(ld_elf_hints_path != NULL) || ld_loadfltr;
    ld_tracing = getenv(LD_ "TRACE_LOADED_OBJECTS");
    ld_utrace = getenv(LD_ "UTRACE");

    if ((ld_elf_hints_path == NULL) || strlen(ld_elf_hints_path) == 0)
	ld_elf_hints_path = _PATH_ELF_HINTS;

    if (ld_debug != NULL && *ld_debug != '\0')
	debug = 1;
    dbg("%s is initialized, base address = %p", __progname,
	(caddr_t) aux_info[AT_BASE]->a_un.a_ptr);
    dbg("RTLD dynamic = %p", obj_rtld.dynamic);
    dbg("RTLD pltgot  = %p", obj_rtld.pltgot);

    dbg("initializing thread locks");
    lockdflt_init();

    /*
     * Load the main program, or process its program header if it is
     * already loaded.
     */
    if (aux_info[AT_EXECFD] != NULL) {	/* Load the main program. */
	int fd = aux_info[AT_EXECFD]->a_un.a_val;
	dbg("loading main program");
	obj_main = map_object(fd, argv0, NULL);
	close(fd);
	if (obj_main == NULL)
	    die();
	max_stack_flags = obj->stack_flags;
    } else {				/* Main program already loaded. */
	const Elf_Phdr *phdr;
	int phnum;
	caddr_t entry;

	dbg("processing main program's program header");
	assert(aux_info[AT_PHDR] != NULL);
	phdr = (const Elf_Phdr *) aux_info[AT_PHDR]->a_un.a_ptr;
	assert(aux_info[AT_PHNUM] != NULL);
	phnum = aux_info[AT_PHNUM]->a_un.a_val;
	assert(aux_info[AT_PHENT] != NULL);
	assert(aux_info[AT_PHENT]->a_un.a_val == sizeof(Elf_Phdr));
	assert(aux_info[AT_ENTRY] != NULL);
	entry = (caddr_t) aux_info[AT_ENTRY]->a_un.a_ptr;
	if ((obj_main = digest_phdr(phdr, phnum, entry, argv0)) == NULL)
	    die();
    }

    if (aux_info[AT_EXECPATH] != 0) {
	    char *kexecpath;
	    char buf[MAXPATHLEN];

	    kexecpath = aux_info[AT_EXECPATH]->a_un.a_ptr;
	    dbg("AT_EXECPATH %p %s", kexecpath, kexecpath);
	    if (kexecpath[0] == '/')
		    obj_main->path = kexecpath;
	    else if (getcwd(buf, sizeof(buf)) == NULL ||
		     strlcat(buf, "/", sizeof(buf)) >= sizeof(buf) ||
		     strlcat(buf, kexecpath, sizeof(buf)) >= sizeof(buf))
		    obj_main->path = xstrdup(argv0);
	    else
		    obj_main->path = xstrdup(buf);
    } else {
	    dbg("No AT_EXECPATH");
	    obj_main->path = xstrdup(argv0);
    }
    dbg("obj_main path %s", obj_main->path);
    obj_main->mainprog = true;

    if (aux_info[AT_STACKPROT] != NULL &&
      aux_info[AT_STACKPROT]->a_un.a_val != 0)
	    stack_prot = aux_info[AT_STACKPROT]->a_un.a_val;

    /*
     * Get the actual dynamic linker pathname from the executable if
     * possible.  (It should always be possible.)  That ensures that
     * gdb will find the right dynamic linker even if a non-standard
     * one is being used.
     */
    if (obj_main->interp != NULL &&
      strcmp(obj_main->interp, obj_rtld.path) != 0) {
	free(obj_rtld.path);
	obj_rtld.path = xstrdup(obj_main->interp);
        __progname = obj_rtld.path;
    }

    digest_dynamic(obj_main, 0);

    linkmap_add(obj_main);
    linkmap_add(&obj_rtld);

    /* Link the main program into the list of objects. */
    *obj_tail = obj_main;
    obj_tail = &obj_main->next;
    obj_count++;
    obj_loads++;
    /* Make sure we don't call the main program's init and fini functions. */
    obj_main->init = obj_main->fini = (Elf_Addr)NULL;

    /* Initialize a fake symbol for resolving undefined weak references. */
    sym_zero.st_info = ELF_ST_INFO(STB_GLOBAL, STT_NOTYPE);
    sym_zero.st_shndx = SHN_UNDEF;
    sym_zero.st_value = -(uintptr_t)obj_main->relocbase;

    if (!libmap_disable)
        libmap_disable = (bool)lm_init(libmap_override);

    dbg("loading LD_PRELOAD libraries");
    if (load_preload_objects() == -1)
	die();
    preload_tail = obj_tail;

    dbg("loading needed objects");
    if (load_needed_objects(obj_main, 0) == -1)
	die();

    /* Make a list of all objects loaded at startup. */
    for (obj = obj_list;  obj != NULL;  obj = obj->next) {
	objlist_push_tail(&list_main, obj);
    	obj->refcount++;
    }

    dbg("checking for required versions");
    if (rtld_verify_versions(&list_main) == -1 && !ld_tracing)
	die();

    if (ld_tracing) {		/* We're done */
	trace_loaded_objects(obj_main);
	exit(0);
    }

    if (getenv(LD_ "DUMP_REL_PRE") != NULL) {
       dump_relocations(obj_main);
       exit (0);
    }

    /* setup TLS for main thread */
    dbg("initializing initial thread local storage");
    STAILQ_FOREACH(entry, &list_main, link) {
	/*
	 * Allocate all the initial objects out of the static TLS
	 * block even if they didn't ask for it.
	 */
	allocate_tls_offset(entry->obj);
    }
    allocate_initial_tls(obj_list);

    if (relocate_objects(obj_main,
      ld_bind_now != NULL && *ld_bind_now != '\0', &obj_rtld, NULL) == -1)
	die();

    dbg("doing copy relocations");
    if (do_copy_relocations(obj_main) == -1)
	die();

    if (getenv(LD_ "DUMP_REL_POST") != NULL) {
       dump_relocations(obj_main);
       exit (0);
    }

    dbg("initializing key program variables");
    set_program_var("__progname", argv[0] != NULL ? basename(argv[0]) : "");
    set_program_var("environ", env);
    set_program_var("__elf_aux_vector", aux);

    /* Make a list of init functions to call. */
    objlist_init(&initlist);
    initlist_add_objects(obj_list, preload_tail, &initlist);

    r_debug_state(NULL, &obj_main->linkmap); /* say hello to gdb! */

    map_stacks_exec(NULL);

    wlock_acquire(rtld_bind_lock, &lockstate);
    objlist_call_init(&initlist, &lockstate);
    objlist_clear(&initlist);
    dbg("loading filtees");
    for (obj = obj_list->next; obj != NULL; obj = obj->next) {
	if (ld_loadfltr || obj->z_loadfltr)
	    load_filtees(obj, 0, &lockstate);
    }
    lock_release(rtld_bind_lock, &lockstate);

    dbg("transferring control to program entry point = %p", obj_main->entry);

    /* Return the exit procedure and the program entry point. */
    *exit_proc = rtld_exit;
    *objp = obj_main;
    return (func_ptr_type) obj_main->entry;
}

Elf_Addr
_rtld_bind(Obj_Entry *obj, Elf_Size reloff)
{
    const Elf_Rel *rel;
    const Elf_Sym *def;
    const Obj_Entry *defobj;
    Elf_Addr *where;
    Elf_Addr target;
    RtldLockState lockstate;

    rlock_acquire(rtld_bind_lock, &lockstate);
    if (setjmp(lockstate.env) != 0)
	    lock_upgrade(rtld_bind_lock, &lockstate);
    if (obj->pltrel)
	rel = (const Elf_Rel *) ((caddr_t) obj->pltrel + reloff);
    else
	rel = (const Elf_Rel *) ((caddr_t) obj->pltrela + reloff);

    where = (Elf_Addr *) (obj->relocbase + rel->r_offset);
    def = find_symdef(ELF_R_SYM(rel->r_info), obj, &defobj, true, NULL,
	&lockstate);
    if (def == NULL)
	die();

    target = (Elf_Addr)(defobj->relocbase + def->st_value);

    dbg("\"%s\" in \"%s\" ==> %p in \"%s\"",
      defobj->strtab + def->st_name, basename(obj->path),
      (void *)target, basename(defobj->path));

    /*
     * Write the new contents for the jmpslot. Note that depending on
     * architecture, the value which we need to return back to the
     * lazy binding trampoline may or may not be the target
     * address. The value returned from reloc_jmpslot() is the value
     * that the trampoline needs.
     */
    target = reloc_jmpslot(where, target, defobj, obj, rel);
    lock_release(rtld_bind_lock, &lockstate);
    return target;
}

/*
 * Error reporting function.  Use it like printf.  If formats the message
 * into a buffer, and sets things up so that the next call to dlerror()
 * will return the message.
 */
void
_rtld_error(const char *fmt, ...)
{
    static char buf[512];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    error_message = buf;
    va_end(ap);
}

/*
 * Return a dynamically-allocated copy of the current error message, if any.
 */
static char *
errmsg_save(void)
{
    return error_message == NULL ? NULL : xstrdup(error_message);
}

/*
 * Restore the current error message from a copy which was previously saved
 * by errmsg_save().  The copy is freed.
 */
static void
errmsg_restore(char *saved_msg)
{
    if (saved_msg == NULL)
	error_message = NULL;
    else {
	_rtld_error("%s", saved_msg);
	free(saved_msg);
    }
}

static const char *
basename(const char *name)
{
    const char *p = strrchr(name, '/');
    return p != NULL ? p + 1 : name;
}

static struct utsname uts;

static int
origin_subst_one(char **res, const char *real, const char *kw, const char *subst,
    char *may_free)
{
    const char *p, *p1;
    char *res1;
    int subst_len;
    int kw_len;

    res1 = *res = NULL;
    p = real;
    subst_len = kw_len = 0;
    for (;;) {
	 p1 = strstr(p, kw);
	 if (p1 != NULL) {
	     if (subst_len == 0) {
		 subst_len = strlen(subst);
		 kw_len = strlen(kw);
	     }
	     if (*res == NULL) {
		 *res = xmalloc(PATH_MAX);
		 res1 = *res;
	     }
	     if ((res1 - *res) + subst_len + (p1 - p) >= PATH_MAX) {
		 _rtld_error("Substitution of %s in %s cannot be performed",
		     kw, real);
		 if (may_free != NULL)
		     free(may_free);
		 free(res);
		 return (false);
	     }
	     memcpy(res1, p, p1 - p);
	     res1 += p1 - p;
	     memcpy(res1, subst, subst_len);
	     res1 += subst_len;
	     p = p1 + kw_len;
	 } else {
	    if (*res == NULL) {
		if (may_free != NULL)
		    *res = may_free;
		else
		    *res = xstrdup(real);
		return (true);
	    }
	    *res1 = '\0';
	    if (may_free != NULL)
		free(may_free);
	    if (strlcat(res1, p, PATH_MAX - (res1 - *res)) >= PATH_MAX) {
		free(res);
		return (false);
	    }
	    return (true);
	 }
    }
}

static char *
origin_subst(const char *real, const char *origin_path)
{
    char *res1, *res2, *res3, *res4;

    if (uts.sysname[0] == '\0') {
	if (uname(&uts) != 0) {
	    _rtld_error("utsname failed: %d", errno);
	    return (NULL);
	}
    }
    if (!origin_subst_one(&res1, real, "$ORIGIN", origin_path, NULL) ||
	!origin_subst_one(&res2, res1, "$OSNAME", uts.sysname, res1) ||
	!origin_subst_one(&res3, res2, "$OSREL", uts.release, res2) ||
	!origin_subst_one(&res4, res3, "$PLATFORM", uts.machine, res3))
	    return (NULL);
    return (res4);
}

static void
die(void)
{
    const char *msg = dlerror();

    if (msg == NULL)
	msg = "Fatal error";
    errx(1, "%s", msg);
}

/*
 * Process a shared object's DYNAMIC section, and save the important
 * information in its Obj_Entry structure.
 */
static void
digest_dynamic1(Obj_Entry *obj, int early, const Elf_Dyn **dyn_rpath,
    const Elf_Dyn **dyn_soname)
{
    const Elf_Dyn *dynp;
    Needed_Entry **needed_tail = &obj->needed;
    Needed_Entry **needed_filtees_tail = &obj->needed_filtees;
    Needed_Entry **needed_aux_filtees_tail = &obj->needed_aux_filtees;
    int plttype = DT_REL;

    *dyn_rpath = NULL;
    *dyn_soname = NULL;

    obj->bind_now = false;
    for (dynp = obj->dynamic;  dynp->d_tag != DT_NULL;  dynp++) {
	switch (dynp->d_tag) {

	case DT_REL:
	    obj->rel = (const Elf_Rel *) (obj->relocbase + dynp->d_un.d_ptr);
	    break;

	case DT_RELSZ:
	    obj->relsize = dynp->d_un.d_val;
	    break;

	case DT_RELENT:
	    assert(dynp->d_un.d_val == sizeof(Elf_Rel));
	    break;

	case DT_JMPREL:
	    obj->pltrel = (const Elf_Rel *)
	      (obj->relocbase + dynp->d_un.d_ptr);
	    break;

	case DT_PLTRELSZ:
	    obj->pltrelsize = dynp->d_un.d_val;
	    break;

	case DT_RELA:
	    obj->rela = (const Elf_Rela *) (obj->relocbase + dynp->d_un.d_ptr);
	    break;

	case DT_RELASZ:
	    obj->relasize = dynp->d_un.d_val;
	    break;

	case DT_RELAENT:
	    assert(dynp->d_un.d_val == sizeof(Elf_Rela));
	    break;

	case DT_PLTREL:
	    plttype = dynp->d_un.d_val;
	    assert(dynp->d_un.d_val == DT_REL || plttype == DT_RELA);
	    break;

	case DT_SYMTAB:
	    obj->symtab = (const Elf_Sym *)
	      (obj->relocbase + dynp->d_un.d_ptr);
	    break;

	case DT_SYMENT:
	    assert(dynp->d_un.d_val == sizeof(Elf_Sym));
	    break;

	case DT_STRTAB:
	    obj->strtab = (const char *) (obj->relocbase + dynp->d_un.d_ptr);
	    break;

	case DT_STRSZ:
	    obj->strsize = dynp->d_un.d_val;
	    break;

	case DT_VERNEED:
	    obj->verneed = (const Elf_Verneed *) (obj->relocbase +
		dynp->d_un.d_val);
	    break;

	case DT_VERNEEDNUM:
	    obj->verneednum = dynp->d_un.d_val;
	    break;

	case DT_VERDEF:
	    obj->verdef = (const Elf_Verdef *) (obj->relocbase +
		dynp->d_un.d_val);
	    break;

	case DT_VERDEFNUM:
	    obj->verdefnum = dynp->d_un.d_val;
	    break;

	case DT_VERSYM:
	    obj->versyms = (const Elf_Versym *)(obj->relocbase +
		dynp->d_un.d_val);
	    break;

	case DT_HASH:
	    {
		const Elf_Hashelt *hashtab = (const Elf_Hashelt *)
		  (obj->relocbase + dynp->d_un.d_ptr);
		obj->nbuckets = hashtab[0];
		obj->nchains = hashtab[1];
		obj->buckets = hashtab + 2;
		obj->chains = obj->buckets + obj->nbuckets;
	    }
	    break;

	case DT_NEEDED:
	    if (!obj->rtld) {
		Needed_Entry *nep = NEW(Needed_Entry);
		nep->name = dynp->d_un.d_val;
		nep->obj = NULL;
		nep->next = NULL;

		*needed_tail = nep;
		needed_tail = &nep->next;
	    }
	    break;

	case DT_FILTER:
	    if (!obj->rtld) {
		Needed_Entry *nep = NEW(Needed_Entry);
		nep->name = dynp->d_un.d_val;
		nep->obj = NULL;
		nep->next = NULL;

		*needed_filtees_tail = nep;
		needed_filtees_tail = &nep->next;
	    }
	    break;

	case DT_AUXILIARY:
	    if (!obj->rtld) {
		Needed_Entry *nep = NEW(Needed_Entry);
		nep->name = dynp->d_un.d_val;
		nep->obj = NULL;
		nep->next = NULL;

		*needed_aux_filtees_tail = nep;
		needed_aux_filtees_tail = &nep->next;
	    }
	    break;

	case DT_PLTGOT:
	    obj->pltgot = (Elf_Addr *) (obj->relocbase + dynp->d_un.d_ptr);
	    break;

	case DT_TEXTREL:
	    obj->textrel = true;
	    break;

	case DT_SYMBOLIC:
	    obj->symbolic = true;
	    break;

	case DT_RPATH:
	case DT_RUNPATH:	/* XXX: process separately */
	    /*
	     * We have to wait until later to process this, because we
	     * might not have gotten the address of the string table yet.
	     */
	    *dyn_rpath = dynp;
	    break;

	case DT_SONAME:
	    *dyn_soname = dynp;
	    break;

	case DT_INIT:
	    obj->init = (Elf_Addr) (obj->relocbase + dynp->d_un.d_ptr);
	    break;

	case DT_FINI:
	    obj->fini = (Elf_Addr) (obj->relocbase + dynp->d_un.d_ptr);
	    break;

	/*
	 * Don't process DT_DEBUG on MIPS as the dynamic section
	 * is mapped read-only. DT_MIPS_RLD_MAP is used instead.
	 */

#ifndef __mips__
	case DT_DEBUG:
	    /* XXX - not implemented yet */
	    if (!early)
		dbg("Filling in DT_DEBUG entry");
	    ((Elf_Dyn*)dynp)->d_un.d_ptr = (Elf_Addr) &r_debug;
	    break;
#endif

	case DT_FLAGS:
		if ((dynp->d_un.d_val & DF_ORIGIN) && trust)
		    obj->z_origin = true;
		if (dynp->d_un.d_val & DF_SYMBOLIC)
		    obj->symbolic = true;
		if (dynp->d_un.d_val & DF_TEXTREL)
		    obj->textrel = true;
		if (dynp->d_un.d_val & DF_BIND_NOW)
		    obj->bind_now = true;
		if (dynp->d_un.d_val & DF_STATIC_TLS)
		    ;
	    break;
#ifdef __mips__
	case DT_MIPS_LOCAL_GOTNO:
		obj->local_gotno = dynp->d_un.d_val;
	    break;

	case DT_MIPS_SYMTABNO:
		obj->symtabno = dynp->d_un.d_val;
		break;

	case DT_MIPS_GOTSYM:
		obj->gotsym = dynp->d_un.d_val;
		break;

	case DT_MIPS_RLD_MAP:
#ifdef notyet
		if (!early)
			dbg("Filling in DT_DEBUG entry");
		((Elf_Dyn*)dynp)->d_un.d_ptr = (Elf_Addr) &r_debug;
#endif
		break;
#endif

	case DT_FLAGS_1:
		if (dynp->d_un.d_val & DF_1_NOOPEN)
		    obj->z_noopen = true;
		if ((dynp->d_un.d_val & DF_1_ORIGIN) && trust)
		    obj->z_origin = true;
		if (dynp->d_un.d_val & DF_1_GLOBAL)
			/* XXX */;
		if (dynp->d_un.d_val & DF_1_BIND_NOW)
		    obj->bind_now = true;
		if (dynp->d_un.d_val & DF_1_NODELETE)
		    obj->z_nodelete = true;
		if (dynp->d_un.d_val & DF_1_LOADFLTR)
		    obj->z_loadfltr = true;
	    break;

	default:
	    if (!early) {
		dbg("Ignoring d_tag %ld = %#lx", (long)dynp->d_tag,
		    (long)dynp->d_tag);
	    }
	    break;
	}
    }

    obj->traced = false;

    if (plttype == DT_RELA) {
	obj->pltrela = (const Elf_Rela *) obj->pltrel;
	obj->pltrel = NULL;
	obj->pltrelasize = obj->pltrelsize;
	obj->pltrelsize = 0;
    }
}

static void
digest_dynamic2(Obj_Entry *obj, const Elf_Dyn *dyn_rpath,
    const Elf_Dyn *dyn_soname)
{

    if (obj->z_origin && obj->origin_path == NULL) {
	obj->origin_path = xmalloc(PATH_MAX);
	if (rtld_dirname_abs(obj->path, obj->origin_path) == -1)
	    die();
    }

    if (dyn_rpath != NULL) {
	obj->rpath = (char *)obj->strtab + dyn_rpath->d_un.d_val;
	if (obj->z_origin)
	    obj->rpath = origin_subst(obj->rpath, obj->origin_path);
    }

    if (dyn_soname != NULL)
	object_add_name(obj, obj->strtab + dyn_soname->d_un.d_val);
}

static void
digest_dynamic(Obj_Entry *obj, int early)
{
	const Elf_Dyn *dyn_rpath;
	const Elf_Dyn *dyn_soname;

	digest_dynamic1(obj, early, &dyn_rpath, &dyn_soname);
	digest_dynamic2(obj, dyn_rpath, dyn_soname);
}

/*
 * Process a shared object's program header.  This is used only for the
 * main program, when the kernel has already loaded the main program
 * into memory before calling the dynamic linker.  It creates and
 * returns an Obj_Entry structure.
 */
static Obj_Entry *
digest_phdr(const Elf_Phdr *phdr, int phnum, caddr_t entry, const char *path)
{
    Obj_Entry *obj;
    const Elf_Phdr *phlimit = phdr + phnum;
    const Elf_Phdr *ph;
    int nsegs = 0;

    obj = obj_new();
    for (ph = phdr;  ph < phlimit;  ph++) {
	if (ph->p_type != PT_PHDR)
	    continue;

	obj->phdr = phdr;
	obj->phsize = ph->p_memsz;
	obj->relocbase = (caddr_t)phdr - ph->p_vaddr;
	break;
    }

    obj->stack_flags = PF_X | PF_R | PF_W;

    for (ph = phdr;  ph < phlimit;  ph++) {
	switch (ph->p_type) {

	case PT_INTERP:
	    obj->interp = (const char *)(ph->p_vaddr + obj->relocbase);
	    break;

	case PT_LOAD:
	    if (nsegs == 0) {	/* First load segment */
		obj->vaddrbase = trunc_page(ph->p_vaddr);
		obj->mapbase = obj->vaddrbase + obj->relocbase;
		obj->textsize = round_page(ph->p_vaddr + ph->p_memsz) -
		  obj->vaddrbase;
	    } else {		/* Last load segment */
		obj->mapsize = round_page(ph->p_vaddr + ph->p_memsz) -
		  obj->vaddrbase;
	    }
	    nsegs++;
	    break;

	case PT_DYNAMIC:
	    obj->dynamic = (const Elf_Dyn *)(ph->p_vaddr + obj->relocbase);
	    break;

	case PT_TLS:
	    obj->tlsindex = 1;
	    obj->tlssize = ph->p_memsz;
	    obj->tlsalign = ph->p_align;
	    obj->tlsinitsize = ph->p_filesz;
	    obj->tlsinit = (void*)(ph->p_vaddr + obj->relocbase);
	    break;

	case PT_GNU_STACK:
	    obj->stack_flags = ph->p_flags;
	    break;
	}
    }
    if (nsegs < 1) {
	_rtld_error("%s: too few PT_LOAD segments", path);
	return NULL;
    }

    obj->entry = entry;
    return obj;
}

static Obj_Entry *
dlcheck(void *handle)
{
    Obj_Entry *obj;

    for (obj = obj_list;  obj != NULL;  obj = obj->next)
	if (obj == (Obj_Entry *) handle)
	    break;

    if (obj == NULL || obj->refcount == 0 || obj->dl_refcount == 0) {
	_rtld_error("Invalid shared object handle %p", handle);
	return NULL;
    }
    return obj;
}

/*
 * If the given object is already in the donelist, return true.  Otherwise
 * add the object to the list and return false.
 */
static bool
donelist_check(DoneList *dlp, const Obj_Entry *obj)
{
    unsigned int i;

    for (i = 0;  i < dlp->num_used;  i++)
	if (dlp->objs[i] == obj)
	    return true;
    /*
     * Our donelist allocation should always be sufficient.  But if
     * our threads locking isn't working properly, more shared objects
     * could have been loaded since we allocated the list.  That should
     * never happen, but we'll handle it properly just in case it does.
     */
    if (dlp->num_used < dlp->num_alloc)
	dlp->objs[dlp->num_used++] = obj;
    return false;
}

/*
 * Hash function for symbol table lookup.  Don't even think about changing
 * this.  It is specified by the System V ABI.
 */
unsigned long
elf_hash(const char *name)
{
    const unsigned char *p = (const unsigned char *) name;
    unsigned long h = 0;
    unsigned long g;

    while (*p != '\0') {
	h = (h << 4) + *p++;
	if ((g = h & 0xf0000000) != 0)
	    h ^= g >> 24;
	h &= ~g;
    }
    return h;
}

/*
 * Find the library with the given name, and return its full pathname.
 * The returned string is dynamically allocated.  Generates an error
 * message and returns NULL if the library cannot be found.
 *
 * If the second argument is non-NULL, then it refers to an already-
 * loaded shared object, whose library search path will be searched.
 *
 * The search order is:
 *   LD_LIBRARY_PATH
 *   rpath in the referencing file
 *   ldconfig hints
 *   /lib:/usr/lib
 */
static char *
find_library(const char *xname, const Obj_Entry *refobj)
{
    char *pathname;
    char *name;

    if (strchr(xname, '/') != NULL) {	/* Hard coded pathname */
	if (xname[0] != '/' && !trust) {
	    _rtld_error("Absolute pathname required for shared object \"%s\"",
	      xname);
	    return NULL;
	}
	if (refobj != NULL && refobj->z_origin)
	    return origin_subst(xname, refobj->origin_path);
	else
	    return xstrdup(xname);
    }

    if (libmap_disable || (refobj == NULL) ||
	(name = lm_find(refobj->path, xname)) == NULL)
	name = (char *)xname;

    dbg(" Searching for \"%s\"", name);

    if ((pathname = search_library_path(name, ld_library_path)) != NULL ||
      (refobj != NULL &&
      (pathname = search_library_path(name, refobj->rpath)) != NULL) ||
      (pathname = search_library_path(name, gethints())) != NULL ||
      (pathname = search_library_path(name, STANDARD_LIBRARY_PATH)) != NULL)
	return pathname;

    if(refobj != NULL && refobj->path != NULL) {
	_rtld_error("Shared object \"%s\" not found, required by \"%s\"",
	  name, basename(refobj->path));
    } else {
	_rtld_error("Shared object \"%s\" not found", name);
    }
    return NULL;
}

/*
 * Given a symbol number in a referencing object, find the corresponding
 * definition of the symbol.  Returns a pointer to the symbol, or NULL if
 * no definition was found.  Returns a pointer to the Obj_Entry of the
 * defining object via the reference parameter DEFOBJ_OUT.
 */
const Elf_Sym *
find_symdef(unsigned long symnum, const Obj_Entry *refobj,
    const Obj_Entry **defobj_out, int flags, SymCache *cache,
    RtldLockState *lockstate)
{
    const Elf_Sym *ref;
    const Elf_Sym *def;
    const Obj_Entry *defobj;
    SymLook req;
    const char *name;
    int res;

    /*
     * If we have already found this symbol, get the information from
     * the cache.
     */
    if (symnum >= refobj->nchains)
	return NULL;	/* Bad object */
    if (cache != NULL && cache[symnum].sym != NULL) {
	*defobj_out = cache[symnum].obj;
	return cache[symnum].sym;
    }

    ref = refobj->symtab + symnum;
    name = refobj->strtab + ref->st_name;
    def = NULL;
    defobj = NULL;

    /*
     * We don't have to do a full scale lookup if the symbol is local.
     * We know it will bind to the instance in this load module; to
     * which we already have a pointer (ie ref). By not doing a lookup,
     * we not only improve performance, but it also avoids unresolvable
     * symbols when local symbols are not in the hash table. This has
     * been seen with the ia64 toolchain.
     */
    if (ELF_ST_BIND(ref->st_info) != STB_LOCAL) {
	if (ELF_ST_TYPE(ref->st_info) == STT_SECTION) {
	    _rtld_error("%s: Bogus symbol table entry %lu", refobj->path,
		symnum);
	}
	symlook_init(&req, name);
	req.flags = flags;
	req.ventry = fetch_ventry(refobj, symnum);
	req.lockstate = lockstate;
	res = symlook_default(&req, refobj);
	if (res == 0) {
	    def = req.sym_out;
	    defobj = req.defobj_out;
	}
    } else {
	def = ref;
	defobj = refobj;
    }

    /*
     * If we found no definition and the reference is weak, treat the
     * symbol as having the value zero.
     */
    if (def == NULL && ELF_ST_BIND(ref->st_info) == STB_WEAK) {
	def = &sym_zero;
	defobj = obj_main;
    }

    if (def != NULL) {
	*defobj_out = defobj;
	/* Record the information in the cache to avoid subsequent lookups. */
	if (cache != NULL) {
	    cache[symnum].sym = def;
	    cache[symnum].obj = defobj;
	}
    } else {
	if (refobj != &obj_rtld)
	    _rtld_error("%s: Undefined symbol \"%s\"", refobj->path, name);
    }
    return def;
}

/*
 * Return the search path from the ldconfig hints file, reading it if
 * necessary.  Returns NULL if there are problems with the hints file,
 * or if the search path there is empty.
 */
static const char *
gethints(void)
{
    static char *hints;

    if (hints == NULL) {
	int fd;
	struct elfhints_hdr hdr;
	char *p;

	/* Keep from trying again in case the hints file is bad. */
	hints = "";

	if ((fd = open(ld_elf_hints_path, O_RDONLY)) == -1)
	    return NULL;
	if (read(fd, &hdr, sizeof hdr) != sizeof hdr ||
	  hdr.magic != ELFHINTS_MAGIC ||
	  hdr.version != 1) {
	    close(fd);
	    return NULL;
	}
	p = xmalloc(hdr.dirlistlen + 1);
	if (lseek(fd, hdr.strtab + hdr.dirlist, SEEK_SET) == -1 ||
	  read(fd, p, hdr.dirlistlen + 1) != (ssize_t)hdr.dirlistlen + 1) {
	    free(p);
	    close(fd);
	    return NULL;
	}
	hints = p;
	close(fd);
    }
    return hints[0] != '\0' ? hints : NULL;
}

static void
init_dag(Obj_Entry *root)
{
    const Needed_Entry *needed;
    const Objlist_Entry *elm;
    DoneList donelist;

    if (root->dag_inited)
	return;
    donelist_init(&donelist);

    /* Root object belongs to own DAG. */
    objlist_push_tail(&root->dldags, root);
    objlist_push_tail(&root->dagmembers, root);
    donelist_check(&donelist, root);

    /*
     * Add dependencies of root object to DAG in breadth order
     * by exploiting the fact that each new object get added
     * to the tail of the dagmembers list.
     */
    STAILQ_FOREACH(elm, &root->dagmembers, link) {
	for (needed = elm->obj->needed; needed != NULL; needed = needed->next) {
	    if (needed->obj == NULL || donelist_check(&donelist, needed->obj))
		continue;
	    objlist_push_tail(&needed->obj->dldags, root);
	    objlist_push_tail(&root->dagmembers, needed->obj);
	}
    }
    root->dag_inited = true;
}

/*
 * Initialize the dynamic linker.  The argument is the address at which
 * the dynamic linker has been mapped into memory.  The primary task of
 * this function is to relocate the dynamic linker.
 */
static void
init_rtld(caddr_t mapbase, Elf_Auxinfo **aux_info)
{
    Obj_Entry objtmp;	/* Temporary rtld object */
    const Elf_Dyn *dyn_rpath;
    const Elf_Dyn *dyn_soname;

    /*
     * Conjure up an Obj_Entry structure for the dynamic linker.
     *
     * The "path" member can't be initialized yet because string constants
     * cannot yet be accessed. Below we will set it correctly.
     */
    memset(&objtmp, 0, sizeof(objtmp));
    objtmp.path = NULL;
    objtmp.rtld = true;
    objtmp.mapbase = mapbase;
#ifdef PIC
    objtmp.relocbase = mapbase;
#endif
    if (RTLD_IS_DYNAMIC()) {
	objtmp.dynamic = rtld_dynamic(&objtmp);
	digest_dynamic1(&objtmp, 1, &dyn_rpath, &dyn_soname);
	assert(objtmp.needed == NULL);
#if !defined(__mips__)
	/* MIPS has a bogus DT_TEXTREL. */
	assert(!objtmp.textrel);
#endif

	/*
	 * Temporarily put the dynamic linker entry into the object list, so
	 * that symbols can be found.
	 */

	relocate_objects(&objtmp, true, &objtmp, NULL);
    }

    /* Initialize the object list. */
    obj_tail = &obj_list;

    /* Now that non-local variables can be accesses, copy out obj_rtld. */
    memcpy(&obj_rtld, &objtmp, sizeof(obj_rtld));

    if (aux_info[AT_PAGESZ] != NULL)
	    pagesize = aux_info[AT_PAGESZ]->a_un.a_val;
    if (aux_info[AT_OSRELDATE] != NULL)
	    osreldate = aux_info[AT_OSRELDATE]->a_un.a_val;

    digest_dynamic2(&obj_rtld, dyn_rpath, dyn_soname);

    /* Replace the path with a dynamically allocated copy. */
    obj_rtld.path = xstrdup(PATH_RTLD);

    r_debug.r_brk = r_debug_state;
    r_debug.r_state = RT_CONSISTENT;
}

/*
 * Add the init functions from a needed object list (and its recursive
 * needed objects) to "list".  This is not used directly; it is a helper
 * function for initlist_add_objects().  The write lock must be held
 * when this function is called.
 */
static void
initlist_add_neededs(Needed_Entry *needed, Objlist *list)
{
    /* Recursively process the successor needed objects. */
    if (needed->next != NULL)
	initlist_add_neededs(needed->next, list);

    /* Process the current needed object. */
    if (needed->obj != NULL)
	initlist_add_objects(needed->obj, &needed->obj->next, list);
}

/*
 * Scan all of the DAGs rooted in the range of objects from "obj" to
 * "tail" and add their init functions to "list".  This recurses over
 * the DAGs and ensure the proper init ordering such that each object's
 * needed libraries are initialized before the object itself.  At the
 * same time, this function adds the objects to the global finalization
 * list "list_fini" in the opposite order.  The write lock must be
 * held when this function is called.
 */
static void
initlist_add_objects(Obj_Entry *obj, Obj_Entry **tail, Objlist *list)
{
    if (obj->init_scanned || obj->init_done)
	return;
    obj->init_scanned = true;

    /* Recursively process the successor objects. */
    if (&obj->next != tail)
	initlist_add_objects(obj->next, tail, list);

    /* Recursively process the needed objects. */
    if (obj->needed != NULL)
	initlist_add_neededs(obj->needed, list);

    /* Add the object to the init list. */
    if (obj->init != (Elf_Addr)NULL)
	objlist_push_tail(list, obj);

    /* Add the object to the global fini list in the reverse order. */
    if (obj->fini != (Elf_Addr)NULL && !obj->on_fini_list) {
	objlist_push_head(&list_fini, obj);
	obj->on_fini_list = true;
    }
}

#ifndef FPTR_TARGET
#define FPTR_TARGET(f)	((Elf_Addr) (f))
#endif

static void
free_needed_filtees(Needed_Entry *n)
{
    Needed_Entry *needed, *needed1;

    for (needed = n; needed != NULL; needed = needed->next) {
	if (needed->obj != NULL) {
	    dlclose(needed->obj);
	    needed->obj = NULL;
	}
    }
    for (needed = n; needed != NULL; needed = needed1) {
	needed1 = needed->next;
	free(needed);
    }
}

static void
unload_filtees(Obj_Entry *obj)
{

    free_needed_filtees(obj->needed_filtees);
    obj->needed_filtees = NULL;
    free_needed_filtees(obj->needed_aux_filtees);
    obj->needed_aux_filtees = NULL;
    obj->filtees_loaded = false;
}

static void
load_filtee1(Obj_Entry *obj, Needed_Entry *needed, int flags)
{

    for (; needed != NULL; needed = needed->next) {
	needed->obj = dlopen_object(obj->strtab + needed->name, obj,
	  flags, ((ld_loadfltr || obj->z_loadfltr) ? RTLD_NOW : RTLD_LAZY) |
	  RTLD_LOCAL);
    }
}

static void
load_filtees(Obj_Entry *obj, int flags, RtldLockState *lockstate)
{

    lock_restart_for_upgrade(lockstate);
    if (!obj->filtees_loaded) {
	load_filtee1(obj, obj->needed_filtees, flags);
	load_filtee1(obj, obj->needed_aux_filtees, flags);
	obj->filtees_loaded = true;
    }
}

static int
process_needed(Obj_Entry *obj, Needed_Entry *needed, int flags)
{
    Obj_Entry *obj1;

    for (; needed != NULL; needed = needed->next) {
	obj1 = needed->obj = load_object(obj->strtab + needed->name, obj,
	  flags & ~RTLD_LO_NOLOAD);
	if (obj1 == NULL && !ld_tracing && (flags & RTLD_LO_FILTEES) == 0)
	    return (-1);
	if (obj1 != NULL && obj1->z_nodelete && !obj1->ref_nodel) {
	    dbg("obj %s nodelete", obj1->path);
	    init_dag(obj1);
	    ref_dag(obj1);
	    obj1->ref_nodel = true;
	}
    }
    return (0);
}

/*
 * Given a shared object, traverse its list of needed objects, and load
 * each of them.  Returns 0 on success.  Generates an error message and
 * returns -1 on failure.
 */
static int
load_needed_objects(Obj_Entry *first, int flags)
{
    Obj_Entry *obj;

    for (obj = first;  obj != NULL;  obj = obj->next) {
	if (process_needed(obj, obj->needed, flags) == -1)
	    return (-1);
    }
    return (0);
}

static int
load_preload_objects(void)
{
    char *p = ld_preload;
    static const char delim[] = " \t:;";

    if (p == NULL)
	return 0;

    p += strspn(p, delim);
    while (*p != '\0') {
	size_t len = strcspn(p, delim);
	char savech;

	savech = p[len];
	p[len] = '\0';
	if (load_object(p, NULL, 0) == NULL)
	    return -1;	/* XXX - cleanup */
	p[len] = savech;
	p += len;
	p += strspn(p, delim);
    }
    LD_UTRACE(UTRACE_PRELOAD_FINISHED, NULL, NULL, 0, 0, NULL);
    return 0;
}

/*
 * Load a shared object into memory, if it is not already loaded.
 *
 * Returns a pointer to the Obj_Entry for the object.  Returns NULL
 * on failure.
 */
static Obj_Entry *
load_object(const char *name, const Obj_Entry *refobj, int flags)
{
    Obj_Entry *obj;
    int fd = -1;
    struct stat sb;
    char *path;

    for (obj = obj_list->next;  obj != NULL;  obj = obj->next)
	if (object_match_name(obj, name))
	    return obj;

    path = find_library(name, refobj);
    if (path == NULL)
	return NULL;

    /*
     * If we didn't find a match by pathname, open the file and check
     * again by device and inode.  This avoids false mismatches caused
     * by multiple links or ".." in pathnames.
     *
     * To avoid a race, we open the file and use fstat() rather than
     * using stat().
     */
    if ((fd = open(path, O_RDONLY)) == -1) {
	_rtld_error("Cannot open \"%s\"", path);
	free(path);
	return NULL;
    }
    if (fstat(fd, &sb) == -1) {
	_rtld_error("Cannot fstat \"%s\"", path);
	close(fd);
	free(path);
	return NULL;
    }
    for (obj = obj_list->next;  obj != NULL;  obj = obj->next) {
	if (obj->ino == sb.st_ino && obj->dev == sb.st_dev) {
	    close(fd);
	    break;
	}
    }
    if (obj != NULL) {
	object_add_name(obj, name);
	free(path);
	close(fd);
	return obj;
    }
    if (flags & RTLD_LO_NOLOAD) {
	free(path);
	return (NULL);
    }

    /* First use of this object, so we must map it in */
    obj = do_load_object(fd, name, path, &sb, flags);
    if (obj == NULL)
	free(path);
    close(fd);

    return obj;
}

static Obj_Entry *
do_load_object(int fd, const char *name, char *path, struct stat *sbp,
  int flags)
{
    Obj_Entry *obj;
    struct statfs fs;

    /*
     * but first, make sure that environment variables haven't been
     * used to circumvent the noexec flag on a filesystem.
     */
    if (dangerous_ld_env) {
	if (fstatfs(fd, &fs) != 0) {
	    _rtld_error("Cannot fstatfs \"%s\"", path);
		return NULL;
	}
	if (fs.f_flags & MNT_NOEXEC) {
	    _rtld_error("Cannot execute objects on %s\n", fs.f_mntonname);
	    return NULL;
	}
    }
    dbg("loading \"%s\"", path);
    obj = map_object(fd, path, sbp);
    if (obj == NULL)
        return NULL;

    object_add_name(obj, name);
    obj->path = path;
    digest_dynamic(obj, 0);
    if (obj->z_noopen && (flags & (RTLD_LO_DLOPEN | RTLD_LO_TRACE)) ==
      RTLD_LO_DLOPEN) {
	dbg("refusing to load non-loadable \"%s\"", obj->path);
	_rtld_error("Cannot dlopen non-loadable %s", obj->path);
	munmap(obj->mapbase, obj->mapsize);
	obj_free(obj);
	return (NULL);
    }

    *obj_tail = obj;
    obj_tail = &obj->next;
    obj_count++;
    obj_loads++;
    linkmap_add(obj);	/* for GDB & dlinfo() */
    max_stack_flags |= obj->stack_flags;

    dbg("  %p .. %p: %s", obj->mapbase,
         obj->mapbase + obj->mapsize - 1, obj->path);
    if (obj->textrel)
	dbg("  WARNING: %s has impure text", obj->path);
    LD_UTRACE(UTRACE_LOAD_OBJECT, obj, obj->mapbase, obj->mapsize, 0,
	obj->path);    

    return obj;
}

static Obj_Entry *
obj_from_addr(const void *addr)
{
    Obj_Entry *obj;

    for (obj = obj_list;  obj != NULL;  obj = obj->next) {
	if (addr < (void *) obj->mapbase)
	    continue;
	if (addr < (void *) (obj->mapbase + obj->mapsize))
	    return obj;
    }
    return NULL;
}

/*
 * Call the finalization functions for each of the objects in "list"
 * belonging to the DAG of "root" and referenced once. If NULL "root"
 * is specified, every finalization function will be called regardless
 * of the reference count and the list elements won't be freed. All of
 * the objects are expected to have non-NULL fini functions.
 */
static void
objlist_call_fini(Objlist *list, Obj_Entry *root, RtldLockState *lockstate)
{
    Objlist_Entry *elm;
    char *saved_msg;

    assert(root == NULL || root->refcount == 1);

    /*
     * Preserve the current error message since a fini function might
     * call into the dynamic linker and overwrite it.
     */
    saved_msg = errmsg_save();
    do {
	STAILQ_FOREACH(elm, list, link) {
	    if (root != NULL && (elm->obj->refcount != 1 ||
	      objlist_find(&root->dagmembers, elm->obj) == NULL))
		continue;
	    dbg("calling fini function for %s at %p", elm->obj->path,
	        (void *)elm->obj->fini);
	    LD_UTRACE(UTRACE_FINI_CALL, elm->obj, (void *)elm->obj->fini, 0, 0,
		elm->obj->path);
	    /* Remove object from fini list to prevent recursive invocation. */
	    STAILQ_REMOVE(list, elm, Struct_Objlist_Entry, link);
	    /*
	     * XXX: If a dlopen() call references an object while the
	     * fini function is in progress, we might end up trying to
	     * unload the referenced object in dlclose() or the object
	     * won't be unloaded although its fini function has been
	     * called.
	     */
	    lock_release(rtld_bind_lock, lockstate);
	    call_initfini_pointer(elm->obj, elm->obj->fini);
	    wlock_acquire(rtld_bind_lock, lockstate);
	    /* No need to free anything if process is going down. */
	    if (root != NULL)
	    	free(elm);
	    /*
	     * We must restart the list traversal after every fini call
	     * because a dlclose() call from the fini function or from
	     * another thread might have modified the reference counts.
	     */
	    break;
	}
    } while (elm != NULL);
    errmsg_restore(saved_msg);
}

/*
 * Call the initialization functions for each of the objects in
 * "list".  All of the objects are expected to have non-NULL init
 * functions.
 */
static void
objlist_call_init(Objlist *list, RtldLockState *lockstate)
{
    Objlist_Entry *elm;
    Obj_Entry *obj;
    char *saved_msg;

    /*
     * Clean init_scanned flag so that objects can be rechecked and
     * possibly initialized earlier if any of vectors called below
     * cause the change by using dlopen.
     */
    for (obj = obj_list;  obj != NULL;  obj = obj->next)
	obj->init_scanned = false;

    /*
     * Preserve the current error message since an init function might
     * call into the dynamic linker and overwrite it.
     */
    saved_msg = errmsg_save();
    STAILQ_FOREACH(elm, list, link) {
	if (elm->obj->init_done) /* Initialized early. */
	    continue;
	dbg("calling init function for %s at %p", elm->obj->path,
	    (void *)elm->obj->init);
	LD_UTRACE(UTRACE_INIT_CALL, elm->obj, (void *)elm->obj->init, 0, 0,
	    elm->obj->path);
	/*
	 * Race: other thread might try to use this object before current
	 * one completes the initilization. Not much can be done here
	 * without better locking.
	 */
	elm->obj->init_done = true;
	lock_release(rtld_bind_lock, lockstate);
	call_initfini_pointer(elm->obj, elm->obj->init);
	wlock_acquire(rtld_bind_lock, lockstate);
    }
    errmsg_restore(saved_msg);
}

static void
objlist_clear(Objlist *list)
{
    Objlist_Entry *elm;

    while (!STAILQ_EMPTY(list)) {
	elm = STAILQ_FIRST(list);
	STAILQ_REMOVE_HEAD(list, link);
	free(elm);
    }
}

static Objlist_Entry *
objlist_find(Objlist *list, const Obj_Entry *obj)
{
    Objlist_Entry *elm;

    STAILQ_FOREACH(elm, list, link)
	if (elm->obj == obj)
	    return elm;
    return NULL;
}

static void
objlist_init(Objlist *list)
{
    STAILQ_INIT(list);
}

static void
objlist_push_head(Objlist *list, Obj_Entry *obj)
{
    Objlist_Entry *elm;

    elm = NEW(Objlist_Entry);
    elm->obj = obj;
    STAILQ_INSERT_HEAD(list, elm, link);
}

static void
objlist_push_tail(Objlist *list, Obj_Entry *obj)
{
    Objlist_Entry *elm;

    elm = NEW(Objlist_Entry);
    elm->obj = obj;
    STAILQ_INSERT_TAIL(list, elm, link);
}

static void
objlist_remove(Objlist *list, Obj_Entry *obj)
{
    Objlist_Entry *elm;

    if ((elm = objlist_find(list, obj)) != NULL) {
	STAILQ_REMOVE(list, elm, Struct_Objlist_Entry, link);
	free(elm);
    }
}

/*
 * Relocate newly-loaded shared objects.  The argument is a pointer to
 * the Obj_Entry for the first such object.  All objects from the first
 * to the end of the list of objects are relocated.  Returns 0 on success,
 * or -1 on failure.
 */
static int
relocate_objects(Obj_Entry *first, bool bind_now, Obj_Entry *rtldobj,
    RtldLockState *lockstate)
{
    Obj_Entry *obj;

    for (obj = first;  obj != NULL;  obj = obj->next) {
	if (obj != rtldobj)
	    dbg("relocating \"%s\"", obj->path);
	if (obj->nbuckets == 0 || obj->nchains == 0 || obj->buckets == NULL ||
	    obj->symtab == NULL || obj->strtab == NULL) {
	    _rtld_error("%s: Shared object has no run-time symbol table",
	      obj->path);
	    return -1;
	}

	if (obj->textrel) {
	    /* There are relocations to the write-protected text segment. */
	    if (mprotect(obj->mapbase, obj->textsize,
	      PROT_READ|PROT_WRITE|PROT_EXEC) == -1) {
		_rtld_error("%s: Cannot write-enable text segment: %s",
		  obj->path, strerror(errno));
		return -1;
	    }
	}

	/* Process the non-PLT relocations. */
	if (reloc_non_plt(obj, rtldobj, lockstate))
		return -1;

	if (obj->textrel) {	/* Re-protected the text segment. */
	    if (mprotect(obj->mapbase, obj->textsize,
	      PROT_READ|PROT_EXEC) == -1) {
		_rtld_error("%s: Cannot write-protect text segment: %s",
		  obj->path, strerror(errno));
		return -1;
	    }
	}

	/* Process the PLT relocations. */
	if (reloc_plt(obj) == -1)
	    return -1;
	/* Relocate the jump slots if we are doing immediate binding. */
	if (obj->bind_now || bind_now)
	    if (reloc_jmpslots(obj, lockstate) == -1)
		return -1;


	/*
	 * Set up the magic number and version in the Obj_Entry.  These
	 * were checked in the crt1.o from the original ElfKit, so we
	 * set them for backward compatibility.
	 */
	obj->magic = RTLD_MAGIC;
	obj->version = RTLD_VERSION;

	/* Set the special PLT or GOT entries. */
	init_pltgot(obj);
    }

    return 0;
}

/*
 * Cleanup procedure.  It will be called (by the atexit mechanism) just
 * before the process exits.
 */
static void
rtld_exit(void)
{
    RtldLockState lockstate;

    wlock_acquire(rtld_bind_lock, &lockstate);
    dbg("rtld_exit()");
    objlist_call_fini(&list_fini, NULL, &lockstate);
    /* No need to remove the items from the list, since we are exiting. */
    if (!libmap_disable)
        lm_fini();
    lock_release(rtld_bind_lock, &lockstate);
}

static void *
path_enumerate(const char *path, path_enum_proc callback, void *arg)
{
#ifdef COMPAT_32BIT
    const char *trans;
#endif
    if (path == NULL)
	return (NULL);

    path += strspn(path, ":;");
    while (*path != '\0') {
	size_t len;
	char  *res;

	len = strcspn(path, ":;");
#ifdef COMPAT_32BIT
	trans = lm_findn(NULL, path, len);
	if (trans)
	    res = callback(trans, strlen(trans), arg);
	else
#endif
	res = callback(path, len, arg);

	if (res != NULL)
	    return (res);

	path += len;
	path += strspn(path, ":;");
    }

    return (NULL);
}

struct try_library_args {
    const char	*name;
    size_t	 namelen;
    char	*buffer;
    size_t	 buflen;
};

static void *
try_library_path(const char *dir, size_t dirlen, void *param)
{
    struct try_library_args *arg;

    arg = param;
    if (*dir == '/' || trust) {
	char *pathname;

	if (dirlen + 1 + arg->namelen + 1 > arg->buflen)
		return (NULL);

	pathname = arg->buffer;
	strncpy(pathname, dir, dirlen);
	pathname[dirlen] = '/';
	strcpy(pathname + dirlen + 1, arg->name);

	dbg("  Trying \"%s\"", pathname);
	if (access(pathname, F_OK) == 0) {		/* We found it */
	    pathname = xmalloc(dirlen + 1 + arg->namelen + 1);
	    strcpy(pathname, arg->buffer);
	    return (pathname);
	}
    }
    return (NULL);
}

static char *
search_library_path(const char *name, const char *path)
{
    char *p;
    struct try_library_args arg;

    if (path == NULL)
	return NULL;

    arg.name = name;
    arg.namelen = strlen(name);
    arg.buffer = xmalloc(PATH_MAX);
    arg.buflen = PATH_MAX;

    p = path_enumerate(path, try_library_path, &arg);

    free(arg.buffer);

    return (p);
}

int
dlclose(void *handle)
{
    Obj_Entry *root;
    RtldLockState lockstate;

    wlock_acquire(rtld_bind_lock, &lockstate);
    root = dlcheck(handle);
    if (root == NULL) {
	lock_release(rtld_bind_lock, &lockstate);
	return -1;
    }
    LD_UTRACE(UTRACE_DLCLOSE_START, handle, NULL, 0, root->dl_refcount,
	root->path);

    /* Unreference the object and its dependencies. */
    root->dl_refcount--;

    if (root->refcount == 1) {
	/*
	 * The object will be no longer referenced, so we must unload it.
	 * First, call the fini functions.
	 */
	objlist_call_fini(&list_fini, root, &lockstate);

	unref_dag(root);

	/* Finish cleaning up the newly-unreferenced objects. */
	GDB_STATE(RT_DELETE,&root->linkmap);
	unload_object(root);
	GDB_STATE(RT_CONSISTENT,NULL);
    } else
	unref_dag(root);

    LD_UTRACE(UTRACE_DLCLOSE_STOP, handle, NULL, 0, 0, NULL);
    lock_release(rtld_bind_lock, &lockstate);
    return 0;
}

char *
dlerror(void)
{
    char *msg = error_message;
    error_message = NULL;
    return msg;
}

/*
 * This function is deprecated and has no effect.
 */
void
dllockinit(void *context,
	   void *(*lock_create)(void *context),
           void (*rlock_acquire)(void *lock),
           void (*wlock_acquire)(void *lock),
           void (*lock_release)(void *lock),
           void (*lock_destroy)(void *lock),
	   void (*context_destroy)(void *context))
{
    static void *cur_context;
    static void (*cur_context_destroy)(void *);

    /* Just destroy the context from the previous call, if necessary. */
    if (cur_context_destroy != NULL)
	cur_context_destroy(cur_context);
    cur_context = context;
    cur_context_destroy = context_destroy;
}

void *
dlopen(const char *name, int mode)
{
    RtldLockState lockstate;
    int lo_flags;

    LD_UTRACE(UTRACE_DLOPEN_START, NULL, NULL, 0, mode, name);
    ld_tracing = (mode & RTLD_TRACE) == 0 ? NULL : "1";
    if (ld_tracing != NULL) {
	rlock_acquire(rtld_bind_lock, &lockstate);
	if (setjmp(lockstate.env) != 0)
	    lock_upgrade(rtld_bind_lock, &lockstate);
	environ = (char **)*get_program_var_addr("environ", &lockstate);
	lock_release(rtld_bind_lock, &lockstate);
    }
    lo_flags = RTLD_LO_DLOPEN;
    if (mode & RTLD_NODELETE)
	    lo_flags |= RTLD_LO_NODELETE;
    if (mode & RTLD_NOLOAD)
	    lo_flags |= RTLD_LO_NOLOAD;
    if (ld_tracing != NULL)
	    lo_flags |= RTLD_LO_TRACE;

    return (dlopen_object(name, obj_main, lo_flags,
      mode & (RTLD_MODEMASK | RTLD_GLOBAL)));
}

static Obj_Entry *
dlopen_object(const char *name, Obj_Entry *refobj, int lo_flags, int mode)
{
    Obj_Entry **old_obj_tail;
    Obj_Entry *obj;
    Objlist initlist;
    RtldLockState lockstate;
    int result;

    objlist_init(&initlist);

    wlock_acquire(rtld_bind_lock, &lockstate);
    GDB_STATE(RT_ADD,NULL);

    old_obj_tail = obj_tail;
    obj = NULL;
    if (name == NULL) {
	obj = obj_main;
	obj->refcount++;
    } else {
	obj = load_object(name, refobj, lo_flags);
    }

    if (obj) {
	obj->dl_refcount++;
	if (mode & RTLD_GLOBAL && objlist_find(&list_global, obj) == NULL)
	    objlist_push_tail(&list_global, obj);
	if (*old_obj_tail != NULL) {		/* We loaded something new. */
	    assert(*old_obj_tail == obj);
	    result = load_needed_objects(obj, lo_flags & RTLD_LO_DLOPEN);
	    init_dag(obj);
	    ref_dag(obj);
	    if (result != -1)
		result = rtld_verify_versions(&obj->dagmembers);
	    if (result != -1 && ld_tracing)
		goto trace;
	    if (result == -1 || (relocate_objects(obj, (mode & RTLD_MODEMASK)
	      == RTLD_NOW, &obj_rtld, &lockstate)) == -1) {
		obj->dl_refcount--;
		unref_dag(obj);
		if (obj->refcount == 0)
		    unload_object(obj);
		obj = NULL;
	    } else {
		/* Make list of init functions to call. */
		initlist_add_objects(obj, &obj->next, &initlist);
	    }
	} else {

	    /*
	     * Bump the reference counts for objects on this DAG.  If
	     * this is the first dlopen() call for the object that was
	     * already loaded as a dependency, initialize the dag
	     * starting at it.
	     */
	    init_dag(obj);
	    ref_dag(obj);

	    if ((lo_flags & RTLD_LO_TRACE) != 0)
		goto trace;
	}
	if (obj != NULL && ((lo_flags & RTLD_LO_NODELETE) != 0 ||
	  obj->z_nodelete) && !obj->ref_nodel) {
	    dbg("obj %s nodelete", obj->path);
	    ref_dag(obj);
	    obj->z_nodelete = obj->ref_nodel = true;
	}
    }

    LD_UTRACE(UTRACE_DLOPEN_STOP, obj, NULL, 0, obj ? obj->dl_refcount : 0,
	name);
    GDB_STATE(RT_CONSISTENT,obj ? &obj->linkmap : NULL);

    map_stacks_exec(&lockstate);

    /* Call the init functions. */
    objlist_call_init(&initlist, &lockstate);
    objlist_clear(&initlist);
    lock_release(rtld_bind_lock, &lockstate);
    return obj;
trace:
    trace_loaded_objects(obj);
    lock_release(rtld_bind_lock, &lockstate);
    exit(0);
}

static void *
do_dlsym(void *handle, const char *name, void *retaddr, const Ver_Entry *ve,
    int flags)
{
    DoneList donelist;
    const Obj_Entry *obj, *defobj;
    const Elf_Sym *def;
    SymLook req;
    RtldLockState lockstate;
    int res;

    def = NULL;
    defobj = NULL;
    symlook_init(&req, name);
    req.ventry = ve;
    req.flags = flags | SYMLOOK_IN_PLT;
    req.lockstate = &lockstate;

    rlock_acquire(rtld_bind_lock, &lockstate);
    if (setjmp(lockstate.env) != 0)
	    lock_upgrade(rtld_bind_lock, &lockstate);
    if (handle == NULL || handle == RTLD_NEXT ||
	handle == RTLD_DEFAULT || handle == RTLD_SELF) {

	if ((obj = obj_from_addr(retaddr)) == NULL) {
	    _rtld_error("Cannot determine caller's shared object");
	    lock_release(rtld_bind_lock, &lockstate);
	    return NULL;
	}
	if (handle == NULL) {	/* Just the caller's shared object. */
	    res = symlook_obj(&req, obj);
	    if (res == 0) {
		def = req.sym_out;
		defobj = req.defobj_out;
	    }
	} else if (handle == RTLD_NEXT || /* Objects after caller's */
		   handle == RTLD_SELF) { /* ... caller included */
	    if (handle == RTLD_NEXT)
		obj = obj->next;
	    for (; obj != NULL; obj = obj->next) {
		res = symlook_obj(&req, obj);
		if (res == 0) {
		    if (def == NULL ||
		      ELF_ST_BIND(req.sym_out->st_info) != STB_WEAK) {
			def = req.sym_out;
			defobj = req.defobj_out;
			if (ELF_ST_BIND(def->st_info) != STB_WEAK)
			    break;
		    }
		}
	    }
	    /*
	     * Search the dynamic linker itself, and possibly resolve the
	     * symbol from there.  This is how the application links to
	     * dynamic linker services such as dlopen.
	     */
	    if (def == NULL || ELF_ST_BIND(def->st_info) == STB_WEAK) {
		res = symlook_obj(&req, &obj_rtld);
		if (res == 0) {
		    def = req.sym_out;
		    defobj = req.defobj_out;
		}
	    }
	} else {
	    assert(handle == RTLD_DEFAULT);
	    res = symlook_default(&req, obj);
	    if (res == 0) {
		defobj = req.defobj_out;
		def = req.sym_out;
	    }
	}
    } else {
	if ((obj = dlcheck(handle)) == NULL) {
	    lock_release(rtld_bind_lock, &lockstate);
	    return NULL;
	}

	donelist_init(&donelist);
	if (obj->mainprog) {
            /* Handle obtained by dlopen(NULL, ...) implies global scope. */
	    res = symlook_global(&req, &donelist);
	    if (res == 0) {
		def = req.sym_out;
		defobj = req.defobj_out;
	    }
	    /*
	     * Search the dynamic linker itself, and possibly resolve the
	     * symbol from there.  This is how the application links to
	     * dynamic linker services such as dlopen.
	     */
	    if (def == NULL || ELF_ST_BIND(def->st_info) == STB_WEAK) {
		res = symlook_obj(&req, &obj_rtld);
		if (res == 0) {
		    def = req.sym_out;
		    defobj = req.defobj_out;
		}
	    }
	}
	else {
	    /* Search the whole DAG rooted at the given object. */
	    res = symlook_list(&req, &obj->dagmembers, &donelist);
	    if (res == 0) {
		def = req.sym_out;
		defobj = req.defobj_out;
	    }
	}
    }

    if (def != NULL) {
	lock_release(rtld_bind_lock, &lockstate);

	/*
	 * The value required by the caller is derived from the value
	 * of the symbol. For the ia64 architecture, we need to
	 * construct a function descriptor which the caller can use to
	 * call the function with the right 'gp' value. For other
	 * architectures and for non-functions, the value is simply
	 * the relocated value of the symbol.
	 */
	if (ELF_ST_TYPE(def->st_info) == STT_FUNC)
	    return make_function_pointer(def, defobj);
	else
	    return defobj->relocbase + def->st_value;
    }

    _rtld_error("Undefined symbol \"%s\"", name);
    lock_release(rtld_bind_lock, &lockstate);
    return NULL;
}

void *
dlsym(void *handle, const char *name)
{
	return do_dlsym(handle, name, __builtin_return_address(0), NULL,
	    SYMLOOK_DLSYM);
}

dlfunc_t
dlfunc(void *handle, const char *name)
{
	union {
		void *d;
		dlfunc_t f;
	} rv;

	rv.d = do_dlsym(handle, name, __builtin_return_address(0), NULL,
	    SYMLOOK_DLSYM);
	return (rv.f);
}

void *
dlvsym(void *handle, const char *name, const char *version)
{
	Ver_Entry ventry;

	ventry.name = version;
	ventry.file = NULL;
	ventry.hash = elf_hash(version);
	ventry.flags= 0;
	return do_dlsym(handle, name, __builtin_return_address(0), &ventry,
	    SYMLOOK_DLSYM);
}

int
_rtld_addr_phdr(const void *addr, struct dl_phdr_info *phdr_info)
{
    const Obj_Entry *obj;
    RtldLockState lockstate;

    rlock_acquire(rtld_bind_lock, &lockstate);
    obj = obj_from_addr(addr);
    if (obj == NULL) {
        _rtld_error("No shared object contains address");
	lock_release(rtld_bind_lock, &lockstate);
        return (0);
    }
    rtld_fill_dl_phdr_info(obj, phdr_info);
    lock_release(rtld_bind_lock, &lockstate);
    return (1);
}

int
dladdr(const void *addr, Dl_info *info)
{
    const Obj_Entry *obj;
    const Elf_Sym *def;
    void *symbol_addr;
    unsigned long symoffset;
    RtldLockState lockstate;

    rlock_acquire(rtld_bind_lock, &lockstate);
    obj = obj_from_addr(addr);
    if (obj == NULL) {
        _rtld_error("No shared object contains address");
	lock_release(rtld_bind_lock, &lockstate);
        return 0;
    }
    info->dli_fname = obj->path;
    info->dli_fbase = obj->mapbase;
    info->dli_saddr = (void *)0;
    info->dli_sname = NULL;

    /*
     * Walk the symbol list looking for the symbol whose address is
     * closest to the address sent in.
     */
    for (symoffset = 0; symoffset < obj->nchains; symoffset++) {
        def = obj->symtab + symoffset;

        /*
         * For skip the symbol if st_shndx is either SHN_UNDEF or
         * SHN_COMMON.
         */
        if (def->st_shndx == SHN_UNDEF || def->st_shndx == SHN_COMMON)
            continue;

        /*
         * If the symbol is greater than the specified address, or if it
         * is further away from addr than the current nearest symbol,
         * then reject it.
         */
        symbol_addr = obj->relocbase + def->st_value;
        if (symbol_addr > addr || symbol_addr < info->dli_saddr)
            continue;

        /* Update our idea of the nearest symbol. */
        info->dli_sname = obj->strtab + def->st_name;
        info->dli_saddr = symbol_addr;

        /* Exact match? */
        if (info->dli_saddr == addr)
            break;
    }
    lock_release(rtld_bind_lock, &lockstate);
    return 1;
}

int
dlinfo(void *handle, int request, void *p)
{
    const Obj_Entry *obj;
    RtldLockState lockstate;
    int error;

    rlock_acquire(rtld_bind_lock, &lockstate);

    if (handle == NULL || handle == RTLD_SELF) {
	void *retaddr;

	retaddr = __builtin_return_address(0);	/* __GNUC__ only */
	if ((obj = obj_from_addr(retaddr)) == NULL)
	    _rtld_error("Cannot determine caller's shared object");
    } else
	obj = dlcheck(handle);

    if (obj == NULL) {
	lock_release(rtld_bind_lock, &lockstate);
	return (-1);
    }

    error = 0;
    switch (request) {
    case RTLD_DI_LINKMAP:
	*((struct link_map const **)p) = &obj->linkmap;
	break;
    case RTLD_DI_ORIGIN:
	error = rtld_dirname(obj->path, p);
	break;

    case RTLD_DI_SERINFOSIZE:
    case RTLD_DI_SERINFO:
	error = do_search_info(obj, request, (struct dl_serinfo *)p);
	break;

    default:
	_rtld_error("Invalid request %d passed to dlinfo()", request);
	error = -1;
    }

    lock_release(rtld_bind_lock, &lockstate);

    return (error);
}

static void
rtld_fill_dl_phdr_info(const Obj_Entry *obj, struct dl_phdr_info *phdr_info)
{

	phdr_info->dlpi_addr = (Elf_Addr)obj->relocbase;
	phdr_info->dlpi_name = STAILQ_FIRST(&obj->names) ?
	    STAILQ_FIRST(&obj->names)->name : obj->path;
	phdr_info->dlpi_phdr = obj->phdr;
	phdr_info->dlpi_phnum = obj->phsize / sizeof(obj->phdr[0]);
	phdr_info->dlpi_tls_modid = obj->tlsindex;
	phdr_info->dlpi_tls_data = obj->tlsinit;
	phdr_info->dlpi_adds = obj_loads;
	phdr_info->dlpi_subs = obj_loads - obj_count;
}

int
dl_iterate_phdr(__dl_iterate_hdr_callback callback, void *param)
{
    struct dl_phdr_info phdr_info;
    const Obj_Entry *obj;
    RtldLockState bind_lockstate, phdr_lockstate;
    int error;

    wlock_acquire(rtld_phdr_lock, &phdr_lockstate);
    rlock_acquire(rtld_bind_lock, &bind_lockstate);

    error = 0;

    for (obj = obj_list;  obj != NULL;  obj = obj->next) {
	rtld_fill_dl_phdr_info(obj, &phdr_info);
	if ((error = callback(&phdr_info, sizeof phdr_info, param)) != 0)
		break;

    }
    lock_release(rtld_bind_lock, &bind_lockstate);
    lock_release(rtld_phdr_lock, &phdr_lockstate);

    return (error);
}

struct fill_search_info_args {
    int		 request;
    unsigned int flags;
    Dl_serinfo  *serinfo;
    Dl_serpath  *serpath;
    char	*strspace;
};

static void *
fill_search_info(const char *dir, size_t dirlen, void *param)
{
    struct fill_search_info_args *arg;

    arg = param;

    if (arg->request == RTLD_DI_SERINFOSIZE) {
	arg->serinfo->dls_cnt ++;
	arg->serinfo->dls_size += sizeof(Dl_serpath) + dirlen + 1;
    } else {
	struct dl_serpath *s_entry;

	s_entry = arg->serpath;
	s_entry->dls_name  = arg->strspace;
	s_entry->dls_flags = arg->flags;

	strncpy(arg->strspace, dir, dirlen);
	arg->strspace[dirlen] = '\0';

	arg->strspace += dirlen + 1;
	arg->serpath++;
    }

    return (NULL);
}

static int
do_search_info(const Obj_Entry *obj, int request, struct dl_serinfo *info)
{
    struct dl_serinfo _info;
    struct fill_search_info_args args;

    args.request = RTLD_DI_SERINFOSIZE;
    args.serinfo = &_info;

    _info.dls_size = __offsetof(struct dl_serinfo, dls_serpath);
    _info.dls_cnt  = 0;

    path_enumerate(ld_library_path, fill_search_info, &args);
    path_enumerate(obj->rpath, fill_search_info, &args);
    path_enumerate(gethints(), fill_search_info, &args);
    path_enumerate(STANDARD_LIBRARY_PATH, fill_search_info, &args);


    if (request == RTLD_DI_SERINFOSIZE) {
	info->dls_size = _info.dls_size;
	info->dls_cnt = _info.dls_cnt;
	return (0);
    }

    if (info->dls_cnt != _info.dls_cnt || info->dls_size != _info.dls_size) {
	_rtld_error("Uninitialized Dl_serinfo struct passed to dlinfo()");
	return (-1);
    }

    args.request  = RTLD_DI_SERINFO;
    args.serinfo  = info;
    args.serpath  = &info->dls_serpath[0];
    args.strspace = (char *)&info->dls_serpath[_info.dls_cnt];

    args.flags = LA_SER_LIBPATH;
    if (path_enumerate(ld_library_path, fill_search_info, &args) != NULL)
	return (-1);

    args.flags = LA_SER_RUNPATH;
    if (path_enumerate(obj->rpath, fill_search_info, &args) != NULL)
	return (-1);

    args.flags = LA_SER_CONFIG;
    if (path_enumerate(gethints(), fill_search_info, &args) != NULL)
	return (-1);

    args.flags = LA_SER_DEFAULT;
    if (path_enumerate(STANDARD_LIBRARY_PATH, fill_search_info, &args) != NULL)
	return (-1);
    return (0);
}

static int
rtld_dirname(const char *path, char *bname)
{
    const char *endp;

    /* Empty or NULL string gets treated as "." */
    if (path == NULL || *path == '\0') {
	bname[0] = '.';
	bname[1] = '\0';
	return (0);
    }

    /* Strip trailing slashes */
    endp = path + strlen(path) - 1;
    while (endp > path && *endp == '/')
	endp--;

    /* Find the start of the dir */
    while (endp > path && *endp != '/')
	endp--;

    /* Either the dir is "/" or there are no slashes */
    if (endp == path) {
	bname[0] = *endp == '/' ? '/' : '.';
	bname[1] = '\0';
	return (0);
    } else {
	do {
	    endp--;
	} while (endp > path && *endp == '/');
    }

    if (endp - path + 2 > PATH_MAX)
    {
	_rtld_error("Filename is too long: %s", path);
	return(-1);
    }

    strncpy(bname, path, endp - path + 1);
    bname[endp - path + 1] = '\0';
    return (0);
}

static int
rtld_dirname_abs(const char *path, char *base)
{
	char base_rel[PATH_MAX];

	if (rtld_dirname(path, base) == -1)
		return (-1);
	if (base[0] == '/')
		return (0);
	if (getcwd(base_rel, sizeof(base_rel)) == NULL ||
	    strlcat(base_rel, "/", sizeof(base_rel)) >= sizeof(base_rel) ||
	    strlcat(base_rel, base, sizeof(base_rel)) >= sizeof(base_rel))
		return (-1);
	strcpy(base, base_rel);
	return (0);
}

static void
linkmap_add(Obj_Entry *obj)
{
    struct link_map *l = &obj->linkmap;
    struct link_map *prev;

    obj->linkmap.l_name = obj->path;
    obj->linkmap.l_addr = obj->mapbase;
    obj->linkmap.l_ld = obj->dynamic;
#ifdef __mips__
    /* GDB needs load offset on MIPS to use the symbols */
    obj->linkmap.l_offs = obj->relocbase;
#endif

    if (r_debug.r_map == NULL) {
	r_debug.r_map = l;
	return;
    }

    /*
     * Scan to the end of the list, but not past the entry for the
     * dynamic linker, which we want to keep at the very end.
     */
    for (prev = r_debug.r_map;
      prev->l_next != NULL && prev->l_next != &obj_rtld.linkmap;
      prev = prev->l_next)
	;

    /* Link in the new entry. */
    l->l_prev = prev;
    l->l_next = prev->l_next;
    if (l->l_next != NULL)
	l->l_next->l_prev = l;
    prev->l_next = l;
}

static void
linkmap_delete(Obj_Entry *obj)
{
    struct link_map *l = &obj->linkmap;

    if (l->l_prev == NULL) {
	if ((r_debug.r_map = l->l_next) != NULL)
	    l->l_next->l_prev = NULL;
	return;
    }

    if ((l->l_prev->l_next = l->l_next) != NULL)
	l->l_next->l_prev = l->l_prev;
}

/*
 * Function for the debugger to set a breakpoint on to gain control.
 *
 * The two parameters allow the debugger to easily find and determine
 * what the runtime loader is doing and to whom it is doing it.
 *
 * When the loadhook trap is hit (r_debug_state, set at program
 * initialization), the arguments can be found on the stack:
 *
 *  +8   struct link_map *m
 *  +4   struct r_debug  *rd
 *  +0   RetAddr
 */
void
r_debug_state(struct r_debug* rd, struct link_map *m)
{
}

/*
 * Get address of the pointer variable in the main program.
 * Prefer non-weak symbol over the weak one.
 */
static const void **
get_program_var_addr(const char *name, RtldLockState *lockstate)
{
    SymLook req;
    DoneList donelist;

    symlook_init(&req, name);
    req.lockstate = lockstate;
    donelist_init(&donelist);
    if (symlook_global(&req, &donelist) != 0)
	return (NULL);
    if (ELF_ST_TYPE(req.sym_out->st_info) == STT_FUNC)
	return ((const void **)make_function_pointer(req.sym_out,
	  req.defobj_out));
    else
	return ((const void **)(req.defobj_out->relocbase +
	  req.sym_out->st_value));
}

/*
 * Set a pointer variable in the main program to the given value.  This
 * is used to set key variables such as "environ" before any of the
 * init functions are called.
 */
static void
set_program_var(const char *name, const void *value)
{
    const void **addr;

    if ((addr = get_program_var_addr(name, NULL)) != NULL) {
	dbg("\"%s\": *%p <-- %p", name, addr, value);
	*addr = value;
    }
}

/*
 * Search the global objects, including dependencies and main object,
 * for the given symbol.
 */
static int
symlook_global(SymLook *req, DoneList *donelist)
{
    SymLook req1;
    const Objlist_Entry *elm;
    int res;

    symlook_init_from_req(&req1, req);

    /* Search all objects loaded at program start up. */
    if (req->defobj_out == NULL ||
      ELF_ST_BIND(req->sym_out->st_info) == STB_WEAK) {
	res = symlook_list(&req1, &list_main, donelist);
	if (res == 0 && (req->defobj_out == NULL ||
	  ELF_ST_BIND(req1.sym_out->st_info) != STB_WEAK)) {
	    req->sym_out = req1.sym_out;
	    req->defobj_out = req1.defobj_out;
	    assert(req->defobj_out != NULL);
	}
    }

    /* Search all DAGs whose roots are RTLD_GLOBAL objects. */
    STAILQ_FOREACH(elm, &list_global, link) {
	if (req->defobj_out != NULL &&
	  ELF_ST_BIND(req->sym_out->st_info) != STB_WEAK)
	    break;
	res = symlook_list(&req1, &elm->obj->dagmembers, donelist);
	if (res == 0 && (req->defobj_out == NULL ||
	  ELF_ST_BIND(req1.sym_out->st_info) != STB_WEAK)) {
	    req->sym_out = req1.sym_out;
	    req->defobj_out = req1.defobj_out;
	    assert(req->defobj_out != NULL);
	}
    }

    return (req->sym_out != NULL ? 0 : ESRCH);
}

/*
 * Given a symbol name in a referencing object, find the corresponding
 * definition of the symbol.  Returns a pointer to the symbol, or NULL if
 * no definition was found.  Returns a pointer to the Obj_Entry of the
 * defining object via the reference parameter DEFOBJ_OUT.
 */
static int
symlook_default(SymLook *req, const Obj_Entry *refobj)
{
    DoneList donelist;
    const Objlist_Entry *elm;
    SymLook req1;
    int res;

    donelist_init(&donelist);
    symlook_init_from_req(&req1, req);

    /* Look first in the referencing object if linked symbolically. */
    if (refobj->symbolic && !donelist_check(&donelist, refobj)) {
	res = symlook_obj(&req1, refobj);
	if (res == 0) {
	    req->sym_out = req1.sym_out;
	    req->defobj_out = req1.defobj_out;
	    assert(req->defobj_out != NULL);
	}
    }

    symlook_global(req, &donelist);

    /* Search all dlopened DAGs containing the referencing object. */
    STAILQ_FOREACH(elm, &refobj->dldags, link) {
	if (req->sym_out != NULL &&
	  ELF_ST_BIND(req->sym_out->st_info) != STB_WEAK)
	    break;
	res = symlook_list(&req1, &elm->obj->dagmembers, &donelist);
	if (res == 0 && (req->sym_out == NULL ||
	  ELF_ST_BIND(req1.sym_out->st_info) != STB_WEAK)) {
	    req->sym_out = req1.sym_out;
	    req->defobj_out = req1.defobj_out;
	    assert(req->defobj_out != NULL);
	}
    }

    /*
     * Search the dynamic linker itself, and possibly resolve the
     * symbol from there.  This is how the application links to
     * dynamic linker services such as dlopen.
     */
    if (req->sym_out == NULL ||
      ELF_ST_BIND(req->sym_out->st_info) == STB_WEAK) {
	res = symlook_obj(&req1, &obj_rtld);
	if (res == 0) {
	    req->sym_out = req1.sym_out;
	    req->defobj_out = req1.defobj_out;
	    assert(req->defobj_out != NULL);
	}
    }

    return (req->sym_out != NULL ? 0 : ESRCH);
}

static int
symlook_list(SymLook *req, const Objlist *objlist, DoneList *dlp)
{
    const Elf_Sym *def;
    const Obj_Entry *defobj;
    const Objlist_Entry *elm;
    SymLook req1;
    int res;

    def = NULL;
    defobj = NULL;
    STAILQ_FOREACH(elm, objlist, link) {
	if (donelist_check(dlp, elm->obj))
	    continue;
	symlook_init_from_req(&req1, req);
	if ((res = symlook_obj(&req1, elm->obj)) == 0) {
	    if (def == NULL || ELF_ST_BIND(req1.sym_out->st_info) != STB_WEAK) {
		def = req1.sym_out;
		defobj = req1.defobj_out;
		if (ELF_ST_BIND(def->st_info) != STB_WEAK)
		    break;
	    }
	}
    }
    if (def != NULL) {
	req->sym_out = def;
	req->defobj_out = defobj;
	return (0);
    }
    return (ESRCH);
}

/*
 * Search the chain of DAGS cointed to by the given Needed_Entry
 * for a symbol of the given name.  Each DAG is scanned completely
 * before advancing to the next one.  Returns a pointer to the symbol,
 * or NULL if no definition was found.
 */
static int
symlook_needed(SymLook *req, const Needed_Entry *needed, DoneList *dlp)
{
    const Elf_Sym *def;
    const Needed_Entry *n;
    const Obj_Entry *defobj;
    SymLook req1;
    int res;

    def = NULL;
    defobj = NULL;
    symlook_init_from_req(&req1, req);
    for (n = needed; n != NULL; n = n->next) {
	if (n->obj == NULL ||
	    (res = symlook_list(&req1, &n->obj->dagmembers, dlp)) != 0)
	    continue;
	if (def == NULL || ELF_ST_BIND(req1.sym_out->st_info) != STB_WEAK) {
	    def = req1.sym_out;
	    defobj = req1.defobj_out;
	    if (ELF_ST_BIND(def->st_info) != STB_WEAK)
		break;
	}
    }
    if (def != NULL) {
	req->sym_out = def;
	req->defobj_out = defobj;
	return (0);
    }
    return (ESRCH);
}

/*
 * Search the symbol table of a single shared object for a symbol of
 * the given name and version, if requested.  Returns a pointer to the
 * symbol, or NULL if no definition was found.  If the object is
 * filter, return filtered symbol from filtee.
 *
 * The symbol's hash value is passed in for efficiency reasons; that
 * eliminates many recomputations of the hash value.
 */
int
symlook_obj(SymLook *req, const Obj_Entry *obj)
{
    DoneList donelist;
    SymLook req1;
    int res, mres;

    mres = symlook_obj1(req, obj);
    if (mres == 0) {
	if (obj->needed_filtees != NULL) {
	    load_filtees(__DECONST(Obj_Entry *, obj), 0, req->lockstate);
	    donelist_init(&donelist);
	    symlook_init_from_req(&req1, req);
	    res = symlook_needed(&req1, obj->needed_filtees, &donelist);
	    if (res == 0) {
		req->sym_out = req1.sym_out;
		req->defobj_out = req1.defobj_out;
	    }
	    return (res);
	}
	if (obj->needed_aux_filtees != NULL) {
	    load_filtees(__DECONST(Obj_Entry *, obj), 0, req->lockstate);
	    donelist_init(&donelist);
	    symlook_init_from_req(&req1, req);
	    res = symlook_needed(&req1, obj->needed_aux_filtees, &donelist);
	    if (res == 0) {
		req->sym_out = req1.sym_out;
		req->defobj_out = req1.defobj_out;
		return (res);
	    }
	}
    }
    return (mres);
}

static int
symlook_obj1(SymLook *req, const Obj_Entry *obj)
{
    unsigned long symnum;
    const Elf_Sym *vsymp;
    Elf_Versym verndx;
    int vcount;

    if (obj->buckets == NULL)
	return (ESRCH);

    vsymp = NULL;
    vcount = 0;
    symnum = obj->buckets[req->hash % obj->nbuckets];

    for (; symnum != STN_UNDEF; symnum = obj->chains[symnum]) {
	const Elf_Sym *symp;
	const char *strp;

	if (symnum >= obj->nchains)
	    return (ESRCH);	/* Bad object */

	symp = obj->symtab + symnum;
	strp = obj->strtab + symp->st_name;

	switch (ELF_ST_TYPE(symp->st_info)) {
	case STT_FUNC:
	case STT_NOTYPE:
	case STT_OBJECT:
	    if (symp->st_value == 0)
		continue;
		/* fallthrough */
	case STT_TLS:
	    if (symp->st_shndx != SHN_UNDEF)
		break;
#ifndef __mips__
	    else if (((req->flags & SYMLOOK_IN_PLT) == 0) &&
		 (ELF_ST_TYPE(symp->st_info) == STT_FUNC))
		break;
		/* fallthrough */
#endif
	default:
	    continue;
	}
	if (req->name[0] != strp[0] || strcmp(req->name, strp) != 0)
	    continue;

	if (req->ventry == NULL) {
	    if (obj->versyms != NULL) {
		verndx = VER_NDX(obj->versyms[symnum]);
		if (verndx > obj->vernum) {
		    _rtld_error("%s: symbol %s references wrong version %d",
			obj->path, obj->strtab + symnum, verndx);
		    continue;
		}
		/*
		 * If we are not called from dlsym (i.e. this is a normal
		 * relocation from unversioned binary), accept the symbol
		 * immediately if it happens to have first version after
		 * this shared object became versioned. Otherwise, if
		 * symbol is versioned and not hidden, remember it. If it
		 * is the only symbol with this name exported by the
		 * shared object, it will be returned as a match at the
		 * end of the function. If symbol is global (verndx < 2)
		 * accept it unconditionally.
		 */
		if ((req->flags & SYMLOOK_DLSYM) == 0 &&
		  verndx == VER_NDX_GIVEN) {
		    req->sym_out = symp;
		    req->defobj_out = obj;
		    return (0);
		}
		else if (verndx >= VER_NDX_GIVEN) {
		    if ((obj->versyms[symnum] & VER_NDX_HIDDEN) == 0) {
			if (vsymp == NULL)
			    vsymp = symp;
			vcount ++;
		    }
		    continue;
		}
	    }
	    req->sym_out = symp;
	    req->defobj_out = obj;
	    return (0);
	} else {
	    if (obj->versyms == NULL) {
		if (object_match_name(obj, req->ventry->name)) {
		    _rtld_error("%s: object %s should provide version %s for "
			"symbol %s", obj_rtld.path, obj->path,
			req->ventry->name, obj->strtab + symnum);
		    continue;
		}
	    } else {
		verndx = VER_NDX(obj->versyms[symnum]);
		if (verndx > obj->vernum) {
		    _rtld_error("%s: symbol %s references wrong version %d",
			obj->path, obj->strtab + symnum, verndx);
		    continue;
		}
		if (obj->vertab[verndx].hash != req->ventry->hash ||
		    strcmp(obj->vertab[verndx].name, req->ventry->name)) {
		    /*
		     * Version does not match. Look if this is a global symbol
		     * and if it is not hidden. If global symbol (verndx < 2)
		     * is available, use it. Do not return symbol if we are
		     * called by dlvsym, because dlvsym looks for a specific
		     * version and default one is not what dlvsym wants.
		     */
		    if ((req->flags & SYMLOOK_DLSYM) ||
			(obj->versyms[symnum] & VER_NDX_HIDDEN) ||
			(verndx >= VER_NDX_GIVEN))
			continue;
		}
	    }
	    req->sym_out = symp;
	    req->defobj_out = obj;
	    return (0);
	}
    }
    if (vcount == 1) {
	req->sym_out = vsymp;
	req->defobj_out = obj;
	return (0);
    }
    return (ESRCH);
}

static void
trace_loaded_objects(Obj_Entry *obj)
{
    char	*fmt1, *fmt2, *fmt, *main_local, *list_containers;
    int		c;

    if ((main_local = getenv(LD_ "TRACE_LOADED_OBJECTS_PROGNAME")) == NULL)
	main_local = "";

    if ((fmt1 = getenv(LD_ "TRACE_LOADED_OBJECTS_FMT1")) == NULL)
	fmt1 = "\t%o => %p (%x)\n";

    if ((fmt2 = getenv(LD_ "TRACE_LOADED_OBJECTS_FMT2")) == NULL)
	fmt2 = "\t%o (%x)\n";

    list_containers = getenv(LD_ "TRACE_LOADED_OBJECTS_ALL");

    for (; obj; obj = obj->next) {
	Needed_Entry		*needed;
	char			*name, *path;
	bool			is_lib;

	if (list_containers && obj->needed != NULL)
	    printf("%s:\n", obj->path);
	for (needed = obj->needed; needed; needed = needed->next) {
	    if (needed->obj != NULL) {
		if (needed->obj->traced && !list_containers)
		    continue;
		needed->obj->traced = true;
		path = needed->obj->path;
	    } else
		path = "not found";

	    name = (char *)obj->strtab + needed->name;
	    is_lib = strncmp(name, "lib", 3) == 0;	/* XXX - bogus */

	    fmt = is_lib ? fmt1 : fmt2;
	    while ((c = *fmt++) != '\0') {
		switch (c) {
		default:
		    putchar(c);
		    continue;
		case '\\':
		    switch (c = *fmt) {
		    case '\0':
			continue;
		    case 'n':
			putchar('\n');
			break;
		    case 't':
			putchar('\t');
			break;
		    }
		    break;
		case '%':
		    switch (c = *fmt) {
		    case '\0':
			continue;
		    case '%':
		    default:
			putchar(c);
			break;
		    case 'A':
			printf("%s", main_local);
			break;
		    case 'a':
			printf("%s", obj_main->path);
			break;
		    case 'o':
			printf("%s", name);
			break;
#if 0
		    case 'm':
			printf("%d", sodp->sod_major);
			break;
		    case 'n':
			printf("%d", sodp->sod_minor);
			break;
#endif
		    case 'p':
			printf("%s", path);
			break;
		    case 'x':
			printf("%p", needed->obj ? needed->obj->mapbase : 0);
			break;
		    }
		    break;
		}
		++fmt;
	    }
	}
    }
}

/*
 * Unload a dlopened object and its dependencies from memory and from
 * our data structures.  It is assumed that the DAG rooted in the
 * object has already been unreferenced, and that the object has a
 * reference count of 0.
 */
static void
unload_object(Obj_Entry *root)
{
    Obj_Entry *obj;
    Obj_Entry **linkp;

    assert(root->refcount == 0);

    /*
     * Pass over the DAG removing unreferenced objects from
     * appropriate lists.
     */
    unlink_object(root);

    /* Unmap all objects that are no longer referenced. */
    linkp = &obj_list->next;
    while ((obj = *linkp) != NULL) {
	if (obj->refcount == 0) {
	    LD_UTRACE(UTRACE_UNLOAD_OBJECT, obj, obj->mapbase, obj->mapsize, 0,
		obj->path);
	    dbg("unloading \"%s\"", obj->path);
	    unload_filtees(root);
	    munmap(obj->mapbase, obj->mapsize);
	    linkmap_delete(obj);
	    *linkp = obj->next;
	    obj_count--;
	    obj_free(obj);
	} else
	    linkp = &obj->next;
    }
    obj_tail = linkp;
}

static void
unlink_object(Obj_Entry *root)
{
    Objlist_Entry *elm;

    if (root->refcount == 0) {
	/* Remove the object from the RTLD_GLOBAL list. */
	objlist_remove(&list_global, root);

    	/* Remove the object from all objects' DAG lists. */
    	STAILQ_FOREACH(elm, &root->dagmembers, link) {
	    objlist_remove(&elm->obj->dldags, root);
	    if (elm->obj != root)
		unlink_object(elm->obj);
	}
    }
}

static void
ref_dag(Obj_Entry *root)
{
    Objlist_Entry *elm;

    assert(root->dag_inited);
    STAILQ_FOREACH(elm, &root->dagmembers, link)
	elm->obj->refcount++;
}

static void
unref_dag(Obj_Entry *root)
{
    Objlist_Entry *elm;

    assert(root->dag_inited);
    STAILQ_FOREACH(elm, &root->dagmembers, link)
	elm->obj->refcount--;
}

/*
 * Common code for MD __tls_get_addr().
 */
void *
tls_get_addr_common(Elf_Addr** dtvp, int index, size_t offset)
{
    Elf_Addr* dtv = *dtvp;
    RtldLockState lockstate;

    /* Check dtv generation in case new modules have arrived */
    if (dtv[0] != tls_dtv_generation) {
	Elf_Addr* newdtv;
	int to_copy;

	wlock_acquire(rtld_bind_lock, &lockstate);
	newdtv = calloc(1, (tls_max_index + 2) * sizeof(Elf_Addr));
	to_copy = dtv[1];
	if (to_copy > tls_max_index)
	    to_copy = tls_max_index;
	memcpy(&newdtv[2], &dtv[2], to_copy * sizeof(Elf_Addr));
	newdtv[0] = tls_dtv_generation;
	newdtv[1] = tls_max_index;
	free(dtv);
	lock_release(rtld_bind_lock, &lockstate);
	*dtvp = newdtv;
    }

    /* Dynamically allocate module TLS if necessary */
    if (!dtv[index + 1]) {
	/* Signal safe, wlock will block out signals. */
	    wlock_acquire(rtld_bind_lock, &lockstate);
	if (!dtv[index + 1])
	    dtv[index + 1] = (Elf_Addr)allocate_module_tls(index);
	lock_release(rtld_bind_lock, &lockstate);
    }
    return (void*) (dtv[index + 1] + offset);
}

/* XXX not sure what variants to use for arm. */

#if defined(__ia64__) || defined(__powerpc__)

/*
 * Allocate Static TLS using the Variant I method.
 */
void *
allocate_tls(Obj_Entry *objs, void *oldtcb, size_t tcbsize, size_t tcbalign)
{
    Obj_Entry *obj;
    char *tcb;
    Elf_Addr **tls;
    Elf_Addr *dtv;
    Elf_Addr addr;
    int i;

    if (oldtcb != NULL && tcbsize == TLS_TCB_SIZE)
	return (oldtcb);

    assert(tcbsize >= TLS_TCB_SIZE);
    tcb = calloc(1, tls_static_space - TLS_TCB_SIZE + tcbsize);
    tls = (Elf_Addr **)(tcb + tcbsize - TLS_TCB_SIZE);

    if (oldtcb != NULL) {
	memcpy(tls, oldtcb, tls_static_space);
	free(oldtcb);

	/* Adjust the DTV. */
	dtv = tls[0];
	for (i = 0; i < dtv[1]; i++) {
	    if (dtv[i+2] >= (Elf_Addr)oldtcb &&
		dtv[i+2] < (Elf_Addr)oldtcb + tls_static_space) {
		dtv[i+2] = dtv[i+2] - (Elf_Addr)oldtcb + (Elf_Addr)tls;
	    }
	}
    } else {
	dtv = calloc(tls_max_index + 2, sizeof(Elf_Addr));
	tls[0] = dtv;
	dtv[0] = tls_dtv_generation;
	dtv[1] = tls_max_index;

	for (obj = objs; obj; obj = obj->next) {
	    if (obj->tlsoffset > 0) {
		addr = (Elf_Addr)tls + obj->tlsoffset;
		if (obj->tlsinitsize > 0)
		    memcpy((void*) addr, obj->tlsinit, obj->tlsinitsize);
		if (obj->tlssize > obj->tlsinitsize)
		    memset((void*) (addr + obj->tlsinitsize), 0,
			   obj->tlssize - obj->tlsinitsize);
		dtv[obj->tlsindex + 1] = addr;
	    }
	}
    }

    return (tcb);
}

void
free_tls(void *tcb, size_t tcbsize, size_t tcbalign)
{
    Elf_Addr *dtv;
    Elf_Addr tlsstart, tlsend;
    int dtvsize, i;

    assert(tcbsize >= TLS_TCB_SIZE);

    tlsstart = (Elf_Addr)tcb + tcbsize - TLS_TCB_SIZE;
    tlsend = tlsstart + tls_static_space;

    dtv = *(Elf_Addr **)tlsstart;
    dtvsize = dtv[1];
    for (i = 0; i < dtvsize; i++) {
	if (dtv[i+2] && (dtv[i+2] < tlsstart || dtv[i+2] >= tlsend)) {
	    free((void*)dtv[i+2]);
	}
    }
    free(dtv);
    free(tcb);
}

#endif

#if defined(__i386__) || defined(__amd64__) || defined(__sparc64__) || \
    defined(__arm__) || defined(__mips__)

/*
 * Allocate Static TLS using the Variant II method.
 */
void *
allocate_tls(Obj_Entry *objs, void *oldtls, size_t tcbsize, size_t tcbalign)
{
    Obj_Entry *obj;
    size_t size;
    char *tls;
    Elf_Addr *dtv, *olddtv;
    Elf_Addr segbase, oldsegbase, addr;
    int i;

    size = round(tls_static_space, tcbalign);

    assert(tcbsize >= 2*sizeof(Elf_Addr));
    tls = calloc(1, size + tcbsize);
    dtv = calloc(1, (tls_max_index + 2) * sizeof(Elf_Addr));

    segbase = (Elf_Addr)(tls + size);
    ((Elf_Addr*)segbase)[0] = segbase;
    ((Elf_Addr*)segbase)[1] = (Elf_Addr) dtv;

    dtv[0] = tls_dtv_generation;
    dtv[1] = tls_max_index;

    if (oldtls) {
	/*
	 * Copy the static TLS block over whole.
	 */
	oldsegbase = (Elf_Addr) oldtls;
	memcpy((void *)(segbase - tls_static_space),
	       (const void *)(oldsegbase - tls_static_space),
	       tls_static_space);

	/*
	 * If any dynamic TLS blocks have been created tls_get_addr(),
	 * move them over.
	 */
	olddtv = ((Elf_Addr**)oldsegbase)[1];
	for (i = 0; i < olddtv[1]; i++) {
	    if (olddtv[i+2] < oldsegbase - size || olddtv[i+2] > oldsegbase) {
		dtv[i+2] = olddtv[i+2];
		olddtv[i+2] = 0;
	    }
	}

	/*
	 * We assume that this block was the one we created with
	 * allocate_initial_tls().
	 */
	free_tls(oldtls, 2*sizeof(Elf_Addr), sizeof(Elf_Addr));
    } else {
	for (obj = objs; obj; obj = obj->next) {
	    if (obj->tlsoffset) {
		addr = segbase - obj->tlsoffset;
		memset((void*) (addr + obj->tlsinitsize),
		       0, obj->tlssize - obj->tlsinitsize);
		if (obj->tlsinit)
		    memcpy((void*) addr, obj->tlsinit, obj->tlsinitsize);
		dtv[obj->tlsindex + 1] = addr;
	    }
	}
    }

    return (void*) segbase;
}

void
free_tls(void *tls, size_t tcbsize, size_t tcbalign)
{
    size_t size;
    Elf_Addr* dtv;
    int dtvsize, i;
    Elf_Addr tlsstart, tlsend;

    /*
     * Figure out the size of the initial TLS block so that we can
     * find stuff which ___tls_get_addr() allocated dynamically.
     */
    size = round(tls_static_space, tcbalign);

    dtv = ((Elf_Addr**)tls)[1];
    dtvsize = dtv[1];
    tlsend = (Elf_Addr) tls;
    tlsstart = tlsend - size;
    for (i = 0; i < dtvsize; i++) {
	if (dtv[i+2] && (dtv[i+2] < tlsstart || dtv[i+2] > tlsend)) {
	    free((void*) dtv[i+2]);
	}
    }

    free((void*) tlsstart);
    free((void*) dtv);
}

#endif

/*
 * Allocate TLS block for module with given index.
 */
void *
allocate_module_tls(int index)
{
    Obj_Entry* obj;
    char* p;

    for (obj = obj_list; obj; obj = obj->next) {
	if (obj->tlsindex == index)
	    break;
    }
    if (!obj) {
	_rtld_error("Can't find module with TLS index %d", index);
	die();
    }

    p = malloc(obj->tlssize);
    if (p == NULL) {
	_rtld_error("Cannot allocate TLS block for index %d", index);
	die();
    }
    memcpy(p, obj->tlsinit, obj->tlsinitsize);
    memset(p + obj->tlsinitsize, 0, obj->tlssize - obj->tlsinitsize);

    return p;
}

bool
allocate_tls_offset(Obj_Entry *obj)
{
    size_t off;

    if (obj->tls_done)
	return true;

    if (obj->tlssize == 0) {
	obj->tls_done = true;
	return true;
    }

    if (obj->tlsindex == 1)
	off = calculate_first_tls_offset(obj->tlssize, obj->tlsalign);
    else
	off = calculate_tls_offset(tls_last_offset, tls_last_size,
				   obj->tlssize, obj->tlsalign);

    /*
     * If we have already fixed the size of the static TLS block, we
     * must stay within that size. When allocating the static TLS, we
     * leave a small amount of space spare to be used for dynamically
     * loading modules which use static TLS.
     */
    if (tls_static_space) {
	if (calculate_tls_end(off, obj->tlssize) > tls_static_space)
	    return false;
    }

    tls_last_offset = obj->tlsoffset = off;
    tls_last_size = obj->tlssize;
    obj->tls_done = true;

    return true;
}

void
free_tls_offset(Obj_Entry *obj)
{

    /*
     * If we were the last thing to allocate out of the static TLS
     * block, we give our space back to the 'allocator'. This is a
     * simplistic workaround to allow libGL.so.1 to be loaded and
     * unloaded multiple times.
     */
    if (calculate_tls_end(obj->tlsoffset, obj->tlssize)
	== calculate_tls_end(tls_last_offset, tls_last_size)) {
	tls_last_offset -= obj->tlssize;
	tls_last_size = 0;
    }
}

void *
_rtld_allocate_tls(void *oldtls, size_t tcbsize, size_t tcbalign)
{
    void *ret;
    RtldLockState lockstate;

    wlock_acquire(rtld_bind_lock, &lockstate);
    ret = allocate_tls(obj_list, oldtls, tcbsize, tcbalign);
    lock_release(rtld_bind_lock, &lockstate);
    return (ret);
}

void
_rtld_free_tls(void *tcb, size_t tcbsize, size_t tcbalign)
{
    RtldLockState lockstate;

    wlock_acquire(rtld_bind_lock, &lockstate);
    free_tls(tcb, tcbsize, tcbalign);
    lock_release(rtld_bind_lock, &lockstate);
}

static void
object_add_name(Obj_Entry *obj, const char *name)
{
    Name_Entry *entry;
    size_t len;

    len = strlen(name);
    entry = malloc(sizeof(Name_Entry) + len);

    if (entry != NULL) {
	strcpy(entry->name, name);
	STAILQ_INSERT_TAIL(&obj->names, entry, link);
    }
}

static int
object_match_name(const Obj_Entry *obj, const char *name)
{
    Name_Entry *entry;

    STAILQ_FOREACH(entry, &obj->names, link) {
	if (strcmp(name, entry->name) == 0)
	    return (1);
    }
    return (0);
}

static Obj_Entry *
locate_dependency(const Obj_Entry *obj, const char *name)
{
    const Objlist_Entry *entry;
    const Needed_Entry *needed;

    STAILQ_FOREACH(entry, &list_main, link) {
	if (object_match_name(entry->obj, name))
	    return entry->obj;
    }

    for (needed = obj->needed;  needed != NULL;  needed = needed->next) {
	if (needed->obj == NULL)
	    continue;
	if (object_match_name(needed->obj, name))
	    return needed->obj;
    }
    _rtld_error("%s: Unexpected inconsistency: dependency %s not found",
	obj->path, name);
    die();
}

static int
check_object_provided_version(Obj_Entry *refobj, const Obj_Entry *depobj,
    const Elf_Vernaux *vna)
{
    const Elf_Verdef *vd;
    const char *vername;

    vername = refobj->strtab + vna->vna_name;
    vd = depobj->verdef;
    if (vd == NULL) {
	_rtld_error("%s: version %s required by %s not defined",
	    depobj->path, vername, refobj->path);
	return (-1);
    }
    for (;;) {
	if (vd->vd_version != VER_DEF_CURRENT) {
	    _rtld_error("%s: Unsupported version %d of Elf_Verdef entry",
		depobj->path, vd->vd_version);
	    return (-1);
	}
	if (vna->vna_hash == vd->vd_hash) {
	    const Elf_Verdaux *aux = (const Elf_Verdaux *)
		((char *)vd + vd->vd_aux);
	    if (strcmp(vername, depobj->strtab + aux->vda_name) == 0)
		return (0);
	}
	if (vd->vd_next == 0)
	    break;
	vd = (const Elf_Verdef *) ((char *)vd + vd->vd_next);
    }
    if (vna->vna_flags & VER_FLG_WEAK)
	return (0);
    _rtld_error("%s: version %s required by %s not found",
	depobj->path, vername, refobj->path);
    return (-1);
}

static int
rtld_verify_object_versions(Obj_Entry *obj)
{
    const Elf_Verneed *vn;
    const Elf_Verdef  *vd;
    const Elf_Verdaux *vda;
    const Elf_Vernaux *vna;
    const Obj_Entry *depobj;
    int maxvernum, vernum;

    maxvernum = 0;
    /*
     * Walk over defined and required version records and figure out
     * max index used by any of them. Do very basic sanity checking
     * while there.
     */
    vn = obj->verneed;
    while (vn != NULL) {
	if (vn->vn_version != VER_NEED_CURRENT) {
	    _rtld_error("%s: Unsupported version %d of Elf_Verneed entry",
		obj->path, vn->vn_version);
	    return (-1);
	}
	vna = (const Elf_Vernaux *) ((char *)vn + vn->vn_aux);
	for (;;) {
	    vernum = VER_NEED_IDX(vna->vna_other);
	    if (vernum > maxvernum)
		maxvernum = vernum;
	    if (vna->vna_next == 0)
		 break;
	    vna = (const Elf_Vernaux *) ((char *)vna + vna->vna_next);
	}
	if (vn->vn_next == 0)
	    break;
	vn = (const Elf_Verneed *) ((char *)vn + vn->vn_next);
    }

    vd = obj->verdef;
    while (vd != NULL) {
	if (vd->vd_version != VER_DEF_CURRENT) {
	    _rtld_error("%s: Unsupported version %d of Elf_Verdef entry",
		obj->path, vd->vd_version);
	    return (-1);
	}
	vernum = VER_DEF_IDX(vd->vd_ndx);
	if (vernum > maxvernum)
		maxvernum = vernum;
	if (vd->vd_next == 0)
	    break;
	vd = (const Elf_Verdef *) ((char *)vd + vd->vd_next);
    }

    if (maxvernum == 0)
	return (0);

    /*
     * Store version information in array indexable by version index.
     * Verify that object version requirements are satisfied along the
     * way.
     */
    obj->vernum = maxvernum + 1;
    obj->vertab = calloc(obj->vernum, sizeof(Ver_Entry));

    vd = obj->verdef;
    while (vd != NULL) {
	if ((vd->vd_flags & VER_FLG_BASE) == 0) {
	    vernum = VER_DEF_IDX(vd->vd_ndx);
	    assert(vernum <= maxvernum);
	    vda = (const Elf_Verdaux *)((char *)vd + vd->vd_aux);
	    obj->vertab[vernum].hash = vd->vd_hash;
	    obj->vertab[vernum].name = obj->strtab + vda->vda_name;
	    obj->vertab[vernum].file = NULL;
	    obj->vertab[vernum].flags = 0;
	}
	if (vd->vd_next == 0)
	    break;
	vd = (const Elf_Verdef *) ((char *)vd + vd->vd_next);
    }

    vn = obj->verneed;
    while (vn != NULL) {
	depobj = locate_dependency(obj, obj->strtab + vn->vn_file);
	vna = (const Elf_Vernaux *) ((char *)vn + vn->vn_aux);
	for (;;) {
	    if (check_object_provided_version(obj, depobj, vna))
		return (-1);
	    vernum = VER_NEED_IDX(vna->vna_other);
	    assert(vernum <= maxvernum);
	    obj->vertab[vernum].hash = vna->vna_hash;
	    obj->vertab[vernum].name = obj->strtab + vna->vna_name;
	    obj->vertab[vernum].file = obj->strtab + vn->vn_file;
	    obj->vertab[vernum].flags = (vna->vna_other & VER_NEED_HIDDEN) ?
		VER_INFO_HIDDEN : 0;
	    if (vna->vna_next == 0)
		 break;
	    vna = (const Elf_Vernaux *) ((char *)vna + vna->vna_next);
	}
	if (vn->vn_next == 0)
	    break;
	vn = (const Elf_Verneed *) ((char *)vn + vn->vn_next);
    }
    return 0;
}

static int
rtld_verify_versions(const Objlist *objlist)
{
    Objlist_Entry *entry;
    int rc;

    rc = 0;
    STAILQ_FOREACH(entry, objlist, link) {
	/*
	 * Skip dummy objects or objects that have their version requirements
	 * already checked.
	 */
	if (entry->obj->strtab == NULL || entry->obj->vertab != NULL)
	    continue;
	if (rtld_verify_object_versions(entry->obj) == -1) {
	    rc = -1;
	    if (ld_tracing == NULL)
		break;
	}
    }
    if (rc == 0 || ld_tracing != NULL)
    	rc = rtld_verify_object_versions(&obj_rtld);
    return rc;
}

const Ver_Entry *
fetch_ventry(const Obj_Entry *obj, unsigned long symnum)
{
    Elf_Versym vernum;

    if (obj->vertab) {
	vernum = VER_NDX(obj->versyms[symnum]);
	if (vernum >= obj->vernum) {
	    _rtld_error("%s: symbol %s has wrong verneed value %d",
		obj->path, obj->strtab + symnum, vernum);
	} else if (obj->vertab[vernum].hash != 0) {
	    return &obj->vertab[vernum];
	}
    }
    return NULL;
}

int
_rtld_get_stack_prot(void)
{

	return (stack_prot);
}

static void
map_stacks_exec(RtldLockState *lockstate)
{
	void (*thr_map_stacks_exec)(void);

	if ((max_stack_flags & PF_X) == 0 || (stack_prot & PROT_EXEC) != 0)
		return;
	thr_map_stacks_exec = (void (*)(void))(uintptr_t)
	    get_program_var_addr("__pthread_map_stacks_exec", lockstate);
	if (thr_map_stacks_exec != NULL) {
		stack_prot |= PROT_EXEC;
		thr_map_stacks_exec();
	}
}

void
symlook_init(SymLook *dst, const char *name)
{

	bzero(dst, sizeof(*dst));
	dst->name = name;
	dst->hash = elf_hash(name);
}

static void
symlook_init_from_req(SymLook *dst, const SymLook *src)
{

	dst->name = src->name;
	dst->hash = src->hash;
	dst->ventry = src->ventry;
	dst->flags = src->flags;
	dst->defobj_out = NULL;
	dst->sym_out = NULL;
	dst->lockstate = src->lockstate;
}

/*
 * Overrides for libc_pic-provided functions.
 */

int
__getosreldate(void)
{
	size_t len;
	int oid[2];
	int error, osrel;

	if (osreldate != 0)
		return (osreldate);

	oid[0] = CTL_KERN;
	oid[1] = KERN_OSRELDATE;
	osrel = 0;
	len = sizeof(osrel);
	error = sysctl(oid, 2, &osrel, &len, NULL, 0);
	if (error == 0 && osrel > 0 && len == sizeof(osrel))
		osreldate = osrel;
	return (osreldate);
}

/*
 * No unresolved symbols for rtld.
 */
void
__pthread_cxa_finalize(struct dl_phdr_info *a)
{
}
