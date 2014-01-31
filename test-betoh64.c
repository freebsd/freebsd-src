#include <sys/types.h>

#if defined(__linux__)
# include <endian.h>
#elif defined(__APPLE__)
# include <libkern/OSByteOrder.h>
#else
# include <sys/endian.h>
#endif

int
main(int argc, char **argv)
{
	u_int64_t hostorder;
	u_int64_t bigendian = 1;
	hostorder = betoh64(bigendian);
	return 0;
}
