/*
 * $Id: pam_malloc.c,v 1.3 2000/12/04 19:02:34 baggins Exp $
 */

/*
 * This pair of files helps to locate memory leaks. It is a wrapper for
 * the malloc family of calls. (Actutally, it currently only deals
 * with calloc, malloc, realloc, free and exit)
 *
 * To use these functions the header "pam_malloc.h" must be included
 * in all parts of the code (that use the malloc functions) and this
 * file must be linked with the result. The pam_malloc_flags can be
 * set from another function and determine the level of logging.
 *
 * The output is via the macros defined in _pam_macros.h
 *
 * It is a debugging tool and should be turned off in released code.
 *
 * This suite was written by Andrew Morgan <morgan@parc.power.net> for
 * Linux-PAM.
 */

#ifndef DEBUG
#define DEBUG
#endif

#include "pam_private.h"

#include <security/pam_malloc.h>
#include <security/_pam_macros.h>

/* this must be done to stop infinite recursion! */
#undef malloc
#undef calloc
#undef free
#undef realloc
#undef exit

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * default debugging level
 */

int pam_malloc_flags = PAM_MALLOC_ALL;
int pam_malloc_delay_length = 4;

#define on(x) ((pam_malloc_flags&(x))==(x))

/*
 * the implementation
 */

static const char *last_fn=NULL;
static const char *last_file=NULL;
static const char *last_call=NULL;
static int last_line = 1;

#define err(x) { _pam_output_xdebug_info(); _pam_output_debug x ; }

static void set_last_(const char *x, const char *f
		      , const char *fn, const int l)
{
     last_fn   = x  ? x : "error-in-pam_malloc..";
     last_file = f  ? f : "*bad-file*";
     last_call = fn ? fn: "*bad-fn*";
     last_line = l;
}

static void _pam_output_xdebug_info(void)
{
    FILE *logfile;
    int must_close = 1, fd;

#ifdef O_NOFOLLOW
    if ((fd = open(_PAM_LOGFILE, O_WRONLY|O_NOFOLLOW|O_APPEND)) != -1) {
#else
    if ((fd = open(_PAM_LOGFILE, O_WRONLY|O_APPEND)) != -1) {
#endif
	if (!(logfile = fdopen(fd,"a"))) {
	    logfile = stderr;
	    must_close = 0;
	    close(fd);
	}
    } else {
	logfile = stderr;
	must_close = 0;
    }
    fprintf(logfile, "[%s:%s(%d)->%s()] ",
           last_file, last_call, last_line, last_fn);
    fflush(logfile);
    if (must_close)
        fclose(logfile);
}

static void hinder(void)
{
     if (on(PAM_MALLOC_PAUSE)) {
	  if (on(0)) err(("pause requested"));
	  sleep(pam_malloc_delay_length);
     }

     if (on(PAM_MALLOC_STOP)) {
	  if (on(0)) err(("stop requested"));
	  exit(1);
     }
}

/*
 * here are the memory pointer registering functions.. these actually
 * use malloc(!) but that's ok! ;^)
 */

struct reference {
     void *ptr;          /* pointer */
     int nelements;      /* number of elements */
     int size;           /* - each of this size */
     char *file;         /* where it was requested - filename */
     char *function;     /*                        - function */
     int line;           /*                        - line number */
/*
 * linking info
 */
     struct reference *next;
};

static void _dump(const char *say, const struct reference *ref)
{
    _pam_output_debug(" <%s: %p (#%d of %d) req. by %s(); %s line %d>\n"
		      , say
		      , ref->ptr,ref->nelements,ref->size
		      , ref->function,ref->file,ref->line);
}

static struct reference *root=NULL;

static char *_strdup(const char *x)
{
     char *s;

     s = (char *)malloc(strlen(x)+1);
     if (s == NULL) {
	  if (on(0)) err(("_strdup failed"));
	  exit(1);
     }

     strcpy(s,x);
     return s;
}

static void add_new_ref(void *new, int n, int size)
{
     struct reference *ref=NULL;

     ref = (struct reference *) malloc( sizeof(struct reference) );
     if (new == NULL || ref == NULL) {
	  if (on(0)) err(("internal error {add_new_ref}"));
	  exit(1);
     }

     ref->ptr = new;
     ref->nelements = n;
     ref->size = size;

     ref->file = _strdup(last_file);
     ref->function = _strdup(last_call);
     ref->line = last_line;

     ref->next = root;

     if (on(PAM_MALLOC_REQUEST)) {
	  _dump("new_ptr", ref);
     }

     root = ref;
}

static void del_old_ref(void *old)
{
     struct reference *this,*last;

     if (old == NULL) {
	  if (on(0)) err(("internal error {del_old_ref}"));
	  exit(1);
     }

     /* locate old pointer */

     last = NULL;
     this = root;
     while (this) {
	  if (this->ptr == old)
	       break;
	  last = this;
	  this = this->next;
     }

     /* Did we find a reference ? */

     if (this) {
	  if (on(PAM_MALLOC_FREE)) {
	       _dump("free old_ptr", this);
	  }
	  if (last == NULL) {
	       root = this->next;
	  } else {
	       last->next = this->next;
	  }
	  free(this->file);
	  free(this->function);
	  free(this);
     } else {
	  if (on(0)) err(("ERROR!: bad memory"));
	  hinder();
     }
}

static void verify_old_ref(void *old)
{
     struct reference *this;

     if (old == NULL) {
	  if (on(0)) err(("internal error {verify_old_ref}"));
	  exit(1);
     }

     /* locate old pointer */

     this = root;
     while (this) {
	  if (this->ptr == old)
	       break;
	  this = this->next;
     }

     /* Did we find a reference ? */

     if (this) {
	  if (on(PAM_MALLOC_VERIFY)) {
	       _dump("verify_ptr", this);
	  }
     } else {
	  if (on(0)) err(("ERROR!: bad request"));
	  hinder();
     }
}

static void dump_memory_list(const char *dump)
{
     struct reference *this;

     this = root;
     if (this) {
	  if (on(0)) err(("un-free()'d memory"));
	  while (this) {
	       _dump(dump, this);
	       this = this->next;
	  }
     } else {
	  if (on(0)) err(("no memory allocated"));
     }
}

/* now for the wrappers */

#define _fn(x)  set_last_(x,file,fn,line)

void *pam_malloc(size_t size, const char *file, const char *fn, const int line)
{
     void *new;

     _fn("malloc");

     if (on(PAM_MALLOC_FUNC)) err(("request for %d", size));

     new = malloc(size);
     if (new == NULL) {
	  if (on(PAM_MALLOC_FAIL)) err(("returned NULL"));
     } else {
	  if (on(PAM_MALLOC_REQUEST)) err(("request new"));
	  add_new_ref(new, 1, size);
     }

     return new;
}

void *pam_calloc(size_t nelm, size_t size
		, const char *file, const char *fn, const int line)
{
     void *new;

     _fn("calloc");

     if (on(PAM_MALLOC_FUNC)) err(("request for %d of %d", nelm, size));

     new = calloc(nelm,size);
     if (new == NULL) {
	  if (on(PAM_MALLOC_FAIL)) err(("returned NULL"));
     } else {
	  if (on(PAM_MALLOC_REQUEST)) err(("request new"));
	  add_new_ref(new, nelm, size);
     }

     return new;
}

void  pam_free(void *ptr
	      , const char *file, const char *fn, const int line)
{
     _fn("free");

     if (on(PAM_MALLOC_FUNC)) err(("request to free %p", ptr));

     if (ptr == NULL) {
	  if (on(PAM_MALLOC_NULL)) err(("passed NULL pointer"));
     } else {
	  if (on(PAM_MALLOC_FREE)) err(("deleted old"));
	  del_old_ref(ptr);
	  free(ptr);
     }
}

void *pam_memalign(size_t ali, size_t size
		  , const char *file, const char *fn, const int line)
{
     _fn("memalign");
     if (on(0)) err(("not implemented currently (Sorry)"));
     exit(1);
}

void *pam_realloc(void *ptr, size_t size
		, const char *file, const char *fn, const int line)
{
     void *new;

     _fn("realloc");

     if (on(PAM_MALLOC_FUNC)) err(("resize %p to %d", ptr, size));

     if (ptr == NULL) {
	  if (on(PAM_MALLOC_NULL)) err(("passed NULL pointer"));
     } else {
	  verify_old_ref(ptr);
     }

     new = realloc(ptr, size);
     if (new == NULL) {
	  if (on(PAM_MALLOC_FAIL)) err(("returned NULL"));
     } else {
	  if (ptr) {
	       if (on(PAM_MALLOC_FREE)) err(("deleted old"));
	       del_old_ref(ptr);
	  } else {
	       if (on(PAM_MALLOC_NULL)) err(("old is NULL"));
	  }
	  if (on(PAM_MALLOC_REQUEST)) err(("request new"));
	  add_new_ref(new, 1, size);
     }

     return new;
}

void *pam_valloc(size_t size
		, const char *file, const char *fn, const int line)
{
     _fn("valloc");
     if (on(0)) err(("not implemented currently (Sorry)"));
     exit(1);
}

#include <alloca.h>

void *pam_alloca(size_t size
		, const char *file, const char *fn, const int line)
{
     _fn("alloca");
     if (on(0)) err(("not implemented currently (Sorry)"));
     exit(1);
}

void pam_exit(int i
	      , const char *file, const char *fn, const int line)
{
     _fn("exit");

     if (on(0)) err(("passed (%d)", i));
     if (on(PAM_MALLOC_LEAKED)) {
	  dump_memory_list("leaked");
     }
     exit(i);
}

/* end of file */
