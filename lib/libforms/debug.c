#ifdef DEBUG
#include <forms.h>

int
debug_dump_bindings(char *key, void *data, void *arg)
{
		TUPLE *tuple = (TUPLE *)data;

		printf("%s, %d, %x\n", tuple->name, tuple->type, (int)tuple->addr);

		return (1);
}

void
debug_dump_table(hash_table *htable)
{
	printf("Dumping table at address %x\n", htable);
	hash_traverse(htable, debug_dump_bindings, NULL);
	printf("------------------------------\n");
}
#endif
