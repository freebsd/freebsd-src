#ifndef HOSTFILE_H
#define HOSTFILE_H

/*
 * Checks whether the given host is already in the list of our known hosts.
 * Returns HOST_OK if the host is known and has the specified key, HOST_NEW
 * if the host is not known, and HOST_CHANGED if the host is known but used
 * to have a different host key.  The host must be in all lowercase.
 */
typedef enum {
	HOST_OK, HOST_NEW, HOST_CHANGED
}       HostStatus;
HostStatus 
check_host_in_hostfile(const char *filename, const char *host, Key *key, Key *found);

/*
 * Appends an entry to the host file.  Returns false if the entry could not
 * be appended.
 */
int	add_host_to_hostfile(const char *filename, const char *host, Key *key);

#endif
