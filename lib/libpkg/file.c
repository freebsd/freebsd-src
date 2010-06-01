/*
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
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
 * Jordan K. Hubbard
 * 18 July 1993
 *
 * Miscellaneous file access utilities.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "pkg.h"
#include <err.h>
#include <pwd.h>
#include <time.h>
#include <sys/wait.h>

/* Quick check to see if a file exists */
Boolean
fexists(const char *fname)
{
    int fd;

    if ((fd = open(fname, O_RDONLY)) == -1)
	return FALSE;

    close(fd);
    return TRUE;
}

/* Quick check to see if something is a directory or symlink to a directory */
Boolean
isdir(const char *fname)
{
    struct stat sb;

    if (lstat(fname, &sb) != FAIL && S_ISDIR(sb.st_mode))
	return TRUE;
    else if (lstat(strconcat(fname, "/."), &sb) != FAIL && S_ISDIR(sb.st_mode))
	return TRUE;
    else
	return FALSE;
}

/* Check to see if file is a dir or symlink to a dir, and is empty */
Boolean
isemptydir(const char *fname)
{
    if (isdir(fname)) {
	DIR *dirp;
	struct dirent *dp;

	dirp = opendir(fname);
	if (!dirp)
	    return FALSE;	/* no perms, leave it alone */
	for (dp = readdir(dirp); dp != NULL; dp = readdir(dirp)) {
	    if (strcmp(dp->d_name, ".") && strcmp(dp->d_name, "..")) {
		closedir(dirp);
		return FALSE;
	    }
	}
	(void)closedir(dirp);
	return TRUE;
    }
    return FALSE;
}

/*
 * Returns TRUE if file is a regular file or symlink pointing to a regular
 * file
 */
Boolean
isfile(const char *fname)
{
    struct stat sb;
    if (stat(fname, &sb) != FAIL && S_ISREG(sb.st_mode))
	return TRUE;
    return FALSE;
}

/*
 * Check to see if file is a file or symlink pointing to a file and is empty.
 * If nonexistent or not a file, say "it's empty", otherwise return TRUE if
 * zero sized.
 */
Boolean
isemptyfile(const char *fname)
{
    struct stat sb;
    if (stat(fname, &sb) != FAIL && S_ISREG(sb.st_mode)) {
	if (sb.st_size != 0)
	    return FALSE;
    }
    return TRUE;
}

/* Returns TRUE if file is a symbolic link. */
Boolean
issymlink(const char *fname)
{
    struct stat sb;
    if (lstat(fname, &sb) != FAIL && S_ISLNK(sb.st_mode))
	return TRUE;
    return FALSE;
}

/* Returns TRUE if file is a URL specification */
Boolean
isURL(const char *fname)
{
    /*
     * I'm sure there are other types of URL specifications that I could
     * also be looking for here, but for now I'll just be happy to get ftp
     * and http working.
     */
    if (!fname)
	return FALSE;
    while (isspace(*fname))
	++fname;
    if (!strncmp(fname, "ftp://", 6) || !strncmp(fname, "http://", 7) ||
	!strncmp(fname, "https://", 8) || !strncmp(fname, "file://", 7))
	return TRUE;
    return FALSE;
}

char *
fileFindByPath(const char *base, const char *fname)
{
    static char tmp[FILENAME_MAX];
    char *cp;
    const char *suffixes[] = {".tbz", ".tgz", ".tar", NULL};
    int i;

    if (fexists(fname) && isfile(fname)) {
	strcpy(tmp, fname);
	return tmp;
    }
    if (base) {
	strcpy(tmp, base);

	cp = strrchr(tmp, '/');
	if (cp) {
	    *cp = '\0';	/* chop name */
	    cp = strrchr(tmp, '/');
	}
	if (cp)
	    for (i = 0; suffixes[i] != NULL; i++) {
		*(cp + 1) = '\0';
		strcat(cp, "All/");
		strcat(cp, fname);
		strcat(cp, suffixes[i]);
		if (fexists(tmp))
		    return tmp;
	    }
    }

    cp = getenv("PKG_PATH");
    while (cp) {
	char *cp2 = strsep(&cp, ":");

	for (i = 0; suffixes[i] != NULL; i++) {
	    snprintf(tmp, FILENAME_MAX, "%s/%s%s", cp2 ? cp2 : cp, fname, suffixes[i]);
	    if (fexists(tmp) && isfile(tmp))
		return tmp;
	}
    }
    return NULL;
}

char *
fileGetContents(const char *fname)
{
    char *contents;
    struct stat sb;
    int fd;

    if (stat(fname, &sb) == FAIL) {
	cleanup(0);
	errx(2, "%s: can't stat '%s'", __func__, fname);
    }

    contents = (char *)malloc(sb.st_size + 1);
    fd = open(fname, O_RDONLY, 0);
    if (fd == FAIL) {
	cleanup(0);
	errx(2, "%s: unable to open '%s' for reading", __func__, fname);
    }
    if (read(fd, contents, sb.st_size) != sb.st_size) {
	cleanup(0);
	errx(2, "%s: short read on '%s' - did not get %lld bytes", __func__,
	     fname, (long long)sb.st_size);
    }
    close(fd);
    contents[sb.st_size] = '\0';
    return contents;
}

/*
 * Takes a filename and package name, returning (in "try") the
 * canonical "preserve" name for it.
 */
Boolean
make_preserve_name(char *try, int max, const char *name, const char *file)
{
    int len, i;

    if ((len = strlen(file)) == 0)
	return FALSE;
    else
	i = len - 1;
    strncpy(try, file, max);
    if (try[i] == '/') /* Catch trailing slash early and save checking in the loop */
	--i;
    for (; i; i--) {
	if (try[i] == '/') {
	    try[i + 1]= '.';
	    strncpy(&try[i + 2], &file[i + 1], max - i - 2);
	    break;
	}
    }
    if (!i) {
	try[0] = '.';
	strncpy(try + 1, file, max - 1);
    }
    /* I should probably be called rude names for these inline assignments */
    strncat(try, ".",  max -= strlen(try));
    strncat(try, name, max -= strlen(name));
    strncat(try, ".",  max--);
    strncat(try, "backup", max -= 6);
    return TRUE;
}

/* Write the contents of "str" to a file */
void
write_file(const char *name, const char *str)
{
    FILE *fp;
    size_t len;

    fp = fopen(name, "w");
    if (!fp) {
	cleanup(0);
	errx(2, "%s: cannot fopen '%s' for writing", __func__, name);
    }
    len = strlen(str);
    if (fwrite(str, 1, len, fp) != len) {
	cleanup(0);
	errx(2, "%s: short fwrite on '%s', tried to write %ld bytes",
	    __func__, name, (long)len);
    }
    if (fclose(fp)) {
	cleanup(0);
	errx(2, "%s: failure to fclose '%s'", __func__, name);
    }
}

void
copy_file(const char *dir, const char *fname, const char *to)
{
    char cmd[FILENAME_MAX];

    if (fname[0] == '/')
	snprintf(cmd, FILENAME_MAX, "/bin/cp -r %s %s", fname, to);
    else
	snprintf(cmd, FILENAME_MAX, "/bin/cp -r %s/%s %s", dir, fname, to);
    if (vsystem(cmd)) {
	cleanup(0);
	errx(2, "%s: could not perform '%s'", __func__, cmd);
    }
}

void
move_file(const char *dir, const char *fname, const char *tdir)
{
    char from[FILENAME_MAX];
    char to[FILENAME_MAX];

    if (fname[0] == '/')
	strncpy(from, fname, FILENAME_MAX);
    else
	snprintf(from, FILENAME_MAX, "%s/%s", dir, fname);

    snprintf(to, FILENAME_MAX, "%s/%s", tdir, fname);

    if (rename(from, to) == -1) {
        if (vsystem("/bin/mv %s %s", from, to)) {
	    cleanup(0);
	    errx(2, "%s: could not move '%s' to '%s'", __func__, from, to);
	}
    }
}

/*
 * Copy a hierarchy (possibly from dir) to the current directory, or
 * if "to" is TRUE, from the current directory to a location someplace
 * else.
 *
 * Though slower, using tar to copy preserves symlinks and everything
 * without me having to write some big hairy routine to do it.
 */
void
copy_hierarchy(const char *dir, const char *fname, Boolean to)
{
    char cmd[FILENAME_MAX * 3];

    if (!to) {
	/* If absolute path, use it */
	if (*fname == '/')
	    dir = "/";
	snprintf(cmd, FILENAME_MAX * 3, "/usr/bin/tar cf - -C %s %s | /usr/bin/tar xpf -",
		 dir, fname);
    }
    else
	snprintf(cmd, FILENAME_MAX * 3, "/usr/bin/tar cf - %s | /usr/bin/tar xpf - -C %s",
		 fname, dir);
#ifdef DEBUG
    printf("Using '%s' to copy trees.\n", cmd);
#endif
    if (system(cmd)) {
	cleanup(0);
	errx(2, "%s: could not perform '%s'", __func__, cmd);
    }
}

/* Unpack a tar file */
int
unpack(const char *pkg, const char *flist)
{
    const char *comp, *cp;
    char suff[80];

    comp = "";
    /*
     * Figure out by a crude heuristic whether this or not this is probably
     * compressed and whichever compression utility was used (gzip or bzip2).
     */
    if (strcmp(pkg, "-")) {
	cp = strrchr(pkg, '.');
	if (cp) {
	    strcpy(suff, cp + 1);
	    if (strchr(suff, 'z') || strchr(suff, 'Z')) {
		if (strchr(suff, 'b'))
		    comp = "-j";
		else
		    comp = "-z";
	    }
	}
    }
    else
	comp = "-j";
    if (vsystem("/usr/bin/tar -xp %s -f '%s' %s", comp, pkg, flist ? flist : "")) {
	warnx("tar extract of %s failed!", pkg);
	return 1;
    }
    return 0;
}

/*
 * Using fmt, replace all instances of:
 *
 * %F	With the parameter "name"
 * %D	With the parameter "dir"
 * %B	Return the directory part ("base") of %D/%F
 * %f	Return the filename part of %D/%F
 *
 * Does not check for overflow - caution!
 *
 */
void
format_cmd(char *buf, int max, const char *fmt, const char *dir, const char *name)
{
    char *cp, scratch[FILENAME_MAX * 2];
    int l;

    while (*fmt && max > 0) {
	if (*fmt == '%') {
	    switch (*++fmt) {
	    case 'F':
		strncpy(buf, name, max);
		l = strlen(name);
		buf += l, max -= l;
		break;

	    case 'D':
		strncpy(buf, dir, max);
		l = strlen(dir);
		buf += l, max -= l;
		break;

	    case 'B':
		snprintf(scratch, FILENAME_MAX * 2, "%s/%s", dir, name);
		cp = &scratch[strlen(scratch) - 1];
		while (cp != scratch && *cp != '/')
		    --cp;
		*cp = '\0';
		strncpy(buf, scratch, max);
		l = strlen(scratch);
		buf += l, max -= l;
		break;

	    case 'f':
		snprintf(scratch, FILENAME_MAX * 2, "%s/%s", dir, name);
		cp = &scratch[strlen(scratch) - 1];
		while (cp != scratch && *(cp - 1) != '/')
		    --cp;
		strncpy(buf, cp, max);
		l = strlen(cp);
		buf += l, max -= l;
		break;

	    default:
		*buf++ = *fmt;
		--max;
		break;
	    }
	    ++fmt;
	}
	else {
	    *buf++ = *fmt++;
	    --max;
	}
    }
    *buf = '\0';
}
