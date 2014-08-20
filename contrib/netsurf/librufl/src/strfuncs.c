#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "strfuncs.h"

char *strdup(const char *s)
{
	size_t len = strlen(s);
	char *new = malloc(len + 1);
	if (!new)
		return 0;
	memcpy(new, s, len);
	new[len] = '\0';
	return new;
}

char *strndup(const char *s, size_t n)
{
	size_t len = strlen(s);
	if (n < len)
		len = n;
	char *new = malloc(len + 1);
	if (!new)
		return 0;
	memcpy(new, s, len);
	new[len] = '\0';
	return new;
}

int strcasecmp(const char *s1, const char *s2)
{
	int i;
	while ((i = tolower(*s1)) && i == tolower(*s2))
		s1++, s2++;
	return ((unsigned char) tolower(*s1) - (unsigned char) tolower(*s2));
}
