/*  $Revision: 1.4 $
**
**  Internal header file for editline library.
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CRLF		"\r\n"

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_DIRENT_H
#include <dirent.h>
typedef struct dirent	DIRENTRY;
#else
#include <sys/dir.h>
typedef struct direct	DIRENTRY;
#endif

#include <roken.h>

#if	!defined(S_ISDIR)
#define S_ISDIR(m)		(((m) & S_IFMT) == S_IFDIR)
#endif	/* !defined(S_ISDIR) */

typedef unsigned char	CHAR;

#define MEM_INC		64
#define SCREEN_INC	256

/*
**  Variables and routines internal to this package.
*/
extern int	rl_eof;
extern int	rl_erase;
extern int	rl_intr;
extern int	rl_kill;
extern int	rl_quit;

typedef char* (*rl_complete_func_t)(char*, int*);

typedef int (*rl_list_possib_func_t)(char*, char***);

void	add_history (char*);
char*	readline (const char* prompt);
void	rl_add_slash (char*, char*);
char*	rl_complete (char*, int*);
void	rl_initialize (void);
int	rl_list_possib (char*, char***);
void	rl_reset_terminal (char*);
void	rl_ttyset (int);
rl_complete_func_t	rl_set_complete_func (rl_complete_func_t);
rl_list_possib_func_t	rl_set_list_possib_func (rl_list_possib_func_t);
 
