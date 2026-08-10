/*
 * Coordination of operations between processes
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef PROC_COORD_H
#define PROC_COORD_H

struct proc_coord;

enum proc_coord_message_types {
	PROC_COORD_MSG_REQUEST = 0,
	PROC_COORD_MSG_RESPONSE = 1,
	PROC_COORD_MSG_EVENT = 2,
};

enum proc_coord_commands {
	PROC_COORD_CMD_STARTING = 0,
	PROC_COORD_CMD_STOPPING = 1,
	PROC_COORD_CMD_PING = 2,
	PROC_COORD_CMD_TEST = 3,
};

/**
 * proc_coord_init - Initialize process coordinations
 * @dir: Access controlled directory for process coordination
 * Returns: Context pointer on success or %NULL on failure
 *
 * The returned context must be released with a call to proc_coord_deinit().
 */
struct proc_coord * proc_coord_init(const char *dir);

/**
 * proc_coord_deinit - Deinitialize process coordinations
 * @pc: Process coordination context from proc_coord_init()
 */
void proc_coord_deinit(struct proc_coord *pc);

typedef bool (*proc_coord_cb)(void *ctx, int src,
			      enum proc_coord_message_types msg_type,
			      enum proc_coord_commands cmd,
			      u32 seq, const struct wpabuf *msg);

/**
 * proc_coord_register_handler - Register a handler for process coordination
 * @pc: Process coordination context from proc_coord_init()
 * @cb: Callback function
 * @cb_ctx: Context for the callback function
 * Returns: 0 on success or -1 on failure
 *
 * The registered handler will be called for received request and event
 * messages. Received request messages are delivered to the separate handler
 * registered with proc_coord_send_request().
 *
 * The handler function can return true to stop iteration of handler functions
 * or false to allow the iteration to continue reporting the message to other
 * registered handler functions, if any.
 */
int proc_coord_register_handler(struct proc_coord *pc, proc_coord_cb cb,
				void *cb_ctx);

/**
 * proc_coord_unregister_handler - Unregister a handler for process coordination
 * @pc: Process coordination context from proc_coord_init()
 * @cb: Callback function
 * @cb_ctx: Context for the callback function
 */
void proc_coord_unregister_handler(struct proc_coord *pc, proc_coord_cb cb,
				   void *cb_ctx);

/**
 * proc_coord_send_event - Send an event message
 * @pc: Process coordination context from proc_coord_init()
 * @dst: Destination peer (PID) or 0 for all active peers
 * @cmd: The command ID for the message
 * @msg: Payload of the message
 * Returns: The number of peers the message was sent to
 */
int proc_coord_send_event(struct proc_coord *pc, int dst,
			  enum proc_coord_commands cmd,
			  const struct wpabuf *msg);

typedef void (*proc_coord_response_cb)(void *ctx, int pid,
				       const struct wpabuf *msg);

/**
 * proc_coord_send_request - Send a request message
 * @pc: Process coordination context from proc_coord_init()
 * @dst: Destination peer (PID) or 0 for all active peers
 * @cmd: The command ID for the message
 * @msg: Payload of the message
 * @timeout_ms: Timeout for receiving a response
 * @cb: Callback function to report the responses or %NULL for no callback
 * @cb_ctx: Context for the callback function
 * Returns: The number of peers the message was sent to
 *
 * If a response is received from a peer, the response is reported to the
 * callback function. If no response is received within the specified timeout,
 * the callback function is called with msg == NULL. The specified @cb_ctx has
 * to remain valid until all the pending responses have been reported or until
 * proc_coord_cancel_wait() has been used to cancel any pending wait.
 */
int proc_coord_send_request(struct proc_coord *pc, int dst,
			    enum proc_coord_commands cmd,
			    const struct wpabuf *msg,
			    unsigned int timeout_ms,
			    proc_coord_response_cb cb,
			    void *cb_ctx);

/**
 * proc_coord_cancel_wait - Cancel wait for a pending response message
 * @pc: Process coordination context from proc_coord_init()
 * @cb: Callback function registered with proc_coord_send_request()
 * @cb_ctx: Context for the callback function
 */
void proc_coord_cancel_wait(struct proc_coord *pc, proc_coord_response_cb cb,
			    void *cb_ctx);

/**
 * proc_coord_send_event - Send a response message
 * @pc: Process coordination context from proc_coord_init()
 * @dst: Destination peer (PID)
 * @cmd: The command ID for the message
 * @seq: The sequence number from the received request message
 * @msg: Payload of the message
 * Returns: 0 on success or -1 on failure
 *
 * This is used to send a response to a request message that was reported
 * through a call to the handler function that was registered with
 * proc_coord_register_handler().
 */
int proc_coord_send_response(struct proc_coord *pc, int dst,
			     enum proc_coord_commands cmd, u32 seq,
			     const struct wpabuf *msg);

#endif /* PROC_COORD_H */
