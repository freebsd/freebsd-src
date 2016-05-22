/* constants needed for display.c */

/* "type" argument for new_message function */

#define  MT_standout  1
#define  MT_delayed   2

#include "machine.h"

int		 display_updatecpus(struct statics *statics);
void	 clear_message(void);
int		 display_resize(void);
void	 i_header(char *text);
char	*printable(char *string);
char	*cpustates_tag(void);
void	 display_header(int t);
int		 display_init(struct statics *statics);
void	 i_arc(int *stats);
void	 i_cpustates(int *states);
void	 i_loadave(int mpid, double *avenrun);
void	 i_memory(int *stats);
void	 i_message(void);
void	 i_process(int line, char *thisline);
void	 i_procstates(int total, int *brkdn);
void	 i_swap(int *stats);
void	 i_timeofday(time_t *tod);
void	 i_uptime(struct timeval *bt, time_t *tod);
void	 new_message();
int		 readline(char *buffer, int size, int numeric);
char	*trim_header(char *text);
void	 u_arc(int *stats);
void	 u_cpustates(int *states);
void	 u_endscreen(int hi);
void	 u_header(char *text);
void	 u_loadave(int mpid, double *avenrun);
void	 u_memory(int *stats);
void	 u_message(void);
void	 u_process(int line, char *newline);
void	 u_procstates(int total, int *brkdn);
void	 u_swap(int *stats);
void	 z_cpustates(void);
