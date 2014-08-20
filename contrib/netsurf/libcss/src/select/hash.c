/*
 * This file is part of LibCSS
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <stdio.h>
#include <string.h>

#include "stylesheet.h"
#include "select/hash.h"
#include "utils/utils.h"

#undef PRINT_CHAIN_BLOOM_DETAILS

typedef struct hash_entry {
	const css_selector *sel;
	css_bloom sel_chain_bloom[CSS_BLOOM_SIZE];
	struct hash_entry *next;
} hash_entry;

typedef struct hash_t {
#define DEFAULT_SLOTS (1<<6)
	size_t n_slots;

	hash_entry *slots;
} hash_t;

struct css_selector_hash {
	hash_t elements;

	hash_t classes;

	hash_t ids;

	hash_entry universal;

	size_t hash_size;
};

static hash_entry empty_slot;

static inline lwc_string *_class_name(const css_selector *selector);
static inline lwc_string *_id_name(const css_selector *selector);
static css_error _insert_into_chain(css_selector_hash *ctx, hash_entry *head, 
		const css_selector *selector);
static css_error _remove_from_chain(css_selector_hash *ctx, hash_entry *head,
		const css_selector *selector);

static css_error _iterate_elements(
		const struct css_hash_selection_requirments *req,
		const css_selector **current,
		const css_selector ***next);
static css_error _iterate_classes(
		const struct css_hash_selection_requirments *req,
		const css_selector **current,
		const css_selector ***next);
static css_error _iterate_ids(
		const struct css_hash_selection_requirments *req,
		const css_selector **current,
		const css_selector ***next);
static css_error _iterate_universal(
		const struct css_hash_selection_requirments *req,
		const css_selector **current,
		const css_selector ***next);




/* Get case insensitive hash value for a name.
 * All element/class/id names are known to have their insensitive ptr set. */
#define _hash_name(name) \
	lwc_string_hash_value(name->insensitive)


/* No bytecode if rule body is empty or wholly invalid --
 * Only interested in rules with bytecode */
#define RULE_HAS_BYTECODE(r) \
	(((css_rule_selector *)(r->sel->rule))->style != NULL)


/**
 * Test first selector on selector chain for having matching element name.
 *
 *   If source of rule is element or universal hash, we know the
 *   element name is a match.  If it comes from the class or id hash,
 *   we have to test for a match.
 *
 * \param selector	selector chain head to test
 * \param qname		element name to look for
 * \return true iff chain head doesn't fail to match element name
 */
static inline bool _chain_good_for_element_name(const css_selector *selector,
		const css_qname *qname, const lwc_string *uni)
{
	if (selector->data.qname.name != uni) {
		bool match;
		if (lwc_string_caseless_isequal(
				selector->data.qname.name, qname->name,
				&match) == lwc_error_ok && match == false) {
			return false;
		}
	}

	return true;
}

/**
 * Test whether the rule applies for current media.
 *
 * \param rule		Rule to test
 * \meaid media		Current media type(s)
 * \return true iff chain's rule applies for media
 */
static inline bool _rule_good_for_media(const css_rule *rule, uint64_t media)
{
	bool applies = true;
	const css_rule *ancestor = rule;

	while (ancestor != NULL) {
		const css_rule_media *m = (const css_rule_media *) ancestor;

		if (ancestor->type == CSS_RULE_MEDIA &&
				(m->media & media) == 0) {
			applies = false;
			break;
		}

		if (ancestor->ptype != CSS_RULE_PARENT_STYLESHEET)
			ancestor = ancestor->parent;
		else
			ancestor = NULL;
	}

	return applies;
}


/**
 * Create a hash
 *
 * \param hash   Pointer to location to receive result
 * \return CSS_OK on success, appropriate error otherwise
 */
css_error css__selector_hash_create(css_selector_hash **hash)
{
	css_selector_hash *h;

	if (hash == NULL)
		return CSS_BADPARM;

	h = malloc(sizeof(css_selector_hash));
	if (h == NULL)
		return CSS_NOMEM;

	/* Element hash */
	h->elements.slots = malloc(DEFAULT_SLOTS * sizeof(hash_entry));
	if (h->elements.slots == NULL) {
		free(h);
		return CSS_NOMEM;
	}
	memset(h->elements.slots, 0, DEFAULT_SLOTS * sizeof(hash_entry));
	h->elements.n_slots = DEFAULT_SLOTS;

	/* Class hash */
	h->classes.slots = malloc(DEFAULT_SLOTS * sizeof(hash_entry));
	if (h->classes.slots == NULL) {
		free(h->elements.slots);
		free(h);
		return CSS_NOMEM;
	}
	memset(h->classes.slots, 0, DEFAULT_SLOTS * sizeof(hash_entry));
	h->classes.n_slots = DEFAULT_SLOTS;

	/* ID hash */
	h->ids.slots = malloc(DEFAULT_SLOTS * sizeof(hash_entry));
	if (h->ids.slots == NULL) {
		free(h->classes.slots);
		free(h->elements.slots);
		free(h);
		return CSS_NOMEM;
	}
	memset(h->ids.slots, 0, DEFAULT_SLOTS * sizeof(hash_entry));
	h->ids.n_slots = DEFAULT_SLOTS;

	/* Universal chain */
	memset(&h->universal, 0, sizeof(hash_entry));

	h->hash_size = sizeof(css_selector_hash) + 
			DEFAULT_SLOTS * sizeof(hash_entry) +
			DEFAULT_SLOTS * sizeof(hash_entry) +
			DEFAULT_SLOTS * sizeof(hash_entry);

	*hash = h;

	return CSS_OK;
}

/**
 * Destroy a hash
 *
 * \param hash  The hash to destroy
 * \return CSS_OK on success, appropriate error otherwise
 */
css_error css__selector_hash_destroy(css_selector_hash *hash)
{
	hash_entry *d, *e;
	uint32_t i;

	if (hash == NULL)
		return CSS_BADPARM;

	/* Element hash */
	for (i = 0; i < hash->elements.n_slots; i++) {
		for (d = hash->elements.slots[i].next; d != NULL; d = e) {
			e = d->next;

			free(d);
		}
	}
	free(hash->elements.slots);

	/* Class hash */
	for (i = 0; i < hash->classes.n_slots; i++) {
		for (d = hash->classes.slots[i].next; d != NULL; d = e) {
			e = d->next;

			free(d);
		}
	}
	free(hash->classes.slots);

	/* ID hash */
	for (i = 0; i < hash->ids.n_slots; i++) {
		for (d = hash->ids.slots[i].next; d != NULL; d = e) {
			e = d->next;

			free(d);
		}
	}
	free(hash->ids.slots);

	/* Universal chain */
	for (d = hash->universal.next; d != NULL; d = e) {
		e = d->next;

		free(d);
	}

	free(hash);

	return CSS_OK;
}

/**
 * Insert an item into a hash
 *
 * \param hash      The hash to insert into
 * \param selector  Pointer to selector
 * \return CSS_OK on success, appropriate error otherwise
 */
css_error css__selector_hash_insert(css_selector_hash *hash,
		const css_selector *selector)
{
	uint32_t index, mask;
	lwc_string *name;
	css_error error;

	if (hash == NULL || selector == NULL)
		return CSS_BADPARM;

	/* Work out which hash to insert into */
	if ((name = _id_name(selector)) != NULL) {
		/* Named ID */
		mask = hash->ids.n_slots - 1;
		index = _hash_name(name) & mask;

		error = _insert_into_chain(hash, &hash->ids.slots[index],
				selector);
	} else if ((name = _class_name(selector)) != NULL) {
		/* Named class */
		mask = hash->classes.n_slots - 1;
		index = _hash_name(name) & mask;

		error = _insert_into_chain(hash, &hash->classes.slots[index],
				selector);
	} else if (lwc_string_length(selector->data.qname.name) != 1 ||
			lwc_string_data(selector->data.qname.name)[0] != '*') {
		/* Named element */
		mask = hash->elements.n_slots - 1;
		index = _hash_name(selector->data.qname.name) & mask;

		error = _insert_into_chain(hash, &hash->elements.slots[index],
				selector);
	} else {
		/* Universal chain */
		error = _insert_into_chain(hash, &hash->universal, selector);
	}

	return error;
}

/**
 * Remove an item from a hash
 *
 * \param hash      The hash to remove from
 * \param selector  Pointer to selector
 * \return CSS_OK on success, appropriate error otherwise
 */
css_error css__selector_hash_remove(css_selector_hash *hash,
		const css_selector *selector)
{
	uint32_t index, mask;
	lwc_string *name;
	css_error error;

	if (hash == NULL || selector == NULL)
		return CSS_BADPARM;

	/* Work out which hash to remove from */
	if ((name = _id_name(selector)) != NULL) {
		/* Named ID */
		mask = hash->ids.n_slots - 1;
		index = _hash_name(name) & mask;

		error = _remove_from_chain(hash, &hash->ids.slots[index],
				selector);
	} else if ((name = _class_name(selector)) != NULL) {
		/* Named class */
		mask = hash->classes.n_slots - 1;
		index = _hash_name(name) & mask;

		error = _remove_from_chain(hash, &hash->classes.slots[index],
				selector);
	} else if (lwc_string_length(selector->data.qname.name) != 1 ||
			lwc_string_data(selector->data.qname.name)[0] != '*') {
		/* Named element */
		mask = hash->elements.n_slots - 1;
		index = _hash_name(selector->data.qname.name) & mask;

		error = _remove_from_chain(hash, &hash->elements.slots[index],
				selector);
	} else {
		/* Universal chain */
		error = _remove_from_chain(hash, &hash->universal, selector);
	}

	return error;
}

/**
 * Find the first selector that matches name
 *
 * \param hash      Hash to search
 * \param qname     Qualified name to match
 * \param iterator  Pointer to location to receive iterator function
 * \param matched   Pointer to location to receive selector
 * \return CSS_OK on success, appropriate error otherwise
 *
 * If nothing matches, CSS_OK will be returned and **matched == NULL
 */
css_error css__selector_hash_find(css_selector_hash *hash,
		const struct css_hash_selection_requirments *req,
		css_selector_hash_iterator *iterator,
		const css_selector ***matched)
{
	uint32_t index, mask;
	hash_entry *head;

	if (hash == NULL || req == NULL || iterator == NULL || matched == NULL)
		return CSS_BADPARM;

	/* Find index */
	mask = hash->elements.n_slots - 1;

	if (req->qname.name->insensitive == NULL &&
			lwc__intern_caseless_string(
			req->qname.name) != lwc_error_ok) {
		return CSS_NOMEM;
	}
	index = _hash_name(req->qname.name) & mask;

	head = &hash->elements.slots[index];

	if (head->sel != NULL) {
		/* Search through chain for first match */
		while (head != NULL) {
			lwc_error lerror;
			bool match = false;

			lerror = lwc_string_isequal(
					req->qname.name->insensitive,
					head->sel->data.qname.name->insensitive,
					&match);
			if (lerror != lwc_error_ok)
				return css_error_from_lwc_error(lerror);

			if (match && RULE_HAS_BYTECODE(head)) {
				if (css_bloom_in_bloom(
						head->sel_chain_bloom,
						req->node_bloom) &&
				    _rule_good_for_media(head->sel->rule,
						req->media)) {
					/* Found a match */
					break;
				}
			}

			head = head->next;
		}

		if (head == NULL)
			head = &empty_slot;
	}

	(*iterator) = _iterate_elements;
	(*matched) = (const css_selector **) head;

	return CSS_OK;
}

/**
 * Find the first selector that has a class that matches name
 *
 * \param hash      Hash to search
 * \param name      Name to match
 * \param iterator  Pointer to location to receive iterator function
 * \param matched   Pointer to location to receive selector
 * \return CSS_OK on success, appropriate error otherwise
 *
 * If nothing matches, CSS_OK will be returned and **matched == NULL
 */
css_error css__selector_hash_find_by_class(css_selector_hash *hash,
		const struct css_hash_selection_requirments *req,
		css_selector_hash_iterator *iterator,
		const css_selector ***matched)
{
	uint32_t index, mask;
	hash_entry *head;

	if (hash == NULL || req == NULL || req->class == NULL ||
			iterator == NULL || matched == NULL)
		return CSS_BADPARM;

	/* Find index */
	mask = hash->classes.n_slots - 1;

	if (req->class->insensitive == NULL &&
			lwc__intern_caseless_string(
			req->class) != lwc_error_ok) {
		return CSS_NOMEM;
	}
	index = _hash_name(req->class) & mask;

	head = &hash->classes.slots[index];

	if (head->sel != NULL) {
		/* Search through chain for first match */
		while (head != NULL) {
			lwc_error lerror;
			lwc_string *n;
			bool match = false;

			n = _class_name(head->sel);
			if (n != NULL) {
				lerror = lwc_string_isequal(
						req->class->insensitive,
						n->insensitive, &match);
				if (lerror != lwc_error_ok)
					return css_error_from_lwc_error(lerror);

				if (match && RULE_HAS_BYTECODE(head)) {
					if (css_bloom_in_bloom(
							head->sel_chain_bloom,
							req->node_bloom) &&
					    _chain_good_for_element_name(
							head->sel,
							&(req->qname),
							req->uni) &&
					    _rule_good_for_media(
							head->sel->rule,
							req->media)) {
						/* Found a match */
						break;
					}
				}
			}

			head = head->next;
		}

		if (head == NULL)
			head = &empty_slot;
	}

	(*iterator) = _iterate_classes;
	(*matched) = (const css_selector **) head;

	return CSS_OK;
}

/**
 * Find the first selector that has an ID that matches name
 *
 * \param hash      Hash to search
 * \param name      Name to match
 * \param iterator  Pointer to location to receive iterator function
 * \param matched   Pointer to location to receive selector
 * \return CSS_OK on success, appropriate error otherwise
 *
 * If nothing matches, CSS_OK will be returned and **matched == NULL
 */
css_error css__selector_hash_find_by_id(css_selector_hash *hash,
		const struct css_hash_selection_requirments *req,
		css_selector_hash_iterator *iterator,
		const css_selector ***matched)
{
	uint32_t index, mask;
	hash_entry *head;

	if (hash == NULL || req == NULL || req->id == NULL ||
			iterator == NULL || matched == NULL)
		return CSS_BADPARM;

	/* Find index */
	mask = hash->ids.n_slots - 1;

	if (req->id->insensitive == NULL &&
			lwc__intern_caseless_string(
			req->id) != lwc_error_ok) {
		return CSS_NOMEM;
	}
	index = _hash_name(req->id) & mask;

	head = &hash->ids.slots[index];

	if (head->sel != NULL) {
		/* Search through chain for first match */
		while (head != NULL) {
			lwc_error lerror;
			lwc_string *n;
			bool match = false;

			n = _id_name(head->sel);
			if (n != NULL) {
				lerror = lwc_string_isequal(
						req->id->insensitive,
						n->insensitive, &match);
				if (lerror != lwc_error_ok)
					return css_error_from_lwc_error(lerror);

				if (match && RULE_HAS_BYTECODE(head)) {
					if (css_bloom_in_bloom(
							head->sel_chain_bloom,
							req->node_bloom) &&
					    _chain_good_for_element_name(
							head->sel,
							&req->qname,
							req->uni) &&
					    _rule_good_for_media(
							head->sel->rule,
							req->media)) {
						/* Found a match */
						break;
					}
				}
			}

			head = head->next;
		}

		if (head == NULL)
			head = &empty_slot;
	}

	(*iterator) = _iterate_ids;
	(*matched) = (const css_selector **) head;

	return CSS_OK;
}

/**
 * Find the first universal selector
 *
 * \param hash      Hash to search
 * \param iterator  Pointer to location to receive iterator function
 * \param matched   Pointer to location to receive selector
 * \return CSS_OK on success, appropriate error otherwise
 *
 * If nothing matches, CSS_OK will be returned and **matched == NULL
 */
css_error css__selector_hash_find_universal(css_selector_hash *hash,
		const struct css_hash_selection_requirments *req,
		css_selector_hash_iterator *iterator,
		const css_selector ***matched)
{
	hash_entry *head;

	if (hash == NULL || req == NULL || iterator == NULL || matched == NULL)
		return CSS_BADPARM;

	head = &hash->universal;

	if (head->sel != NULL) {
		/* Search through chain for first match */
		while (head != NULL) {
			if (RULE_HAS_BYTECODE(head) &&
			    css_bloom_in_bloom(
					head->sel_chain_bloom,
					req->node_bloom) &&
			    _rule_good_for_media(head->sel->rule,
					req->media)) {
				/* Found a match */
				break;
			}

			head = head->next;
		}

		if (head == NULL)
			head = &empty_slot;
	}

	(*iterator) = _iterate_universal;
	(*matched) = (const css_selector **) head;

	return CSS_OK;
}

/**
 * Determine the memory-resident size of a hash
 *
 * \param hash  Hash to consider
 * \param size  Pointer to location to receive byte count
 * \return CSS_OK on success.
 *
 * \note The returned size will represent the size of the hash datastructures,
 *       and will not include the size of the data stored in the hash.
 */
css_error css__selector_hash_size(css_selector_hash *hash, size_t *size)
{
	if (hash == NULL || size == NULL)
		return CSS_BADPARM;

	*size = hash->hash_size;

	return CSS_OK;
}

/******************************************************************************
 * Private functions                                                          *
 ******************************************************************************/

/**
 * Retrieve the first class name in a selector, or NULL if none
 *
 * \param selector  Selector to consider
 * \return Pointer to class name, or NULL if none
 */
lwc_string *_class_name(const css_selector *selector)
{
	const css_selector_detail *detail = &selector->data;
	lwc_string *name = NULL;

	do {
		/* Ignore :not(.class) */
		if (detail->type == CSS_SELECTOR_CLASS && detail->negate == 0) {
			name = detail->qname.name;
			break;
		}

		if (detail->next)
			detail++;
		else
			detail = NULL;
	} while (detail != NULL);

	return name;
}

/**
 * Retrieve the first ID name in a selector, or NULL if none
 *
 * \param selector  Selector to consider
 * \return Pointer to ID name, or NULL if none
 */
lwc_string *_id_name(const css_selector *selector)
{
	const css_selector_detail *detail = &selector->data;
	lwc_string *name = NULL;

	do {
		/* Ignore :not(#id) */
		if (detail->type == CSS_SELECTOR_ID && detail->negate == 0) {
			name = detail->qname.name;
			break;
		}

		if (detail->next)
			detail++;
		else
			detail = NULL;
	} while (detail != NULL);

	return name;
}


/**
 * Add a selector detail to the bloom filter, if the detail is relevant.
 *
 * \param d		Selector detail to consider and add if relevant
 * \param bloom		Bloom filter to add to.
 */
static inline void _chain_bloom_add_detail(
		const css_selector_detail *d,
		css_bloom bloom[CSS_BLOOM_SIZE])
{
	lwc_string *add; /* String to add to bloom */

	switch (d->type) {
	case CSS_SELECTOR_ELEMENT:
		/* Don't add universal element selector to bloom */
		if (lwc_string_length(d->qname.name) == 1 &&
				lwc_string_data(d->qname.name)[0] == '*') {
			return;
		}
		/* Fall through */
	case CSS_SELECTOR_CLASS:
	case CSS_SELECTOR_ID:
		/* Element, Id and Class names always have the insensitive
		 * string set at css_selector_detail creation time. */
		add = d->qname.name->insensitive;

		if (add != NULL) {
			css_bloom_add_hash(bloom, lwc_string_hash_value(add));
		}
		break;

	default:
		break;
	}

	return;
}


/**
 * Generate a selector chain's bloom filter
 *
 * \param s		Selector at head of selector chain
 * \param bloom		Bloom filter to generate.
 */
static void _chain_bloom_generate(const css_selector *s,
		css_bloom bloom[CSS_BLOOM_SIZE])
{
	css_bloom_init(bloom);

	/* Work back through selector chain... */
	do {
		/* ...looking for Ancestor/Parent combinators */
		if (s->data.comb == CSS_COMBINATOR_ANCESTOR ||
				 s->data.comb == CSS_COMBINATOR_PARENT) {
			const css_selector_detail *d = &s->combinator->data;
			do {
				if (d->negate == 0) {
					_chain_bloom_add_detail(d, bloom);
				}
			} while ((d++)->next != 0);
		}

		s = s->combinator;
	} while (s != NULL);
}

#ifdef PRINT_CHAIN_BLOOM_DETAILS
/* Count bits set in uint32_t */
static int bits_set(uint32_t n) {
	n = n - ((n >> 1) & 0x55555555);
	n = (n & 0x33333333) + ((n >> 2) & 0x33333333);
	n = (n + (n >> 4)) & 0x0f0f0f0f;
	n = n + (n >> 8);
	n = n + (n >> 16);
	return n & 0x0000003f;
}

/* Selector chain bloom instrumentation ouput display. */
static void print_chain_bloom_details(css_bloom bloom[CSS_BLOOM_SIZE])
{
	printf("Chain bloom:\t");
	int total = 0, i;
	int set[4];
	for (i = 0; i < CSS_BLOOM_SIZE; i++) {
		set[i] = bits_set(bloom[i]);
		total += set[i];
	}
	printf("bits set:");
	for (i = 0; i < CSS_BLOOM_SIZE; i++) {
		printf(" %2i", set[i]);
	}
	printf(" (total:%4i of %i)   saturation: %3i%%\n", total,
			(32 * CSS_BLOOM_SIZE),
			(100 * total) / (32 * CSS_BLOOM_SIZE));
}
#endif

/**
 * Insert a selector into a hash chain
 *
 * \param ctx       Selector hash
 * \param head      Head of chain to insert into
 * \param selector  Selector to insert
 * \return CSS_OK    on success,
 *         CSS_NOMEM on memory exhaustion.
 */
css_error _insert_into_chain(css_selector_hash *ctx, hash_entry *head, 
		const css_selector *selector)
{
	if (head->sel == NULL) {
		head->sel = selector;
		head->next = NULL;
		_chain_bloom_generate(selector, head->sel_chain_bloom);

#ifdef PRINT_CHAIN_BLOOM_DETAILS
		print_chain_bloom_details(head->sel_chain_bloom);
#endif
	} else {
		hash_entry *search = head;
		hash_entry *prev = NULL;
		hash_entry *entry = malloc(sizeof(hash_entry));
		if (entry == NULL)
			return CSS_NOMEM;

		/* Find place to insert entry */
		do {
			/* Sort by ascending specificity */
			if (search->sel->specificity > selector->specificity)
				break;

			/* Sort by ascending rule index */
			if (search->sel->specificity == selector->specificity &&
					search->sel->rule->index > 
					selector->rule->index)
				break;

			prev = search;
			search = search->next;
		} while (search != NULL);

		entry->sel = selector;
		_chain_bloom_generate(selector, entry->sel_chain_bloom);

#ifdef PRINT_CHAIN_BLOOM_DETAILS
		print_chain_bloom_details(entry->sel_chain_bloom);
#endif

		if (prev == NULL) {
			hash_entry temp;
			entry->next = entry;
			temp = *entry;
			*entry = *head;
			*head = temp;
		} else {
			entry->next = prev->next;
			prev->next = entry;
		}

		ctx->hash_size += sizeof(hash_entry);
	}

	return CSS_OK;
}

/**
 * Remove a selector from a hash chain
 *
 * \param ctx       Selector hash
 * \param head      Head of chain to remove from
 * \param selector  Selector to remove
 * \return CSS_OK       on success,
 *         CSS_INVALID  if selector not found in chain.
 */
css_error _remove_from_chain(css_selector_hash *ctx, hash_entry *head,
		const css_selector *selector)
{
	hash_entry *search = head, *prev = NULL;

	if (head->sel == NULL)
		return CSS_INVALID;

	do {
		if (search->sel == selector)
			break;

		prev = search;
		search = search->next;
	} while (search != NULL);

	if (search == NULL)
		return CSS_INVALID;

	if (prev == NULL) {
		if (search->next != NULL) {
			head->sel = search->next->sel;
			head->next = search->next->next;
		} else {
			head->sel = NULL;
			head->next = NULL;
		}
	} else {
		prev->next = search->next;

		free(search);

		ctx->hash_size -= sizeof(hash_entry);
	}

	return CSS_OK;
}

/**
 * Find the next selector that matches
 *
 * \param current  Current item
 * \param next     Pointer to location to receive next item
 * \return CSS_OK on success, appropriate error otherwise
 *
 * If nothing further matches, CSS_OK will be returned and **next == NULL
 */
css_error _iterate_elements(
		const struct css_hash_selection_requirments *req,
		const css_selector **current,
		const css_selector ***next)
{
	const hash_entry *head = (const hash_entry *) current;
	bool match = false;
	lwc_error lerror = lwc_error_ok;
	lwc_string *name;

	name = req->qname.name;
	head = head->next;

	if (head != NULL && head->sel != NULL) {
		/* Search through chain for first match */
		while (head != NULL) {
			lerror = lwc_string_caseless_isequal(name,
					head->sel->data.qname.name, &match);
			if (lerror != lwc_error_ok)
				return css_error_from_lwc_error(lerror);

			if (match && RULE_HAS_BYTECODE(head)) {
				if (css_bloom_in_bloom(
						head->sel_chain_bloom,
						req->node_bloom) &&
				    _rule_good_for_media(head->sel->rule,
						req->media)) {
					/* Found a match */
					break;
				}
			}
			head = head->next;
		}
	}

	if (head == NULL)
		head = &empty_slot;

	(*next) = (const css_selector **) head;

	return CSS_OK;
}

/**
 * Find the next selector that matches
 *
 * \param current  Current item
 * \param next     Pointer to location to receive next item
 * \return CSS_OK on success, appropriate error otherwise
 *
 * If nothing further matches, CSS_OK will be returned and **next == NULL
 */
css_error _iterate_classes(
		const struct css_hash_selection_requirments *req,
		const css_selector **current,
		const css_selector ***next)
{
	const hash_entry *head = (const hash_entry *) current;
	bool match = false;
	lwc_error lerror = lwc_error_ok;
	lwc_string *name, *ref;

	ref = req->class;
	head = head->next;

	if (head != NULL && head->sel != NULL) {
		/* Search through chain for first match */
		while (head != NULL) {
			name = _class_name(head->sel);

			if (name != NULL) {
				lerror = lwc_string_caseless_isequal(
						ref, name, &match);
				if (lerror != lwc_error_ok)
					return css_error_from_lwc_error(lerror);

				if (match && RULE_HAS_BYTECODE(head)) {
					if (css_bloom_in_bloom(
							head->sel_chain_bloom,
							req->node_bloom) &&
					    _chain_good_for_element_name(
							head->sel,
							&(req->qname),
							req->uni) &&
					    _rule_good_for_media(
							head->sel->rule,
							req->media)) {
						/* Found a match */
						break;
					}
				}
			}
			head = head->next;
		}
	}

	if (head == NULL)
		head = &empty_slot;

	(*next) = (const css_selector **) head;

	return CSS_OK;
}

/**
 * Find the next selector that matches
 *
 * \param current  Current item
 * \param next     Pointer to location to receive next item
 * \return CSS_OK on success, appropriate error otherwise
 *
 * If nothing further matches, CSS_OK will be returned and **next == NULL
 */
css_error _iterate_ids(
		const struct css_hash_selection_requirments *req,
		const css_selector **current,
		const css_selector ***next)
{
	const hash_entry *head = (const hash_entry *) current;
	bool match = false;
	lwc_error lerror = lwc_error_ok;
	lwc_string *name, *ref;

	ref = req->id;
	head = head->next;

	if (head != NULL && head->sel != NULL) {
		/* Search through chain for first match */
		while (head != NULL) {
			name = _id_name(head->sel);

			if (name != NULL) {
				lerror = lwc_string_caseless_isequal(
						ref, name, &match);
				if (lerror != lwc_error_ok)
					return css_error_from_lwc_error(lerror);

				if (match && RULE_HAS_BYTECODE(head)) {
					if (css_bloom_in_bloom(
							head->sel_chain_bloom,
							req->node_bloom) &&
					    _chain_good_for_element_name(
							head->sel,
							&req->qname,
							req->uni) &&
					    _rule_good_for_media(
							head->sel->rule,
							req->media)) {
						/* Found a match */
						break;
					}
				}
			}
			head = head->next;
		}
	}

	if (head == NULL)
		head = &empty_slot;

	(*next) = (const css_selector **) head;

	return CSS_OK;
}

/**
 * Find the next selector that matches
 *
 * \param current  Current item
 * \param next     Pointer to location to receive next item
 * \return CSS_OK on success, appropriate error otherwise
 *
 * If nothing further matches, CSS_OK will be returned and **next == NULL
 */
css_error _iterate_universal(
		const struct css_hash_selection_requirments *req,
		const css_selector **current,
		const css_selector ***next)
{
	const hash_entry *head = (const hash_entry *) current;
	head = head->next;

	if (head != NULL && head->sel != NULL) {
		/* Search through chain for first match */
		while (head != NULL) {
			if (RULE_HAS_BYTECODE(head) &&
			    css_bloom_in_bloom(
					head->sel_chain_bloom,
					req->node_bloom) &&
			    _rule_good_for_media(head->sel->rule,
					req->media)) {
				/* Found a match */
				break;
			}
			head = head->next;
		}
	}

	if (head == NULL)
		head = &empty_slot;

	(*next) = (const css_selector **) head;

	return CSS_OK;
}

