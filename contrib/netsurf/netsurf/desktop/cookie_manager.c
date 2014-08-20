/*
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
 * Cookie Manager (implementation).
 */


#include <stdlib.h>

#include "content/urldb.h"
#include "desktop/cookie_manager.h"
#include "desktop/treeview.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "utils/log.h"

enum cookie_manager_field {
	COOKIE_M_NAME,
	COOKIE_M_CONTENT,
	COOKIE_M_DOMAIN,
	COOKIE_M_PATH,
	COOKIE_M_EXPIRES,
	COOKIE_M_LAST_USED,
	COOKIE_M_RESTRICTIONS,
	COOKIE_M_VERSION,
	COOKIE_M_PERSISTENT,
	COOKIE_M_DOMAIN_FOLDER,
	COOKIE_M_N_FIELDS
};

enum cookie_manager_value {
	COOKIE_M_HTTPS,
	COOKIE_M_SECURE,
	COOKIE_M_HTTP,
	COOKIE_M_NONE,
	COOKIE_M_NETSCAPE,
	COOKIE_M_RFC2109,
	COOKIE_M_RFC2965,
	COOKIE_M_YES,
	COOKIE_M_NO,
	COOKIE_M_N_VALUES
};

struct cookie_manager_folder {
	treeview_node *folder;
	struct treeview_field_data data;
};

struct cookie_manager_ctx {
	treeview *tree;
	struct treeview_field_desc fields[COOKIE_M_N_FIELDS];
	struct treeview_field_data values[COOKIE_M_N_VALUES];
	bool built;
};
struct cookie_manager_ctx cm_ctx;

struct cookie_manager_entry {
	bool user_delete;

	treeview_node *entry;

	struct treeview_field_data data[COOKIE_M_N_FIELDS - 1];
};


struct treeview_walk_ctx {
	const char *title;
	size_t title_len;
	struct cookie_manager_folder *folder;
	struct cookie_manager_entry *entry;
};
/** Callback for treeview_walk */
static nserror cookie_manager_walk_cb(void *ctx, void *node_data,
		enum treeview_node_type type, bool *abort)
{
	struct treeview_walk_ctx *tw = ctx;

	if (type == TREE_NODE_ENTRY) {
		struct cookie_manager_entry *entry = node_data;

		if (entry->data[COOKIE_M_NAME].value_len == tw->title_len &&
				strcmp(tw->title,
				entry->data[COOKIE_M_NAME].value) == 0) {
			/* Found what we're looking for */
			tw->entry = entry;
			*abort = true;
		}

	} else if (type == TREE_NODE_FOLDER) {
		struct cookie_manager_folder *folder = node_data;

		if (folder->data.value_len == tw->title_len &&
				strcmp(tw->title, folder->data.value) == 0) {
			/* Found what we're looking for */
			tw->folder = folder;
			*abort = true;
		}
	}

	return NSERROR_OK;
}
/**
 * Find a cookie entry in the cookie manager's treeview
 *
 * \param root		Search root node, or NULL to search from tree's root
 * \param title		ID of the node to look for
 * \param title_len	Byte length of title string
 * \param found		Updated to the matching node's cookie maanger entry
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static nserror cookie_manager_find_entry(treeview_node *root,
		const char *title, size_t title_len,
		struct cookie_manager_entry **found)
{
	nserror err;
	struct treeview_walk_ctx tw = {
		.title = title,
		.title_len = title_len,
		.folder = NULL,
		.entry = NULL
	};

	err = treeview_walk(cm_ctx.tree, root, cookie_manager_walk_cb, NULL,
			&tw, TREE_NODE_ENTRY);
	if (err != NSERROR_OK)
		return err;

	*found = tw.entry;

	return NSERROR_OK;
}
/**
 * Find a cookie domain folder in the cookie manager's treeview
 *
 * \param root		Search root node, or NULL to search from tree's root
 * \param title		ID of the node to look for
 * \param title_len	Byte length of title string
 * \param found		Updated to the matching node's cookie maanger folder
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static nserror cookie_manager_find_folder(treeview_node *root,
		const char *title, size_t title_len,
		struct cookie_manager_folder **found)
{
	nserror err;
	struct treeview_walk_ctx tw = {
		.title = title,
		.title_len = title_len,
		.folder = NULL,
		.entry = NULL
	};

	err = treeview_walk(cm_ctx.tree, root, cookie_manager_walk_cb, NULL,
			&tw, TREE_NODE_FOLDER);
	if (err != NSERROR_OK)
		return err;

	*found = tw.folder;

	return NSERROR_OK;
}


/**
 * Free a cookie manager entry's treeview field data.
 *
 * \param e		Cookie manager entry to free data from
 */
static void cookie_manager_free_treeview_field_data(
		struct cookie_manager_entry *e)
{
	/* Eww */
	free((void *)e->data[COOKIE_M_NAME].value);
	free((void *)e->data[COOKIE_M_CONTENT].value);
	free((void *)e->data[COOKIE_M_DOMAIN].value);
	free((void *)e->data[COOKIE_M_PATH].value);
	free((void *)e->data[COOKIE_M_EXPIRES].value);
	free((void *)e->data[COOKIE_M_LAST_USED].value);
}


/**
 * Build a cookie manager treeview field from given text
 *
 * \param field		Cookie manager treeview field to build
 * \param data		Cookie manager entry field data to set
 * \param value		Text to set in field, ownership yielded
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static inline nserror cookie_manager_field_builder(
		enum cookie_manager_field field,
		struct treeview_field_data *data,
		const char *value)
{
	data->field = cm_ctx.fields[field].field;
	data->value = value;
	data->value_len = (value != NULL) ? strlen(value) : 0;

	return NSERROR_OK;
}


/**
 * Set a cookie manager entry's data from the cookie_data.
 *
 * \param e		Cookie manager entry to set up
 * \param data		Data associated with entry's cookie
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static nserror cookie_manager_set_treeview_field_data(
		struct cookie_manager_entry *e,
		const struct cookie_data *data)
{
	const char *date;
	char *date2;

	assert(e != NULL);
	assert(data != NULL);

	/* Set the fields up */
	cookie_manager_field_builder(COOKIE_M_NAME,
			&e->data[COOKIE_M_NAME], strdup(data->name));
	cookie_manager_field_builder(COOKIE_M_CONTENT,
			&e->data[COOKIE_M_CONTENT], strdup(data->value));
	cookie_manager_field_builder(COOKIE_M_DOMAIN,
			&e->data[COOKIE_M_DOMAIN], strdup(data->domain));
	cookie_manager_field_builder(COOKIE_M_PATH,
			&e->data[COOKIE_M_PATH], strdup(data->path));

	/* Set the Expires date field */
	date = ctime(&data->expires);
	date2 = strdup(date);
	if (date2 != NULL) {
		assert(date2[24] == '\n');
		date2[24] = '\0';
	}
	cookie_manager_field_builder(COOKIE_M_EXPIRES,
			&e->data[COOKIE_M_EXPIRES], date2);

	/* Set the Last used date field */
	date = ctime(&data->last_used);
	date2 = strdup(date);
	if (date2 != NULL) {
		assert(date2[24] == '\n');
		date2[24] = '\0';
	}
	cookie_manager_field_builder(COOKIE_M_LAST_USED,
			&e->data[COOKIE_M_LAST_USED], date2);

	/* Set the Restrictions text */
	if (data->secure && data->http_only)
		e->data[COOKIE_M_RESTRICTIONS] = cm_ctx.values[COOKIE_M_HTTPS];
	else if (data->secure)
		e->data[COOKIE_M_RESTRICTIONS] = cm_ctx.values[COOKIE_M_SECURE];
	else if (data->http_only)
		e->data[COOKIE_M_RESTRICTIONS] = cm_ctx.values[COOKIE_M_HTTP];
	else
		e->data[COOKIE_M_RESTRICTIONS] = cm_ctx.values[COOKIE_M_NONE];

	/* Set the Version text */
	switch (data->version) {
	case COOKIE_NETSCAPE:
		e->data[COOKIE_M_VERSION] = cm_ctx.values[COOKIE_M_NETSCAPE];
		break;
	case COOKIE_RFC2109:
		e->data[COOKIE_M_VERSION] = cm_ctx.values[COOKIE_M_RFC2109];
		break;
	case COOKIE_RFC2965:
		e->data[COOKIE_M_VERSION] = cm_ctx.values[COOKIE_M_RFC2965];
		break;
	}

	/* Set the Persistent text */
	if (data->no_destroy)
		e->data[COOKIE_M_PERSISTENT] = cm_ctx.values[COOKIE_M_YES];
	else
		e->data[COOKIE_M_PERSISTENT] = cm_ctx.values[COOKIE_M_NO];

	return NSERROR_OK;
}


/**
 * Creates an empty tree entry for a cookie, and links it into the tree.
 *
 * All information is copied from the cookie_data, and as such can
 * be edited and should be freed.
 *
 * \param parent      the node to link to
 * \param data	      the cookie data to use
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static nserror cookie_manager_create_cookie_node(
		struct cookie_manager_folder *parent,
		const struct cookie_data *data)
{
	nserror err;
	struct cookie_manager_entry *cookie;

	/* Create new cookie manager entry */
	cookie = malloc(sizeof(struct cookie_manager_entry));
	if (cookie == NULL) {
		return NSERROR_NOMEM;
	}

	cookie->user_delete = false;

	err = cookie_manager_set_treeview_field_data(cookie, data);
	if (err != NSERROR_OK) {
		free(cookie);
		return err;
	}

	err = treeview_create_node_entry(cm_ctx.tree, &(cookie->entry),
			parent->folder, TREE_REL_FIRST_CHILD,
			cookie->data, cookie,
			cm_ctx.built ? TREE_OPTION_NONE :
					TREE_OPTION_SUPPRESS_RESIZE |
					TREE_OPTION_SUPPRESS_REDRAW);
	if (err != NSERROR_OK) {
		cookie_manager_free_treeview_field_data(cookie);
		free(cookie);
		return err;
	}

	return NSERROR_OK;
}


/**
 * Updates a cookie manager entry for updated cookie_data.
 *
 * All information is copied from the cookie_data, and as such can
 * be edited and should be freed.
 *
 * \param e	      the entry to update
 * \param data	      the cookie data to use
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static nserror cookie_manager_update_cookie_node(
		struct cookie_manager_entry *e,
		const struct cookie_data *data)
{
	nserror err;

	assert(e != NULL);

	/* Reset to defaults */
	e->user_delete = false;
	cookie_manager_free_treeview_field_data(e);

	/* Set new field values from the cookie_data */
	err = cookie_manager_set_treeview_field_data(e, data);
	if (err != NSERROR_OK) {
		return err;
	}

	/* Update the treeview */
	err = treeview_update_node_entry(cm_ctx.tree, e->entry, e->data, e);
	if (err != NSERROR_OK) {
		return err;
	}

	return NSERROR_OK;
}


/**
 * Creates an empty tree folder for a cookie domain, and links it into the tree.
 *
 * All information is copied from the cookie_data, and as such can
 * be edited and should be freed.
 *
 * \param folder      updated to the new folder
 * \param data	      the cookie data to use
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static nserror cookie_manager_create_domain_folder(
		struct cookie_manager_folder **folder,
		const struct cookie_data *data)
{
	nserror err;
	struct cookie_manager_folder *f;

	/* Create new cookie manager entry */
	f = malloc(sizeof(struct cookie_manager_folder));
	if (f == NULL) {
		return NSERROR_NOMEM;
	}

	f->data.field = cm_ctx.fields[COOKIE_M_N_FIELDS - 1].field;
	f->data.value = strdup(data->domain);
	f->data.value_len = (f->data.value != NULL) ?
			strlen(data->domain) : 0;

	err = treeview_create_node_folder(cm_ctx.tree, &(f->folder),
			NULL, TREE_REL_FIRST_CHILD, &f->data, f,
			cm_ctx.built ? TREE_OPTION_NONE :
					TREE_OPTION_SUPPRESS_RESIZE |
					TREE_OPTION_SUPPRESS_REDRAW);
	if (err != NSERROR_OK) {
		free((void *)f->data.value);
		free(f);
		return err;
	}

	*folder = f;

	return NSERROR_OK;
}


/* exported interface documented in cookie_manager.h */
bool cookie_manager_add(const struct cookie_data *data)
{
	struct cookie_manager_folder *parent = NULL;
	struct cookie_manager_entry *cookie = NULL;
	nserror err;

	assert(data != NULL);

	/* If we don't have a cookie manager at the moment, just return true */
	if (cm_ctx.tree == NULL)
		return true;

	err = cookie_manager_find_folder(NULL, data->domain,
			strlen(data->domain), &parent);
	if (err != NSERROR_OK) {
		return false;
	}

	if (parent == NULL) {
		/* Need to create domain directory */
		err = cookie_manager_create_domain_folder(&parent, data);
		if (err != NSERROR_OK || parent == NULL)
			return false;
	}

	/* Create cookie node */
	err = cookie_manager_find_entry(parent->folder, data->name,
			strlen(data->name), &cookie);
	if (err != NSERROR_OK)
		return false;

	if (cookie == NULL) {
		err = cookie_manager_create_cookie_node(parent, data);
	} else {
		err = cookie_manager_update_cookie_node(cookie, data);
	}
	if (err != NSERROR_OK)
		return false;

	return true;
}


/* exported interface documented in cookie_manager.h */
void cookie_manager_remove(const struct cookie_data *data)
{
	struct cookie_manager_folder *parent = NULL;
	struct cookie_manager_entry *cookie = NULL;
	nserror err;

	assert(data != NULL);

	/* If we don't have a cookie manager at the moment, just return */
	if (cm_ctx.tree == NULL)
		return;

	err = cookie_manager_find_folder(NULL, data->domain,
			strlen(data->domain), &parent);
	if (err != NSERROR_OK || parent == NULL) {
		/* Nothing to delete */
		return;
	}

	err = cookie_manager_find_entry(parent->folder, data->name,
			strlen(data->name), &cookie);
	if (err != NSERROR_OK || cookie == NULL) {
		/* Nothing to delete */
		return;
	}

	/* Delete the node */
	treeview_delete_node(cm_ctx.tree, cookie->entry, TREE_OPTION_NONE);
}


/**
 * Initialise the treeview entry feilds
 *
 * \return true on success, false on memory exhaustion
 */
static nserror cookie_manager_init_entry_fields(void)
{
	int i;
	const char *label;

	for (i = 0; i < COOKIE_M_N_FIELDS; i++)
		cm_ctx.fields[i].field = NULL;

	cm_ctx.fields[COOKIE_M_NAME].flags = TREE_FLAG_DEFAULT;
	label = "TreeviewLabelName";
	label = messages_get(label);
	if (lwc_intern_string(label, strlen(label),
			&cm_ctx.fields[COOKIE_M_NAME].field) !=
			lwc_error_ok) {
		goto error;
	}

	cm_ctx.fields[COOKIE_M_CONTENT].flags = TREE_FLAG_SHOW_NAME;
	label = "TreeviewLabelContent";
	label = messages_get(label);
	if (lwc_intern_string(label, strlen(label),
			&cm_ctx.fields[COOKIE_M_CONTENT].field) !=
			lwc_error_ok) {
		goto error;
	}

	cm_ctx.fields[COOKIE_M_DOMAIN].flags = TREE_FLAG_SHOW_NAME;
	label = "TreeviewLabelDomain";
	label = messages_get(label);
	if (lwc_intern_string(label, strlen(label),
			&cm_ctx.fields[COOKIE_M_DOMAIN].field) !=
			lwc_error_ok) {
		goto error;
	}

	cm_ctx.fields[COOKIE_M_PATH].flags = TREE_FLAG_SHOW_NAME;
	label = "TreeviewLabelPath";
	label = messages_get(label);
	if (lwc_intern_string(label, strlen(label),
			&cm_ctx.fields[COOKIE_M_PATH].field) !=
			lwc_error_ok) {
		goto error;
	}

	cm_ctx.fields[COOKIE_M_EXPIRES].flags = TREE_FLAG_SHOW_NAME;
	label = "TreeviewLabelExpires";
	label = messages_get(label);
	if (lwc_intern_string(label, strlen(label),
			&cm_ctx.fields[COOKIE_M_EXPIRES].field) !=
			lwc_error_ok) {
		goto error;
	}

	cm_ctx.fields[COOKIE_M_LAST_USED].flags = TREE_FLAG_SHOW_NAME;
	label = "TreeviewLabelLastUsed";
	label = messages_get(label);
	if (lwc_intern_string(label, strlen(label),
			&cm_ctx.fields[COOKIE_M_LAST_USED].field) !=
			lwc_error_ok) {
		goto error;
	}

	cm_ctx.fields[COOKIE_M_RESTRICTIONS].flags = TREE_FLAG_SHOW_NAME;
	label = "TreeviewLabelRestrictions";
	label = messages_get(label);
	if (lwc_intern_string(label, strlen(label),
			&cm_ctx.fields[COOKIE_M_RESTRICTIONS].field) !=
			lwc_error_ok) {
		goto error;
	}

	cm_ctx.fields[COOKIE_M_VERSION].flags = TREE_FLAG_SHOW_NAME;
	label = "TreeviewLabelVersion";
	label = messages_get(label);
	if (lwc_intern_string(label, strlen(label),
			&cm_ctx.fields[COOKIE_M_VERSION].field) !=
			lwc_error_ok) {
		goto error;
	}

	cm_ctx.fields[COOKIE_M_PERSISTENT].flags = TREE_FLAG_SHOW_NAME;
	label = "TreeviewLabelPersistent";
	label = messages_get(label);
	if (lwc_intern_string(label, strlen(label),
			&cm_ctx.fields[COOKIE_M_PERSISTENT].field) !=
			lwc_error_ok) {
		goto error;
	}

	cm_ctx.fields[COOKIE_M_DOMAIN_FOLDER].flags = TREE_FLAG_DEFAULT;
	label = "TreeviewLabelDomainFolder";
	label = messages_get(label);
	if (lwc_intern_string(label, strlen(label),
			&cm_ctx.fields[COOKIE_M_DOMAIN_FOLDER].field) !=
			lwc_error_ok) {
		return false;
	}

	return NSERROR_OK;

error:
	for (i = 0; i < COOKIE_M_N_FIELDS; i++)
		if (cm_ctx.fields[i].field != NULL)
			lwc_string_unref(cm_ctx.fields[i].field);

	return NSERROR_UNKNOWN;
}



/**
 * Initialise the common entry values
 *
 * \return true on success, false on memory exhaustion
 */
static nserror cookie_manager_init_common_values(void)
{
	const char *temp;

	/* Set the Restrictions text */
	temp = messages_get("CookieManagerHTTPS");
	cookie_manager_field_builder(COOKIE_M_RESTRICTIONS,
			&cm_ctx.values[COOKIE_M_HTTPS], strdup(temp));

	temp = messages_get("CookieManagerSecure");
	cookie_manager_field_builder(COOKIE_M_RESTRICTIONS,
			&cm_ctx.values[COOKIE_M_SECURE], strdup(temp));

	temp = messages_get("CookieManagerHTTP");
	cookie_manager_field_builder(COOKIE_M_RESTRICTIONS,
			&cm_ctx.values[COOKIE_M_HTTP], strdup(temp));

	temp = messages_get("None");
	cookie_manager_field_builder(COOKIE_M_RESTRICTIONS,
			&cm_ctx.values[COOKIE_M_NONE], strdup(temp));

	/* Set the Cookie version text */
	assert(COOKIE_NETSCAPE == 0);
	temp = messages_get("TreeVersion0");
	cookie_manager_field_builder(COOKIE_M_VERSION,
			&cm_ctx.values[COOKIE_M_NETSCAPE], strdup(temp));

	assert(COOKIE_RFC2109 == 1);
	temp = messages_get("TreeVersion1");
	cookie_manager_field_builder(COOKIE_M_VERSION,
			&cm_ctx.values[COOKIE_M_RFC2109], strdup(temp));

	assert(COOKIE_RFC2965 == 2);
	temp = messages_get("TreeVersion2");
	cookie_manager_field_builder(COOKIE_M_VERSION,
			&cm_ctx.values[COOKIE_M_RFC2965], strdup(temp));

	/* Set the Persistent value text */
	temp = messages_get("Yes");
	cookie_manager_field_builder(COOKIE_M_PERSISTENT,
			&cm_ctx.values[COOKIE_M_YES], strdup(temp));

	temp = messages_get("No");
	cookie_manager_field_builder(COOKIE_M_PERSISTENT,
			&cm_ctx.values[COOKIE_M_NO], strdup(temp));

	return NSERROR_OK;
}


/**
 * Delete cookie manager entries (and optionally delete from urldb)
 *
 * \param e		Cookie manager entry to delete.
 */
static void cookie_manager_delete_entry(struct cookie_manager_entry *e)
{
	const char *domain;
	const char *path;
	const char *name;

	if (e->user_delete) {
		/* Delete the cookie from URLdb */
		domain = e->data[COOKIE_M_DOMAIN].value;
		path = e->data[COOKIE_M_PATH].value;
		name = e->data[COOKIE_M_NAME].value;

		if ((domain != NULL) && (path != NULL) && (name != NULL)) {
			
			urldb_delete_cookie(domain, path, name);
		} else {
			LOG(("Delete cookie fail: "
					"need domain, path, and name."));
		}
	}

	/* Delete the cookie manager entry */
	cookie_manager_free_treeview_field_data(e);
	free(e);
}


static nserror cookie_manager_tree_node_folder_cb(
		struct treeview_node_msg msg, void *data)
{
	struct cookie_manager_folder *f = data;

	switch (msg.msg) {
	case TREE_MSG_NODE_DELETE:
		free(f);
		break;

	case TREE_MSG_NODE_EDIT:
		break;

	case TREE_MSG_NODE_LAUNCH:
		break;
	}

	return NSERROR_OK;
}
static nserror cookie_manager_tree_node_entry_cb(
		struct treeview_node_msg msg, void *data)
{
	struct cookie_manager_entry *e = data;

	switch (msg.msg) {
	case TREE_MSG_NODE_DELETE:
		e->entry = NULL;
		e->user_delete = msg.data.delete.user;
		cookie_manager_delete_entry(e);
		break;

	case TREE_MSG_NODE_EDIT:
		break;

	case TREE_MSG_NODE_LAUNCH:
		break;
	}
	return NSERROR_OK;
}
struct treeview_callback_table cm_tree_cb_t = {
	.folder = cookie_manager_tree_node_folder_cb,
	.entry = cookie_manager_tree_node_entry_cb
};


/* Exported interface, documented in cookie_manager.h */
nserror cookie_manager_init(struct core_window_callback_table *cw_t,
		void *core_window_handle)
{
	nserror err;

	LOG(("Generating cookie manager data"));

	/* Init. cookie manager treeview entry fields */
	err = cookie_manager_init_entry_fields();
	if (err != NSERROR_OK) {
		cm_ctx.tree = NULL;
		return err;
	}

	/* Init. common treeview field values */
	err = cookie_manager_init_common_values();
	if (err != NSERROR_OK) {
		cm_ctx.tree = NULL;
		return err;
	}

	/* Create the cookie manager treeview */
	err = treeview_create(&cm_ctx.tree, &cm_tree_cb_t,
			COOKIE_M_N_FIELDS, cm_ctx.fields,
			cw_t, core_window_handle,
			TREEVIEW_NO_MOVES | TREEVIEW_DEL_EMPTY_DIRS);
	if (err != NSERROR_OK) {
		cm_ctx.tree = NULL;
		return err;
	}

	/* Load the cookies */
	urldb_iterate_cookies(cookie_manager_add);

	/* Cookie manager is built
	 * We suppress the treeview height callback on entry insertion before
	 * the treeview is built. */
	cm_ctx.built = true;

	/* Inform client of window height */
	treeview_get_height(cm_ctx.tree);

	LOG(("Generated cookie manager data"));

	return NSERROR_OK;
}


/* Exported interface, documented in cookie_manager.h */
nserror cookie_manager_fini(void)
{
	int i;
	nserror err;

	LOG(("Finalising cookie manager"));

	cm_ctx.built = false;

	/* Destroy the cookie manager treeview */
	err = treeview_destroy(cm_ctx.tree);

	/* Free cookie manager treeview entry fields */
	for (i = 0; i < COOKIE_M_N_FIELDS; i++)
		if (cm_ctx.fields[i].field != NULL)
			lwc_string_unref(cm_ctx.fields[i].field);

	/* Free cookie manager treeview common entry values */
	for (i = 0; i < COOKIE_M_N_VALUES; i++)
		if (cm_ctx.values[i].value != NULL)
			free((void *) cm_ctx.values[i].value);

	LOG(("Finalised cookie manager"));

	return err;
}


/* Exported interface, documented in cookie_manager.h */
void cookie_manager_redraw(int x, int y, struct rect *clip,
		const struct redraw_context *ctx)
{
	treeview_redraw(cm_ctx.tree, x, y, clip, ctx);
}


/* Exported interface, documented in cookie_manager.h */
void cookie_manager_mouse_action(browser_mouse_state mouse, int x, int y)
{
	treeview_mouse_action(cm_ctx.tree, mouse, x, y);
}


/* Exported interface, documented in cookie_manager.h */
void cookie_manager_keypress(uint32_t key)
{
	treeview_keypress(cm_ctx.tree, key);
}


/* Exported interface, documented in cookie_manager.h */
bool cookie_manager_has_selection(void)
{
	return treeview_has_selection(cm_ctx.tree);
}


/* Exported interface, documented in cookie_manager.h */
nserror cookie_manager_expand(bool only_folders)
{
	return treeview_expand(cm_ctx.tree, only_folders);
}


/* Exported interface, documented in cookie_manager.h */
nserror cookie_manager_contract(bool all)
{
	return treeview_contract(cm_ctx.tree, all);
}

