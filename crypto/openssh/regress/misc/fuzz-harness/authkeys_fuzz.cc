#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>
#include <unistd.h>

extern "C" {

#include "hostfile.h"
#include "auth.h"
#include "auth-options.h"
#include "sshkey.h"

// testdata/id_ed25519.pub and testdata/id_ed25519-cert.pub
const char *pubkey = "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIDPQXmEVMVLmeFRyafKMVWgPDkv8/uRBTwmcEDatZzMD"; 
const char *certtext = "ssh-ed25519-cert-v01@openssh.com AAAAIHNzaC1lZDI1NTE5LWNlcnQtdjAxQG9wZW5zc2guY29tAAAAIMDQjYH6XRzH3j3MW1DdjCoAfvrHfgjnVGF+sLK0pBfqAAAAIDPQXmEVMVLmeFRyafKMVWgPDkv8/uRBTwmcEDatZzMDAAAAAAAAA+sAAAABAAAAB3VseXNzZXMAAAAXAAAAB3VseXNzZXMAAAAIb2R5c3NldXMAAAAAAAAAAP//////////AAAAAAAAAIIAAAAVcGVybWl0LVgxMS1mb3J3YXJkaW5nAAAAAAAAABdwZXJtaXQtYWdlbnQtZm9yd2FyZGluZwAAAAAAAAAWcGVybWl0LXBvcnQtZm9yd2FyZGluZwAAAAAAAAAKcGVybWl0LXB0eQAAAAAAAAAOcGVybWl0LXVzZXItcmMAAAAAAAAAAAAAADMAAAALc3NoLWVkMjU1MTkAAAAgM9BeYRUxUuZ4VHJp8oxVaA8OS/z+5EFPCZwQNq1nMwMAAABTAAAAC3NzaC1lZDI1NTE5AAAAQBj0og+s09/HpwdHZbzN0twooKPDWWrxGfnP1Joy6cDnY2BCSQ7zg9vbq11kLF8H/sKOTZWAQrUZ7LlChOu9Ogw= id_ed25519.pub";

// stubs
void auth_debug_add(const char *fmt,...)
{
}

void
auth_log_authopts(const char *loc, const struct sshauthopt *opts, int do_remote)
{
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	char *tmp, *o, *cp = (char *)malloc(size + 1 + strlen(pubkey) + 1);
	struct sshauthopt *opts = NULL;
	struct passwd *pw = getpwuid(getuid());
	static struct sshkey *key, *cert;

	if (key == NULL) {
		if ((key = sshkey_new(KEY_UNSPEC)) == NULL ||
		    (cert = sshkey_new(KEY_UNSPEC)) == NULL)
			abort();
		if ((o = tmp = strdup(pubkey)) == NULL ||
		    sshkey_read(key, &tmp) != 0)
			abort();
		free(o);
		if ((o = tmp = strdup(certtext)) == NULL ||
		    sshkey_read(cert, &tmp) != 0)
			abort();
		free(o);
	}
	if (cp == NULL || pw == NULL || key == NULL || cert == NULL)
		abort();

	// Cleanup whitespace at input EOL.
	for (; size > 0 && strchr(" \t\r\n", data[size - 1]) != NULL; size--) ;

	// Append a pubkey that will match.
	memcpy(cp, data, size);
	cp[size] = ' ';
	memcpy(cp + size + 1, pubkey, strlen(pubkey) + 1);

	// Try key.
	if ((tmp = strdup(cp)) == NULL)
		abort();
	(void) auth_check_authkey_line(pw, key, tmp, "127.0.0.1", "localhost",
	    "fuzz", &opts);
	free(tmp);
	sshauthopt_free(opts);

	// Try cert.
	if ((tmp = strdup(cp)) == NULL)
		abort();
	(void) auth_check_authkey_line(pw, cert, tmp, "127.0.0.1", "localhost",
	    "fuzz", &opts);
	free(tmp);
	sshauthopt_free(opts);

	free(cp);
	return 0;
}

} // extern "C"
