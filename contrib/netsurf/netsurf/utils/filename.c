/*
 * Copyright 2006 Richard Wilson <info@tinct.net>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file
 * Provides a central method of obtaining unique filenames.
 *
 * A maximum of 2^24 files can be allocated at any point in time.
 */

#include <assert.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "utils/config.h"
#include "utils/filename.h"
#include "utils/log.h"
#include "utils/utils.h"

#define FULL_WORD (unsigned int)0xffffffffu
#define START_PREFIX ('0' + '0' * 10)

struct directory {
	int numeric_prefix;		/** numeric representation of prefix */
	char prefix[10];		/** directory prefix, eg '00/11/52/' */
	unsigned int low_used;		/** first 32 files, 1 bit per file */
	unsigned int high_used;		/** last 32 files, 1 bit per file */
	struct directory *next;		/** next directory (sorted by prefix) */
};


static struct directory *root = NULL;
static char filename_buffer[12];
static char filename_directory[256];

static struct directory *filename_create_directory(const char *prefix);
static bool filename_flush_directory(const char *folder, int depth);
static bool filename_delete_recursive(char *folder);

/**
 * Request a new, unique, filename.
 *
 * \return a pointer to a shared buffer containing the new filename,
 *         NULL on failure
 */
const char *filename_request(void)
{
	struct directory *dir;
	int i = -1;

	for (dir = root; dir; dir = dir->next) {
		if ((dir->low_used & dir->high_used) != FULL_WORD) {
			if (dir->low_used != FULL_WORD) {
				for (i = 0; (dir->low_used & (1 << i)); i++);
			} else {
				for (i = 0; (dir->high_used & (1 << i)); i++);
				i += 32;
			}
			break;
		}
	}

	if (i == -1) {
		/* no available slots - create a new directory */
		dir = filename_create_directory(NULL);
		if (dir == NULL) {
			LOG(("Failed to create a new directory."));
			return NULL;
		}
		i = 63;
	}

	if (i < 32)
		dir->low_used |= (1 << i);
	else
		dir->high_used |= (1 << (i - 32));

	sprintf(filename_buffer, "%s%.2i", dir->prefix, i);

	return filename_buffer;
}


/**
 * Claim a specific filename.
 *
 * \param  filename  the filename to claim
 * \return whether the claim was successful
 */
bool filename_claim(const char *filename)
{
	char dir_prefix[9];
	int file;
	struct directory *dir;

	/* filename format is always '01/23/45/XX' */
	strncpy(dir_prefix, filename, 9);
	dir_prefix[8] = '\0';
	file = (filename[10] + filename[9] * 10 - START_PREFIX);

	/* create the directory */
	dir = filename_create_directory(dir_prefix);
	if (dir == NULL)
		return false;

	/* update the entry */
	if (file < 32) {
		if (dir->low_used & (1 << file))
			return false;
		dir->low_used |= (1 << file);
	} else {
		if (dir->high_used & (1 << (file - 32)))
			return false;
		dir->high_used |= (1 << (file - 32));
	}

	return true;
}


/**
 * Releases a filename for future use.
 *
 * \param  filename  the filename to release
 */
void filename_release(const char *filename)
{
	struct directory *dir;
	int index, file;

	/* filename format is always '01/23/45/XX' */
	index = ((filename[7] + filename[6] * 10 - START_PREFIX) |
		((filename[4] + filename[3] * 10 - START_PREFIX) << 6) |
		((filename[1] + filename[0] * 10 - START_PREFIX) << 12));
	file = (filename[10] + filename[9] * 10 - START_PREFIX);

	/* modify the correct directory entry */
	for (dir = root; dir; dir = dir->next) {
		if (dir->numeric_prefix == index) {
			if (file < 32)
				dir->low_used &= ~(1 << file);
			else
				dir->high_used &= ~(1 << (file - 32));
			return;
		}
	}
}


/**
 * Initialise the filename provider.
 */
bool filename_initialise(void)
{
	char *directory, *start;
	int ret;

	directory = strdup(TEMP_FILENAME_PREFIX);
	if (directory == NULL)
		return false;

	for (start = directory; *start != '\0'; start++) {
		if (*start == '/') {
			*start = '\0';
			LOG(("Creating \"%s\"", directory));
			ret = nsmkdir(directory, S_IRWXU);
			if (ret != 0 && errno != EEXIST) {
				LOG(("Failed to create directory \"%s\"",
						directory));
				free(directory);
				return false;
			}

			*start = '/';
		}
	}

	LOG(("Temporary directory location: %s", directory));
	ret = nsmkdir(directory, S_IRWXU);

	free(directory);

	if (ret != 0) {
		return false;
	}
	return true;
}


/**
 * Deletes all files in the cache directory that are not accounted for.
 */
void filename_flush(void)
{
	while (filename_flush_directory(TEMP_FILENAME_PREFIX, 0));
}


/**
 * Deletes some files in a directory that are not accounted for.
 *
 * A single call to this function may not delete all the files in
 * a directory. It should be called until it returns false.
 *
 * \param folder	the folder to search
 * \param depth		the folder depth
 * \returns whether further calls may be needed
 */
bool filename_flush_directory(const char *folder, int depth)
{
	DIR *parent;
	struct dirent *entry;
	bool changed = false;
	bool del;
	int number, i;
	int prefix = 0;
	unsigned int prefix_mask = (0x3f << 12);
	char child[256];
	const char *prefix_start = NULL;
	struct directory *dir = NULL;

	/* Maximum permissible depth is 3 */
	assert(depth <= 3);

	if (depth > 0) {
		/* Not a top-level directory, so determine the prefix
		 * by removing the last /XX component */
		prefix_start = folder + strlen(folder) - depth * 3 + 1;
	}

	/* Calculate the numeric prefix */
	for (i = 0; i < depth; i++) {
		number = prefix_start[1] + prefix_start[0] * 10 - START_PREFIX;
		prefix |= (number << (12 - i * 6));
		prefix_mask |= (0x3f << (12 - i * 6));
		prefix_start += 3;
	}

	/* If we're flushing a leaf directory, find it in the list */
	if (depth == 3) {
		for (dir = root; dir; dir = dir->next) {
			if (dir->numeric_prefix == prefix)
				break;
		}

		if (dir == NULL)
			return false;
	}

	parent = opendir(folder);

	while ((entry = readdir(parent))) {
	 	struct stat statbuf;

		/* Ignore '.' and '..' */
		if (strcmp(entry->d_name, ".") == 0 ||
				strcmp(entry->d_name, "..") == 0)
			continue;

		snprintf(child, sizeof(child), "%s/%s", folder, entry->d_name);
		child[sizeof(child) - 1] = '\0';

		if (stat(child, &statbuf) == -1) {
			LOG(("Unable to stat %s: %s", child, strerror(errno)));
			continue;
		}

		/* first 3 depths are directories only, then files only */
		if (depth < 3) {
			/* Delete any unexpected files */
			del = !S_ISDIR(statbuf.st_mode);
		} else {
			/* Delete any unexpected directories */
		  	del = S_ISDIR(statbuf.st_mode);
		}

		/* check we are a file numbered '00' -> '63' */
		if (del == false && (entry->d_name[0] >= '0') &&
				(entry->d_name[0] <= '6') &&
				(entry->d_name[1] >= '0') &&
				(entry->d_name[1] <= '9') &&
				(entry->d_name[2] == '\0')) {
			number = atoi(entry->d_name);

			if (number >= 0 && number <= 63) {
				if (depth == 3) {
					/* File: delete if not in bitfield */
					if (number < 32)
						del = !(dir->low_used &
							(1 << number));
					else
						del = !(dir->high_used &
							(1 << (number - 32)));
				} else {
					/* Directory: delete unless in list */
					del = true;

					/* Insert into numeric prefix */
					prefix &= ~(0x3f << (12 - depth * 6));
					prefix |= (number << (12 - depth * 6));

					/* Find in dir list */
					for (dir = root; dir; dir = dir->next) {
						number = dir->numeric_prefix &
								prefix_mask;
						if (number == prefix) {
							/* In list: retain */
							del = false;
							break;
						}
					}
				}
			} else {
				/* Unexpected name: delete */
				del = true;
			}
		} else {
			/* Unexpected name: delete */
			del = true;
		}

		/* continue if this is a file we want to retain */
		if (del == false && (!S_ISDIR(statbuf.st_mode)))
		    	continue;

		/* delete or recurse */
		if (del) {
			if (S_ISDIR(statbuf.st_mode))
				filename_delete_recursive(child);

			if (remove(child))
				LOG(("Failed to remove '%s'", child));
			else
				changed = true;
		} else {
			while (filename_flush_directory(child, depth + 1));
		}
	}

	closedir(parent);

	return changed;
}


/**
 * Recursively deletes the contents of a directory
 *
 * \param directory	the directory to delete
 * \return true on success, false otherwise
 */
bool filename_delete_recursive(char *folder)
{
	DIR *parent;
	struct dirent *entry;
	char child[256];
	struct stat statbuf;

	parent = opendir(folder);

	while ((entry = readdir(parent))) {
		/* Ignore '.' and '..' */
		if (strcmp(entry->d_name, ".") == 0 ||
				strcmp(entry->d_name, "..") == 0)
			continue;

		snprintf(child, sizeof(child), "%s/%s", folder, entry->d_name);
		child[sizeof(child) - 1] = '\0';

		if (stat(child, &statbuf) == -1) {
                        LOG(("Unable to stat %s: %s", child, strerror(errno)));
			continue;
		}

		if (S_ISDIR(statbuf.st_mode)) {
			if (!filename_delete_recursive(child)) {
				closedir(parent);
				return false;
			}
		}

		if (remove(child)) {
			LOG(("Failed to remove '%s'", child));
			closedir(parent);
			return false;
		}
	}

	closedir(parent);

	return true;
}


/**
 * Creates a new directory.
 *
 * \param  prefix  the prefix to use, or NULL to allocate a new one
 * \return a new directory structure, or NULL on memory exhaustion or
 * creation failure
 *
 * Empty directories are never deleted, except by an explicit call to
 * filename_flush().
 */
static struct directory *filename_create_directory(const char *prefix)
{
	char *last_1, *last_2;
	int index;
	struct directory *old_dir, *new_dir, *prev_dir = NULL;
	char dir_prefix[16];
	int i;

	/* get the lowest unique prefix, or use the provided one */
	if (prefix == NULL) {
		for (index = 0, old_dir = root; old_dir; 
				index++, old_dir = old_dir->next) {
			if (old_dir->numeric_prefix != index)
				break;

			prev_dir = old_dir;
		}

		sprintf(dir_prefix, "%.2i/%.2i/%.2i/",
				((index >> 12) & 63),
				((index >> 6) & 63),
				((index >> 0) & 63));

		prefix = dir_prefix;
	} else {
		/* prefix format is always '01/23/45/' */
		index = ((prefix[7] + prefix[6] * 10 - START_PREFIX) |
			((prefix[4] + prefix[3] * 10 - START_PREFIX) << 6) |
			((prefix[1] + prefix[0] * 10 - START_PREFIX) << 12));

		for (old_dir = root; old_dir; old_dir = old_dir->next) {
			if (old_dir->numeric_prefix == index)
				return old_dir;

			else if (old_dir->numeric_prefix > index)
				break;

			prev_dir = old_dir;
		}
	}

	/* allocate a new directory */
	new_dir = malloc(sizeof(struct directory));
	if (new_dir == NULL) {
		LOG(("No memory for malloc()"));
		return NULL;
	}

	strncpy(new_dir->prefix, prefix, 9);
	new_dir->prefix[9] = '\0';
	new_dir->low_used = new_dir->high_used = 0;
	new_dir->numeric_prefix = index;

	if (prev_dir == NULL) {
		new_dir->next = root;
		root = new_dir;
	} else {
		new_dir->next = prev_dir->next;
		prev_dir->next = new_dir;
	}

	/* if the previous directory has the same parent then we can simply
	 * create the child. */
	if (prev_dir && strncmp(prev_dir->prefix, new_dir->prefix, 6) == 0) {
		new_dir->prefix[8] = '\0';
		sprintf(filename_directory, "%s/%s",
				TEMP_FILENAME_PREFIX,
				new_dir->prefix);
		new_dir->prefix[8] = '/';

		if (!is_dir(filename_directory)) {
			if (!nsmkdir(filename_directory, S_IRWXU))
				return new_dir;

			/* the user has probably deleted the parent directory
			 * whilst we are running if there is an error, so we
			 * don't report this yet and try to create the
			 * structure normally. */
			LOG(("Failed to create optimised structure '%s'",
					filename_directory));
		}
	}

	/* create the directory structure */
	sprintf(filename_directory, "%s/", TEMP_FILENAME_PREFIX);
	last_1 = filename_directory + SLEN(TEMP_FILENAME_PREFIX) + 1;
	last_2 = new_dir->prefix;

	/* create each subdirectory, up to the maximum depth of 3 */
	for (i = 0; i < 3 && *last_2; i++) {
		*last_1++ = *last_2++;
		while (*last_2 && *last_2 != '/')
			*last_1++ = *last_2++;

		if (*last_2) {
			last_1[0] = '\0';

			if (!is_dir(filename_directory)) {
				if (nsmkdir(filename_directory, S_IRWXU)) {
					LOG(("Failed to create directory '%s'",
							filename_directory));
					return NULL;
				}
			}
		}
	}

	return new_dir;
}
