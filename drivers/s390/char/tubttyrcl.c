/*
 *  IBM/3270 Driver -- Copyright (C) 2000 UTS Global LLC
 *
 *  tubttyrcl.c -- Linemode Command-recall functionality
 *
 *
 *
 *
 *
 *  Author:  Richard Hitt
 */
#include "tubio.h"

int
tty3270_rcl_init(tub_t *tubp)
{
	return tty3270_rcl_resize(tubp, 20);
}

int
tty3270_rcl_resize(tub_t *tubp, int newrclk)
{
	char *(*newrclb)[];

	if (newrclk > 1000)
		return -EINVAL;
	if (newrclk <= 0) {
		tty3270_rcl_purge(tubp),
		kfree(tubp->tty_rclbufs);
		tubp->tty_rclbufs = NULL;
		return 0;
	}
	if ((newrclb = (char *(*)[])kmalloc(
	    newrclk * sizeof (char *), GFP_KERNEL)) == NULL)
		return -ENOMEM;
	memset(newrclb, 0, newrclk * sizeof (char *));
	if (tubp->tty_rclbufs != NULL) {
		int i, j, k;
		char *data;

		i = tubp->tty_rclp;
		j = newrclk;
		k = tubp->tty_rclk;
		while (j-- && k--) {
			if ((data = (*tubp->tty_rclbufs)[i]) == NULL)
				break;
			(*newrclb)[j] = data;
			(*tubp->tty_rclbufs)[i] = NULL;
			if (--i < 0)
				i = tubp->tty_rclk - 1;
		}
		tty3270_rcl_purge(tubp);
		kfree(tubp->tty_rclbufs);
	}
	tubp->tty_rclbufs = newrclb;
	tubp->tty_rclk = newrclk;
	tubp->tty_rclp = newrclk - 1;
	tty3270_rcl_sync(tubp);
	return 0;
}

int
tty3270_rcl_set(tub_t *tubp, char *buf, int count)
{
#define RCL_SIZ "recallsize="
#define L_RCL_SIZ (strlen(RCL_SIZ))
	int newsize;
	int len;
	int rc;
	char *rcl_siz = RCL_SIZ;
	int l_rcl_siz = L_RCL_SIZ;

	if (count < l_rcl_siz || strncmp(buf, rcl_siz, l_rcl_siz) != 0)
		return 0;
	if ((len = count - l_rcl_siz) == 0)
		return count;
	newsize = simple_strtoul(buf + l_rcl_siz, 0, 0);
	rc = tty3270_rcl_resize(tubp, newsize);
	return rc < 0? rc: count;
}

void
tty3270_rcl_fini(tub_t *tubp)
{
	if (tubp->tty_rclbufs != NULL) {
		tty3270_rcl_purge(tubp);
		kfree(tubp->tty_rclbufs);
		tubp->tty_rclbufs = NULL;
	}
}

void
tty3270_rcl_purge(tub_t *tubp)
{
	int i;
	char *buf;

	if (tubp->tty_rclbufs == NULL)
		return;
	for (i = 0; i < tubp->tty_rclk; i++) {
		if ((buf = (*tubp->tty_rclbufs)[i]) == NULL)
			continue;
		kfree(buf);
		(*tubp->tty_rclbufs)[i] = NULL;
	}
}

int
tty3270_rcl_get(tub_t *tubp, char *buf, int len, int inc)
{
	int iter;
	int i;
	char *data;

	if (tubp->tty_rclbufs == NULL)
		return 0;
	if (tubp->tty_rclk <= 0)	/* overcautious */
		return 0;
	if (inc != 1 && inc != -1)	/* overcautious */
		return 0;

	if ((i = tubp->tty_rclb) == -1) {
		i = tubp->tty_rclp;
		if (inc == 1)
			i++;
	} else {
		i += inc;
	}
	for (iter = tubp->tty_rclk; iter; iter--, i += inc) {
		if (i < 0)
			i = tubp->tty_rclk - 1;
		else if (i >= tubp->tty_rclk)
			i = 0;
		if ((*tubp->tty_rclbufs)[i] != NULL)
			break;
	}
	if (iter < 0 || (data = (*tubp->tty_rclbufs)[i]) == NULL)
		return 0;
	tubp->tty_rclb = i;
	if ((len = MIN(len - 1, strlen(data))) <= 0)
		return 0;
	memcpy(buf, data, len);
	buf[len] = '\0';
	return len;
}

void
tty3270_rcl_put(tub_t *tubp, char *data, int len)
{
	char *buf, **bufp;
	int i;

	if (tubp->tty_rclbufs == NULL)
		return;

	if (tubp->tty_rclk <= 0)        /* overcautious */
		return;

	/* If input area is invisible, don't log */
	if (tubp->tty_inattr == TF_INPUTN)
		return;

	/* If this & most recent cmd text match, don't log */
	if ((buf = (*tubp->tty_rclbufs)[tubp->tty_rclp]) != NULL &&
	    strlen(buf) == len && memcmp(buf, data, len) == 0) {
		tty3270_rcl_sync(tubp);
		return;
	}

	/* Don't stack zero-length commands */
	if (len == 0) {
		tty3270_rcl_sync(tubp);
		return;
	}

	i = tubp->tty_rclp;
	if (++i == tubp->tty_rclk)
		i = 0;
	bufp = &(*tubp->tty_rclbufs)[i];
	if (*bufp == NULL || strlen(*bufp) < len + 1) {
		if (*bufp) {
			kfree(*bufp);
			*bufp = NULL;
		}
		if ((*bufp = kmalloc(len + 1, GFP_ATOMIC)) == NULL)
			return;
	}
	memcpy(*bufp, data, len);
	(*bufp)[len] = '\0';
	tubp->tty_rclp = i;
	tty3270_rcl_sync(tubp);
}

void
tty3270_rcl_sync(tub_t *tubp)
{
	tubp->tty_rclb = -1;
}

