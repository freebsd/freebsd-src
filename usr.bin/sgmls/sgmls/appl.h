/* appl.h */

enum {
     E_NOMEM = 1,
     E_DOC,
     E_EXEC,
     E_FORK,
     E_WAIT,
     E_SIGNAL,
     E_OPEN,
     E_CAPBOTCH,
     E_SUBDOC
};

VOID process_document P((int));
VOID output_conforming P((void));

UNIV xmalloc P((UNS));
UNIV xrealloc P((UNIV, UNS));
VOID appl_error VP((int, ...));

#ifdef SUPPORT_SUBDOC
int run_process P((char **));
char **make_argv P((UNIV));
VOID get_subcaps P((void));
#endif

#ifdef SUPPORT_SUBDOC
extern int suberr;
#endif

extern int suppsw;
extern int locsw;
