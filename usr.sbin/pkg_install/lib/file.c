#ifndef lint
static const char *rcsid = "$Id: file.c,v 1.5 1993/08/26 08:13:48 jkh Exp $";
#endif

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

#include "lib.h"

/* Quick check to see if a file exists */
Boolean
fexists(char *fname)
{
    if (!access(fname, F_OK))
	return TRUE;
    return FALSE;
}

/* Quick check to see if something is a directory */
Boolean
isdir(char *fname)
{
    struct stat sb;

    if (stat(fname, &sb) != FAIL &&
	(sb.st_mode & S_IFDIR))
	return TRUE;
    else
	return FALSE;
}

/* Check to see if file is a dir, and is empty */
Boolean
isempty(char *fname)
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

char *
get_file_contents(char *fname)
{
    char *contents;
    struct stat sb;
    int fd;

    if (stat(fname, &sb) == FAIL)
	barf("Can't stat '%s'.", fname);

    contents = (char *)malloc(sb.st_size + 1);
    fd = open(fname, O_RDONLY, 0);
    if (fd == FAIL)
	barf("Unable to open '%s' for reading.", fname);
    if (read(fd, contents, sb.st_size) != sb.st_size)
	barf("Short read on '%s' - did not get %d bytes.", fname, sb.st_size);
    close(fd);
    contents[sb.st_size] = '\0';
    return contents;
}

/* Write the contents of "str" to a file */
void
write_file(char *name, char *str)
{
    FILE *fp;
    int len;

    fp = fopen(name, "w");
    if (!fp)
	barf("Can't fopen '%s' for writing.", name);
    len = strlen(str);
    if (fwrite(str, 1, len, fp) != len)
	barf("Short fwrite on '%s', tried to write %d bytes.", name, len);
    if (fclose(fp))
	barf("failure to fclose '%s'.", name);
}

void
copy_file(char *dir, char *fname, char *to)
{
    if (vsystem("cp -p -r %s/%s %s", dir, fname, to))
	barf("Couldn't copy %s/%s to %s!", dir, fname, to);
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
copy_hierarchy(char *dir, char *fname, Boolean to)
{
    char cmd[FILENAME_MAX * 3];

    if (!to) {
	/* If absolute path, use it */
	if (*fname == '/')
	    dir = "/";
	sprintf(cmd, "tar cf - -C %s %s | tar xpf -", dir, fname);
    }
    else
	sprintf(cmd, "tar cf - %s | tar xpf - -C %s", fname, dir);
    if (system(cmd))
	barf("copy_file: Couldn't perform '%s'", cmd);
}

/* Unpack a tar file */
int
unpack(char *pkg, char *flist)
{
    char args[10], suffix[80], *cp;

    /*
     * Figure out by a crude heuristic whether this or not this is probably
     * compressed.
     */
    cp = rindex(pkg, '.');
    if (cp) {
	strcpy(suffix, cp + 1);
	if (index(suffix, 'z') || index(suffix, 'Z'))
	    strcpy(args, "z");
    }
    strcat(args, "xpf");
    if (vsystem("tar %s %s %s", args, pkg, flist ? flist : "")) {
	whinge("Tar extract of %s failed!", pkg);
	return 1;
    }
    return 0;
}
