/* pam_data.c */

/*
 * $Id: pam_data.c,v 1.2 2001/01/22 06:07:28 agmorgan Exp $
 */

#include <stdlib.h>
#include <string.h>

#include "pam_private.h"

static struct pam_data *_pam_locate_data(const pam_handle_t *pamh,
					 const char *name)
{
    struct pam_data *data;

    D(("called"));

    IF_NO_PAMH("_pam_locate_data", pamh, NULL);

    data = pamh->data;
    
    while (data) {
	if (!strcmp(data->name, name)) {
	    return data;
	}
	data = data->next;
    }

    return NULL;
}

int pam_set_data(
    pam_handle_t *pamh,
    const char *module_data_name,
    void *data,
    void (*cleanup)(pam_handle_t *pamh, void *data, int error_status))
{
    struct pam_data *data_entry;
    
    D(("called"));

    IF_NO_PAMH("pam_set_data", pamh, PAM_SYSTEM_ERR);

    if (__PAM_FROM_APP(pamh)) {
	D(("called from application!?"));
	return PAM_SYSTEM_ERR;
    }

    /* first check if there is some data already. If so clean it up */

    if ((data_entry = _pam_locate_data(pamh, module_data_name))) {
	if (data_entry->cleanup) {
	    data_entry->cleanup(pamh, data_entry->data,
				PAM_DATA_REPLACE | PAM_SUCCESS );
	}
    } else if ((data_entry = malloc(sizeof(*data_entry)))) {
	char *tname;

	if ((tname = _pam_strdup(module_data_name)) == NULL) {
	    _pam_system_log(LOG_CRIT, "pam_set_data: no memory for data name");
	    _pam_drop(data_entry);
	    return PAM_BUF_ERR;
	}
	data_entry->next = pamh->data;
	pamh->data = data_entry;
	data_entry->name = tname;
    } else {
	_pam_system_log(LOG_CRIT, "pam_set_data: cannot allocate data entry");
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

    D(("called"));

    IF_NO_PAMH("pam_get_data", pamh, PAM_SYSTEM_ERR);

    if (__PAM_FROM_APP(pamh)) {
	D(("called from application!?"));
	return PAM_SYSTEM_ERR;
    }

    data = _pam_locate_data(pamh, module_data_name);
    if (data) {
	*datap = data->data;
	return PAM_SUCCESS;
    }

    return PAM_NO_MODULE_DATA;
}

void _pam_free_data(pam_handle_t *pamh, int status)
{
    struct pam_data *last;
    struct pam_data *data;

    D(("called"));

    IF_NO_PAMH("_pam_free_data", pamh, /* no return value for void fn */);
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
