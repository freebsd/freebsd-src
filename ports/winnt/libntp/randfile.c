/*
 * Make sure that there is a good source of random characters
 * so that OpenSSL can work properly and securely.
 */

#include <config.h>
#include <wincrypt.h>

#include <stdio.h>

unsigned int	getrandom_chars(int desired, unsigned char *buf, int lenbuf);
BOOL		create_random_file(char *filename);

BOOL
init_randfile()
{
	FILE *rf;
	char *randfile;
	char *homedir;
	char tmp[256];
	/* See if the environmental variable RANDFILE is defined
	 * and the file exists
	 */
	randfile = getenv("RANDFILE");
	if (randfile != NULL) {
		rf = fopen(randfile, "rb");
		if (rf != NULL) {
			fclose(rf);
			return (TRUE);
		}
		else {
			/* The environmental variable exists but not the file */
			return (create_random_file(randfile));
		}
	}
	/*
	 * If the RANDFILE environmental variable does not exist,
	 * see if the HOME enviromental variable exists and
	 * a .rnd file is in there.
	 */
	homedir = getenv("HOME");
	if (homedir != NULL &&
	    (strlen(homedir) + 5 /* \.rnd */) < sizeof(tmp)) {
		snprintf(tmp, sizeof(tmp), "%s\\.rnd", homedir);
		rf = fopen(tmp, "rb");
		if (rf != NULL) {
			fclose(rf);
			return (TRUE);
		}
		else {
			/* The HOME environmental variable exists but not the file */
			return (create_random_file(tmp));
		}
	}
	/*
	 * Final try. Look for it on the C:\ directory
	 * NOTE: This is a really bad place for it security-wise
	 * However, OpenSSL looks for it there if it can't find it elsewhere
	 */
	rf = fopen("C:\\.rnd", "rb");
	if (rf != NULL) {
		fclose(rf);
		return (TRUE);
	}
	/* The file does not exist */
	return (create_random_file("C:\\.rnd"));
}
/*
 * Routine to create the random file with 1024 random characters
 */
BOOL
create_random_file(char *filename) {
	FILE *rf;
	int nchars;
	unsigned char buf[1025];

	nchars = getrandom_chars(1024, buf, sizeof(buf));
	rf = fopen(filename, "wb");
	if (rf == NULL)
		return (FALSE);
	fwrite(buf, sizeof(unsigned char), nchars, rf);
	fclose(rf);
	return (TRUE);
}

unsigned int
getrandom_chars(int desired, unsigned char *buf, int lenbuf) {
	HCRYPTPROV hcryptprov;
	BOOL err;

	if (buf == NULL || lenbuf <= 0 || desired > lenbuf)
		return (0);
	/*
	 * The first time we just try to acquire the context
	 */
	err = CryptAcquireContext(&hcryptprov, NULL, NULL, PROV_RSA_FULL,
				  CRYPT_VERIFYCONTEXT);
	if (!err){
		return (0);
	}
	if (!CryptGenRandom(hcryptprov, desired, buf)) {
		CryptReleaseContext(hcryptprov, 0);
		return (0);
	}

	CryptReleaseContext(hcryptprov, 0);
	return (desired);
}

