/*
 * Copyright (c) 2000 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID ("$Id: rtbl.c,v 1.3 2000/07/20 14:42:31 assar Exp $");
#endif
#include "roken.h"
#include "rtbl.h"

struct column_entry {
    char *data;
};

struct column_data {
    char *header;
    char *prefix;
    int width;
    unsigned flags;
    size_t num_rows;
    struct column_entry *rows;
};

struct rtbl_data {
    char *column_prefix;
    size_t num_columns;
    struct column_data **columns;
};

rtbl_t
rtbl_create (void)
{
    return calloc (1, sizeof (struct rtbl_data));
}

static struct column_data *
rtbl_get_column (rtbl_t table, const char *column)
{
    int i;
    for(i = 0; i < table->num_columns; i++)
	if(strcmp(table->columns[i]->header, column) == 0)
	    return table->columns[i];
    return NULL;
}

void
rtbl_destroy (rtbl_t table)
{
    int i, j;

    for (i = 0; i < table->num_columns; i++) {
	struct column_data *c = table->columns[i];

	for (j = 0; j < c->num_rows; j++)
	    free (c->rows[j].data);
	free (c->header);
	free (c->prefix);
	free (c);
    }
    free (table->column_prefix);
    free (table->columns);
}

int
rtbl_add_column (rtbl_t table, const char *header, unsigned int flags)
{
    struct column_data *col, **tmp;

    tmp = realloc (table->columns, (table->num_columns + 1) * sizeof (*tmp));
    if (tmp == NULL)
	return ENOMEM;
    table->columns = tmp;
    col = malloc (sizeof (*col));
    if (col == NULL)
	return ENOMEM;
    col->header = strdup (header);
    if (col->header == NULL) {
	free (col);
	return ENOMEM;
    }
    col->prefix   = NULL;
    col->width    = 0;
    col->flags    = flags;
    col->num_rows = 0;
    col->rows     = NULL;
    table->columns[table->num_columns++] = col;
    return 0;
}

static void
column_compute_width (struct column_data *column)
{
    int i;

    column->width = strlen (column->header);
    for (i = 0; i < column->num_rows; i++)
	column->width = max (column->width, strlen (column->rows[i].data));
}

int
rtbl_set_prefix (rtbl_t table, const char *prefix)
{
    if (table->column_prefix)
	free (table->column_prefix);
    table->column_prefix = strdup (prefix);
    if (table->column_prefix == NULL)
	return ENOMEM;
    return 0;
}

int
rtbl_set_column_prefix (rtbl_t table, const char *column,
			const char *prefix)
{
    struct column_data *c = rtbl_get_column (table, column);

    if (c == NULL)
	return -1;
    if (c->prefix)
	free (c->prefix);
    c->prefix = strdup (prefix);
    if (c->prefix == NULL)
	return ENOMEM;
    return 0;
}


static const char *
get_column_prefix (rtbl_t table, struct column_data *c)
{
    if (c == NULL)
	return "";
    if (c->prefix)
	return c->prefix;
    if (table->column_prefix)
	return table->column_prefix;
    return "";
}

int
rtbl_add_column_entry (rtbl_t table, const char *column, const char *data)
{
    struct column_entry row, *tmp;

    struct column_data *c = rtbl_get_column (table, column);

    if (c == NULL)
	return -1;

    row.data = strdup (data);
    if (row.data == NULL)
	return ENOMEM;
    tmp = realloc (c->rows, (c->num_rows + 1) * sizeof (*tmp));
    if (tmp == NULL) {
	free (row.data);
	return ENOMEM;
    }
    c->rows = tmp;
    c->rows[c->num_rows++] = row;
    return 0;
}

int
rtbl_format (rtbl_t table, FILE * f)
{
    int i, j;

    for (i = 0; i < table->num_columns; i++)
	column_compute_width (table->columns[i]);
    for (i = 0; i < table->num_columns; i++) {
	struct column_data *c = table->columns[i];

	fprintf (f, "%s", get_column_prefix (table, c));
	fprintf (f, "%-*s", (int)c->width, c->header);
    }
    fprintf (f, "\n");

    for (j = 0;; j++) {
	int flag = 0;

	for (i = 0; flag == 0 && i < table->num_columns; ++i) {
	    struct column_data *c = table->columns[i];

	    if (c->num_rows > j) {
		++flag;
		break;
	    }
	}
	if (flag == 0)
	    break;

	for (i = 0; i < table->num_columns; i++) {
	    int w;
	    struct column_data *c = table->columns[i];

	    w = c->width;

	    if ((c->flags & RTBL_ALIGN_RIGHT) == 0)
		w = -w;
	    fprintf (f, "%s", get_column_prefix (table, c));
	    if (c->num_rows <= j)
		fprintf (f, "%*s", w, "");
	    else
		fprintf (f, "%*s", w, c->rows[j].data);
	}
	fprintf (f, "\n");
    }
    return 0;
}

#ifdef TEST
int
main (int argc, char **argv)
{
    rtbl_t table;
    unsigned int a, b, c, d;

    table = rtbl_create ();
    rtbl_add_column (table, "Issued", 0, &a);
    rtbl_add_column (table, "Expires", 0, &b);
    rtbl_add_column (table, "Foo", RTBL_ALIGN_RIGHT, &d);
    rtbl_add_column (table, "Principal", 0, &c);

    rtbl_add_column_entry (table, a, "Jul  7 21:19:29");
    rtbl_add_column_entry (table, b, "Jul  8 07:19:29");
    rtbl_add_column_entry (table, d, "73");
    rtbl_add_column_entry (table, d, "0");
    rtbl_add_column_entry (table, d, "-2000");
    rtbl_add_column_entry (table, c, "krbtgt/NADA.KTH.SE@NADA.KTH.SE");

    rtbl_add_column_entry (table, a, "Jul  7 21:19:29");
    rtbl_add_column_entry (table, b, "Jul  8 07:19:29");
    rtbl_add_column_entry (table, c, "afs/pdc.kth.se@NADA.KTH.SE");

    rtbl_add_column_entry (table, a, "Jul  7 21:19:29");
    rtbl_add_column_entry (table, b, "Jul  8 07:19:29");
    rtbl_add_column_entry (table, c, "afs@NADA.KTH.SE");

    rtbl_set_prefix (table, "  ");
    rtbl_set_column_prefix (table, a, "");

    rtbl_format (table, stdout);

    rtbl_destroy (table);

}

#endif
