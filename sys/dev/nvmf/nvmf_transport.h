/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#ifndef __NVMF_TRANSPORT_H__
#define	__NVMF_TRANSPORT_H__

/*
 * Interface used by the Fabrics host (initiator) and controller
 * (target) to send and receive capsules and associated data.
 */

#include <sys/_nv.h>
#include <sys/sysctl.h>
#include <dev/nvmf/nvmf_proto.h>

struct mbuf;
struct memdesc;
struct nvmf_capsule;
struct nvmf_connection;
struct nvmf_ioc_nv;
struct nvmf_qpair;

SYSCTL_DECL(_kern_nvmf);

/*
 * Callback to invoke when an error occurs on a qpair.  The last
 * parameter is an error value.  If the error value is zero, the qpair
 * has been closed at the transport level rather than a transport
 * error occuring.
 */
typedef void nvmf_qpair_error_t(void *, int);

/* Callback to invoke when a capsule is received. */
typedef void nvmf_capsule_receive_t(void *, struct nvmf_capsule *);

/*
 * Callback to invoke when an I/O request has completed.  The second
 * parameter is the amount of data transferred.  The last parameter is
 * an error value which is non-zero if the request did not complete
 * successfully.  A request with an error may complete partially.
 */
typedef void nvmf_io_complete_t(void *, size_t, int);

/*
 * A queue pair represents either an Admin or I/O
 * submission/completion queue pair.  The params contains negotiated
 * values passed in from userland.
 *
 * Unlike libnvmf in userland, the kernel transport interface does not
 * have any notion of an association.  Instead, qpairs are
 * independent.
 */
struct nvmf_qpair *nvmf_allocate_qpair(enum nvmf_trtype trtype,
    bool controller, const nvlist_t *params,
    nvmf_qpair_error_t *error_cb, void *error_cb_arg,
    nvmf_capsule_receive_t *receive_cb, void *receive_cb_arg);
void	nvmf_free_qpair(struct nvmf_qpair *qp);

/*
 * Capsules are either commands (host -> controller) or responses
 * (controller -> host).  A data buffer may be associated with a
 * command capsule.  Transmitted data is not copied by this API but
 * instead must be preserved until the completion callback is invoked
 * to indicate capsule transmission has completed.
 */
struct nvmf_capsule *nvmf_allocate_command(struct nvmf_qpair *qp,
    const void *sqe, int how);
struct nvmf_capsule *nvmf_allocate_response(struct nvmf_qpair *qp,
    const void *cqe, int how);
void	nvmf_free_capsule(struct nvmf_capsule *nc);
int	nvmf_capsule_append_data(struct nvmf_capsule *nc,
    struct memdesc *mem, size_t len, bool send,
    nvmf_io_complete_t *complete_cb, void *cb_arg);
int	nvmf_transmit_capsule(struct nvmf_capsule *nc);
void	nvmf_abort_capsule_data(struct nvmf_capsule *nc, int error);
void *nvmf_capsule_sqe(struct nvmf_capsule *nc);
void *nvmf_capsule_cqe(struct nvmf_capsule *nc);
bool	nvmf_sqhd_valid(struct nvmf_capsule *nc);

/* Controller-specific APIs. */

/*
 * A controller calls this function to check for any
 * transport-specific errors (invalid fields) in a received command
 * capsule.  The callback returns a generic command status value:
 * NVME_SC_SUCCESS if no error is found.
 */
uint8_t	nvmf_validate_command_capsule(struct nvmf_capsule *nc);

/*
 * A controller calls this function to query the amount of data
 * associated with a command capsule.
 */
size_t	nvmf_capsule_data_len(const struct nvmf_capsule *cc);

/*
 * A controller calls this function to receive data associated with a
 * command capsule (e.g. the data for a WRITE command).  This can
 * either return in-capsule data or fetch data from the host
 * (e.g. using a R2T PDU over TCP).  The received command capsule
 * should be passed in 'nc'.  The received data is stored in 'mem'.
 * If this function returns success, then the callback will be invoked
 * once the operation has completed.  Note that the callback might be
 * invoked before this function returns.
 */
int	nvmf_receive_controller_data(struct nvmf_capsule *nc,
    uint32_t data_offset, struct memdesc *mem, size_t len,
    nvmf_io_complete_t *complete_cb, void *cb_arg);

/*
 * A controller calls this function to send data in response to a
 * command prior to sending a response capsule.  If an error occurs,
 * the function returns a generic status completion code to be sent in
 * the following CQE.  Note that the transfer might send a subset of
 * the data requested by nc.  If the transfer succeeds, this function
 * can return one of the following values:
 *
 * - NVME_SC_SUCCESS: The transfer has completed successfully and the
 *   caller should send a success CQE in a response capsule.
 *
 * - NVMF_SUCCESS_SENT: The transfer has completed successfully and
 *   the transport layer has sent an implicit success CQE to the
 *   remote host (e.g. the SUCCESS flag for TCP).  The caller should
 *   not send a response capsule.
 *
 * - NVMF_MORE: The transfer has completed successfully, but the
 *   transfer did not complete the data buffer.
 *
 * The mbuf chain in 'm' is consumed by this function even if an error
 * is returned.
 */
u_int	nvmf_send_controller_data(struct nvmf_capsule *nc,
    uint32_t data_offset, struct mbuf *m, size_t len);

#define	NVMF_SUCCESS_SENT	0x100
#define	NVMF_MORE		0x101

/* Helper APIs for nvlists used in icotls. */

/*
 * Pack the nvlist nvl and copyout to the buffer described by nv.
 */
int	nvmf_pack_ioc_nvlist(const nvlist_t *nvl, struct nvmf_ioc_nv *nv);

/*
 * Copyin and unpack an nvlist described by nv.  The unpacked nvlist
 * is returned in *nvlp on success.
 */
int	nvmf_unpack_ioc_nvlist(const struct nvmf_ioc_nv *nv, nvlist_t **nvlp);

/*
 * Returns true if a qpair handoff nvlist has all the required
 * transport-independent values.
 */
bool	nvmf_validate_qpair_nvlist(const nvlist_t *nvl, bool controller);

#endif /* !__NVMF_TRANSPORT_H__ */
