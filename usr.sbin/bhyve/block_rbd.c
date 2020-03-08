/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2013  Willem Jan Withagen <wjw@digiware.nl>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/disk.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <pthread_np.h>
#include <signal.h>
#include <sysexits.h>
#include <unistd.h>

#include <machine/atomic.h>

#include "bhyverun.h"
#include "debug.h"
#include "mevent.h"
#include "block_if.h"

#include <sys/linker_set.h>

#include <rbd/librbd.h>

static pthread_once_t blockrbd_once = PTHREAD_ONCE_INIT;
struct block_backend block_backend_rbd;

struct blockrbd_sig_elem {
    pthread_mutex_t           bse_mtx;
    pthread_cond_t            bse_cond;
    int                       bse_pending;
    struct blockrbd_sig_elem *bse_next;
};

static struct blockrbd_sig_elem *blockrbd_bse_head;

struct block_rbd_inst {
    rados_t ri_rados;
    rados_ioctx_t ri_ioctx;
    rbd_image_t ri_image;
};

/* 
 * The actual RBD/Rados workhorse functions
 *     signature: block_rbd<function>
 * They will be used in the blockrbd_proc function.
 */
static ssize_t
block_rbd_readv(struct block_rbd_inst *bd, const struct iovec *iov, int iovcnt, off_t offset) 
{
    struct block_rbd_inst *ri = bd;
    rbd_completion_t comp;
    ssize_t nbytes;
    int r;

    r = rbd_aio_create_completion(NULL, NULL, &comp);
    if (r < 0) {
        errno = -r;
        perror("rbd_aio_create_completion");
        return (-1);
    }

    r = rbd_aio_readv(ri->ri_image, iov, iovcnt, offset, comp);
    if (r < 0) {
        errno = -r;
        perror("rbd_aio_readv");
        return (-1);
    }

    r = rbd_aio_wait_for_complete(comp);
    if (r < 0) {
        errno = -r;
        perror("rbd_aio_wait_for_complete");
        rbd_aio_release(comp);
        return (-1);
    }

    nbytes = rbd_aio_get_return_value(comp);

    rbd_aio_release(comp);
    if (r < 0) {
        errno = -r;
        perror("rbd_aio_get_return_value");
        return (-1);
    }

    return (nbytes);
}

static ssize_t
block_rbd_read(struct block_rbd_inst *bd, char *buf, size_t nbytes, off_t offset) 
{
    struct block_rbd_inst *ri = bd;
    rbd_completion_t comp;
    int r;

    r = rbd_aio_create_completion(NULL, NULL, &comp);
    if (r < 0) {
        errno = -r;
        perror("rbd_aio_create_completion");
        return (-1);
    }

    r = rbd_aio_read(ri->ri_image, offset, nbytes, buf, comp);
    if (r < 0) {
        errno = -r;
        perror("rbd_aio_read");
        return (-1);
    }

    r = rbd_aio_wait_for_complete(comp);
    if (r < 0) {
        errno = -r;
        perror("rbd_aio_wait_for_complete");
        rbd_aio_release(comp);
        return (-1);
    }

    r = rbd_aio_get_return_value(comp);
    rbd_aio_release(comp);
    if (r < 0) {
        errno = -r;
        perror("rbd_aio_get_return_value");
        return (-1);
    }
    return (r);
}

static ssize_t
block_rbd_writev(struct block_rbd_inst *bd, const struct iovec *iov, int iovcnt,
    off_t offset)
{
    struct block_rbd_inst *ri = bd;
        rbd_completion_t comp;
        int r;

    r = rbd_aio_create_completion(NULL, NULL, &comp);
    if (r < 0) {
        errno = -r;
        perror("rbd_aio_create_completion");
        return (-1);
    }

    r = rbd_aio_writev(ri->ri_image, iov, iovcnt, offset, comp);
    if (r < 0) {
        errno = -r;
        perror("rbd_aio_writev");
        return (-1);
    }

        r = rbd_aio_wait_for_complete(comp);
    if (r < 0) {
        errno = -r;
        perror("rbd_aio_wait_for_complete");
        rbd_aio_release(comp);
        return (-1);
    }

    r = rbd_aio_get_return_value(comp);
    rbd_aio_release(comp);
    if (r < 0) {
        errno = -r;
        perror("rbd_aio_get_return_value");
        return (-1);
    }

    return (r);
}

static ssize_t
block_rbd_delete(struct block_rbd_inst *bd, size_t nbytes, off_t offset)
{
    struct block_rbd_inst *ri = bd;
    rbd_completion_t comp;
    int r;

    r = rbd_aio_create_completion(NULL, NULL, &comp);
    if (r < 0) {
        errno = -r;
        perror("rbd_aio_create_completion");
        return (-1);
    }

    r = rbd_aio_discard(ri->ri_image, offset, nbytes, comp);
    if (r < 0) {
        errno = -r;
        perror("rbd_aio_discard");
        return (-1);
    }

    r = rbd_aio_wait_for_complete(comp);
    if (r < 0) {
        errno = -r;
        perror("rbd_aio_wait_for_complete");
        rbd_aio_release(comp);
        return (-1);
    }

    r = rbd_aio_get_return_value(comp);
    rbd_aio_release(comp);
    if (r < 0) {
        errno = -r;
        perror("rbd_aio_get_return_value");
        return (-1);
    }

    return (nbytes);
}

static int
block_rbd_flush(struct block_rbd_inst *bd)
{
    struct block_rbd_inst *ri = bd;
    rbd_completion_t comp;
    int r;

    r = rbd_aio_create_completion(NULL, NULL, &comp);
    if (r < 0) {
        errno = -r;
        perror("rbd_aio_create_completion");
        return (-1);
    }

    r = rbd_aio_flush(ri->ri_image, comp);
    if (r < 0) {
        errno = -r;
        perror("rbd_aio_flush");
        return (-1);
    }

    r = rbd_aio_wait_for_complete(comp);
    if (r < 0) {
        errno = -r;
        perror("rbd_aio_wait_for_complete");
        rbd_aio_release(comp);
        return (-1);
    }

    r = rbd_aio_get_return_value(comp);
    rbd_aio_release(comp);
    if (r < 0) {
        errno = -r;
        perror("rbd_aio_get_return_value");
        return (-1);
    }

    return (r);
}

static int
blockrbd_enqueue(struct blockif_ctxt *bc, struct blockif_req *breq, enum blockop op)
{
    struct blockif_elem *be, *tbe;
    off_t off;
    int i;

    be = TAILQ_FIRST(&bc->bc_freeq);
    assert(be != NULL);
    assert(be->be_status == BST_FREE);
    TAILQ_REMOVE(&bc->bc_freeq, be, be_link);
    be->be_req = breq;
    be->be_op = op;
    switch (op) {
    case BOP_READ:
    case BOP_WRITE:
    case BOP_DELETE:
        off = breq->br_offset;
        for (i = 0; i < breq->br_iovcnt; i++)
            off += breq->br_iov[i].iov_len;
        break;
    default:
        off = OFF_MAX;
    }
    be->be_block = off;
    TAILQ_FOREACH(tbe, &bc->bc_pendq, be_link) {
        if (tbe->be_block == breq->br_offset)
            break;
    }
    if (tbe == NULL) {
        TAILQ_FOREACH(tbe, &bc->bc_busyq, be_link) {
            if (tbe->be_block == breq->br_offset)
                break;
        }
    }
    if (tbe == NULL)
        be->be_status = BST_PEND;
    else
        be->be_status = BST_BLOCK;
    TAILQ_INSERT_TAIL(&bc->bc_pendq, be, be_link);
    return (be->be_status == BST_PEND);
}

static int
blockrbd_dequeue(struct blockif_ctxt *bc, pthread_t t, struct blockif_elem **bep)
{
    struct blockif_elem *be;

    TAILQ_FOREACH(be, &bc->bc_pendq, be_link) {
        if (be->be_status == BST_PEND)
            break;
        assert(be->be_status == BST_BLOCK);
    }
    if (be == NULL)
        return (0);
    TAILQ_REMOVE(&bc->bc_pendq, be, be_link);
    be->be_status = BST_BUSY;
    be->be_tid = t;
    TAILQ_INSERT_TAIL(&bc->bc_busyq, be, be_link);
    *bep = be;
    return (1);
}

static void
blockrbd_complete(struct blockif_ctxt *bc, struct blockif_elem *be)
{
    struct blockif_elem *tbe;

    if (be->be_status == BST_DONE || be->be_status == BST_BUSY)
        TAILQ_REMOVE(&bc->bc_busyq, be, be_link);
    else
        TAILQ_REMOVE(&bc->bc_pendq, be, be_link);
    TAILQ_FOREACH(tbe, &bc->bc_pendq, be_link) {
        if (tbe->be_req->br_offset == be->be_block)
            tbe->be_status = BST_PEND;
    }
    be->be_tid = 0;
    be->be_status = BST_FREE;
    be->be_req = NULL;
    TAILQ_INSERT_TAIL(&bc->bc_freeq, be, be_link);
}

static void
blockrbd_proc(struct blockif_ctxt *bc, struct blockif_elem *be, uint8_t *buf)
{
    struct blockif_req *br;
    ssize_t len;
    int err;

    br = be->be_req;
    if (br->br_iovcnt <= 1)
        buf = NULL;
    err = 0;
    switch (be->be_op) {
    case BOP_READ:
        if (buf == NULL) {
            if (br->br_iovcnt <= 1) { 
                if ((len = block_rbd_read((struct block_rbd_inst *)bc->bc_desc, 
                            br->br_iov[0].iov_base, br->br_iov[0].iov_len, 
                            br->br_offset)) < 0)
                        err = len;
                else
                        br->br_resid -= len;
                break;
            } else {
                if ((len = block_rbd_readv((struct block_rbd_inst*)bc->bc_desc, 
			    br->br_iov, br->br_iovcnt, br->br_offset)) < 0) 
                        err = len;
                else
                        br->br_resid -= len;
                break;
            }
       }
       /* Have yet to see that buf != NULL for iovcnt > 1 */
       assert("block_proc::BOP_READ buf != NULL");
       break;
    case BOP_WRITE:
        if (bc->bc_rdonly) {
            err = EROFS;
            break;
        }
        if (buf == NULL) {
            if ((len = block_rbd_writev((struct block_rbd_inst*)bc->bc_desc, 
                        br->br_iov, br->br_iovcnt, br->br_offset)) < 0)
                err = errno;
            else
                br->br_resid -= len;
            break;
        }
        /* Have yet to see that buf != NULL for iovcnt > 1 */
        assert("BOP_WRITE buf != NULL");
        break;
    case BOP_FLUSH:
        err = block_rbd_flush((struct block_rbd_inst*)bc->bc_desc);
        break;
    case BOP_DELETE:
        if (!bc->bc_candelete)
            err = EOPNOTSUPP;
        else if (bc->bc_rdonly)
            err = EROFS;
        else if (bc->bc_ischr) {
                /* block_rbd_delete returns # of deleted bytes */
                if ((err = block_rbd_delete((struct block_rbd_inst*)bc->bc_desc, 
                            br->br_resid, br->br_offset)) > 0)
                    err = 0; 
            else
                br->br_resid = 0;
        } else
            err = EOPNOTSUPP;
        break;
    default:
        err = EINVAL;
        break;
    }

    be->be_status = BST_DONE;
    (*br->br_callback)(br, err);
}

static void *
blockrbd_thr(void *arg)
{
    struct blockif_ctxt *bc;
    struct blockif_elem *be;
    pthread_t t;
    uint8_t *buf;

    bc = arg;
    if (bc->bc_isgeom)
        buf = malloc(MAXPHYS);
    else
        buf = NULL;
    t = pthread_self();

    pthread_mutex_lock(&bc->bc_mtx);
    for (;;) {
        while (blockrbd_dequeue(bc, t, &be)) {
            pthread_mutex_unlock(&bc->bc_mtx);
            blockrbd_proc(bc, be, buf);
            pthread_mutex_lock(&bc->bc_mtx);
            blockrbd_complete(bc, be);
        }
        /* Check ctxt status here to see if exit requested */
        if (bc->bc_closing)
            break;
        pthread_cond_wait(&bc->bc_cond, &bc->bc_mtx);
    }
    pthread_mutex_unlock(&bc->bc_mtx);

    if (buf)
        free(buf);
    pthread_exit(NULL);
    return (NULL);
}

static void
blockrbd_sigcont_handler(int signal, enum ev_type type, void *arg)
{
    struct blockrbd_sig_elem *bse;

    for (;;) {
        /*
         * Process the entire list even if not intended for
         * this thread.
         */
        do {
            bse = blockrbd_bse_head;
            if (bse == NULL)
                return;
        } while (!atomic_cmpset_ptr((uintptr_t *)&blockrbd_bse_head,
                        (uintptr_t)bse,
                        (uintptr_t)bse->bse_next));

        pthread_mutex_lock(&bse->bse_mtx);
        bse->bse_pending = 0;
        pthread_cond_signal(&bse->bse_cond);
        pthread_mutex_unlock(&bse->bse_mtx);
    }
}

void
blockrbd_init(void)
{
    mevent_add(SIGCONT, EVF_SIGNAL, blockrbd_sigcont_handler, NULL);
    (void) signal(SIGCONT, SIG_IGN);
}

void
blockrbd_cleanup(struct blockif_ctxt *bc)
{ /* empty block
   * currently no cleanup required.
   */
}

static struct blockif_ctxt *
blockrbd_open(const char *optstr, const char *ident)
{
    char *nopt, *xopts, *cp;
    char *pool_name, *image_name, *snap_name;
    struct block_rbd_inst *ri;
    rbd_image_info_t info;
    uint64_t features, stripe_unit;
    struct blockif_ctxt *bc;
    off_t psectoff;
    int ro, candelete;
    int i, r;

    pthread_once(&blockrbd_once, blockrbd_init);

    /*
     * Parse the given options, the rbd: header is already removed.
     * First element is the pool/image specifier
     * Optional elements follow
     *   ro = readonly
     */
    ro = 0;
    nopt = xopts = strdup(optstr);
    while (xopts != NULL) {
        cp = strsep(&xopts, ",");
        if (cp == nopt)        /* image spec */
            continue;
        else if (!strcmp(cp, "ro"))
            ro = 1;
        else {
            EPRINTLN("Invalid device option \"%s\"", cp);
            errno = EINVAL;
            goto err_free;
        }
    }
    xopts = nopt;
    pool_name = strsep(&xopts, "/");
    if (xopts == NULL) {
        EPRINTLN("Invalid device spec \"%s\"", pool_name);
        errno = EINVAL;
        goto err_free;
    }
    image_name = strsep(&xopts, "@");
    snap_name = xopts;
    if (snap_name != NULL) {
        ro = 1;
    }

    ri = calloc(1, sizeof(struct block_rbd_inst));
    if (ri == NULL) {
        perror("calloc");
        goto err_free;
    }

    r = rados_create(&ri->ri_rados, NULL);
    if (r < 0) {
        errno = -r;
        perror("rados_create");
        goto err_free;
    }

    r = rados_conf_read_file(ri->ri_rados, NULL);
    if (r < 0) {
        errno = -r;
        perror("rados_conf_read_file");
        goto err_rados_shutdown;
    }

    rados_conf_parse_env(ri->ri_rados, NULL);

    r = rados_connect(ri->ri_rados);
    if (r < 0) {
        errno = -r;
        perror("rados_connect");
        goto err_rados_shutdown;
    }

    r = rados_ioctx_create(ri->ri_rados, pool_name, &ri->ri_ioctx);
    if (r < 0) {
        errno = -r;
        perror("rados_ioctx_create");
        goto err_rados_shutdown;
    }

    r = rbd_open(ri->ri_ioctx, image_name, &ri->ri_image, snap_name);
    if (r < 0) {
        errno = -r;
        perror("rbd_open");
        goto err_ioctx_destroy;
    }

    r = rbd_stat(ri->ri_image, &info, sizeof(info));
    if (r < 0) {
        errno = -r;
        perror("rbd_stat");
        goto err;
    }

    r = rbd_get_features(ri->ri_image, &features);
    if (r < 0) {
        errno = -r;
        perror("rbd_get_features");
        goto err;
    }

    if ((features & RBD_FEATURE_STRIPINGV2) != 0) {
        r = rbd_get_stripe_unit(ri->ri_image, &stripe_unit);
        if (r < 0) {
            errno = -r;
            perror("rbd_get_stripe_unit");
            goto err;
        }
    } else {
        stripe_unit = (1 << info.order);
    }

    bc = calloc(1, sizeof(struct blockif_ctxt));
    if (bc == NULL) {
        perror("calloc");
        goto err;
    }

    bc->bc_magic = BLOCKIF_SIG;
    bc->bc_ischr = 1;
    bc->bc_isgeom = 0;
    bc->bc_candelete = candelete;
    bc->bc_rdonly = ro;
    bc->bc_size = info.size;
    bc->bc_sectsz = 512;
    bc->bc_psectsz = 512;
    bc->bc_psectoff = psectoff;
    bc->bc_desc = (intptr_t)ri;

    /* Start the thread for the IO */
    pthread_mutex_init(&bc->bc_mtx, NULL);
    pthread_cond_init(&bc->bc_cond, NULL);
    TAILQ_INIT(&bc->bc_freeq);
    TAILQ_INIT(&bc->bc_pendq);
    TAILQ_INIT(&bc->bc_busyq);
    for (i = 0; i < BLOCKIF_MAXREQ; i++) {
        bc->bc_reqs[i].be_status = BST_FREE;
        TAILQ_INSERT_HEAD(&bc->bc_freeq, &bc->bc_reqs[i], be_link);
    }

    for (i = 0; i < BLOCKIF_NUMTHR; i++) {
    char tname[MAXCOMLEN + 1];
        pthread_create(&bc->bc_btid[i], NULL, blockrbd_thr, bc);
        snprintf(tname, sizeof(tname), "rbd-%s-%d", ident, i);
        pthread_set_name_np(bc->bc_btid[i], tname);
    }

    return (bc);
err:
    rbd_close(ri->ri_image);
err_ioctx_destroy:
    rados_ioctx_destroy(ri->ri_ioctx);
err_rados_shutdown:
    rados_shutdown(ri->ri_rados);
err_free:
    free(ri);
    free(nopt);
    return (NULL);
}

static int
blockif_request(struct blockif_ctxt *bc, struct blockif_req *breq,
        enum blockop op)
{
    int err;

    err = 0;

    pthread_mutex_lock(&bc->bc_mtx);
    if (!TAILQ_EMPTY(&bc->bc_freeq)) {
        /*
         * Enqueue and inform the block i/o thread
         * that there is work available
         */
        if (blockrbd_enqueue(bc, breq, op))
            pthread_cond_signal(&bc->bc_cond);
    } else {
        /*
         * Callers are not allowed to enqueue more than
         * the specified blockif queue limit. Return an
         * error to indicate that the queue length has been
         * exceeded.
         */
        err = E2BIG;
    }
    pthread_mutex_unlock(&bc->bc_mtx);

    return (err);
}

static int
blockrbd_read(struct blockif_ctxt *bc, struct blockif_req *breq)
{
    assert(bc->bc_magic == BLOCKIF_SIG);
    return (blockif_request(bc, breq, BOP_READ));
}

static int
blockrbd_write(struct blockif_ctxt *bc, struct blockif_req *breq)
{
    assert(bc->bc_magic == BLOCKIF_SIG);
    return (blockif_request(bc, breq, BOP_WRITE));
}

static int
blockrbd_flush(struct blockif_ctxt *bc, struct blockif_req *breq)
{
    assert(bc->bc_magic == BLOCKIF_SIG);
    return (blockif_request(bc, breq, BOP_FLUSH));
}

static int
blockrbd_delete(struct blockif_ctxt *bc, struct blockif_req *breq)
{
    assert(bc->bc_magic == BLOCKIF_SIG);
    return (blockif_request(bc, breq, BOP_DELETE));
}

static int
blockrbd_cancel(struct blockif_ctxt *bc, struct blockif_req *breq)
{
    struct blockif_elem *be;

    assert(bc->bc_magic == BLOCKIF_SIG);

    pthread_mutex_lock(&bc->bc_mtx);
    /*
     * Check pending requests.
     */
    TAILQ_FOREACH(be, &bc->bc_pendq, be_link) {
        if (be->be_req == breq)
            break;
    }
    if (be != NULL) {
        /*
         * Found it.
         */
        blockrbd_complete(bc, be);
        pthread_mutex_unlock(&bc->bc_mtx);

        return (0);
    }

    /*
     * Check in-flight requests.
     */
    TAILQ_FOREACH(be, &bc->bc_busyq, be_link) {
        if (be->be_req == breq)
            break;
    }
    if (be == NULL) {
        /*
         * Didn't find it.
         */
        pthread_mutex_unlock(&bc->bc_mtx);
        return (EINVAL);
    }

    /*
     * Interrupt the processing thread to force it return
     * prematurely via it's normal callback path.
     */
    while (be->be_status == BST_BUSY) {
        struct blockrbd_sig_elem bse, *old_head;

        pthread_mutex_init(&bse.bse_mtx, NULL);
        pthread_cond_init(&bse.bse_cond, NULL);

        bse.bse_pending = 1;

        do {
            old_head = blockrbd_bse_head;
            bse.bse_next = old_head;
        } while (!atomic_cmpset_ptr((uintptr_t *)&blockrbd_bse_head,
                        (uintptr_t)old_head,
                        (uintptr_t)&bse));

        pthread_kill(be->be_tid, SIGCONT);

        pthread_mutex_lock(&bse.bse_mtx);
        while (bse.bse_pending)
            pthread_cond_wait(&bse.bse_cond, &bse.bse_mtx);
        pthread_mutex_unlock(&bse.bse_mtx);
    }

    pthread_mutex_unlock(&bc->bc_mtx);

    /*
     * The processing thread has been interrupted.  Since it's not
     * clear if the callback has been invoked yet, return EBUSY.
     */
    return (EBUSY);
}

static int
blockrbd_close(struct blockif_ctxt *bc)
{
    void *jval;
    int i;
    struct block_rbd_inst *ri = (void*)bc;

    assert(bc->bc_magic == BLOCKIF_SIG);

    /*
     * Stop the block i/o thread
     */
    pthread_mutex_lock(&bc->bc_mtx);
    bc->bc_closing = 1;
    pthread_mutex_unlock(&bc->bc_mtx);
    pthread_cond_broadcast(&bc->bc_cond);
    for (i = 0; i < BLOCKIF_NUMTHR; i++)
        pthread_join(bc->bc_btid[i], &jval);

    /* XXX Cancel queued i/o's ??? */

    /*
     * Release resources
     */
    bc->bc_magic = 0;

    rbd_close(ri->ri_image);
    rados_ioctx_destroy(ri->ri_ioctx);
    rados_shutdown(ri->ri_rados);
    free(ri);
    free(bc);

    return (0);
}

/*
 * Return virtual C/H/S values for a given block. Use the algorithm
 * outlined in the VHD specification to calculate values.
 */
static void
blockrbd_chs(struct blockif_ctxt *bc, uint16_t *c, uint8_t *h, uint8_t *s)
{
    off_t sectors;        /* total sectors of the block dev */
    off_t hcyl;        /* cylinders times heads */
    uint16_t secpt;        /* sectors per track */
    uint8_t heads;

    assert(bc->bc_magic == BLOCKIF_SIG);

    sectors = bc->bc_size / bc->bc_sectsz;

    /* Clamp the size to the largest possible with CHS */
    if (sectors > 65535UL*16*255)
        sectors = 65535UL*16*255;

    if (sectors >= 65536UL*16*63) {
        secpt = 255;
        heads = 16;
        hcyl = sectors / secpt;
    } else {
        secpt = 17;
        hcyl = sectors / secpt;
        heads = (hcyl + 1023) / 1024;

        if (heads < 4)
            heads = 4;

        if (hcyl >= (heads * 1024) || heads > 16) {
            secpt = 31;
            heads = 16;
            hcyl = sectors / secpt;
        }
        if (hcyl >= (heads * 1024)) {
            secpt = 63;
            heads = 16;
            hcyl = sectors / secpt;
        }
    }

    *c = hcyl / heads;
    *h = heads;
    *s = secpt;
}

/*
 * Accessors
 */
static off_t
blockrbd_size(struct blockif_ctxt *bc)
{
    assert(bc->bc_magic == BLOCKIF_SIG);
    return (bc->bc_size);
}

static int
blockrbd_sectsz(struct blockif_ctxt *bc)
{
    assert(bc->bc_magic == BLOCKIF_SIG);
    return (bc->bc_sectsz);
}

static void
blockrbd_psectsz(struct blockif_ctxt *bc, int *size, int *off)
{
    assert(bc->bc_magic == BLOCKIF_SIG);
    *size = bc->bc_psectsz;
    *off = bc->bc_psectoff;
}

static int
blockrbd_queuesz(struct blockif_ctxt *bc)
{
    assert(bc->bc_magic == BLOCKIF_SIG);
    return (BLOCKIF_MAXREQ - 1);
}

static int
blockrbd_is_ro(struct blockif_ctxt *bc)
{
    assert(bc->bc_magic == BLOCKIF_SIG);
    return (bc->bc_rdonly);
}

static int
blockrbd_candelete(struct blockif_ctxt *bc)
{
    assert(bc->bc_magic == BLOCKIF_SIG);
    return (bc->bc_candelete);
}

struct block_backend block_backend_rbd = {
    .bb_name      = "rbd",
    .bb_init      = blockrbd_init,
    .bb_cleanup   = blockrbd_cleanup,
    .bb_open      = blockrbd_open,
    .bb_size      = blockrbd_size,
    .bb_chs       = blockrbd_chs,
    .bb_sectsz    = blockrbd_sectsz,
    .bb_psectsz   = blockrbd_psectsz,
    .bb_queuesz   = blockrbd_queuesz,
    .bb_is_ro     = blockrbd_is_ro,
    .bb_candelete = blockrbd_candelete,
    .bb_read      = blockrbd_read,
    .bb_write     = blockrbd_write,
    .bb_flush     = blockrbd_flush,
    .bb_delete    = blockrbd_delete,
    .bb_cancel    = blockrbd_cancel,
    .bb_close     = blockrbd_close,
    .bb_opaque    = (void*)&block_backend_rbd,
};
DATA_SET(block_backend_set, block_backend_rbd);

static void *  get_block_backend_rbd() {
    return((void*)&block_backend_rbd);
}
