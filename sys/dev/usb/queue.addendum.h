/* These definitions are taken from the NetBSD /sys/sys/queue.h file
 * The copyright as in /sys/sys/queue.h from FreeBSD applies (they are the same)
 */

/* This was called SIMPLEQ
 */
#ifndef STAILQ_HEAD_INITIALIZER
#define STAILQ_HEAD_INITIALIZER(head)                                   \
        { NULL, &(head).stqh_first }
#endif

/* This one was called SIMPLEQ_REMOVE_HEAD but removes not only the
 * head element, but a whole queue of elements from the head.
 */
#ifndef STAILQ_REMOVE_HEAD_QUEUE
#define STAILQ_REMOVE_HEAD_QUEUE(head, elm, field) do {                      \
      if (((head)->stqh_first = (elm)->field.stqe_next) == NULL)      \
              (head)->stqh_last = &(head)->stqh_first;                \
} while (0)
#endif


/* This is called LIST and was called like that as well in the NetBSD version
 */
#ifndef LIST_HEAD_INITIALIZER
#define LIST_HEAD_INITIALIZER(head)                                   \
      { NULL }
#endif

