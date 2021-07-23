/* $FreeBSD$ */

bool destructed;
bool destructed2;

class Test {
public:
	Test() { std::printf("Test::Test()\n"); }
	~Test() { std::printf("Test::~Test()\n"); destructed = true; }
};

void
cleanup_handler(void *arg)
{
	destructed2 = true;
	std::printf("%s()\n", __func__);
}

void
check_destruct(void)
{
	if (destructed)
		std::printf("OK\n");
	else
		std::printf("Bug, object destructor is not called\n");
}

void
check_destruct2(void)
{
	if (!destructed)
		std::printf("Bug, object destructor is not called\n");
	else if (!destructed2)
		std::printf("Bug, cleanup handler is not called\n");
	else
		std::printf("OK\n");
}
