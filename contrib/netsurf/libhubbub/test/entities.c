#include "tokeniser/entities.h"

#include "testutils.h"

int main(int argc, char **argv)
{
	uint32_t result;
	int32_t context = -1;

	UNUSED(argc);
	UNUSED(argv);

	assert(hubbub_entities_search_step('A', &result, &context) ==
			HUBBUB_NEEDDATA);

	assert(hubbub_entities_search_step('E', &result, &context) ==
			HUBBUB_NEEDDATA);

	assert(hubbub_entities_search_step('l', &result, &context) ==
			HUBBUB_NEEDDATA);

	assert(hubbub_entities_search_step('i', &result, &context) ==
			HUBBUB_NEEDDATA);

	assert(hubbub_entities_search_step('g', &result, &context) ==
			HUBBUB_OK);

	assert(hubbub_entities_search_step(';', &result, &context) ==
			HUBBUB_OK);

	assert(hubbub_entities_search_step('z', &result, &context) ==
			HUBBUB_INVALID);

	printf("PASS\n");

	return 0;
}
