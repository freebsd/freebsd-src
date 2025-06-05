/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/kdb/ldap/ldap_util/kdb5_ldap_list.c */
/* Copyright (c) 2004-2005, Novell, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *   * The copyright holder's name is not used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Miscellaneous functions for managing the string and integer lists
 */

#include <k5-int.h>
#include "kdb5_ldap_list.h"

/*
 * Counts the number of entries in the given array of strings
 */
int
list_count_str_array(char **list)
{
    int i = 0;

    if (list == NULL)
        return 0;

    for (i = 0; *list != NULL; list++) {
        i++;
    }

    return i;
}


/*
 * Counts the number of entries in the given array of integers
 */
int
list_count_int_array(int *list)
{
    int i = 0;

    if (list == NULL)
        return 0;

    for (i = 0; *list != END_OF_LIST; list++) {
        i++;
    }

    return i;
}


/*
 * Frees the entries in a given list and not the list pointer
 */
void
krb5_free_list_entries(char **list)
{
    if (list == NULL)
        return;
    for (; *list != NULL; list++) {
        free(*list);
        *list = NULL;
    }

    return;
}


/*
 * Tokenize the given string based on the delimiter provided
 * and return the result as a list
 */
krb5_error_code
krb5_parse_list(char *buffer, char *delimiter, char **list)
{
    char *str = NULL;
    char *token = NULL;
    char *ptrptr = NULL;
    char **plist = list;
    krb5_error_code retval = 0;
    int count = 0;

    if ((buffer == NULL) || (list == NULL) || (delimiter == NULL)) {
        return EINVAL;
    }

    str = strdup(buffer);
    if (str == NULL)
        return ENOMEM;

    token = strtok_r(str, delimiter, &ptrptr);
    for (count = 1; ((token != NULL) && (count < MAX_LIST_ENTRIES));
         plist++, count++) {
        *plist = strdup(token);
        if (*plist == NULL) {
            retval = ENOMEM;
            goto cleanup;
        }
        token = strtok_r(NULL, delimiter, &ptrptr);
    }
    *plist = NULL;

cleanup:
    if (str) {
        free(str);
        str = NULL;
    }
    if (retval)
        krb5_free_list_entries(list);

    return retval;
}


int
compare_int(const void *m1, const void *m2)
{
    int mi1 = *(const int *)m1;
    int mi2 = *(const int *)m2;

    return (mi1 - mi2);
}


/*
 * Modifies the destination list to contain or not to contain the
 * entries present in the source list, depending on the mode
 * (ADD or DELETE).
 */
void
list_modify_str_array(char ***destlist, const char **sourcelist, int mode)
{
    char **dlist = NULL, **tmplist = NULL;
    const char **slist = NULL;
    int dcount = 0, scount = 0, copycount = 0;

    if ((destlist == NULL) || (*destlist == NULL) || (sourcelist == NULL))
        return;

    /* We need to add every entry present in the source list to
     * the destination list */
    if (mode == LIST_MODE_ADD) {
        /* Traverse through the end of destlist for appending */
        for (dlist = *destlist, dcount = 0; *dlist != NULL;
             dlist++, dcount++) {
            ;   /* NULL statement */
        }
        /* Count the number of entries in the source list */
        for (slist = sourcelist, scount = 0; *slist != NULL;
             slist++, scount++) {
            ;   /* NULL statement */
        }
        /* Reset the slist pointer to the start of source list */
        slist = sourcelist;

        /* Now append the source list to the existing destlist */
        if ((dcount + scount) < MAX_LIST_ENTRIES)
            copycount = scount;
        else
            /* Leave the last entry for list terminator(=NULL) */
            copycount = (MAX_LIST_ENTRIES -1) - dcount;

        memcpy(dlist, slist, (sizeof(char *) * copycount));
        dlist += copycount;
        *dlist = NULL;
    } else if (mode == LIST_MODE_DELETE) {
        /* We need to delete every entry present in the source list
         * from the destination list */
        for (slist = sourcelist; *slist != NULL; slist++) {
            for (dlist = *destlist; *dlist != NULL; dlist++) {
                /* DN is case insensitive string */
                if (strcasecmp(*dlist, *slist) == 0) {
                    free(*dlist);
                    /* Advance the rest of the entries by one */
                    for (tmplist = dlist; *tmplist != NULL; tmplist++) {
                        *tmplist = *(tmplist+1);
                    }
                    break;
                }
            }
        }
    }

    return;
}


/*
 * Modifies the destination list to contain or not to contain the
 * entries present in the source list, depending on the mode
 * (ADD or DELETE). where the list is array of integers.
 */
int
list_modify_int_array(int *destlist, const int *sourcelist, int mode)
{
    int *dlist = NULL, *tmplist = NULL;
    const int *slist = NULL;
    int dcount = 0, scount = 0, copycount = 0;
    int tcount = 0;

    if ((destlist == NULL) || (sourcelist == NULL))
        return 0;

    /* We need to add every entry present in the source list to the
     * destination list */
    if (mode == LIST_MODE_ADD) {
        /* Traverse through the end of destlist for appending */
        for (dlist = destlist, dcount = 0; *dlist != END_OF_LIST;
             dlist++, dcount++)
            ;   /* NULL statement */

        /* Count the number of entries in the source list */
        for (slist = sourcelist, scount = 0; *slist != END_OF_LIST;
             slist++, scount++)
            ;   /* NULL statement */

        /* Reset the slist pointer to the start of source list */
        slist = sourcelist;

        /* Now append the source list to the existing destlist */
        if ((dcount + scount) < MAX_LIST_ENTRIES)
            copycount = scount;
        else
            /* Leave the last entry for list terminator(=NULL) */
            copycount = (MAX_LIST_ENTRIES -1) - dcount;

        memcpy(dlist, slist, (sizeof(int) * copycount));
        dlist += copycount;
        *dlist = END_OF_LIST;
        tcount = dcount + copycount;
    } else if (mode == LIST_MODE_DELETE) {
        /* We need to delete every entry present in the source list from
         * the destination list */
        for (slist = sourcelist; *slist != END_OF_LIST; slist++) {
            for (dlist = destlist; *dlist != END_OF_LIST; dlist++) {
                if (*dlist == *slist) {
                    /* Advance the rest of the entries by one */
                    for (tmplist = dlist; *tmplist != END_OF_LIST; tmplist++) {
                        *tmplist = *(tmplist+1);
                    }
                    break;
                }
            }
        }
        /* count the number of entries */
        for (dlist = destlist, tcount = 0; *dlist != END_OF_LIST; dlist++) {
            tcount++;
        }
    }

    return tcount;
}
