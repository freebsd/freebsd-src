/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * prof_tree.c --- these routines maintain the parse tree of the
 *      config file.
 *
 * All of the details of how the tree is stored is abstracted away in
 * this file; all of the other profile routines build, access, and
 * modify the tree via the accessor functions found in this file.
 *
 * Each node may represent either a relation or a section header.
 *
 * A section header must have its value field set to 0, and may a one
 * or more child nodes, pointed to by first_child.
 *
 * A relation has as its value a pointer to allocated memory
 * containing a string.  Its first_child pointer must be null.
 *
 */


#include "prof_int.h"

#include <stdio.h>
#include <string.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <errno.h>
#include <ctype.h>

struct profile_node {
    errcode_t       magic;
    char *name;
    char *value;
    int group_level;
    unsigned int final:1;           /* Indicate don't search next file */
    unsigned int deleted:1;
    struct profile_node *first_child;
    struct profile_node *parent;
    struct profile_node *next, *prev;
};

#define CHECK_MAGIC(node)                       \
    if ((node)->magic != PROF_MAGIC_NODE)       \
        return PROF_MAGIC_NODE;

/*
 * Free a node, and any children
 */
void profile_free_node(struct profile_node *node)
{
    struct profile_node *child, *next;

    if (node->magic != PROF_MAGIC_NODE)
        return;

    if (node->name)
        free(node->name);
    if (node->value)
        free(node->value);

    for (child=node->first_child; child; child = next) {
        next = child->next;
        profile_free_node(child);
    }
    node->magic = 0;

    free(node);
}

#ifndef HAVE_STRDUP
#undef strdup
#define strdup MYstrdup
static char *MYstrdup (const char *s)
{
    size_t sz = strlen(s) + 1;
    char *p = malloc(sz);
    if (p != 0)
        memcpy(p, s, sz);
    return p;
}
#endif

/*
 * Create a node
 */
errcode_t profile_create_node(const char *name, const char *value,
                              struct profile_node **ret_node)
{
    struct profile_node *new;

    new = malloc(sizeof(struct profile_node));
    if (!new)
        return ENOMEM;
    memset(new, 0, sizeof(struct profile_node));
    /* Set magic here so profile_free_node will free memory */
    new->magic = PROF_MAGIC_NODE;
    new->name = strdup(name);
    if (new->name == 0) {
        profile_free_node(new);
        return ENOMEM;
    }
    if (value) {
        new->value = strdup(value);
        if (new->value == 0) {
            profile_free_node(new);
            return ENOMEM;
        }
    }

    *ret_node = new;
    return 0;
}

/*
 * This function verifies that all of the representation invarients of
 * the profile are true.  If not, we have a programming bug somewhere,
 * probably in this file.
 */
errcode_t profile_verify_node(struct profile_node *node)
{
    struct profile_node *p, *last;
    errcode_t       retval;

    CHECK_MAGIC(node);

    if (node->value && node->first_child)
        return PROF_SECTION_WITH_VALUE;

    last = 0;
    for (p = node->first_child; p; last = p, p = p->next) {
        if (p->prev != last)
            return PROF_BAD_LINK_LIST;
        if (last && (last->next != p))
            return PROF_BAD_LINK_LIST;
        if (node->group_level+1 != p->group_level)
            return PROF_BAD_GROUP_LVL;
        if (p->parent != node)
            return PROF_BAD_PARENT_PTR;
        retval = profile_verify_node(p);
        if (retval)
            return retval;
    }
    return 0;
}

/*
 * Add a node to a particular section
 */
errcode_t profile_add_node(struct profile_node *section, const char *name,
                           const char *value, struct profile_node **ret_node)
{
    errcode_t retval;
    struct profile_node *p, *last, *new;

    CHECK_MAGIC(section);

    if (section->value)
        return PROF_ADD_NOT_SECTION;

    /*
     * Find the place to insert the new node.  We look for the
     * place *after* the last match of the node name, since
     * order matters.
     */
    for (p=section->first_child, last = 0; p; last = p, p = p->next) {
        int cmp;
        cmp = strcmp(p->name, name);
        if (cmp > 0)
            break;
    }
    retval = profile_create_node(name, value, &new);
    if (retval)
        return retval;
    new->group_level = section->group_level+1;
    new->deleted = 0;
    new->parent = section;
    new->prev = last;
    new->next = p;
    if (p)
        p->prev = new;
    if (last)
        last->next = new;
    else
        section->first_child = new;
    if (ret_node)
        *ret_node = new;
    return 0;
}

/*
 * Set the final flag on a particular node.
 */
errcode_t profile_make_node_final(struct profile_node *node)
{
    CHECK_MAGIC(node);

    node->final = 1;
    return 0;
}

/*
 * Check the final flag on a node
 */
int profile_is_node_final(struct profile_node *node)
{
    return (node->final != 0);
}

/*
 * Return the name of a node.  (Note: this is for internal functions
 * only; if the name needs to be returned from an exported function,
 * strdup it first!)
 */
const char *profile_get_node_name(struct profile_node *node)
{
    return node->name;
}

/*
 * Return the value of a node.  (Note: this is for internal functions
 * only; if the name needs to be returned from an exported function,
 * strdup it first!)
 */
const char *profile_get_node_value(struct profile_node *node)
{
    return node->value;
}

/*
 * Iterate through the section, returning the nodes which match
 * the given name.  If name is NULL, then interate through all the
 * nodes in the section.  If section_flag is non-zero, only return the
 * section which matches the name; don't return relations.  If value
 * is non-NULL, then only return relations which match the requested
 * value.  (The value argument is ignored if section_flag is non-zero.)
 *
 * The first time this routine is called, the state pointer must be
 * null.  When this profile_find_node_relation() returns, if the state
 * pointer is non-NULL, then this routine should be called again.
 * (This won't happen if section_flag is non-zero, obviously.)
 *
 */
errcode_t profile_find_node(struct profile_node *section, const char *name,
                            const char *value, int section_flag, void **state,
                            struct profile_node **node)
{
    struct profile_node *p;

    CHECK_MAGIC(section);
    p = *state;
    if (p) {
        CHECK_MAGIC(p);
    } else
        p = section->first_child;

    for (; p; p = p->next) {
        if (name && (strcmp(p->name, name)))
            continue;
        if (section_flag) {
            if (p->value)
                continue;
        } else {
            if (!p->value)
                continue;
            if (value && (strcmp(p->value, value)))
                continue;
        }
        if (p->deleted)
            continue;
        /* A match! */
        if (node)
            *node = p;
        break;
    }
    if (p == 0) {
        *state = 0;
        return section_flag ? PROF_NO_SECTION : PROF_NO_RELATION;
    }
    /*
     * OK, we've found one match; now let's try to find another
     * one.  This way, if we return a non-zero state pointer,
     * there's guaranteed to be another match that's returned.
     */
    for (p = p->next; p; p = p->next) {
        if (name && (strcmp(p->name, name)))
            continue;
        if (section_flag) {
            if (p->value)
                continue;
        } else {
            if (!p->value)
                continue;
            if (value && (strcmp(p->value, value)))
                continue;
        }
        if (p->deleted)
            continue;
        /* A match! */
        break;
    }
    *state = p;
    return 0;
}


/*
 * Iterate through the section, returning the relations which match
 * the given name.  If name is NULL, then interate through all the
 * relations in the section.  The first time this routine is called,
 * the state pointer must be null.  When this profile_find_node_relation()
 * returns, if the state pointer is non-NULL, then this routine should
 * be called again.
 *
 * The returned character string in value points to the stored
 * character string in the parse string.  Before this string value is
 * returned to a calling application (profile_find_node_relation is not an
 * exported interface), it should be strdup()'ed.
 */
errcode_t profile_find_node_relation(struct profile_node *section,
                                     const char *name, void **state,
                                     char **ret_name, char **value)
{
    struct profile_node *p;
    errcode_t       retval;

    retval = profile_find_node(section, name, 0, 0, state, &p);
    if (retval)
        return retval;

    if (p) {
        if (value)
            *value = p->value;
        if (ret_name)
            *ret_name = p->name;
    }
    return 0;
}

/*
 * Iterate through the section, returning the subsections which match
 * the given name.  If name is NULL, then interate through all the
 * subsections in the section.  The first time this routine is called,
 * the state pointer must be null.  When this profile_find_node_subsection()
 * returns, if the state pointer is non-NULL, then this routine should
 * be called again.
 *
 * This is (plus accessor functions for the name and value given a
 * profile node) makes this function mostly syntactic sugar for
 * profile_find_node.
 */
errcode_t profile_find_node_subsection(struct profile_node *section,
                                       const char *name, void **state,
                                       char **ret_name,
                                       struct profile_node **subsection)
{
    struct profile_node *p;
    errcode_t       retval;

    retval = profile_find_node(section, name, 0, 1, state, &p);
    if (retval)
        return retval;

    if (p) {
        if (subsection)
            *subsection = p;
        if (ret_name)
            *ret_name = p->name;
    }
    return 0;
}

/*
 * This function returns the parent of a particular node.
 */
errcode_t profile_get_node_parent(struct profile_node *section,
                                  struct profile_node **parent)
{
    *parent = section->parent;
    return 0;
}

/*
 * This is a general-purpose iterator for returning all nodes that
 * match the specified name array.
 */
struct profile_node_iterator {
    prf_magic_t             magic;
    int                     flags;
    const char              *const *names;
    const char              *name;
    prf_file_t              file;
    int                     file_serial;
    int                     done_idx;
    struct profile_node     *node;
    int                     num;
};

errcode_t profile_node_iterator_create(profile_t profile,
                                       const char *const *names, int flags,
                                       void **ret_iter)
{
    struct profile_node_iterator *iter;
    int     done_idx = 0;

    if (profile == 0)
        return PROF_NO_PROFILE;
    if (profile->magic != PROF_MAGIC_PROFILE)
        return PROF_MAGIC_PROFILE;
    if (!names)
        return PROF_BAD_NAMESET;
    if (!(flags & PROFILE_ITER_LIST_SECTION)) {
        if (!names[0])
            return PROF_BAD_NAMESET;
        done_idx = 1;
    }

    iter = malloc(sizeof(*iter));
    if (iter == NULL)
        return ENOMEM;

    iter->magic = PROF_MAGIC_NODE_ITERATOR;
    iter->names = names;
    iter->flags = flags;
    iter->file = profile->first_file;
    iter->done_idx = done_idx;
    iter->node = 0;
    iter->num = 0;
    *ret_iter = iter;
    return 0;
}

void profile_node_iterator_free(void **iter_p)
{
    struct profile_node_iterator *iter;

    if (!iter_p)
        return;
    iter = *iter_p;
    if (!iter || iter->magic != PROF_MAGIC_NODE_ITERATOR)
        return;
    free(iter);
    *iter_p = 0;
}

/*
 * Note: the returned character strings in ret_name and ret_value
 * points to the stored character string in the parse string.  Before
 * this string value is returned to a calling application
 * (profile_node_iterator is not an exported interface), it should be
 * strdup()'ed.
 */
errcode_t profile_node_iterator(void **iter_p,
                                struct profile_node **ret_node,
                                char **ret_name, char **ret_value)
{
    struct profile_node_iterator    *iter = *iter_p;
    struct profile_node             *section, *p;
    const char                      *const *cpp;
    errcode_t                       retval;
    int                             skip_num = 0;

    if (!iter || iter->magic != PROF_MAGIC_NODE_ITERATOR)
        return PROF_MAGIC_NODE_ITERATOR;
    if (iter->file && iter->file->magic != PROF_MAGIC_FILE)
        return PROF_MAGIC_FILE;
    if (iter->file && iter->file->data->magic != PROF_MAGIC_FILE_DATA)
        return PROF_MAGIC_FILE_DATA;
    /*
     * If the file has changed, then the node pointer is invalid,
     * so we'll have search the file again looking for it.
     */
    if (iter->file)
        k5_mutex_lock(&iter->file->data->lock);
    if (iter->node && (iter->file->data->upd_serial != iter->file_serial)) {
        iter->flags &= ~PROFILE_ITER_FINAL_SEEN;
        skip_num = iter->num;
        iter->node = 0;
    }
    if (iter->node && iter->node->magic != PROF_MAGIC_NODE) {
        if (iter->file)
            k5_mutex_unlock(&iter->file->data->lock);
        return PROF_MAGIC_NODE;
    }
get_new_file:
    if (iter->node == 0) {
        if (iter->file == 0 ||
            (iter->flags & PROFILE_ITER_FINAL_SEEN)) {
            if (iter->file)
                k5_mutex_unlock(&iter->file->data->lock);
            profile_node_iterator_free(iter_p);
            if (ret_node)
                *ret_node = 0;
            if (ret_name)
                *ret_name = 0;
            if (ret_value)
                *ret_value =0;
            return 0;
        }
        if ((retval = profile_update_file_locked(iter->file, NULL))) {
            k5_mutex_unlock(&iter->file->data->lock);
            if (retval == ENOENT || retval == EACCES) {
                /* XXX memory leak? */
                iter->file = iter->file->next;
                if (iter->file)
                    k5_mutex_lock(&iter->file->data->lock);
                skip_num = 0;
                retval = 0;
                goto get_new_file;
            } else {
                profile_node_iterator_free(iter_p);
                return retval;
            }
        }
        iter->file_serial = iter->file->data->upd_serial;
        /*
         * Find the section to list if we are a LIST_SECTION,
         * or find the containing section if not.
         */
        section = iter->file->data->root;
        assert(section != NULL);
        for (cpp = iter->names; cpp[iter->done_idx]; cpp++) {
            for (p=section->first_child; p; p = p->next) {
                if (!strcmp(p->name, *cpp) && !p->value && !p->deleted)
                    break;
            }
            if (!p) {
                section = 0;
                break;
            }
            section = p;
            if (p->final)
                iter->flags |= PROFILE_ITER_FINAL_SEEN;
        }
        if (!section) {
            k5_mutex_unlock(&iter->file->data->lock);
            iter->file = iter->file->next;
            if (iter->file)
                k5_mutex_lock(&iter->file->data->lock);
            skip_num = 0;
            goto get_new_file;
        }
        iter->name = *cpp;
        iter->node = section->first_child;
    }
    /*
     * OK, now we know iter->node is set up correctly.  Let's do
     * the search.
     */
    for (p = iter->node; p; p = p->next) {
        if (iter->name && strcmp(p->name, iter->name))
            continue;
        if ((iter->flags & PROFILE_ITER_SECTIONS_ONLY) &&
            p->value)
            continue;
        if ((iter->flags & PROFILE_ITER_RELATIONS_ONLY) &&
            !p->value)
            continue;
        if (skip_num > 0) {
            skip_num--;
            continue;
        }
        if (p->deleted)
            continue;
        break;
    }
    iter->num++;
    if (!p) {
        k5_mutex_unlock(&iter->file->data->lock);
        iter->file = iter->file->next;
        if (iter->file)
            k5_mutex_lock(&iter->file->data->lock);
        iter->node = 0;
        skip_num = 0;
        goto get_new_file;
    }
    k5_mutex_unlock(&iter->file->data->lock);
    if ((iter->node = p->next) == NULL)
        iter->file = iter->file->next;
    if (ret_node)
        *ret_node = p;
    if (ret_name)
        *ret_name = p->name;
    if (ret_value)
        *ret_value = p->value;
    return 0;
}

/*
 * Remove a particular node.
 *
 * TYT, 2/25/99
 */
errcode_t profile_remove_node(struct profile_node *node)
{
    CHECK_MAGIC(node);

    if (node->parent == 0)
        return PROF_EINVAL; /* Can't remove the root! */

    node->deleted = 1;

    return 0;
}

/*
 * Set the value of a specific node containing a relation.
 *
 * TYT, 2/25/99
 */
errcode_t profile_set_relation_value(struct profile_node *node,
                                     const char *new_value)
{
    char    *cp;

    CHECK_MAGIC(node);

    if (!node->value)
        return PROF_SET_SECTION_VALUE;

    cp = strdup(new_value);
    if (!cp)
        return ENOMEM;

    free(node->value);
    node->value = cp;

    return 0;
}

/*
 * Rename a specific node
 *
 * TYT 2/25/99
 */
errcode_t profile_rename_node(struct profile_node *node, const char *new_name)
{
    char                    *new_string;
    struct profile_node     *p, *last;

    CHECK_MAGIC(node);

    if (strcmp(new_name, node->name) == 0)
        return 0;       /* It's the same name, return */

    /*
     * Make sure we can allocate memory for the new name, first!
     */
    new_string = strdup(new_name);
    if (!new_string)
        return ENOMEM;

    /*
     * Find the place to where the new node should go.  We look
     * for the place *after* the last match of the node name,
     * since order matters.
     */
    for (p=node->parent->first_child, last = 0; p; last = p, p = p->next) {
        if (strcmp(p->name, new_name) > 0)
            break;
    }

    /*
     * If we need to move the node, do it now.
     */
    if ((p != node) && (last != node)) {
        /*
         * OK, let's detach the node
         */
        if (node->prev)
            node->prev->next = node->next;
        else
            node->parent->first_child = node->next;
        if (node->next)
            node->next->prev = node->prev;

        /*
         * Now let's reattach it in the right place.
         */
        if (p)
            p->prev = node;
        if (last)
            last->next = node;
        else
            node->parent->first_child = node;
        node->next = p;
        node->prev = last;
    }

    free(node->name);
    node->name = new_string;
    return 0;
}
