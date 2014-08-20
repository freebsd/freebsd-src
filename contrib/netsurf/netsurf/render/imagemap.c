/*
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
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

/*
 * Much of this shamelessly copied from utils/messages.c
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>

#include <dom/dom.h>

#include "content/content_protected.h"
#include "content/hlcache.h"
#include "render/box.h"
#include "render/html_internal.h"
#include "render/imagemap.h"
#include "utils/corestrings.h"
#include "utils/log.h"
#include "utils/utils.h"

#define HASH_SIZE 31 /* fixed size hash table */

typedef enum {
	IMAGEMAP_DEFAULT,
	IMAGEMAP_RECT,
	IMAGEMAP_CIRCLE,
	IMAGEMAP_POLY
} imagemap_entry_type;

struct mapentry {
	imagemap_entry_type type;	/**< type of shape */
	nsurl *url;			/**< absolute url to go to */
	char *target;			/**< target frame (if any) */
	union {
		struct {
			int x;		/**< x coordinate of centre */
			int y;		/**< y coordinate of center */
			int r;		/**< radius of circle */
		} circle;
		struct {
			int x0;		/**< left hand edge */
			int y0;		/**< top edge */
			int x1;		/**< right hand edge */
			int y1;		/**< bottom edge */
		} rect;
		struct {
			int num;	/**< number of points */
			float *xcoords;	/**< x coordinates */
			float *ycoords;	/**< y coordinates */
		} poly;
	} bounds;
	struct mapentry *next;		/**< next entry in list */
};

struct imagemap {
	char *key;		/**< key for this entry */
	struct mapentry *list;	/**< pointer to linked list of entries */
	struct imagemap *next;	/**< next entry in this hash chain */
};

static bool imagemap_add(html_content *c, dom_string *key,
		struct mapentry *list);
static bool imagemap_create(html_content *c);
static bool imagemap_extract_map(dom_node *node, html_content *c,
		struct mapentry **entry);
static bool imagemap_addtolist(dom_node *n, nsurl *base_url,
		struct mapentry **entry, dom_string *tagtype);
static void imagemap_freelist(struct mapentry *list);
static unsigned int imagemap_hash(const char *key);
static int imagemap_point_in_poly(int num, float *xpt, float *ypt,
		unsigned long x, unsigned long y, unsigned long click_x,
		unsigned long click_y);

/**
 * Add an imagemap to the hashtable, creating it if it doesn't exist
 *
 * \param c The containing content
 * \param key The name of the imagemap
 * \param list List of map regions
 * \return true on succes, false otherwise
 */
bool imagemap_add(html_content *c, dom_string *key, struct mapentry *list)
{
	struct imagemap *map;
	unsigned int slot;

	assert(c != NULL);
	assert(key != NULL);
	assert(list != NULL);

	if (imagemap_create(c) == false)
		return false;

	map = calloc(1, sizeof(*map));
	if (map == NULL)
		return false;
	
	/* \todo Stop relying on NULL termination of dom_string */
	map->key = strdup(dom_string_data(key));
	if (map->key == NULL) {
		free(map);
		return false;
	}

	map->list = list;

	slot = imagemap_hash(map->key);

	map->next = c->imagemaps[slot];
	c->imagemaps[slot] = map;

	return true;
}

/**
 * Create hashtable of imagemaps
 *
 * \param c The containing content
 * \return true on success, false otherwise
 */
bool imagemap_create(html_content *c)
{
	assert(c != NULL);

	if (c->imagemaps == NULL) {
		c->imagemaps = calloc(HASH_SIZE, sizeof(struct imagemap *));
		if (c->imagemaps == NULL) {
			return false;
		}
	}

	return true;
}

/**
 * Destroy hashtable of imagemaps
 *
 * \param c The containing content
 */
void imagemap_destroy(html_content *c)
{
	unsigned int i;

	assert(c != NULL);

	/* no imagemaps -> return */
	if (c->imagemaps == NULL)
		return;

	for (i = 0; i != HASH_SIZE; i++) {
		struct imagemap *map, *next;

		map = c->imagemaps[i];
		while (map != NULL) {
			next = map->next;
			imagemap_freelist(map->list);
			free(map->key);
			free(map);
			map = next;
		}
	}

	free(c->imagemaps);
}

/**
 * Dump imagemap data to the log
 *
 * \param c The containing content
 */
void imagemap_dump(html_content *c)
{
	unsigned int i;

	int j;

	assert(c != NULL);

	if (c->imagemaps == NULL)
		return;

	for (i = 0; i != HASH_SIZE; i++) {
		struct imagemap *map;
		struct mapentry *entry;

		map = c->imagemaps[i];
		while (map != NULL) {
			LOG(("Imagemap: %s", map->key));

			for (entry = map->list; entry; entry = entry->next) {
				switch (entry->type) {
				case IMAGEMAP_DEFAULT:
					LOG(("\tDefault: %s", nsurl_access(
							entry->url)));
					break;
				case IMAGEMAP_RECT:
					LOG(("\tRectangle: %s: [(%d,%d),(%d,%d)]",
						nsurl_access(entry->url),
						entry->bounds.rect.x0,
						entry->bounds.rect.y0,
						entry->bounds.rect.x1,
						entry->bounds.rect.y1));
					break;
				case IMAGEMAP_CIRCLE:
					LOG(("\tCircle: %s: [(%d,%d),%d]",
						nsurl_access(entry->url),
						entry->bounds.circle.x,
						entry->bounds.circle.y,
						entry->bounds.circle.r));
					break;
				case IMAGEMAP_POLY:
					LOG(("\tPolygon: %s:", nsurl_access(
							entry->url)));
					for (j = 0; j != entry->bounds.poly.num;
							j++) {
						fprintf(stderr, "(%d,%d) ",
							(int)entry->bounds.poly.xcoords[j],
							(int)entry->bounds.poly.ycoords[j]);
					}
					fprintf(stderr,"\n");
					break;
				}
			}
			map = map->next;
		}
	}
}

/**
 * Extract all imagemaps from a document tree
 *
 * \param c The content
 * \param map_str A dom_string which is "map"
 * \return false on memory exhaustion, true otherwise
 */
nserror
imagemap_extract(html_content *c)
{
	dom_nodelist *nlist;
	dom_exception exc;
	unsigned long mapnr;
	uint32_t maybe_maps;
	nserror ret = NSERROR_OK;

	exc = dom_document_get_elements_by_tag_name(c->document, 
						    corestring_dom_map, 
						    &nlist);
	if (exc != DOM_NO_ERR) {
		return NSERROR_DOM;
	}
	
	exc = dom_nodelist_get_length(nlist, &maybe_maps);
	if (exc != DOM_NO_ERR) {
		ret = NSERROR_DOM;
		goto out_nlist;
	}
	
	for (mapnr = 0; mapnr < maybe_maps; ++mapnr) {
		dom_node *node;
		dom_string *name;
		exc = dom_nodelist_item(nlist, mapnr, &node);
		if (exc != DOM_NO_ERR) {
			ret = NSERROR_DOM;
			goto out_nlist;
		}
		
		exc = dom_element_get_attribute(node, corestring_dom_id,
						&name);
		if (exc != DOM_NO_ERR) {
			dom_node_unref(node);
			ret = NSERROR_DOM;
			goto out_nlist;
		}
		
		if (name == NULL) {
			exc = dom_element_get_attribute(node, 
							corestring_dom_name,
							&name);
			if (exc != DOM_NO_ERR) {
				dom_node_unref(node);
				ret = NSERROR_DOM;
				goto out_nlist;
			}
		}
		
		if (name != NULL) {
			struct mapentry *entry = NULL;
			if (imagemap_extract_map(node, c, &entry) == false) {
				if (entry != NULL) {
					imagemap_freelist(entry);
				}

				dom_string_unref(name);
				dom_node_unref(node);
				ret = NSERROR_NOMEM; /** @todo check this */
				goto out_nlist;
			}
			
			/* imagemap_extract_map may not extract anything,
			 * so entry can still be NULL here. This isn't an
			 * error as it just means that we've encountered
			 * an incorrectly defined <map>...</map> block
			 */
			if ((entry != NULL) && 
			    (imagemap_add(c, name, entry) == false)) {
				imagemap_freelist(entry);

				dom_string_unref(name);
				dom_node_unref(node);
				ret = NSERROR_NOMEM; /** @todo check this */
				goto out_nlist;
			}
		}
		
		dom_string_unref(name);
		dom_node_unref(node);
	}
	
	
out_nlist:
	
	dom_nodelist_unref(nlist);

	return ret;
}

/**
 * Extract an imagemap from html source
 *
 * \param node  XML node containing map
 * \param c     Content containing document
 * \param entry List of map entries
 * \param tname The sub-tags to consider on this pass
 * \return false on memory exhaustion, true otherwise
 */
static bool
imagemap_extract_map_entries(dom_node *node, html_content *c,
			     struct mapentry **entry, dom_string *tname)
{
	dom_nodelist *nlist;
	dom_exception exc;
	unsigned long ent;
	uint32_t tag_count;
	
	exc = dom_element_get_elements_by_tag_name(node, tname, &nlist);
	if (exc != DOM_NO_ERR) {
		return false;
	}
	
	exc = dom_nodelist_get_length(nlist, &tag_count);
	if (exc != DOM_NO_ERR) {
		dom_nodelist_unref(nlist);
		return false;
	}
	
	for (ent = 0; ent < tag_count; ++ent) {
		dom_node *subnode;
		
		exc = dom_nodelist_item(nlist, ent, &subnode);
		if (exc != DOM_NO_ERR) {
			dom_nodelist_unref(nlist);
			return false;
		}
		if (imagemap_addtolist(subnode, c->base_url, 
				       entry, tname) == false) {
			dom_node_unref(subnode);
			dom_nodelist_unref(nlist);
			return false;
		}
		dom_node_unref(subnode);
	}
	
	dom_nodelist_unref(nlist);
	
	return true;
}

/**
 * Extract an imagemap from html source
 *
 * \param node  XML node containing map
 * \param c     Content containing document
 * \param entry List of map entries
 * \return false on memory exhaustion, true otherwise
 */
bool imagemap_extract_map(dom_node *node, html_content *c,
		struct mapentry **entry)
{
	if (imagemap_extract_map_entries(node, c, entry, 
			corestring_dom_area) == false)
		return false;
	return imagemap_extract_map_entries(node, c, entry,
			corestring_dom_a);
}
/**
 * Adds an imagemap entry to the list
 *
 * \param n     The xmlNode representing the entry to add
 * \param base_url  Base URL for resolving relative URLs
 * \param entry Pointer to list of entries
 * \return false on memory exhaustion, true otherwise
 */
bool
imagemap_addtolist(dom_node *n, nsurl *base_url,
		   struct mapentry **entry, dom_string *tagtype)
{
	dom_exception exc;
	dom_string *href = NULL, *target = NULL, *shape = NULL;
	dom_string *coords = NULL;
	struct mapentry *new_map, *temp;
	bool ret = true;
	
	if (dom_string_caseless_isequal(tagtype, corestring_dom_area)) {
		bool nohref = false;
		exc = dom_element_has_attribute(n, 
				corestring_dom_nohref, &nohref);
		if ((exc != DOM_NO_ERR) || nohref)
			/* Skip <area nohref="anything" /> */
			goto ok_out;
	}
	
	exc = dom_element_get_attribute(n, corestring_dom_href, &href);
	if (exc != DOM_NO_ERR || href == NULL) {
		/* No href="" attribute, skip this element */
		goto ok_out;
	}
	
	exc = dom_element_get_attribute(n, corestring_dom_target, &target);
	if (exc != DOM_NO_ERR) {
		goto ok_out;
	}
	
	exc = dom_element_get_attribute(n, corestring_dom_shape, &shape);
	if (exc != DOM_NO_ERR) {
		goto ok_out;
	}
	
	/* If there's no shape, we default to rectangles */
	if (shape == NULL)
		shape = dom_string_ref(corestring_dom_rect);
	
	if (!dom_string_caseless_lwc_isequal(shape, corestring_lwc_default)) {
		/* If not 'default' and there's no 'coords' give up */
		exc = dom_element_get_attribute(n, corestring_dom_coords, 
						&coords);
		if (exc != DOM_NO_ERR || coords == NULL) {
			goto ok_out;
		}
	}
	
	new_map = calloc(1, sizeof(*new_map));
	if (new_map == NULL) {
		goto bad_out;
	}
	
	if (dom_string_caseless_lwc_isequal(shape, corestring_lwc_rect) ||
	    dom_string_caseless_lwc_isequal(shape, corestring_lwc_rectangle))
		new_map->type = IMAGEMAP_RECT;
	else if (dom_string_caseless_lwc_isequal(shape, corestring_lwc_circle))
		new_map->type = IMAGEMAP_CIRCLE;
	else if (dom_string_caseless_lwc_isequal(shape, corestring_lwc_poly) ||
		 dom_string_caseless_lwc_isequal(shape, corestring_lwc_polygon))
		new_map->type = IMAGEMAP_POLY;
	else if (dom_string_caseless_lwc_isequal(shape, corestring_lwc_default))
		new_map->type = IMAGEMAP_DEFAULT;
	else
		goto bad_out;
	
	if (box_extract_link(dom_string_data(href), 
			     base_url, &new_map->url) == false)
		goto bad_out;
	
	if (new_map->url == NULL) {
		/* non-fatal error -> ignore this */
		goto ok_free_map_out;
	}
	
	if (target != NULL) {
		/* Copy target into the map */
		new_map->target = malloc(dom_string_byte_length(target) + 1);
		if (new_map->target == NULL)
			goto bad_out;
		/* Safe, but relies on dom_strings being NULL terminated */
		/* \todo Do this better */
		strcpy(new_map->target, dom_string_data(target));
	}
	
	if (new_map->type != IMAGEMAP_DEFAULT) {
		int x, y;
		float *xcoords, *ycoords;
		/* coordinates are a comma-separated list of values */
		char *val = strtok((char *)dom_string_data(coords), ",");
		int num = 1;

		switch (new_map->type) {
		case IMAGEMAP_RECT:
			/* (left, top, right, bottom) */
			while (val != NULL && num <= 4) {
				switch (num) {
				case 1:
					new_map->bounds.rect.x0 = atoi(val);
					break;
				case 2:
					new_map->bounds.rect.y0 = atoi(val);
					break;
				case 3:
					new_map->bounds.rect.x1 = atoi(val);
					break;
				case 4:
					new_map->bounds.rect.y1 = atoi(val);
					break;
				}

				num++;
				val = strtok(NULL, ",");
			}
			break;
		case IMAGEMAP_CIRCLE:
			/* (x, y, radius ) */
			while (val != NULL && num <= 3) {
				switch (num) {
				case 1:
					new_map->bounds.circle.x = atoi(val);
					break;
				case 2:
					new_map->bounds.circle.y = atoi(val);
					break;
				case 3:
					new_map->bounds.circle.r = atoi(val);
					break;
				}

				num++;
				val = strtok(NULL, ",");
			}
			break;
		case IMAGEMAP_POLY:
			new_map->bounds.poly.xcoords = NULL;
			new_map->bounds.poly.ycoords = NULL;

			while (val != NULL) {
				x = atoi(val);

				val = strtok(NULL, ",");
				if (val == NULL)
					break;

				y = atoi(val);

				xcoords = realloc(new_map->bounds.poly.xcoords,
						num * sizeof(float));
				if (xcoords == NULL) {
					goto bad_out;
				}
				new_map->bounds.poly.xcoords = xcoords;

				ycoords = realloc(new_map->bounds.poly.ycoords,
					num * sizeof(float));
				if (ycoords == NULL) {
					goto bad_out;
				}
				new_map->bounds.poly.ycoords = ycoords;

				new_map->bounds.poly.xcoords[num - 1] = x;
				new_map->bounds.poly.ycoords[num - 1] = y;

				num++;
				val = strtok(NULL, ",");
			}

			new_map->bounds.poly.num = num - 1;

			break;
		default:
			break;
		}
	}
	
	new_map->next = NULL;

	if (entry && *entry) {
		/* add to END of list */
		for (temp = (*entry); temp->next != NULL; temp = temp->next)
			;
		temp->next = new_map;
	}
	else {
		(*entry) = new_map;
	}
	
	/* All good, linked in, let's clean up */
	goto ok_out;
	
bad_out:
	ret = false;
ok_free_map_out:
	if (new_map != NULL) {
		if (new_map->url != NULL)
			nsurl_unref(new_map->url);
		if (new_map->type == IMAGEMAP_POLY &&
				new_map->bounds.poly.ycoords != NULL)
			free(new_map->bounds.poly.ycoords);
		if (new_map->type == IMAGEMAP_POLY &&
				new_map->bounds.poly.xcoords != NULL)
			free(new_map->bounds.poly.xcoords);
		if (new_map->target != NULL)
			free(new_map->target);

		free(new_map);
	}
ok_out:
	if (href != NULL)
		dom_string_unref(href);
	if (target != NULL)
		dom_string_unref(target);
	if (shape != NULL)
		dom_string_unref(shape);
	if (coords != NULL)
		dom_string_unref(coords);
	
	return ret;
}

/**
 * Free list of imagemap entries
 *
 * \param list Pointer to head of list
 */
void imagemap_freelist(struct mapentry *list)
{
	struct mapentry *entry, *prev;

	assert(list != NULL);

	entry = list;

	while (entry != NULL) {
		prev = entry;

		nsurl_unref(entry->url);

		if (entry->target)
			free(entry->target);

		if (entry->type == IMAGEMAP_POLY) {
			free(entry->bounds.poly.xcoords);
			free(entry->bounds.poly.ycoords);
		}

		entry = entry->next;
		free(prev);
	}
}

/**
 * Retrieve url associated with imagemap entry
 *
 * \param h        The containing content
 * \param key      The map name to search for
 * \param x        The left edge of the containing box
 * \param y        The top edge of the containing box
 * \param click_x  The horizontal location of the click
 * \param click_y  The vertical location of the click
 * \param target   Pointer to location to receive target pointer (if any)
 * \return The url associated with this area, or NULL if not found
 */
nsurl *imagemap_get(struct html_content *c, const char *key,
		unsigned long x, unsigned long y,
		unsigned long click_x, unsigned long click_y,
		const char **target)
{
	unsigned int slot = 0;
	struct imagemap *map;
	struct mapentry *entry;
	unsigned long cx, cy;

	assert(c != NULL);

	if (key == NULL)
		return NULL;

	if (c->imagemaps == NULL)
		return NULL;

	slot = imagemap_hash(key);

	for (map = c->imagemaps[slot]; map != NULL; map = map->next) {
		if (map->key != NULL && strcasecmp(map->key, key) == 0)
			break;
	}

	if (map == NULL || map->list == NULL)
		return NULL;

	for (entry = map->list; entry; entry = entry->next) {
		switch (entry->type) {
		case IMAGEMAP_DEFAULT:
			/* just return the URL. no checks required */
			if (target)
				*target = entry->target;
			return entry->url;
			break;
		case IMAGEMAP_RECT:
			if (click_x >= x + entry->bounds.rect.x0 &&
				    click_x <= x + entry->bounds.rect.x1 &&
				    click_y >= y + entry->bounds.rect.y0 &&
				    click_y <= y + entry->bounds.rect.y1) {
				if (target)
					*target = entry->target;
				return entry->url;
			}
			break;
		case IMAGEMAP_CIRCLE:
			cx = x + entry->bounds.circle.x - click_x;
			cy = y + entry->bounds.circle.y - click_y;
			if ((cx * cx + cy * cy) <=
				(unsigned long) (entry->bounds.circle.r *
					entry->bounds.circle.r)) {
				if (target)
					*target = entry->target;
				return entry->url;
			}
			break;
		case IMAGEMAP_POLY:
			if (imagemap_point_in_poly(entry->bounds.poly.num,
					entry->bounds.poly.xcoords,
					entry->bounds.poly.ycoords, x, y,
					click_x, click_y)) {
				if (target)
					*target = entry->target;
				return entry->url;
			}
			break;
		}
	}

	if (target)
		*target = NULL;

	return NULL;
}

/**
 * Hash function
 *
 * \param key The key to hash
 * \return The hashed value
 */
unsigned int imagemap_hash(const char *key)
{
	unsigned int z = 0;

	if (key == 0) return 0;

	for (; *key != 0; key++) {
		z += *key & 0x1f;
	}

	return (z % (HASH_SIZE - 1)) + 1;
}

/**
 * Test if a point lies within an arbitrary polygon
 * Modified from comp.graphics.algorithms FAQ 2.03
 *
 * \param num Number of vertices
 * \param xpt Array of x coordinates
 * \param ypt Array of y coordinates
 * \param x Left hand edge of containing box
 * \param y Top edge of containing box
 * \param click_x X coordinate of click
 * \param click_y Y coordinate of click
 * \return 1 if point is in polygon, 0 if outside. 0 or 1 if on boundary
 */
int imagemap_point_in_poly(int num, float *xpt, float *ypt, unsigned long x,
		unsigned long y, unsigned long click_x,
		unsigned long click_y)
{
	int i, j, c = 0;

	assert(xpt != NULL);
	assert(ypt != NULL);

	for (i = 0, j = num - 1; i < num; j = i++) {
		if ((((ypt[i] + y <= click_y) && (click_y < ypt[j] + y)) ||
		     ((ypt[j] + y <= click_y) && (click_y < ypt[i] + y))) &&
		     (click_x < (xpt[j] - xpt[i]) *
		     (click_y - (ypt[i] + y)) / (ypt[j] - ypt[i]) + xpt[i] + x))
			c = !c;
	}

	return c;
}
