#include <stdio.h>
#include <string.h>

#include "charset/aliases.h"

#include "testutils.h"

int main (int argc, char **argv)
{
	parserutils_charset_aliases_canon *c;

	UNUSED(argc);
	UNUSED(argv);

	c = parserutils__charset_alias_canonicalise("moose", 5);
	if (c) {
		printf("FAIL - found invalid encoding 'moose'\n");
		return 1;
	}

	c = parserutils__charset_alias_canonicalise("csinvariant", 11);
	if (c) {
		printf("%s %d\n", c->name, c->mib_enum);
	} else {
		printf("FAIL - failed finding encoding 'csinvariant'\n");
		return 1;
	}

	c = parserutils__charset_alias_canonicalise("csinvariant\"", 12);
	if (c) {
		printf("%s %d\n", c->name, c->mib_enum);
	} else {
		printf("FAIL - failed finding encoding 'csinvariant'\n");
		return 1;
	}

	c = parserutils__charset_alias_canonicalise("nats-sefi-add", 13);
	if (c) {
		printf("%s %d\n", c->name, c->mib_enum);
	} else {
		printf("FAIL - failed finding encoding 'nats-sefi-add'\n");
		return 1;
	}

	printf("%d\n", parserutils_charset_mibenum_from_name(c->name,
			strlen(c->name)));

	printf("%s\n", parserutils_charset_mibenum_to_name(c->mib_enum));


	c = parserutils__charset_alias_canonicalise("u.t.f.8", 7);
	if (c) {
		printf("%s %d\n", c->name, c->mib_enum);
	} else {
		printf("FAIL - failed finding encoding 'u.t.f.8'\n");
		return 1;
	}

	printf("PASS\n");

	return 0;
}
