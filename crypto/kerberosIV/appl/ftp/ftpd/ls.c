/*
 * Copyright (c) 1999 - 2000 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of KTH nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY KTH AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL KTH OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include "ftpd_locl.h"

RCSID("$Id: ls.c,v 1.13.2.2 2000/06/23 02:51:09 assar Exp $");

struct fileinfo {
    struct stat st;
    int inode;
    int bsize;
    char mode[11];
    int n_link;
    char *user;
    char *group;
    char *size;
    char *major;
    char *minor;
    char *date;
    char *filename;
    char *link;
};

#define LS_DIRS		 1
#define LS_IGNORE_DOT	 2
#define LS_SORT_MODE	 12
#define SORT_MODE(f) ((f) & LS_SORT_MODE)
#define LS_SORT_NAME	 4
#define LS_SORT_MTIME	 8
#define LS_SORT_SIZE	12
#define LS_SORT_REVERSE	16

#define LS_SIZE		32
#define LS_INODE	64

#ifndef S_ISTXT
#define S_ISTXT S_ISVTX
#endif

#ifndef S_ISSOCK
#define S_ISSOCK(mode)  (((mode) & _S_IFMT) == S_IFSOCK)
#endif

#ifndef S_ISLNK
#define S_ISLNK(mode)   (((mode) & _S_IFMT) == S_IFLNK)
#endif

static void
make_fileinfo(const char *filename, struct fileinfo *file, int flags)
{
    char buf[128];
    struct stat *st = &file->st;

    file->inode = st->st_ino;
#ifdef S_BLKSIZE
    file->bsize = st->st_blocks * S_BLKSIZE / 1024;
#else
    file->bsize = st->st_blocks * 512 / 1024;
#endif

    if(S_ISDIR(st->st_mode))
	file->mode[0] = 'd';
    else if(S_ISCHR(st->st_mode))
	file->mode[0] = 'c';
    else if(S_ISBLK(st->st_mode))
	file->mode[0] = 'b';
    else if(S_ISREG(st->st_mode))
	file->mode[0] = '-';
    else if(S_ISFIFO(st->st_mode))
	file->mode[0] = 'p';
    else if(S_ISLNK(st->st_mode))
	file->mode[0] = 'l';
    else if(S_ISSOCK(st->st_mode))
	file->mode[0] = 's';
#ifdef S_ISWHT
    else if(S_ISWHT(st->st_mode))
	file->mode[0] = 'w';
#endif
    else 
	file->mode[0] = '?';
    {
	char *x[] = { "---", "--x", "-w-", "-wx", 
		      "r--", "r-x", "rw-", "rwx" };
	strcpy(file->mode + 1, x[(st->st_mode & S_IRWXU) >> 6]);
	strcpy(file->mode + 4, x[(st->st_mode & S_IRWXG) >> 3]);
	strcpy(file->mode + 7, x[(st->st_mode & S_IRWXO) >> 0]);
	if((st->st_mode & S_ISUID)) {
	    if((st->st_mode & S_IXUSR))
		file->mode[3] = 's';
	    else
		file->mode[3] = 'S';
	}
	if((st->st_mode & S_ISGID)) {
	    if((st->st_mode & S_IXGRP))
		file->mode[6] = 's';
	    else
		file->mode[6] = 'S';
	}
	if((st->st_mode & S_ISTXT)) {
	    if((st->st_mode & S_IXOTH))
		file->mode[9] = 't';
	    else
		file->mode[9] = 'T';
	}
    }
    file->n_link = st->st_nlink;
    {
	struct passwd *pwd;
	pwd = getpwuid(st->st_uid);
	if(pwd == NULL)
	    asprintf(&file->user, "%u", (unsigned)st->st_uid);
	else
	    file->user = strdup(pwd->pw_name);
    }
    {
	struct group *grp;
	grp = getgrgid(st->st_gid);
	if(grp == NULL)
	    asprintf(&file->group, "%u", (unsigned)st->st_gid);
	else
	    file->group = strdup(grp->gr_name);
    }
    
    if(S_ISCHR(st->st_mode) || S_ISBLK(st->st_mode)) {
#if defined(major) && defined(minor)
	asprintf(&file->major, "%u", (unsigned)major(st->st_rdev));
	asprintf(&file->minor, "%u", (unsigned)minor(st->st_rdev));
#else
	/* Don't want to use the DDI/DKI crap. */
	asprintf(&file->major, "%u", (unsigned)st->st_rdev);
	asprintf(&file->minor, "%u", 0);
#endif
    } else
	asprintf(&file->size, "%lu", (unsigned long)st->st_size);

    {
	time_t t = time(NULL);
	time_t mtime = st->st_mtime;
	struct tm *tm = localtime(&mtime);
	if((t - mtime > 6*30*24*60*60) ||
	   (mtime - t > 6*30*24*60*60))
	    strftime(buf, sizeof(buf), "%b %e  %Y", tm);
	else
	    strftime(buf, sizeof(buf), "%b %e %H:%M", tm);
	file->date = strdup(buf);
    }
    {
	const char *p = strrchr(filename, '/');
	if(p)
	    p++;
	else
	    p = filename;
	file->filename = strdup(p);
    }
    if(S_ISLNK(st->st_mode)) {
	int n;
	n = readlink((char *)filename, buf, sizeof(buf));
	if(n >= 0) {
	    buf[n] = '\0';
	    file->link = strdup(buf);
	} else
	    warn("%s: readlink", filename);
    }
}

static void
print_file(FILE *out,
	   int flags,
	   struct fileinfo *f,
	   int max_inode,
	   int max_bsize,
	   int max_n_link,
	   int max_user,
	   int max_group,
	   int max_size,
	   int max_major,
	   int max_minor,
	   int max_date)
{
    if(f->filename == NULL)
	return;

    if(flags & LS_INODE) {
	sec_fprintf2(out, "%*d", max_inode, f->inode);
	sec_fprintf2(out, "  ");
    }
    if(flags & LS_SIZE) {
	sec_fprintf2(out, "%*d", max_bsize, f->bsize);
	sec_fprintf2(out, "  ");
    }
    sec_fprintf2(out, "%s", f->mode);
    sec_fprintf2(out, "  ");
    sec_fprintf2(out, "%*d", max_n_link, f->n_link);
    sec_fprintf2(out, " ");
    sec_fprintf2(out, "%-*s", max_user, f->user);
    sec_fprintf2(out, "  ");
    sec_fprintf2(out, "%-*s", max_group, f->group);
    sec_fprintf2(out, "  ");
    if(f->major != NULL && f->minor != NULL)
	sec_fprintf2(out, "%*s, %*s", max_major, f->major, max_minor, f->minor);
    else
	sec_fprintf2(out, "%*s", max_size, f->size);
    sec_fprintf2(out, " ");
    sec_fprintf2(out, "%*s", max_date, f->date);
    sec_fprintf2(out, " ");
    sec_fprintf2(out, "%s", f->filename);
    if(f->link)
	sec_fprintf2(out, " -> %s", f->link);
    sec_fprintf2(out, "\r\n");
}

static int
compare_filename(struct fileinfo *a, struct fileinfo *b)
{
    if(a->filename == NULL)
	return 1;
    if(b->filename == NULL)
	return -1;
    return strcmp(a->filename, b->filename);
}

static int
compare_mtime(struct fileinfo *a, struct fileinfo *b)
{
    if(a->filename == NULL)
	return 1;
    if(b->filename == NULL)
	return -1;
    return a->st.st_mtime - b->st.st_mtime;
}

static int
compare_size(struct fileinfo *a, struct fileinfo *b)
{
    if(a->filename == NULL)
	return 1;
    if(b->filename == NULL)
	return -1;
    return a->st.st_size - b->st.st_size;
}

static void
list_dir(FILE *out, const char *directory, int flags);

static int
log10(int num)
{
    int i = 1;
    while(num > 10) {
	i++;
	num /= 10;
    }
    return i;
}

/*
 * Operate as lstat but fake up entries for AFS mount points so we don't
 * have to fetch them.
 */

static int
lstat_file (const char *file, struct stat *sb)
{
#ifdef KRB4
    if (k_hasafs() 
	&& strcmp(file, ".")
	&& strcmp(file, "..")) 
    {
	struct ViceIoctl    a_params;
	char               *last;
	char               *path_bkp;
	static ino_t	   ino_counter = 0, ino_last = 0;
	int		   ret;
	const int	   maxsize = 2048;
	
	path_bkp = strdup (file);
	if (path_bkp == NULL)
	    return -1;
	
	a_params.out = malloc (maxsize);
	if (a_params.out == NULL) { 
	    free (path_bkp);
	    return -1;
	}
	
	/* If path contains more than the filename alone - split it */
	
	last = strrchr (path_bkp, '/');
	if (last != NULL) {
	    *last = '\0';
	    a_params.in = last + 1;
	} else
	    a_params.in = (char *)file;
	
	a_params.in_size  = strlen (a_params.in) + 1;
	a_params.out_size = maxsize;
	
	ret = k_pioctl (last ? path_bkp : "." ,
			VIOC_AFS_STAT_MT_PT, &a_params, 0);
	free (a_params.out);
	if (ret < 0) {
	    free (path_bkp);

	    if (errno != EINVAL)
		return ret;
	    else
		/* if we get EINVAL this is probably not a mountpoint */
		return lstat (file, sb);
	}

	/* 
	 * wow this was a mountpoint, lets cook the struct stat
	 * use . as a prototype
	 */

	ret = lstat (path_bkp, sb);
	free (path_bkp);
	if (ret < 0)
	    return ret;

	if (ino_last == sb->st_ino)
	    ino_counter++;
	else {
	    ino_last    = sb->st_ino;
	    ino_counter = 0;
	}
	sb->st_ino += ino_counter;
	sb->st_nlink = 3;

	return 0;
    }
#endif /* KRB4 */
    return lstat (file, sb);
}

static void
list_files(FILE *out, char **files, int n_files, int flags)
{
    struct fileinfo *fi;
    int i;

    fi = calloc(n_files, sizeof(*fi));
    if (fi == NULL) {
	sec_fprintf2(out, "ouf of memory\r\n");
	return;
    }
    for(i = 0; i < n_files; i++) {
	if(lstat_file(files[i], &fi[i].st) < 0) {
	    sec_fprintf2(out, "%s: %s\r\n", files[i], strerror(errno));
	    fi[i].filename = NULL;
	} else {
	    if((flags & LS_DIRS) == 0 && S_ISDIR(fi[i].st.st_mode)) {
		if(n_files > 1)
		    sec_fprintf2(out, "%s:\r\n", files[i]);
		list_dir(out, files[i], flags);
	    } else {
		make_fileinfo(files[i], &fi[i], flags);
	    }
	}
    }
    switch(SORT_MODE(flags)) {
    case LS_SORT_NAME:
	qsort(fi, n_files, sizeof(*fi), 
	      (int (*)(const void*, const void*))compare_filename);
	break;
    case LS_SORT_MTIME:
	qsort(fi, n_files, sizeof(*fi), 
	      (int (*)(const void*, const void*))compare_mtime);
	break;
    case LS_SORT_SIZE:
	qsort(fi, n_files, sizeof(*fi), 
	      (int (*)(const void*, const void*))compare_size);
	break;
    }
    {
	int max_inode = 0;
	int max_bsize = 0;
	int max_n_link = 0;
	int max_user = 0;
	int max_group = 0;
	int max_size = 0;
	int max_major = 0;
	int max_minor = 0;
	int max_date = 0;
	for(i = 0; i < n_files; i++) {
	    if(fi[i].filename == NULL)
		continue;
	    if(fi[i].inode > max_inode)
		max_inode = fi[i].inode;
	    if(fi[i].bsize > max_bsize)
		max_bsize = fi[i].bsize;
	    if(fi[i].n_link > max_n_link)
		max_n_link = fi[i].n_link;
	    if(strlen(fi[i].user) > max_user)
		max_user = strlen(fi[i].user);
	    if(strlen(fi[i].group) > max_group)
		max_group = strlen(fi[i].group);
	    if(fi[i].major != NULL && strlen(fi[i].major) > max_major)
		max_major = strlen(fi[i].major);
	    if(fi[i].minor != NULL && strlen(fi[i].minor) > max_minor)
		max_minor = strlen(fi[i].minor);
	    if(fi[i].size != NULL && strlen(fi[i].size) > max_size)
		max_size = strlen(fi[i].size);
	    if(strlen(fi[i].date) > max_date)
		max_date = strlen(fi[i].date);
	}
	if(max_size < max_major + max_minor + 2)
	    max_size = max_major + max_minor + 2;
	else if(max_size - max_minor - 2 > max_major)
	    max_major = max_size - max_minor - 2;
	max_inode = log10(max_inode);
	max_bsize = log10(max_bsize);
	max_n_link = log10(max_n_link);

	if(flags & LS_SORT_REVERSE)
	    for(i = n_files - 1; i >= 0; i--)
		print_file(out,
			   flags,
			   &fi[i],
			   max_inode,
			   max_bsize,
			   max_n_link,
			   max_user,
			   max_group,
			   max_size,
			   max_major,
			   max_minor,
			   max_date);
	else
	    for(i = 0; i < n_files; i++)
		print_file(out,
			   flags,
			   &fi[i],
			   max_inode,
			   max_bsize,
			   max_n_link,
			   max_user,
			   max_group,
			   max_size,
			   max_major,
			   max_minor,
			   max_date);
    }
}

static void
free_files (char **files, int n)
{
    int i;

    for (i = 0; i < n; ++i)
	free (files[i]);
    free (files);
}

static void
list_dir(FILE *out, const char *directory, int flags)
{
    DIR *d = opendir(directory);
    struct dirent *ent;
    char **files = NULL;
    int n_files = 0;

    if(d == NULL) {
	sec_fprintf2(out, "%s: %s\r\n", directory, strerror(errno));
	return;
    }
    while((ent = readdir(d)) != NULL) {
	void *tmp;

	if(ent->d_name[0] == '.') {
	    if (flags & LS_IGNORE_DOT)
	        continue;
	    if (ent->d_name[1] == 0) /* Ignore . */
	        continue;
	    if (ent->d_name[1] == '.' && ent->d_name[2] == 0) /* Ignore .. */
	        continue;
	}
	tmp = realloc(files, (n_files + 1) * sizeof(*files));
	if (tmp == NULL) {
	    sec_fprintf2(out, "%s: out of memory\r\n", directory);
	    free_files (files, n_files);
	    closedir (d);
	    return;
	}
	files = tmp;
	asprintf(&files[n_files], "%s/%s", directory, ent->d_name);
	if (files[n_files] == NULL) {
	    sec_fprintf2(out, "%s: out of memory\r\n", directory);
	    free_files (files, n_files);
	    closedir (d);
	    return;
	}
	++n_files;
    }
    closedir(d);
    list_files(out, files, n_files, flags | LS_DIRS);
}

void
builtin_ls(FILE *out, const char *file)
{
    int flags = LS_SORT_NAME;

    if(*file == '-') {
	const char *p;
	for(p = file + 1; *p; p++) {
	    switch(*p) {
	    case 'a':
	    case 'A':
		flags &= ~LS_IGNORE_DOT;
		break;
	    case 'C':
		break;
	    case 'd':
		flags |= LS_DIRS;
		break;
	    case 'f':
		flags = (flags & ~LS_SORT_MODE);
		break;
	    case 'i':
		flags |= flags | LS_INODE;
		break;
	    case 'l':
		break;
	    case 't':
		flags = (flags & ~LS_SORT_MODE) | LS_SORT_MTIME;
		break;
	    case 's':
		flags |= LS_SIZE;
		break;
	    case 'S':
		flags = (flags & ~LS_SORT_MODE) | LS_SORT_SIZE;
		break;
	    case 'r':
		flags |= LS_SORT_REVERSE;
		break;
	    }
	}
	file = ".";
    }
    list_files(out, &file, 1, flags);
    sec_fflush(out);
}
