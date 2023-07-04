/*
 * SPDX-License-Identifier: CDDL 1.0
 *
 * Copyright (c) 2022 Christos Margiolis <christos@FreeBSD.org>
 * Copyright (c) 2023 The FreeBSD Foundation
 *
 * Portions of this software were developed by Christos Margiolis
 * <christos@FreeBSD.org> under sponsorship from the FreeBSD Foundation.
 */

#ifndef _KINST_H_
#define _KINST_H_

#include <sys/dtrace.h>

typedef struct {
	char	kpd_func[DTRACE_FUNCNAMELEN];
	char	kpd_mod[DTRACE_MODNAMELEN];
	int	kpd_off;
} dtrace_kinst_probedesc_t;

#define KINSTIOC_MAKEPROBE	_IOW('k', 1, dtrace_kinst_probedesc_t)

#ifdef _KERNEL

#include <sys/queue.h>

#include "kinst_isa.h"

struct kinst_probe {
	LIST_ENTRY(kinst_probe)	kp_hashnext;
	const char		*kp_func;
	char			kp_name[16];
	dtrace_id_t		kp_id;
	kinst_patchval_t	kp_patchval;
	kinst_patchval_t	kp_savedval;
	kinst_patchval_t	*kp_patchpoint;

	struct kinst_probe_md	kp_md;
};

LIST_HEAD(kinst_probe_list, kinst_probe);

extern struct kinst_probe_list	*kinst_probetab;

#define KINST_PROBETAB_MAX	0x8000	/* 32k */
#define KINST_ADDR2NDX(addr)	(((uintptr_t)(addr)) & (KINST_PROBETAB_MAX - 1))
#define KINST_GETPROBE(i) 	(&kinst_probetab[KINST_ADDR2NDX(i)])

struct linker_file;
struct linker_symval;

volatile void	*kinst_memcpy(volatile void *, volatile const void *, size_t);
bool	kinst_excluded(const char *);
bool	kinst_md_excluded(const char *);
int	kinst_invop(uintptr_t, struct trapframe *, uintptr_t);
int	kinst_make_probe(struct linker_file *, int, struct linker_symval *,
	    void *);
void	kinst_patch_tracepoint(struct kinst_probe *, kinst_patchval_t);
void	kinst_probe_create(struct kinst_probe *, struct linker_file *);

int	kinst_trampoline_init(void);
int	kinst_trampoline_deinit(void);
uint8_t	*kinst_trampoline_alloc(int);
void	kinst_trampoline_dealloc(uint8_t *);

int	kinst_md_init(void);
void	kinst_md_deinit(void);

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_KINST);
#endif /* MALLOC_DECLARE */

#define KINST_LOG_HELPER(fmt, ...)	\
	printf("%s:%d: " fmt "%s\n", __func__, __LINE__, __VA_ARGS__)
#define KINST_LOG(...)			\
	KINST_LOG_HELPER(__VA_ARGS__, "")

#endif /* _KERNEL */

#endif /* _KINST_H_ */
