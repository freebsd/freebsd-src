/*
 *  Top users/processes display for Unix
 *  Version 3
 *
 *  This program may be freely redistributed,
 *  but this entire comment MUST remain intact.
 *
 *  Copyright (c) 1984, 1989, William LeFebvre, Rice University
 *  Copyright (c) 1989, 1990, 1992, William LeFebvre, Northwestern University
 */

/* prototypes for functions found in utils.c */

int atoiwi(char *str);
char *itoa(int);
char *itoa7(int);
int digits(int);
char *strecpy(char *to, char *from);
char **argparse(char *line, int *cntp);
long percentages(int cnt, int *out, long *new_, long *old, long *diffs);
char *errmsg(int);
char *format_time(long seconds);
char *format_k(int amt);
char *format_k2(unsigned long long);
int string_index(char *string, char **array);

