/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 1996, 1997, 1998, 1999, 2000 John D. Polstra.
 * Copyright 2003 Alexander Kabaev <kan@FreeBSD.ORG>.
 * Copyright 2009-2013 Konstantin Belousov <kib@FreeBSD.ORG>.
 * Copyright 2012 John Marino <draco@marino.st>.
 * Copyright 2014-2017 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by Konstantin Belousov
 * under sponsorship from the FreeBSD Foundation.
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
 */

/*
 * Dynamic linker for ELF.
 *
 * John Polstra <jdp@polstra.com>.
 */

#include <sys/param.h>
#include <sys/ktrace.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/utsname.h>

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
#include "libmap.h"
#include "notes.h"
#include "rtld.h"
#include "rtld_libc.h"
#include "rtld_malloc.h"
#include "rtld_paths.h"
#include "rtld_printf.h"
#include "rtld_tls.h"
#include "rtld_utrace.h"

/* Types. */
typedef void (*func_ptr_type)(void);
typedef void *(*path_enum_proc)(const char *path, size_t len, void *arg);

/* Variables that cannot be static: */
extern struct r_debug r_debug; /* For GDB */
extern int _thread_autoinit_dummy_decl;
extern void (*__cleanup)(void);

struct dlerror_save {
	int seen;
	char *msg;
};

/*
 * Function declarations.
 */
static const char *basename(const char *);
static void digest_dynamic1(Obj_Entry *, int, const Elf_Dyn **,
    const Elf_Dyn **, const Elf_Dyn **);
static bool digest_dynamic2(Obj_Entry *, const Elf_Dyn *, const Elf_Dyn *,
    const Elf_Dyn *);
static bool digest_dynamic(Obj_Entry *, int);
static Obj_Entry *digest_phdr(const Elf_Phdr *, int, caddr_t, const char *);
static void distribute_static_tls(Objlist *, RtldLockState *);
static Obj_Entry *dlcheck(void *);
static int dlclose_locked(void *, RtldLockState *);
static Obj_Entry *dlopen_object(const char *name, int fd, Obj_Entry *refobj,
    int lo_flags, int mode, RtldLockState *lockstate);
static Obj_Entry *do_load_object(int, const char *, char *, struct stat *, int);
static int do_search_info(const Obj_Entry *obj, int, struct dl_serinfo *);
static bool donelist_check(DoneList *, const Obj_Entry *);
static void dump_auxv(Elf_Auxinfo **aux_info);
static void errmsg_restore(struct dlerror_save *);
static struct dlerror_save *errmsg_save(void);
static void *fill_search_info(const char *, size_t, void *);
static char *find_library(const char *, const Obj_Entry *, int *);
static const char *gethints(bool);
static void hold_object(Obj_Entry *);
static void unhold_object(Obj_Entry *);
static void init_dag(Obj_Entry *);
static void init_marker(Obj_Entry *);
static void init_pagesizes(Elf_Auxinfo **aux_info);
static void init_rtld(caddr_t, Elf_Auxinfo **);
static void initlist_add_neededs(Needed_Entry *, Objlist *, Objlist *);
static void initlist_add_objects(Obj_Entry *, Obj_Entry *, Objlist *,
    Objlist *);
static void initlist_for_loaded_obj(Obj_Entry *obj, Obj_Entry *tail,
    Objlist *list);
static int initlist_objects_ifunc(Objlist *, bool, int, RtldLockState *);
static void linkmap_add(Obj_Entry *);
static void linkmap_delete(Obj_Entry *);
static void load_filtees(Obj_Entry *, int flags, RtldLockState *);
static void unload_filtees(Obj_Entry *, RtldLockState *);
static int load_needed_objects(Obj_Entry *, int);
static int load_preload_objects(const char *, bool);
static int load_kpreload(const void *addr);
static Obj_Entry *load_object(const char *, int fd, const Obj_Entry *, int);
static void map_stacks_exec(RtldLockState *);
static int obj_disable_relro(Obj_Entry *);
static int obj_enforce_relro(Obj_Entry *);
static void objlist_call_fini(Objlist *, Obj_Entry *, RtldLockState *);
static void objlist_call_init(Objlist *, RtldLockState *);
static void objlist_clear(Objlist *);
static Objlist_Entry *objlist_find(Objlist *, const Obj_Entry *);
static void objlist_init(Objlist *);
static void objlist_push_head(Objlist *, Obj_Entry *);
static void objlist_push_tail(Objlist *, Obj_Entry *);
static void objlist_put_after(Objlist *, Obj_Entry *, Obj_Entry *);
static void objlist_remove(Objlist *, Obj_Entry *);
static int open_binary_fd(const char *argv0, bool search_in_path,
    const char **binpath_res);
static int parse_args(char *argv[], int argc, bool *use_pathp, int *fdp,
    const char **argv0, bool *dir_ignore);
static int parse_integer(const char *);
static void *path_enumerate(const char *, path_enum_proc, const char *, void *);
static void print_usage(const char *argv0);
static void release_object(Obj_Entry *);
static int relocate_object_dag(Obj_Entry *root, bool bind_now,
    Obj_Entry *rtldobj, int flags, RtldLockState *lockstate);
static int relocate_object(Obj_Entry *obj, bool bind_now, Obj_Entry *rtldobj,
    int flags, RtldLockState *lockstate);
static int relocate_objects(Obj_Entry *, bool, Obj_Entry *, int,
    RtldLockState *);
static int resolve_object_ifunc(Obj_Entry *, bool, int, RtldLockState *);
static int rtld_dirname(const char *, char *);
static int rtld_dirname_abs(const char *, char *);
static void *rtld_dlopen(const char *name, int fd, int mode);
static void rtld_exit(void);
static void rtld_nop_exit(void);
static char *search_library_path(const char *, const char *, const char *,
    int *);
static char *search_library_pathfds(const char *, const char *, int *);
static const void **get_program_var_addr(const char *, RtldLockState *);
static void set_program_var(const char *, const void *);
static int symlook_default(SymLook *, const Obj_Entry *refobj);
static int symlook_global(SymLook *, DoneList *);
static void symlook_init_from_req(SymLook *, const SymLook *);
static int symlook_list(SymLook *, const Objlist *, DoneList *);
static int symlook_needed(SymLook *, const Needed_Entry *, DoneList *);
static int symlook_obj1_sysv(SymLook *, const Obj_Entry *);
static int symlook_obj1_gnu(SymLook *, const Obj_Entry *);
static void *tls_get_addr_slow(struct dtv **, int, size_t, bool) __noinline;
static void trace_loaded_objects(Obj_Entry *, bool);
static void unlink_object(Obj_Entry *);
static void unload_object(Obj_Entry *, RtldLockState *lockstate);
static void unref_dag(Obj_Entry *);
static void ref_dag(Obj_Entry *);
static char *origin_subst_one(Obj_Entry *, char *, const char *, const char *,
    bool);
static char *origin_subst(Obj_Entry *, const char *);
static bool obj_resolve_origin(Obj_Entry *obj);
static void preinit_main(void);
static int rtld_verify_versions(const Objlist *);
static int rtld_verify_object_versions(Obj_Entry *);
static void object_add_name(Obj_Entry *, const char *);
static int object_match_name(const Obj_Entry *, const char *);
static void ld_utrace_log(int, void *, void *, size_t, int, const char *);
static void rtld_fill_dl_phdr_info(const Obj_Entry *obj,
    struct dl_phdr_info *phdr_info);
static uint32_t gnu_hash(const char *);
static bool matched_symbol(SymLook *, const Obj_Entry *, Sym_Match_Result *,
    const unsigned long);

void r_debug_state(struct r_debug *, struct link_map *) __noinline __exported;
void _r_debug_postinit(struct link_map *) __noinline __exported;

int __sys_openat(int, const char *, int, ...);

/*
 * Data declarations.
 */
struct r_debug r_debug __exported;  /* for GDB; */
static bool libmap_disable;	    /* Disable libmap */
static bool ld_loadfltr;	    /* Immediate filters processing */
static const char *libmap_override; /* Maps to use in addition to libmap.conf */
static bool trust;		    /* False for setuid and setgid programs */
static bool dangerous_ld_env;	    /* True if environment variables have been
				       used to affect the libraries loaded */
bool ld_bind_not;		    /* Disable PLT update */
static const char *ld_bind_now; /* Environment variable for immediate binding */
static const char *ld_debug;	/* Environment variable for debugging */
static bool ld_dynamic_weak = true; /* True if non-weak definition overrides
				       weak definition */
static const char *ld_library_path; /* Environment variable for search path */
static const char
    *ld_library_dirs; /* Environment variable for library descriptors */
static const char *ld_preload;	   /* Environment variable for libraries to
				      load first */
static const char *ld_preload_fds; /* Environment variable for libraries
				    represented by descriptors */
static const char
    *ld_elf_hints_path; /* Environment variable for alternative hints path */
static const char *ld_tracing;	    /* Called from ldd to print libs */
static const char *ld_utrace;	    /* Use utrace() to log events. */
static struct obj_entry_q obj_list; /* Queue of all loaded objects */
static Obj_Entry *obj_main;	    /* The main program shared object */
static Obj_Entry obj_rtld;	    /* The dynamic linker shared object */
static unsigned int obj_count;	    /* Number of objects in obj_list */
static unsigned int obj_loads;	    /* Number of loads of objects (gen count) */
size_t ld_static_tls_extra =	    /* Static TLS extra space (bytes) */
    RTLD_STATIC_TLS_EXTRA;

static Objlist list_global = /* Objects dlopened with RTLD_GLOBAL */
    STAILQ_HEAD_INITIALIZER(list_global);
static Objlist list_main = /* Objects loaded at program startup */
    STAILQ_HEAD_INITIALIZER(list_main);
static Objlist list_fini = /* Objects needing fini() calls */
    STAILQ_HEAD_INITIALIZER(list_fini);

Elf_Sym sym_zero; /* For resolving undefined weak refs. */

#define GDB_STATE(s, m)      \
	r_debug.r_state = s; \
	r_debug_state(&r_debug, m);

extern Elf_Dyn _DYNAMIC;
#pragma weak _DYNAMIC

int dlclose(void *) __exported;
char *dlerror(void) __exported;
void *dlopen(const char *, int) __exported;
void *fdlopen(int, int) __exported;
void *dlsym(void *, const char *) __exported;
dlfunc_t dlfunc(void *, const char *) __exported;
void *dlvsym(void *, const char *, const char *) __exported;
int dladdr(const void *, Dl_info *) __exported;
void dllockinit(void *, void *(*)(void *), void (*)(void *), void (*)(void *),
    void (*)(void *), void (*)(void *), void (*)(void *)) __exported;
int dlinfo(void *, int, void *) __exported;
int _dl_iterate_phdr_locked(__dl_iterate_hdr_callback, void *) __exported;
int dl_iterate_phdr(__dl_iterate_hdr_callback, void *) __exported;
int _rtld_addr_phdr(const void *, struct dl_phdr_info *) __exported;
int _rtld_get_stack_prot(void) __exported;
int _rtld_is_dlopened(void *) __exported;
void _rtld_error(const char *, ...) __exported;
const char *rtld_get_var(const char *name) __exported;
int rtld_set_var(const char *name, const char *val) __exported;

/* Only here to fix -Wmissing-prototypes warnings */
int __getosreldate(void);
func_ptr_type _rtld(Elf_Addr *sp, func_ptr_type *exit_proc, Obj_Entry **objp);
Elf_Addr _rtld_bind(Obj_Entry *obj, Elf_Size reloff);

int npagesizes;
static int osreldate;
size_t *pagesizes;
size_t page_size;

static int stack_prot = PROT_READ | PROT_WRITE | PROT_EXEC;
static int max_stack_flags;

/*
 * Global declarations normally provided by crt1.  The dynamic linker is
 * not built with crt1, so we have to provide them ourselves.
 */
char *__progname;
char **environ;

/*
 * Used to pass argc, argv to init functions.
 */
int main_argc;
char **main_argv;

/*
 * Globals to control TLS allocation.
 */
size_t tls_last_offset;	 /* Static TLS offset of last module */
size_t tls_last_size;	 /* Static TLS size of last module */
size_t tls_static_space; /* Static TLS space allocated */
static size_t tls_static_max_align;
Elf_Addr tls_dtv_generation = 1; /* Used to detect when dtv size changes */
int tls_max_index = 1;		 /* Largest module index allocated */

static bool ld_library_path_rpath = false;
bool ld_fast_sigblock = false;

/*
 * Globals for path names, and such
 */
const char *ld_elf_hints_default = _PATH_ELF_HINTS;
const char *ld_path_libmap_conf = _PATH_LIBMAP_CONF;
const char *ld_path_rtld = _PATH_RTLD;
const char *ld_standard_library_path = STANDARD_LIBRARY_PATH;
const char *ld_env_prefix = LD_;

static void (*rtld_exit_ptr)(void);

/*
 * Fill in a DoneList with an allocation large enough to hold all of
 * the currently-loaded objects.  Keep this as a macro since it calls
 * alloca and we want that to occur within the scope of the caller.
 */
#define donelist_init(dlp)                                             \
	((dlp)->objs = alloca(obj_count * sizeof(dlp)->objs[0]),       \
	    assert((dlp)->objs != NULL), (dlp)->num_alloc = obj_count, \
	    (dlp)->num_used = 0)

#define LD_UTRACE(e, h, mb, ms, r, n)                      \
	do {                                               \
		if (ld_utrace != NULL)                     \
			ld_utrace_log(e, h, mb, ms, r, n); \
	} while (0)

static void
ld_utrace_log(int event, void *handle, void *mapbase, size_t mapsize,
    int refcnt, const char *name)
{
	struct utrace_rtld ut;
	static const char rtld_utrace_sig[RTLD_UTRACE_SIG_SZ] = RTLD_UTRACE_SIG;

	memset(&ut, 0, sizeof(ut));	/* clear holes */
	memcpy(ut.sig, rtld_utrace_sig, sizeof(ut.sig));
	ut.event = event;
	ut.handle = handle;
	ut.mapbase = mapbase;
	ut.mapsize = mapsize;
	ut.refcnt = refcnt;
	if (name != NULL)
		strlcpy(ut.name, name, sizeof(ut.name));
	utrace(&ut, sizeof(ut));
}

struct ld_env_var_desc {
	const char *const n;
	const char *val;
	const bool unsecure : 1;
	const bool can_update : 1;
	const bool debug : 1;
	bool owned : 1;
};
#define LD_ENV_DESC(var, unsec, ...) \
	[LD_##var] = { .n = #var, .unsecure = unsec, __VA_ARGS__ }

static struct ld_env_var_desc ld_env_vars[] = {
	LD_ENV_DESC(BIND_NOW, false),
	LD_ENV_DESC(PRELOAD, true),
	LD_ENV_DESC(LIBMAP, true),
	LD_ENV_DESC(LIBRARY_PATH, true, .can_update = true),
	LD_ENV_DESC(LIBRARY_PATH_FDS, true, .can_update = true),
	LD_ENV_DESC(LIBMAP_DISABLE, true),
	LD_ENV_DESC(BIND_NOT, true),
	LD_ENV_DESC(DEBUG, true, .can_update = true, .debug = true),
	LD_ENV_DESC(ELF_HINTS_PATH, true),
	LD_ENV_DESC(LOADFLTR, true),
	LD_ENV_DESC(LIBRARY_PATH_RPATH, true, .can_update = true),
	LD_ENV_DESC(PRELOAD_FDS, true),
	LD_ENV_DESC(DYNAMIC_WEAK, true, .can_update = true),
	LD_ENV_DESC(TRACE_LOADED_OBJECTS, false),
	LD_ENV_DESC(UTRACE, false, .can_update = true),
	LD_ENV_DESC(DUMP_REL_PRE, false, .can_update = true),
	LD_ENV_DESC(DUMP_REL_POST, false, .can_update = true),
	LD_ENV_DESC(TRACE_LOADED_OBJECTS_PROGNAME, false),
	LD_ENV_DESC(TRACE_LOADED_OBJECTS_FMT1, false),
	LD_ENV_DESC(TRACE_LOADED_OBJECTS_FMT2, false),
	LD_ENV_DESC(TRACE_LOADED_OBJECTS_ALL, false),
	LD_ENV_DESC(SHOW_AUXV, false),
	LD_ENV_DESC(STATIC_TLS_EXTRA, false),
	LD_ENV_DESC(NO_DL_ITERATE_PHDR_AFTER_FORK, false),
};

const char *
ld_get_env_var(int idx)
{
	return (ld_env_vars[idx].val);
}

static const char *
rtld_get_env_val(char **env, const char *name, size_t name_len)
{
	char **m, *n, *v;

	for (m = env; *m != NULL; m++) {
		n = *m;
		v = strchr(n, '=');
		if (v == NULL) {
			/* corrupt environment? */
			continue;
		}
		if (v - n == (ptrdiff_t)name_len &&
		    strncmp(name, n, name_len) == 0)
			return (v + 1);
	}
	return (NULL);
}

static void
rtld_init_env_vars_for_prefix(char **env, const char *env_prefix)
{
	struct ld_env_var_desc *lvd;
	size_t prefix_len, nlen;
	char **m, *n, *v;
	int i;

	prefix_len = strlen(env_prefix);
	for (m = env; *m != NULL; m++) {
		n = *m;
		if (strncmp(env_prefix, n, prefix_len) != 0) {
			/* Not a rtld environment variable. */
			continue;
		}
		n += prefix_len;
		v = strchr(n, '=');
		if (v == NULL) {
			/* corrupt environment? */
			continue;
		}
		for (i = 0; i < (int)nitems(ld_env_vars); i++) {
			lvd = &ld_env_vars[i];
			if (lvd->val != NULL) {
				/* Saw higher-priority variable name already. */
				continue;
			}
			nlen = strlen(lvd->n);
			if (v - n == (ptrdiff_t)nlen &&
			    strncmp(lvd->n, n, nlen) == 0) {
				lvd->val = v + 1;
				break;
			}
		}
	}
}

static void
rtld_init_env_vars(char **env)
{
	rtld_init_env_vars_for_prefix(env, ld_env_prefix);
}

static void
set_ld_elf_hints_path(void)
{
	if (ld_elf_hints_path == NULL || strlen(ld_elf_hints_path) == 0)
		ld_elf_hints_path = ld_elf_hints_default;
}

uintptr_t
rtld_round_page(uintptr_t x)
{
	return (roundup2(x, page_size));
}

uintptr_t
rtld_trunc_page(uintptr_t x)
{
	return (rounddown2(x, page_size));
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
	Elf_Auxinfo *aux, *auxp, *auxpf, *aux_info[AT_COUNT], auxtmp;
	Objlist_Entry *entry;
	Obj_Entry *last_interposer, *obj, *preload_tail;
	const Elf_Phdr *phdr;
	Objlist initlist;
	RtldLockState lockstate;
	struct stat st;
	Elf_Addr *argcp;
	char **argv, **env, **envp, *kexecpath;
	const char *argv0, *binpath, *library_path_rpath, *static_tls_extra;
	struct ld_env_var_desc *lvd;
	caddr_t imgentry;
	char buf[MAXPATHLEN];
	int argc, fd, i, mib[4], old_osrel, osrel, phnum, rtld_argc;
	size_t sz;
#ifdef __powerpc__
	int old_auxv_format = 1;
#endif
	bool dir_enable, dir_ignore, direct_exec, explicit_fd, search_in_path;

	/*
	 * On entry, the dynamic linker itself has not been relocated yet.
	 * Be very careful not to reference any global data until after
	 * init_rtld has returned.  It is OK to reference file-scope statics
	 * and string constants, and to call static and global functions.
	 */

	/* Find the auxiliary vector on the stack. */
	argcp = sp;
	argc = *sp++;
	argv = (char **)sp;
	sp += argc + 1; /* Skip over arguments and NULL terminator */
	env = (char **)sp;
	while (*sp++ != 0) /* Skip over environment, and NULL terminator */
		;
	aux = (Elf_Auxinfo *)sp;

	/* Digest the auxiliary vector. */
	for (i = 0; i < AT_COUNT; i++)
		aux_info[i] = NULL;
	for (auxp = aux; auxp->a_type != AT_NULL; auxp++) {
		if (auxp->a_type < AT_COUNT)
			aux_info[auxp->a_type] = auxp;
#ifdef __powerpc__
		if (auxp->a_type == 23) /* AT_STACKPROT */
			old_auxv_format = 0;
#endif
	}

#ifdef __powerpc__
	if (old_auxv_format) {
		/* Remap from old-style auxv numbers. */
		aux_info[23] = aux_info[21]; /* AT_STACKPROT */
		aux_info[21] = aux_info[19]; /* AT_PAGESIZESLEN */
		aux_info[19] = aux_info[17]; /* AT_NCPUS */
		aux_info[17] = aux_info[15]; /* AT_CANARYLEN */
		aux_info[15] = aux_info[13]; /* AT_EXECPATH */
		aux_info[13] = NULL;	     /* AT_GID */

		aux_info[20] = aux_info[18]; /* AT_PAGESIZES */
		aux_info[18] = aux_info[16]; /* AT_OSRELDATE */
		aux_info[16] = aux_info[14]; /* AT_CANARY */
		aux_info[14] = NULL;	     /* AT_EGID */
	}
#endif

	/* Initialize and relocate ourselves. */
	assert(aux_info[AT_BASE] != NULL);
	init_rtld((caddr_t)aux_info[AT_BASE]->a_un.a_ptr, aux_info);

	dlerror_dflt_init();

	__progname = obj_rtld.path;
	argv0 = argv[0] != NULL ? argv[0] : "(null)";
	environ = env;
	main_argc = argc;
	main_argv = argv;

	if (aux_info[AT_BSDFLAGS] != NULL &&
	    (aux_info[AT_BSDFLAGS]->a_un.a_val & ELF_BSDF_SIGFASTBLK) != 0)
		ld_fast_sigblock = true;

	trust = !issetugid();
	direct_exec = false;

	md_abi_variant_hook(aux_info);
	rtld_init_env_vars(env);

	fd = -1;
	if (aux_info[AT_EXECFD] != NULL) {
		fd = aux_info[AT_EXECFD]->a_un.a_val;
	} else {
		assert(aux_info[AT_PHDR] != NULL);
		phdr = (const Elf_Phdr *)aux_info[AT_PHDR]->a_un.a_ptr;
		if (phdr == obj_rtld.phdr) {
			if (!trust) {
				_rtld_error(
				    "Tainted process refusing to run binary %s",
				    argv0);
				rtld_die();
			}
			direct_exec = true;

			dbg("opening main program in direct exec mode");
			if (argc >= 2) {
				rtld_argc = parse_args(argv, argc,
				    &search_in_path, &fd, &argv0, &dir_ignore);
				explicit_fd = (fd != -1);
				binpath = NULL;
				if (!explicit_fd)
					fd = open_binary_fd(argv0,
					    search_in_path, &binpath);
				if (fstat(fd, &st) == -1) {
					_rtld_error(
					    "Failed to fstat FD %d (%s): %s",
					    fd,
					    explicit_fd ?
						"user-provided descriptor" :
						argv0,
					    rtld_strerror(errno));
					rtld_die();
				}

				/*
				 * Rough emulation of the permission checks done
				 * by execve(2), only Unix DACs are checked,
				 * ACLs are ignored.  Preserve the semantic of
				 * disabling owner to execute if owner x bit is
				 * cleared, even if others x bit is enabled.
				 * mmap(2) does not allow to mmap with PROT_EXEC
				 * if binary' file comes from noexec mount.  We
				 * cannot set a text reference on the binary.
				 */
				dir_enable = false;
				if (st.st_uid == geteuid()) {
					if ((st.st_mode & S_IXUSR) != 0)
						dir_enable = true;
				} else if (st.st_gid == getegid()) {
					if ((st.st_mode & S_IXGRP) != 0)
						dir_enable = true;
				} else if ((st.st_mode & S_IXOTH) != 0) {
					dir_enable = true;
				}
				if (!dir_enable && !dir_ignore) {
					_rtld_error(
				    "No execute permission for binary %s",
					    argv0);
					rtld_die();
				}

				/*
				 * For direct exec mode, argv[0] is the
				 * interpreter name, we must remove it and shift
				 * arguments left before invoking binary main.
				 * Since stack layout places environment
				 * pointers and aux vectors right after the
				 * terminating NULL, we must shift environment
				 * and aux as well.
				 */
				main_argc = argc - rtld_argc;
				for (i = 0; i <= main_argc; i++)
					argv[i] = argv[i + rtld_argc];
				*argcp -= rtld_argc;
				environ = env = envp = argv + main_argc + 1;
				dbg("move env from %p to %p", envp + rtld_argc,
				    envp);
				do {
					*envp = *(envp + rtld_argc);
				} while (*envp++ != NULL);
				aux = auxp = (Elf_Auxinfo *)envp;
				auxpf = (Elf_Auxinfo *)(envp + rtld_argc);
				dbg("move aux from %p to %p", auxpf, aux);
				/*
				 * XXXKIB insert place for AT_EXECPATH if not
				 * present
				 */
				for (;; auxp++, auxpf++) {
					/*
					 * NB: Use a temporary since *auxpf and
					 * *auxp overlap if rtld_argc is 1
					 */
					auxtmp = *auxpf;
					*auxp = auxtmp;
					if (auxp->a_type == AT_NULL)
						break;
				}
				/*
				 * Since the auxiliary vector has moved,
				 * redigest it.
				 */
				for (i = 0; i < AT_COUNT; i++)
					aux_info[i] = NULL;
				for (auxp = aux; auxp->a_type != AT_NULL;
				    auxp++) {
					if (auxp->a_type < AT_COUNT)
						aux_info[auxp->a_type] = auxp;
				}

				/*
				 * Point AT_EXECPATH auxv and aux_info to the
				 * binary path.
				 */
				if (binpath == NULL) {
					aux_info[AT_EXECPATH] = NULL;
				} else {
					if (aux_info[AT_EXECPATH] == NULL) {
						aux_info[AT_EXECPATH] = xmalloc(
						    sizeof(Elf_Auxinfo));
						aux_info[AT_EXECPATH]->a_type =
						    AT_EXECPATH;
					}
					aux_info[AT_EXECPATH]->a_un.a_ptr =
					    __DECONST(void *, binpath);
				}
			} else {
				_rtld_error("No binary");
				rtld_die();
			}
		}
	}

	ld_bind_now = ld_get_env_var(LD_BIND_NOW);

	/*
	 * If the process is tainted, then we un-set the dangerous environment
	 * variables.  The process will be marked as tainted until setuid(2)
	 * is called.  If any child process calls setuid(2) we do not want any
	 * future processes to honor the potentially un-safe variables.
	 */
	if (!trust) {
		for (i = 0; i < (int)nitems(ld_env_vars); i++) {
			lvd = &ld_env_vars[i];
			if (lvd->unsecure)
				lvd->val = NULL;
		}
	}

	ld_debug = ld_get_env_var(LD_DEBUG);
	if (ld_bind_now == NULL)
		ld_bind_not = ld_get_env_var(LD_BIND_NOT) != NULL;
	ld_dynamic_weak = ld_get_env_var(LD_DYNAMIC_WEAK) == NULL;
	libmap_disable = ld_get_env_var(LD_LIBMAP_DISABLE) != NULL;
	libmap_override = ld_get_env_var(LD_LIBMAP);
	ld_library_path = ld_get_env_var(LD_LIBRARY_PATH);
	ld_library_dirs = ld_get_env_var(LD_LIBRARY_PATH_FDS);
	ld_preload = ld_get_env_var(LD_PRELOAD);
	ld_preload_fds = ld_get_env_var(LD_PRELOAD_FDS);
	ld_elf_hints_path = ld_get_env_var(LD_ELF_HINTS_PATH);
	ld_loadfltr = ld_get_env_var(LD_LOADFLTR) != NULL;
	library_path_rpath = ld_get_env_var(LD_LIBRARY_PATH_RPATH);
	if (library_path_rpath != NULL) {
		if (library_path_rpath[0] == 'y' ||
		    library_path_rpath[0] == 'Y' ||
		    library_path_rpath[0] == '1')
			ld_library_path_rpath = true;
		else
			ld_library_path_rpath = false;
	}
	static_tls_extra = ld_get_env_var(LD_STATIC_TLS_EXTRA);
	if (static_tls_extra != NULL && static_tls_extra[0] != '\0') {
		sz = parse_integer(static_tls_extra);
		if (sz >= RTLD_STATIC_TLS_EXTRA && sz <= SIZE_T_MAX)
			ld_static_tls_extra = sz;
	}
	dangerous_ld_env = libmap_disable || libmap_override != NULL ||
	    ld_library_path != NULL || ld_preload != NULL ||
	    ld_elf_hints_path != NULL || ld_loadfltr || !ld_dynamic_weak ||
	    static_tls_extra != NULL;
	ld_tracing = ld_get_env_var(LD_TRACE_LOADED_OBJECTS);
	ld_utrace = ld_get_env_var(LD_UTRACE);

	set_ld_elf_hints_path();
	if (ld_debug != NULL && *ld_debug != '\0')
		debug = 1;
	dbg("%s is initialized, base address = %p", __progname,
	    (caddr_t)aux_info[AT_BASE]->a_un.a_ptr);
	dbg("RTLD dynamic = %p", obj_rtld.dynamic);
	dbg("RTLD pltgot  = %p", obj_rtld.pltgot);

	dbg("initializing thread locks");
	lockdflt_init();

	/*
	 * Load the main program, or process its program header if it is
	 * already loaded.
	 */
	if (fd != -1) { /* Load the main program. */
		dbg("loading main program");
		obj_main = map_object(fd, argv0, NULL, true);
		close(fd);
		if (obj_main == NULL)
			rtld_die();
		max_stack_flags = obj_main->stack_flags;
	} else { /* Main program already loaded. */
		dbg("processing main program's program header");
		assert(aux_info[AT_PHDR] != NULL);
		phdr = (const Elf_Phdr *)aux_info[AT_PHDR]->a_un.a_ptr;
		assert(aux_info[AT_PHNUM] != NULL);
		phnum = aux_info[AT_PHNUM]->a_un.a_val;
		assert(aux_info[AT_PHENT] != NULL);
		assert(aux_info[AT_PHENT]->a_un.a_val == sizeof(Elf_Phdr));
		assert(aux_info[AT_ENTRY] != NULL);
		imgentry = (caddr_t)aux_info[AT_ENTRY]->a_un.a_ptr;
		if ((obj_main = digest_phdr(phdr, phnum, imgentry, argv0)) ==
		    NULL)
			rtld_die();
	}

	if (aux_info[AT_EXECPATH] != NULL && fd == -1) {
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
		dbg("No AT_EXECPATH or direct exec");
		obj_main->path = xstrdup(argv0);
	}
	dbg("obj_main path %s", obj_main->path);
	obj_main->mainprog = true;

	if (aux_info[AT_STACKPROT] != NULL &&
	    aux_info[AT_STACKPROT]->a_un.a_val != 0)
		stack_prot = aux_info[AT_STACKPROT]->a_un.a_val;

#ifndef COMPAT_libcompat
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
#endif

	if (!digest_dynamic(obj_main, 0))
		rtld_die();
	dbg("%s valid_hash_sysv %d valid_hash_gnu %d dynsymcount %d",
	    obj_main->path, obj_main->valid_hash_sysv, obj_main->valid_hash_gnu,
	    obj_main->dynsymcount);

	linkmap_add(obj_main);
	linkmap_add(&obj_rtld);

	/* Link the main program into the list of objects. */
	TAILQ_INSERT_HEAD(&obj_list, obj_main, next);
	obj_count++;
	obj_loads++;

	/* Initialize a fake symbol for resolving undefined weak references. */
	sym_zero.st_info = ELF_ST_INFO(STB_GLOBAL, STT_NOTYPE);
	sym_zero.st_shndx = SHN_UNDEF;
	sym_zero.st_value = -(uintptr_t)obj_main->relocbase;

	if (!libmap_disable)
		libmap_disable = (bool)lm_init(libmap_override);

	if (aux_info[AT_KPRELOAD] != NULL &&
	    aux_info[AT_KPRELOAD]->a_un.a_ptr != NULL) {
		dbg("loading kernel vdso");
		if (load_kpreload(aux_info[AT_KPRELOAD]->a_un.a_ptr) == -1)
			rtld_die();
	}

	dbg("loading LD_PRELOAD_FDS libraries");
	if (load_preload_objects(ld_preload_fds, true) == -1)
		rtld_die();

	dbg("loading LD_PRELOAD libraries");
	if (load_preload_objects(ld_preload, false) == -1)
		rtld_die();
	preload_tail = globallist_curr(TAILQ_LAST(&obj_list, obj_entry_q));

	dbg("loading needed objects");
	if (load_needed_objects(obj_main,
		ld_tracing != NULL ? RTLD_LO_TRACE : 0) == -1)
		rtld_die();

	/* Make a list of all objects loaded at startup. */
	last_interposer = obj_main;
	TAILQ_FOREACH(obj, &obj_list, next) {
		if (obj->marker)
			continue;
		if (obj->z_interpose && obj != obj_main) {
			objlist_put_after(&list_main, last_interposer, obj);
			last_interposer = obj;
		} else {
			objlist_push_tail(&list_main, obj);
		}
		obj->refcount++;
	}

	dbg("checking for required versions");
	if (rtld_verify_versions(&list_main) == -1 && !ld_tracing)
		rtld_die();

	if (ld_get_env_var(LD_SHOW_AUXV) != NULL)
		dump_auxv(aux_info);

	if (ld_tracing) { /* We're done */
		trace_loaded_objects(obj_main, true);
		exit(0);
	}

	if (ld_get_env_var(LD_DUMP_REL_PRE) != NULL) {
		dump_relocations(obj_main);
		exit(0);
	}

	/*
	 * Processing tls relocations requires having the tls offsets
	 * initialized.  Prepare offsets before starting initial
	 * relocation processing.
	 */
	dbg("initializing initial thread local storage offsets");
	STAILQ_FOREACH(entry, &list_main, link) {
		/*
		 * Allocate all the initial objects out of the static TLS
		 * block even if they didn't ask for it.
		 */
		allocate_tls_offset(entry->obj);
	}

	if (relocate_objects(obj_main,
		ld_bind_now != NULL && *ld_bind_now != '\0', &obj_rtld,
		SYMLOOK_EARLY, NULL) == -1)
		rtld_die();

	dbg("doing copy relocations");
	if (do_copy_relocations(obj_main) == -1)
		rtld_die();

	if (ld_get_env_var(LD_DUMP_REL_POST) != NULL) {
		dump_relocations(obj_main);
		exit(0);
	}

	ifunc_init(aux_info);

	/*
	 * Setup TLS for main thread.  This must be done after the
	 * relocations are processed, since tls initialization section
	 * might be the subject for relocations.
	 */
	dbg("initializing initial thread local storage");
	allocate_initial_tls(globallist_curr(TAILQ_FIRST(&obj_list)));

	dbg("initializing key program variables");
	set_program_var("__progname", argv[0] != NULL ? basename(argv[0]) : "");
	set_program_var("environ", env);
	set_program_var("__elf_aux_vector", aux);

	/* Make a list of init functions to call. */
	objlist_init(&initlist);
	initlist_for_loaded_obj(globallist_curr(TAILQ_FIRST(&obj_list)),
	    preload_tail, &initlist);

	r_debug_state(NULL, &obj_main->linkmap); /* say hello to gdb! */

	map_stacks_exec(NULL);

	if (!obj_main->crt_no_init) {
		/*
		 * Make sure we don't call the main program's init and fini
		 * functions for binaries linked with old crt1 which calls
		 * _init itself.
		 */
		obj_main->init = obj_main->fini = (Elf_Addr)NULL;
		obj_main->preinit_array = obj_main->init_array =
		    obj_main->fini_array = (Elf_Addr)NULL;
	}

	if (direct_exec) {
		/* Set osrel for direct-execed binary */
		mib[0] = CTL_KERN;
		mib[1] = KERN_PROC;
		mib[2] = KERN_PROC_OSREL;
		mib[3] = getpid();
		osrel = obj_main->osrel;
		sz = sizeof(old_osrel);
		dbg("setting osrel to %d", osrel);
		(void)sysctl(mib, 4, &old_osrel, &sz, &osrel, sizeof(osrel));
	}

	wlock_acquire(rtld_bind_lock, &lockstate);

	dbg("resolving ifuncs");
	if (initlist_objects_ifunc(&initlist,
		ld_bind_now != NULL && *ld_bind_now != '\0', SYMLOOK_EARLY,
		&lockstate) == -1)
		rtld_die();

	rtld_exit_ptr = rtld_exit;
	if (obj_main->crt_no_init)
		preinit_main();
	objlist_call_init(&initlist, &lockstate);
	_r_debug_postinit(&obj_main->linkmap);
	objlist_clear(&initlist);
	dbg("loading filtees");
	TAILQ_FOREACH(obj, &obj_list, next) {
		if (obj->marker)
			continue;
		if (ld_loadfltr || obj->z_loadfltr)
			load_filtees(obj, 0, &lockstate);
	}

	dbg("enforcing main obj relro");
	if (obj_enforce_relro(obj_main) == -1)
		rtld_die();

	lock_release(rtld_bind_lock, &lockstate);

	dbg("transferring control to program entry point = %p",
	    obj_main->entry);

	/* Return the exit procedure and the program entry point. */
	*exit_proc = rtld_exit_ptr;
	*objp = obj_main;
	return ((func_ptr_type)obj_main->entry);
}

void *
rtld_resolve_ifunc(const Obj_Entry *obj, const Elf_Sym *def)
{
	void *ptr;
	Elf_Addr target;

	ptr = (void *)make_function_pointer(def, obj);
	target = call_ifunc_resolver(ptr);
	return ((void *)target);
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

relock:
	rlock_acquire(rtld_bind_lock, &lockstate);
	if (sigsetjmp(lockstate.env, 0) != 0)
		lock_upgrade(rtld_bind_lock, &lockstate);
	if (obj->pltrel)
		rel = (const Elf_Rel *)((const char *)obj->pltrel + reloff);
	else
		rel = (const Elf_Rel *)((const char *)obj->pltrela + reloff);

	where = (Elf_Addr *)(obj->relocbase + rel->r_offset);
	def = find_symdef(ELF_R_SYM(rel->r_info), obj, &defobj, SYMLOOK_IN_PLT,
	    NULL, &lockstate);
	if (def == NULL)
		rtld_die();
	if (ELF_ST_TYPE(def->st_info) == STT_GNU_IFUNC) {
		if (lockstate_wlocked(&lockstate)) {
			lock_release(rtld_bind_lock, &lockstate);
			goto relock;
		}
		target = (Elf_Addr)rtld_resolve_ifunc(defobj, def);
	} else {
		target = (Elf_Addr)(defobj->relocbase + def->st_value);
	}

	dbg("\"%s\" in \"%s\" ==> %p in \"%s\"", defobj->strtab + def->st_name,
	    obj->path == NULL ? NULL : basename(obj->path), (void *)target,
	    defobj->path == NULL ? NULL : basename(defobj->path));

	/*
	 * Write the new contents for the jmpslot. Note that depending on
	 * architecture, the value which we need to return back to the
	 * lazy binding trampoline may or may not be the target
	 * address. The value returned from reloc_jmpslot() is the value
	 * that the trampoline needs.
	 */
	target = reloc_jmpslot(where, target, defobj, obj, rel);
	lock_release(rtld_bind_lock, &lockstate);
	return (target);
}

/*
 * Error reporting function.  Use it like printf.  If formats the message
 * into a buffer, and sets things up so that the next call to dlerror()
 * will return the message.
 */
void
_rtld_error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	rtld_vsnprintf(lockinfo.dlerror_loc(), lockinfo.dlerror_loc_sz, fmt,
	    ap);
	va_end(ap);
	*lockinfo.dlerror_seen() = 0;
	dbg("rtld_error: %s", lockinfo.dlerror_loc());
	LD_UTRACE(UTRACE_RTLD_ERROR, NULL, NULL, 0, 0, lockinfo.dlerror_loc());
}

/*
 * Return a dynamically-allocated copy of the current error message, if any.
 */
static struct dlerror_save *
errmsg_save(void)
{
	struct dlerror_save *res;

	res = xmalloc(sizeof(*res));
	res->seen = *lockinfo.dlerror_seen();
	if (res->seen == 0)
		res->msg = xstrdup(lockinfo.dlerror_loc());
	return (res);
}

/*
 * Restore the current error message from a copy which was previously saved
 * by errmsg_save().  The copy is freed.
 */
static void
errmsg_restore(struct dlerror_save *saved_msg)
{
	if (saved_msg == NULL || saved_msg->seen == 1) {
		*lockinfo.dlerror_seen() = 1;
	} else {
		*lockinfo.dlerror_seen() = 0;
		strlcpy(lockinfo.dlerror_loc(), saved_msg->msg,
		    lockinfo.dlerror_loc_sz);
		free(saved_msg->msg);
	}
	free(saved_msg);
}

static const char *
basename(const char *name)
{
	const char *p;

	p = strrchr(name, '/');
	return (p != NULL ? p + 1 : name);
}

static struct utsname uts;

static char *
origin_subst_one(Obj_Entry *obj, char *real, const char *kw, const char *subst,
    bool may_free)
{
	char *p, *p1, *res, *resp;
	int subst_len, kw_len, subst_count, old_len, new_len;

	kw_len = strlen(kw);

	/*
	 * First, count the number of the keyword occurrences, to
	 * preallocate the final string.
	 */
	for (p = real, subst_count = 0;; p = p1 + kw_len, subst_count++) {
		p1 = strstr(p, kw);
		if (p1 == NULL)
			break;
	}

	/*
	 * If the keyword is not found, just return.
	 *
	 * Return non-substituted string if resolution failed.  We
	 * cannot do anything more reasonable, the failure mode of the
	 * caller is unresolved library anyway.
	 */
	if (subst_count == 0 || (obj != NULL && !obj_resolve_origin(obj)))
		return (may_free ? real : xstrdup(real));
	if (obj != NULL)
		subst = obj->origin_path;

	/*
	 * There is indeed something to substitute.  Calculate the
	 * length of the resulting string, and allocate it.
	 */
	subst_len = strlen(subst);
	old_len = strlen(real);
	new_len = old_len + (subst_len - kw_len) * subst_count;
	res = xmalloc(new_len + 1);

	/*
	 * Now, execute the substitution loop.
	 */
	for (p = real, resp = res, *resp = '\0';;) {
		p1 = strstr(p, kw);
		if (p1 != NULL) {
			/* Copy the prefix before keyword. */
			memcpy(resp, p, p1 - p);
			resp += p1 - p;
			/* Keyword replacement. */
			memcpy(resp, subst, subst_len);
			resp += subst_len;
			*resp = '\0';
			p = p1 + kw_len;
		} else
			break;
	}

	/* Copy to the end of string and finish. */
	strcat(resp, p);
	if (may_free)
		free(real);
	return (res);
}

static const struct {
	const char *kw;
	bool pass_obj;
	const char *subst;
} tokens[] = {
	{ .kw = "$ORIGIN", .pass_obj = true, .subst = NULL },
	{ .kw = "${ORIGIN}", .pass_obj = true, .subst = NULL },
	{ .kw = "$OSNAME", .pass_obj = false, .subst = uts.sysname },
	{ .kw = "${OSNAME}", .pass_obj = false, .subst = uts.sysname },
	{ .kw = "$OSREL", .pass_obj = false, .subst = uts.release },
	{ .kw = "${OSREL}", .pass_obj = false, .subst = uts.release },
	{ .kw = "$PLATFORM", .pass_obj = false, .subst = uts.machine },
	{ .kw = "${PLATFORM}", .pass_obj = false, .subst = uts.machine },
	{ .kw = "$LIB", .pass_obj = false, .subst = TOKEN_LIB },
	{ .kw = "${LIB}", .pass_obj = false, .subst = TOKEN_LIB },
};

static char *
origin_subst(Obj_Entry *obj, const char *real)
{
	char *res;
	int i;

	if (obj == NULL || !trust)
		return (xstrdup(real));
	if (uts.sysname[0] == '\0') {
		if (uname(&uts) != 0) {
			_rtld_error("utsname failed: %d", errno);
			return (NULL);
		}
	}

	/* __DECONST is safe here since without may_free real is unchanged */
	res = __DECONST(char *, real);
	for (i = 0; i < (int)nitems(tokens); i++) {
		res = origin_subst_one(tokens[i].pass_obj ? obj : NULL, res,
		    tokens[i].kw, tokens[i].subst, i != 0);
	}
	return (res);
}

void
rtld_die(void)
{
	const char *msg = dlerror();

	if (msg == NULL)
		msg = "Fatal error";
	rtld_fdputstr(STDERR_FILENO, _BASENAME_RTLD ": ");
	rtld_fdputstr(STDERR_FILENO, msg);
	rtld_fdputchar(STDERR_FILENO, '\n');
	_exit(1);
}

/*
 * Process a shared object's DYNAMIC section, and save the important
 * information in its Obj_Entry structure.
 */
static void
digest_dynamic1(Obj_Entry *obj, int early, const Elf_Dyn **dyn_rpath,
    const Elf_Dyn **dyn_soname, const Elf_Dyn **dyn_runpath)
{
	const Elf_Dyn *dynp;
	Needed_Entry **needed_tail = &obj->needed;
	Needed_Entry **needed_filtees_tail = &obj->needed_filtees;
	Needed_Entry **needed_aux_filtees_tail = &obj->needed_aux_filtees;
	const Elf_Hashelt *hashtab;
	const Elf32_Word *hashval;
	Elf32_Word bkt, nmaskwords;
	int bloom_size32;
	int plttype = DT_REL;

	*dyn_rpath = NULL;
	*dyn_soname = NULL;
	*dyn_runpath = NULL;

	obj->bind_now = false;
	dynp = obj->dynamic;
	if (dynp == NULL)
		return;
	for (; dynp->d_tag != DT_NULL; dynp++) {
		switch (dynp->d_tag) {
		case DT_REL:
			obj->rel = (const Elf_Rel *)(obj->relocbase +
			    dynp->d_un.d_ptr);
			break;

		case DT_RELSZ:
			obj->relsize = dynp->d_un.d_val;
			break;

		case DT_RELENT:
			assert(dynp->d_un.d_val == sizeof(Elf_Rel));
			break;

		case DT_JMPREL:
			obj->pltrel = (const Elf_Rel *)(obj->relocbase +
			    dynp->d_un.d_ptr);
			break;

		case DT_PLTRELSZ:
			obj->pltrelsize = dynp->d_un.d_val;
			break;

		case DT_RELA:
			obj->rela = (const Elf_Rela *)(obj->relocbase +
			    dynp->d_un.d_ptr);
			break;

		case DT_RELASZ:
			obj->relasize = dynp->d_un.d_val;
			break;

		case DT_RELAENT:
			assert(dynp->d_un.d_val == sizeof(Elf_Rela));
			break;

		case DT_RELR:
			obj->relr = (const Elf_Relr *)(obj->relocbase +
			    dynp->d_un.d_ptr);
			break;

		case DT_RELRSZ:
			obj->relrsize = dynp->d_un.d_val;
			break;

		case DT_RELRENT:
			assert(dynp->d_un.d_val == sizeof(Elf_Relr));
			break;

		case DT_PLTREL:
			plttype = dynp->d_un.d_val;
			assert(
			    dynp->d_un.d_val == DT_REL || plttype == DT_RELA);
			break;

		case DT_SYMTAB:
			obj->symtab = (const Elf_Sym *)(obj->relocbase +
			    dynp->d_un.d_ptr);
			break;

		case DT_SYMENT:
			assert(dynp->d_un.d_val == sizeof(Elf_Sym));
			break;

		case DT_STRTAB:
			obj->strtab = (const char *)(obj->relocbase +
			    dynp->d_un.d_ptr);
			break;

		case DT_STRSZ:
			obj->strsize = dynp->d_un.d_val;
			break;

		case DT_VERNEED:
			obj->verneed = (const Elf_Verneed *)(obj->relocbase +
			    dynp->d_un.d_val);
			break;

		case DT_VERNEEDNUM:
			obj->verneednum = dynp->d_un.d_val;
			break;

		case DT_VERDEF:
			obj->verdef = (const Elf_Verdef *)(obj->relocbase +
			    dynp->d_un.d_val);
			break;

		case DT_VERDEFNUM:
			obj->verdefnum = dynp->d_un.d_val;
			break;

		case DT_VERSYM:
			obj->versyms = (const Elf_Versym *)(obj->relocbase +
			    dynp->d_un.d_val);
			break;

		case DT_HASH: {
			hashtab = (const Elf_Hashelt *)(obj->relocbase +
			    dynp->d_un.d_ptr);
			obj->nbuckets = hashtab[0];
			obj->nchains = hashtab[1];
			obj->buckets = hashtab + 2;
			obj->chains = obj->buckets + obj->nbuckets;
			obj->valid_hash_sysv = obj->nbuckets > 0 &&
			    obj->nchains > 0 && obj->buckets != NULL;
		} break;

		case DT_GNU_HASH: {
			hashtab = (const Elf_Hashelt *)(obj->relocbase +
			    dynp->d_un.d_ptr);
			obj->nbuckets_gnu = hashtab[0];
			obj->symndx_gnu = hashtab[1];
			nmaskwords = hashtab[2];
			bloom_size32 = (__ELF_WORD_SIZE / 32) * nmaskwords;
			obj->maskwords_bm_gnu = nmaskwords - 1;
			obj->shift2_gnu = hashtab[3];
			obj->bloom_gnu = (const Elf_Addr *)(hashtab + 4);
			obj->buckets_gnu = hashtab + 4 + bloom_size32;
			obj->chain_zero_gnu = obj->buckets_gnu +
			    obj->nbuckets_gnu - obj->symndx_gnu;
			/* Number of bitmask words is required to be power of 2
			 */
			obj->valid_hash_gnu = powerof2(nmaskwords) &&
			    obj->nbuckets_gnu > 0 && obj->buckets_gnu != NULL;
		} break;

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

				if (obj->linkmap.l_refname == NULL)
					obj->linkmap.l_refname =
					    (char *)dynp->d_un.d_val;
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
			obj->pltgot = (Elf_Addr *)(obj->relocbase +
			    dynp->d_un.d_ptr);
			break;

		case DT_TEXTREL:
			obj->textrel = true;
			break;

		case DT_SYMBOLIC:
			obj->symbolic = true;
			break;

		case DT_RPATH:
			/*
			 * We have to wait until later to process this, because
			 * we might not have gotten the address of the string
			 * table yet.
			 */
			*dyn_rpath = dynp;
			break;

		case DT_SONAME:
			*dyn_soname = dynp;
			break;

		case DT_RUNPATH:
			*dyn_runpath = dynp;
			break;

		case DT_INIT:
			obj->init = (Elf_Addr)(obj->relocbase +
			    dynp->d_un.d_ptr);
			break;

		case DT_PREINIT_ARRAY:
			obj->preinit_array = (Elf_Addr)(obj->relocbase +
			    dynp->d_un.d_ptr);
			break;

		case DT_PREINIT_ARRAYSZ:
			obj->preinit_array_num = dynp->d_un.d_val /
			    sizeof(Elf_Addr);
			break;

		case DT_INIT_ARRAY:
			obj->init_array = (Elf_Addr)(obj->relocbase +
			    dynp->d_un.d_ptr);
			break;

		case DT_INIT_ARRAYSZ:
			obj->init_array_num = dynp->d_un.d_val /
			    sizeof(Elf_Addr);
			break;

		case DT_FINI:
			obj->fini = (Elf_Addr)(obj->relocbase +
			    dynp->d_un.d_ptr);
			break;

		case DT_FINI_ARRAY:
			obj->fini_array = (Elf_Addr)(obj->relocbase +
			    dynp->d_un.d_ptr);
			break;

		case DT_FINI_ARRAYSZ:
			obj->fini_array_num = dynp->d_un.d_val /
			    sizeof(Elf_Addr);
			break;

		case DT_DEBUG:
			if (!early)
				dbg("Filling in DT_DEBUG entry");
			(__DECONST(Elf_Dyn *, dynp))->d_un.d_ptr =
			    (Elf_Addr)&r_debug;
			break;

		case DT_FLAGS:
			if (dynp->d_un.d_val & DF_ORIGIN)
				obj->z_origin = true;
			if (dynp->d_un.d_val & DF_SYMBOLIC)
				obj->symbolic = true;
			if (dynp->d_un.d_val & DF_TEXTREL)
				obj->textrel = true;
			if (dynp->d_un.d_val & DF_BIND_NOW)
				obj->bind_now = true;
			if (dynp->d_un.d_val & DF_STATIC_TLS)
				obj->static_tls = true;
			break;

		case DT_FLAGS_1:
			if (dynp->d_un.d_val & DF_1_NOOPEN)
				obj->z_noopen = true;
			if (dynp->d_un.d_val & DF_1_ORIGIN)
				obj->z_origin = true;
			if (dynp->d_un.d_val & DF_1_GLOBAL)
				obj->z_global = true;
			if (dynp->d_un.d_val & DF_1_BIND_NOW)
				obj->bind_now = true;
			if (dynp->d_un.d_val & DF_1_NODELETE)
				obj->z_nodelete = true;
			if (dynp->d_un.d_val & DF_1_LOADFLTR)
				obj->z_loadfltr = true;
			if (dynp->d_un.d_val & DF_1_INTERPOSE)
				obj->z_interpose = true;
			if (dynp->d_un.d_val & DF_1_NODEFLIB)
				obj->z_nodeflib = true;
			if (dynp->d_un.d_val & DF_1_PIE)
				obj->z_pie = true;
			if (dynp->d_un.d_val & DF_1_INITFIRST)
				obj->z_initfirst = true;
			break;

		default:
			if (arch_digest_dynamic(obj, dynp))
				break;

			if (!early) {
				dbg("Ignoring d_tag %ld = %#lx",
				    (long)dynp->d_tag, (long)dynp->d_tag);
			}
			break;
		}
	}

	obj->traced = false;

	if (plttype == DT_RELA) {
		obj->pltrela = (const Elf_Rela *)obj->pltrel;
		obj->pltrel = NULL;
		obj->pltrelasize = obj->pltrelsize;
		obj->pltrelsize = 0;
	}

	/* Determine size of dynsym table (equal to nchains of sysv hash) */
	if (obj->valid_hash_sysv)
		obj->dynsymcount = obj->nchains;
	else if (obj->valid_hash_gnu) {
		obj->dynsymcount = 0;
		for (bkt = 0; bkt < obj->nbuckets_gnu; bkt++) {
			if (obj->buckets_gnu[bkt] == 0)
				continue;
			hashval = &obj->chain_zero_gnu[obj->buckets_gnu[bkt]];
			do
				obj->dynsymcount++;
			while ((*hashval++ & 1u) == 0);
		}
		obj->dynsymcount += obj->symndx_gnu;
	}

	if (obj->linkmap.l_refname != NULL)
		obj->linkmap.l_refname = obj->strtab +
		    (unsigned long)obj->linkmap.l_refname;
}

static bool
obj_resolve_origin(Obj_Entry *obj)
{
	if (obj->origin_path != NULL)
		return (true);
	obj->origin_path = xmalloc(PATH_MAX);
	return (rtld_dirname_abs(obj->path, obj->origin_path) != -1);
}

static bool
digest_dynamic2(Obj_Entry *obj, const Elf_Dyn *dyn_rpath,
    const Elf_Dyn *dyn_soname, const Elf_Dyn *dyn_runpath)
{
	if (obj->z_origin && !obj_resolve_origin(obj))
		return (false);

	if (dyn_runpath != NULL) {
		obj->runpath = (const char *)obj->strtab +
		    dyn_runpath->d_un.d_val;
		obj->runpath = origin_subst(obj, obj->runpath);
	} else if (dyn_rpath != NULL) {
		obj->rpath = (const char *)obj->strtab + dyn_rpath->d_un.d_val;
		obj->rpath = origin_subst(obj, obj->rpath);
	}
	if (dyn_soname != NULL)
		object_add_name(obj, obj->strtab + dyn_soname->d_un.d_val);
	return (true);
}

static bool
digest_dynamic(Obj_Entry *obj, int early)
{
	const Elf_Dyn *dyn_rpath;
	const Elf_Dyn *dyn_soname;
	const Elf_Dyn *dyn_runpath;

	digest_dynamic1(obj, early, &dyn_rpath, &dyn_soname, &dyn_runpath);
	return (digest_dynamic2(obj, dyn_rpath, dyn_soname, dyn_runpath));
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
	Elf_Addr note_start, note_end;
	int nsegs = 0;

	obj = obj_new();
	for (ph = phdr; ph < phlimit; ph++) {
		if (ph->p_type != PT_PHDR)
			continue;

		obj->phdr = phdr;
		obj->phsize = ph->p_memsz;
		obj->relocbase = __DECONST(char *, phdr) - ph->p_vaddr;
		break;
	}

	obj->stack_flags = PF_X | PF_R | PF_W;

	for (ph = phdr; ph < phlimit; ph++) {
		switch (ph->p_type) {
		case PT_INTERP:
			obj->interp = (const char *)(ph->p_vaddr +
			    obj->relocbase);
			break;

		case PT_LOAD:
			if (nsegs == 0) { /* First load segment */
				obj->vaddrbase = rtld_trunc_page(ph->p_vaddr);
				obj->mapbase = obj->vaddrbase + obj->relocbase;
			} else { /* Last load segment */
				obj->mapsize = rtld_round_page(
				    ph->p_vaddr + ph->p_memsz) -
				    obj->vaddrbase;
			}
			nsegs++;
			break;

		case PT_DYNAMIC:
			obj->dynamic = (const Elf_Dyn *)(ph->p_vaddr +
			    obj->relocbase);
			break;

		case PT_TLS:
			obj->tlsindex = 1;
			obj->tlssize = ph->p_memsz;
			obj->tlsalign = ph->p_align;
			obj->tlsinitsize = ph->p_filesz;
			obj->tlsinit = (void *)(ph->p_vaddr + obj->relocbase);
			obj->tlspoffset = ph->p_offset;
			break;

		case PT_GNU_STACK:
			obj->stack_flags = ph->p_flags;
			break;

		case PT_NOTE:
			note_start = (Elf_Addr)obj->relocbase + ph->p_vaddr;
			note_end = note_start + ph->p_filesz;
			digest_notes(obj, note_start, note_end);
			break;
		}
	}
	if (nsegs < 1) {
		_rtld_error("%s: too few PT_LOAD segments", path);
		return (NULL);
	}

	obj->entry = entry;
	return (obj);
}

void
digest_notes(Obj_Entry *obj, Elf_Addr note_start, Elf_Addr note_end)
{
	const Elf_Note *note;
	const char *note_name;
	uintptr_t p;

	for (note = (const Elf_Note *)note_start; (Elf_Addr)note < note_end;
	    note = (const Elf_Note *)((const char *)(note + 1) +
		roundup2(note->n_namesz, sizeof(Elf32_Addr)) +
		roundup2(note->n_descsz, sizeof(Elf32_Addr)))) {
		if (arch_digest_note(obj, note))
			continue;

		if (note->n_namesz != sizeof(NOTE_FREEBSD_VENDOR) ||
		    note->n_descsz != sizeof(int32_t))
			continue;
		if (note->n_type != NT_FREEBSD_ABI_TAG &&
		    note->n_type != NT_FREEBSD_FEATURE_CTL &&
		    note->n_type != NT_FREEBSD_NOINIT_TAG)
			continue;
		note_name = (const char *)(note + 1);
		if (strncmp(NOTE_FREEBSD_VENDOR, note_name,
			sizeof(NOTE_FREEBSD_VENDOR)) != 0)
			continue;
		switch (note->n_type) {
		case NT_FREEBSD_ABI_TAG:
			/* FreeBSD osrel note */
			p = (uintptr_t)(note + 1);
			p += roundup2(note->n_namesz, sizeof(Elf32_Addr));
			obj->osrel = *(const int32_t *)(p);
			dbg("note osrel %d", obj->osrel);
			break;
		case NT_FREEBSD_FEATURE_CTL:
			/* FreeBSD ABI feature control note */
			p = (uintptr_t)(note + 1);
			p += roundup2(note->n_namesz, sizeof(Elf32_Addr));
			obj->fctl0 = *(const uint32_t *)(p);
			dbg("note fctl0 %#x", obj->fctl0);
			break;
		case NT_FREEBSD_NOINIT_TAG:
			/* FreeBSD 'crt does not call init' note */
			obj->crt_no_init = true;
			dbg("note crt_no_init");
			break;
		}
	}
}

static Obj_Entry *
dlcheck(void *handle)
{
	Obj_Entry *obj;

	TAILQ_FOREACH(obj, &obj_list, next) {
		if (obj == (Obj_Entry *)handle)
			break;
	}

	if (obj == NULL || obj->refcount == 0 || obj->dl_refcount == 0) {
		_rtld_error("Invalid shared object handle %p", handle);
		return (NULL);
	}
	return (obj);
}

/*
 * If the given object is already in the donelist, return true.  Otherwise
 * add the object to the list and return false.
 */
static bool
donelist_check(DoneList *dlp, const Obj_Entry *obj)
{
	unsigned int i;

	for (i = 0; i < dlp->num_used; i++)
		if (dlp->objs[i] == obj)
			return (true);
	/*
	 * Our donelist allocation should always be sufficient.  But if
	 * our threads locking isn't working properly, more shared objects
	 * could have been loaded since we allocated the list.  That should
	 * never happen, but we'll handle it properly just in case it does.
	 */
	if (dlp->num_used < dlp->num_alloc)
		dlp->objs[dlp->num_used++] = obj;
	return (false);
}

/*
 * SysV hash function for symbol table lookup.  It is a slightly optimized
 * version of the hash specified by the System V ABI.
 */
Elf32_Word
elf_hash(const char *name)
{
	const unsigned char *p = (const unsigned char *)name;
	Elf32_Word h = 0;

	while (*p != '\0') {
		h = (h << 4) + *p++;
		h ^= (h >> 24) & 0xf0;
	}
	return (h & 0x0fffffff);
}

/*
 * The GNU hash function is the Daniel J. Bernstein hash clipped to 32 bits
 * unsigned in case it's implemented with a wider type.
 */
static uint32_t
gnu_hash(const char *s)
{
	uint32_t h;
	unsigned char c;

	h = 5381;
	for (c = *s; c != '\0'; c = *++s)
		h = h * 33 + c;
	return (h & 0xffffffff);
}

/*
 * Find the library with the given name, and return its full pathname.
 * The returned string is dynamically allocated.  Generates an error
 * message and returns NULL if the library cannot be found.
 *
 * If the second argument is non-NULL, then it refers to an already-
 * loaded shared object, whose library search path will be searched.
 *
 * If a library is successfully located via LD_LIBRARY_PATH_FDS, its
 * descriptor (which is close-on-exec) will be passed out via the third
 * argument.
 *
 * The search order is:
 *   DT_RPATH in the referencing file _unless_ DT_RUNPATH is present (1)
 *   DT_RPATH of the main object if DSO without defined DT_RUNPATH (1)
 *   LD_LIBRARY_PATH
 *   DT_RUNPATH in the referencing file
 *   ldconfig hints (if -z nodefaultlib, filter out default library directories
 *	 from list)
 *   /lib:/usr/lib _unless_ the referencing file is linked with -z nodefaultlib
 *
 * (1) Handled in digest_dynamic2 - rpath left NULL if runpath defined.
 */
static char *
find_library(const char *xname, const Obj_Entry *refobj, int *fdp)
{
	char *pathname, *refobj_path;
	const char *name;
	bool nodeflib, objgiven;

	objgiven = refobj != NULL;

	if (libmap_disable || !objgiven ||
	    (name = lm_find(refobj->path, xname)) == NULL)
		name = xname;

	if (strchr(name, '/') != NULL) { /* Hard coded pathname */
		if (name[0] != '/' && !trust) {
			_rtld_error(
		    "Absolute pathname required for shared object \"%s\"",
			    name);
			return (NULL);
		}
		return (origin_subst(__DECONST(Obj_Entry *, refobj),
		    __DECONST(char *, name)));
	}

	dbg(" Searching for \"%s\"", name);
	refobj_path = objgiven ? refobj->path : NULL;

	/*
	 * If refobj->rpath != NULL, then refobj->runpath is NULL.  Fall
	 * back to pre-conforming behaviour if user requested so with
	 * LD_LIBRARY_PATH_RPATH environment variable and ignore -z
	 * nodeflib.
	 */
	if (objgiven && refobj->rpath != NULL && ld_library_path_rpath) {
		pathname = search_library_path(name, ld_library_path,
		    refobj_path, fdp);
		if (pathname != NULL)
			return (pathname);
		if (refobj != NULL) {
			pathname = search_library_path(name, refobj->rpath,
			    refobj_path, fdp);
			if (pathname != NULL)
				return (pathname);
		}
		pathname = search_library_pathfds(name, ld_library_dirs, fdp);
		if (pathname != NULL)
			return (pathname);
		pathname = search_library_path(name, gethints(false),
		    refobj_path, fdp);
		if (pathname != NULL)
			return (pathname);
		pathname = search_library_path(name, ld_standard_library_path,
		    refobj_path, fdp);
		if (pathname != NULL)
			return (pathname);
	} else {
		nodeflib = objgiven ? refobj->z_nodeflib : false;
		if (objgiven) {
			pathname = search_library_path(name, refobj->rpath,
			    refobj->path, fdp);
			if (pathname != NULL)
				return (pathname);
		}
		if (objgiven && refobj->runpath == NULL && refobj != obj_main) {
			pathname = search_library_path(name, obj_main->rpath,
			    refobj_path, fdp);
			if (pathname != NULL)
				return (pathname);
		}
		pathname = search_library_path(name, ld_library_path,
		    refobj_path, fdp);
		if (pathname != NULL)
			return (pathname);
		if (objgiven) {
			pathname = search_library_path(name, refobj->runpath,
			    refobj_path, fdp);
			if (pathname != NULL)
				return (pathname);
		}
		pathname = search_library_pathfds(name, ld_library_dirs, fdp);
		if (pathname != NULL)
			return (pathname);
		pathname = search_library_path(name, gethints(nodeflib),
		    refobj_path, fdp);
		if (pathname != NULL)
			return (pathname);
		if (objgiven && !nodeflib) {
			pathname = search_library_path(name,
			    ld_standard_library_path, refobj_path, fdp);
			if (pathname != NULL)
				return (pathname);
		}
	}

	if (objgiven && refobj->path != NULL) {
		_rtld_error(
	    "Shared object \"%s\" not found, required by \"%s\"",
		    name, basename(refobj->path));
	} else {
		_rtld_error("Shared object \"%s\" not found", name);
	}
	return (NULL);
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
	const Ver_Entry *ve;
	SymLook req;
	const char *name;
	int res;

	/*
	 * If we have already found this symbol, get the information from
	 * the cache.
	 */
	if (symnum >= refobj->dynsymcount)
		return (NULL); /* Bad object */
	if (cache != NULL && cache[symnum].sym != NULL) {
		*defobj_out = cache[symnum].obj;
		return (cache[symnum].sym);
	}

	ref = refobj->symtab + symnum;
	name = refobj->strtab + ref->st_name;
	def = NULL;
	defobj = NULL;
	ve = NULL;

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
			_rtld_error("%s: Bogus symbol table entry %lu",
			    refobj->path, symnum);
		}
		symlook_init(&req, name);
		req.flags = flags;
		ve = req.ventry = fetch_ventry(refobj, symnum);
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
		/*
		 * Record the information in the cache to avoid subsequent
		 * lookups.
		 */
		if (cache != NULL) {
			cache[symnum].sym = def;
			cache[symnum].obj = defobj;
		}
	} else {
		if (refobj != &obj_rtld)
			_rtld_error("%s: Undefined symbol \"%s%s%s\"",
			    refobj->path, name, ve != NULL ? "@" : "",
			    ve != NULL ? ve->name : "");
	}
	return (def);
}

/* Convert between native byte order and forced little resp. big endian. */
#define COND_SWAP(n) (is_le ? le32toh(n) : be32toh(n))

/*
 * Return the search path from the ldconfig hints file, reading it if
 * necessary.  If nostdlib is true, then the default search paths are
 * not added to result.
 *
 * Returns NULL if there are problems with the hints file,
 * or if the search path there is empty.
 */
static const char *
gethints(bool nostdlib)
{
	static char *filtered_path;
	static const char *hints;
	static struct elfhints_hdr hdr;
	struct fill_search_info_args sargs, hargs;
	struct dl_serinfo smeta, hmeta, *SLPinfo, *hintinfo;
	struct dl_serpath *SLPpath, *hintpath;
	char *p;
	struct stat hint_stat;
	unsigned int SLPndx, hintndx, fndx, fcount;
	int fd;
	size_t flen;
	uint32_t dl;
	uint32_t magic;	     /* Magic number */
	uint32_t version;    /* File version (1) */
	uint32_t strtab;     /* Offset of string table in file */
	uint32_t dirlist;    /* Offset of directory list in string table */
	uint32_t dirlistlen; /* strlen(dirlist) */
	bool is_le;	     /* Does the hints file use little endian */
	bool skip;

	/* First call, read the hints file */
	if (hints == NULL) {
		/* Keep from trying again in case the hints file is bad. */
		hints = "";

		if ((fd = open(ld_elf_hints_path, O_RDONLY | O_CLOEXEC)) ==
		    -1) {
			dbg("failed to open hints file \"%s\"",
			    ld_elf_hints_path);
			return (NULL);
		}

		/*
		 * Check of hdr.dirlistlen value against type limit
		 * intends to pacify static analyzers.  Further
		 * paranoia leads to checks that dirlist is fully
		 * contained in the file range.
		 */
		if (read(fd, &hdr, sizeof hdr) != sizeof hdr) {
			dbg("failed to read %lu bytes from hints file \"%s\"",
			    (u_long)sizeof hdr, ld_elf_hints_path);
cleanup1:
			close(fd);
			hdr.dirlistlen = 0;
			return (NULL);
		}
		dbg("host byte-order: %s-endian",
		    le32toh(1) == 1 ? "little" : "big");
		dbg("hints file byte-order: %s-endian",
		    hdr.magic == htole32(ELFHINTS_MAGIC) ? "little" : "big");
		is_le = /*htole32(1) == 1 || */ hdr.magic ==
		    htole32(ELFHINTS_MAGIC);
		magic = COND_SWAP(hdr.magic);
		version = COND_SWAP(hdr.version);
		strtab = COND_SWAP(hdr.strtab);
		dirlist = COND_SWAP(hdr.dirlist);
		dirlistlen = COND_SWAP(hdr.dirlistlen);
		if (magic != ELFHINTS_MAGIC) {
			dbg("invalid magic number %#08x (expected: %#08x)",
			    magic, ELFHINTS_MAGIC);
			goto cleanup1;
		}
		if (version != 1) {
			dbg("hints file version %d (expected: 1)", version);
			goto cleanup1;
		}
		if (dirlistlen > UINT_MAX / 2) {
			dbg("directory list is to long: %d > %d", dirlistlen,
			    UINT_MAX / 2);
			goto cleanup1;
		}
		if (fstat(fd, &hint_stat) == -1) {
			dbg("failed to find length of hints file \"%s\"",
			    ld_elf_hints_path);
			goto cleanup1;
		}
		dl = strtab;
		if (dl + dirlist < dl) {
			dbg("invalid string table position %d", dl);
			goto cleanup1;
		}
		dl += dirlist;
		if (dl + dirlistlen < dl) {
			dbg("invalid directory list offset %d", dirlist);
			goto cleanup1;
		}
		dl += dirlistlen;
		if (dl > hint_stat.st_size) {
			dbg("hints file \"%s\" is truncated (%d vs. %jd bytes)",
			    ld_elf_hints_path, dl,
			    (uintmax_t)hint_stat.st_size);
			goto cleanup1;
		}
		p = xmalloc(dirlistlen + 1);
		if (pread(fd, p, dirlistlen + 1, strtab + dirlist) !=
		    (ssize_t)dirlistlen + 1 || p[dirlistlen] != '\0') {
			free(p);
			dbg(
	    "failed to read %d bytes starting at %d from hints file \"%s\"",
			    dirlistlen + 1, strtab + dirlist,
			    ld_elf_hints_path);
			goto cleanup1;
		}
		hints = p;
		close(fd);
	}

	/*
	 * If caller agreed to receive list which includes the default
	 * paths, we are done. Otherwise, if we still did not
	 * calculated filtered result, do it now.
	 */
	if (!nostdlib)
		return (hints[0] != '\0' ? hints : NULL);
	if (filtered_path != NULL)
		goto filt_ret;

	/*
	 * Obtain the list of all configured search paths, and the
	 * list of the default paths.
	 *
	 * First estimate the size of the results.
	 */
	smeta.dls_size = __offsetof(struct dl_serinfo, dls_serpath);
	smeta.dls_cnt = 0;
	hmeta.dls_size = __offsetof(struct dl_serinfo, dls_serpath);
	hmeta.dls_cnt = 0;

	sargs.request = RTLD_DI_SERINFOSIZE;
	sargs.serinfo = &smeta;
	hargs.request = RTLD_DI_SERINFOSIZE;
	hargs.serinfo = &hmeta;

	path_enumerate(ld_standard_library_path, fill_search_info, NULL,
	    &sargs);
	path_enumerate(hints, fill_search_info, NULL, &hargs);

	SLPinfo = xmalloc(smeta.dls_size);
	hintinfo = xmalloc(hmeta.dls_size);

	/*
	 * Next fetch both sets of paths.
	 */
	sargs.request = RTLD_DI_SERINFO;
	sargs.serinfo = SLPinfo;
	sargs.serpath = &SLPinfo->dls_serpath[0];
	sargs.strspace = (char *)&SLPinfo->dls_serpath[smeta.dls_cnt];

	hargs.request = RTLD_DI_SERINFO;
	hargs.serinfo = hintinfo;
	hargs.serpath = &hintinfo->dls_serpath[0];
	hargs.strspace = (char *)&hintinfo->dls_serpath[hmeta.dls_cnt];

	path_enumerate(ld_standard_library_path, fill_search_info, NULL,
	    &sargs);
	path_enumerate(hints, fill_search_info, NULL, &hargs);

	/*
	 * Now calculate the difference between two sets, by excluding
	 * standard paths from the full set.
	 */
	fndx = 0;
	fcount = 0;
	filtered_path = xmalloc(dirlistlen + 1);
	hintpath = &hintinfo->dls_serpath[0];
	for (hintndx = 0; hintndx < hmeta.dls_cnt; hintndx++, hintpath++) {
		skip = false;
		SLPpath = &SLPinfo->dls_serpath[0];
		/*
		 * Check each standard path against current.
		 */
		for (SLPndx = 0; SLPndx < smeta.dls_cnt; SLPndx++, SLPpath++) {
			/* matched, skip the path */
			if (!strcmp(hintpath->dls_name, SLPpath->dls_name)) {
				skip = true;
				break;
			}
		}
		if (skip)
			continue;
		/*
		 * Not matched against any standard path, add the path
		 * to result. Separate consequtive paths with ':'.
		 */
		if (fcount > 0) {
			filtered_path[fndx] = ':';
			fndx++;
		}
		fcount++;
		flen = strlen(hintpath->dls_name);
		strncpy((filtered_path + fndx), hintpath->dls_name, flen);
		fndx += flen;
	}
	filtered_path[fndx] = '\0';

	free(SLPinfo);
	free(hintinfo);

filt_ret:
	return (filtered_path[0] != '\0' ? filtered_path : NULL);
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
		for (needed = elm->obj->needed; needed != NULL;
		    needed = needed->next) {
			if (needed->obj == NULL ||
			    donelist_check(&donelist, needed->obj))
				continue;
			objlist_push_tail(&needed->obj->dldags, root);
			objlist_push_tail(&root->dagmembers, needed->obj);
		}
	}
	root->dag_inited = true;
}

static void
init_marker(Obj_Entry *marker)
{
	bzero(marker, sizeof(*marker));
	marker->marker = true;
}

Obj_Entry *
globallist_curr(const Obj_Entry *obj)
{
	for (;;) {
		if (obj == NULL)
			return (NULL);
		if (!obj->marker)
			return (__DECONST(Obj_Entry *, obj));
		obj = TAILQ_PREV(obj, obj_entry_q, next);
	}
}

Obj_Entry *
globallist_next(const Obj_Entry *obj)
{
	for (;;) {
		obj = TAILQ_NEXT(obj, next);
		if (obj == NULL)
			return (NULL);
		if (!obj->marker)
			return (__DECONST(Obj_Entry *, obj));
	}
}

/* Prevent the object from being unmapped while the bind lock is dropped. */
static void
hold_object(Obj_Entry *obj)
{
	obj->holdcount++;
}

static void
unhold_object(Obj_Entry *obj)
{
	assert(obj->holdcount > 0);
	if (--obj->holdcount == 0 && obj->unholdfree)
		release_object(obj);
}

static void
process_z(Obj_Entry *root)
{
	const Objlist_Entry *elm;
	Obj_Entry *obj;

	/*
	 * Walk over object DAG and process every dependent object
	 * that is marked as DF_1_NODELETE or DF_1_GLOBAL. They need
	 * to grow their own DAG.
	 *
	 * For DF_1_GLOBAL, DAG is required for symbol lookups in
	 * symlook_global() to work.
	 *
	 * For DF_1_NODELETE, the DAG should have its reference upped.
	 */
	STAILQ_FOREACH(elm, &root->dagmembers, link) {
		obj = elm->obj;
		if (obj == NULL)
			continue;
		if (obj->z_nodelete && !obj->ref_nodel) {
			dbg("obj %s -z nodelete", obj->path);
			init_dag(obj);
			ref_dag(obj);
			obj->ref_nodel = true;
		}
		if (obj->z_global && objlist_find(&list_global, obj) == NULL) {
			dbg("obj %s -z global", obj->path);
			objlist_push_tail(&list_global, obj);
			init_dag(obj);
		}
	}
}

static void
parse_rtld_phdr(Obj_Entry *obj)
{
	const Elf_Phdr *ph;
	Elf_Addr note_start, note_end;

	obj->stack_flags = PF_X | PF_R | PF_W;
	for (ph = obj->phdr;
	    (const char *)ph < (const char *)obj->phdr + obj->phsize; ph++) {
		switch (ph->p_type) {
		case PT_GNU_STACK:
			obj->stack_flags = ph->p_flags;
			break;
		case PT_NOTE:
			note_start = (Elf_Addr)obj->relocbase + ph->p_vaddr;
			note_end = note_start + ph->p_filesz;
			digest_notes(obj, note_start, note_end);
			break;
		}
	}
}

/*
 * Initialize the dynamic linker.  The argument is the address at which
 * the dynamic linker has been mapped into memory.  The primary task of
 * this function is to relocate the dynamic linker.
 */
static void
init_rtld(caddr_t mapbase, Elf_Auxinfo **aux_info)
{
	Obj_Entry objtmp; /* Temporary rtld object */
	const Elf_Ehdr *ehdr;
	const Elf_Dyn *dyn_rpath;
	const Elf_Dyn *dyn_soname;
	const Elf_Dyn *dyn_runpath;

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

	objtmp.dynamic = rtld_dynamic(&objtmp);
	digest_dynamic1(&objtmp, 1, &dyn_rpath, &dyn_soname, &dyn_runpath);
	assert(objtmp.needed == NULL);
	assert(!objtmp.textrel);
	/*
	 * Temporarily put the dynamic linker entry into the object list, so
	 * that symbols can be found.
	 */
	relocate_objects(&objtmp, true, &objtmp, 0, NULL);

	ehdr = (Elf_Ehdr *)mapbase;
	objtmp.phdr = (Elf_Phdr *)((char *)mapbase + ehdr->e_phoff);
	objtmp.phsize = ehdr->e_phnum * sizeof(objtmp.phdr[0]);

	/* Initialize the object list. */
	TAILQ_INIT(&obj_list);

	/* Now that non-local variables can be accesses, copy out obj_rtld. */
	memcpy(&obj_rtld, &objtmp, sizeof(obj_rtld));

	/* The page size is required by the dynamic memory allocator. */
	init_pagesizes(aux_info);

	if (aux_info[AT_OSRELDATE] != NULL)
		osreldate = aux_info[AT_OSRELDATE]->a_un.a_val;

	digest_dynamic2(&obj_rtld, dyn_rpath, dyn_soname, dyn_runpath);

	/* Replace the path with a dynamically allocated copy. */
	obj_rtld.path = xstrdup(ld_path_rtld);

	parse_rtld_phdr(&obj_rtld);
	if (obj_enforce_relro(&obj_rtld) == -1)
		rtld_die();

	r_debug.r_version = R_DEBUG_VERSION;
	r_debug.r_brk = r_debug_state;
	r_debug.r_state = RT_CONSISTENT;
	r_debug.r_ldbase = obj_rtld.relocbase;
}

/*
 * Retrieve the array of supported page sizes.  The kernel provides the page
 * sizes in increasing order.
 */
static void
init_pagesizes(Elf_Auxinfo **aux_info)
{
	static size_t psa[MAXPAGESIZES];
	int mib[2];
	size_t len, size;

	if (aux_info[AT_PAGESIZES] != NULL &&
	    aux_info[AT_PAGESIZESLEN] != NULL) {
		size = aux_info[AT_PAGESIZESLEN]->a_un.a_val;
		pagesizes = aux_info[AT_PAGESIZES]->a_un.a_ptr;
	} else {
		len = 2;
		if (sysctlnametomib("hw.pagesizes", mib, &len) == 0)
			size = sizeof(psa);
		else {
			/* As a fallback, retrieve the base page size. */
			size = sizeof(psa[0]);
			if (aux_info[AT_PAGESZ] != NULL) {
				psa[0] = aux_info[AT_PAGESZ]->a_un.a_val;
				goto psa_filled;
			} else {
				mib[0] = CTL_HW;
				mib[1] = HW_PAGESIZE;
				len = 2;
			}
		}
		if (sysctl(mib, len, psa, &size, NULL, 0) == -1) {
			_rtld_error("sysctl for hw.pagesize(s) failed");
			rtld_die();
		}
	psa_filled:
		pagesizes = psa;
	}
	npagesizes = size / sizeof(pagesizes[0]);
	/* Discard any invalid entries at the end of the array. */
	while (npagesizes > 0 && pagesizes[npagesizes - 1] == 0)
		npagesizes--;

	page_size = pagesizes[0];
}

/*
 * Add the init functions from a needed object list (and its recursive
 * needed objects) to "list".  This is not used directly; it is a helper
 * function for initlist_add_objects().  The write lock must be held
 * when this function is called.
 */
static void
initlist_add_neededs(Needed_Entry *needed, Objlist *list, Objlist *iflist)
{
	/* Recursively process the successor needed objects. */
	if (needed->next != NULL)
		initlist_add_neededs(needed->next, list, iflist);

	/* Process the current needed object. */
	if (needed->obj != NULL)
		initlist_add_objects(needed->obj, needed->obj, list, iflist);
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
initlist_for_loaded_obj(Obj_Entry *obj, Obj_Entry *tail, Objlist *list)
{
	Objlist iflist;		/* initfirst objs and their needed */
	Objlist_Entry *tmp;

	objlist_init(&iflist);
	initlist_add_objects(obj, tail, list, &iflist);

	STAILQ_FOREACH(tmp, &iflist, link) {
		Obj_Entry *tobj = tmp->obj;

		if ((tobj->fini != (Elf_Addr)NULL ||
		    tobj->fini_array != (Elf_Addr)NULL) &&
		    !tobj->on_fini_list) {
			objlist_push_tail(&list_fini, tobj);
			tobj->on_fini_list = true;
		}
	}

	/*
	 * This might result in the same object appearing more
	 * than once on the init list.  objlist_call_init()
	 * uses obj->init_scanned to avoid dup calls.
	 */
	STAILQ_REVERSE(&iflist, Struct_Objlist_Entry, link);
	STAILQ_FOREACH(tmp, &iflist, link)
		objlist_push_head(list, tmp->obj);

	objlist_clear(&iflist);
}

static void
initlist_add_objects(Obj_Entry *obj, Obj_Entry *tail, Objlist *list,
    Objlist *iflist)
{
	Obj_Entry *nobj;

	if (obj->init_done)
		return;

	if (obj->z_initfirst || list == NULL) {
		/*
		 * Ignore obj->init_scanned.  The object might indeed
		 * already be on the init list, but due to being
		 * needed by an initfirst object, we must put it at
		 * the head of the init list.  obj->init_done protects
		 * against double-initialization.
		 */
		if (obj->needed != NULL)
			initlist_add_neededs(obj->needed, NULL, iflist);
		if (obj->needed_filtees != NULL)
			initlist_add_neededs(obj->needed_filtees, NULL,
			    iflist);
		if (obj->needed_aux_filtees != NULL)
			initlist_add_neededs(obj->needed_aux_filtees,
			    NULL, iflist);
		objlist_push_tail(iflist, obj);
	} else {
		if (obj->init_scanned)
			return;
		obj->init_scanned = true;

		/* Recursively process the successor objects. */
		nobj = globallist_next(obj);
		if (nobj != NULL && obj != tail)
			initlist_add_objects(nobj, tail, list, iflist);

		/* Recursively process the needed objects. */
		if (obj->needed != NULL)
			initlist_add_neededs(obj->needed, list, iflist);
		if (obj->needed_filtees != NULL)
			initlist_add_neededs(obj->needed_filtees, list,
			    iflist);
		if (obj->needed_aux_filtees != NULL)
			initlist_add_neededs(obj->needed_aux_filtees, list,
			    iflist);

		/* Add the object to the init list. */
		objlist_push_tail(list, obj);

		/*
		 * Add the object to the global fini list in the
		 * reverse order.
		 */
		if ((obj->fini != (Elf_Addr)NULL ||
		    obj->fini_array != (Elf_Addr)NULL) &&
		    !obj->on_fini_list) {
			objlist_push_head(&list_fini, obj);
			obj->on_fini_list = true;
		}
	}
}

static void
free_needed_filtees(Needed_Entry *n, RtldLockState *lockstate)
{
	Needed_Entry *needed, *needed1;

	for (needed = n; needed != NULL; needed = needed->next) {
		if (needed->obj != NULL) {
			dlclose_locked(needed->obj, lockstate);
			needed->obj = NULL;
		}
	}
	for (needed = n; needed != NULL; needed = needed1) {
		needed1 = needed->next;
		free(needed);
	}
}

static void
unload_filtees(Obj_Entry *obj, RtldLockState *lockstate)
{
	free_needed_filtees(obj->needed_filtees, lockstate);
	obj->needed_filtees = NULL;
	free_needed_filtees(obj->needed_aux_filtees, lockstate);
	obj->needed_aux_filtees = NULL;
	obj->filtees_loaded = false;
}

static void
load_filtee1(Obj_Entry *obj, Needed_Entry *needed, int flags,
    RtldLockState *lockstate)
{
	for (; needed != NULL; needed = needed->next) {
		needed->obj = dlopen_object(obj->strtab + needed->name, -1, obj,
		    flags, ((ld_loadfltr || obj->z_loadfltr) ? RTLD_NOW :
		    RTLD_LAZY) | RTLD_LOCAL, lockstate);
	}
}

static void
load_filtees(Obj_Entry *obj, int flags, RtldLockState *lockstate)
{
	if (obj->filtees_loaded || obj->filtees_loading)
		return;
	lock_restart_for_upgrade(lockstate);
	obj->filtees_loading = true;
	load_filtee1(obj, obj->needed_filtees, flags, lockstate);
	load_filtee1(obj, obj->needed_aux_filtees, flags, lockstate);
	obj->filtees_loaded = true;
	obj->filtees_loading = false;
}

static int
process_needed(Obj_Entry *obj, Needed_Entry *needed, int flags)
{
	Obj_Entry *obj1;

	for (; needed != NULL; needed = needed->next) {
		obj1 = needed->obj = load_object(obj->strtab + needed->name, -1,
		    obj, flags & ~RTLD_LO_NOLOAD);
		if (obj1 == NULL && !ld_tracing &&
		    (flags & RTLD_LO_FILTEES) == 0)
			return (-1);
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

	for (obj = first; obj != NULL; obj = TAILQ_NEXT(obj, next)) {
		if (obj->marker)
			continue;
		if (process_needed(obj, obj->needed, flags) == -1)
			return (-1);
	}
	return (0);
}

static int
load_preload_objects(const char *penv, bool isfd)
{
	Obj_Entry *obj;
	const char *name;
	size_t len;
	char savech, *p, *psave;
	int fd;
	static const char delim[] = " \t:;";

	if (penv == NULL)
		return (0);

	p = psave = xstrdup(penv);
	p += strspn(p, delim);
	while (*p != '\0') {
		len = strcspn(p, delim);

		savech = p[len];
		p[len] = '\0';
		if (isfd) {
			name = NULL;
			fd = parse_integer(p);
			if (fd == -1) {
				free(psave);
				return (-1);
			}
		} else {
			name = p;
			fd = -1;
		}

		obj = load_object(name, fd, NULL, 0);
		if (obj == NULL) {
			free(psave);
			return (-1); /* XXX - cleanup */
		}
		obj->z_interpose = true;
		p[len] = savech;
		p += len;
		p += strspn(p, delim);
	}
	LD_UTRACE(UTRACE_PRELOAD_FINISHED, NULL, NULL, 0, 0, NULL);

	free(psave);
	return (0);
}

static const char *
printable_path(const char *path)
{
	return (path == NULL ? "<unknown>" : path);
}

/*
 * Load a shared object into memory, if it is not already loaded.  The
 * object may be specified by name or by user-supplied file descriptor
 * fd_u. In the later case, the fd_u descriptor is not closed, but its
 * duplicate is.
 *
 * Returns a pointer to the Obj_Entry for the object.  Returns NULL
 * on failure.
 */
static Obj_Entry *
load_object(const char *name, int fd_u, const Obj_Entry *refobj, int flags)
{
	Obj_Entry *obj;
	int fd;
	struct stat sb;
	char *path;

	fd = -1;
	if (name != NULL) {
		TAILQ_FOREACH(obj, &obj_list, next) {
			if (obj->marker || obj->doomed)
				continue;
			if (object_match_name(obj, name))
				return (obj);
		}

		path = find_library(name, refobj, &fd);
		if (path == NULL)
			return (NULL);
	} else
		path = NULL;

	if (fd >= 0) {
		/*
		 * search_library_pathfds() opens a fresh file descriptor for
		 * the library, so there is no need to dup().
		 */
	} else if (fd_u == -1) {
		/*
		 * If we didn't find a match by pathname, or the name is not
		 * supplied, open the file and check again by device and inode.
		 * This avoids false mismatches caused by multiple links or ".."
		 * in pathnames.
		 *
		 * To avoid a race, we open the file and use fstat() rather than
		 * using stat().
		 */
		if ((fd = open(path, O_RDONLY | O_CLOEXEC | O_VERIFY)) == -1) {
			_rtld_error("Cannot open \"%s\"", path);
			free(path);
			return (NULL);
		}
	} else {
		fd = fcntl(fd_u, F_DUPFD_CLOEXEC, 0);
		if (fd == -1) {
			_rtld_error("Cannot dup fd");
			free(path);
			return (NULL);
		}
	}
	if (fstat(fd, &sb) == -1) {
		_rtld_error("Cannot fstat \"%s\"", printable_path(path));
		close(fd);
		free(path);
		return (NULL);
	}
	TAILQ_FOREACH(obj, &obj_list, next) {
		if (obj->marker || obj->doomed)
			continue;
		if (obj->ino == sb.st_ino && obj->dev == sb.st_dev)
			break;
	}
	if (obj != NULL) {
		if (name != NULL)
			object_add_name(obj, name);
		free(path);
		close(fd);
		return (obj);
	}
	if (flags & RTLD_LO_NOLOAD) {
		free(path);
		close(fd);
		return (NULL);
	}

	/* First use of this object, so we must map it in */
	obj = do_load_object(fd, name, path, &sb, flags);
	if (obj == NULL)
		free(path);
	close(fd);

	return (obj);
}

static Obj_Entry *
do_load_object(int fd, const char *name, char *path, struct stat *sbp,
    int flags)
{
	Obj_Entry *obj;
	struct statfs fs;

	/*
	 * First, make sure that environment variables haven't been
	 * used to circumvent the noexec flag on a filesystem.
	 * We ignore fstatfs(2) failures, since fd might reference
	 * not a file, e.g. shmfd.
	 */
	if (dangerous_ld_env && fstatfs(fd, &fs) == 0 &&
	    (fs.f_flags & MNT_NOEXEC) != 0) {
		_rtld_error("Cannot execute objects on %s", fs.f_mntonname);
		return (NULL);
	}

	dbg("loading \"%s\"", printable_path(path));
	obj = map_object(fd, printable_path(path), sbp, false);
	if (obj == NULL)
		return (NULL);

	/*
	 * If DT_SONAME is present in the object, digest_dynamic2 already
	 * added it to the object names.
	 */
	if (name != NULL)
		object_add_name(obj, name);
	obj->path = path;
	if (!digest_dynamic(obj, 0))
		goto errp;
	dbg("%s valid_hash_sysv %d valid_hash_gnu %d dynsymcount %d", obj->path,
	    obj->valid_hash_sysv, obj->valid_hash_gnu, obj->dynsymcount);
	if (obj->z_pie && (flags & RTLD_LO_TRACE) == 0) {
		dbg("refusing to load PIE executable \"%s\"", obj->path);
		_rtld_error("Cannot load PIE binary %s as DSO", obj->path);
		goto errp;
	}
	if (obj->z_noopen &&
	    (flags & (RTLD_LO_DLOPEN | RTLD_LO_TRACE)) == RTLD_LO_DLOPEN) {
		dbg("refusing to load non-loadable \"%s\"", obj->path);
		_rtld_error("Cannot dlopen non-loadable %s", obj->path);
		goto errp;
	}

	obj->dlopened = (flags & RTLD_LO_DLOPEN) != 0;
	TAILQ_INSERT_TAIL(&obj_list, obj, next);
	obj_count++;
	obj_loads++;
	linkmap_add(obj); /* for GDB & dlinfo() */
	max_stack_flags |= obj->stack_flags;

	dbg("  %p .. %p: %s", obj->mapbase, obj->mapbase + obj->mapsize - 1,
	    obj->path);
	if (obj->textrel)
		dbg("  WARNING: %s has impure text", obj->path);
	LD_UTRACE(UTRACE_LOAD_OBJECT, obj, obj->mapbase, obj->mapsize, 0,
	    obj->path);

	return (obj);

errp:
	munmap(obj->mapbase, obj->mapsize);
	obj_free(obj);
	return (NULL);
}

static int
load_kpreload(const void *addr)
{
	Obj_Entry *obj;
	const Elf_Ehdr *ehdr;
	const Elf_Phdr *phdr, *phlimit, *phdyn, *seg0, *segn;
	static const char kname[] = "[vdso]";

	ehdr = addr;
	if (!check_elf_headers(ehdr, "kpreload"))
		return (-1);
	obj = obj_new();
	phdr = (const Elf_Phdr *)((const char *)addr + ehdr->e_phoff);
	obj->phdr = phdr;
	obj->phsize = ehdr->e_phnum * sizeof(*phdr);
	phlimit = phdr + ehdr->e_phnum;
	seg0 = segn = NULL;

	for (; phdr < phlimit; phdr++) {
		switch (phdr->p_type) {
		case PT_DYNAMIC:
			phdyn = phdr;
			break;
		case PT_GNU_STACK:
			/* Absense of PT_GNU_STACK implies stack_flags == 0. */
			obj->stack_flags = phdr->p_flags;
			break;
		case PT_LOAD:
			if (seg0 == NULL || seg0->p_vaddr > phdr->p_vaddr)
				seg0 = phdr;
			if (segn == NULL ||
			    segn->p_vaddr + segn->p_memsz <
				phdr->p_vaddr + phdr->p_memsz)
				segn = phdr;
			break;
		}
	}

	obj->mapbase = __DECONST(caddr_t, addr);
	obj->mapsize = segn->p_vaddr + segn->p_memsz - (Elf_Addr)addr;
	obj->vaddrbase = 0;
	obj->relocbase = obj->mapbase;

	object_add_name(obj, kname);
	obj->path = xstrdup(kname);
	obj->dynamic = (const Elf_Dyn *)(obj->relocbase + phdyn->p_vaddr);

	if (!digest_dynamic(obj, 0)) {
		obj_free(obj);
		return (-1);
	}

	/*
	 * We assume that kernel-preloaded object does not need
	 * relocation.  It is currently written into read-only page,
	 * handling relocations would mean we need to allocate at
	 * least one additional page per AS.
	 */
	dbg("%s mapbase %p phdrs %p PT_LOAD phdr %p vaddr %p dynamic %p",
	    obj->path, obj->mapbase, obj->phdr, seg0,
	    obj->relocbase + seg0->p_vaddr, obj->dynamic);

	TAILQ_INSERT_TAIL(&obj_list, obj, next);
	obj_count++;
	obj_loads++;
	linkmap_add(obj); /* for GDB & dlinfo() */
	max_stack_flags |= obj->stack_flags;

	LD_UTRACE(UTRACE_LOAD_OBJECT, obj, obj->mapbase, 0, 0, obj->path);
	return (0);
}

Obj_Entry *
obj_from_addr(const void *addr)
{
	Obj_Entry *obj;

	TAILQ_FOREACH(obj, &obj_list, next) {
		if (obj->marker)
			continue;
		if (addr < (void *)obj->mapbase)
			continue;
		if (addr < (void *)(obj->mapbase + obj->mapsize))
			return obj;
	}
	return (NULL);
}

static void
preinit_main(void)
{
	Elf_Addr *preinit_addr;
	int index;

	preinit_addr = (Elf_Addr *)obj_main->preinit_array;
	if (preinit_addr == NULL)
		return;

	for (index = 0; index < obj_main->preinit_array_num; index++) {
		if (preinit_addr[index] != 0 && preinit_addr[index] != 1) {
			dbg("calling preinit function for %s at %p",
			    obj_main->path, (void *)preinit_addr[index]);
			LD_UTRACE(UTRACE_INIT_CALL, obj_main,
			    (void *)preinit_addr[index], 0, 0, obj_main->path);
			call_init_pointer(obj_main, preinit_addr[index]);
		}
	}
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
	struct dlerror_save *saved_msg;
	Elf_Addr *fini_addr;
	int index;

	assert(root == NULL || root->refcount == 1);

	if (root != NULL)
		root->doomed = true;

	/*
	 * Preserve the current error message since a fini function might
	 * call into the dynamic linker and overwrite it.
	 */
	saved_msg = errmsg_save();
	do {
		STAILQ_FOREACH(elm, list, link) {
			if (root != NULL &&
			    (elm->obj->refcount != 1 ||
				objlist_find(&root->dagmembers, elm->obj) ==
				    NULL))
				continue;
			/* Remove object from fini list to prevent recursive
			 * invocation. */
			STAILQ_REMOVE(list, elm, Struct_Objlist_Entry, link);
			/* Ensure that new references cannot be acquired. */
			elm->obj->doomed = true;

			hold_object(elm->obj);
			lock_release(rtld_bind_lock, lockstate);
			/*
			 * It is legal to have both DT_FINI and DT_FINI_ARRAY
			 * defined. When this happens, DT_FINI_ARRAY is
			 * processed first.
			 */
			fini_addr = (Elf_Addr *)elm->obj->fini_array;
			if (fini_addr != NULL && elm->obj->fini_array_num > 0) {
				for (index = elm->obj->fini_array_num - 1;
				    index >= 0; index--) {
					if (fini_addr[index] != 0 &&
					    fini_addr[index] != 1) {
				dbg("calling fini function for %s at %p",
						    elm->obj->path,
						    (void *)fini_addr[index]);
						LD_UTRACE(UTRACE_FINI_CALL,
						    elm->obj,
						    (void *)fini_addr[index], 0,
						    0, elm->obj->path);
						call_initfini_pointer(elm->obj,
						    fini_addr[index]);
					}
				}
			}
			if (elm->obj->fini != (Elf_Addr)NULL) {
				dbg("calling fini function for %s at %p",
				    elm->obj->path, (void *)elm->obj->fini);
				LD_UTRACE(UTRACE_FINI_CALL, elm->obj,
				    (void *)elm->obj->fini, 0, 0,
				    elm->obj->path);
				call_initfini_pointer(elm->obj, elm->obj->fini);
			}
			wlock_acquire(rtld_bind_lock, lockstate);
			unhold_object(elm->obj);
			/* No need to free anything if process is going down. */
			if (root != NULL)
				free(elm);
			/*
			 * We must restart the list traversal after every fini
			 * call because a dlclose() call from the fini function
			 * or from another thread might have modified the
			 * reference counts.
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
	struct dlerror_save *saved_msg;
	Elf_Addr *init_addr;
	void (*reg)(void (*)(void));
	int index;

	/*
	 * Clean init_scanned flag so that objects can be rechecked and
	 * possibly initialized earlier if any of vectors called below
	 * cause the change by using dlopen.
	 */
	TAILQ_FOREACH(obj, &obj_list, next) {
		if (obj->marker)
			continue;
		obj->init_scanned = false;
	}

	/*
	 * Preserve the current error message since an init function might
	 * call into the dynamic linker and overwrite it.
	 */
	saved_msg = errmsg_save();
	STAILQ_FOREACH(elm, list, link) {
		if (elm->obj->init_done) /* Initialized early. */
			continue;
		/*
		 * Race: other thread might try to use this object before
		 * current one completes the initialization. Not much can be
		 * done here without better locking.
		 */
		elm->obj->init_done = true;
		hold_object(elm->obj);
		reg = NULL;
		if (elm->obj == obj_main && obj_main->crt_no_init) {
			reg = (void (*)(void (*)(void)))
			    get_program_var_addr("__libc_atexit", lockstate);
		}
		lock_release(rtld_bind_lock, lockstate);
		if (reg != NULL) {
			reg(rtld_exit);
			rtld_exit_ptr = rtld_nop_exit;
		}

		/*
		 * It is legal to have both DT_INIT and DT_INIT_ARRAY defined.
		 * When this happens, DT_INIT is processed first.
		 */
		if (elm->obj->init != (Elf_Addr)NULL) {
			dbg("calling init function for %s at %p",
			    elm->obj->path, (void *)elm->obj->init);
			LD_UTRACE(UTRACE_INIT_CALL, elm->obj,
			    (void *)elm->obj->init, 0, 0, elm->obj->path);
			call_init_pointer(elm->obj, elm->obj->init);
		}
		init_addr = (Elf_Addr *)elm->obj->init_array;
		if (init_addr != NULL) {
			for (index = 0; index < elm->obj->init_array_num;
			    index++) {
				if (init_addr[index] != 0 &&
				    init_addr[index] != 1) {
				dbg("calling init function for %s at %p",
					    elm->obj->path,
					    (void *)init_addr[index]);
					LD_UTRACE(UTRACE_INIT_CALL, elm->obj,
					    (void *)init_addr[index], 0, 0,
					    elm->obj->path);
					call_init_pointer(elm->obj,
					    init_addr[index]);
				}
			}
		}
		wlock_acquire(rtld_bind_lock, lockstate);
		unhold_object(elm->obj);
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
	return (NULL);
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
objlist_put_after(Objlist *list, Obj_Entry *listobj, Obj_Entry *obj)
{
	Objlist_Entry *elm, *listelm;

	STAILQ_FOREACH(listelm, list, link) {
		if (listelm->obj == listobj)
			break;
	}
	elm = NEW(Objlist_Entry);
	elm->obj = obj;
	if (listelm != NULL)
		STAILQ_INSERT_AFTER(list, listelm, elm, link);
	else
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
 * Relocate dag rooted in the specified object.
 * Returns 0 on success, or -1 on failure.
 */

static int
relocate_object_dag(Obj_Entry *root, bool bind_now, Obj_Entry *rtldobj,
    int flags, RtldLockState *lockstate)
{
	Objlist_Entry *elm;
	int error;

	error = 0;
	STAILQ_FOREACH(elm, &root->dagmembers, link) {
		error = relocate_object(elm->obj, bind_now, rtldobj, flags,
		    lockstate);
		if (error == -1)
			break;
	}
	return (error);
}

/*
 * Prepare for, or clean after, relocating an object marked with
 * DT_TEXTREL or DF_TEXTREL.  Before relocating, all read-only
 * segments are remapped read-write.  After relocations are done, the
 * segment's permissions are returned back to the modes specified in
 * the phdrs.  If any relocation happened, or always for wired
 * program, COW is triggered.
 */
static int
reloc_textrel_prot(Obj_Entry *obj, bool before)
{
	const Elf_Phdr *ph;
	void *base;
	size_t l, sz;
	int prot;

	for (l = obj->phsize / sizeof(*ph), ph = obj->phdr; l > 0; l--, ph++) {
		if (ph->p_type != PT_LOAD || (ph->p_flags & PF_W) != 0)
			continue;
		base = obj->relocbase + rtld_trunc_page(ph->p_vaddr);
		sz = rtld_round_page(ph->p_vaddr + ph->p_filesz) -
		    rtld_trunc_page(ph->p_vaddr);
		prot = before ? (PROT_READ | PROT_WRITE) :
		    convert_prot(ph->p_flags);
		if (mprotect(base, sz, prot) == -1) {
			_rtld_error("%s: Cannot write-%sable text segment: %s",
			    obj->path, before ? "en" : "dis",
			    rtld_strerror(errno));
			return (-1);
		}
	}
	return (0);
}

/* Process RELR relative relocations. */
static void
reloc_relr(Obj_Entry *obj)
{
	const Elf_Relr *relr, *relrlim;
	Elf_Addr *where;

	relrlim = (const Elf_Relr *)((const char *)obj->relr + obj->relrsize);
	for (relr = obj->relr; relr < relrlim; relr++) {
		Elf_Relr entry = *relr;

		if ((entry & 1) == 0) {
			where = (Elf_Addr *)(obj->relocbase + entry);
			*where++ += (Elf_Addr)obj->relocbase;
		} else {
			for (long i = 0; (entry >>= 1) != 0; i++)
				if ((entry & 1) != 0)
					where[i] += (Elf_Addr)obj->relocbase;
			where += CHAR_BIT * sizeof(Elf_Relr) - 1;
		}
	}
}

/*
 * Relocate single object.
 * Returns 0 on success, or -1 on failure.
 */
static int
relocate_object(Obj_Entry *obj, bool bind_now, Obj_Entry *rtldobj, int flags,
    RtldLockState *lockstate)
{
	if (obj->relocated)
		return (0);
	obj->relocated = true;
	if (obj != rtldobj)
		dbg("relocating \"%s\"", obj->path);

	if (obj->symtab == NULL || obj->strtab == NULL ||
	    !(obj->valid_hash_sysv || obj->valid_hash_gnu))
		dbg("object %s has no run-time symbol table", obj->path);

	/* There are relocations to the write-protected text segment. */
	if (obj->textrel && reloc_textrel_prot(obj, true) != 0)
		return (-1);

	/* Process the non-PLT non-IFUNC relocations. */
	if (reloc_non_plt(obj, rtldobj, flags, lockstate))
		return (-1);
	reloc_relr(obj);

	/* Re-protected the text segment. */
	if (obj->textrel && reloc_textrel_prot(obj, false) != 0)
		return (-1);

	/* Set the special PLT or GOT entries. */
	init_pltgot(obj);

	/* Process the PLT relocations. */
	if (reloc_plt(obj, flags, lockstate) == -1)
		return (-1);
	/* Relocate the jump slots if we are doing immediate binding. */
	if ((obj->bind_now || bind_now) &&
	    reloc_jmpslots(obj, flags, lockstate) == -1)
		return (-1);

	if (obj != rtldobj && !obj->mainprog && obj_enforce_relro(obj) == -1)
		return (-1);

	/*
	 * Set up the magic number and version in the Obj_Entry.  These
	 * were checked in the crt1.o from the original ElfKit, so we
	 * set them for backward compatibility.
	 */
	obj->magic = RTLD_MAGIC;
	obj->version = RTLD_VERSION;

	return (0);
}

/*
 * Relocate newly-loaded shared objects.  The argument is a pointer to
 * the Obj_Entry for the first such object.  All objects from the first
 * to the end of the list of objects are relocated.  Returns 0 on success,
 * or -1 on failure.
 */
static int
relocate_objects(Obj_Entry *first, bool bind_now, Obj_Entry *rtldobj, int flags,
    RtldLockState *lockstate)
{
	Obj_Entry *obj;
	int error;

	for (error = 0, obj = first; obj != NULL; obj = TAILQ_NEXT(obj, next)) {
		if (obj->marker)
			continue;
		error = relocate_object(obj, bind_now, rtldobj, flags,
		    lockstate);
		if (error == -1)
			break;
	}
	return (error);
}

/*
 * The handling of R_MACHINE_IRELATIVE relocations and jumpslots
 * referencing STT_GNU_IFUNC symbols is postponed till the other
 * relocations are done.  The indirect functions specified as
 * ifunc are allowed to call other symbols, so we need to have
 * objects relocated before asking for resolution from indirects.
 *
 * The R_MACHINE_IRELATIVE slots are resolved in greedy fashion,
 * instead of the usual lazy handling of PLT slots.  It is
 * consistent with how GNU does it.
 */
static int
resolve_object_ifunc(Obj_Entry *obj, bool bind_now, int flags,
    RtldLockState *lockstate)
{
	if (obj->ifuncs_resolved)
		return (0);
	obj->ifuncs_resolved = true;
	if (!obj->irelative && !obj->irelative_nonplt &&
	    !((obj->bind_now || bind_now) && obj->gnu_ifunc) &&
	    !obj->non_plt_gnu_ifunc)
		return (0);
	if (obj_disable_relro(obj) == -1 ||
	    (obj->irelative && reloc_iresolve(obj, lockstate) == -1) ||
	    (obj->irelative_nonplt &&
	    reloc_iresolve_nonplt(obj, lockstate) == -1) ||
	    ((obj->bind_now || bind_now) && obj->gnu_ifunc &&
	    reloc_gnu_ifunc(obj, flags, lockstate) == -1) ||
	    (obj->non_plt_gnu_ifunc &&
	    reloc_non_plt(obj, &obj_rtld, flags | SYMLOOK_IFUNC,
	    lockstate) == -1) ||
	    obj_enforce_relro(obj) == -1)
		return (-1);
	return (0);
}

static int
initlist_objects_ifunc(Objlist *list, bool bind_now, int flags,
    RtldLockState *lockstate)
{
	Objlist_Entry *elm;
	Obj_Entry *obj;

	STAILQ_FOREACH(elm, list, link) {
		obj = elm->obj;
		if (obj->marker)
			continue;
		if (resolve_object_ifunc(obj, bind_now, flags, lockstate) == -1)
			return (-1);
	}
	return (0);
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

static void
rtld_nop_exit(void)
{
}

/*
 * Iterate over a search path, translate each element, and invoke the
 * callback on the result.
 */
static void *
path_enumerate(const char *path, path_enum_proc callback,
    const char *refobj_path, void *arg)
{
	const char *trans;
	if (path == NULL)
		return (NULL);

	path += strspn(path, ":;");
	while (*path != '\0') {
		size_t len;
		char *res;

		len = strcspn(path, ":;");
		trans = lm_findn(refobj_path, path, len);
		if (trans)
			res = callback(trans, strlen(trans), arg);
		else
			res = callback(path, len, arg);

		if (res != NULL)
			return (res);

		path += len;
		path += strspn(path, ":;");
	}

	return (NULL);
}

struct try_library_args {
	const char *name;
	size_t namelen;
	char *buffer;
	size_t buflen;
	int fd;
};

static void *
try_library_path(const char *dir, size_t dirlen, void *param)
{
	struct try_library_args *arg;
	int fd;

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
		fd = open(pathname, O_RDONLY | O_CLOEXEC | O_VERIFY);
		if (fd >= 0) {
			dbg("  Opened \"%s\", fd %d", pathname, fd);
			pathname = xmalloc(dirlen + 1 + arg->namelen + 1);
			strcpy(pathname, arg->buffer);
			arg->fd = fd;
			return (pathname);
		} else {
			dbg("  Failed to open \"%s\": %s", pathname,
			    rtld_strerror(errno));
		}
	}
	return (NULL);
}

static char *
search_library_path(const char *name, const char *path, const char *refobj_path,
    int *fdp)
{
	char *p;
	struct try_library_args arg;

	if (path == NULL)
		return (NULL);

	arg.name = name;
	arg.namelen = strlen(name);
	arg.buffer = xmalloc(PATH_MAX);
	arg.buflen = PATH_MAX;
	arg.fd = -1;

	p = path_enumerate(path, try_library_path, refobj_path, &arg);
	*fdp = arg.fd;

	free(arg.buffer);

	return (p);
}

/*
 * Finds the library with the given name using the directory descriptors
 * listed in the LD_LIBRARY_PATH_FDS environment variable.
 *
 * Returns a freshly-opened close-on-exec file descriptor for the library,
 * or -1 if the library cannot be found.
 */
static char *
search_library_pathfds(const char *name, const char *path, int *fdp)
{
	char *envcopy, *fdstr, *found, *last_token;
	size_t len;
	int dirfd, fd;

	dbg("%s('%s', '%s', fdp)", __func__, name, path);

	/* Don't load from user-specified libdirs into setuid binaries. */
	if (!trust)
		return (NULL);

	/* We can't do anything if LD_LIBRARY_PATH_FDS isn't set. */
	if (path == NULL)
		return (NULL);

	/* LD_LIBRARY_PATH_FDS only works with relative paths. */
	if (name[0] == '/') {
		dbg("Absolute path (%s) passed to %s", name, __func__);
		return (NULL);
	}

	/*
	 * Use strtok_r() to walk the FD:FD:FD list.  This requires a local
	 * copy of the path, as strtok_r rewrites separator tokens
	 * with '\0'.
	 */
	found = NULL;
	envcopy = xstrdup(path);
	for (fdstr = strtok_r(envcopy, ":", &last_token); fdstr != NULL;
	    fdstr = strtok_r(NULL, ":", &last_token)) {
		dirfd = parse_integer(fdstr);
		if (dirfd < 0) {
			_rtld_error("failed to parse directory FD: '%s'",
			    fdstr);
			break;
		}
		fd = __sys_openat(dirfd, name, O_RDONLY | O_CLOEXEC | O_VERIFY);
		if (fd >= 0) {
			*fdp = fd;
			len = strlen(fdstr) + strlen(name) + 3;
			found = xmalloc(len);
			if (rtld_snprintf(found, len, "#%d/%s", dirfd, name) <
			    0) {
				_rtld_error("error generating '%d/%s'", dirfd,
				    name);
				rtld_die();
			}
			dbg("open('%s') => %d", found, fd);
			break;
		}
	}
	free(envcopy);

	return (found);
}

int
dlclose(void *handle)
{
	RtldLockState lockstate;
	int error;

	wlock_acquire(rtld_bind_lock, &lockstate);
	error = dlclose_locked(handle, &lockstate);
	lock_release(rtld_bind_lock, &lockstate);
	return (error);
}

static int
dlclose_locked(void *handle, RtldLockState *lockstate)
{
	Obj_Entry *root;

	root = dlcheck(handle);
	if (root == NULL)
		return (-1);
	LD_UTRACE(UTRACE_DLCLOSE_START, handle, NULL, 0, root->dl_refcount,
	    root->path);

	/* Unreference the object and its dependencies. */
	root->dl_refcount--;

	if (root->refcount == 1) {
		/*
		 * The object will be no longer referenced, so we must unload
		 * it. First, call the fini functions.
		 */
		objlist_call_fini(&list_fini, root, lockstate);

		unref_dag(root);

		/* Finish cleaning up the newly-unreferenced objects. */
		GDB_STATE(RT_DELETE, &root->linkmap);
		unload_object(root, lockstate);
		GDB_STATE(RT_CONSISTENT, NULL);
	} else
		unref_dag(root);

	LD_UTRACE(UTRACE_DLCLOSE_STOP, handle, NULL, 0, 0, NULL);
	return (0);
}

char *
dlerror(void)
{
	if (*(lockinfo.dlerror_seen()) != 0)
		return (NULL);
	*lockinfo.dlerror_seen() = 1;
	return (lockinfo.dlerror_loc());
}

/*
 * This function is deprecated and has no effect.
 */
void
dllockinit(void *context, void *(*_lock_create)(void *context)__unused,
    void (*_rlock_acquire)(void *lock) __unused,
    void (*_wlock_acquire)(void *lock) __unused,
    void (*_lock_release)(void *lock) __unused,
    void (*_lock_destroy)(void *lock) __unused,
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
	return (rtld_dlopen(name, -1, mode));
}

void *
fdlopen(int fd, int mode)
{
	return (rtld_dlopen(NULL, fd, mode));
}

static void *
rtld_dlopen(const char *name, int fd, int mode)
{
	RtldLockState lockstate;
	int lo_flags;

	LD_UTRACE(UTRACE_DLOPEN_START, NULL, NULL, 0, mode, name);
	ld_tracing = (mode & RTLD_TRACE) == 0 ? NULL : "1";
	if (ld_tracing != NULL) {
		rlock_acquire(rtld_bind_lock, &lockstate);
		if (sigsetjmp(lockstate.env, 0) != 0)
			lock_upgrade(rtld_bind_lock, &lockstate);
		environ = __DECONST(char **,
		    *get_program_var_addr("environ", &lockstate));
		lock_release(rtld_bind_lock, &lockstate);
	}
	lo_flags = RTLD_LO_DLOPEN;
	if (mode & RTLD_NODELETE)
		lo_flags |= RTLD_LO_NODELETE;
	if (mode & RTLD_NOLOAD)
		lo_flags |= RTLD_LO_NOLOAD;
	if (mode & RTLD_DEEPBIND)
		lo_flags |= RTLD_LO_DEEPBIND;
	if (ld_tracing != NULL)
		lo_flags |= RTLD_LO_TRACE | RTLD_LO_IGNSTLS;

	return (dlopen_object(name, fd, obj_main, lo_flags,
	    mode & (RTLD_MODEMASK | RTLD_GLOBAL), NULL));
}

static void
dlopen_cleanup(Obj_Entry *obj, RtldLockState *lockstate)
{
	obj->dl_refcount--;
	unref_dag(obj);
	if (obj->refcount == 0)
		unload_object(obj, lockstate);
}

static Obj_Entry *
dlopen_object(const char *name, int fd, Obj_Entry *refobj, int lo_flags,
    int mode, RtldLockState *lockstate)
{
	Obj_Entry *obj;
	Objlist initlist;
	RtldLockState mlockstate;
	int result;

	dbg(
    "dlopen_object name \"%s\" fd %d refobj \"%s\" lo_flags %#x mode %#x",
	    name != NULL ? name : "<null>", fd,
	    refobj == NULL ? "<null>" : refobj->path, lo_flags, mode);
	objlist_init(&initlist);

	if (lockstate == NULL && !(lo_flags & RTLD_LO_EARLY)) {
		wlock_acquire(rtld_bind_lock, &mlockstate);
		lockstate = &mlockstate;
	}
	GDB_STATE(RT_ADD, NULL);

	obj = NULL;
	if (name == NULL && fd == -1) {
		obj = obj_main;
		obj->refcount++;
	} else {
		obj = load_object(name, fd, refobj, lo_flags);
	}

	if (obj != NULL) {
		obj->dl_refcount++;
		if ((mode & RTLD_GLOBAL) != 0 &&
		    objlist_find(&list_global, obj) == NULL)
			objlist_push_tail(&list_global, obj);

		if (!obj->init_done) {
			/* We loaded something new and have to init something.
			 */
			if ((lo_flags & RTLD_LO_DEEPBIND) != 0)
				obj->deepbind = true;
			result = 0;
			if ((lo_flags & (RTLD_LO_EARLY |
			    RTLD_LO_IGNSTLS)) == 0 &&
			    obj->static_tls && !allocate_tls_offset(obj)) {
				_rtld_error(
		    "%s: No space available for static Thread Local Storage",
				    obj->path);
				result = -1;
			}
			if (result != -1)
				result = load_needed_objects(obj,
				    lo_flags & (RTLD_LO_DLOPEN | RTLD_LO_EARLY |
				    RTLD_LO_IGNSTLS | RTLD_LO_TRACE));
			init_dag(obj);
			ref_dag(obj);
			if (result != -1)
				result = rtld_verify_versions(&obj->dagmembers);
			if (result != -1 && ld_tracing)
				goto trace;
			if (result == -1 || relocate_object_dag(obj,
			    (mode & RTLD_MODEMASK) == RTLD_NOW, &obj_rtld,
			    (lo_flags & RTLD_LO_EARLY) ? SYMLOOK_EARLY : 0,
			    lockstate) == -1) {
				dlopen_cleanup(obj, lockstate);
				obj = NULL;
			} else if ((lo_flags & RTLD_LO_EARLY) != 0) {
				/*
				 * Do not call the init functions for early
				 * loaded filtees.  The image is still not
				 * initialized enough for them to work.
				 *
				 * Our object is found by the global object list
				 * and will be ordered among all init calls done
				 * right before transferring control to main.
				 */
			} else {
				/* Make list of init functions to call. */
				initlist_for_loaded_obj(obj, obj, &initlist);
			}
			/*
			 * Process all no_delete or global objects here, given
			 * them own DAGs to prevent their dependencies from
			 * being unloaded.  This has to be done after we have
			 * loaded all of the dependencies, so that we do not
			 * miss any.
			 */
			if (obj != NULL)
				process_z(obj);
		} else {
			/*
			 * Bump the reference counts for objects on this DAG. If
			 * this is the first dlopen() call for the object that
			 * was already loaded as a dependency, initialize the
			 * dag starting at it.
			 */
			init_dag(obj);
			ref_dag(obj);

			if ((lo_flags & RTLD_LO_TRACE) != 0)
				goto trace;
		}
		if (obj != NULL &&
		    ((lo_flags & RTLD_LO_NODELETE) != 0 || obj->z_nodelete) &&
		    !obj->ref_nodel) {
			dbg("obj %s nodelete", obj->path);
			ref_dag(obj);
			obj->z_nodelete = obj->ref_nodel = true;
		}
	}

	LD_UTRACE(UTRACE_DLOPEN_STOP, obj, NULL, 0, obj ? obj->dl_refcount : 0,
	    name);
	GDB_STATE(RT_CONSISTENT, obj ? &obj->linkmap : NULL);

	if ((lo_flags & RTLD_LO_EARLY) == 0) {
		map_stacks_exec(lockstate);
		if (obj != NULL)
			distribute_static_tls(&initlist, lockstate);
	}

	if (initlist_objects_ifunc(&initlist, (mode & RTLD_MODEMASK) ==
	    RTLD_NOW, (lo_flags & RTLD_LO_EARLY) ? SYMLOOK_EARLY : 0,
	    lockstate) == -1) {
		objlist_clear(&initlist);
		dlopen_cleanup(obj, lockstate);
		if (lockstate == &mlockstate)
			lock_release(rtld_bind_lock, lockstate);
		return (NULL);
	}

	if ((lo_flags & RTLD_LO_EARLY) == 0) {
		/* Call the init functions. */
		objlist_call_init(&initlist, lockstate);
	}
	objlist_clear(&initlist);
	if (lockstate == &mlockstate)
		lock_release(rtld_bind_lock, lockstate);
	return (obj);
trace:
	trace_loaded_objects(obj, false);
	if (lockstate == &mlockstate)
		lock_release(rtld_bind_lock, lockstate);
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
	tls_index ti;
	void *sym;
	int res;

	def = NULL;
	defobj = NULL;
	symlook_init(&req, name);
	req.ventry = ve;
	req.flags = flags | SYMLOOK_IN_PLT;
	req.lockstate = &lockstate;

	LD_UTRACE(UTRACE_DLSYM_START, handle, NULL, 0, 0, name);
	rlock_acquire(rtld_bind_lock, &lockstate);
	if (sigsetjmp(lockstate.env, 0) != 0)
		lock_upgrade(rtld_bind_lock, &lockstate);
	if (handle == NULL || handle == RTLD_NEXT || handle == RTLD_DEFAULT ||
	    handle == RTLD_SELF) {
		if ((obj = obj_from_addr(retaddr)) == NULL) {
			_rtld_error("Cannot determine caller's shared object");
			lock_release(rtld_bind_lock, &lockstate);
			LD_UTRACE(UTRACE_DLSYM_STOP, handle, NULL, 0, 0, name);
			return (NULL);
		}
		if (handle == NULL) { /* Just the caller's shared object. */
			res = symlook_obj(&req, obj);
			if (res == 0) {
				def = req.sym_out;
				defobj = req.defobj_out;
			}
		} else if (handle == RTLD_NEXT || /* Objects after caller's */
		    handle == RTLD_SELF) {	  /* ... caller included */
			if (handle == RTLD_NEXT)
				obj = globallist_next(obj);
			for (; obj != NULL; obj = TAILQ_NEXT(obj, next)) {
				if (obj->marker)
					continue;
				res = symlook_obj(&req, obj);
				if (res == 0) {
					if (def == NULL ||
					    (ld_dynamic_weak &&
						ELF_ST_BIND(
						    req.sym_out->st_info) !=
						    STB_WEAK)) {
						def = req.sym_out;
						defobj = req.defobj_out;
						if (!ld_dynamic_weak ||
						    ELF_ST_BIND(def->st_info) !=
							STB_WEAK)
							break;
					}
				}
			}
			/*
			 * Search the dynamic linker itself, and possibly
			 * resolve the symbol from there.  This is how the
			 * application links to dynamic linker services such as
			 * dlopen. Note that we ignore ld_dynamic_weak == false
			 * case, always overriding weak symbols by rtld
			 * definitions.
			 */
			if (def == NULL ||
			    ELF_ST_BIND(def->st_info) == STB_WEAK) {
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
			LD_UTRACE(UTRACE_DLSYM_STOP, handle, NULL, 0, 0, name);
			return (NULL);
		}

		donelist_init(&donelist);
		if (obj->mainprog) {
			/* Handle obtained by dlopen(NULL, ...) implies global
			 * scope. */
			res = symlook_global(&req, &donelist);
			if (res == 0) {
				def = req.sym_out;
				defobj = req.defobj_out;
			}
			/*
			 * Search the dynamic linker itself, and possibly
			 * resolve the symbol from there.  This is how the
			 * application links to dynamic linker services such as
			 * dlopen.
			 */
			if (def == NULL ||
			    ELF_ST_BIND(def->st_info) == STB_WEAK) {
				res = symlook_obj(&req, &obj_rtld);
				if (res == 0) {
					def = req.sym_out;
					defobj = req.defobj_out;
				}
			}
		} else {
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
		 * of the symbol. this is simply the relocated value of the
		 * symbol.
		 */
		if (ELF_ST_TYPE(def->st_info) == STT_FUNC)
			sym = make_function_pointer(def, defobj);
		else if (ELF_ST_TYPE(def->st_info) == STT_GNU_IFUNC)
			sym = rtld_resolve_ifunc(defobj, def);
		else if (ELF_ST_TYPE(def->st_info) == STT_TLS) {
			ti.ti_module = defobj->tlsindex;
			ti.ti_offset = def->st_value - TLS_DTV_OFFSET;
			sym = __tls_get_addr(&ti);
		} else
			sym = defobj->relocbase + def->st_value;
		LD_UTRACE(UTRACE_DLSYM_STOP, handle, sym, 0, 0, name);
		return (sym);
	}

	_rtld_error("Undefined symbol \"%s%s%s\"", name, ve != NULL ? "@" : "",
	    ve != NULL ? ve->name : "");
	lock_release(rtld_bind_lock, &lockstate);
	LD_UTRACE(UTRACE_DLSYM_STOP, handle, NULL, 0, 0, name);
	return (NULL);
}

void *
dlsym(void *handle, const char *name)
{
	return (do_dlsym(handle, name, __builtin_return_address(0), NULL,
	    SYMLOOK_DLSYM));
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
	ventry.flags = 0;
	return (do_dlsym(handle, name, __builtin_return_address(0), &ventry,
	    SYMLOOK_DLSYM));
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
		return (0);
	}
	info->dli_fname = obj->path;
	info->dli_fbase = obj->mapbase;
	info->dli_saddr = (void *)0;
	info->dli_sname = NULL;

	/*
	 * Walk the symbol list looking for the symbol whose address is
	 * closest to the address sent in.
	 */
	for (symoffset = 0; symoffset < obj->dynsymcount; symoffset++) {
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
	return (1);
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

		retaddr = __builtin_return_address(0); /* __GNUC__ only */
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
	struct dtv **dtvp;

	phdr_info->dlpi_addr = (Elf_Addr)obj->relocbase;
	phdr_info->dlpi_name = obj->path;
	phdr_info->dlpi_phdr = obj->phdr;
	phdr_info->dlpi_phnum = obj->phsize / sizeof(obj->phdr[0]);
	phdr_info->dlpi_tls_modid = obj->tlsindex;
	dtvp = &_tcb_get()->tcb_dtv;
	phdr_info->dlpi_tls_data = (char *)tls_get_addr_slow(dtvp,
	    obj->tlsindex, 0, true);
	phdr_info->dlpi_adds = obj_loads;
	phdr_info->dlpi_subs = obj_loads - obj_count;
}

/*
 * It's completely UB to actually use this, so extreme caution is advised.  It's
 * probably not what you want.
 */
int
_dl_iterate_phdr_locked(__dl_iterate_hdr_callback callback, void *param)
{
	struct dl_phdr_info phdr_info;
	Obj_Entry *obj;
	int error;

	for (obj = globallist_curr(TAILQ_FIRST(&obj_list)); obj != NULL;
	    obj = globallist_next(obj)) {
		rtld_fill_dl_phdr_info(obj, &phdr_info);
		error = callback(&phdr_info, sizeof(phdr_info), param);
		if (error != 0)
			return (error);
	}

	rtld_fill_dl_phdr_info(&obj_rtld, &phdr_info);
	return (callback(&phdr_info, sizeof(phdr_info), param));
}

int
dl_iterate_phdr(__dl_iterate_hdr_callback callback, void *param)
{
	struct dl_phdr_info phdr_info;
	Obj_Entry *obj, marker;
	RtldLockState bind_lockstate, phdr_lockstate;
	int error;

	init_marker(&marker);
	error = 0;

	wlock_acquire(rtld_phdr_lock, &phdr_lockstate);
	wlock_acquire(rtld_bind_lock, &bind_lockstate);
	for (obj = globallist_curr(TAILQ_FIRST(&obj_list)); obj != NULL;) {
		TAILQ_INSERT_AFTER(&obj_list, obj, &marker, next);
		rtld_fill_dl_phdr_info(obj, &phdr_info);
		hold_object(obj);
		lock_release(rtld_bind_lock, &bind_lockstate);

		error = callback(&phdr_info, sizeof phdr_info, param);

		wlock_acquire(rtld_bind_lock, &bind_lockstate);
		unhold_object(obj);
		obj = globallist_next(&marker);
		TAILQ_REMOVE(&obj_list, &marker, next);
		if (error != 0) {
			lock_release(rtld_bind_lock, &bind_lockstate);
			lock_release(rtld_phdr_lock, &phdr_lockstate);
			return (error);
		}
	}

	if (error == 0) {
		rtld_fill_dl_phdr_info(&obj_rtld, &phdr_info);
		lock_release(rtld_bind_lock, &bind_lockstate);
		error = callback(&phdr_info, sizeof(phdr_info), param);
	}
	lock_release(rtld_phdr_lock, &phdr_lockstate);
	return (error);
}

static void *
fill_search_info(const char *dir, size_t dirlen, void *param)
{
	struct fill_search_info_args *arg;

	arg = param;

	if (arg->request == RTLD_DI_SERINFOSIZE) {
		arg->serinfo->dls_cnt++;
		arg->serinfo->dls_size += sizeof(struct dl_serpath) + dirlen +
		    1;
	} else {
		struct dl_serpath *s_entry;

		s_entry = arg->serpath;
		s_entry->dls_name = arg->strspace;
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
	_info.dls_cnt = 0;

	path_enumerate(obj->rpath, fill_search_info, NULL, &args);
	path_enumerate(ld_library_path, fill_search_info, NULL, &args);
	path_enumerate(obj->runpath, fill_search_info, NULL, &args);
	path_enumerate(gethints(obj->z_nodeflib), fill_search_info, NULL,
	    &args);
	if (!obj->z_nodeflib)
		path_enumerate(ld_standard_library_path, fill_search_info, NULL,
		    &args);

	if (request == RTLD_DI_SERINFOSIZE) {
		info->dls_size = _info.dls_size;
		info->dls_cnt = _info.dls_cnt;
		return (0);
	}

	if (info->dls_cnt != _info.dls_cnt ||
	    info->dls_size != _info.dls_size) {
		_rtld_error(
		    "Uninitialized Dl_serinfo struct passed to dlinfo()");
		return (-1);
	}

	args.request = RTLD_DI_SERINFO;
	args.serinfo = info;
	args.serpath = &info->dls_serpath[0];
	args.strspace = (char *)&info->dls_serpath[_info.dls_cnt];

	args.flags = LA_SER_RUNPATH;
	if (path_enumerate(obj->rpath, fill_search_info, NULL, &args) != NULL)
		return (-1);

	args.flags = LA_SER_LIBPATH;
	if (path_enumerate(ld_library_path, fill_search_info, NULL, &args) !=
	    NULL)
		return (-1);

	args.flags = LA_SER_RUNPATH;
	if (path_enumerate(obj->runpath, fill_search_info, NULL, &args) != NULL)
		return (-1);

	args.flags = LA_SER_CONFIG;
	if (path_enumerate(gethints(obj->z_nodeflib), fill_search_info, NULL,
		&args) != NULL)
		return (-1);

	args.flags = LA_SER_DEFAULT;
	if (!obj->z_nodeflib &&
	    path_enumerate(ld_standard_library_path, fill_search_info, NULL,
		&args) != NULL)
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

	if (endp - path + 2 > PATH_MAX) {
		_rtld_error("Filename is too long: %s", path);
		return (-1);
	}

	strncpy(bname, path, endp - path + 1);
	bname[endp - path + 1] = '\0';
	return (0);
}

static int
rtld_dirname_abs(const char *path, char *base)
{
	char *last;

	if (realpath(path, base) == NULL) {
		_rtld_error("realpath \"%s\" failed (%s)", path,
		    rtld_strerror(errno));
		return (-1);
	}
	dbg("%s -> %s", path, base);
	last = strrchr(base, '/');
	if (last == NULL) {
		_rtld_error("non-abs result from realpath \"%s\"", path);
		return (-1);
	}
	if (last != base)
		*last = '\0';
	return (0);
}

static void
linkmap_add(Obj_Entry *obj)
{
	struct link_map *l, *prev;

	l = &obj->linkmap;
	l->l_name = obj->path;
	l->l_base = obj->mapbase;
	l->l_ld = obj->dynamic;
	l->l_addr = obj->relocbase;

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
	struct link_map *l;

	l = &obj->linkmap;
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
r_debug_state(struct r_debug *rd __unused, struct link_map *m __unused)
{
	/*
	 * The following is a hack to force the compiler to emit calls to
	 * this function, even when optimizing.  If the function is empty,
	 * the compiler is not obliged to emit any code for calls to it,
	 * even when marked __noinline.  However, gdb depends on those
	 * calls being made.
	 */
	__compiler_membar();
}

/*
 * A function called after init routines have completed. This can be used to
 * break before a program's entry routine is called, and can be used when
 * main is not available in the symbol table.
 */
void
_r_debug_postinit(struct link_map *m __unused)
{
	/* See r_debug_state(). */
	__compiler_membar();
}

static void
release_object(Obj_Entry *obj)
{
	if (obj->holdcount > 0) {
		obj->unholdfree = true;
		return;
	}
	munmap(obj->mapbase, obj->mapsize);
	linkmap_delete(obj);
	obj_free(obj);
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
	else if (ELF_ST_TYPE(req.sym_out->st_info) == STT_GNU_IFUNC)
		return ((const void **)rtld_resolve_ifunc(req.defobj_out,
		    req.sym_out));
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
	if (req->defobj_out == NULL || (ld_dynamic_weak &&
	    ELF_ST_BIND(req->sym_out->st_info) == STB_WEAK)) {
		res = symlook_list(&req1, &list_main, donelist);
		if (res == 0 && (!ld_dynamic_weak || req->defobj_out == NULL ||
		    ELF_ST_BIND(req1.sym_out->st_info) != STB_WEAK)) {
			req->sym_out = req1.sym_out;
			req->defobj_out = req1.defobj_out;
			assert(req->defobj_out != NULL);
		}
	}

	/* Search all DAGs whose roots are RTLD_GLOBAL objects. */
	STAILQ_FOREACH(elm, &list_global, link) {
		if (req->defobj_out != NULL && (!ld_dynamic_weak ||
		    ELF_ST_BIND(req->sym_out->st_info) != STB_WEAK))
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

	/*
	 * Look first in the referencing object if linked symbolically,
	 * and similarly handle protected symbols.
	 */
	res = symlook_obj(&req1, refobj);
	if (res == 0 && (refobj->symbolic ||
	    ELF_ST_VISIBILITY(req1.sym_out->st_other) == STV_PROTECTED ||
	    refobj->deepbind)) {
		req->sym_out = req1.sym_out;
		req->defobj_out = req1.defobj_out;
		assert(req->defobj_out != NULL);
	}
	if (refobj->symbolic || req->defobj_out != NULL || refobj->deepbind)
		donelist_check(&donelist, refobj);

	if (!refobj->deepbind)
		symlook_global(req, &donelist);

	/* Search all dlopened DAGs containing the referencing object. */
	STAILQ_FOREACH(elm, &refobj->dldags, link) {
		if (req->sym_out != NULL && (!ld_dynamic_weak ||
		    ELF_ST_BIND(req->sym_out->st_info) != STB_WEAK))
			break;
		res = symlook_list(&req1, &elm->obj->dagmembers, &donelist);
		if (res == 0 && (req->sym_out == NULL ||
		    ELF_ST_BIND(req1.sym_out->st_info) != STB_WEAK)) {
			req->sym_out = req1.sym_out;
			req->defobj_out = req1.defobj_out;
			assert(req->defobj_out != NULL);
		}
	}

	if (refobj->deepbind)
		symlook_global(req, &donelist);

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
			if (def == NULL || (ld_dynamic_weak &&
			    ELF_ST_BIND(req1.sym_out->st_info) != STB_WEAK)) {
				def = req1.sym_out;
				defobj = req1.defobj_out;
				if (!ld_dynamic_weak ||
				    ELF_ST_BIND(def->st_info) != STB_WEAK)
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
		if (n->obj == NULL || (res = symlook_list(&req1,
		    &n->obj->dagmembers, dlp)) != 0)
			continue;
		if (def == NULL || (ld_dynamic_weak &&
		    ELF_ST_BIND(req1.sym_out->st_info) != STB_WEAK)) {
			def = req1.sym_out;
			defobj = req1.defobj_out;
			if (!ld_dynamic_weak ||
			    ELF_ST_BIND(def->st_info) != STB_WEAK)
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

static int
symlook_obj_load_filtees(SymLook *req, SymLook *req1, const Obj_Entry *obj,
    Needed_Entry *needed)
{
	DoneList donelist;
	int flags;

	flags = (req->flags & SYMLOOK_EARLY) != 0 ? RTLD_LO_EARLY : 0;
	load_filtees(__DECONST(Obj_Entry *, obj), flags, req->lockstate);
	donelist_init(&donelist);
	symlook_init_from_req(req1, req);
	return (symlook_needed(req1, needed, &donelist));
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
	SymLook req1;
	int res, mres;

	/*
	 * If there is at least one valid hash at this point, we prefer to
	 * use the faster GNU version if available.
	 */
	if (obj->valid_hash_gnu)
		mres = symlook_obj1_gnu(req, obj);
	else if (obj->valid_hash_sysv)
		mres = symlook_obj1_sysv(req, obj);
	else
		return (EINVAL);

	if (mres == 0) {
		if (obj->needed_filtees != NULL) {
			res = symlook_obj_load_filtees(req, &req1, obj,
			    obj->needed_filtees);
			if (res == 0) {
				req->sym_out = req1.sym_out;
				req->defobj_out = req1.defobj_out;
			}
			return (res);
		}
		if (obj->needed_aux_filtees != NULL) {
			res = symlook_obj_load_filtees(req, &req1, obj,
			    obj->needed_aux_filtees);
			if (res == 0) {
				req->sym_out = req1.sym_out;
				req->defobj_out = req1.defobj_out;
				return (res);
			}
		}
	}
	return (mres);
}

/* Symbol match routine common to both hash functions */
static bool
matched_symbol(SymLook *req, const Obj_Entry *obj, Sym_Match_Result *result,
    const unsigned long symnum)
{
	Elf_Versym verndx;
	const Elf_Sym *symp;
	const char *strp;

	symp = obj->symtab + symnum;
	strp = obj->strtab + symp->st_name;

	switch (ELF_ST_TYPE(symp->st_info)) {
	case STT_FUNC:
	case STT_NOTYPE:
	case STT_OBJECT:
	case STT_COMMON:
	case STT_GNU_IFUNC:
		if (symp->st_value == 0)
			return (false);
		/* fallthrough */
	case STT_TLS:
		if (symp->st_shndx != SHN_UNDEF)
			break;
		else if (((req->flags & SYMLOOK_IN_PLT) == 0) &&
		    (ELF_ST_TYPE(symp->st_info) == STT_FUNC))
			break;
		/* fallthrough */
	default:
		return (false);
	}
	if (req->name[0] != strp[0] || strcmp(req->name, strp) != 0)
		return (false);

	if (req->ventry == NULL) {
		if (obj->versyms != NULL) {
			verndx = VER_NDX(obj->versyms[symnum]);
			if (verndx > obj->vernum) {
				_rtld_error(
				    "%s: symbol %s references wrong version %d",
				    obj->path, obj->strtab + symnum, verndx);
				return (false);
			}
			/*
			 * If we are not called from dlsym (i.e. this
			 * is a normal relocation from unversioned
			 * binary), accept the symbol immediately if
			 * it happens to have first version after this
			 * shared object became versioned.  Otherwise,
			 * if symbol is versioned and not hidden,
			 * remember it. If it is the only symbol with
			 * this name exported by the shared object, it
			 * will be returned as a match by the calling
			 * function. If symbol is global (verndx < 2)
			 * accept it unconditionally.
			 */
			if ((req->flags & SYMLOOK_DLSYM) == 0 &&
			    verndx == VER_NDX_GIVEN) {
				result->sym_out = symp;
				return (true);
			} else if (verndx >= VER_NDX_GIVEN) {
				if ((obj->versyms[symnum] & VER_NDX_HIDDEN) ==
				    0) {
					if (result->vsymp == NULL)
						result->vsymp = symp;
					result->vcount++;
				}
				return (false);
			}
		}
		result->sym_out = symp;
		return (true);
	}
	if (obj->versyms == NULL) {
		if (object_match_name(obj, req->ventry->name)) {
			_rtld_error(
		    "%s: object %s should provide version %s for symbol %s",
			    obj_rtld.path, obj->path, req->ventry->name,
			    obj->strtab + symnum);
			return (false);
		}
	} else {
		verndx = VER_NDX(obj->versyms[symnum]);
		if (verndx > obj->vernum) {
			_rtld_error("%s: symbol %s references wrong version %d",
			    obj->path, obj->strtab + symnum, verndx);
			return (false);
		}
		if (obj->vertab[verndx].hash != req->ventry->hash ||
		    strcmp(obj->vertab[verndx].name, req->ventry->name)) {
			/*
			 * Version does not match. Look if this is a
			 * global symbol and if it is not hidden. If
			 * global symbol (verndx < 2) is available,
			 * use it. Do not return symbol if we are
			 * called by dlvsym, because dlvsym looks for
			 * a specific version and default one is not
			 * what dlvsym wants.
			 */
			if ((req->flags & SYMLOOK_DLSYM) ||
			    (verndx >= VER_NDX_GIVEN) ||
			    (obj->versyms[symnum] & VER_NDX_HIDDEN))
				return (false);
		}
	}
	result->sym_out = symp;
	return (true);
}

/*
 * Search for symbol using SysV hash function.
 * obj->buckets is known not to be NULL at this point; the test for this was
 * performed with the obj->valid_hash_sysv assignment.
 */
static int
symlook_obj1_sysv(SymLook *req, const Obj_Entry *obj)
{
	unsigned long symnum;
	Sym_Match_Result matchres;

	matchres.sym_out = NULL;
	matchres.vsymp = NULL;
	matchres.vcount = 0;

	for (symnum = obj->buckets[req->hash % obj->nbuckets];
	    symnum != STN_UNDEF; symnum = obj->chains[symnum]) {
		if (symnum >= obj->nchains)
			return (ESRCH); /* Bad object */

		if (matched_symbol(req, obj, &matchres, symnum)) {
			req->sym_out = matchres.sym_out;
			req->defobj_out = obj;
			return (0);
		}
	}
	if (matchres.vcount == 1) {
		req->sym_out = matchres.vsymp;
		req->defobj_out = obj;
		return (0);
	}
	return (ESRCH);
}

/* Search for symbol using GNU hash function */
static int
symlook_obj1_gnu(SymLook *req, const Obj_Entry *obj)
{
	Elf_Addr bloom_word;
	const Elf32_Word *hashval;
	Elf32_Word bucket;
	Sym_Match_Result matchres;
	unsigned int h1, h2;
	unsigned long symnum;

	matchres.sym_out = NULL;
	matchres.vsymp = NULL;
	matchres.vcount = 0;

	/* Pick right bitmask word from Bloom filter array */
	bloom_word = obj->bloom_gnu[(req->hash_gnu / __ELF_WORD_SIZE) &
	    obj->maskwords_bm_gnu];

	/* Calculate modulus word size of gnu hash and its derivative */
	h1 = req->hash_gnu & (__ELF_WORD_SIZE - 1);
	h2 = ((req->hash_gnu >> obj->shift2_gnu) & (__ELF_WORD_SIZE - 1));

	/* Filter out the "definitely not in set" queries */
	if (((bloom_word >> h1) & (bloom_word >> h2) & 1) == 0)
		return (ESRCH);

	/* Locate hash chain and corresponding value element*/
	bucket = obj->buckets_gnu[req->hash_gnu % obj->nbuckets_gnu];
	if (bucket == 0)
		return (ESRCH);
	hashval = &obj->chain_zero_gnu[bucket];
	do {
		if (((*hashval ^ req->hash_gnu) >> 1) == 0) {
			symnum = hashval - obj->chain_zero_gnu;
			if (matched_symbol(req, obj, &matchres, symnum)) {
				req->sym_out = matchres.sym_out;
				req->defobj_out = obj;
				return (0);
			}
		}
	} while ((*hashval++ & 1) == 0);
	if (matchres.vcount == 1) {
		req->sym_out = matchres.vsymp;
		req->defobj_out = obj;
		return (0);
	}
	return (ESRCH);
}

static void
trace_calc_fmts(const char **main_local, const char **fmt1, const char **fmt2)
{
	*main_local = ld_get_env_var(LD_TRACE_LOADED_OBJECTS_PROGNAME);
	if (*main_local == NULL)
		*main_local = "";

	*fmt1 = ld_get_env_var(LD_TRACE_LOADED_OBJECTS_FMT1);
	if (*fmt1 == NULL)
		*fmt1 = "\t%o => %p (%x)\n";

	*fmt2 = ld_get_env_var(LD_TRACE_LOADED_OBJECTS_FMT2);
	if (*fmt2 == NULL)
		*fmt2 = "\t%o (%x)\n";
}

static void
trace_print_obj(Obj_Entry *obj, const char *name, const char *path,
    const char *main_local, const char *fmt1, const char *fmt2)
{
	const char *fmt;
	int c;

	if (fmt1 == NULL)
		fmt = fmt2;
	else
		/* XXX bogus */
		fmt = strncmp(name, "lib", 3) == 0 ? fmt1 : fmt2;

	while ((c = *fmt++) != '\0') {
		switch (c) {
		default:
			rtld_putchar(c);
			continue;
		case '\\':
			switch (c = *fmt) {
			case '\0':
				continue;
			case 'n':
				rtld_putchar('\n');
				break;
			case 't':
				rtld_putchar('\t');
				break;
			}
			break;
		case '%':
			switch (c = *fmt) {
			case '\0':
				continue;
			case '%':
			default:
				rtld_putchar(c);
				break;
			case 'A':
				rtld_putstr(main_local);
				break;
			case 'a':
				rtld_putstr(obj_main->path);
				break;
			case 'o':
				rtld_putstr(name);
				break;
			case 'p':
				rtld_putstr(path);
				break;
			case 'x':
				rtld_printf("%p",
				    obj != NULL ? obj->mapbase : NULL);
				break;
			}
			break;
		}
		++fmt;
	}
}

static void
trace_loaded_objects(Obj_Entry *obj, bool show_preload)
{
	const char *fmt1, *fmt2, *main_local;
	const char *name, *path;
	bool first_spurious, list_containers;

	trace_calc_fmts(&main_local, &fmt1, &fmt2);
	list_containers = ld_get_env_var(LD_TRACE_LOADED_OBJECTS_ALL) != NULL;

	for (; obj != NULL; obj = TAILQ_NEXT(obj, next)) {
		Needed_Entry *needed;

		if (obj->marker)
			continue;
		if (list_containers && obj->needed != NULL)
			rtld_printf("%s:\n", obj->path);
		for (needed = obj->needed; needed; needed = needed->next) {
			if (needed->obj != NULL) {
				if (needed->obj->traced && !list_containers)
					continue;
				needed->obj->traced = true;
				path = needed->obj->path;
			} else
				path = "not found";

			name = obj->strtab + needed->name;
			trace_print_obj(needed->obj, name, path, main_local,
			    fmt1, fmt2);
		}
	}

	if (show_preload) {
		if (ld_get_env_var(LD_TRACE_LOADED_OBJECTS_FMT2) == NULL)
			fmt2 = "\t%p (%x)\n";
		first_spurious = true;

		TAILQ_FOREACH(obj, &obj_list, next) {
			if (obj->marker || obj == obj_main || obj->traced)
				continue;

			if (list_containers && first_spurious) {
				rtld_printf("[preloaded]\n");
				first_spurious = false;
			}

			Name_Entry *fname = STAILQ_FIRST(&obj->names);
			name = fname == NULL ? "<unknown>" : fname->name;
			trace_print_obj(obj, name, obj->path, main_local, NULL,
			    fmt2);
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
unload_object(Obj_Entry *root, RtldLockState *lockstate)
{
	Obj_Entry marker, *obj, *next;

	assert(root->refcount == 0);

	/*
	 * Pass over the DAG removing unreferenced objects from
	 * appropriate lists.
	 */
	unlink_object(root);

	/* Unmap all objects that are no longer referenced. */
	for (obj = TAILQ_FIRST(&obj_list); obj != NULL; obj = next) {
		next = TAILQ_NEXT(obj, next);
		if (obj->marker || obj->refcount != 0)
			continue;
		LD_UTRACE(UTRACE_UNLOAD_OBJECT, obj, obj->mapbase, obj->mapsize,
		    0, obj->path);
		dbg("unloading \"%s\"", obj->path);
		/*
		 * Unlink the object now to prevent new references from
		 * being acquired while the bind lock is dropped in
		 * recursive dlclose() invocations.
		 */
		TAILQ_REMOVE(&obj_list, obj, next);
		obj_count--;

		if (obj->filtees_loaded) {
			if (next != NULL) {
				init_marker(&marker);
				TAILQ_INSERT_BEFORE(next, &marker, next);
				unload_filtees(obj, lockstate);
				next = TAILQ_NEXT(&marker, next);
				TAILQ_REMOVE(&obj_list, &marker, next);
			} else
				unload_filtees(obj, lockstate);
		}
		release_object(obj);
	}
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
static void *
tls_get_addr_slow(struct dtv **dtvp, int index, size_t offset, bool locked)
{
	struct dtv *newdtv, *dtv;
	RtldLockState lockstate;
	int to_copy;

	dtv = *dtvp;
	/* Check dtv generation in case new modules have arrived */
	if (dtv->dtv_gen != tls_dtv_generation) {
		if (!locked)
			wlock_acquire(rtld_bind_lock, &lockstate);
		newdtv = xcalloc(1, sizeof(struct dtv) + tls_max_index *
		    sizeof(struct dtv_slot));
		to_copy = dtv->dtv_size;
		if (to_copy > tls_max_index)
			to_copy = tls_max_index;
		memcpy(newdtv->dtv_slots, dtv->dtv_slots, to_copy *
		    sizeof(struct dtv_slot));
		newdtv->dtv_gen = tls_dtv_generation;
		newdtv->dtv_size = tls_max_index;
		free(dtv);
		if (!locked)
			lock_release(rtld_bind_lock, &lockstate);
		dtv = *dtvp = newdtv;
	}

	/* Dynamically allocate module TLS if necessary */
	if (dtv->dtv_slots[index - 1].dtvs_tls == 0) {
		/* Signal safe, wlock will block out signals. */
		if (!locked)
			wlock_acquire(rtld_bind_lock, &lockstate);
		if (!dtv->dtv_slots[index - 1].dtvs_tls)
			dtv->dtv_slots[index - 1].dtvs_tls =
			    allocate_module_tls(index);
		if (!locked)
			lock_release(rtld_bind_lock, &lockstate);
	}
	return (dtv->dtv_slots[index - 1].dtvs_tls + offset);
}

void *
tls_get_addr_common(struct dtv **dtvp, int index, size_t offset)
{
	struct dtv *dtv;

	dtv = *dtvp;
	/* Check dtv generation in case new modules have arrived */
	if (__predict_true(dtv->dtv_gen == tls_dtv_generation &&
	    dtv->dtv_slots[index - 1].dtvs_tls != 0))
		return (dtv->dtv_slots[index - 1].dtvs_tls + offset);
	return (tls_get_addr_slow(dtvp, index, offset, false));
}

#ifdef TLS_VARIANT_I

/*
 * Return pointer to allocated TLS block
 */
static void *
get_tls_block_ptr(void *tcb, size_t tcbsize)
{
	size_t extra_size, post_size, pre_size, tls_block_size;
	size_t tls_init_align;

	tls_init_align = MAX(obj_main->tlsalign, 1);

	/* Compute fragments sizes. */
	extra_size = tcbsize - TLS_TCB_SIZE;
	post_size = calculate_tls_post_size(tls_init_align);
	tls_block_size = tcbsize + post_size;
	pre_size = roundup2(tls_block_size, tls_init_align) - tls_block_size;

	return ((char *)tcb - pre_size - extra_size);
}

/*
 * Allocate Static TLS using the Variant I method.
 *
 * For details on the layout, see lib/libc/gen/tls.c.
 *
 * NB: rtld's tls_static_space variable includes TLS_TCB_SIZE and post_size as
 *     it is based on tls_last_offset, and TLS offsets here are really TCB
 *     offsets, whereas libc's tls_static_space is just the executable's static
 *     TLS segment.
 */
void *
allocate_tls(Obj_Entry *objs, void *oldtcb, size_t tcbsize, size_t tcbalign)
{
	Obj_Entry *obj;
	char *tls_block;
	struct dtv *dtv;
	struct tcb *tcb;
	char *addr;
	size_t i;
	size_t extra_size, maxalign, post_size, pre_size, tls_block_size;
	size_t tls_init_align, tls_init_offset;

	if (oldtcb != NULL && tcbsize == TLS_TCB_SIZE)
		return (oldtcb);

	assert(tcbsize >= TLS_TCB_SIZE);
	maxalign = MAX(tcbalign, tls_static_max_align);
	tls_init_align = MAX(obj_main->tlsalign, 1);

	/* Compute fragments sizes. */
	extra_size = tcbsize - TLS_TCB_SIZE;
	post_size = calculate_tls_post_size(tls_init_align);
	tls_block_size = tcbsize + post_size;
	pre_size = roundup2(tls_block_size, tls_init_align) - tls_block_size;
	tls_block_size += pre_size + tls_static_space - TLS_TCB_SIZE -
	    post_size;

	/* Allocate whole TLS block */
	tls_block = xmalloc_aligned(tls_block_size, maxalign, 0);
	tcb = (struct tcb *)(tls_block + pre_size + extra_size);

	if (oldtcb != NULL) {
		memcpy(tls_block, get_tls_block_ptr(oldtcb, tcbsize),
		    tls_static_space);
		free(get_tls_block_ptr(oldtcb, tcbsize));

		/* Adjust the DTV. */
		dtv = tcb->tcb_dtv;
		for (i = 0; i < dtv->dtv_size; i++) {
			if ((uintptr_t)dtv->dtv_slots[i].dtvs_tls >=
			    (uintptr_t)oldtcb &&
			    (uintptr_t)dtv->dtv_slots[i].dtvs_tls <
			    (uintptr_t)oldtcb + tls_static_space) {
				dtv->dtv_slots[i].dtvs_tls = (char *)tcb +
				    (dtv->dtv_slots[i].dtvs_tls -
				    (char *)oldtcb);
			}
		}
	} else {
		dtv = xcalloc(1, sizeof(struct dtv) + tls_max_index *
		    sizeof(struct dtv_slot));
		tcb->tcb_dtv = dtv;
		dtv->dtv_gen = tls_dtv_generation;
		dtv->dtv_size = tls_max_index;

		for (obj = globallist_curr(objs); obj != NULL;
		    obj = globallist_next(obj)) {
			if (obj->tlsoffset == 0)
				continue;
			tls_init_offset = obj->tlspoffset & (obj->tlsalign - 1);
			addr = (char *)tcb + obj->tlsoffset;
			if (tls_init_offset > 0)
				memset(addr, 0, tls_init_offset);
			if (obj->tlsinitsize > 0) {
				memcpy(addr + tls_init_offset, obj->tlsinit,
				    obj->tlsinitsize);
			}
			if (obj->tlssize > obj->tlsinitsize) {
				memset(addr + tls_init_offset +
				    obj->tlsinitsize,
				    0,
				    obj->tlssize - obj->tlsinitsize -
					tls_init_offset);
			}
			dtv->dtv_slots[obj->tlsindex - 1].dtvs_tls = addr;
		}
	}

	return (tcb);
}

void
free_tls(void *tcb, size_t tcbsize, size_t tcbalign __unused)
{
	struct dtv *dtv;
	uintptr_t tlsstart, tlsend;
	size_t post_size;
	size_t i, tls_init_align __unused;

	assert(tcbsize >= TLS_TCB_SIZE);
	tls_init_align = MAX(obj_main->tlsalign, 1);

	/* Compute fragments sizes. */
	post_size = calculate_tls_post_size(tls_init_align);

	tlsstart = (uintptr_t)tcb + TLS_TCB_SIZE + post_size;
	tlsend = (uintptr_t)tcb + tls_static_space;

	dtv = ((struct tcb *)tcb)->tcb_dtv;
	for (i = 0; i < dtv->dtv_size; i++) {
		if (dtv->dtv_slots[i].dtvs_tls != NULL &&
		    ((uintptr_t)dtv->dtv_slots[i].dtvs_tls < tlsstart ||
		    (uintptr_t)dtv->dtv_slots[i].dtvs_tls >= tlsend)) {
			free(dtv->dtv_slots[i].dtvs_tls);
		}
	}
	free(dtv);
	free(get_tls_block_ptr(tcb, tcbsize));
}

#endif /* TLS_VARIANT_I */

#ifdef TLS_VARIANT_II

/*
 * Allocate Static TLS using the Variant II method.
 */
void *
allocate_tls(Obj_Entry *objs, void *oldtcb, size_t tcbsize, size_t tcbalign)
{
	Obj_Entry *obj;
	size_t size, ralign;
	char *tls_block;
	struct dtv *dtv, *olddtv;
	struct tcb *tcb;
	char *addr;
	size_t i;

	ralign = tcbalign;
	if (tls_static_max_align > ralign)
		ralign = tls_static_max_align;
	size = roundup(tls_static_space, ralign) + roundup(tcbsize, ralign);

	assert(tcbsize >= 2 * sizeof(uintptr_t));
	tls_block = xmalloc_aligned(size, ralign, 0 /* XXX */);
	dtv = xcalloc(1, sizeof(struct dtv) + tls_max_index *
	    sizeof(struct dtv_slot));

	tcb = (struct tcb *)(tls_block + roundup(tls_static_space, ralign));
	tcb->tcb_self = tcb;
	tcb->tcb_dtv = dtv;

	dtv->dtv_gen = tls_dtv_generation;
	dtv->dtv_size = tls_max_index;

	if (oldtcb != NULL) {
		/*
		 * Copy the static TLS block over whole.
		 */
		memcpy((char *)tcb - tls_static_space,
		    (const char *)oldtcb - tls_static_space,
		    tls_static_space);

		/*
		 * If any dynamic TLS blocks have been created tls_get_addr(),
		 * move them over.
		 */
		olddtv = ((struct tcb *)oldtcb)->tcb_dtv;
		for (i = 0; i < olddtv->dtv_size; i++) {
			if ((uintptr_t)olddtv->dtv_slots[i].dtvs_tls <
			    (uintptr_t)oldtcb - size ||
			    (uintptr_t)olddtv->dtv_slots[i].dtvs_tls >
			    (uintptr_t)oldtcb) {
				dtv->dtv_slots[i].dtvs_tls =
				    olddtv->dtv_slots[i].dtvs_tls;
				olddtv->dtv_slots[i].dtvs_tls = NULL;
			}
		}

		/*
		 * We assume that this block was the one we created with
		 * allocate_initial_tls().
		 */
		free_tls(oldtcb, 2 * sizeof(uintptr_t), sizeof(uintptr_t));
	} else {
		for (obj = objs; obj != NULL; obj = TAILQ_NEXT(obj, next)) {
			if (obj->marker || obj->tlsoffset == 0)
				continue;
			addr = (char *)tcb - obj->tlsoffset;
			memset(addr + obj->tlsinitsize, 0, obj->tlssize -
			    obj->tlsinitsize);
			if (obj->tlsinit) {
				memcpy(addr, obj->tlsinit, obj->tlsinitsize);
				obj->static_tls_copied = true;
			}
			dtv->dtv_slots[obj->tlsindex - 1].dtvs_tls = addr;
		}
	}

	return (tcb);
}

void
free_tls(void *tcb, size_t tcbsize __unused, size_t tcbalign)
{
	struct dtv *dtv;
	size_t size, ralign;
	size_t i;
	uintptr_t tlsstart, tlsend;

	/*
	 * Figure out the size of the initial TLS block so that we can
	 * find stuff which ___tls_get_addr() allocated dynamically.
	 */
	ralign = tcbalign;
	if (tls_static_max_align > ralign)
		ralign = tls_static_max_align;
	size = roundup(tls_static_space, ralign);

	dtv = ((struct tcb *)tcb)->tcb_dtv;
	tlsend = (uintptr_t)tcb;
	tlsstart = tlsend - size;
	for (i = 0; i < dtv->dtv_size; i++) {
		if (dtv->dtv_slots[i].dtvs_tls != NULL &&
		    ((uintptr_t)dtv->dtv_slots[i].dtvs_tls < tlsstart ||
		    (uintptr_t)dtv->dtv_slots[i].dtvs_tls > tlsend)) {
			free(dtv->dtv_slots[i].dtvs_tls);
		}
	}

	free((void *)tlsstart);
	free(dtv);
}

#endif /* TLS_VARIANT_II */

/*
 * Allocate TLS block for module with given index.
 */
void *
allocate_module_tls(int index)
{
	Obj_Entry *obj;
	char *p;

	TAILQ_FOREACH(obj, &obj_list, next) {
		if (obj->marker)
			continue;
		if (obj->tlsindex == index)
			break;
	}
	if (obj == NULL) {
		_rtld_error("Can't find module with TLS index %d", index);
		rtld_die();
	}

	if (obj->tls_static) {
#ifdef TLS_VARIANT_I
		p = (char *)_tcb_get() + obj->tlsoffset + TLS_TCB_SIZE;
#else
		p = (char *)_tcb_get() - obj->tlsoffset;
#endif
		return (p);
	}

	obj->tls_dynamic = true;

	p = xmalloc_aligned(obj->tlssize, obj->tlsalign, obj->tlspoffset);
	memcpy(p, obj->tlsinit, obj->tlsinitsize);
	memset(p + obj->tlsinitsize, 0, obj->tlssize - obj->tlsinitsize);
	return (p);
}

bool
allocate_tls_offset(Obj_Entry *obj)
{
	size_t off;

	if (obj->tls_dynamic)
		return (false);

	if (obj->tls_static)
		return (true);

	if (obj->tlssize == 0) {
		obj->tls_static = true;
		return (true);
	}

	if (tls_last_offset == 0)
		off = calculate_first_tls_offset(obj->tlssize, obj->tlsalign,
		    obj->tlspoffset);
	else
		off = calculate_tls_offset(tls_last_offset, tls_last_size,
		    obj->tlssize, obj->tlsalign, obj->tlspoffset);

	obj->tlsoffset = off;
#ifdef TLS_VARIANT_I
	off += obj->tlssize;
#endif

	/*
	 * If we have already fixed the size of the static TLS block, we
	 * must stay within that size. When allocating the static TLS, we
	 * leave a small amount of space spare to be used for dynamically
	 * loading modules which use static TLS.
	 */
	if (tls_static_space != 0) {
		if (off > tls_static_space)
			return (false);
	} else if (obj->tlsalign > tls_static_max_align) {
		tls_static_max_align = obj->tlsalign;
	}

	tls_last_offset = off;
	tls_last_size = obj->tlssize;
	obj->tls_static = true;

	return (true);
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
	size_t off = obj->tlsoffset;

#ifdef TLS_VARIANT_I
	off += obj->tlssize;
#endif
	if (off == tls_last_offset) {
		tls_last_offset -= obj->tlssize;
		tls_last_size = 0;
	}
}

void *
_rtld_allocate_tls(void *oldtcb, size_t tcbsize, size_t tcbalign)
{
	void *ret;
	RtldLockState lockstate;

	wlock_acquire(rtld_bind_lock, &lockstate);
	ret = allocate_tls(globallist_curr(TAILQ_FIRST(&obj_list)), oldtcb,
	    tcbsize, tcbalign);
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
			return (entry->obj);
	}

	for (needed = obj->needed; needed != NULL; needed = needed->next) {
		if (strcmp(obj->strtab + needed->name, name) == 0 ||
		    (needed->obj != NULL && object_match_name(needed->obj,
		    name))) {
			/*
			 * If there is DT_NEEDED for the name we are looking
			 * for, we are all set.  Note that object might not be
			 * found if dependency was not loaded yet, so the
			 * function can return NULL here.  This is expected and
			 * handled properly by the caller.
			 */
			return (needed->obj);
		}
	}
	_rtld_error("%s: Unexpected inconsistency: dependency %s not found",
	    obj->path, name);
	rtld_die();
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
			_rtld_error(
			    "%s: Unsupported version %d of Elf_Verdef entry",
			    depobj->path, vd->vd_version);
			return (-1);
		}
		if (vna->vna_hash == vd->vd_hash) {
			const Elf_Verdaux *aux =
			    (const Elf_Verdaux *)((const char *)vd +
				vd->vd_aux);
			if (strcmp(vername, depobj->strtab + aux->vda_name) ==
			    0)
				return (0);
		}
		if (vd->vd_next == 0)
			break;
		vd = (const Elf_Verdef *)((const char *)vd + vd->vd_next);
	}
	if (vna->vna_flags & VER_FLG_WEAK)
		return (0);
	_rtld_error("%s: version %s required by %s not found", depobj->path,
	    vername, refobj->path);
	return (-1);
}

static int
rtld_verify_object_versions(Obj_Entry *obj)
{
	const Elf_Verneed *vn;
	const Elf_Verdef *vd;
	const Elf_Verdaux *vda;
	const Elf_Vernaux *vna;
	const Obj_Entry *depobj;
	int maxvernum, vernum;

	if (obj->ver_checked)
		return (0);
	obj->ver_checked = true;

	maxvernum = 0;
	/*
	 * Walk over defined and required version records and figure out
	 * max index used by any of them. Do very basic sanity checking
	 * while there.
	 */
	vn = obj->verneed;
	while (vn != NULL) {
		if (vn->vn_version != VER_NEED_CURRENT) {
			_rtld_error(
			    "%s: Unsupported version %d of Elf_Verneed entry",
			    obj->path, vn->vn_version);
			return (-1);
		}
		vna = (const Elf_Vernaux *)((const char *)vn + vn->vn_aux);
		for (;;) {
			vernum = VER_NEED_IDX(vna->vna_other);
			if (vernum > maxvernum)
				maxvernum = vernum;
			if (vna->vna_next == 0)
				break;
			vna = (const Elf_Vernaux *)((const char *)vna +
			    vna->vna_next);
		}
		if (vn->vn_next == 0)
			break;
		vn = (const Elf_Verneed *)((const char *)vn + vn->vn_next);
	}

	vd = obj->verdef;
	while (vd != NULL) {
		if (vd->vd_version != VER_DEF_CURRENT) {
			_rtld_error(
			    "%s: Unsupported version %d of Elf_Verdef entry",
			    obj->path, vd->vd_version);
			return (-1);
		}
		vernum = VER_DEF_IDX(vd->vd_ndx);
		if (vernum > maxvernum)
			maxvernum = vernum;
		if (vd->vd_next == 0)
			break;
		vd = (const Elf_Verdef *)((const char *)vd + vd->vd_next);
	}

	if (maxvernum == 0)
		return (0);

	/*
	 * Store version information in array indexable by version index.
	 * Verify that object version requirements are satisfied along the
	 * way.
	 */
	obj->vernum = maxvernum + 1;
	obj->vertab = xcalloc(obj->vernum, sizeof(Ver_Entry));

	vd = obj->verdef;
	while (vd != NULL) {
		if ((vd->vd_flags & VER_FLG_BASE) == 0) {
			vernum = VER_DEF_IDX(vd->vd_ndx);
			assert(vernum <= maxvernum);
			vda = (const Elf_Verdaux *)((const char *)vd +
			    vd->vd_aux);
			obj->vertab[vernum].hash = vd->vd_hash;
			obj->vertab[vernum].name = obj->strtab + vda->vda_name;
			obj->vertab[vernum].file = NULL;
			obj->vertab[vernum].flags = 0;
		}
		if (vd->vd_next == 0)
			break;
		vd = (const Elf_Verdef *)((const char *)vd + vd->vd_next);
	}

	vn = obj->verneed;
	while (vn != NULL) {
		depobj = locate_dependency(obj, obj->strtab + vn->vn_file);
		if (depobj == NULL)
			return (-1);
		vna = (const Elf_Vernaux *)((const char *)vn + vn->vn_aux);
		for (;;) {
			if (check_object_provided_version(obj, depobj, vna))
				return (-1);
			vernum = VER_NEED_IDX(vna->vna_other);
			assert(vernum <= maxvernum);
			obj->vertab[vernum].hash = vna->vna_hash;
			obj->vertab[vernum].name = obj->strtab + vna->vna_name;
			obj->vertab[vernum].file = obj->strtab + vn->vn_file;
			obj->vertab[vernum].flags = (vna->vna_other &
			    VER_NEED_HIDDEN) != 0 ? VER_INFO_HIDDEN : 0;
			if (vna->vna_next == 0)
				break;
			vna = (const Elf_Vernaux *)((const char *)vna +
			    vna->vna_next);
		}
		if (vn->vn_next == 0)
			break;
		vn = (const Elf_Verneed *)((const char *)vn + vn->vn_next);
	}
	return (0);
}

static int
rtld_verify_versions(const Objlist *objlist)
{
	Objlist_Entry *entry;
	int rc;

	rc = 0;
	STAILQ_FOREACH(entry, objlist, link) {
		/*
		 * Skip dummy objects or objects that have their version
		 * requirements already checked.
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
	return (rc);
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
			return (&obj->vertab[vernum]);
		}
	}
	return (NULL);
}

int
_rtld_get_stack_prot(void)
{
	return (stack_prot);
}

int
_rtld_is_dlopened(void *arg)
{
	Obj_Entry *obj;
	RtldLockState lockstate;
	int res;

	rlock_acquire(rtld_bind_lock, &lockstate);
	obj = dlcheck(arg);
	if (obj == NULL)
		obj = obj_from_addr(arg);
	if (obj == NULL) {
		_rtld_error("No shared object contains address");
		lock_release(rtld_bind_lock, &lockstate);
		return (-1);
	}
	res = obj->dlopened ? 1 : 0;
	lock_release(rtld_bind_lock, &lockstate);
	return (res);
}

static int
obj_remap_relro(Obj_Entry *obj, int prot)
{
	const Elf_Phdr *ph;
	caddr_t relro_page;
	size_t relro_size;

	for (ph = obj->phdr; (const char *)ph < (const char *)obj->phdr +
	    obj->phsize; ph++) {
		if (ph->p_type != PT_GNU_RELRO)
			continue;
		relro_page = obj->relocbase + rtld_trunc_page(ph->p_vaddr);
		relro_size = rtld_round_page(ph->p_vaddr + ph->p_memsz) -
		    rtld_trunc_page(ph->p_vaddr);
		if (mprotect(relro_page, relro_size, prot) == -1) {
			_rtld_error(
			    "%s: Cannot set relro protection to %#x: %s",
			    obj->path, prot, rtld_strerror(errno));
			return (-1);
		}
		break;
	}
	return (0);
}

static int
obj_disable_relro(Obj_Entry *obj)
{
	return (obj_remap_relro(obj, PROT_READ | PROT_WRITE));
}

static int
obj_enforce_relro(Obj_Entry *obj)
{
	return (obj_remap_relro(obj, PROT_READ));
}

static void
map_stacks_exec(RtldLockState *lockstate)
{
	void (*thr_map_stacks_exec)(void);

	if ((max_stack_flags & PF_X) == 0 || (stack_prot & PROT_EXEC) != 0)
		return;
	thr_map_stacks_exec = (void (*)(void))(
	    uintptr_t)get_program_var_addr("__pthread_map_stacks_exec",
	    lockstate);
	if (thr_map_stacks_exec != NULL) {
		stack_prot |= PROT_EXEC;
		thr_map_stacks_exec();
	}
}

static void
distribute_static_tls(Objlist *list, RtldLockState *lockstate)
{
	Objlist_Entry *elm;
	Obj_Entry *obj;
	void (*distrib)(size_t, void *, size_t, size_t);

	distrib = (void (*)(size_t, void *, size_t, size_t))(
	    uintptr_t)get_program_var_addr("__pthread_distribute_static_tls",
	    lockstate);
	if (distrib == NULL)
		return;
	STAILQ_FOREACH(elm, list, link) {
		obj = elm->obj;
		if (obj->marker || !obj->tls_static || obj->static_tls_copied)
			continue;
		lock_release(rtld_bind_lock, lockstate);
		distrib(obj->tlsoffset, obj->tlsinit, obj->tlsinitsize,
		    obj->tlssize);
		wlock_acquire(rtld_bind_lock, lockstate);
		obj->static_tls_copied = true;
	}
}

void
symlook_init(SymLook *dst, const char *name)
{
	bzero(dst, sizeof(*dst));
	dst->name = name;
	dst->hash = elf_hash(name);
	dst->hash_gnu = gnu_hash(name);
}

static void
symlook_init_from_req(SymLook *dst, const SymLook *src)
{
	dst->name = src->name;
	dst->hash = src->hash;
	dst->hash_gnu = src->hash_gnu;
	dst->ventry = src->ventry;
	dst->flags = src->flags;
	dst->defobj_out = NULL;
	dst->sym_out = NULL;
	dst->lockstate = src->lockstate;
}

static int
open_binary_fd(const char *argv0, bool search_in_path, const char **binpath_res)
{
	char *binpath, *pathenv, *pe, *res1;
	const char *res;
	int fd;

	binpath = NULL;
	res = NULL;
	if (search_in_path && strchr(argv0, '/') == NULL) {
		binpath = xmalloc(PATH_MAX);
		pathenv = getenv("PATH");
		if (pathenv == NULL) {
			_rtld_error("-p and no PATH environment variable");
			rtld_die();
		}
		pathenv = strdup(pathenv);
		if (pathenv == NULL) {
			_rtld_error("Cannot allocate memory");
			rtld_die();
		}
		fd = -1;
		errno = ENOENT;
		while ((pe = strsep(&pathenv, ":")) != NULL) {
			if (strlcpy(binpath, pe, PATH_MAX) >= PATH_MAX)
				continue;
			if (binpath[0] != '\0' &&
			    strlcat(binpath, "/", PATH_MAX) >= PATH_MAX)
				continue;
			if (strlcat(binpath, argv0, PATH_MAX) >= PATH_MAX)
				continue;
			fd = open(binpath, O_RDONLY | O_CLOEXEC | O_VERIFY);
			if (fd != -1 || errno != ENOENT) {
				res = binpath;
				break;
			}
		}
		free(pathenv);
	} else {
		fd = open(argv0, O_RDONLY | O_CLOEXEC | O_VERIFY);
		res = argv0;
	}

	if (fd == -1) {
		_rtld_error("Cannot open %s: %s", argv0, rtld_strerror(errno));
		rtld_die();
	}
	if (res != NULL && res[0] != '/') {
		res1 = xmalloc(PATH_MAX);
		if (realpath(res, res1) != NULL) {
			if (res != argv0)
				free(__DECONST(char *, res));
			res = res1;
		} else {
			free(res1);
		}
	}
	*binpath_res = res;
	return (fd);
}

/*
 * Parse a set of command-line arguments.
 */
static int
parse_args(char *argv[], int argc, bool *use_pathp, int *fdp,
    const char **argv0, bool *dir_ignore)
{
	const char *arg;
	char machine[64];
	size_t sz;
	int arglen, fd, i, j, mib[2];
	char opt;
	bool seen_b, seen_f;

	dbg("Parsing command-line arguments");
	*use_pathp = false;
	*fdp = -1;
	*dir_ignore = false;
	seen_b = seen_f = false;

	for (i = 1; i < argc; i++) {
		arg = argv[i];
		dbg("argv[%d]: '%s'", i, arg);

		/*
		 * rtld arguments end with an explicit "--" or with the first
		 * non-prefixed argument.
		 */
		if (strcmp(arg, "--") == 0) {
			i++;
			break;
		}
		if (arg[0] != '-')
			break;

		/*
		 * All other arguments are single-character options that can
		 * be combined, so we need to search through `arg` for them.
		 */
		arglen = strlen(arg);
		for (j = 1; j < arglen; j++) {
			opt = arg[j];
			if (opt == 'h') {
				print_usage(argv[0]);
				_exit(0);
			} else if (opt == 'b') {
				if (seen_f) {
					_rtld_error("Both -b and -f specified");
					rtld_die();
				}
				if (j != arglen - 1) {
					_rtld_error("Invalid options: %s", arg);
					rtld_die();
				}
				i++;
				*argv0 = argv[i];
				seen_b = true;
				break;
			} else if (opt == 'd') {
				*dir_ignore = true;
			} else if (opt == 'f') {
				if (seen_b) {
					_rtld_error("Both -b and -f specified");
					rtld_die();
				}

				/*
				 * -f XX can be used to specify a
				 * descriptor for the binary named at
				 * the command line (i.e., the later
				 * argument will specify the process
				 * name but the descriptor is what
				 * will actually be executed).
				 *
				 * -f must be the last option in the
				 * group, e.g., -abcf <fd>.
				 */
				if (j != arglen - 1) {
					_rtld_error("Invalid options: %s", arg);
					rtld_die();
				}
				i++;
				fd = parse_integer(argv[i]);
				if (fd == -1) {
					_rtld_error(
					    "Invalid file descriptor: '%s'",
					    argv[i]);
					rtld_die();
				}
				*fdp = fd;
				seen_f = true;
				break;
			} else if (opt == 'o') {
				struct ld_env_var_desc *l;
				char *n, *v;
				u_int ll;

				if (j != arglen - 1) {
					_rtld_error("Invalid options: %s", arg);
					rtld_die();
				}
				i++;
				n = argv[i];
				v = strchr(n, '=');
				if (v == NULL) {
					_rtld_error("No '=' in -o parameter");
					rtld_die();
				}
				for (ll = 0; ll < nitems(ld_env_vars); ll++) {
					l = &ld_env_vars[ll];
					if (v - n == (ptrdiff_t)strlen(l->n) &&
					    strncmp(n, l->n, v - n) == 0) {
						l->val = v + 1;
						break;
					}
				}
				if (ll == nitems(ld_env_vars)) {
					_rtld_error("Unknown LD_ option %s", n);
					rtld_die();
				}
			} else if (opt == 'p') {
				*use_pathp = true;
			} else if (opt == 'u') {
				u_int ll;

				for (ll = 0; ll < nitems(ld_env_vars); ll++)
					ld_env_vars[ll].val = NULL;
			} else if (opt == 'v') {
				machine[0] = '\0';
				mib[0] = CTL_HW;
				mib[1] = HW_MACHINE;
				sz = sizeof(machine);
				sysctl(mib, nitems(mib), machine, &sz, NULL, 0);
				ld_elf_hints_path = ld_get_env_var(
				    LD_ELF_HINTS_PATH);
				set_ld_elf_hints_path();
				rtld_printf(
				    "FreeBSD ld-elf.so.1 %s\n"
				    "FreeBSD_version %d\n"
				    "Default lib path %s\n"
				    "Hints lib path %s\n"
				    "Env prefix %s\n"
				    "Default hint file %s\n"
				    "Hint file %s\n"
				    "libmap file %s\n"
				    "Optional static TLS size %zd bytes\n",
				    machine, __FreeBSD_version,
				    ld_standard_library_path, gethints(false),
				    ld_env_prefix, ld_elf_hints_default,
				    ld_elf_hints_path, ld_path_libmap_conf,
				    ld_static_tls_extra);
				_exit(0);
			} else {
				_rtld_error("Invalid argument: '%s'", arg);
				print_usage(argv[0]);
				rtld_die();
			}
		}
	}

	if (!seen_b)
		*argv0 = argv[i];
	return (i);
}

/*
 * Parse a file descriptor number without pulling in more of libc (e.g. atoi).
 */
static int
parse_integer(const char *str)
{
	static const int RADIX = 10; /* XXXJA: possibly support hex? */
	const char *orig;
	int n;
	char c;

	orig = str;
	n = 0;
	for (c = *str; c != '\0'; c = *++str) {
		if (c < '0' || c > '9')
			return (-1);

		n *= RADIX;
		n += c - '0';
	}

	/* Make sure we actually parsed something. */
	if (str == orig)
		return (-1);
	return (n);
}

static void
print_usage(const char *argv0)
{
	rtld_printf(
	    "Usage: %s [-h] [-b <exe>] [-d] [-f <FD>] [-p] [--] <binary> [<args>]\n"
	    "\n"
	    "Options:\n"
	    "  -h        Display this help message\n"
	    "  -b <exe>  Execute <exe> instead of <binary>, arg0 is <binary>\n"
	    "  -d        Ignore lack of exec permissions for the binary\n"
	    "  -f <FD>   Execute <FD> instead of searching for <binary>\n"
	    "  -o <OPT>=<VAL> Set LD_<OPT> to <VAL>, without polluting env\n"
	    "  -p        Search in PATH for named binary\n"
	    "  -u        Ignore LD_ environment variables\n"
	    "  -v        Display identification information\n"
	    "  --        End of RTLD options\n"
	    "  <binary>  Name of process to execute\n"
	    "  <args>    Arguments to the executed process\n",
	    argv0);
}

#define AUXFMT(at, xfmt) [at] = { .name = #at, .fmt = xfmt }
static const struct auxfmt {
	const char *name;
	const char *fmt;
} auxfmts[] = {
	AUXFMT(AT_NULL, NULL),
	AUXFMT(AT_IGNORE, NULL),
	AUXFMT(AT_EXECFD, "%ld"),
	AUXFMT(AT_PHDR, "%p"),
	AUXFMT(AT_PHENT, "%lu"),
	AUXFMT(AT_PHNUM, "%lu"),
	AUXFMT(AT_PAGESZ, "%lu"),
	AUXFMT(AT_BASE, "%#lx"),
	AUXFMT(AT_FLAGS, "%#lx"),
	AUXFMT(AT_ENTRY, "%p"),
	AUXFMT(AT_NOTELF, NULL),
	AUXFMT(AT_UID, "%ld"),
	AUXFMT(AT_EUID, "%ld"),
	AUXFMT(AT_GID, "%ld"),
	AUXFMT(AT_EGID, "%ld"),
	AUXFMT(AT_EXECPATH, "%s"),
	AUXFMT(AT_CANARY, "%p"),
	AUXFMT(AT_CANARYLEN, "%lu"),
	AUXFMT(AT_OSRELDATE, "%lu"),
	AUXFMT(AT_NCPUS, "%lu"),
	AUXFMT(AT_PAGESIZES, "%p"),
	AUXFMT(AT_PAGESIZESLEN, "%lu"),
	AUXFMT(AT_TIMEKEEP, "%p"),
	AUXFMT(AT_STACKPROT, "%#lx"),
	AUXFMT(AT_EHDRFLAGS, "%#lx"),
	AUXFMT(AT_HWCAP, "%#lx"),
	AUXFMT(AT_HWCAP2, "%#lx"),
	AUXFMT(AT_BSDFLAGS, "%#lx"),
	AUXFMT(AT_ARGC, "%lu"),
	AUXFMT(AT_ARGV, "%p"),
	AUXFMT(AT_ENVC, "%p"),
	AUXFMT(AT_ENVV, "%p"),
	AUXFMT(AT_PS_STRINGS, "%p"),
	AUXFMT(AT_FXRNG, "%p"),
	AUXFMT(AT_KPRELOAD, "%p"),
	AUXFMT(AT_USRSTACKBASE, "%#lx"),
	AUXFMT(AT_USRSTACKLIM, "%#lx"),
};

static bool
is_ptr_fmt(const char *fmt)
{
	char last;

	last = fmt[strlen(fmt) - 1];
	return (last == 'p' || last == 's');
}

static void
dump_auxv(Elf_Auxinfo **aux_info)
{
	Elf_Auxinfo *auxp;
	const struct auxfmt *fmt;
	int i;

	for (i = 0; i < AT_COUNT; i++) {
		auxp = aux_info[i];
		if (auxp == NULL)
			continue;
		fmt = &auxfmts[i];
		if (fmt->fmt == NULL)
			continue;
		rtld_fdprintf(STDOUT_FILENO, "%s:\t", fmt->name);
		if (is_ptr_fmt(fmt->fmt)) {
			rtld_fdprintfx(STDOUT_FILENO, fmt->fmt,
			    auxp->a_un.a_ptr);
		} else {
			rtld_fdprintfx(STDOUT_FILENO, fmt->fmt,
			    auxp->a_un.a_val);
		}
		rtld_fdprintf(STDOUT_FILENO, "\n");
	}
}

const char *
rtld_get_var(const char *name)
{
	const struct ld_env_var_desc *lvd;
	u_int i;

	for (i = 0; i < nitems(ld_env_vars); i++) {
		lvd = &ld_env_vars[i];
		if (strcmp(lvd->n, name) == 0)
			return (lvd->val);
	}
	return (NULL);
}

int
rtld_set_var(const char *name, const char *val)
{
	struct ld_env_var_desc *lvd;
	u_int i;

	for (i = 0; i < nitems(ld_env_vars); i++) {
		lvd = &ld_env_vars[i];
		if (strcmp(lvd->n, name) != 0)
			continue;
		if (!lvd->can_update || (lvd->unsecure && !trust))
			return (EPERM);
		if (lvd->owned)
			free(__DECONST(char *, lvd->val));
		if (val != NULL)
			lvd->val = xstrdup(val);
		else
			lvd->val = NULL;
		lvd->owned = true;
		if (lvd->debug)
			debug = lvd->val != NULL && *lvd->val != '\0';
		return (0);
	}
	return (ENOENT);
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
const char *
rtld_strerror(int errnum)
{
	if (errnum < 0 || errnum >= sys_nerr)
		return ("Unknown error");
	return (sys_errlist[errnum]);
}

char *
getenv(const char *name)
{
	return (__DECONST(char *, rtld_get_env_val(environ, name,
	    strlen(name))));
}

/* malloc */
void *
malloc(size_t nbytes)
{
	return (__crt_malloc(nbytes));
}

void *
calloc(size_t num, size_t size)
{
	return (__crt_calloc(num, size));
}

void
free(void *cp)
{
	__crt_free(cp);
}

void *
realloc(void *cp, size_t nbytes)
{
	return (__crt_realloc(cp, nbytes));
}

extern int _rtld_version__FreeBSD_version __exported;
int _rtld_version__FreeBSD_version = __FreeBSD_version;

extern char _rtld_version_laddr_offset __exported;
char _rtld_version_laddr_offset;

extern char _rtld_version_dlpi_tls_data __exported;
char _rtld_version_dlpi_tls_data;
