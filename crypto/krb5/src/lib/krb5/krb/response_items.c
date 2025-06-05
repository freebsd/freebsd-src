/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/response_items.c - Response items */
/*
 * Copyright 2012 Red Hat, Inc.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of Red Hat not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original Red Hat software.
 * Red Hat makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include "k5-int.h"
#include "int-proto.h"

struct k5_response_items_st {
    size_t count;
    char **questions;
    char **challenges;
    char **answers;
};

krb5_error_code
k5_response_items_new(k5_response_items **ri_out)
{
    *ri_out = calloc(1, sizeof(**ri_out));
    return (*ri_out == NULL) ? ENOMEM : 0;
}

void
k5_response_items_free(k5_response_items *ri)
{
    k5_response_items_reset(ri);
    free(ri);
}

void
k5_response_items_reset(k5_response_items *ri)
{
    size_t i;

    if (ri == NULL)
        return;

    for (i = 0; i < ri->count; i++)
        free(ri->questions[i]);
    free(ri->questions);
    ri->questions = NULL;

    for (i = 0; i < ri->count; i++)
        zapfreestr(ri->challenges[i]);
    free(ri->challenges);
    ri->challenges = NULL;

    for (i = 0; i < ri->count; i++)
        zapfreestr(ri->answers[i]);
    free(ri->answers);
    ri->answers = NULL;

    ri->count = 0;
}

krb5_boolean
k5_response_items_empty(const k5_response_items *ri)
{
    return ri == NULL ? TRUE : ri->count == 0;
}

const char * const *
k5_response_items_list_questions(const k5_response_items *ri)
{
    if (ri == NULL)
        return NULL;
    return (const char * const *)ri->questions;
}

static ssize_t
find_question(const k5_response_items *ri, const char *question)
{
    size_t i;

    if (ri == NULL)
        return -1;

    for (i = 0; i < ri->count; i++) {
        if (strcmp(ri->questions[i], question) == 0)
            return i;
    }

    return -1;
}

static krb5_error_code
push_question(k5_response_items *ri, const char *question,
              const char *challenge)
{
    char **tmp;
    size_t size;

    if (ri == NULL)
        return EINVAL;

    size = sizeof(char *) * (ri->count + 2);
    tmp = realloc(ri->questions, size);
    if (tmp == NULL)
        return ENOMEM;
    ri->questions = tmp;
    ri->questions[ri->count] = NULL;
    ri->questions[ri->count + 1] = NULL;

    tmp = realloc(ri->challenges, size);
    if (tmp == NULL)
        return ENOMEM;
    ri->challenges = tmp;
    ri->challenges[ri->count] = NULL;
    ri->challenges[ri->count + 1] = NULL;

    tmp = realloc(ri->answers, size);
    if (tmp == NULL)
        return ENOMEM;
    ri->answers = tmp;
    ri->answers[ri->count] = NULL;
    ri->answers[ri->count + 1] = NULL;

    ri->questions[ri->count] = strdup(question);
    if (ri->questions[ri->count] == NULL)
        return ENOMEM;

    if (challenge != NULL) {
        ri->challenges[ri->count] = strdup(challenge);
        if (ri->challenges[ri->count] == NULL) {
            free(ri->questions[ri->count]);
            ri->questions[ri->count] = NULL;
            return ENOMEM;
        }
    }

    ri->count++;
    return 0;
}

krb5_error_code
k5_response_items_ask_question(k5_response_items *ri, const char *question,
                               const char *challenge)
{
    ssize_t i;
    char *tmp = NULL;

    i = find_question(ri, question);
    if (i < 0)
        return push_question(ri, question, challenge);

    if (challenge != NULL) {
        tmp = strdup(challenge);
        if (tmp == NULL)
            return ENOMEM;
    }

    zapfreestr(ri->challenges[i]);
    ri->challenges[i] = tmp;
    return 0;
}

const char *
k5_response_items_get_challenge(const k5_response_items *ri,
                                const char *question)
{
    ssize_t i;

    i = find_question(ri, question);
    if (i < 0)
        return NULL;

    return ri->challenges[i];
}

krb5_error_code
k5_response_items_set_answer(k5_response_items *ri, const char *question,
                             const char *answer)
{
    char *tmp = NULL;
    ssize_t i;

    i = find_question(ri, question);
    if (i < 0)
        return EINVAL;

    if (answer != NULL) {
        tmp = strdup(answer);
        if (tmp == NULL)
            return ENOMEM;
    }

    zapfreestr(ri->answers[i]);
    ri->answers[i] = tmp;
    return 0;
}

const char *
k5_response_items_get_answer(const k5_response_items *ri,
                             const char *question)
{
    ssize_t i;

    i = find_question(ri, question);
    if (i < 0)
        return NULL;

    return ri->answers[i];
}
