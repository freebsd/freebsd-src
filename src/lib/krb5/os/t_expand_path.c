#include "k5-int.h"
#include "os-proto.h"

int
main(int argc, char **argv)
{
    char *path;

    if (k5_expand_path_tokens_extra(NULL, argv[1], &path, "animal", "frog",
				    "place", "pad", "s", "s", NULL) != 0)
	return 2;
    if (argc == 2)
	printf("%s\n", path);
    else if (strcmp(path, argv[2]) != 0)
	return 1;
    free(path);
    return 0;
}
