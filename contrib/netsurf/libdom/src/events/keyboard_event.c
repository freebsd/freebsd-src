/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include <stdlib.h>
#include <string.h>

#include "events/keyboard_event.h"
#include "core/document.h"

#include "utils/utils.h"

static void _virtual_dom_keyboard_event_destroy(struct dom_event *evt);

static struct dom_event_private_vtable _event_vtable = {
	_virtual_dom_keyboard_event_destroy
};

/* Constructor */
dom_exception _dom_keyboard_event_create(struct dom_document *doc, 
		struct dom_keyboard_event **evt)
{
	*evt = malloc(sizeof(dom_keyboard_event));
	if (*evt == NULL) 
		return DOM_NO_MEM_ERR;
	
	((struct dom_event *) *evt)->vtable = &_event_vtable;

	return _dom_keyboard_event_initialise(doc, *evt);
}

/* Destructor */
void _dom_keyboard_event_destroy(struct dom_keyboard_event *evt)
{
	_dom_keyboard_event_finalise(evt);

	free(evt);
}

/* Initialise function */
dom_exception _dom_keyboard_event_initialise(struct dom_document *doc, 
		struct dom_keyboard_event *evt)
{
	evt->key_ident = NULL;
	evt->modifier_state = 0;

	return _dom_ui_event_initialise(doc, &evt->base);
}

/* Finalise function */
void _dom_keyboard_event_finalise(struct dom_keyboard_event *evt)
{
	_dom_ui_event_finalise(&evt->base);
}

/* The virtual destroy function */
void _virtual_dom_keyboard_event_destroy(struct dom_event *evt)
{
	_dom_keyboard_event_destroy((dom_keyboard_event *) evt);
}

/*----------------------------------------------------------------------*/
/* The public API */

/**
 * Get the key identifier
 *
 * \param evt  The Event object
 * \param ident  The returned key identifier
 * \return DOM_NO_ERR.
 */
dom_exception _dom_keyboard_event_get_key_identifier(dom_keyboard_event *evt,
		dom_string **ident)
{
	*ident = evt->key_ident;
	dom_string_ref(*ident);

	return DOM_NO_ERR;
}

/**
 * Get the key location
 *
 * \param evt  The Event object
 * \param loc  The returned key location
 * \return DOM_NO_ERR.
 */
dom_exception _dom_keyboard_event_get_key_location(dom_keyboard_event *evt,
		dom_key_location *loc)
{
	*loc = evt->key_loc;

	return DOM_NO_ERR;
}

/**
 * Get the ctrl key state
 *
 * \param evt  The Event object
 * \param key  Whether the Control key is pressed down
 * \return DOM_NO_ERR.
 */
dom_exception _dom_keyboard_event_get_ctrl_key(dom_keyboard_event *evt,
		bool *key)
{
	*key = ((evt->modifier_state & DOM_MOD_CTRL) != 0);

	return DOM_NO_ERR;
}

/**
 * Get the shift key state
 *
 * \param evt  The Event object
 * \param key  Whether the Shift key is pressed down
 * \return DOM_NO_ERR.
 */
dom_exception _dom_keyboard_event_get_shift_key(dom_keyboard_event *evt,
		bool *key)
{
	*key = ((evt->modifier_state & DOM_MOD_SHIFT) != 0);

	return DOM_NO_ERR;
}
		
/**
 * Get the alt key state
 *
 * \param evt  The Event object
 * \param key  Whether the Alt key is pressed down
 * \return DOM_NO_ERR.
 */
dom_exception _dom_keyboard_event_get_alt_key(dom_keyboard_event *evt,
		bool *key)
{
	*key = ((evt->modifier_state & DOM_MOD_ALT) != 0);

	return DOM_NO_ERR;
}

/**
 * Get the meta key state
 *
 * \param evt  The Event object
 * \param key  Whether the Meta key is pressed down
 * \return DOM_NO_ERR.
 */
dom_exception _dom_keyboard_event_get_meta_key(dom_keyboard_event *evt,
		bool *key)
{
	*key = ((evt->modifier_state & DOM_MOD_META) != 0);

	return DOM_NO_ERR;
}

/**
 * Query the state of a modifier using a key identifier
 *
 * \param evt    The event object
 * \param ml     The modifier identifier, such as "Alt", "Control", "Meta", 
 *               "AltGraph", "CapsLock", "NumLock", "Scroll", "Shift".
 * \param state  Whether the modifier key is pressed
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 *
 * @note: If an application wishes to distinguish between right and left 
 * modifiers, this information could be deduced using keyboard events and 
 * KeyboardEvent.keyLocation.
 */
dom_exception _dom_keyboard_event_get_modifier_state(dom_keyboard_event *evt,
		dom_string *m, bool *state)
{
	const char *data;
	size_t len;

	if (m == NULL) {
		*state = false;
		return DOM_NO_ERR;
	}

	data = dom_string_data(m);
	len = dom_string_byte_length(m);

	if (len == SLEN("AltGraph") && strncmp(data, "AltGraph", len) == 0) {
		*state = ((evt->modifier_state & DOM_MOD_ALT_GRAPH) != 0);
	} else if (len == SLEN("Alt") && strncmp(data, "Alt", len) == 0) {
		*state = ((evt->modifier_state & DOM_MOD_ALT) != 0);
	} else if (len == SLEN("CapsLock") &&
			strncmp(data, "CapsLock", len) == 0) {
		*state = ((evt->modifier_state & DOM_MOD_CAPS_LOCK) != 0);
	} else if (len == SLEN("Control") &&
			strncmp(data, "Control", len) == 0) {
		*state = ((evt->modifier_state & DOM_MOD_CTRL) != 0);
	} else if (len == SLEN("Meta") && strncmp(data, "Meta", len) == 0) {
		*state = ((evt->modifier_state & DOM_MOD_META) != 0);
	} else if (len == SLEN("NumLock") &&
			strncmp(data, "NumLock", len) == 0) {
		*state = ((evt->modifier_state & DOM_MOD_NUM_LOCK) != 0);
	} else if (len == SLEN("Scroll") &&
			strncmp(data, "Scroll", len) == 0) {
		*state = ((evt->modifier_state & DOM_MOD_SCROLL) != 0);
	} else if (len == SLEN("Shift") && strncmp(data, "Shift", len) == 0) {
		*state = ((evt->modifier_state & DOM_MOD_SHIFT) != 0);
	}

	return DOM_NO_ERR;
}

/**
 * Initialise the keyboard event
 *
 * \param evt            The Event object
 * \param type           The event's type
 * \param bubble         Whether this is a bubbling event
 * \param cancelable     Whether this is a cancelable event
 * \param view           The AbstractView associated with this event
 * \param key_indent     The key identifier of pressed key
 * \param key_loc        The key location of the preesed key
 * \param modifier_list  A string of modifier key identifiers, separated with
 *                       space
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_keyboard_event_init(dom_keyboard_event *evt, 
		dom_string *type, bool bubble, bool cancelable, 
		struct dom_abstract_view *view, dom_string *key_ident,
		dom_key_location key_loc, dom_string *modifier_list)
{
	dom_exception err;

	evt->key_ident = key_ident;
	dom_string_ref(evt->key_ident);
	evt->key_loc = key_loc;

	err = _dom_parse_modifier_list(modifier_list, &evt->modifier_state);
	if (err != DOM_NO_ERR)
		return err;

	return _dom_ui_event_init(&evt->base, type, bubble, cancelable,
			view, 0);
}

/**
 * Initialise the keyboard event with namespace
 *
 * \param evt            The Event object
 * \param namespace      The namespace of this event
 * \param type           The event's type
 * \param bubble         Whether this is a bubbling event
 * \param cancelable     Whether this is a cancelable event
 * \param view           The AbstractView associated with this event
 * \param key_indent     The key identifier of pressed key
 * \param key_loc        The key location of the preesed key
 * \param modifier_list  A string of modifier key identifiers, separated with
 *                       space
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_keyboard_event_init_ns(dom_keyboard_event *evt, 
		dom_string *namespace, dom_string *type,
		bool bubble, bool cancelable, struct dom_abstract_view *view, 
		dom_string *key_ident, dom_key_location key_loc, 
		dom_string *modifier_list)
{
	dom_exception err;

	evt->key_ident = key_ident;
	dom_string_ref(evt->key_ident);
	evt->key_loc = key_loc;

	err = _dom_parse_modifier_list(modifier_list, &evt->modifier_state);
	if (err != DOM_NO_ERR)
		return err;

	return _dom_ui_event_init_ns(&evt->base, namespace, type, bubble, 
			cancelable, view, 0);
}


/*-------------------------------------------------------------------------*/

/**
 * Parse the modifier list string to corresponding bool variable state
 *
 * \param modifier_list   The modifier list string
 * \param modifier_state  The returned state
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_parse_modifier_list(dom_string *modifier_list,
		uint32_t *modifier_state)
{
	const char *data;
	const char *m;
	size_t len = 0;

	*modifier_state = 0;

	if (modifier_list == NULL)
		return DOM_NO_ERR;
	
	data = dom_string_data(modifier_list);
	m = data;

	while (true) {
		/* If we reach a space or end of the string, we should parse
		 * the new token. */
		if (*data == ' ' || *data == '\0') {
			if (len == SLEN("AltGraph") &&
					strncmp(m, "AltGraph", len) == 0) {
				*modifier_state = *modifier_state | 
						DOM_MOD_ALT_GRAPH;
			} else if (len == SLEN("Alt") &&
					strncmp(m, "Alt", len) == 0) {
				*modifier_state = *modifier_state | 
						DOM_MOD_ALT;
			} else if (len == SLEN("CapsLock") &&
					strncmp(m, "CapsLock", len) == 0) {
				*modifier_state = *modifier_state | 
						DOM_MOD_CAPS_LOCK;
			} else if (len == SLEN("Control") &&
					strncmp(m, "Control", len) == 0) {
				*modifier_state = *modifier_state | 
						DOM_MOD_CTRL;
			} else if (len == SLEN("Meta") &&
					strncmp(m, "Meta", len) == 0) {
				*modifier_state = *modifier_state | 
						DOM_MOD_META;
			} else if (len == SLEN("NumLock") &&
					strncmp(m, "NumLock", len) == 0) {
				*modifier_state = *modifier_state | 
						DOM_MOD_NUM_LOCK;
			} else if (len == SLEN("Scroll") &&
					strncmp(m, "Scroll", len) == 0) {
				*modifier_state = *modifier_state | 
						DOM_MOD_SCROLL;
			} else if (len == SLEN("Shift") &&
					strncmp(m, "Shift", len) == 0) {
				*modifier_state = *modifier_state | 
						DOM_MOD_SHIFT;
			}

			while (*data == ' ') {
				data++;
			}
			/* Finished parsing and break */
			if (*data == '\0')
				break;

			m = data;
			len = 0;
		}

		data++;
		len++;
	}

	return DOM_NO_ERR;
}

