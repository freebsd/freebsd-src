#include <sys/param.h>
#include <sys/pcpu.h>
#include <support/debug.h>
#include <support/mrlock.h>

int
ismrlocked(mrlock_t *mrp, int type)
{

	sx_assert(mrp, SX_LOCKED);
	if (type == MR_UPDATE)
		return sx_xlocked(mrp);
	return 1;
}
