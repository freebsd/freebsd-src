/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Modified to use APR and APR pools.
 *  TODO: Is malloc() better? Will long running skiplists grow too much?
 *  Keep the skiplist_alloc() and skiplist_free() until we know
 *  Yeah, if using pools it means some bogus cycles for checks
 *  (and an useless function call for skiplist_free) which we
 *  can removed if/when needed.
 */

#include "apr_skiplist.h"

struct apr_skiplist {
    apr_skiplist_compare compare;
    apr_skiplist_compare comparek;
    int height;
    int preheight;
    int size;
    apr_skiplistnode *top;
    apr_skiplistnode *bottom;
    /* These two are needed for appending */
    apr_skiplistnode *topend;
    apr_skiplistnode *bottomend;
    apr_skiplist *index;
    apr_array_header_t *memlist;
    apr_pool_t *pool;
};

struct apr_skiplistnode {
    void *data;
    apr_skiplistnode *next;
    apr_skiplistnode *prev;
    apr_skiplistnode *down;
    apr_skiplistnode *up;
    apr_skiplistnode *previndex;
    apr_skiplistnode *nextindex;
    apr_skiplist *sl;
};

#ifndef MIN
#define MIN(a,b) ((a<b)?(a):(b))
#endif

static int get_b_rand(void)
{
    static int ph = 32;         /* More bits than we will ever use */
    static apr_uint32_t randseq;
    if (ph > 31) {              /* Num bits in return of rand() */
        ph = 0;
        randseq = (apr_uint32_t) rand();
    }
    ph++;
    return ((randseq & (1 << (ph - 1))) >> (ph - 1));
}

typedef struct {
    size_t size;
    apr_array_header_t *list;
} memlist_t;

typedef struct {
    void *ptr;
    char inuse;
} chunk_t;

APR_DECLARE(void *) apr_skiplist_alloc(apr_skiplist *sl, size_t size)
{
    if (sl->pool) {
        void *ptr;
        int found_size = 0;
        int i;
        chunk_t *newchunk;
        memlist_t *memlist = (memlist_t *)sl->memlist->elts;
        for (i = 0; i < sl->memlist->nelts; i++) {
            if (memlist->size == size) {
                int j;
                chunk_t *chunk = (chunk_t *)memlist->list->elts;
                found_size = 1;
                for (j = 0; j < memlist->list->nelts; j++) {
                    if (!chunk->inuse) {
                        chunk->inuse = 1;
                        return chunk->ptr;
                    }
                    chunk++;
                }
                break; /* no free of this size; punt */
            }
            memlist++;
        }
        /* no free chunks */
        ptr = apr_pcalloc(sl->pool, size);
        if (!ptr) {
            return ptr;
        }
        /*
         * is this a new sized chunk? If so, we need to create a new
         * array of them. Otherwise, re-use what we already have.
         */
        if (!found_size) {
            memlist = apr_array_push(sl->memlist);
            memlist->size = size;
            memlist->list = apr_array_make(sl->pool, 20, sizeof(chunk_t));
        }
        newchunk = apr_array_push(memlist->list);
        newchunk->ptr = ptr;
        newchunk->inuse = 1;
        return ptr;
    }
    else {
        return calloc(1, size);
    }
}

APR_DECLARE(void) apr_skiplist_free(apr_skiplist *sl, void *mem)
{
    if (!sl->pool) {
        free(mem);
    }
    else {
        int i;
        memlist_t *memlist = (memlist_t *)sl->memlist->elts;
        for (i = 0; i < sl->memlist->nelts; i++) {
            int j;
            chunk_t *chunk = (chunk_t *)memlist->list->elts;
            for (j = 0; j < memlist->list->nelts; j++) {
                if (chunk->ptr == mem) {
                    chunk->inuse = 0;
                    return;
                }
                chunk++;
            }
            memlist++;
        }
    }
}

static apr_status_t skiplisti_init(apr_skiplist **s, apr_pool_t *p)
{
    apr_skiplist *sl;
    if (p) {
        sl = apr_pcalloc(p, sizeof(apr_skiplist));
        sl->memlist = apr_array_make(p, 20, sizeof(memlist_t));
    }
    else {
        sl = calloc(1, sizeof(apr_skiplist));
    }
#if 0
    sl->compare = (apr_skiplist_compare) NULL;
    sl->comparek = (apr_skiplist_compare) NULL;
    sl->height = 0;
    sl->preheight = 0;
    sl->size = 0;
    sl->top = NULL;
    sl->bottom = NULL;
    sl->index = NULL;
#endif
    sl->pool = p;
    *s = sl;
    return APR_SUCCESS;
}

static int indexing_comp(void *a, void *b)
{
    void *ac = (void *) (((apr_skiplist *) a)->compare);
    void *bc = (void *) (((apr_skiplist *) b)->compare);
    return ((ac < bc) ? -1 : ((ac > bc) ? 1 : 0));
}

static int indexing_compk(void *ac, void *b)
{
    void *bc = (void *) (((apr_skiplist *) b)->compare);
    return ((ac < bc) ? -1 : ((ac > bc) ? 1 : 0));
}

APR_DECLARE(apr_status_t) apr_skiplist_init(apr_skiplist **s, apr_pool_t *p)
{
    apr_skiplist *sl;
    skiplisti_init(s, p);
    sl = *s;
    skiplisti_init(&(sl->index), p);
    apr_skiplist_set_compare(sl->index, indexing_comp, indexing_compk);
    return APR_SUCCESS;
}

APR_DECLARE(void) apr_skiplist_set_compare(apr_skiplist *sl,
                          apr_skiplist_compare comp,
                          apr_skiplist_compare compk)
{
    if (sl->compare && sl->comparek) {
        apr_skiplist_add_index(sl, comp, compk);
    }
    else {
        sl->compare = comp;
        sl->comparek = compk;
    }
}

APR_DECLARE(void) apr_skiplist_add_index(apr_skiplist *sl,
                        apr_skiplist_compare comp,
                        apr_skiplist_compare compk)
{
    apr_skiplistnode *m;
    apr_skiplist *ni;
    int icount = 0;
    apr_skiplist_find(sl->index, (void *)comp, &m);
    if (m) {
        return;                 /* Index already there! */
    }
    skiplisti_init(&ni, sl->pool);
    apr_skiplist_set_compare(ni, comp, compk);
    /* Build the new index... This can be expensive! */
    m = apr_skiplist_insert(sl->index, ni);
    while (m->prev) {
        m = m->prev;
        icount++;
    }
    for (m = apr_skiplist_getlist(sl); m; apr_skiplist_next(sl, &m)) {
        int j = icount - 1;
        apr_skiplistnode *nsln;
        nsln = apr_skiplist_insert(ni, m->data);
        /* skip from main index down list */
        while (j > 0) {
            m = m->nextindex;
            j--;
        }
        /* insert this node in the indexlist after m */
        nsln->nextindex = m->nextindex;
        if (m->nextindex) {
            m->nextindex->previndex = nsln;
        }
        nsln->previndex = m;
        m->nextindex = nsln;
    }
}

APR_DECLARE(apr_skiplistnode *) apr_skiplist_getlist(apr_skiplist *sl)
{
    if (!sl->bottom) {
        return NULL;
    }
    return sl->bottom->next;
}

APR_DECLARE(void *) apr_skiplist_find(apr_skiplist *sl, void *data, apr_skiplistnode **iter)
{
    void *ret;
    apr_skiplistnode *aiter;
    if (!sl->compare) {
        return 0;
    }
    if (iter) {
        ret = apr_skiplist_find_compare(sl, data, iter, sl->compare);
    }
    else {
        ret = apr_skiplist_find_compare(sl, data, &aiter, sl->compare);
    }
    return ret;
}

static int skiplisti_find_compare(apr_skiplist *sl, void *data,
                           apr_skiplistnode **ret,
                           apr_skiplist_compare comp)
{
    apr_skiplistnode *m = NULL;
    int count = 0;
    m = sl->top;
    while (m) {
        int compared;
        compared = (m->next) ? comp(data, m->next->data) : -1;
        if (compared == 0) {
            m = m->next;
            while (m->down) {
                m = m->down;
            }
            *ret = m;
            return count;
        }
        if ((m->next == NULL) || (compared < 0)) {
            m = m->down;
            count++;
        }
        else {
            m = m->next;
            count++;
        }
    }
    *ret = NULL;
    return count;
}

APR_DECLARE(void *) apr_skiplist_find_compare(apr_skiplist *sli, void *data,
                               apr_skiplistnode **iter,
                               apr_skiplist_compare comp)
{
    apr_skiplistnode *m = NULL;
    apr_skiplist *sl;
    if (comp == sli->compare || !sli->index) {
        sl = sli;
    }
    else {
        apr_skiplist_find(sli->index, (void *)comp, &m);
        sl = (apr_skiplist *) m->data;
    }
    skiplisti_find_compare(sl, data, iter, sl->comparek);
    return (iter && *iter) ? ((*iter)->data) : NULL;
}


APR_DECLARE(void *) apr_skiplist_next(apr_skiplist *sl, apr_skiplistnode **iter)
{
    if (!*iter) {
        return NULL;
    }
    *iter = (*iter)->next;
    return (*iter) ? ((*iter)->data) : NULL;
}

APR_DECLARE(void *) apr_skiplist_previous(apr_skiplist *sl, apr_skiplistnode **iter)
{
    if (!*iter) {
        return NULL;
    }
    *iter = (*iter)->prev;
    return (*iter) ? ((*iter)->data) : NULL;
}

APR_DECLARE(apr_skiplistnode *) apr_skiplist_insert(apr_skiplist *sl, void *data)
{
    if (!sl->compare) {
        return 0;
    }
    return apr_skiplist_insert_compare(sl, data, sl->compare);
}

APR_DECLARE(apr_skiplistnode *) apr_skiplist_insert_compare(apr_skiplist *sl, void *data,
                                      apr_skiplist_compare comp)
{
    apr_skiplistnode *m, *p, *tmp, *ret = NULL, **stack;
    int nh = 1, ch, stacki;
    if (!sl->top) {
        sl->height = 1;
        sl->topend = sl->bottomend = sl->top = sl->bottom =
            (apr_skiplistnode *)apr_skiplist_alloc(sl, sizeof(apr_skiplistnode));
#if 0
        sl->top->next = (apr_skiplistnode *)NULL;
        sl->top->data = (apr_skiplistnode *)NULL;
        sl->top->prev = (apr_skiplistnode *)NULL;
        sl->top->up = (apr_skiplistnode *)NULL;
        sl->top->down = (apr_skiplistnode *)NULL;
        sl->top->nextindex = (apr_skiplistnode *)NULL;
        sl->top->previndex = (apr_skiplistnode *)NULL;
#endif
        sl->top->sl = sl;
    }
    if (sl->preheight) {
        while (nh < sl->preheight && get_b_rand()) {
            nh++;
        }
    }
    else {
        while (nh <= sl->height && get_b_rand()) {
            nh++;
        }
    }
    /* Now we have the new height at which we wish to insert our new node */
    /*
     * Let us make sure that our tree is a least that tall (grow if
     * necessary)
     */
    for (; sl->height < nh; sl->height++) {
        sl->top->up =
            (apr_skiplistnode *)apr_skiplist_alloc(sl, sizeof(apr_skiplistnode));
        sl->top->up->down = sl->top;
        sl->top = sl->topend = sl->top->up;
#if 0
        sl->top->prev = sl->top->next = sl->top->nextindex =
            sl->top->previndex = sl->top->up = NULL;
        sl->top->data = NULL;
#endif
        sl->top->sl = sl;
    }
    ch = sl->height;
    /* Find the node (or node after which we would insert) */
    /* Keep a stack to pop back through for insertion */
    /* malloc() is OK since we free the temp stack */
    m = sl->top;
    stack = (apr_skiplistnode **)malloc(sizeof(apr_skiplistnode *) * (nh));
    stacki = 0;
    while (m) {
        int compared = -1;
        if (m->next) {
            compared = comp(data, m->next->data);
        }
        if (compared == 0) {
            free(stack);    /* OK. was malloc'ed */
            return 0;
        }
        if ((m->next == NULL) || (compared < 0)) {
            if (ch <= nh) {
                /* push on stack */
                stack[stacki++] = m;
            }
            m = m->down;
            ch--;
        }
        else {
            m = m->next;
        }
    }
    /* Pop the stack and insert nodes */
    p = NULL;
    for (; stacki > 0; stacki--) {
        m = stack[stacki - 1];
        tmp = (apr_skiplistnode *)apr_skiplist_alloc(sl, sizeof(apr_skiplistnode));
        tmp->next = m->next;
        if (m->next) {
            m->next->prev = tmp;
        }
        tmp->prev = m;
        tmp->up = NULL;
        tmp->nextindex = tmp->previndex = NULL;
        tmp->down = p;
        if (p) {
            p->up = tmp;
        }
        tmp->data = data;
        tmp->sl = sl;
        m->next = tmp;
        /* This sets ret to the bottom-most node we are inserting */
        if (!p) {
            ret = tmp;
            sl->size++; /* this seems to go here got each element to be counted */
        }
        p = tmp;
    }
    free(stack); /* OK. was malloc'ed */
    if (sl->index != NULL) {
        /*
         * this is a external insertion, we must insert into each index as
         * well
         */
        apr_skiplistnode *ni, *li;
        li = ret;
        for (p = apr_skiplist_getlist(sl->index); p; apr_skiplist_next(sl->index, &p)) {
            ni = apr_skiplist_insert((apr_skiplist *) p->data, ret->data);
            li->nextindex = ni;
            ni->previndex = li;
            li = ni;
        }
    }
    else {
        /* sl->size++; */
    }
    sl->size++;
    return ret;
}

APR_DECLARE(int) apr_skiplist_remove(apr_skiplist *sl, void *data, apr_skiplist_freefunc myfree)
{
    if (!sl->compare) {
        return 0;
    }
    return apr_skiplist_remove_compare(sl, data, myfree, sl->comparek);
}

#if 0
void skiplist_print_struct(apr_skiplist * sl, char *prefix)
{
    apr_skiplistnode *p, *q;
    fprintf(stderr, "Skiplist Structure (height: %d)\n", sl->height);
    p = sl->bottom;
    while (p) {
        q = p;
        fprintf(stderr, prefix);
        while (q) {
            fprintf(stderr, "%p ", q->data);
            q = q->up;
        }
        fprintf(stderr, "\n");
        p = p->next;
    }
}
#endif

static int skiplisti_remove(apr_skiplist *sl, apr_skiplistnode *m, apr_skiplist_freefunc myfree)
{
    apr_skiplistnode *p;
    if (!m) {
        return 0;
    }
    if (m->nextindex) {
        skiplisti_remove(m->nextindex->sl, m->nextindex, NULL);
    }
    while (m->up) {
        m = m->up;
    }
    while (m) {
        p = m;
        p->prev->next = p->next;/* take me out of the list */
        if (p->next) {
            p->next->prev = p->prev;    /* take me out of the list */
        }
        m = m->down;
        /* This only frees the actual data in the bottom one */
        if (!m && myfree && p->data) {
            myfree(p->data);
        }
        apr_skiplist_free(sl, p);
    }
    sl->size--;
    while (sl->top && sl->top->next == NULL) {
        /* While the row is empty and we are not on the bottom row */
        p = sl->top;
        sl->top = sl->top->down;/* Move top down one */
        if (sl->top) {
            sl->top->up = NULL; /* Make it think its the top */
        }
        apr_skiplist_free(sl, p);
        sl->height--;
    }
    if (!sl->top) {
        sl->bottom = NULL;
    }
    return sl->height;  /* return 1; ?? */
}

APR_DECLARE(int) apr_skiplist_remove_compare(apr_skiplist *sli,
                            void *data,
                            apr_skiplist_freefunc myfree, apr_skiplist_compare comp)
{
    apr_skiplistnode *m;
    apr_skiplist *sl;
    if (comp == sli->comparek || !sli->index) {
        sl = sli;
    }
    else {
        apr_skiplist_find(sli->index, (void *)comp, &m);
        sl = (apr_skiplist *) m->data;
    }
    skiplisti_find_compare(sl, data, &m, comp);
    if (!m) {
        return 0;
    }
    while (m->previndex) {
        m = m->previndex;
    }
    return skiplisti_remove(sl, m, myfree);
}

APR_DECLARE(void) apr_skiplist_remove_all(apr_skiplist *sl, apr_skiplist_freefunc myfree)
{
    /*
     * This must remove even the place holder nodes (bottom though top)
     * because we specify in the API that one can free the Skiplist after
     * making this call without memory leaks
     */
    apr_skiplistnode *m, *p, *u;
    m = sl->bottom;
    while (m) {
        p = m->next;
        if (p && myfree && p->data)
            myfree(p->data);
        while (m) {
            u = m->up;
            apr_skiplist_free(sl, p);
            m = u;
        }
        m = p;
    }
    sl->top = sl->bottom = NULL;
    sl->height = 0;
    sl->size = 0;
}

APR_DECLARE(void *) apr_skiplist_pop(apr_skiplist *a, apr_skiplist_freefunc myfree)
{
    apr_skiplistnode *sln;
    void *data = NULL;
    sln = apr_skiplist_getlist(a);
    if (sln) {
        data = sln->data;
        skiplisti_remove(a, sln, myfree);
    }
    return data;
}

APR_DECLARE(void *) apr_skiplist_peek(apr_skiplist *a)
{
    apr_skiplistnode *sln;
    sln = apr_skiplist_getlist(a);
    if (sln) {
        return sln->data;
    }
    return NULL;
}

static void skiplisti_destroy(void *vsl)
{
    apr_skiplist_destroy((apr_skiplist *) vsl, NULL);
    apr_skiplist_free((apr_skiplist *) vsl, vsl);
}

APR_DECLARE(void) apr_skiplist_destroy(apr_skiplist *sl, apr_skiplist_freefunc myfree)
{
    while (apr_skiplist_pop(sl->index, skiplisti_destroy) != NULL)
        ;
    apr_skiplist_remove_all(sl, myfree);
}

APR_DECLARE(apr_skiplist *) apr_skiplist_merge(apr_skiplist *sl1, apr_skiplist *sl2)
{
    /* Check integrity! */
    apr_skiplist temp;
    struct apr_skiplistnode *b2;
    if (sl1->bottomend == NULL || sl1->bottomend->prev == NULL) {
        apr_skiplist_remove_all(sl1, NULL);
        temp = *sl1;
        *sl1 = *sl2;
        *sl2 = temp;
        /* swap them so that sl2 can be freed normally upon return. */
        return sl1;
    }
    if(sl2->bottom == NULL || sl2->bottom->next == NULL) {
        apr_skiplist_remove_all(sl2, NULL);
        return sl1;
    }
    /* This is what makes it brute force... Just insert :/ */
    b2 = apr_skiplist_getlist(sl2);
    while (b2) {
        apr_skiplist_insert(sl1, b2->data);
        apr_skiplist_next(sl2, &b2);
    }
    apr_skiplist_remove_all(sl2, NULL);
    return sl1;
}
