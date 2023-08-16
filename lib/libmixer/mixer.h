/*-
 * Copyright (c) 2021 Christos Margiolis <christos@FreeBSD.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef _MIXER_H_
#define _MIXER_H_

#include <sys/cdefs.h>
#include <sys/queue.h>
#include <sys/soundcard.h>

#include <limits.h>

#define MIX_ISSET(n,f)		(((1U << (n)) & (f)) ? 1 : 0)
#define MIX_ISDEV(m,n)		MIX_ISSET(n, (m)->devmask)
#define MIX_ISMUTE(m,n)		MIX_ISSET(n, (m)->mutemask)
#define MIX_ISREC(m,n)		MIX_ISSET(n, (m)->recmask)
#define MIX_ISRECSRC(m,n)	MIX_ISSET(n, (m)->recsrc)

/* Forward declarations */
struct mixer;
struct mix_dev;

typedef struct mix_ctl mix_ctl_t;
typedef struct mix_volume mix_volume_t;

/* User-defined controls */
struct mix_ctl {
	struct mix_dev *parent_dev;		/* parent device */
	int id;					/* control id */
	char name[NAME_MAX];			/* control name */
	int (*mod)(struct mix_dev *, void *);	/* modify control values */
	int (*print)(struct mix_dev *, void *);	/* print control */
	TAILQ_ENTRY(mix_ctl) ctls;
};

struct mix_dev {
	struct mixer *parent_mixer;		/* parent mixer */
	char name[NAME_MAX];			/* device name (e.g "vol") */
	int devno;				/* device number */
	struct mix_volume {
#define MIX_VOLMIN		0.0f
#define MIX_VOLMAX		1.0f
#define MIX_VOLNORM(v)		((v) / 100.0f)
#define MIX_VOLDENORM(v)	((int)((v) * 100.0f + 0.5f))
		float left;			/* left volume */
		float right;			/* right volume */
	} vol;
	int nctl;				/* number of controls */
	TAILQ_HEAD(mix_ctlhead, mix_ctl) ctls;	/* control list */
	TAILQ_ENTRY(mix_dev) devs;
};

struct mixer {
	TAILQ_HEAD(mix_devhead, mix_dev) devs;	/* device list */
	struct mix_dev *dev;			/* selected device */
	oss_mixerinfo mi;			/* mixer info */
	oss_card_info ci;			/* audio card info */
	char name[NAME_MAX];			/* mixer name (e.g /dev/mixer0) */
	int fd;					/* file descriptor */
	int unit;				/* audio card unit */
	int ndev;				/* number of devices */
	int devmask;				/* supported devices */
#define MIX_MUTE		0x01
#define MIX_UNMUTE		0x02
#define MIX_TOGGLEMUTE		0x04
	int mutemask;				/* muted devices */
	int recmask;				/* recording devices */
#define MIX_ADDRECSRC		0x01
#define MIX_REMOVERECSRC	0x02
#define MIX_SETRECSRC		0x04
#define MIX_TOGGLERECSRC	0x08
	int recsrc;				/* recording sources */
#define MIX_MODE_MIXER		0x01
#define MIX_MODE_PLAY		0x02
#define MIX_MODE_REC		0x04
	int mode;				/* dev.pcm.X.mode sysctl */
	int f_default;				/* default mixer flag */
};

__BEGIN_DECLS

struct mixer *mixer_open(const char *);
int mixer_close(struct mixer *);
struct mix_dev *mixer_get_dev(struct mixer *, int);
struct mix_dev *mixer_get_dev_byname(struct mixer *, const char *);
int mixer_add_ctl(struct mix_dev *, int, const char *,
    int (*)(struct mix_dev *, void *), int (*)(struct mix_dev *, void *));
int mixer_add_ctl_s(mix_ctl_t *);
int mixer_remove_ctl(mix_ctl_t *);
mix_ctl_t *mixer_get_ctl(struct mix_dev *, int);
mix_ctl_t *mixer_get_ctl_byname(struct mix_dev *, const char *);
int mixer_set_vol(struct mixer *, mix_volume_t);
int mixer_set_mute(struct mixer *, int);
int mixer_mod_recsrc(struct mixer *, int);
int mixer_get_dunit(void);
int mixer_set_dunit(struct mixer *, int);
int mixer_get_mode(int);
int mixer_get_nmixers(void);

__END_DECLS

#endif /* _MIXER_H_ */
