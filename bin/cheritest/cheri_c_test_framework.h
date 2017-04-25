#ifndef __CHERI_C_TEST_FRAMEWORK_H__
#define	__CHERI_C_TEST_FRAMEWORK_H__
#include <sys/types.h>
#include <stdlib.h>
#include <cheritest.h>

#undef assert
#define	assert(x)							\
do {									\
	if (!(x)) 							\
		cheritest_failure_errx("%s is false: %s:%d", #x, __FILE__, __LINE__);	\
} while(0)

void test_setup(void);

#define	DECLARE_TEST(name, desc) \
    void cheri_c_test_ ## name(const struct cheri_test *ctp __unused);
#define DECLARE_TEST_FAULT(name, desc)	/* Not supported */
#define BEGIN_TEST(name) \
    void cheri_c_test_ ## name(const struct cheri_test *ctp __unused) {	\
	test_setup();
#define END_TEST cheritest_success(); }

#endif /* __CHERI_C_TEST_FRAMEWORK_H__ */
