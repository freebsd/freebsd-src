/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated for what's essentially a complete rewrite.
 *
 * $FreeBSD$
 *
 * Copyright (c) 1997 FreeBSD, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL TRAINA ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL PAUL TRAINA OR HIS KILLER RATS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/* The structure */
typedef struct _qelement {
    struct _qelement *q_forw;
    struct _qelement *q_back;
} qelement;

#define	INITQUE(Xhead) { \
    (Xhead).q_forw = &(Xhead); \
    (Xhead).q_back = &(Xhead); \
}

#define	EMPTYQUE(Xhead) \
    ((Xhead).q_forw == &(Xhead))

#define	INSQUEUE(elem, pred) { \
    register qelement *Xe = (qelement *) (elem); \
    register qelement *Xp = (qelement *) (pred); \
    Xp->q_forw = (Xe->q_forw = (Xe->q_back = Xp)->q_forw)->q_back = Xe; \
}

#define REMQUE(elem) { \
    register qelement *Xe = (qelement *) (elem); \
    (Xe->q_back->q_forw = Xe->q_forw)->q_back = Xe->q_back; \
}
