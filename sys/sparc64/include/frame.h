/*-
 * Copyright (c) 2001 Jake Burkholder.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
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

#ifndef	_MACHINE_FRAME_H_
#define	_MACHINE_FRAME_H_

#define	SPOFF	2047

struct trapframe {
	u_long	tf_global[8];
	u_long	tf_out[8];
	u_long	tf_pil;
	u_long	tf_tnpc;
	u_long	tf_tpc;
	u_long	tf_tstate;
	u_long	tf_type;
	u_long	tf_wstate;
	uintptr_t tf_arg;
};
#define	tf_sp	tf_out[6]
 
#define	TF_DONE(tf) do { \
	tf->tf_tpc = tf->tf_tnpc; \
	tf->tf_tnpc += 4; \
} while (0)

struct mmuframe {
	u_long	mf_sfar;
	u_long	mf_sfsr;
	u_long	mf_tar;
};

struct kdbframe {
	u_long	kf_fp;
	u_long	kf_cfp;
	u_long	kf_canrestore;
	u_long	kf_cansave;
	u_long	kf_cleanwin;
	u_long	kf_cwp;
	u_long	kf_otherwin;
};

struct clockframe {
	struct	trapframe cf_tf;
};

struct frame {
	u_long	f_local[8];
	u_long	f_in[8];
	u_long	f_pad[8];
};
#define	f_fp	f_in[6]
#define	f_pc	f_in[7]

/*
 * Frame used for pcb_wscratch.
 */
struct rwindow {
	u_long	rw_local[8];
	u_long	rw_in[8];
};

struct thread;

int	rwindow_save(struct thread *td);
int	rwindow_load(struct thread *td, struct trapframe *tf, int n);

int	kdb_trap(struct trapframe *tf);

#endif /* !_MACHINE_FRAME_H_ */
