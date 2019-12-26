/*-
 * Copyright (c) 2015 Mark R V Murray
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/random.h>
#include <sys/sysctl.h>

#include <dev/random/randomdev.h>

/* Set up the sysctl root node for the entropy device */
SYSCTL_NODE(_kern, OID_AUTO, random, CTLFLAG_RW, 0,
    "Cryptographically Secure Random Number Generator");
SYSCTL_NODE(_kern_random, OID_AUTO, initial_seeding, CTLFLAG_RW, 0,
    "Initial seeding control and information");

/*
 * N.B., this is a dangerous default, but it matches the behavior prior to
 * r346250 (and, say, OpenBSD -- although they get some guaranteed saved
 * entropy from the prior boot because of their KARL system, on RW media).
 */
bool random_bypass_before_seeding = true;
SYSCTL_BOOL(_kern_random_initial_seeding, OID_AUTO,
    bypass_before_seeding, CTLFLAG_RDTUN, &random_bypass_before_seeding,
    0, "If set non-zero, bypass the random device in requests for random "
    "data when the random device is not yet seeded.  This is considered "
    "dangerous.  Ordinarily, the random device will block requests until "
    "it is seeded by sufficient entropy.");

/*
 * This is a read-only diagnostic that reports the combination of the former
 * tunable and actual bypass.  It is intended for programmatic inspection by
 * userspace administrative utilities after boot.
 */
bool read_random_bypassed_before_seeding = false;
SYSCTL_BOOL(_kern_random_initial_seeding, OID_AUTO,
    read_random_bypassed_before_seeding, CTLFLAG_RD,
    &read_random_bypassed_before_seeding, 0, "If non-zero, the random device "
    "was bypassed because the 'bypass_before_seeding' knob was enabled and a "
    "request was submitted prior to initial seeding.");

/*
 * This is a read-only diagnostic that reports the combination of the former
 * tunable and actual bypass for arc4random initial seeding.  It is intended
 * for programmatic inspection by userspace administrative utilities after
 * boot.
 */
bool arc4random_bypassed_before_seeding = false;
SYSCTL_BOOL(_kern_random_initial_seeding, OID_AUTO,
    arc4random_bypassed_before_seeding, CTLFLAG_RD,
    &arc4random_bypassed_before_seeding, 0, "If non-zero, the random device "
    "was bypassed when initially seeding the kernel arc4random(9), because "
    "the 'bypass_before_seeding' knob was enabled and a request was submitted "
    "prior to initial seeding.");

/*
 * This knob is for users who do not want additional warnings in their logs
 * because they intend to handle bypass by inspecting the status of the
 * diagnostic sysctls.
 */
bool random_bypass_disable_warnings = false;
SYSCTL_BOOL(_kern_random_initial_seeding, OID_AUTO,
    disable_bypass_warnings, CTLFLAG_RDTUN,
    &random_bypass_disable_warnings, 0, "If non-zero, do not log a warning "
    "if the 'bypass_before_seeding' knob is enabled and a request is "
    "submitted prior to initial seeding.");

MALLOC_DEFINE(M_ENTROPY, "entropy", "Entropy harvesting buffers and data structures");

#if defined(RANDOM_LOADABLE)
const struct random_algorithm *p_random_alg_context;
void (*_read_random)(void *, u_int);
int (*_read_random_uio)(struct uio *, bool);
bool (*_is_random_seeded)(void);
#endif /* defined(RANDOM_LOADABLE) */
