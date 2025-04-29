/*-
 * Copyright (c) 2020 Emmanuel Vadot <manu@FreeBSD.org>
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _LINUXKPI_LINUX_SHRINKER_H_
#define	_LINUXKPI_LINUX_SHRINKER_H_

#include <sys/queue.h>

#include <linux/bitops.h>
#include <linux/gfp.h>

struct shrink_control {
	gfp_t		gfp_mask;
	unsigned long	nr_to_scan;
	unsigned long	nr_scanned;
};

struct shrinker {
	unsigned long		(*count_objects)(struct shrinker *, struct shrink_control *);
	unsigned long		(*scan_objects)(struct shrinker *, struct shrink_control *);
	int			seeks;
	unsigned int		flags;
	void *			private_data;
	long			batch;
	TAILQ_ENTRY(shrinker)	next;
};

#define	SHRINK_STOP	(~0UL)

#define	DEFAULT_SEEKS	2

#define SHRINKER_REGISTERED	BIT(0)
#define SHRINKER_ALLOCATED	BIT(1)

struct shrinker *linuxkpi_shrinker_alloc(
    unsigned int flags, const char *fmt, ...);
int	linuxkpi_register_shrinker(struct shrinker *s);
void	linuxkpi_unregister_shrinker(struct shrinker *s);
void	linuxkpi_shrinker_free(struct shrinker *shrinker);
void	linuxkpi_synchronize_shrinkers(void);

#define	shrinker_alloc(flags, fmt, ...) \
    linuxkpi_shrinker_alloc(flags, fmt __VA_OPT__(,) __VA_ARGS__)
#define	shrinker_register(shrinker) \
    linuxkpi_register_shrinker(shrinker)
#define	shrinker_free(shrinker) \
    linuxkpi_shrinker_free(shrinker)

#if defined(LINUXKPI_VERSION) && LINUXKPI_VERSION >= 60000
#define	register_shrinker(s, ...)	linuxkpi_register_shrinker(s)
#else
#define	register_shrinker(s)	linuxkpi_register_shrinker(s)
#endif
#define	unregister_shrinker(s)	linuxkpi_unregister_shrinker(s)
#define	synchronize_shrinkers()	linuxkpi_synchronize_shrinkers()

#endif	/* _LINUXKPI_LINUX_SHRINKER_H_ */
