/*
 * Doubly-linked list - test program
 * Copyright (c) 2009, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"
#include "utils/os.h"
#include "utils/list.h"

struct test {
	struct dl_list list;
	int value;
};

static void dump_list(struct dl_list *head)
{
	struct test *t;
	printf("dump:");
	dl_list_for_each(t, head, struct test, list)
		printf(" %d", t->value);
	printf(" (len=%d%s)\n", dl_list_len(head),
	       dl_list_empty(head) ? " empty" : "");
}

int main(int argc, char *argv[])
{
	struct dl_list head;
	struct test *t, *tmp;
	int i;

	dl_list_init(&head);
	dump_list(&head);

	for (i = 0; i < 5; i++) {
		t = os_zalloc(sizeof(*t));
		if (t == NULL)
			return -1;
		t->value = i;
		dl_list_add(&head, &t->list);
		dump_list(&head);
	}

	for (i = 10; i > 5; i--) {
		t = os_zalloc(sizeof(*t));
		if (t == NULL)
			return -1;
		t->value = i;
		dl_list_add_tail(&head, &t->list);
		dump_list(&head);
	}

	i = 0;
	dl_list_for_each(t, &head, struct test, list)
		if (++i == 5)
			break;
	printf("move: %d\n", t->value);
	dl_list_del(&t->list);
	dl_list_add(&head, &t->list);
	dump_list(&head);

	dl_list_for_each_safe(t, tmp, &head, struct test, list) {
		printf("delete: %d\n", t->value);
		dl_list_del(&t->list);
		os_free(t);
		dump_list(&head);
	}

	return 0;
}
