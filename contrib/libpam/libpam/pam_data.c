/* pam_data.c */

/*
 * $Id: pam_data.c,v 1.5 1996/12/01 03:14:13 morgan Exp $
 * $FreeBSD$
 *
 * $Log: pam_data.c,v $
 * Revision 1.5  1996/12/01 03:14:13  morgan
 * use _pam_macros.h
 *
 * Revision 1.4  1996/11/10 19:59:56  morgan
 * internalized strdup for malloc debugging
 *
 * Revision 1.3  1996/09/05 06:10:31  morgan
 * changed type of cleanup(), added PAM_DATA_REPLACE to replacement
 * cleanup() call.
 *
 * Revision 1.2  1996/03/16 21:33:05  morgan
 * removed const from cleanup argument, also deleted comment about SUN stuff
 *
 *
 */

#include <stdlib.h>
#include <string.h>

#include "pam_private.h"

struct pam_data *_pam_locate_data(const pam_handle_t *pamh, const char *name);

int pam_set_data(
    pam_handle_t *pamh,
    const char *module_data_name,
    void *data,
    void (*cleanup)(pam_handle_t *pamh, void *data, int error_status))
{
    struct pam_data *data_entry;
    
    IF_NO_PAMH("pam_set_data",pamh,PAM_SYSTEM_ERR);

    /* first check if there is some data already. If so clean it up */

    if ((data_entry = _pam_locate_data(pamh, module_data_name))) {
	if (data_entry->cleanup) {
	    data_entry->cleanup(pamh, data_entry->data
				, PAM_DATA_REPLACE | PAM_SUCCESS );
	}
    } else if ((data_entry = malloc(sizeof(*data_entry)))) {
	char *tname;

	if ((tname = _pam_strdup(module_data_name)) == NULL) {
	    pam_system_log(pamh, NULL, LOG_CRIT,
			   "pam_set_data: no memory for data name");
	    _pam_drop(data_entry);
	    return PAM_BUF_ERR;
	}
	data_entry->next = pamh->data;
	pamh->data = data_entry;
	data_entry->name = tname;
    } else {
	pam_system_log(pamh, NULL, LOG_CRIT,
		       "pam_set_data: cannot allocate data entry");
	return PAM_BUF_ERR;
    }

    data_entry->data = data;           /* note this could be NULL */
    data_entry->cleanup = cleanup;

    return PAM_SUCCESS;
}

int pam_get_data(
    const pam_handle_t *pamh,
    const char *module_data_name,
    const void **datap)
{
    struct pam_data *data;

    IF_NO_PAMH("pam_get_data",pamh,PAM_SYSTEM_ERR);

    data = _pam_locate_data(pamh, module_data_name);
    if (data) {
	*datap = data->data;
	return PAM_SUCCESS;
    }

    return PAM_NO_MODULE_DATA;
}

struct pam_data *_pam_locate_data(const pam_handle_t *pamh, const char *name)
{
    struct pam_data *data;

    IF_NO_PAMH("_pam_locate_data",pamh,NULL);
    data = pamh->data;
    
    while (data) {
	if (!strcmp(data->name, name)) {
	    return data;
	}
	data = data->next;
    }

    return NULL;
}

void _pam_free_data(pam_handle_t *pamh, int status)
{
    struct pam_data *last;
    struct pam_data *data;

    IF_NO_PAMH("_pam_free_data",pamh,/* no return value for void fn */);
    data = pamh->data;

    while (data) {
	last = data;
	data = data->next;
	if (last->cleanup) {
	    last->cleanup(pamh, last->data, status);
	}
	_pam_drop(last->name);
	_pam_drop(last);
    }
}
