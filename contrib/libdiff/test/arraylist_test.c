#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arraylist.h>

void test_basic(void)
{
	int *p;
	ARRAYLIST(int) list;
	ARRAYLIST_INIT(list, 2);

#define dump() do {\
		printf("(%d items)\n", list.len); \
		ARRAYLIST_FOREACH(p, list) \
			printf("[%lu] %d\n", \
			(unsigned long)ARRAYLIST_IDX(p, list), *p); \
		printf("\n"); \
	} while(0)

	dump();

	ARRAYLIST_ADD(p, list);
	*p = 100;
	dump();

	ARRAYLIST_ADD(p, list);
	*p = 101;
	dump();

	ARRAYLIST_ADD(p, list);
	*p = 102;
	dump();

#define insert_test(AT) do {\
		printf("insert at [" #AT "]:\n"); \
		ARRAYLIST_INSERT(p, list, AT); \
		*p = AT; \
		dump(); \
	} while(0)

	insert_test(list.len - 1);
	insert_test(1);
	insert_test(0);
	insert_test(6);
	insert_test(123);
	insert_test(-42);

	printf("clear:\n");
	ARRAYLIST_CLEAR(list);
	dump();

	ARRAYLIST_FREE(list);
}

int main(void)
{
	test_basic();
}
