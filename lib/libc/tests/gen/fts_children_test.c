/*-
 * Copyright (c) 2026 Jitendra Bhati
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Tests for fts_children().
 */

#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

#include "fts_test.h"

/*
 * fts_children() before fts_read() returns the list of root entries.
 */
ATF_TC(before_read);
ATF_TC_HEAD(before_read, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "fts_children before fts_read returns root entry list");
}
ATF_TC_BODY(before_read, tc)
{
	char *paths[] = { "dir", NULL };
	FTS *fts;
	FTSENT *children, *p;
	int count;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, close(creat("dir/a", 0644)));

	ATF_REQUIRE((fts = fts_open(paths, FTS_PHYSICAL, NULL)) != NULL);

	errno = 0;
	children = fts_children(fts, 0);
	ATF_REQUIRE_MSG(children != NULL,
	    "fts_children before fts_read must return the root list");
	ATF_CHECK_EQ(0, errno);

	count = 0;
	for (p = children; p != NULL; p = p->fts_link) {
		ATF_CHECK_EQ_MSG(FTS_D, p->fts_info,
		    "root entry should be FTS_D, got %d", p->fts_info);
		count++;
	}
	ATF_CHECK_EQ_MSG(1, count,
	    "expected 1 root entry, found %d", count);

	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

/*
 * fts_children() on an empty directory returns NULL with errno == 0.
 * errno=0 distinguishes "empty" from an actual error.
 */
ATF_TC(empty_dir);
ATF_TC_HEAD(empty_dir, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "fts_children on empty directory returns NULL with errno 0");
}
ATF_TC_BODY(empty_dir, tc)
{
	char *paths[] = { "dir", NULL };
	FTS *fts;
	FTSENT *ent, *children;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));

	ATF_REQUIRE((fts = fts_open(paths, FTS_PHYSICAL, NULL)) != NULL);

	ent = fts_read(fts);
	ATF_REQUIRE(ent != NULL);
	ATF_REQUIRE_EQ_MSG(FTS_D, ent->fts_info,
	    "expected FTS_D, got %d", ent->fts_info);

	errno = 1;	/* sentinel — fts_children must clear this */
	children = fts_children(fts, 0);
	ATF_CHECK_MSG(children == NULL,
	    "fts_children on empty dir must return NULL");
	ATF_CHECK_EQ_MSG(0, errno,
	    "fts_children on empty dir must set errno=0, got %d", errno);

	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

/*
 * fts_children() on a non-empty directory returns a linked list of all
 * children in comparator order.
 */
ATF_TC(nonempty_dir);
ATF_TC_HEAD(nonempty_dir, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "fts_children on non-empty directory returns all children");
}
ATF_TC_BODY(nonempty_dir, tc)
{
	static const char *expected[] = { "a", "b", "c", NULL };
	char *paths[] = { "dir", NULL };
	FTS *fts;
	FTSENT *ent, *children, *p;
	int i;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, close(creat("dir/a", 0644)));
	ATF_REQUIRE_EQ(0, close(creat("dir/b", 0644)));
	ATF_REQUIRE_EQ(0, close(creat("dir/c", 0644)));

	ATF_REQUIRE((fts = fts_open(paths, FTS_PHYSICAL,
	    fts_lexical_compar)) != NULL);

	ent = fts_read(fts);
	ATF_REQUIRE(ent != NULL);
	ATF_REQUIRE_EQ(FTS_D, ent->fts_info);

	children = fts_children(fts, 0);
	ATF_REQUIRE_MSG(children != NULL, "fts_children(): %m");

	i = 0;
	for (p = children; p != NULL; p = p->fts_link, i++) {
		ATF_REQUIRE_MSG(expected[i] != NULL,
		    "more children returned than expected");
		ATF_CHECK_STREQ(expected[i], p->fts_name);
		ATF_CHECK_EQ(FTS_F, p->fts_info);
	}
	ATF_CHECK_MSG(expected[i] == NULL,
	    "fewer children returned than expected");

	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

/*
 * fts_children() called twice on the same FTS_D node must return an
 * equivalent list both times.
 */
ATF_TC(called_twice);
ATF_TC_HEAD(called_twice, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "fts_children called twice returns equivalent results");
}
ATF_TC_BODY(called_twice, tc)
{
	char *paths[] = { "dir", NULL };
	FTS *fts;
	FTSENT *ent, *first, *second, *p;
	int count1, count2;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, close(creat("dir/x", 0644)));
	ATF_REQUIRE_EQ(0, close(creat("dir/y", 0644)));

	ATF_REQUIRE((fts = fts_open(paths, FTS_PHYSICAL,
	    fts_lexical_compar)) != NULL);

	ent = fts_read(fts);
	ATF_REQUIRE(ent != NULL);
	ATF_REQUIRE_EQ(FTS_D, ent->fts_info);

	first = fts_children(fts, 0);
	ATF_REQUIRE_MSG(first != NULL, "first fts_children call: %m");

	count1 = 0;
	for (p = first; p != NULL; p = p->fts_link)
		count1++;

	/*
	 * The second call frees the first list and rebuilds.  Do not
	 * dereference 'first' after this point — it has been freed.
	 */
	second = fts_children(fts, 0);
	ATF_REQUIRE_MSG(second != NULL, "second fts_children call: %m");

	count2 = 0;
	for (p = second; p != NULL; p = p->fts_link)
		count2++;

	ATF_CHECK_EQ_MSG(count1, count2,
	    "first call returned %d children, second returned %d",
	    count1, count2);
	ATF_CHECK_EQ(2, count2);

	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

/*
 * fts_children(FTS_NAMEONLY): only fts_name and fts_namelen are filled.
 * fts_info is FTS_NSOK for every entry.
 */
ATF_TC(nameonly);
ATF_TC_HEAD(nameonly, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "FTS_NAMEONLY fills only fts_name, fts_info is FTS_NSOK");
}
ATF_TC_BODY(nameonly, tc)
{
	char *paths[] = { "dir", NULL };
	FTS *fts;
	FTSENT *ent, *children, *p;
	int count;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, close(creat("dir/f1", 0644)));
	ATF_REQUIRE_EQ(0, close(creat("dir/f2", 0644)));

	ATF_REQUIRE((fts = fts_open(paths, FTS_PHYSICAL,
	    fts_lexical_compar)) != NULL);

	ent = fts_read(fts);
	ATF_REQUIRE(ent != NULL);
	ATF_REQUIRE_EQ(FTS_D, ent->fts_info);

	children = fts_children(fts, FTS_NAMEONLY);
	ATF_REQUIRE_MSG(children != NULL, "fts_children(FTS_NAMEONLY): %m");

	count = 0;
	for (p = children; p != NULL; p = p->fts_link) {
		ATF_CHECK_MSG(p->fts_name[0] != '\0',
		    "FTS_NAMEONLY: fts_name is empty");
		ATF_CHECK_EQ(strlen(p->fts_name), p->fts_namelen);
		ATF_CHECK_EQ_MSG(FTS_NSOK, p->fts_info,
		    "FTS_NAMEONLY: expected FTS_NSOK, got %d", p->fts_info);
		count++;
	}
	ATF_CHECK_EQ(2, count);

	/* Normal traversal must still work after FTS_NAMEONLY. */
	while (fts_read(fts) != NULL)
		;
	ATF_CHECK_EQ_MSG(0, errno,
	    "traversal after FTS_NAMEONLY ended with errno %d", errno);

	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

/*
 * fts_children() on a non-directory node must return NULL with errno == 0.
 */
ATF_TC(nondirectory);
ATF_TC_HEAD(nondirectory, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "fts_children on a non-directory node returns NULL with errno 0");
}
ATF_TC_BODY(nondirectory, tc)
{
	char *paths[] = { "dir", NULL };
	FTS *fts;
	FTSENT *ent;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, close(creat("dir/file", 0644)));

	ATF_REQUIRE((fts = fts_open(paths, FTS_PHYSICAL,
	    fts_lexical_compar)) != NULL);

	ent = fts_read(fts);	/* FTS_D dir */
	ATF_REQUIRE(ent != NULL);
	ATF_REQUIRE_EQ(FTS_D, ent->fts_info);

	ent = fts_read(fts);	/* FTS_F file */
	ATF_REQUIRE(ent != NULL);
	ATF_REQUIRE_EQ(FTS_F, ent->fts_info);

	errno = 1;
	ATF_CHECK_MSG(fts_children(fts, 0) == NULL,
	    "fts_children on FTS_F must return NULL");
	ATF_CHECK_EQ_MSG(0, errno,
	    "fts_children on FTS_F must set errno=0, got %d", errno);

	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

/*
 * fts_children() with an invalid instr value must return NULL with
 * errno == EINVAL.
 */
ATF_TC(invalid_instr);
ATF_TC_HEAD(invalid_instr, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "fts_children with invalid instr returns NULL with EINVAL");
}
ATF_TC_BODY(invalid_instr, tc)
{
	char *paths[] = { ".", NULL };
	FTS *fts;

	ATF_REQUIRE((fts = fts_open(paths, FTS_PHYSICAL, NULL)) != NULL);

	ATF_REQUIRE_ERRNO(EINVAL, fts_children(fts, 99) == NULL);

	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

ATF_TP_ADD_TCS(tp)
{
	fts_check_debug();
	ATF_TP_ADD_TC(tp, before_read);
	ATF_TP_ADD_TC(tp, empty_dir);
	ATF_TP_ADD_TC(tp, nonempty_dir);
	ATF_TP_ADD_TC(tp, called_twice);
	ATF_TP_ADD_TC(tp, nameonly);
	ATF_TP_ADD_TC(tp, nondirectory);
	ATF_TP_ADD_TC(tp, invalid_instr);

	return (atf_no_error());
}
