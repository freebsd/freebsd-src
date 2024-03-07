/*
 * Copyright (c) 2017 Stefan Sperling <stsp@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/queue.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sha1.h>
#include <zlib.h>

#include "got_object.h"
#include "got_repository.h"
#include "got_error.h"
#include "got_diff.h"
#include "got_opentemp.h"
#include "got_path.h"
#include "got_cancel.h"
#include "got_worktree.h"

#include "got_lib_diff.h"
#include "got_lib_delta.h"
#include "got_lib_inflate.h"
#include "got_lib_object.h"

static const struct got_error *
diff_blobs(struct got_blob_object *blob1, struct got_blob_object *blob2,
    const char *label1, const char *label2, mode_t mode1, mode_t mode2,
    int diff_context, int ignore_whitespace, FILE *outfile,
    struct got_diff_changes *changes)
{
	struct got_diff_state ds;
	struct got_diff_args args;
	const struct got_error *err = NULL;
	FILE *f1 = NULL, *f2 = NULL;
	char hex1[SHA1_DIGEST_STRING_LENGTH];
	char hex2[SHA1_DIGEST_STRING_LENGTH];
	char *idstr1 = NULL, *idstr2 = NULL;
	size_t size1, size2;
	int res, flags = 0;

	if (blob1) {
		f1 = got_opentemp();
		if (f1 == NULL)
			return got_error_from_errno("got_opentemp");
	} else
		flags |= D_EMPTY1;

	if (blob2) {
		f2 = got_opentemp();
		if (f2 == NULL) {
			err = got_error_from_errno("got_opentemp");
			fclose(f1);
			return err;
		}
	} else
		flags |= D_EMPTY2;

	size1 = 0;
	if (blob1) {
		idstr1 = got_object_blob_id_str(blob1, hex1, sizeof(hex1));
		err = got_object_blob_dump_to_file(&size1, NULL, NULL, f1,
		    blob1);
		if (err)
			goto done;
	} else
		idstr1 = "/dev/null";

	size2 = 0;
	if (blob2) {
		idstr2 = got_object_blob_id_str(blob2, hex2, sizeof(hex2));
		err = got_object_blob_dump_to_file(&size2, NULL, NULL, f2,
		    blob2);
		if (err)
			goto done;
	} else
		idstr2 = "/dev/null";

	memset(&ds, 0, sizeof(ds));
	/* XXX should stat buffers be passed in args instead of ds? */
	ds.stb1.st_mode = S_IFREG;
	if (blob1)
		ds.stb1.st_size = size1;
	ds.stb1.st_mtime = 0; /* XXX */

	ds.stb2.st_mode = S_IFREG;
	if (blob2)
		ds.stb2.st_size = size2;
	ds.stb2.st_mtime = 0; /* XXX */

	memset(&args, 0, sizeof(args));
	args.diff_format = D_UNIFIED;
	args.label[0] = label1 ? label1 : idstr1;
	args.label[1] = label2 ? label2 : idstr2;
	args.diff_context = diff_context;
	flags |= D_PROTOTYPE;
	if (ignore_whitespace)
		flags |= D_IGNOREBLANKS;

	if (outfile) {
		char *modestr1 = NULL, *modestr2 = NULL;
		int modebits;
		if (mode1 && mode1 != mode2) {
			if (S_ISLNK(mode1))
				modebits = S_IFLNK;
			else
				modebits = (S_IRWXU | S_IRWXG | S_IRWXO);
			if (asprintf(&modestr1, " (mode %o)",
			    mode1 & modebits) == -1) {
				err = got_error_from_errno("asprintf");
				goto done;
			}
		}
		if (mode2 && mode1 != mode2) {
			if (S_ISLNK(mode2))
				modebits = S_IFLNK;
			else
				modebits = (S_IRWXU | S_IRWXG | S_IRWXO);
			if (asprintf(&modestr2, " (mode %o)",
			    mode2 & modebits) == -1) {
				err = got_error_from_errno("asprintf");
				goto done;
			}
		}
		fprintf(outfile, "blob - %s%s\n", idstr1,
		    modestr1 ? modestr1 : "");
		fprintf(outfile, "blob + %s%s\n", idstr2,
		    modestr2 ? modestr2 : "");
		free(modestr1);
		free(modestr2);
	}
	err = got_diffreg(&res, f1, f2, flags, &args, &ds, outfile, changes);
	got_diff_state_free(&ds);
done:
	if (f1 && fclose(f1) != 0 && err == NULL)
		err = got_error_from_errno("fclose");
	if (f2 && fclose(f2) != 0 && err == NULL)
		err = got_error_from_errno("fclose");
	return err;
}

const struct got_error *
got_diff_blob_output_unidiff(void *arg, struct got_blob_object *blob1,
    struct got_blob_object *blob2, struct got_object_id *id1,
    struct got_object_id *id2, const char *label1, const char *label2,
    mode_t mode1, mode_t mode2, struct got_repository *repo)
{
	struct got_diff_blob_output_unidiff_arg *a = arg;

	return diff_blobs(blob1, blob2, label1, label2, mode1, mode2,
	    a->diff_context, a->ignore_whitespace, a->outfile, NULL);
}

const struct got_error *
got_diff_blob(struct got_blob_object *blob1, struct got_blob_object *blob2,
    const char *label1, const char *label2, int diff_context,
    int ignore_whitespace, FILE *outfile)
{
	return diff_blobs(blob1, blob2, label1, label2, 0, 0, diff_context,
	    ignore_whitespace, outfile, NULL);
}

static const struct got_error *
alloc_changes(struct got_diff_changes **changes)
{
	*changes = calloc(1, sizeof(**changes));
	if (*changes == NULL)
		return got_error_from_errno("calloc");
	SIMPLEQ_INIT(&(*changes)->entries);
	return NULL;
}

static const struct got_error *
diff_blob_file(struct got_diff_changes **changes,
    struct got_blob_object *blob1, const char *label1, FILE *f2, size_t size2,
    const char *label2, int diff_context, int ignore_whitespace, FILE *outfile)
{
	struct got_diff_state ds;
	struct got_diff_args args;
	const struct got_error *err = NULL;
	FILE *f1 = NULL;
	char hex1[SHA1_DIGEST_STRING_LENGTH];
	char *idstr1 = NULL;
	size_t size1;
	int res, flags = 0;

	if (changes)
		*changes = NULL;

	size1 = 0;
	if (blob1) {
		f1 = got_opentemp();
		if (f1 == NULL)
			return got_error_from_errno("got_opentemp");
		idstr1 = got_object_blob_id_str(blob1, hex1, sizeof(hex1));
		err = got_object_blob_dump_to_file(&size1, NULL, NULL, f1,
		    blob1);
		if (err)
			goto done;
	} else {
		flags |= D_EMPTY1;
		idstr1 = "/dev/null";
	}

	if (f2 == NULL)
		flags |= D_EMPTY2;

	memset(&ds, 0, sizeof(ds));
	/* XXX should stat buffers be passed in args instead of ds? */
	ds.stb1.st_mode = S_IFREG;
	if (blob1)
		ds.stb1.st_size = size1;
	ds.stb1.st_mtime = 0; /* XXX */

	ds.stb2.st_mode = S_IFREG;
	ds.stb2.st_size = size2;
	ds.stb2.st_mtime = 0; /* XXX */

	memset(&args, 0, sizeof(args));
	args.diff_format = D_UNIFIED;
	args.label[0] = label2;
	args.label[1] = label2;
	args.diff_context = diff_context;
	flags |= D_PROTOTYPE;
	if (ignore_whitespace)
		flags |= D_IGNOREBLANKS;

	if (outfile) {
		fprintf(outfile, "blob - %s\n", label1 ? label1 : idstr1);
		fprintf(outfile, "file + %s\n",
		    f2 == NULL ? "/dev/null" : label2);
	}
	if (changes) {
		err = alloc_changes(changes);
		if (err)
			return err;
	}
	err = got_diffreg(&res, f1, f2, flags, &args, &ds, outfile,
	    changes ? *changes : NULL);
	got_diff_state_free(&ds);
done:
	if (f1 && fclose(f1) != 0 && err == NULL)
		err = got_error_from_errno("fclose");
	return err;
}

const struct got_error *
got_diff_blob_file(struct got_blob_object *blob1, const char *label1,
    FILE *f2, size_t size2, const char *label2, int diff_context,
    int ignore_whitespace, FILE *outfile)
{
	return diff_blob_file(NULL, blob1, label1, f2, size2, label2,
	    diff_context, ignore_whitespace, outfile);
}

const struct got_error *
got_diff_blob_file_lines_changed(struct got_diff_changes **changes,
    struct got_blob_object *blob1, FILE *f2, size_t size2)
{
	return diff_blob_file(changes, blob1, NULL, f2, size2, NULL,
	    0, 0, NULL);
}

const struct got_error *
got_diff_blob_lines_changed(struct got_diff_changes **changes,
    struct got_blob_object *blob1, struct got_blob_object *blob2)
{
	const struct got_error *err = NULL;

	err = alloc_changes(changes);
	if (err)
		return err;

	err = diff_blobs(blob1, blob2, NULL, NULL, 0, 0, 3, 0, NULL, *changes);
	if (err) {
		got_diff_free_changes(*changes);
		*changes = NULL;
	}
	return err;
}

void
got_diff_free_changes(struct got_diff_changes *changes)
{
	struct got_diff_change *change;
	while (!SIMPLEQ_EMPTY(&changes->entries)) {
		change = SIMPLEQ_FIRST(&changes->entries);
		SIMPLEQ_REMOVE_HEAD(&changes->entries, entry);
		free(change);
	}
	free(changes);
}

static const struct got_error *
diff_added_blob(struct got_object_id *id, const char *label, mode_t mode,
    struct got_repository *repo, got_diff_blob_cb cb, void *cb_arg)
{
	const struct got_error *err;
	struct got_blob_object  *blob = NULL;
	struct got_object *obj = NULL;

	err = got_object_open(&obj, repo, id);
	if (err)
		return err;

	err = got_object_blob_open(&blob, repo, obj, 8192);
	if (err)
		goto done;
	err = cb(cb_arg, NULL, blob, NULL, id, NULL, label, 0, mode, repo);
done:
	got_object_close(obj);
	if (blob)
		got_object_blob_close(blob);
	return err;
}

static const struct got_error *
diff_modified_blob(struct got_object_id *id1, struct got_object_id *id2,
    const char *label1, const char *label2, mode_t mode1, mode_t mode2,
    struct got_repository *repo, got_diff_blob_cb cb, void *cb_arg)
{
	const struct got_error *err;
	struct got_object *obj1 = NULL;
	struct got_object *obj2 = NULL;
	struct got_blob_object *blob1 = NULL;
	struct got_blob_object *blob2 = NULL;

	err = got_object_open(&obj1, repo, id1);
	if (err)
		return err;
	if (obj1->type != GOT_OBJ_TYPE_BLOB) {
		err = got_error(GOT_ERR_OBJ_TYPE);
		goto done;
	}

	err = got_object_open(&obj2, repo, id2);
	if (err)
		goto done;
	if (obj2->type != GOT_OBJ_TYPE_BLOB) {
		err = got_error(GOT_ERR_BAD_OBJ_DATA);
		goto done;
	}

	err = got_object_blob_open(&blob1, repo, obj1, 8192);
	if (err)
		goto done;

	err = got_object_blob_open(&blob2, repo, obj2, 8192);
	if (err)
		goto done;

	err = cb(cb_arg, blob1, blob2, id1, id2, label1, label2, mode1, mode2,
	    repo);
done:
	if (obj1)
		got_object_close(obj1);
	if (obj2)
		got_object_close(obj2);
	if (blob1)
		got_object_blob_close(blob1);
	if (blob2)
		got_object_blob_close(blob2);
	return err;
}

static const struct got_error *
diff_deleted_blob(struct got_object_id *id, const char *label, mode_t mode,
    struct got_repository *repo, got_diff_blob_cb cb, void *cb_arg)
{
	const struct got_error *err;
	struct got_blob_object  *blob = NULL;
	struct got_object *obj = NULL;

	err = got_object_open(&obj, repo, id);
	if (err)
		return err;

	err = got_object_blob_open(&blob, repo, obj, 8192);
	if (err)
		goto done;
	err = cb(cb_arg, blob, NULL, id, NULL, label, NULL, mode, 0, repo);
done:
	got_object_close(obj);
	if (blob)
		got_object_blob_close(blob);
	return err;
}

static const struct got_error *
diff_added_tree(struct got_object_id *id, const char *label,
    struct got_repository *repo, got_diff_blob_cb cb, void *cb_arg,
    int diff_content)
{
	const struct got_error *err = NULL;
	struct got_object *treeobj = NULL;
	struct got_tree_object *tree = NULL;

	err = got_object_open(&treeobj, repo, id);
	if (err)
		goto done;

	if (treeobj->type != GOT_OBJ_TYPE_TREE) {
		err = got_error(GOT_ERR_OBJ_TYPE);
		goto done;
	}

	err = got_object_tree_open(&tree, repo, treeobj);
	if (err)
		goto done;

	err = got_diff_tree(NULL, tree, NULL, label, repo, cb, cb_arg,
	    diff_content);
done:
	if (tree)
		got_object_tree_close(tree);
	if (treeobj)
		got_object_close(treeobj);
	return err;
}

static const struct got_error *
diff_modified_tree(struct got_object_id *id1, struct got_object_id *id2,
    const char *label1, const char *label2, struct got_repository *repo,
    got_diff_blob_cb cb, void *cb_arg, int diff_content)
{
	const struct got_error *err;
	struct got_object *treeobj1 = NULL;
	struct got_object *treeobj2 = NULL;
	struct got_tree_object *tree1 = NULL;
	struct got_tree_object *tree2 = NULL;

	err = got_object_open(&treeobj1, repo, id1);
	if (err)
		goto done;

	if (treeobj1->type != GOT_OBJ_TYPE_TREE) {
		err = got_error(GOT_ERR_OBJ_TYPE);
		goto done;
	}

	err = got_object_open(&treeobj2, repo, id2);
	if (err)
		goto done;

	if (treeobj2->type != GOT_OBJ_TYPE_TREE) {
		err = got_error(GOT_ERR_OBJ_TYPE);
		goto done;
	}

	err = got_object_tree_open(&tree1, repo, treeobj1);
	if (err)
		goto done;

	err = got_object_tree_open(&tree2, repo, treeobj2);
	if (err)
		goto done;

	err = got_diff_tree(tree1, tree2, label1, label2, repo, cb, cb_arg,
	    diff_content);

done:
	if (tree1)
		got_object_tree_close(tree1);
	if (tree2)
		got_object_tree_close(tree2);
	if (treeobj1)
		got_object_close(treeobj1);
	if (treeobj2)
		got_object_close(treeobj2);
	return err;
}

static const struct got_error *
diff_deleted_tree(struct got_object_id *id, const char *label,
    struct got_repository *repo, got_diff_blob_cb cb, void *cb_arg,
    int diff_content)
{
	const struct got_error *err;
	struct got_object *treeobj = NULL;
	struct got_tree_object *tree = NULL;

	err = got_object_open(&treeobj, repo, id);
	if (err)
		goto done;

	if (treeobj->type != GOT_OBJ_TYPE_TREE) {
		err = got_error(GOT_ERR_OBJ_TYPE);
		goto done;
	}

	err = got_object_tree_open(&tree, repo, treeobj);
	if (err)
		goto done;

	err = got_diff_tree(tree, NULL, label, NULL, repo, cb, cb_arg,
	    diff_content);
done:
	if (tree)
		got_object_tree_close(tree);
	if (treeobj)
		got_object_close(treeobj);
	return err;
}

static const struct got_error *
diff_kind_mismatch(struct got_object_id *id1, struct got_object_id *id2,
    const char *label1, const char *label2, struct got_repository *repo,
    got_diff_blob_cb cb, void *cb_arg)
{
	/* XXX TODO */
	return NULL;
}

static const struct got_error *
diff_entry_old_new(struct got_tree_entry *te1,
    struct got_tree_entry *te2, const char *label1, const char *label2,
    struct got_repository *repo, got_diff_blob_cb cb, void *cb_arg,
    int diff_content)
{
	const struct got_error *err = NULL;
	int id_match;

	if (got_object_tree_entry_is_submodule(te1))
		return NULL;

	if (te2 == NULL) {
		if (S_ISDIR(te1->mode))
			err = diff_deleted_tree(&te1->id, label1, repo,
			    cb, cb_arg, diff_content);
		else {
			if (diff_content)
				err = diff_deleted_blob(&te1->id, label1,
				    te1->mode, repo, cb, cb_arg);
			else
				err = cb(cb_arg, NULL, NULL, &te1->id, NULL,
				    label1, NULL, te1->mode, 0, repo);
		}
		return err;
	} else if (got_object_tree_entry_is_submodule(te2))
		return NULL;

	id_match = (got_object_id_cmp(&te1->id, &te2->id) == 0);
	if (S_ISDIR(te1->mode) && S_ISDIR(te2->mode)) {
		if (!id_match)
			return diff_modified_tree(&te1->id, &te2->id,
			    label1, label2, repo, cb, cb_arg, diff_content);
	} else if ((S_ISREG(te1->mode) || S_ISLNK(te1->mode)) &&
	    (S_ISREG(te2->mode) || S_ISLNK(te2->mode))) {
		if (!id_match ||
		    ((te1->mode & (S_IFLNK | S_IXUSR))) !=
		    (te2->mode & (S_IFLNK | S_IXUSR))) {
			if (diff_content)
				return diff_modified_blob(&te1->id, &te2->id,
				    label1, label2, te1->mode, te2->mode,
				    repo, cb, cb_arg);
			else
				return cb(cb_arg, NULL, NULL, &te1->id,
				    &te2->id, label1, label2, te1->mode,
				    te2->mode, repo);
		}
	}

	if (id_match)
		return NULL;

	return diff_kind_mismatch(&te1->id, &te2->id, label1, label2, repo,
	    cb, cb_arg);
}

static const struct got_error *
diff_entry_new_old(struct got_tree_entry *te2,
    struct got_tree_entry *te1, const char *label2,
    struct got_repository *repo, got_diff_blob_cb cb, void *cb_arg,
    int diff_content)
{
	if (te1 != NULL) /* handled by diff_entry_old_new() */
		return NULL;

	if (got_object_tree_entry_is_submodule(te2))
		return NULL;

	if (S_ISDIR(te2->mode))
		return diff_added_tree(&te2->id, label2, repo, cb, cb_arg,
		    diff_content);

	if (diff_content)
		return diff_added_blob(&te2->id, label2, te2->mode, repo, cb,
		    cb_arg);

	return cb(cb_arg, NULL, NULL, NULL, &te2->id, NULL, label2, 0,
	    te2->mode, repo);
}

const struct got_error *
got_diff_tree_collect_changed_paths(void *arg, struct got_blob_object *blob1,
    struct got_blob_object *blob2, struct got_object_id *id1,
    struct got_object_id *id2, const char *label1, const char *label2,
    mode_t mode1, mode_t mode2, struct got_repository *repo)
{
	const struct got_error *err = NULL;
	struct got_pathlist_head *paths = arg;
	struct got_diff_changed_path *change = NULL;
	char *path = NULL;

	path = strdup(label2 ? label2 : label1);
	if (path == NULL)
		return got_error_from_errno("malloc");

	change = malloc(sizeof(*change));
	if (change == NULL) {
		err = got_error_from_errno("malloc");
		goto done;
	}

	change->status = GOT_STATUS_NO_CHANGE;
	if (id1 == NULL)
		change->status = GOT_STATUS_ADD;
	else if (id2 == NULL)
		change->status = GOT_STATUS_DELETE;
	else {
		if (got_object_id_cmp(id1, id2) != 0)
			change->status = GOT_STATUS_MODIFY;
		else if (mode1 != mode2)
			change->status = GOT_STATUS_MODE_CHANGE;
	}

	err = got_pathlist_insert(NULL, paths, path, change);
done:
	if (err) {
		free(path);
		free(change);
	}
	return err;
}

const struct got_error *
got_diff_tree(struct got_tree_object *tree1, struct got_tree_object *tree2,
    const char *label1, const char *label2, struct got_repository *repo,
    got_diff_blob_cb cb, void *cb_arg, int diff_content)
{
	const struct got_error *err = NULL;
	struct got_tree_entry *te1 = NULL;
	struct got_tree_entry *te2 = NULL;
	char *l1 = NULL, *l2 = NULL;
	int tidx1 = 0, tidx2 = 0;

	if (tree1) {
		te1 = got_object_tree_get_entry(tree1, 0);
		if (te1 && asprintf(&l1, "%s%s%s", label1, label1[0] ? "/" : "",
		    te1->name) == -1)
			return got_error_from_errno("asprintf");
	}
	if (tree2) {
		te2 = got_object_tree_get_entry(tree2, 0);
		if (te2 && asprintf(&l2, "%s%s%s", label2, label2[0] ? "/" : "",
		    te2->name) == -1)
			return got_error_from_errno("asprintf");
	}

	do {
		if (te1) {
			struct got_tree_entry *te = NULL;
			if (tree2)
				te = got_object_tree_find_entry(tree2,
				    te1->name);
			if (te) {
				free(l2);
				l2 = NULL;
				if (te && asprintf(&l2, "%s%s%s", label2,
				    label2[0] ? "/" : "", te->name) == -1)
					return
					    got_error_from_errno("asprintf");
			}
			err = diff_entry_old_new(te1, te, l1, l2, repo, cb,
			    cb_arg, diff_content);
			if (err)
				break;
		}

		if (te2) {
			struct got_tree_entry *te = NULL;
			if (tree1)
				te = got_object_tree_find_entry(tree1,
				    te2->name);
			free(l2);
			if (te) {
				if (asprintf(&l2, "%s%s%s", label2,
				    label2[0] ? "/" : "", te->name) == -1)
					return
					    got_error_from_errno("asprintf");
			} else {
				if (asprintf(&l2, "%s%s%s", label2,
				    label2[0] ? "/" : "", te2->name) == -1)
					return
					    got_error_from_errno("asprintf");
			}
			err = diff_entry_new_old(te2, te, l2, repo,
			    cb, cb_arg, diff_content);
			if (err)
				break;
		}

		free(l1);
		l1 = NULL;
		if (te1) {
			tidx1++;
			te1 = got_object_tree_get_entry(tree1, tidx1);
			if (te1 &&
			    asprintf(&l1, "%s%s%s", label1,
			    label1[0] ? "/" : "", te1->name) == -1)
				return got_error_from_errno("asprintf");
		}
		free(l2);
		l2 = NULL;
		if (te2) {
			tidx2++;
			te2 = got_object_tree_get_entry(tree2, tidx2);
			if (te2 &&
			    asprintf(&l2, "%s%s%s", label2,
			        label2[0] ? "/" : "", te2->name) == -1)
				return got_error_from_errno("asprintf");
		}
	} while (te1 || te2);

	return err;
}

const struct got_error *
got_diff_objects_as_blobs(struct got_object_id *id1, struct got_object_id *id2,
    const char *label1, const char *label2, int diff_context,
    int ignore_whitespace, struct got_repository *repo, FILE *outfile)
{
	const struct got_error *err;
	struct got_blob_object *blob1 = NULL, *blob2 = NULL;

	if (id1 == NULL && id2 == NULL)
		return got_error(GOT_ERR_NO_OBJ);

	if (id1) {
		err = got_object_open_as_blob(&blob1, repo, id1, 8192);
		if (err)
			goto done;
	}
	if (id2) {
		err = got_object_open_as_blob(&blob2, repo, id2, 8192);
		if (err)
			goto done;
	}
	err = got_diff_blob(blob1, blob2, label1, label2, diff_context,
	    ignore_whitespace, outfile);
done:
	if (blob1)
		got_object_blob_close(blob1);
	if (blob2)
		got_object_blob_close(blob2);
	return err;
}

const struct got_error *
got_diff_objects_as_trees(struct got_object_id *id1, struct got_object_id *id2,
    char *label1, char *label2, int diff_context, int ignore_whitespace,
    struct got_repository *repo, FILE *outfile)
{
	const struct got_error *err;
	struct got_tree_object *tree1 = NULL, *tree2 = NULL;
	struct got_diff_blob_output_unidiff_arg arg;

	if (id1 == NULL && id2 == NULL)
		return got_error(GOT_ERR_NO_OBJ);

	if (id1) {
		err = got_object_open_as_tree(&tree1, repo, id1);
		if (err)
			goto done;
	}
	if (id2) {
		err = got_object_open_as_tree(&tree2, repo, id2);
		if (err)
			goto done;
	}
	arg.diff_context = diff_context;
	arg.ignore_whitespace = ignore_whitespace;
	arg.outfile = outfile;
	err = got_diff_tree(tree1, tree2, label1, label2, repo,
	    got_diff_blob_output_unidiff, &arg, 1);
done:
	if (tree1)
		got_object_tree_close(tree1);
	if (tree2)
		got_object_tree_close(tree2);
	return err;
}

const struct got_error *
got_diff_objects_as_commits(struct got_object_id *id1,
    struct got_object_id *id2, int diff_context, int ignore_whitespace,
    struct got_repository *repo, FILE *outfile)
{
	const struct got_error *err;
	struct got_commit_object *commit1 = NULL, *commit2 = NULL;

	if (id2 == NULL)
		return got_error(GOT_ERR_NO_OBJ);

	if (id1) {
		err = got_object_open_as_commit(&commit1, repo, id1);
		if (err)
			goto done;
	}

	err = got_object_open_as_commit(&commit2, repo, id2);
	if (err)
		goto done;

	err = got_diff_objects_as_trees(
	    commit1 ? got_object_commit_get_tree_id(commit1) : NULL,
	    got_object_commit_get_tree_id(commit2), "", "", diff_context,
	    ignore_whitespace, repo, outfile);
done:
	if (commit1)
		got_object_commit_close(commit1);
	if (commit2)
		got_object_commit_close(commit2);
	return err;
}

const struct got_error *
got_diff_files(struct got_diff_changes **changes,
    struct got_diff_state **ds,
    struct got_diff_args **args,
    int *flags,
    FILE *f1, size_t size1, const char *label1,
    FILE *f2, size_t size2, const char *label2,
    int diff_context, FILE *outfile)
{
	const struct got_error *err = NULL;
	int res;

	*flags = 0;
	*ds = calloc(1, sizeof(**ds));
	if (*ds == NULL)
		return got_error_from_errno("calloc");
	*args = calloc(1, sizeof(**args));
	if (*args == NULL) {
		err = got_error_from_errno("calloc");
		goto done;
	}

	if (changes)
		*changes = NULL;

	if (f1 == NULL)
		*flags |= D_EMPTY1;

	if (f2 == NULL)
		*flags |= D_EMPTY2;

	/* XXX should stat buffers be passed in args instead of ds? */
	(*ds)->stb1.st_mode = S_IFREG;
	(*ds)->stb1.st_size = size1;
	(*ds)->stb1.st_mtime = 0; /* XXX */

	(*ds)->stb2.st_mode = S_IFREG;
	(*ds)->stb2.st_size = size2;
	(*ds)->stb2.st_mtime = 0; /* XXX */

	(*args)->diff_format = D_UNIFIED;
	(*args)->label[0] = label1;
	(*args)->label[1] = label2;
	(*args)->diff_context = diff_context;
	*flags |= D_PROTOTYPE;

	if (outfile) {
		fprintf(outfile, "file - %s\n",
		    f1 == NULL ? "/dev/null" : label1);
		fprintf(outfile, "file + %s\n",
		    f2 == NULL ? "/dev/null" : label2);
	}
	if (changes) {
		err = alloc_changes(changes);
		if (err)
			goto done;
	}
	err = got_diffreg(&res, f1, f2, *flags, *args, *ds, outfile,
	    changes ? *changes : NULL);
done:
	if (err) {
		if (*ds) {
			got_diff_state_free(*ds);
			free(*ds);
			*ds = NULL;
		}
		if (*args) {
			free(*args);
			*args = NULL;
		}
		if (changes) {
			if (*changes)
				got_diff_free_changes(*changes);
			*changes = NULL;
		}
	}
	return err;
}
