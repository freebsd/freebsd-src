/* Public domain. */

#ifndef _LINUXKPI_LINUX_RATELIMIT_H
#define _LINUXKPI_LINUX_RATELIMIT_H

struct ratelimit_state {
};

#define DEFINE_RATELIMIT_STATE(name, interval, burst) \
	int name __used = 1;

#define __ratelimit(x)	(1)

#define ratelimit_state_init(x, y, z)
#define ratelimit_set_flags(x, y)

#define	WARN_RATELIMIT(condition, ...) ({		\
	bool __ret_warn_on = (condition);		\
	if (unlikely(__ret_warn_on))			\
		pr_warn_ratelimited(__VA_ARGS__);	\
	unlikely(__ret_warn_on);			\
})

#endif
