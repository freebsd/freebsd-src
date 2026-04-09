/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 UOS Project Contributors
 *
 * Bionic ABI Compatibility Header for UOS
 * ========================================
 *
 * UOS uses FreeBSD's kernel but needs to support running Android native
 * binaries (NDK-compiled shared libraries and executables) that link
 * against Bionic (Android's libc). This header provides:
 *
 *   1. Type aliases that make Bionic-compiled binaries compatible with
 *      FreeBSD's kernel ABI through the Linux emulation layer
 *      (COMPAT_LINUX64 + COMPAT_LINUXKPI).
 *
 *   2. Syscall number remapping hints for Android-specific syscalls
 *      not covered by standard Linux emulation.
 *
 *   3. Android Binder IPC stub declarations (required for Android
 *      service managers to communicate with UOS native services).
 *
 * DESIGN:
 *   Android binaries run under FreeBSD's Linux binary compatibility
 *   (kern/linux_*.c). This shim extends that layer with Android-specific
 *   errno codes, ioctl numbers, and memory layout expectations.
 *
 * NOTE: Full Bionic support requires userspace components:
 *   - /system/lib64/ with Bionic libc.so, libm.so, libdl.so
 *   - /system/bin/linker64 (Android dynamic linker)
 *   - UOS Android Runtime (UAR) service in userspace
 *
 * See: mobile/bionic/ for the userspace component stubs.
 */

#ifndef _BIONIC_COMPAT_H_
#define _BIONIC_COMPAT_H_

#ifdef _KERNEL

#include <sys/types.h>
#include <sys/syscall.h>
#include <compat/linux/linux.h>
#include <compat/linux/linux_syscall.h>

/*
 * Android-specific syscall numbers (Linux ARM64 ABI).
 * These map to the FreeBSD Linux emulation layer's syscall table.
 * Android uses the standard Linux ARM64 syscall ABI (aarch64).
 */

/* Android Binder IPC driver ioctl magic */
#define ANDROID_BINDER_IOC_MAGIC		'b'
#define ANDROID_BINDER_WRITE_READ		_IOWR('b', 1, struct android_binder_write_read)
#define ANDROID_BINDER_SET_MAX_THREADS		_IOW('b', 5, __u32)
#define ANDROID_BINDER_SET_CONTEXT_MGR		_IOW('b', 7, __s32)
#define ANDROID_BINDER_THREAD_EXIT		_IOW('b', 8, __s32)
#define ANDROID_BINDER_VERSION			_IOWR('b', 9, struct android_binder_version)
#define ANDROID_BINDER_GET_NODE_DEBUG_INFO	_IOWR('b', 11, struct android_binder_node_debug_info)
#define ANDROID_BINDER_GET_NODE_INFO_FOR_REF	_IOWR('b', 12, struct android_binder_node_info_for_ref)
#define ANDROID_BINDER_SET_CONTEXT_MGR_EXT	_IOW('b', 13, struct android_flat_binder_object)
#define ANDROID_BINDER_FREEZE			_IOW('b', 14, struct android_binder_freeze_info)
#define ANDROID_BINDER_GET_FROZEN_INFO		_IOWR('b', 15, struct android_binder_frozen_status_info)
#define ANDROID_BINDER_ENABLE_ONEWAY_SPAM_DETECTION _IOW('b', 16, __u32)

/* Binder protocol version */
#define ANDROID_BINDER_CURRENT_PROTOCOL_VERSION	8

struct android_binder_write_read {
	uint64_t	write_size;
	uint64_t	write_consumed;
	uint64_t	write_buffer;
	uint64_t	read_size;
	uint64_t	read_consumed;
	uint64_t	read_buffer;
};

struct android_binder_version {
	int32_t		protocol_version;
};

struct android_binder_freeze_info {
	uint32_t	pid;
	uint32_t	enable;
	uint32_t	timeout_ms;
};

struct android_binder_frozen_status_info {
	uint32_t	pid;
	uint32_t	sync_recv;
	uint32_t	async_recv;
};

struct android_binder_node_debug_info {
	uint64_t	ptr;
	uint64_t	cookie;
	uint32_t	has_strong_ref;
	uint32_t	has_weak_ref;
};

struct android_binder_node_info_for_ref {
	uint32_t	handle;
	uint32_t	strong_count;
	uint32_t	weak_count;
	uint32_t	reserved1;
	uint32_t	reserved2;
	uint32_t	reserved3;
};

struct android_flat_binder_object {
	uint32_t	type;
	uint32_t	flags;
	union {
		uint64_t	binder;
		uint32_t	handle;
	};
	uint64_t	cookie;
};

/*
 * Android Ashmem (anonymous shared memory) ioctls
 * Used by Android's MemoryFile / SharedMemory APIs.
 */
#define ANDROID_ASHMEM_NAME_LEN		256
#define ANDROID_ASHMEM_IOC_MAGIC	0x77
#define ANDROID_ASHMEM_SET_NAME		_IOW(ANDROID_ASHMEM_IOC_MAGIC, 1, char[ANDROID_ASHMEM_NAME_LEN])
#define ANDROID_ASHMEM_GET_NAME		_IOR(ANDROID_ASHMEM_IOC_MAGIC, 2, char[ANDROID_ASHMEM_NAME_LEN])
#define ANDROID_ASHMEM_SET_SIZE		_IOW(ANDROID_ASHMEM_IOC_MAGIC, 3, size_t)
#define ANDROID_ASHMEM_GET_SIZE		_IO(ANDROID_ASHMEM_IOC_MAGIC, 4)
#define ANDROID_ASHMEM_SET_PROT_MASK	_IOW(ANDROID_ASHMEM_IOC_MAGIC, 5, unsigned long)
#define ANDROID_ASHMEM_GET_PROT_MASK	_IO(ANDROID_ASHMEM_IOC_MAGIC, 6)
#define ANDROID_ASHMEM_PIN		_IOW(ANDROID_ASHMEM_IOC_MAGIC, 7, struct android_ashmem_pin)
#define ANDROID_ASHMEM_UNPIN		_IOW(ANDROID_ASHMEM_IOC_MAGIC, 8, struct android_ashmem_pin)
#define ANDROID_ASHMEM_GET_PIN_STATUS	_IO(ANDROID_ASHMEM_IOC_MAGIC, 9)
#define ANDROID_ASHMEM_PURGE_ALL_CACHES	_IO(ANDROID_ASHMEM_IOC_MAGIC, 10)

struct android_ashmem_pin {
	uint32_t	offset;
	uint32_t	len;
};

/*
 * Android-specific errno extensions.
 * Bionic uses the same errno values as Linux, but we document
 * the ones most encountered during Android app bring-up.
 */
#define EDEADLK_ANDROID	35	/* Resource deadlock would occur */
#define ENAMETOOLONG_ANDROID 36	/* File name too long */
#define ENOSYS_ANDROID	38	/* Syscall not implemented (triggers fallback) */

/*
 * UOS Bionic bridge device minor numbers.
 * /dev/binder  -> BIONIC_MINOR_BINDER
 * /dev/ashmem  -> BIONIC_MINOR_ASHMEM
 * /dev/hwbinder -> BIONIC_MINOR_HWBINDER
 * /dev/vndbinder -> BIONIC_MINOR_VNDBINDER
 */
#define BIONIC_MAJOR		180
#define BIONIC_MINOR_BINDER	0
#define BIONIC_MINOR_ASHMEM	1
#define BIONIC_MINOR_HWBINDER	2
#define BIONIC_MINOR_VNDBINDER	3

/* Function prototypes for UOS Bionic bridge kernel module */
int	uos_binder_ioctl(struct cdev *dev, u_long cmd, caddr_t data,
	    int fflag, struct thread *td);
int	uos_ashmem_ioctl(struct cdev *dev, u_long cmd, caddr_t data,
	    int fflag, struct thread *td);
int	uos_bionic_linux_ioctl(struct thread *td, int fd, u_long cmd,
	    caddr_t data);

#endif /* _KERNEL */

/*
 * Userspace-visible types (for UOS libuos_bridge.so)
 * These allow native UOS services to communicate with Android apps
 * through the Binder IPC mechanism without a full Android runtime.
 */

/* Binder object types */
#define BINDER_TYPE_BINDER		0x73622a85
#define BINDER_TYPE_WEAK_BINDER		0x37722a85
#define BINDER_TYPE_HANDLE		0x73682a85
#define BINDER_TYPE_WEAK_HANDLE		0x37682a85
#define BINDER_TYPE_FD			0x66642a85
#define BINDER_TYPE_FDA			0x46642a85
#define BINDER_TYPE_PTR			0x70742a85

/* Transaction flags */
#define TF_ONE_WAY			0x01
#define TF_ROOT_OBJECT			0x04
#define TF_STATUS_CODE			0x08
#define TF_ACCEPT_FDS			0x10

#endif /* _BIONIC_COMPAT_H_ */
