/*	FreeBSD $Id$ */

/*
 * Copyright (c) 1997, 1998
 *      Nick Hibma <n_hibma@freebsd.org>. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY NICK HIBMA AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

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

