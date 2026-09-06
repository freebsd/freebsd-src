#include <mqueue.h>
#include "syscall.h"

int mq_setattr(mqd_t mqd, const struct mq_attr *restrict new, struct mq_attr *restrict old)
{
	long long attr[8];
	if (new) for (int i=0; i<8; i++)
		attr[i] = *(long *)((char *)new + i*sizeof(long));
	int ret = __syscall(SYS_mq_getsetattr, mqd, new?attr:0, old?attr:0);
	if (ret < 0) return __syscall_ret(ret);
	if (old) for (int i=0; i<8; i++)
		*(long *)((char *)old + i*sizeof(long)) = attr[i];
	return 0;
}
