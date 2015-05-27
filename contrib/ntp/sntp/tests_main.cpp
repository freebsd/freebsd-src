#include "tests_main.h"

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);

	init_lib();
	init_auth();

	// Some tests makes use of extra parameters passed to the tests
	// executable. Save these params as static members of the base class.
	if (argc > 1) {
		ntptest::SetExtraParams(1, argc-1, argv);
	}
	
	return RUN_ALL_TESTS();
}

std::vector<std::string> ntptest::m_params;

void ntptest::SetExtraParams(int start, int count, char** argv)
{
	for (int i=0; i<count; i++) {
		m_params.push_back(argv[i+start]);
	}
}
