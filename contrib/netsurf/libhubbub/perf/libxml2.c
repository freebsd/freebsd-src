#include <stdio.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/mman.h>

#include <libxml/HTMLparser.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

int main(int argc, char **argv)
{
	htmlDocPtr doc;

	struct stat info;
	int fd;
	char *file;

	if (argc != 2) {
		printf("Usage: %s <file>\n", argv[0]);
		return 1;
	}

	/* libxml hack */
	LIBXML_TEST_VERSION


	stat(argv[1], &info);
	fd = open(argv[1], 0);
	file = mmap(NULL, info.st_size, PROT_READ, MAP_SHARED, fd, 0);

	doc = htmlReadMemory(file, info.st_size, NULL, NULL,
			HTML_PARSE_RECOVER | HTML_PARSE_NOERROR |
			HTML_PARSE_NOWARNING);
#if 0
	doc = htmlReadFile(argv[1], NULL, HTML_PARSE_RECOVER |
			HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
#endif
	if (!doc) {
		printf("FAIL\n");
		return 1;
	}

	xmlFreeDoc(doc);

	xmlCleanupParser();

	return 0;
}

