/*
 * This DES validation program shipped with FreeSec is derived from that
 * shipped with UFC-crypt which is apparently derived from one distributed
 * with Phil Karns PD DES package.
 *
 * $FreeBSD$
 */

#include <stdio.h>

int totfails = 0;

char *crypt();
#ifdef HAVE_CRYPT16
char *crypt16();
#endif /* HAVE_CRYPT16 */


static struct crypt_test {
	char	*key, *setting, *answer;
} crypt_tests[] = {
	"foob",			"ar",		"arlEKn0OzVJn.",
	"holyhooplasbatman!",	"_X.......",	"_X.......N89y2Z.e4WU",
	"holyhooplasbatman!",	"_X...X...",	"_X...X...rSUDQ5Na/QM",
	"holyhooplasbatman!",	"_XX..X...",	"_XX..X...P8vb9xU4JAk",
	"holyhooplasbatman!",	"_XX..XX..",	"_XX..XX..JDs5IlGLqT2",
	"holyhooplasbatman!",	"_XX..XXa.",	"_XX..XXa.bFVsOnCNh8Y",
	"holyhooplasbatman!",	"_XXa.X...",	"_XXa.X...Ghsb3QKNaps",
#ifdef TAKES_TOO_LONG_ON_SOME_CRYPTS
	"holyhooplasbatman!",	"_arararar",	"_ararararNGMzvpNjeCc",
#endif
	NULL, NULL, NULL,
};


static struct crypt_test crypt16_tests[] = {
	"foob",			"ar",		"arxo23jZDD5AYbHbqoy9Dalg",
	"holyhooplasbatman!",	"ar",		"arU5FRLJ3kxIoedlmyrOelEw",
	NULL, NULL, NULL
};


void good_bye()
{
  if(totfails == 0) {
    printf(" Passed validation\n");
    exit(0);
  } else {
    printf(" %d failures during validation!!!\n", totfails);
    exit(1);
  }
}


void put8(cp)
char *cp;
{
	int i,j,t;

	for(i = 0; i < 8; i++){
		t = 0;
		for(j = 0; j < 8; j++)
			t = t << 1 | *cp++;
		printf("%02x", t);
	}
}


void print_bits(bits)
unsigned char *bits;
{
	int	i;

	for (i = 0; i < 8; i++) {
		printf("%02x", bits[i]);
	}
}


int parse_line(buff, salt, key, plain, answer)
char *buff;
long *salt;
char *key, *plain, *answer;
{
	char *ptr1, *ptr2;
	int val;
	int i,j,t;

	/*
	 * Extract salt
	 */
	if (sscanf(buff, "%lu", salt) != 1)
		return(-1);
	for (ptr2 = buff; *ptr2 && !isspace(*ptr2); ptr2++)
		;

	/*
	 * Extract key
	 */
	for (ptr1 = ptr2; *ptr1 && isspace(*ptr1); ptr1++)
		;
	for (ptr2 = ptr1; *ptr2 && !isspace(*ptr2); ptr2++)
		;
	if (ptr2 - ptr1 != 16)
		return(-1);
	for (i = 0; i < 8; i++){
		if (sscanf(ptr1 + 2*i, "%2x", &t) != 1)
			return(-2);
		for (j = 0; j < 8; j++)
			*key++ = (t & 1 << (7 - j)) != 0;
	}

	/*
	 * Extract plain
	 */
	for (ptr1 = ptr2; *ptr1 && isspace(*ptr1); ptr1++)
		;
	for (ptr2 = ptr1; *ptr2 && !isspace(*ptr2); ptr2++)
		;
	if (ptr2 - ptr1 != 16)
		return(-1);
	for (i = 0; i < 8; i++){
		if (sscanf(ptr1 + 2*i, "%2x", &t) != 1)
			return(-2);
		for (j = 0; j < 8; j++)
			*plain++ = (t & 1 << (7 - j)) != 0;
	}

	/*
	 * Extract answer
	 */
	for (ptr1 = ptr2; *ptr1 && isspace(*ptr1); ptr1++)
		;
	for (ptr2 = ptr1; *ptr2 && !isspace(*ptr2); ptr2++)
		;
	if (ptr2 - ptr1 != 16)
		return(-1);
	for (i = 0; i < 8; i++){
		if (sscanf(ptr1 + 2*i, "%2x", &t) != 1)
			return(-2);
		for (j = 0; j < 8; j++)
			*answer++ = (t & 1 << (7 - j)) != 0;
	}
	return(0);
}

void bytes_to_bits(bytes, bits)
char *bytes;
unsigned char *bits;
{
	int	i, j;

	for (i = 0; i < 8; i++) {
		bits[i] = 0;
		for (j = 0; j < 8; j++) {
			bits[i] |= (bytes[i*8+j] & 1) << (7 - j);
		}
	}
}



/*
 *	Test the old-style crypt(), the new-style crypt(), and crypt16().
 */
void test_crypt()
{
	char	*result;
	struct crypt_test	*p;

	printf("Testing crypt() family\n");

	for (p = crypt_tests; p->key; p++) {
		printf(" crypt(\"%s\", \"%s\"), \"%s\" expected",
			p->key, p->setting, p->answer);
		fflush(stdout);
		result = crypt(p->key, p->setting);
		if(!strcmp(result, p->answer)) {
			printf(", OK\n");
		} else {
			printf("\n  failed (\"%s\")\n", result);
			totfails++;
		}
	}

#ifdef HAVE_CRYPT16
	for (p = crypt16_tests; p->key; p++) {
		printf(" crypt16(\"%s\", \"%s\"), \"%s\" expected",
			p->key, p->setting, p->answer);
		fflush(stdout);
		result = crypt16(p->key, p->setting);
		if(!strcmp(result, p->answer)) {
			printf(", OK\n");
		} else {
			printf("\n  failed (\"%s\")\n", result);
			totfails++;
		}
	}
#endif /* HAVE_CRYPT16 */
}

main(argc, argv)
int argc;
char *argv[];
{
	test_crypt();
	good_bye();
}
