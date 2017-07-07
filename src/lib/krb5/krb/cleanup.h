/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#ifndef KRB5_CLEANUP
#define KRB5_CLEANUP

struct cleanup {
    void                * arg;
    void                (*func)(void *);
};

#define CLEANUP_INIT(x)                         \
    struct cleanup cleanup_data[x];             \
    int cleanup_count = 0;

#define CLEANUP_PUSH(x, y)                      \
    cleanup_data[cleanup_count].arg = x;        \
    cleanup_data[cleanup_count].func = y;       \
    cleanup_count++;

#define CLEANUP_POP(x)                                                  \
    if ((--cleanup_count) && x && (cleanup_data[cleanup_count].func))   \
        cleanup_data[cleanup_count].func(cleanup_data[cleanup_count].arg);

#define CLEANUP_DONE()                                                  \
    while(cleanup_count--)                                              \
        if (cleanup_data[cleanup_count].func)                           \
            cleanup_data[cleanup_count].func(cleanup_data[cleanup_count].arg);


#endif
