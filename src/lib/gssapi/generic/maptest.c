/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

typedef struct { int a, b; } elt;
static int eltcp(elt *dest, elt src)
{
    *dest = src;
    return 0;
}
static int eltcmp(elt left, elt right)
{
    if (left.a < right.a)
        return -1;
    if (left.a > right.a)
        return 1;
    if (left.b < right.b)
        return -1;
    if (left.b > right.b)
        return 1;
    return 0;
}
static void eltprt(elt v, FILE *f)
{
    fprintf(f, "{%d,%d}", v.a, v.b);
}
static int intcmp(int left, int right)
{
    if (left < right)
        return -1;
    if (left > right)
        return 1;
    return 0;
}
static void intprt(int v, FILE *f)
{
    fprintf(f, "%d", v);
}

#include "maptest.h"

foo foo1;

int main ()
{
    elt v1 = { 1, 2 }, v2 = { 3, 4 };
    const elt *vp;
    const int *ip;

    assert(0 == foo_init(&foo1));
    vp = foo_findleft(&foo1, 47);
    assert(vp == NULL);
    assert(0 == foo_add(&foo1, 47, v1));
    vp = foo_findleft(&foo1, 47);
    assert(vp != NULL);
    assert(0 == eltcmp(*vp, v1));
    vp = foo_findleft(&foo1, 3);
    assert(vp == NULL);
    assert(0 == foo_add(&foo1, 93, v2));
    ip = foo_findright(&foo1, v1);
    assert(ip != NULL);
    assert(*ip == 47);
    printf("Map content: ");
    foo_printmap(&foo1, stdout);
    printf("\n");
    return 0;
}
