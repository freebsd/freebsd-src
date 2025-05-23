#include <sys/types.h>
#include <sys/exterrvar.h>
#include <exterr.h>
#include <string.h>

static struct uexterror uexterr = {
	.ver = UEXTERROR_VER,
};

static void uexterr_ctr(void) __attribute__((constructor));
static void
uexterr_ctr(void)
{
	exterrctl(EXTERRCTL_ENABLE, 0, &uexterr);
}
