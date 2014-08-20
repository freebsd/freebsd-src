/*
 * Copyright 2011 Vincent Sanders <vince@netsurf-browser.org>
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


#include "render/form.h"
#include "render/html_internal.h"
#include "utils/corestrings.h"

#include "utils/log.h"

/** 
 * process form element from dom
 */
static struct form *
parse_form_element(const char *docenc, dom_node *node)
{
	dom_string *ds_action = NULL;
	dom_string *ds_charset = NULL;
	dom_string *ds_target = NULL;
	dom_string *ds_method = NULL;
	dom_string *ds_enctype = NULL;
	char *action = NULL, *charset = NULL, *target = NULL;
	form_method method;
	dom_html_form_element *formele = (dom_html_form_element *)(node);
	struct form * ret = NULL;

	/* Retrieve the attributes from the node */
	if (dom_html_form_element_get_action(formele,
			&ds_action) != DOM_NO_ERR)
		goto out;

	if (dom_html_form_element_get_accept_charset(formele,
			&ds_charset) != DOM_NO_ERR)
		goto out;

	if (dom_html_form_element_get_target(formele,
			&ds_target) != DOM_NO_ERR)
		goto out;

	if (dom_html_form_element_get_method(formele,
			&ds_method) != DOM_NO_ERR)
		goto out;

	if (dom_html_form_element_get_enctype(formele,
			&ds_enctype) != DOM_NO_ERR)
		goto out;

	/* Extract the plain attributes ready for use.  We have to do this
	 * because we cannot guarantee that the dom_strings are NULL terminated
	 * and thus we copy them.
	 */
	if (ds_action != NULL)
		action = strndup(dom_string_data(ds_action),
				 dom_string_byte_length(ds_action));

	if (ds_charset != NULL)
		charset = strndup(dom_string_data(ds_charset),
				  dom_string_byte_length(ds_charset));

	if (ds_target != NULL)
		target = strndup(dom_string_data(ds_target),
				 dom_string_byte_length(ds_target));

	/* Determine the method */
	method = method_GET;
	if (ds_method != NULL) {
		if (dom_string_caseless_lwc_isequal(ds_method,
				corestring_lwc_post)) {
			method = method_POST_URLENC;
			if (ds_enctype != NULL) {
				if (dom_string_caseless_lwc_isequal(ds_enctype,
					corestring_lwc_multipart_form_data)) {

					method = method_POST_MULTIPART;
				}
			}
		}
	}

	/* Construct the form object */
	ret = form_new(node, action, target, method, charset, docenc);

out:
	if (ds_action != NULL)
		dom_string_unref(ds_action);
	if (ds_charset != NULL)
		dom_string_unref(ds_charset);
	if (ds_target != NULL)
		dom_string_unref(ds_target);
	if (ds_method != NULL)
		dom_string_unref(ds_method);
	if (ds_enctype != NULL)
		dom_string_unref(ds_enctype);
	if (action != NULL)
		free(action);
	if (charset != NULL)
		free(charset);
	if (target != NULL)
		free(target);
	return ret;
}

/* documented in html_internal.h */
struct form *html_forms_get_forms(const char *docenc, dom_html_document *doc)
{
	dom_html_collection *forms;
	struct form *ret = NULL, *newf;
	dom_node *node;
	unsigned long n;
	uint32_t nforms;

	if (doc == NULL)
		return NULL;

	/* Attempt to build a set of all the forms */
	if (dom_html_document_get_forms(doc, &forms) != DOM_NO_ERR)
		return NULL;

	/* Count the number of forms so we can iterate */
	if (dom_html_collection_get_length(forms, &nforms) != DOM_NO_ERR)
		goto out;

	/* Iterate the forms collection, making form structs for returning */
	for (n = 0; n < nforms; ++n) {
		if (dom_html_collection_item(forms, n, &node) != DOM_NO_ERR) {
			goto out;
		}
		newf = parse_form_element(docenc, node);
		dom_node_unref(node);
		if (newf == NULL) {
			goto err;
		}
		newf->prev = ret;
		ret = newf;
	}

	/* All went well */
	goto out;
err:
	while (ret != NULL) {
		struct form *prev = ret->prev;
		/* Destroy ret */
		free(ret);
		ret = prev;
	}
out:
	/* Finished with the collection, return it */
	dom_html_collection_unref(forms);

	return ret;
}

static struct form *
find_form(struct form *forms, dom_html_form_element *form)
{
	while (forms != NULL) {
		if (forms->node == form)
			break;
		forms = forms->prev;
	}

	return forms;
}

static struct form_control *
parse_button_element(struct form *forms, dom_html_button_element *button)
{
	struct form_control *control = NULL;
	dom_exception err;
	dom_html_form_element *form = NULL;
	dom_string *ds_type = NULL;
	dom_string *ds_value = NULL;
	dom_string *ds_name = NULL;

	err = dom_html_button_element_get_form(button, &form);
	if (err != DOM_NO_ERR)
		goto out;

	err = dom_html_button_element_get_type(button, &ds_type);
	if (err != DOM_NO_ERR)
		goto out;

	if (ds_type == NULL) {
		control = form_new_control(button, GADGET_SUBMIT);
	} else {
		if (dom_string_caseless_lwc_isequal(ds_type,
				corestring_lwc_submit)) {
			control = form_new_control(button, GADGET_SUBMIT);
		} else if (dom_string_caseless_lwc_isequal(ds_type,
				corestring_lwc_reset)) {
			control = form_new_control(button, GADGET_RESET);
		} else {
			control = form_new_control(button, GADGET_BUTTON);
		}
	}

	if (control == NULL)
		goto out;

	err = dom_html_button_element_get_value(button, &ds_value);
	if (err != DOM_NO_ERR)
		goto out;
	err = dom_html_button_element_get_name(button, &ds_name);
	if (err != DOM_NO_ERR)
		goto out;

	if (ds_value != NULL) {
		control->value = strndup(
			dom_string_data(ds_value),
			dom_string_byte_length(ds_value));

		if (control->value == NULL) {
			form_free_control(control);
			control = NULL;
			goto out;
		}
	}

	if (ds_name != NULL) {
		control->name = strndup(
			dom_string_data(ds_name),
			dom_string_byte_length(ds_name));

		if (control->name == NULL) {
			form_free_control(control);
			control = NULL;
			goto out;
		}
	}

	if (form != NULL && control != NULL)
		form_add_control(find_form(forms, form), control);

out:
	if (form != NULL)
		dom_node_unref(form);
	if (ds_type != NULL)
		dom_string_unref(ds_type);
	if (ds_value != NULL)
		dom_string_unref(ds_value);
	if (ds_name != NULL)
		dom_string_unref(ds_name);

	return control;
}

static struct form_control *
parse_input_element(struct form *forms, dom_html_input_element *input)
{
	struct form_control *control = NULL;
	dom_html_form_element *form = NULL;
	dom_string *ds_type = NULL;
	dom_string *ds_name = NULL;
	dom_string *ds_value = NULL;

	char *name = NULL;

	if (dom_html_input_element_get_form(input, &form) != DOM_NO_ERR)
		goto out;

	if (dom_html_input_element_get_type(input, &ds_type) != DOM_NO_ERR)
		goto out;

	if (dom_html_input_element_get_name(input, &ds_name) != DOM_NO_ERR)
		goto out;

	if (ds_name != NULL)
		name = strndup(dom_string_data(ds_name),
			       dom_string_byte_length(ds_name));

	if (ds_type != NULL && dom_string_caseless_lwc_isequal(ds_type,
			corestring_lwc_password)) {
		control = form_new_control(input, GADGET_PASSWORD);
	} else if (ds_type != NULL && dom_string_caseless_lwc_isequal(ds_type,
			corestring_lwc_file)) {
		control = form_new_control(input, GADGET_FILE);
	} else if (ds_type != NULL && dom_string_caseless_lwc_isequal(ds_type,
			corestring_lwc_hidden)) {
		control = form_new_control(input, GADGET_HIDDEN);
	} else if (ds_type != NULL && dom_string_caseless_lwc_isequal(ds_type,
			corestring_lwc_checkbox)) {
		control = form_new_control(input, GADGET_CHECKBOX);
	} else if (ds_type != NULL && dom_string_caseless_lwc_isequal(ds_type,
			corestring_lwc_radio)) {
		control = form_new_control(input, GADGET_RADIO);
	} else if (ds_type != NULL && dom_string_caseless_lwc_isequal(ds_type,
			corestring_lwc_submit)) {
		control = form_new_control(input, GADGET_SUBMIT);
	} else if (ds_type != NULL && dom_string_caseless_lwc_isequal(ds_type,
			corestring_lwc_reset)) {
		control = form_new_control(input, GADGET_RESET);
	} else if (ds_type != NULL && dom_string_caseless_lwc_isequal(ds_type,
			corestring_lwc_button)) {
		control = form_new_control(input, GADGET_BUTTON);
	} else if (ds_type != NULL && dom_string_caseless_lwc_isequal(ds_type,
			corestring_lwc_image)) {
		control = form_new_control(input, GADGET_IMAGE);
	} else {
		control = form_new_control(input, GADGET_TEXTBOX);
	}

	if (control == NULL)
		goto out;

	if (name != NULL) {
		/* Hand the name string over */
		control->name = name;
		name = NULL;
	}

	if (control->type == GADGET_CHECKBOX || control->type == GADGET_RADIO) {
		bool selected;
		if (dom_html_input_element_get_checked(
			    input, &selected) == DOM_NO_ERR) {
			control->selected = selected;
		}
	}

	if (control->type == GADGET_PASSWORD ||
	    control->type == GADGET_TEXTBOX) {
		int32_t maxlength;
		if (dom_html_input_element_get_max_length(
			    input, &maxlength) != DOM_NO_ERR) {
			maxlength = -1;
		}

		if (maxlength >= 0) {
			/* Got valid maxlength */
			control->maxlength = maxlength;
		} else {
			/* Input has no maxlength attr, or
			 * dom_html_input_element_get_max_length failed.
			 *
			 * Set it to something insane. */
			control->maxlength = UINT_MAX;
		}
	}

	if (control->type != GADGET_FILE && control->type != GADGET_IMAGE) {
		if (dom_html_input_element_get_value(
			    input, &ds_value) == DOM_NO_ERR) {
			if (ds_value != NULL) {
				control->value = strndup(
					dom_string_data(ds_value),
					dom_string_byte_length(ds_value));
				if (control->value == NULL) {
					form_free_control(control);
					control = NULL;
					goto out;
				}
				control->length = strlen(control->value);
			}
		}

		if (control->type == GADGET_TEXTBOX ||
				control->type == GADGET_PASSWORD) {
			if (control->value == NULL) {
				control->value = strdup("");
				if (control->value == NULL) {
					form_free_control(control);
					control = NULL;
					goto out;
				}

				control->length = 0;
			}

			control->initial_value = strdup(control->value);
			if (control->initial_value == NULL) {
				form_free_control(control);
				control = NULL;
				goto out;
			}
		}
	}

	if (form != NULL && control != NULL)
		form_add_control(find_form(forms, form), control);

out:
	if (form != NULL)
		dom_node_unref(form);
	if (ds_type != NULL)
		dom_string_unref(ds_type);
	if (ds_name != NULL)
		dom_string_unref(ds_name);
	if (ds_value != NULL)
		dom_string_unref(ds_value);

	if (name != NULL)
		free(name);

	return control;
}

static struct form_control *
parse_textarea_element(struct form *forms, dom_html_text_area_element *ta)
{
	struct form_control *control = NULL;
	dom_html_form_element *form = NULL;
	dom_string *ds_name = NULL;

	char *name = NULL;

	if (dom_html_text_area_element_get_form(ta, &form) != DOM_NO_ERR)
		goto out;

	if (dom_html_text_area_element_get_name(ta, &ds_name) != DOM_NO_ERR)
		goto out;

	if (ds_name != NULL)
		name = strndup(dom_string_data(ds_name),
			       dom_string_byte_length(ds_name));

	control = form_new_control(ta, GADGET_TEXTAREA);

	if (control == NULL)
		goto out;

	if (name != NULL) {
		/* Hand the name string over */
		control->name = name;
		name = NULL;
	}

	if (form != NULL && control != NULL)
		form_add_control(find_form(forms, form), control);

out:
	if (form != NULL)
		dom_node_unref(form);
	if (ds_name != NULL)
		dom_string_unref(ds_name);

	if (name != NULL)
		free(name);


	return control;
}

static struct form_control *
parse_select_element(struct form *forms, dom_html_select_element *select)
{
	struct form_control *control = NULL;
	dom_html_form_element *form = NULL;
	dom_string *ds_name = NULL;

	char *name = NULL;

	if (dom_html_select_element_get_form(select, &form) != DOM_NO_ERR)
		goto out;

	if (dom_html_select_element_get_name(select, &ds_name) != DOM_NO_ERR)
		goto out;

	if (ds_name != NULL)
		name = strndup(dom_string_data(ds_name),
			       dom_string_byte_length(ds_name));

	control = form_new_control(select, GADGET_SELECT);

	if (control == NULL)
		goto out;

	if (name != NULL) {
		/* Hand the name string over */
		control->name = name;
		name = NULL;
	}

	dom_html_select_element_get_multiple(select,
			&(control->data.select.multiple));

	if (form != NULL && control != NULL)
		form_add_control(find_form(forms, form), control);

out:
	if (form != NULL)
		dom_node_unref(form);
	if (ds_name != NULL)
		dom_string_unref(ds_name);

	if (name != NULL)
		free(name);


	return control;
}


static struct form_control *
invent_fake_gadget(dom_node *node)
{
	struct form_control *ctl = form_new_control(node, GADGET_HIDDEN);
	if (ctl != NULL) {
		ctl->value = strdup("");
		ctl->initial_value = strdup("");
		ctl->name = strdup("foo");

		if (ctl->value == NULL || ctl->initial_value == NULL ||
				ctl->name == NULL) {
			form_free_control(ctl);
			ctl = NULL;
		}
	}
	return ctl;
}

/* documented in html_internal.h */
struct form_control *html_forms_get_control_for_node(struct form *forms,
		dom_node *node)
{
	struct form *f;
	struct form_control *ctl = NULL;
	dom_exception err;
	dom_string *ds_name = NULL;

	/* Step one, see if we already have a control */
	for (f = forms; f != NULL; f = f->prev) {
		for (ctl = f->controls; ctl != NULL; ctl = ctl->next) {
			if (ctl->node == node)
				return ctl;
		}
	}

	/* Step two, extract the node's name so we can construct a gadget. */
	err = dom_element_get_tag_name(node, &ds_name);
	if (err == DOM_NO_ERR && ds_name != NULL) {

		/* Step three, attempt to work out what gadget to make */
		if (dom_string_caseless_lwc_isequal(ds_name,
				corestring_lwc_button)) {
			ctl = parse_button_element(forms,
					(dom_html_button_element *) node);
		} else if (dom_string_caseless_lwc_isequal(ds_name,
				corestring_lwc_input)) {
			ctl = parse_input_element(forms,
					(dom_html_input_element *) node);
		} else if (dom_string_caseless_lwc_isequal(ds_name,
				corestring_lwc_textarea)) {
			ctl = parse_textarea_element(forms,
					(dom_html_text_area_element *) node);
		} else if (dom_string_caseless_lwc_isequal(ds_name,
				corestring_lwc_select)) {
			ctl = parse_select_element(forms,
					(dom_html_select_element *) node);
		}
	}

	/* If all else fails, fake gadget time */
	if (ctl == NULL)
		ctl = invent_fake_gadget(node);

	if (ds_name != NULL)
		dom_string_unref(ds_name);

	return ctl;
}

