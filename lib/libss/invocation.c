/*
 * Copyright 1987, 1988 by MIT Student Information Processing Board
 *
 * For copyright information, see copyright.h.
 */

#include <stdlib.h>
#include "ss_internal.h"
#include "copyright.h"
#define	size	sizeof(ss_data *)

#ifndef lint
static char const rcsid[] =
    "Header: invocation.c,v 1.6 89/01/25 07:45:47 raeburn Exp ";
#endif

int ss_create_invocation(subsystem_name, version_string, info_ptr,
			 request_table_ptr, code_ptr)
	char *subsystem_name, *version_string;
	char *info_ptr;
	ss_request_table *request_table_ptr;
	int *code_ptr;
{
	register int sci_idx;
	register ss_data *new_table;
	register ss_data **table;

	*code_ptr = 0;
	table = _ss_table;
	new_table = (ss_data *) malloc(sizeof(ss_data));

	if (table == (ss_data **) NULL) {
		table = (ss_data **) malloc(2 * size);
		table[0] = table[1] = (ss_data *)NULL;
	}
	initialize_ss_error_table ();

	for (sci_idx = 1; table[sci_idx] != (ss_data *)NULL; sci_idx++)
		;
	table = (ss_data **) realloc((char *)table,
				     ((unsigned)sci_idx+2)*size);
	table[sci_idx+1] = (ss_data *) NULL;
	table[sci_idx] = new_table;

	new_table->subsystem_name = subsystem_name;
	new_table->subsystem_version = version_string;
	new_table->argv = (char **)NULL;
	new_table->current_request = (char *)NULL;
	new_table->info_dirs = (char **)malloc(sizeof(char *));
	*new_table->info_dirs = (char *)NULL;
	new_table->info_ptr = info_ptr;
	new_table->prompt = malloc((unsigned)strlen(subsystem_name)+4);
	strcpy(new_table->prompt, subsystem_name);
	strcat(new_table->prompt, ":  ");
#ifdef silly
	new_table->abbrev_info = ss_abbrev_initialize("/etc/passwd", code_ptr);
#else
	new_table->abbrev_info = NULL;
#endif
	new_table->flags.escape_disabled = 0;
	new_table->flags.abbrevs_disabled = 0;
	new_table->rqt_tables =
		(ss_request_table **) calloc(2, sizeof(ss_request_table *));
	*(new_table->rqt_tables) = request_table_ptr;
	*(new_table->rqt_tables+1) = (ss_request_table *) NULL;
	_ss_table = table;
	return(sci_idx);
}

void
ss_delete_invocation(sci_idx)
	int sci_idx;
{
	register ss_data *t;
	int ignored_code;

	t = ss_info(sci_idx);
	free(t->prompt);
	free((char *)t->rqt_tables);
	while(t->info_dirs[0] != (char *)NULL)
		ss_delete_info_dir(sci_idx, t->info_dirs[0], &ignored_code);
	free((char *)t->info_dirs);
	free((char *)t);
}
