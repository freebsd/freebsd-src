/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#ifndef __LIBNVMF_H__
#define	__LIBNVMF_H__

#include <sys/_nv.h>
#include <sys/uio.h>
#include <stdbool.h>
#include <stddef.h>
#include <dev/nvme/nvme.h>
#include <dev/nvmf/nvmf.h>
#include <dev/nvmf/nvmf_proto.h>

struct nvmf_capsule;
struct nvmf_association;
struct nvmf_qpair;

/*
 * Parameters shared by all queue-pairs of an association.  Note that
 * this contains the requested values used to initiate transport
 * negotiation.
 */
struct nvmf_association_params {
	bool sq_flow_control;		/* SQ flow control required. */
	bool dynamic_controller_model;	/* Controller only */
	uint16_t max_admin_qsize;	/* Controller only */
	uint32_t max_io_qsize;		/* Controller only, 0 for discovery */
	union {
		struct {
			uint8_t pda;	/* Tx-side PDA. */
			bool header_digests;
			bool data_digests;
			uint32_t maxr2t;	/* Host only */
			uint32_t maxh2cdata;	/* Controller only */
		} tcp;
	};
};

/* Parameters specific to a single queue pair of an association. */
struct nvmf_qpair_params {
	bool admin;			/* Host only */
	union {
		struct {
			int fd;
		} tcp;
	};
};

__BEGIN_DECLS

/* Transport-independent APIs. */

/*
 * A host should allocate a new association for each association with
 * a controller.  After the admin queue has been allocated and the
 * controller's data has been fetched, it should be passed to
 * nvmf_update_association to update internal transport-specific
 * parameters before allocating I/O queues.
 *
 * A controller uses a single association to manage all incoming
 * queues since it is not known until after parsing the CONNECT
 * command which transport queues are admin vs I/O and which
 * controller they are created against.
 */
struct nvmf_association *nvmf_allocate_association(enum nvmf_trtype trtype,
    bool controller, const struct nvmf_association_params *params);
void	nvmf_update_assocation(struct nvmf_association *na,
    const struct nvme_controller_data *cdata);
void	nvmf_free_association(struct nvmf_association *na);

/* The most recent association-wide error message. */
const char *nvmf_association_error(const struct nvmf_association *na);

/*
 * A queue pair represents either an Admin or I/O
 * submission/completion queue pair.
 *
 * Each open qpair holds a reference on its association.  Once queue
 * pairs are allocated, callers can safely free the association to
 * ease bookkeeping.
 *
 * If nvmf_allocate_qpair fails, a detailed error message can be obtained
 * from nvmf_association_error.
 */
struct nvmf_qpair *nvmf_allocate_qpair(struct nvmf_association *na,
    const struct nvmf_qpair_params *params);
void	nvmf_free_qpair(struct nvmf_qpair *qp);

/*
 * Capsules are either commands (host -> controller) or responses
 * (controller -> host).  A single data buffer segment may be
 * associated with a command capsule.  Transmitted data is not copied
 * by this API but instead must be preserved until the capsule is
 * transmitted and freed.
 */
struct nvmf_capsule *nvmf_allocate_command(struct nvmf_qpair *qp,
    const void *sqe);
struct nvmf_capsule *nvmf_allocate_response(struct nvmf_qpair *qp,
    const void *cqe);
void	nvmf_free_capsule(struct nvmf_capsule *nc);
int	nvmf_capsule_append_data(struct nvmf_capsule *nc,
    void *buf, size_t len, bool send);
int	nvmf_transmit_capsule(struct nvmf_capsule *nc);
int	nvmf_receive_capsule(struct nvmf_qpair *qp, struct nvmf_capsule **ncp);
const void *nvmf_capsule_sqe(const struct nvmf_capsule *nc);
const void *nvmf_capsule_cqe(const struct nvmf_capsule *nc);

/* Return a string name for a transport type. */
const char *nvmf_transport_type(uint8_t trtype);

/*
 * Validate a NVMe Qualified Name.  The second version enforces
 * stricter checks inline with the specification.  The first version
 * enforces more minimal checks.
 */
bool	nvmf_nqn_valid(const char *nqn);
bool	nvmf_nqn_valid_strict(const char *nqn);

/* Controller-specific APIs. */

/*
 * A controller calls this function to check for any
 * transport-specific errors (invalid fields) in a received command
 * capsule.  The callback returns a generic command status value:
 * NVME_SC_SUCCESS if no error is found.
 */
uint8_t	nvmf_validate_command_capsule(const struct nvmf_capsule *nc);

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
 * should be passed in 'nc'.  The received data is stored in '*buf'.
 */
int	nvmf_receive_controller_data(const struct nvmf_capsule *nc,
    uint32_t data_offset, void *buf, size_t len);

/*
 * A controller calls this function to send data in response to a
 * command along with a response capsule.  If the data transfer
 * succeeds, a success response is sent.  If the data transfer fails,
 * an appropriate error status capsule is sent.  Regardless, a
 * response capsule is always sent.
 */
int	nvmf_send_controller_data(const struct nvmf_capsule *nc,
    const void *buf, size_t len);

/*
 * Construct a CQE for a reply to a command capsule in 'nc' with the
 * completion status 'status'.  This is useful when additional CQE
 * info is required beyond the completion status.
 */
void	nvmf_init_cqe(void *cqe, const struct nvmf_capsule *nc,
    uint16_t status);

/*
 * Construct and send a response capsule to a command capsule with
 * the supplied CQE.
 */
int	nvmf_send_response(const struct nvmf_capsule *nc, const void *cqe);

/*
 * Wait for a single command capsule and return it in *ncp.  This can
 * fail if an invalid capsule is received or an I/O error occurs.
 */
int	nvmf_controller_receive_capsule(struct nvmf_qpair *qp,
    struct nvmf_capsule **ncp);

/* Send a response capsule from a controller. */
int	nvmf_controller_transmit_response(struct nvmf_capsule *nc);

/* Construct and send an error response capsule. */
int	nvmf_send_error(const struct nvmf_capsule *cc, uint8_t sc_type,
    uint8_t sc_status);

/*
 * Construct and send an error response capsule using a generic status
 * code.
 */
int	nvmf_send_generic_error(const struct nvmf_capsule *nc,
    uint8_t sc_status);

/* Construct and send a simple success response capsule. */
int	nvmf_send_success(const struct nvmf_capsule *nc);

/*
 * Allocate a new queue pair and wait for the CONNECT command capsule.
 * If this fails, a detailed error message can be obtained from
 * nvmf_association_error.  On success, the command capsule is saved
 * in '*ccp' and the connect data is saved in 'data'.  The caller
 * must send an explicit response and free the the command capsule.
 */
struct nvmf_qpair *nvmf_accept(struct nvmf_association *na,
    const struct nvmf_qpair_params *params, struct nvmf_capsule **ccp,
    struct nvmf_fabric_connect_data *data);

/*
 * Construct and send a response capsule with the Fabrics CONNECT
 * invalid parameters error status.  If data is true the offset is
 * relative to the CONNECT data structure, otherwise the offset is
 * relative to the SQE.
 */
void	nvmf_connect_invalid_parameters(const struct nvmf_capsule *cc,
    bool data, uint16_t offset);

/* Construct and send a response capsule for a successful CONNECT. */
int	nvmf_finish_accept(const struct nvmf_capsule *cc, uint16_t cntlid);

/* Compute the initial state of CAP for a controller. */
uint64_t nvmf_controller_cap(struct nvmf_qpair *qp);

/* Generate a serial number string from a host ID. */
void	nvmf_controller_serial(char *buf, size_t len, u_long hostid);

/*
 * Populate an Identify Controller data structure for a Discovery
 * controller.
 */
void	nvmf_init_discovery_controller_data(struct nvmf_qpair *qp,
    struct nvme_controller_data *cdata);

/*
 * Populate an Identify Controller data structure for an I/O
 * controller.
 */
void	nvmf_init_io_controller_data(struct nvmf_qpair *qp, const char *serial,
    const char *subnqn, int nn, uint32_t ioccsz,
    struct nvme_controller_data *cdata);

/*
 * Validate if a new value for CC is legal given the existing values of
 * CAP and CC.
 */
bool	nvmf_validate_cc(struct nvmf_qpair *qp, uint64_t cap, uint32_t old_cc,
    uint32_t new_cc);

/* Return the log page id (LID) of a GET_LOG_PAGE command. */
uint8_t	nvmf_get_log_page_id(const struct nvme_command *cmd);

/* Return the requested data length of a GET_LOG_PAGE command. */
uint64_t nvmf_get_log_page_length(const struct nvme_command *cmd);

/* Return the requested data offset of a GET_LOG_PAGE command. */
uint64_t nvmf_get_log_page_offset(const struct nvme_command *cmd);

/* Prepare to handoff a controller qpair. */
int	nvmf_handoff_controller_qpair(struct nvmf_qpair *qp,
    const struct nvmf_fabric_connect_cmd *cmd,
    const struct nvmf_fabric_connect_data *data, struct nvmf_ioc_nv *nv);

/* Host-specific APIs. */

/*
 * Connect to an admin or I/O queue.  If this fails, a detailed error
 * message can be obtained from nvmf_association_error.
 */
struct nvmf_qpair *nvmf_connect(struct nvmf_association *na,
    const struct nvmf_qpair_params *params, uint16_t qid, u_int queue_size,
    const uint8_t hostid[16], uint16_t cntlid, const char *subnqn,
    const char *hostnqn, uint32_t kato);

/* Return the CNTLID for a queue returned from CONNECT. */
uint16_t nvmf_cntlid(struct nvmf_qpair *qp);

/*
 * Send a command to the controller.  This can fail with EBUSY if the
 * submission queue is full.
 */
int	nvmf_host_transmit_command(struct nvmf_capsule *nc);

/*
 * Wait for a response to a command.  If there are no outstanding
 * commands in the SQ, fails with EWOULDBLOCK.
 */
int	nvmf_host_receive_response(struct nvmf_qpair *qp,
    struct nvmf_capsule **rcp);

/*
 * Wait for a response to a specific command.  The command must have been
 * succesfully sent previously.
 */
int	nvmf_host_wait_for_response(struct nvmf_capsule *cc,
    struct nvmf_capsule **rcp);

/* Build a KeepAlive command. */
struct nvmf_capsule *nvmf_keepalive(struct nvmf_qpair *qp);

/* Read a controller property. */
int	nvmf_read_property(struct nvmf_qpair *qp, uint32_t offset, uint8_t size,
    uint64_t *value);

/* Write a controller property. */
int	nvmf_write_property(struct nvmf_qpair *qp, uint32_t offset,
    uint8_t size, uint64_t value);

/* Construct a 16-byte HostId from kern.hostuuid. */
int	nvmf_hostid_from_hostuuid(uint8_t hostid[16]);

/* Construct a NQN from kern.hostuuid. */
int	nvmf_nqn_from_hostuuid(char nqn[NVMF_NQN_MAX_LEN]);

/* Fetch controller data via IDENTIFY. */
int	nvmf_host_identify_controller(struct nvmf_qpair *qp,
    struct nvme_controller_data *data);

/* Fetch namespace data via IDENTIFY. */
int	nvmf_host_identify_namespace(struct nvmf_qpair *qp, uint32_t nsid,
    struct nvme_namespace_data *nsdata);

/*
 * Fetch discovery log page.  The memory for the log page is allocated
 * by malloc() and returned in *logp.  The caller must free the
 * memory.
 */
int	nvmf_host_fetch_discovery_log_page(struct nvmf_qpair *qp,
    struct nvme_discovery_log **logp);

/*
 * Construct a discovery log page entry that describes the connection
 * used by a host association's admin queue pair.
 */
int	nvmf_init_dle_from_admin_qp(struct nvmf_qpair *qp,
    const struct nvme_controller_data *cdata,
    struct nvme_discovery_log_entry *dle);

/*
 * Request a desired number of I/O queues via SET_FEATURES.  The
 * number of actual I/O queues available is returned in *actual on
 * success.
 */
int	nvmf_host_request_queues(struct nvmf_qpair *qp, u_int requested,
    u_int *actual);

/*
 * Handoff active host association to the kernel.  This frees the
 * qpairs (even on error).
 */
int	nvmf_handoff_host(const struct nvme_discovery_log_entry *dle,
    const char *hostnqn, struct nvmf_qpair *admin_qp, u_int num_queues,
    struct nvmf_qpair **io_queues, const struct nvme_controller_data *cdata,
    uint32_t reconnect_delay, uint32_t controller_loss_timeout);

/*
 * Disconnect an active host association previously handed off to the
 * kernel.  *name is either the name of the device (nvmeX) for this
 * association or the remote subsystem NQN.
 */
int	nvmf_disconnect_host(const char *host);

/*
 * Disconnect all active host associations previously handed off to
 * the kernel.
 */
int	nvmf_disconnect_all(void);

/*
 * Fetch reconnect parameters from an existing kernel host to use for
 * establishing a new association.  The caller must destroy the
 * returned nvlist.
 */
int	nvmf_reconnect_params(int fd, nvlist_t **nvlp);

/*
 * Handoff active host association to an existing host in the kernel.
 * This frees the qpairs (even on error).
 */
int	nvmf_reconnect_host(int fd, const struct nvme_discovery_log_entry *dle,
    const char *hostnqn, struct nvmf_qpair *admin_qp, u_int num_queues,
    struct nvmf_qpair **io_queues, const struct nvme_controller_data *cdata,
    uint32_t reconnect_delay, uint32_t controller_loss_timeout);

/*
 * Fetch connection status from an existing kernel host.
 */
int	nvmf_connection_status(int fd, nvlist_t **nvlp);

__END_DECLS

#endif /* !__LIBNVMF_H__ */
