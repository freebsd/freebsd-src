/*
 * Copyright 2009 Paul Blokus <paul_pl@users.sourceforge.net>
 * Copyright 2013 Michael Drake <tlsa@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file
 * SSL Certificate verification UI (implementation)
 */

#include <assert.h>
#include <stdlib.h>

#include "content/fetch.h"
#include "content/urldb.h"
#include "content/hlcache.h"
#include "desktop/sslcert_viewer.h"
#include "desktop/treeview.h"
#include "utils/messages.h"
#include "utils/log.h"
#include "utils/utils.h"

enum sslcert_viewer_field {
	SSLCERT_V_SUBJECT,
	SSLCERT_V_SERIAL,
	SSLCERT_V_TYPE,
	SSLCERT_V_VALID_UNTIL,
	SSLCERT_V_VALID_FROM,
	SSLCERT_V_VERSION,
	SSLCERT_V_ISSUER,
	SSLCERT_V_CERTIFICATES,
	SSLCERT_V_N_FIELDS
};

/** ssl certificate verification context. */
struct sslcert_session_data {
	const struct ssl_cert_info *certs; /**< Certificates */
	unsigned long num;		/**< Number of certificates in chain */
	nsurl *url;			/**< The url of the certificate */
	llcache_query_response cb;	/**< Cert accept/reject callback */
	void *cbpw;			/**< Context passed to callback */

	treeview *tree;			/**< The treeview object */
	struct treeview_field_desc fields[SSLCERT_V_N_FIELDS];
};

struct sslcert_entry {
	treeview_node *entry;
	char version[24];
	char serial[24];
	char type[24];
	struct treeview_field_data data[SSLCERT_V_N_FIELDS - 1];
};


/**
 * Free a sll certificate viewer entry's treeview field data.
 *
 * \param e		Entry to free data from
 */
static void sslcert_viewer_free_treeview_field_data(
		struct sslcert_entry *e)
{
}


/**
 * Build a sslcert viewer treeview field from given text
 *
 * \param field		SSL certificate treeview field to build
 * \param data		SSL certificate entry field data to set
 * \param value		Text to set in field, ownership yielded
 * \param ssl_d		SSL certificate session data
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static inline nserror sslcert_viewer_field_builder(
		enum sslcert_viewer_field field,
		struct treeview_field_data *data,
		const char *value,
		struct sslcert_session_data *ssl_d)
{
	data->field = ssl_d->fields[field].field;
	data->value = value;
	data->value_len = (value != NULL) ? strlen(value) : 0;

	return NSERROR_OK;
}


/**
 * Set a sslcert viewer entry's data from the certificate.
 *
 * \param e		Entry to set up
 * \param cert		Data associated with entry's certificate
 * \param ssl_d		SSL certificate session data
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static nserror sslcert_viewer_set_treeview_field_data(
		struct sslcert_entry *e,
		const struct ssl_cert_info *cert,
		struct sslcert_session_data *ssl_d)
{
	unsigned int written;

	assert(e != NULL);
	assert(cert != NULL);
	assert(ssl_d != NULL);

	/* Set the fields up */
	sslcert_viewer_field_builder(SSLCERT_V_SUBJECT,
			&e->data[SSLCERT_V_SUBJECT],
			cert->subject, ssl_d);

	written = snprintf(e->serial, sizeof(e->serial), "%li", cert->serial);
	assert(written < sizeof(e->serial));
	sslcert_viewer_field_builder(SSLCERT_V_SERIAL,
			&e->data[SSLCERT_V_SERIAL],
			e->serial, ssl_d);

	written = snprintf(e->type, sizeof(e->type), "%i", cert->cert_type);
	assert(written < sizeof(e->type));
	sslcert_viewer_field_builder(SSLCERT_V_TYPE,
			&e->data[SSLCERT_V_TYPE],
			e->type, ssl_d);

	sslcert_viewer_field_builder(SSLCERT_V_VALID_UNTIL,
			&e->data[SSLCERT_V_VALID_UNTIL],
			cert->not_after, ssl_d);

	sslcert_viewer_field_builder(SSLCERT_V_VALID_FROM,
			&e->data[SSLCERT_V_VALID_FROM],
			cert->not_before, ssl_d);

	written = snprintf(e->version, sizeof(e->version),
			"%li", cert->version);
	assert(written < sizeof(e->version));
	sslcert_viewer_field_builder(SSLCERT_V_VERSION,
			&e->data[SSLCERT_V_VERSION],
			e->version, ssl_d);

	sslcert_viewer_field_builder(SSLCERT_V_ISSUER,
			&e->data[SSLCERT_V_ISSUER],
			cert->issuer, ssl_d);

	return NSERROR_OK;
}


/**
 * Create a treeview node for a certificate
 *
 * \param ssl_d		SSL certificate session data
 * \param n		Number of SSL certificate in chain, to make node for
 * \return true on success, false on memory exhaustion
 */
static nserror sslcert_viewer_create_node(
		struct sslcert_session_data *ssl_d, int n)
{
	struct sslcert_entry *e;
	const struct ssl_cert_info *cert = &(ssl_d->certs[n]);
	nserror err;

	/* Create new certificate viewer entry */
	e = malloc(sizeof(struct sslcert_entry));
	if (e == NULL) {
		return NSERROR_NOMEM;
	}

	err = sslcert_viewer_set_treeview_field_data(e, cert, ssl_d);
	if (err != NSERROR_OK) {
		free(e);
		return err;
	}

	/* Create the new treeview node */
	err = treeview_create_node_entry(ssl_d->tree, &(e->entry),
			NULL, TREE_REL_FIRST_CHILD,
			e->data, e, TREE_OPTION_NONE);
	if (err != NSERROR_OK) {
		sslcert_viewer_free_treeview_field_data(e);
		free(e);
		return err;
	}

	return NSERROR_OK;
}


/**
 * Initialise the treeview entry feilds
 *
 * \param ssl_d		SSL certificate session data
 * \return true on success, false on memory exhaustion
 */
static nserror sslcert_init_entry_fields(struct sslcert_session_data *ssl_d)
{
	int i;
	const char *label;

	for (i = 0; i < SSLCERT_V_N_FIELDS; i++)
		ssl_d->fields[i].field = NULL;

	ssl_d->fields[SSLCERT_V_SUBJECT].flags = TREE_FLAG_DEFAULT;
	label = "TreeviewLabelSubject";
	label = messages_get(label);
	if (lwc_intern_string(label, strlen(label),
			&ssl_d->fields[SSLCERT_V_SUBJECT].field) !=
			lwc_error_ok) {
		goto error;
	}

	ssl_d->fields[SSLCERT_V_SERIAL].flags = TREE_FLAG_SHOW_NAME;
	label = "TreeviewLabelSerial";
	label = messages_get(label);
	if (lwc_intern_string(label, strlen(label),
			&ssl_d->fields[SSLCERT_V_SERIAL].field) !=
			lwc_error_ok) {
		goto error;
	}

	ssl_d->fields[SSLCERT_V_TYPE].flags = TREE_FLAG_SHOW_NAME;
	label = "TreeviewLabelType";
	label = messages_get(label);
	if (lwc_intern_string(label, strlen(label),
			&ssl_d->fields[SSLCERT_V_TYPE].field) !=
			lwc_error_ok) {
		goto error;
	}

	ssl_d->fields[SSLCERT_V_VALID_UNTIL].flags = TREE_FLAG_SHOW_NAME;
	label = "TreeviewLabelValidUntil";
	label = messages_get(label);
	if (lwc_intern_string(label, strlen(label),
			&ssl_d->fields[SSLCERT_V_VALID_UNTIL].field) !=
			lwc_error_ok) {
		goto error;
	}

	ssl_d->fields[SSLCERT_V_VALID_FROM].flags = TREE_FLAG_SHOW_NAME;
	label = "TreeviewLabelValidFrom";
	label = messages_get(label);
	if (lwc_intern_string(label, strlen(label),
			&ssl_d->fields[SSLCERT_V_VALID_FROM].field) !=
			lwc_error_ok) {
		goto error;
	}

	ssl_d->fields[SSLCERT_V_VERSION].flags = TREE_FLAG_SHOW_NAME;
	label = "TreeviewLabelVersion";
	label = messages_get(label);
	if (lwc_intern_string(label, strlen(label),
			&ssl_d->fields[SSLCERT_V_VERSION].field) !=
			lwc_error_ok) {
		goto error;
	}

	ssl_d->fields[SSLCERT_V_ISSUER].flags = TREE_FLAG_SHOW_NAME;
	label = "TreeviewLabelIssuer";
	label = messages_get(label);
	if (lwc_intern_string(label, strlen(label),
			&ssl_d->fields[SSLCERT_V_ISSUER].field) !=
			lwc_error_ok) {
		goto error;
	}

	ssl_d->fields[SSLCERT_V_CERTIFICATES].flags = TREE_FLAG_DEFAULT;
	label = "TreeviewLabelCertificates";
	label = messages_get(label);
	if (lwc_intern_string(label, strlen(label),
			&ssl_d->fields[SSLCERT_V_CERTIFICATES].field) !=
			lwc_error_ok) {
		return false;
	}

	return NSERROR_OK;

error:
	for (i = 0; i < SSLCERT_V_N_FIELDS; i++)
		if (ssl_d->fields[i].field != NULL)
			lwc_string_unref(ssl_d->fields[i].field);

	return NSERROR_UNKNOWN;
}


/**
 * Delete ssl certificate viewer entries
 *
 * \param e		Entry to delete.
 */
static void sslcert_viewer_delete_entry(struct sslcert_entry *e)
{
	sslcert_viewer_free_treeview_field_data(e);
	free(e);
}


static nserror sslcert_viewer_tree_node_folder_cb(
		struct treeview_node_msg msg, void *data)
{
	switch (msg.msg) {
	case TREE_MSG_NODE_DELETE:
	case TREE_MSG_NODE_EDIT:
	case TREE_MSG_NODE_LAUNCH:
		break;
	}

	return NSERROR_OK;
}
static nserror sslcert_viewer_tree_node_entry_cb(
		struct treeview_node_msg msg, void *data)
{
	struct sslcert_entry *e = data;

	switch (msg.msg) {
	case TREE_MSG_NODE_DELETE:
		e->entry = NULL;
		sslcert_viewer_delete_entry(e);
		break;

	case TREE_MSG_NODE_EDIT:
	case TREE_MSG_NODE_LAUNCH:
		break;
	}

	return NSERROR_OK;
}
struct treeview_callback_table sslv_tree_cb_t = {
	.folder = sslcert_viewer_tree_node_folder_cb,
	.entry = sslcert_viewer_tree_node_entry_cb
};


/* Exported interface, documented in sslcert_viewer.h */
nserror sslcert_viewer_init(struct core_window_callback_table *cw_t,
		void *core_window_handle, struct sslcert_session_data *ssl_d)
{
	nserror err;
	int cert_loop;

	assert(ssl_d != NULL);

	LOG(("Building certificate viewer"));

	/* Init. certificate chain treeview entry fields */
	err = sslcert_init_entry_fields(ssl_d);
	if (err != NSERROR_OK) {
		ssl_d->tree = NULL;
		return err;
	}

	/* Create the certificate treeview */
	err = treeview_create(&ssl_d->tree, &sslv_tree_cb_t,
			SSLCERT_V_N_FIELDS, ssl_d->fields,
			cw_t, core_window_handle, TREEVIEW_READ_ONLY);
	if (err != NSERROR_OK) {
		ssl_d->tree = NULL;
		return err;
	}

	/* Build treeview nodes from certificate chain */
	for (cert_loop = ssl_d->num - 1; cert_loop >= 0; cert_loop--) {
		err = sslcert_viewer_create_node(ssl_d, cert_loop);
		if (err != NSERROR_OK) {
			return err;
		}
	}

	LOG(("Built certificate viewer"));

	return NSERROR_OK;
}


/**
 * Free SSL certificate session data
 *
 * \param ssl_d		SSL certificate session data
 */
static void sslcert_cleanup_session(struct sslcert_session_data *ssl_d)
{
	assert(ssl_d != NULL);

	if (ssl_d->url)
		nsurl_unref(ssl_d->url);

	free(ssl_d);
}


/* Exported interface, documented in sslcert_viewer.h */
nserror sslcert_viewer_fini(struct sslcert_session_data *ssl_d)
{
	int i;
	nserror err;

	LOG(("Finalising ssl certificate viewer"));

	/* Destroy the treeview */
	err = treeview_destroy(ssl_d->tree);

	/* Free treeview entry fields */
	for (i = 0; i < SSLCERT_V_N_FIELDS; i++)
		if (ssl_d->fields[i].field != NULL)
			lwc_string_unref(ssl_d->fields[i].field);

	/* Destroy the sslcert_session_data */
	sslcert_cleanup_session(ssl_d);

	LOG(("Finalised ssl certificate viewer"));

	return err;
}


/* Exported interface, documented in sslcert_viewer.h */
nserror sslcert_viewer_create_session_data(unsigned long num, nsurl *url,
		llcache_query_response cb, void *cbpw,
		const struct ssl_cert_info *certs,
		struct sslcert_session_data **ssl_d)
{
	struct sslcert_session_data *data;

	assert(url != NULL);
	assert(certs != NULL);

	data = malloc(sizeof(struct sslcert_session_data));
	if (data == NULL) {
		warn_user("NoMemory", 0);
		*ssl_d = NULL;
		return NSERROR_NOMEM;
	}

	data->certs = certs;
	data->url = nsurl_ref(url);
	data->num = num;
	data->cb = cb;
	data->cbpw = cbpw;

	data->tree = NULL;

	*ssl_d = data;
	return NSERROR_OK;
}


/* Exported interface, documented in sslcert_viewer.h */
nserror sslcert_viewer_reject(struct sslcert_session_data *ssl_d)
{
	assert(ssl_d != NULL);

	ssl_d->cb(false, ssl_d->cbpw);

	return NSERROR_OK;
}


/* Exported interface, documented in sslcert_viewer.h */
nserror sslcert_viewer_accept(struct sslcert_session_data *ssl_d)
{
	assert(ssl_d != NULL);

	urldb_set_cert_permissions(ssl_d->url, true);

	ssl_d->cb(true, ssl_d->cbpw);

	return NSERROR_OK;
}


/* Exported interface, documented in sslcert_viewer.h */
void sslcert_viewer_redraw(struct sslcert_session_data *ssl_d,
		int x, int y, struct rect *clip,
		const struct redraw_context *ctx)
{
	treeview_redraw(ssl_d->tree, x, y, clip, ctx);
}


/* Exported interface, documented in sslcert_viewer.h */
void sslcert_viewer_mouse_action(struct sslcert_session_data *ssl_d,
		browser_mouse_state mouse, int x, int y)
{
	treeview_mouse_action(ssl_d->tree, mouse, x, y);
}


/* Exported interface, documented in sslcert_viewer.h */
void sslcert_viewer_keypress(struct sslcert_session_data *ssl_d,
		uint32_t key)
{
	treeview_keypress(ssl_d->tree, key);
}
