/*-
 * Copyright (c) 2002 Dima Dorfman.
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

/*
 * DEVFS ruleset implementation.
 *
 * A note on terminology: To "run" a rule on a dirent is to take the
 * prescribed action; to "apply" a rule is to check whether it matches
 * a dirent and run if if it does.
 *
 * A note on locking: Only foreign entry points (non-static functions)
 * should deal with locking.  Everything else assumes we already hold
 * the required kind of lock.
 *
 * A note on namespace: devfs_rules_* are the non-static functions for
 * the entire "ruleset" subsystem, devfs_rule_* are the static
 * functions that operate on rules, and devfs_ruleset_* are the static
 * functions that operate on rulesets.  The line between the last two
 * isn't always clear, but the guideline is still useful.
 *
 * A note on "special" identifiers: Ruleset 0 is the NULL, or empty,
 * ruleset; it cannot be deleted or changed in any way.  This may be
 * assumed inside the code; e.g., a ruleset of 0 may be interpeted to
 * mean "no ruleset".  The interpretation of rule 0 is
 * command-dependent, but in no case is there a real rule with number
 * 0.
 *
 * A note on errno codes: To make it easier for the userland to tell
 * what went wrong, we sometimes use errno codes that are not entirely
 * appropriate for the error but that would be less ambiguous than the
 * appropriate "generic" code.  For example, when we can't find a
 * ruleset, we return ESRCH instead of ENOENT (except in
 * DEVFSIO_{R,S}GETNEXT, where a nonexistent ruleset means "end of
 * list", and the userland expects ENOENT to be this indicator); this
 * way, when an operation fails, it's clear that what couldn't be
 * found is a ruleset and not a rule (well, it's clear to those who
 * know the convention).
 */

#include "opt_devfs.h"
#ifndef NODEVFS

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/ioccom.h>

#include <fs/devfs/devfs.h>


/*
 * Kernel version of devfs_rule.
 */
struct devfs_krule {
	SLIST_ENTRY(devfs_krule) dk_list;
	struct devfs_ruleset *dk_ruleset;
	struct devfs_rule dk_rule;
};

/*
 * Structure to describe a ruleset.
 */
struct devfs_ruleset {
	SLIST_ENTRY(devfs_ruleset) ds_list;
	devfs_rsnum ds_number;
	SLIST_HEAD(, devfs_krule) ds_rules;
	int	ds_refcount;
	int	ds_flags;
#define	DS_IMMUTABLE	0x001
	int	ds_running;
};

static devfs_rid devfs_rid_input(devfs_rid rid, struct devfs_mount *dm);

static void devfs_rule_applyde(struct devfs_krule *dk,struct devfs_dirent *de);
static void devfs_rule_applyde_recursive(struct devfs_krule *dk,
		struct devfs_dirent *de);
static void devfs_rule_applydm(struct devfs_krule *dk, struct devfs_mount *dm);
static int  devfs_rule_autonumber(struct devfs_ruleset *ds, devfs_rnum *rnp);
static struct devfs_krule *devfs_rule_byid(devfs_rid rid);
static int  devfs_rule_delete(struct devfs_krule **dkp);
static dev_t devfs_rule_getdev(struct devfs_dirent *de);
static int  devfs_rule_input(struct devfs_rule *dr, struct devfs_mount *dm);
static int  devfs_rule_insert(struct devfs_rule *dr);
static int  devfs_rule_match(struct devfs_krule *dk, struct devfs_dirent *de);
static int  devfs_rule_matchpath(struct devfs_krule *dk,
		struct devfs_dirent *de);
static void devfs_rule_run(struct devfs_krule *dk, struct devfs_dirent *de);

static void devfs_ruleset_applyde(struct devfs_ruleset *ds,
		struct devfs_dirent *de);
static void devfs_ruleset_applydm(struct devfs_ruleset *ds,
		struct devfs_mount *dm);
static struct devfs_ruleset *devfs_ruleset_bynum(devfs_rsnum rsnum);
static struct devfs_ruleset *devfs_ruleset_create(devfs_rsnum rsnum);
static void devfs_ruleset_destroy(struct devfs_ruleset **dsp);
static void devfs_ruleset_reap(struct devfs_ruleset **dsp);
static int  devfs_ruleset_use(devfs_rsnum rsnum, struct devfs_mount *dm);

static SLIST_HEAD(, devfs_ruleset) devfs_rulesets;

/*
 * Called to apply the proper rules for de before the latter can be
 * exposed to the userland.  This should be called with an exclusive
 * lock on dm in case we need to run anything.
 */
void
devfs_rules_apply(struct devfs_mount *dm, struct devfs_dirent *de)
{
	struct devfs_ruleset *ds;

	ds = devfs_ruleset_bynum(dm->dm_ruleset);
	KASSERT(ds != NULL, ("mount-point has NULL ruleset"));
	devfs_ruleset_applyde(ds, de);
}

/*
 * Rule subsystem SYSINIT hook.
 */
void
devfs_rules_init(void)
{
	struct devfs_ruleset *ds;

	SLIST_INIT(&devfs_rulesets);

	ds = devfs_ruleset_create(0);
	ds->ds_flags |= DS_IMMUTABLE;
	ds->ds_refcount = 1;		/* Prevent reaping. */
}

/*
 * Rule subsystem ioctl hook.
 */
int
devfs_rules_ioctl(struct mount *mp, u_long cmd, caddr_t data, struct thread *td)
{
	struct devfs_mount *dm = VFSTODEVFS(mp);
	struct devfs_ruleset *ds;
	struct devfs_krule *dk;
	struct devfs_rule *dr;
	devfs_rsnum rsnum;
	devfs_rnum rnum;
	devfs_rid rid;
	int error;

	/*
	 * XXX: This returns an error regardless of whether we
	 * actually support the cmd or not.
	 */
	error = suser(td);
	if (error != 0)
		return (error);

	lockmgr(&dm->dm_lock, LK_SHARED, 0, td);

	switch (cmd) {
	case DEVFSIO_RADD:
		dr = (struct devfs_rule *)data;
		error = devfs_rule_input(dr, dm);
		if (error != 0)
			goto out;
		dk = devfs_rule_byid(dr->dr_id);
		if (dk != NULL) {
			error = EEXIST;
			goto out;
		}
		lockmgr(&dm->dm_lock, LK_UPGRADE, 0, td);
		error = devfs_rule_insert(dr);
		break;
	case DEVFSIO_RAPPLY:
		dr = (struct devfs_rule *)data;
		error = devfs_rule_input(dr, dm);
		if (error != 0)
			goto out;

		/*
		 * This is one of many possible hackish
		 * implementations.  The primary contender is an
		 * implementation where the rule we read in is
		 * temporarily inserted into some ruleset, perhaps
		 * with a hypothetical DRO_NOAUTO flag so that it
		 * doesn't get used where it isn't intended, and
		 * applied in the normal way.  This can be done in the
		 * userland (DEVFSIO_ADD, DEVFSIO_APPLYID,
		 * DEVFSIO_DEL) or in the kernel; either way it breaks
		 * some corner case assumptions in other parts of the
		 * code (not that this implementation doesn't do
		 * that).
		 */
		if (dr->dr_iacts & DRA_INCSET &&
		    devfs_ruleset_bynum(dr->dr_incset) == NULL) {
			error = ESRCH;
			goto out;
		}
		dk = malloc(sizeof(*dk), M_TEMP, M_WAITOK | M_ZERO);
		memcpy(&dk->dk_rule, dr, sizeof(*dr));
		lockmgr(&dm->dm_lock, LK_UPGRADE, 0, td);
		devfs_rule_applydm(dk, dm);
		lockmgr(&dm->dm_lock, LK_DOWNGRADE, 0, td);
		free(dk, M_TEMP);
		error = 0;
		break;
	case DEVFSIO_RAPPLYID:
		rid = *(devfs_rid *)data;
		rid = devfs_rid_input(rid, dm);
		dk = devfs_rule_byid(rid);
		if (dk == NULL) {
			error = ENOENT;
			goto out;
		}
		lockmgr(&dm->dm_lock, LK_UPGRADE, 0, td);
		devfs_rule_applydm(dk, dm);
		error = 0;
		break;
	case DEVFSIO_RDEL:
		rid = *(devfs_rid *)data;
		rid = devfs_rid_input(rid, dm);
		dk = devfs_rule_byid(rid);
		if (dk == NULL) {
			error = ENOENT;
			goto out;
		}
		ds = dk->dk_ruleset;
		lockmgr(&dm->dm_lock, LK_UPGRADE, 0, td);
		error = devfs_rule_delete(&dk);
		devfs_ruleset_reap(&ds);
		break;
	case DEVFSIO_RGETNEXT:
		dr = (struct devfs_rule *)data;
		error = devfs_rule_input(dr, dm);
		if (error != 0)
			goto out;
		/*
		 * We can't use devfs_rule_byid() here since that
		 * requires the rule specified to exist, but we want
		 * getnext(N) to work whether there is a rule N or not
		 * (specifically, getnext(0) must work, but we should
		 * never have a rule 0 since the add command
		 * interprets 0 to mean "auto-number").
		 */
		ds = devfs_ruleset_bynum(rid2rsn(dr->dr_id));
		if (ds == NULL) {
			error = ENOENT;
			goto out;
		}
		rnum = rid2rn(dr->dr_id);
		SLIST_FOREACH(dk, &ds->ds_rules, dk_list) {
			if (rid2rn(dk->dk_rule.dr_id) > rnum)
				break;
		}
		if (dk == NULL) {
			error = ENOENT;
			goto out;
		}
		memcpy(dr, &dk->dk_rule, sizeof(*dr));
		error = 0;
		break;
	case DEVFSIO_SUSE:
		rsnum = *(devfs_rsnum *)data;
		lockmgr(&dm->dm_lock, LK_UPGRADE, 0, td);
		error = devfs_ruleset_use(rsnum, dm);
		break;
	case DEVFSIO_SAPPLY:
		rsnum = *(devfs_rsnum *)data;
		rsnum = rid2rsn(devfs_rid_input(mkrid(rsnum, 0), dm));
		ds = devfs_ruleset_bynum(rsnum);
		if (ds == NULL) {
			error = ESRCH;
			goto out;
		}
		lockmgr(&dm->dm_lock, LK_UPGRADE, 0, td);
		devfs_ruleset_applydm(ds, dm);
		error = 0;
		break;
	case DEVFSIO_SGETNEXT:
		rsnum = *(devfs_rsnum *)data;
		SLIST_FOREACH(ds, &devfs_rulesets, ds_list) {
			if (ds->ds_number > rsnum)
				break;
		}
		if (ds == NULL)
			error = ENOENT;
		else {
			*(devfs_rsnum *)data = ds->ds_number;
			error = 0;
		}
		break;
	default:
		error = ENOIOCTL;
		break;
	}

out:
	lockmgr(&dm->dm_lock, LK_RELEASE, 0, td);
	return (error);
}

/*
 * Called to initialize dm_ruleset when there is a new mount-point.
 */
void
devfs_rules_newmount(struct devfs_mount *dm, struct thread *td)
{
	struct devfs_ruleset *ds;

	lockmgr(&dm->dm_lock, LK_EXCLUSIVE, 0, td);
	/*
	 * We can't use devfs_ruleset_use() since it will try to
	 * decrement the refcount for the old ruleset, and there is no
	 * old ruleset.  Making some value of ds_ruleset "special" to
	 * mean "don't decrement refcount" is uglier than this.
	 */
	ds = devfs_ruleset_bynum(0);
	KASSERT(ds != NULL, ("no ruleset 0"));
	++ds->ds_refcount;
	dm->dm_ruleset = 0;
	lockmgr(&dm->dm_lock, LK_RELEASE, 0, td);
}

/*
 * Adjust the rule identifier to use the ruleset of dm if one isn't
 * explicitly specified.
 *
 * Note that after this operation, rid2rsn(rid) might still be 0, and
 * that's okay; ruleset 0 is a valid ruleset, but when it's read in
 * from the userland, it means "current ruleset for this mount-point".
 */
static devfs_rid
devfs_rid_input(devfs_rid rid, struct devfs_mount *dm)
{

	if (rid2rsn(rid) == 0)
		return (mkrid(dm->dm_ruleset, rid2rn(rid)));
	else
		return (rid);
}

/*
 * Apply dk to de.
 */
static void
devfs_rule_applyde(struct devfs_krule *dk, struct devfs_dirent *de)
{

	if (devfs_rule_match(dk, de))
		devfs_rule_run(dk, de);
}

/*
 * Apply dk to de and everything under de.
 *
 * XXX: This method needs a function call for every nested
 * subdirectory in a devfs mount.  If we plan to have many of these,
 * we might eventually run out of kernel stack space.
 */
static void
devfs_rule_applyde_recursive(struct devfs_krule *dk, struct devfs_dirent *de)
{
	struct devfs_dirent *de2;

	/* XXX: Should we apply to ourselves first or last?  Does it matter? */
	TAILQ_FOREACH(de2, &de->de_dlist, de_list) {
		devfs_rule_applyde_recursive(dk, de2);
	}
	devfs_rule_applyde(dk, de);
}

/*
 * Apply dk to all entires in dm.
 */
static void
devfs_rule_applydm(struct devfs_krule *dk, struct devfs_mount *dm)
{

	devfs_rule_applyde_recursive(dk, dm->dm_basedir);
}

/*
 * Automatically select a number for a new rule in ds, and write the
 * result into rnump.
 */
static int
devfs_rule_autonumber(struct devfs_ruleset *ds, devfs_rnum *rnump)
{
	struct devfs_krule *dk;

	/* Find the last rule. */
	SLIST_FOREACH(dk, &ds->ds_rules, dk_list) {
		if (SLIST_NEXT(dk, dk_list) == NULL)
			break;
	}
	if (dk == NULL)
		*rnump = 100;
	else {
		*rnump = rid2rn(dk->dk_rule.dr_id) + 100;
		/* Detect overflow. */
		if (*rnump < rid2rn(dk->dk_rule.dr_id))
			return (ERANGE);
	}
	KASSERT(devfs_rule_byid(mkrid(ds->ds_number, *rnump)) == NULL,
	    ("autonumbering resulted in an already existing rule"));
	return (0);
}

/*
 * Find a krule by id.
 */
static struct devfs_krule *
devfs_rule_byid(devfs_rid rid)
{
	struct devfs_ruleset *ds;
	struct devfs_krule *dk;
	devfs_rnum rn;

	rn = rid2rn(rid);
	ds = devfs_ruleset_bynum(rid2rsn(rid));
	if (ds == NULL)
		return (NULL);
	SLIST_FOREACH(dk, &ds->ds_rules, dk_list) {
		if (rid2rn(dk->dk_rule.dr_id) == rn)
			return (dk);
		else if (rid2rn(dk->dk_rule.dr_id) > rn)
			break;
	}
	return (NULL);
}

/*
 * Remove dkp from any lists it may be on and remove memory associated
 * with it.
 */
static int
devfs_rule_delete(struct devfs_krule **dkp)
{
	struct devfs_krule *dk = *dkp;
	struct devfs_ruleset *ds;

	if (dk->dk_rule.dr_iacts & DRA_INCSET) {
		ds = devfs_ruleset_bynum(dk->dk_rule.dr_incset);
		KASSERT(ds != NULL, ("DRA_INCSET but bad dr_incset"));
		--ds->ds_refcount;
		devfs_ruleset_reap(&ds);
	}
	SLIST_REMOVE(&dk->dk_ruleset->ds_rules, dk, devfs_krule, dk_list);
	free(dk, M_DEVFS);
	*dkp = NULL;
	return (0);
}

/*
 * Get a dev_t corresponding to de so we can try to match rules based
 * on it.  If this routine returns NULL, there is no dev_t associated
 * with the dirent (symlinks and directories don't have dev_ts), and
 * the caller should assume that any critera dependent on a dev_t
 * don't match.
 */
static dev_t
devfs_rule_getdev(struct devfs_dirent *de)
{
	dev_t *devp, dev;

	devp = devfs_itod(de->de_inode);
	if (devp != NULL)
		dev = *devp;
	else
		dev = NULL;
	/* If we think this dirent should have a dev_t, alert the user. */
	if (dev == NULL && de->de_dirent->d_type != DT_LNK &&
	    de->de_dirent->d_type != DT_DIR)
		printf("Warning: no dev_t for %s\n", de->de_dirent->d_name);
	return (dev);
}

/*
 * Do what we need to do to a rule that we just loaded from the
 * userland.  In particular, we need to check the magic, and adjust
 * the ruleset appropriate if desired.
 */
static int
devfs_rule_input(struct devfs_rule *dr, struct devfs_mount *dm)
{

	if (dr->dr_magic != DEVFS_MAGIC)
		return (ERPCMISMATCH);
	dr->dr_id = devfs_rid_input(dr->dr_id, dm);
	return (0);
}

/*
 * Import dr into the appropriate place in the kernel (i.e., make a
 * krule).  The value of dr is copied, so the pointer may be destroyed
 * after this call completes.
 */
static int
devfs_rule_insert(struct devfs_rule *dr)
{
	struct devfs_ruleset *ds, *dsi;
	struct devfs_krule *k1, *k2;
	struct devfs_krule *dk;
	devfs_rsnum rsnum;
	devfs_rnum dkrn;
	int error;

	/*
	 * This stuff seems out of place here, but we want to do it as
	 * soon as possible so that if it fails, we don't have to roll
	 * back any changes we already made (e.g., ruleset creation).
	 */
	if (dr->dr_iacts & DRA_INCSET) {
		dsi = devfs_ruleset_bynum(dr->dr_incset);
		if (dsi == NULL)
			return (ESRCH);
	} else
		dsi = NULL;

	rsnum = rid2rsn(dr->dr_id);
	ds = devfs_ruleset_bynum(rsnum);
	if (ds == NULL)
		ds = devfs_ruleset_create(rsnum);
	if (ds->ds_flags & DS_IMMUTABLE)
		return (EIO);
	dkrn = rid2rn(dr->dr_id);
	if (dkrn == 0) {
		error = devfs_rule_autonumber(ds, &dkrn);
		if (error != 0)
			return (error);
	}

	dk = malloc(sizeof(*dk), M_DEVFS, M_WAITOK);
	dk->dk_ruleset = ds;
	if (dsi != NULL)
		++dsi->ds_refcount;
	/* XXX: Inspect dr? */
	memcpy(&dk->dk_rule, dr, sizeof(*dr));
	dk->dk_rule.dr_id = mkrid(rid2rsn(dk->dk_rule.dr_id), dkrn);

	k1 = SLIST_FIRST(&ds->ds_rules);
	if (k1 == NULL || rid2rn(k1->dk_rule.dr_id) > dkrn)
		SLIST_INSERT_HEAD(&ds->ds_rules, dk, dk_list);
	else {
		SLIST_FOREACH(k1, &ds->ds_rules, dk_list) {
			k2 = SLIST_NEXT(k1, dk_list);
			if (k2 == NULL || rid2rn(k2->dk_rule.dr_id) > dkrn) {
				SLIST_INSERT_AFTER(k1, dk, dk_list);
				break;
			}
		}
	}

	return (0);
}

/*
 * Determine whether dk matches de.  Returns 1 if dk should be run on
 * de; 0, otherwise.
 */
static int
devfs_rule_match(struct devfs_krule *dk, struct devfs_dirent *de)
{
	struct devfs_rule *dr = &dk->dk_rule;
	dev_t dev;

	dev = devfs_rule_getdev(de);
	/*
	 * At this point, if dev is NULL, we should assume that any
	 * criteria that depend on it don't match.  We should *not*
	 * just ignore them (i.e., act like they weren't specified),
	 * since that makes a rule that only has criteria dependent on
	 * the dev_t match all symlinks and directories.
	 *
	 * Note also that the following tests are somewhat reversed:
	 * They're actually testing to see whether the condition does
	 * *not* match, since the default is to assume the rule should
	 * be run (such as if there are no conditions).
	 */
	if (dr->dr_icond & DRC_DSWFLAGS)
		if (dev == NULL ||
		    (dev->si_devsw->d_flags & dr->dr_dswflags) == 0)
			goto nomatch;
	if (dr->dr_icond & DRC_PATHPTRN)
		if (!devfs_rule_matchpath(dk, de))
			goto nomatch;
	if (dr->dr_icond & DRC_MAJOR)
		if (dev == NULL || major(dev) != dr->dr_major)
			goto nomatch;

	return (1);

nomatch:
	return (0);
}

/*
 * Determine whether dk matches de on account of dr_pathptrn.
 */
static int
devfs_rule_matchpath(struct devfs_krule *dk, struct devfs_dirent *de)
{
	struct devfs_rule *dr = &dk->dk_rule;
	char *pname;
	dev_t dev;

	dev = devfs_rule_getdev(de);
	if (dev != NULL)
		pname = dev->si_name;
	else if (de->de_dirent->d_type == DT_LNK)
		pname = de->de_dirent->d_name;
	else
		return (0);
	KASSERT(pname != NULL, ("devfs_rule_matchpath: NULL pname"));

	return (fnmatch(dr->dr_pathptrn, pname, 0) == 0);
}

/*
 * Run dk on de.
 */
static void
devfs_rule_run(struct devfs_krule *dk, struct devfs_dirent *de)
{
	struct devfs_rule *dr = &dk->dk_rule;
	struct devfs_ruleset *ds;

	if (dr->dr_iacts & DRA_BACTS) {
		if (dr->dr_bacts & DRB_HIDE)
			de->de_flags |= DE_WHITEOUT;
		if (dr->dr_bacts & DRB_UNHIDE)
			de->de_flags &= ~DE_WHITEOUT;
	}
	if (dr->dr_iacts & DRA_UID)
		de->de_uid = dr->dr_uid;
	if (dr->dr_iacts & DRA_GID)
		de->de_gid = dr->dr_gid;
	if (dr->dr_iacts & DRA_MODE)
		de->de_mode = dr->dr_mode;
	if (dr->dr_iacts & DRA_INCSET) {
		ds = devfs_ruleset_bynum(dk->dk_rule.dr_incset);
		KASSERT(ds != NULL, ("DRA_INCSET but bad dr_incset"));
		if (ds->ds_running)
			printf("Warning: avoiding loop through ruleset %d\n",
			    ds->ds_number);
		else
			devfs_ruleset_applyde(ds, de);
	}
}

/*
 * Apply all the rules in ds to de.
 */
static void
devfs_ruleset_applyde(struct devfs_ruleset *ds, struct devfs_dirent *de)
{
	struct devfs_krule *dk;

	KASSERT(!ds->ds_running,("ruleset %d already running", ds->ds_number));
	ds->ds_running = 1;
	SLIST_FOREACH(dk, &ds->ds_rules, dk_list) {
		devfs_rule_applyde(dk, de);
	}
	ds->ds_running = 0;
}

/*
 * Apply all the rules in ds to all the entires in dm.
 */
static void
devfs_ruleset_applydm(struct devfs_ruleset *ds, struct devfs_mount *dm)
{
	struct devfs_krule *dk;

	KASSERT(!ds->ds_running,("ruleset %d already running", ds->ds_number));
	ds->ds_running = 1;
	/*
	 * XXX: Does it matter whether we do
	 *
	 *	foreach(dk in ds)
	 *		foreach(de in dm)
	 *			apply(dk to de)
	 *
	 * as opposed to
	 *
	 *	foreach(de in dm)
	 *		foreach(dk in ds)
	 *			apply(dk to de)
	 *
	 * The end result is obviously the same, but does the order
	 * matter?
	 */
	SLIST_FOREACH(dk, &ds->ds_rules, dk_list) {
		devfs_rule_applydm(dk, dm);
	}
	ds->ds_running = 0;
}

/*
 * Find a ruleset by number.
 */
static struct devfs_ruleset *
devfs_ruleset_bynum(devfs_rsnum rsnum)
{
	struct devfs_ruleset *ds;

	SLIST_FOREACH(ds, &devfs_rulesets, ds_list) {
		if (ds->ds_number == rsnum)
			return (ds);
	}
	return (NULL);
}

/*
 * Create a new ruleset.
 */
static struct devfs_ruleset *
devfs_ruleset_create(devfs_rsnum rsnum)
{
	struct devfs_ruleset *s1, *s2;
	struct devfs_ruleset *ds;

	KASSERT(devfs_ruleset_bynum(rsnum) == NULL,
	    ("creating already existent ruleset %d", rsnum));

	ds = malloc(sizeof(*ds), M_DEVFS, M_WAITOK | M_ZERO);
	ds->ds_number = rsnum;
	ds->ds_refcount = ds->ds_flags = 0;
	SLIST_INIT(&ds->ds_rules);

	s1 = SLIST_FIRST(&devfs_rulesets);
	if (s1 == NULL || s1->ds_number > rsnum)
		SLIST_INSERT_HEAD(&devfs_rulesets, ds, ds_list);
	else {
		SLIST_FOREACH(s1, &devfs_rulesets, ds_list) {
			s2 = SLIST_NEXT(s1, ds_list);
			if (s2 == NULL || s2->ds_number > rsnum) {
				SLIST_INSERT_AFTER(s1, ds, ds_list);
				break;
			}
		}
	}

	return (ds);
}

/*
 * Remove a ruleset form the system.  The ruleset specified must be
 * empty and not in use.
 */
static void
devfs_ruleset_destroy(struct devfs_ruleset **dsp)
{
	struct devfs_ruleset *ds = *dsp;

	KASSERT(SLIST_EMPTY(&ds->ds_rules), ("destroying non-empty ruleset"));
	KASSERT(ds->ds_refcount == 0, ("destroying busy ruleset"));
	KASSERT((ds->ds_flags & DS_IMMUTABLE) == 0,
	    ("destroying immutable ruleset"));

	SLIST_REMOVE(&devfs_rulesets, ds, devfs_ruleset, ds_list);
	free(ds, M_DEVFS);
	*dsp = NULL;
}

/*
 * Remove a ruleset from the system if it's empty and not used
 * anywhere.  This should be called after every time a rule is deleted
 * from this ruleset or the reference count is decremented.
 */
static void
devfs_ruleset_reap(struct devfs_ruleset **dsp)
{
	struct devfs_ruleset *ds = *dsp;

	if (SLIST_EMPTY(&ds->ds_rules) && ds->ds_refcount == 0) {
		devfs_ruleset_destroy(&ds);
		*dsp = ds;
	}
}

/*
 * Make rsnum the active ruleset for dm.
 */
static int
devfs_ruleset_use(devfs_rsnum rsnum, struct devfs_mount *dm)
{
	struct devfs_ruleset *cds, *ds;

	ds = devfs_ruleset_bynum(rsnum);
	if (ds == NULL)
		ds = devfs_ruleset_create(rsnum);
	cds = devfs_ruleset_bynum(dm->dm_ruleset);
	KASSERT(cds != NULL, ("mount-point has NULL ruleset"));

	/* These should probably be made atomic somehow. */
	--cds->ds_refcount;
	++ds->ds_refcount;
	dm->dm_ruleset = rsnum;

	devfs_ruleset_reap(&cds);
	return (0);
}

#endif /* !NODEVFS */
