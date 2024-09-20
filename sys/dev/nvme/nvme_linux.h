/*-
 * Copyright (c) 2024, Netflix Inc.
 * Written by Warner Losh
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Linux compatible NVME ioctls. So far we just support ID, ADMIN_CMD and
 * IO_CMD. The rest are not supported.
 */


#include <sys/ioccom.h>
#include <sys/_types.h>

struct nvme_passthru_cmd {
	__uint8_t	opcode;
	__uint8_t	flags;
	__uint16_t	rsvd1;
	__uint32_t	nsid;
	__uint32_t	cdw2;
	__uint32_t	cdw3;
	__uint64_t	metadata;
	__uint64_t	addr;
	__uint32_t	metadata_len;
	__uint32_t	data_len;
	__uint32_t	cdw10;
	__uint32_t	cdw11;
	__uint32_t	cdw12;
	__uint32_t	cdw13;
	__uint32_t	cdw14;
	__uint32_t	cdw15;
	__uint32_t	timeout_ms;
	__uint32_t	result;
};

#define nvme_admin_cmd nvme_passthru_cmd

/*
 * Linux nvme ioctls, commented out ones are not supported
 */
#define NVME_IOCTL_ID		_IO('N', 0x40)
#define NVME_IOCTL_ADMIN_CMD	_IOWR('N', 0x41, struct nvme_admin_cmd)
/* #define NVME_IOCTL_SUBMIT_IO	_IOW('N', 0x42, struct nvme_user_io) */
#define NVME_IOCTL_IO_CMD	_IOWR('N', 0x43, struct nvme_passthru_cmd)
#define NVME_IOCTL_RESET	_IO('N', 0x44)
/* #define NVME_IOCTL_SUBSYS_RESET	_IO('N', 0x45) */
/* #define NVME_IOCTL_RESCAN	_IO('N', 0x46) */
/* #define NVME_IOCTL_ADMIN64_CMD	_IOWR('N', 0x47, struct nvme_passthru_cmd64) */
/* #define NVME_IOCTL_IO64_CMD	_IOWR('N', 0x48, struct nvme_passthru_cmd64) */
/* #define NVME_IOCTL_IO64_CMD_VEC	_IOWR('N', 0x49, struct nvme_passthru_cmd64) */

/* io_uring async commands: */
/* #define NVME_URING_CMD_IO	_IOWR('N', 0x80, struct nvme_uring_cmd) */
/* #define NVME_URING_CMD_IO_VEC	_IOWR('N', 0x81, struct nvme_uring_cmd) */
/* #define NVME_URING_CMD_ADMIN	_IOWR('N', 0x82, struct nvme_uring_cmd) */
/* #define NVME_URING_CMD_ADMIN_VEC _IOWR('N', 0x83, struct nvme_uring_cmd) */
