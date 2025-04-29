/*-
 * Copyright (c) 2022 Vladimir Kondratyev <wulf@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _LINUXKPI_LINUX_PWM_H_
#define	_LINUXKPI_LINUX_PWM_H_

#include <linux/device.h>
#include <linux/err.h>

struct pwm_state {
	uint64_t period;
	bool enabled;
};

struct pwm_device {
	struct pwm_state state;
};

static inline struct pwm_device *
pwm_get(struct device *dev, const char *consumer)
{
	return (ERR_PTR(-ENODEV));
}

static inline void
pwm_put(struct pwm_device *pwm)
{
}

static inline int
pwm_enable(struct pwm_device *pwm)
{
	return (-EINVAL);
}

static inline void
pwm_disable(struct pwm_device *pwm)
{
}

static inline bool
pwm_is_enabled(const struct pwm_device *pwm)
{
	return (false);
}

static inline unsigned int
pwm_get_relative_duty_cycle(const struct pwm_state *state, unsigned int scale)
{
	return (0);
}

static inline int
pwm_set_relative_duty_cycle(struct pwm_state *state, unsigned int duty_cycle,
    unsigned int scale)
{
	return (0);
}

static inline void
pwm_get_state(const struct pwm_device *pwm, struct pwm_state *state)
{
	*state = pwm->state;
}

static inline int
pwm_apply_state(struct pwm_device *pwm, const struct pwm_state *state)
{
	return (-ENOTSUPP);
}

static inline int
pwm_apply_might_sleep(struct pwm_device *pwm, const struct pwm_state *state)
{
	return (0);
}

#endif	/* _LINUXKPI_LINUX_PWM_H_ */
