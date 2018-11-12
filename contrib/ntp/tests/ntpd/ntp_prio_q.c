#include "config.h"

#include "ntp.h"
#include "ntp_calendar.h"
#include "ntp_stdlib.h"

#include "ntp_prio_q.h"

#include "unity.h"



#include <string.h>
/*
TODO:
-fix the includes
-makefile: ntpdsim-ntp_prio_q.o - make sure it's okay
*/


/* helpers */

typedef struct Element
{
	char str[37]; // 37 seems like a nice candidate to break stuff
	int number;

} element;

int
compare_elements(const void * e1, const void * e2)
{
	return ((element*)e1)->number < ((element*)e2)->number;
}

/* tests */

void
test_AllocateDeallocateNode(void) {
	element* e_ptr = debug_get_node(sizeof(element));
	free_node(e_ptr);
}


void
test_EmptyQueue(void) {
	queue* q = create_queue();

	TEST_ASSERT_NOT_NULL(q);
	TEST_ASSERT_TRUE(empty(q));
	TEST_ASSERT_NULL(queue_head(q));
	TEST_ASSERT_NULL(dequeue(q));
	TEST_ASSERT_EQUAL(0, get_no_of_elements(q));

	destroy_queue(q);
}


void
test_OneElementQueue(void) {
	queue* q = create_queue();

	TEST_ASSERT_NOT_NULL(q);

	element e = {"string", 3};
	element* e_ptr = debug_get_node(sizeof(element));
	enqueue(q, e_ptr);
	*e_ptr = e;

	TEST_ASSERT_FALSE(empty(q));
	TEST_ASSERT_NOT_NULL(queue_head(q));
	TEST_ASSERT_EQUAL(1, get_no_of_elements(q));

	element* e_ptr_returned = dequeue(q);

	TEST_ASSERT_NOT_NULL(e_ptr_returned);
	TEST_ASSERT_EQUAL_STRING(e_ptr_returned->str, "string");
	TEST_ASSERT_EQUAL_PTR(e_ptr_returned, e_ptr);
	TEST_ASSERT_EQUAL(0, get_no_of_elements(q));
	TEST_ASSERT_TRUE(empty(q));
	TEST_ASSERT_NULL(dequeue(q));

	destroy_queue(q);
}


void
test_MultipleElementQueue(void) {
	queue* q = create_queue();

	TEST_ASSERT_NOT_NULL(q);

	element *e1_ptr, *e2_ptr, *e3_ptr;

	e1_ptr = (element*)debug_get_node(sizeof(element));
	e2_ptr = (element*)debug_get_node(sizeof(element));
	e3_ptr = (element*)debug_get_node(sizeof(element));

	enqueue(q, e1_ptr);
	enqueue(q, e2_ptr);
	enqueue(q, e3_ptr);

	TEST_ASSERT_EQUAL(3, get_no_of_elements(q));

	dequeue(q);
	enqueue(q, e1_ptr);

	TEST_ASSERT_EQUAL(3, get_no_of_elements(q));

	dequeue(q);
	dequeue(q);
	enqueue(q, e3_ptr);
	enqueue(q, e2_ptr);

	TEST_ASSERT_EQUAL_PTR(dequeue(q), e1_ptr);
	TEST_ASSERT_EQUAL_PTR(dequeue(q), e3_ptr);
	TEST_ASSERT_EQUAL_PTR(dequeue(q), e2_ptr);
	TEST_ASSERT_EQUAL(0, get_no_of_elements(q));
	TEST_ASSERT_NULL(dequeue(q));

	destroy_queue(q);
}


void
test_CustomOrderQueue(void) {
	queue* q = debug_create_priority_queue(compare_elements);
	element *e1_ptr, *e2_ptr, *e3_ptr, *e4_ptr, *e5_ptr, *e6_ptr;

	e1_ptr = (element*)debug_get_node(sizeof(element));
	e2_ptr = (element*)debug_get_node(sizeof(element));
	e3_ptr = (element*)debug_get_node(sizeof(element));
	e4_ptr = (element*)debug_get_node(sizeof(element));
	e5_ptr = (element*)debug_get_node(sizeof(element));
	e6_ptr = (element*)debug_get_node(sizeof(element));

	e1_ptr->number = 1;
	e2_ptr->number = 1;
	e3_ptr->number = 10;
	e4_ptr->number = 10;
	e5_ptr->number = 100;
	e6_ptr->number = 100;

	enqueue(q, e3_ptr);
	enqueue(q, e5_ptr);
	enqueue(q, e2_ptr);
	enqueue(q, e1_ptr);
	enqueue(q, e4_ptr);
	enqueue(q, e6_ptr);

	TEST_ASSERT_EQUAL(((element*)queue_head(q))->number, 100);
	TEST_ASSERT_EQUAL(((element*)dequeue(q))->number, 100);

	TEST_ASSERT_EQUAL(((element*)queue_head(q))->number, 100);
	TEST_ASSERT_EQUAL(((element*)dequeue(q))->number, 100);

	TEST_ASSERT_EQUAL(((element*)queue_head(q))->number, 10);
	TEST_ASSERT_EQUAL(((element*)dequeue(q))->number, 10);

	TEST_ASSERT_EQUAL(((element*)queue_head(q))->number, 10);
	TEST_ASSERT_EQUAL(((element*)dequeue(q))->number, 10);

	TEST_ASSERT_EQUAL(((element*)queue_head(q))->number, 1);
	TEST_ASSERT_EQUAL(((element*)dequeue(q))->number, 1);

	TEST_ASSERT_EQUAL(((element*)queue_head(q))->number, 1);
	TEST_ASSERT_EQUAL(((element*)dequeue(q))->number, 1);

	TEST_ASSERT_TRUE(empty(q));

	destroy_queue(q);

	free_node(e1_ptr);
	free_node(e2_ptr);
	free_node(e3_ptr);
	free_node(e4_ptr);
	free_node(e5_ptr);
	free_node(e6_ptr);
}


void
test_DestroyNonEmptyQueue(void) {
	queue* q = create_queue();
	element *e1_ptr, *e2_ptr, *e3_ptr, *e4_ptr, *e5_ptr, *e6_ptr;

	e1_ptr = (element*)debug_get_node(sizeof(element));
	e2_ptr = (element*)debug_get_node(sizeof(element));
	e3_ptr = (element*)debug_get_node(sizeof(element));
	e4_ptr = (element*)debug_get_node(sizeof(element));
	e5_ptr = (element*)debug_get_node(sizeof(element));
	e6_ptr = (element*)debug_get_node(sizeof(element));

	enqueue(q, e3_ptr);
	enqueue(q, e2_ptr);
	enqueue(q, e4_ptr);
	enqueue(q, e1_ptr);
	enqueue(q, e6_ptr);
	enqueue(q, e5_ptr);

	destroy_queue(q);
}

void
test_AppendQueues(void) {
	queue* q1 = create_queue();
	queue* q2 = create_queue();
	queue* q3 = create_queue();
	queue* q4 = create_queue();
	queue* q5 = create_queue();

	// append empty queue to empty queue
	append_queue(q1, q2);	// destroys q2

	element *e1_ptr, *e2_ptr, *e3_ptr, *e4_ptr, *e5_ptr, *e6_ptr;
	e1_ptr = (element*)debug_get_node(sizeof(element));
	e2_ptr = (element*)debug_get_node(sizeof(element));
	e3_ptr = (element*)debug_get_node(sizeof(element));
	e4_ptr = (element*)debug_get_node(sizeof(element));
	e5_ptr = (element*)debug_get_node(sizeof(element));
	e6_ptr = (element*)debug_get_node(sizeof(element));

	enqueue(q1, e1_ptr);
	enqueue(q1, e2_ptr);
	enqueue(q1, e3_ptr);


	// append empty queue to non empty queue
	append_queue(q1, q3);	// destroys q3
	TEST_ASSERT_EQUAL(3, get_no_of_elements(q1));

	// append non empty queue to empty queue
	append_queue(q4, q1);	// destroys q1
	TEST_ASSERT_EQUAL(3, get_no_of_elements(q4));

	enqueue(q5, e4_ptr);
	enqueue(q5, e5_ptr);

	// append non empty queue to non empty queue
	append_queue(q4, q5);	// destroys q5
	TEST_ASSERT_EQUAL(5, get_no_of_elements(q4));

	dequeue(q4);
	dequeue(q4);
	dequeue(q4);
	dequeue(q4);
	dequeue(q4);

	free_node(e1_ptr);
	free_node(e2_ptr);
	free_node(e3_ptr);
	free_node(e4_ptr);
	free_node(e5_ptr);
	free_node(e6_ptr);

	TEST_ASSERT_EQUAL(0, get_no_of_elements(q4));

	// destroy_queue(q1);	// destroyed already
	// destroy_queue(q2);	// destroyed already
	// destroy_queue(q3);	// destroyed already
	destroy_queue(q4);
	// destroy_queue(q5);	// destroyed already
}
