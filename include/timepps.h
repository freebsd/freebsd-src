/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id$
 *
 * The is a FreeBSD protype version of the "draft-mogul-pps-api-02.txt" 
 * specification for Pulse Per Second timing interfaces.
 *
 */

#ifndef _TIME_PPS_H_
#define	_TIME_PPS_H_

#include <sys/timepps.h>

int time_pps_create(int filedes, pps_handle_t *handle);
int time_pps_destroy(pps_handle_t handle);
int time_pps_setparams(pps_handle_t handle, const pps_params_t *ppsparams);
int time_pps_getparams(pps_handle_t handle, pps_params_t *ppsparams);
int time_pps_getcap(pps_handle_t handle, int *mode);
int time_pps_fetch(pps_handle_t handle, pps_info_t *ppsinfobuf);
int time_pps_wait(pps_handle_t handle, const struct timespec *timeout, 
	pps_info_t *ppsinfobuf);

__inline int
time_pps_create(int filedes, pps_handle_t *handle)
{
	int error;

	*handle = -1;
	error = ioctl(filedes, PPS_IOC_CREATE, 0);
	if (error < 0) 
		return (-1);
	*handle = filedes;
	return (0);
}

__inline int
time_pps_destroy(pps_handle_t handle)
{
	return (ioctl(handle, PPS_IOC_DESTROY, 0));
}

__inline int
time_pps_setparams(pps_handle_t handle, const pps_params_t *ppsparams)
{
	return (ioctl(handle, PPS_IOC_SETPARAMS, ppsparams));
}

__inline int
time_pps_getparams(pps_handle_t handle, pps_params_t *ppsparams)
{
	return (ioctl(handle, PPS_IOC_GETPARAMS, ppsparams));
}

__inline int 
time_pps_getcap(pps_handle_t handle, int *mode)
{
	return (ioctl(handle, PPS_IOC_GETCAP, mode));
}

__inline int
time_pps_fetch(pps_handle_t handle, pps_info_t *ppsinfobuf)
{
	return (ioctl(handle, PPS_IOC_FETCH, ppsinfobuf));
}

__inline int
time_pps_wait(pps_handle_t handle, const struct timespec *timeout,
        pps_info_t *ppsinfobuf)
{
	int error;
	struct pps_wait_args arg;

	arg.timeout = *timeout;
	error = ioctl(handle, PPS_IOC_WAIT, &arg);
	*ppsinfobuf = arg.pps_info_buf;
	return (error);
}

#endif /* !_TIME_PPS_H_ */
