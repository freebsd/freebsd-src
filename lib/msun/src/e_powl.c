#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "math.h"
#include "math_private.h"

long double
powl(long double x, long double y)
{

	return (pow(x, y));
}
