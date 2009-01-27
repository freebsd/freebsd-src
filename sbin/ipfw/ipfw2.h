/*
 * Copyright (c) 2002-2003 Luigi Rizzo
 * Copyright (c) 1996 Alex Nash, Paul Traina, Poul-Henning Kamp
 * Copyright (c) 1994 Ugen J.S.Antsilevich
 *
 * Idea and grammar partially left from:
 * Copyright (c) 1993 Daniel Boulet
 *
 * Redistribution and use in source forms, with and without modification,
 * are permitted provided that this entire comment appears intact.
 *
 * Redistribution in binary form may occur without any restrictions.
 * Obviously, it would be nice if you gave credit where credit is due
 * but requiring it would be too onerous.
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 *
 * NEW command line interface for IP firewall facility
 *
 * $FreeBSD$
 */

/*
 * Options that can be set on the command line.
 * When reading commands from a file, a subset of the options can also
 * be applied globally by specifying them before the file name.
 * After that, each line can contain its own option that changes
 * the global value.
 * XXX The context is not restored after each line.
 */

struct cmdline_opts {
	/* boolean options: */
	int	do_value_as_ip;	/* show table value as IP */
	int	do_resolv;	/* try to resolve all ip to names */
	int	do_time;	/* Show time stamps */
	int	do_quiet;	/* Be quiet in add and flush */
	int	do_pipe;	/* this cmd refers to a pipe */
	int	do_nat; 	/* this cmd refers to a nat config */
	int	do_dynamic;	/* display dynamic rules */
	int	do_expired;	/* display expired dynamic rules */
	int	do_compact;	/* show rules in compact mode */
	int	do_force;	/* do not ask for confirmation */
	int	show_sets;	/* display the set each rule belongs to */
	int	test_only;	/* only check syntax */
	int	comment_only;	/* only print action and comment */
	int	verbose;	/* be verbose on some commands */

	/* The options below can have multiple values. */

	int	do_sort;	/* field to sort results (0 = no) */
		/* valid fields are 1 and above */

	int	use_set;	/* work with specified set number */
		/* 0 means all sets, otherwise apply to set use_set - 1 */

};

extern struct cmdline_opts co;

/*
 * _s_x is a structure that stores a string <-> token pairs, used in
 * various places in the parser. Entries are stored in arrays,
 * with an entry with s=NULL as terminator.
 * The search routines are match_token() and match_value().
 * Often, an element with x=0 contains an error string.
 *
 */
struct _s_x {
	char const *s;
	int x;
};

/*
 * the following macro returns an error message if we run out of
 * arguments.
 */
#define NEED1(msg)      {if (!ac) errx(EX_USAGE, msg);}

/* memory allocation support */
void *safe_calloc(size_t number, size_t size);
void *safe_realloc(void *ptr, size_t size);

/* a string comparison function used for historical compatibility */
int _substrcmp(const char *str1, const char* str2);

/*
 * The reserved set numer. This is a constant in ip_fw.h
 * but we store it in a variable so other files do not depend
 * in that header just for one constant.
 */
extern int resvd_set_number;

void ipfw_add(int ac, char *av[]);
void ipfw_show_nat(int ac, char **av);
void ipfw_config_pipe(int ac, char **av);
void ipfw_config_nat(int ac, char **av);
void ipfw_sets_handler(int ac, char *av[]);
void ipfw_table_handler(int ac, char *av[]);
void ipfw_sysctl_handler(int ac, char *av[], int which);
void ipfw_delete(int ac, char *av[]);
void ipfw_flush(int force);
void ipfw_zero(int ac, char *av[], int optname);
void ipfw_list(int ac, char *av[], int show_counters);

