/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */
#ifndef _ASM_IA64_SN_IOERROR_HANDLING_H
#define _ASM_IA64_SN_IOERROR_HANDLING_H

#include <linux/config.h>
#include <linux/types.h>
#include <linux/devfs_fs_kernel.h>
#include <asm/sn/sgi.h>

#if __KERNEL__

/*
 * Basic types required for io error handling interfaces.
 */

/*
 * Return code from the io error handling interfaces.
 */

enum error_return_code_e {
	/* Success */
	ERROR_RETURN_CODE_SUCCESS,

	/* Unknown failure */
	ERROR_RETURN_CODE_GENERAL_FAILURE,

	/* Nth error noticed while handling the first error */
	ERROR_RETURN_CODE_NESTED_CALL,

	/* State of the vertex is invalid */
	ERROR_RETURN_CODE_INVALID_STATE,

	/* Invalid action */
	ERROR_RETURN_CODE_INVALID_ACTION,

	/* Valid action but not cannot set it */
	ERROR_RETURN_CODE_CANNOT_SET_ACTION,

	/* Valid action but not possible for the current state */
	ERROR_RETURN_CODE_CANNOT_PERFORM_ACTION,

	/* Valid state but cannot change the state of the vertex to it */
	ERROR_RETURN_CODE_CANNOT_SET_STATE,

	/* ??? */
	ERROR_RETURN_CODE_DUPLICATE,

	/* Reached the root of the system critical graph */
	ERROR_RETURN_CODE_SYS_CRITICAL_GRAPH_BEGIN,

	/* Reached the leaf of the system critical graph */
	ERROR_RETURN_CODE_SYS_CRITICAL_GRAPH_ADD,

	/* Cannot shutdown the device in hw/sw */
	ERROR_RETURN_CODE_SHUTDOWN_FAILED,

	/* Cannot restart the device in hw/sw */
	ERROR_RETURN_CODE_RESET_FAILED,

	/* Cannot failover the io subsystem */
	ERROR_RETURN_CODE_FAILOVER_FAILED,

	/* No Jump Buffer exists */
	ERROR_RETURN_CODE_NO_JUMP_BUFFER
};

typedef uint64_t  error_return_code_t;

/*
 * State of the vertex during error handling.
 */
enum error_state_e {
	/* Ignore state */
	ERROR_STATE_IGNORE,

	/* Invalid state */
	ERROR_STATE_NONE,

	/* Trying to decipher the error bits */
	ERROR_STATE_LOOKUP,

	/* Trying to carryout the action decided upon after
	 * looking at the error bits 
	 */
	ERROR_STATE_ACTION,

	/* Donot allow any other operations to this vertex from
	 * other parts of the kernel. This is also used to indicate
	 * that the device has been software shutdown.
	 */
	ERROR_STATE_SHUTDOWN,

	/* This is a transitory state when no new requests are accepted
	 * on behalf of the device. This is usually used when trying to
	 * quiesce all the outstanding operations and preparing the
	 * device for a failover / shutdown etc.
	 */
	ERROR_STATE_SHUTDOWN_IN_PROGRESS,

	/* This is the state when there is absolutely no activity going
	 * on wrt device.
	 */
	ERROR_STATE_SHUTDOWN_COMPLETE,
	
	/* This is the state when the device has issued a retry. */
	ERROR_STATE_RETRY,

	/* This is the normal state. This can also be used to indicate
	 * that the device has been software-enabled after software-
	 * shutting down previously.
	 */
	ERROR_STATE_NORMAL
	
};

typedef uint64_t  error_state_t;

/*
 * Generic error classes. This is used to classify errors after looking
 * at the error bits and helpful in deciding on the action.
 */
enum error_class_e {
	/* Unclassified error */
	ERROR_CLASS_UNKNOWN,

	/* LLP transmit error */
	ERROR_CLASS_LLP_XMIT,

	/* LLP receive error */
	ERROR_CLASS_LLP_RECV,

	/* Credit error */
	ERROR_CLASS_CREDIT,

	/* Timeout error */
	ERROR_CLASS_TIMEOUT,

	/* Access error */
	ERROR_CLASS_ACCESS,

	/* System coherency error */
	ERROR_CLASS_SYS_COHERENCY,

	/* Bad data error (ecc / parity etc) */
	ERROR_CLASS_BAD_DATA,

	/* Illegal request packet */
	ERROR_CLASS_BAD_REQ_PKT,
	
	/* Illegal response packet */
	ERROR_CLASS_BAD_RESP_PKT
};

typedef uint64_t  error_class_t;


/* 
 * Error context which the error action can use.
 */
typedef void			*error_context_t;
#define ERROR_CONTEXT_IGNORE	((error_context_t)-1ll)


/* 
 * Error action type.
 */
typedef error_return_code_t 	(*error_action_f)( error_context_t);
#define ERROR_ACTION_IGNORE	((error_action_f)-1ll)

/* Typical set of error actions */
typedef struct error_action_set_s {
	error_action_f		eas_panic;
	error_action_f		eas_shutdown;
	error_action_f		eas_abort;
	error_action_f		eas_retry;
	error_action_f		eas_failover;
	error_action_f		eas_log_n_ignore;
	error_action_f		eas_reset;
} error_action_set_t;


/* Set of priorites for in case mutliple error actions/states
 * are trying to be prescribed for a device.
 * NOTE : The ordering below encapsulates the priorities. Highest value
 * corresponds to highest priority.
 */
enum error_priority_e {
	ERROR_PRIORITY_IGNORE,
	ERROR_PRIORITY_NONE,
	ERROR_PRIORITY_NORMAL,
	ERROR_PRIORITY_LOG,
	ERROR_PRIORITY_FAILOVER,
	ERROR_PRIORITY_RETRY,
	ERROR_PRIORITY_ABORT,
	ERROR_PRIORITY_SHUTDOWN,
	ERROR_PRIORITY_RESTART,
	ERROR_PRIORITY_PANIC
};

typedef uint64_t  error_priority_t;

/* Error state interfaces */
#if defined(CONFIG_SGI_IO_ERROR_HANDLING)
extern error_return_code_t	error_state_set(vertex_hdl_t,error_state_t);
extern error_state_t		error_state_get(vertex_hdl_t);
#endif

/* Error action interfaces */

extern error_return_code_t	error_action_set(vertex_hdl_t,
						 error_action_f,
						 error_context_t,
						 error_priority_t);
extern error_return_code_t	error_action_perform(vertex_hdl_t);


#define INFO_LBL_ERROR_SKIP_ENV	"error_skip_env"

#define v_error_skip_env_get(v, l)		\
hwgraph_info_get_LBL(v, INFO_LBL_ERROR_SKIP_ENV, (arbitrary_info_t *)&l)

#define v_error_skip_env_set(v, l, r)		\
(r ? 						\
 hwgraph_info_replace_LBL(v, INFO_LBL_ERROR_SKIP_ENV, (arbitrary_info_t)l,0) :\
 hwgraph_info_add_LBL(v, INFO_LBL_ERROR_SKIP_ENV, (arbitrary_info_t)l))

#define v_error_skip_env_clear(v)		\
hwgraph_info_remove_LBL(v, INFO_LBL_ERROR_SKIP_ENV, 0)

/* REFERENCED */
#if defined(CONFIG_SGI_IO_ERROR_HANDLING)

inline static int
error_skip_point_mark(vertex_hdl_t  v)  			 
{									
	label_t		*error_env = NULL;	 			
	int		code = 0;		

	/* Check if we have a valid hwgraph vertex */
#ifdef	LATER
	if (!dev_is_vertex(v))
		return(code);
#endif
				
	/* There is no error jump buffer for this device vertex. Allocate
	 * one.								 
	 */								 
	if (v_error_skip_env_get(v, error_env) != GRAPH_SUCCESS) {	 
		error_env = snia_kmem_zalloc(sizeof(label_t));
		/* Unable to allocate memory for jum buffer. This should 
		 * be a very rare occurrence.				 
		 */							 
		if (!error_env)						 
			return(-1);					 
		/* Store the jump buffer information on the vertex.*/	 
		if (v_error_skip_env_set(v, error_env, 0) != GRAPH_SUCCESS)
			return(-2);					   
	}								   
	ASSERT(v_error_skip_env_get(v, error_env) == GRAPH_SUCCESS);
	code = setjmp(*error_env);					   
	return(code);							     
}
#endif	/* CONFIG_SGI_IO_ERROR_HANDLING */

typedef uint64_t		counter_t;

extern counter_t		error_retry_count_get(vertex_hdl_t);
extern error_return_code_t	error_retry_count_set(vertex_hdl_t,counter_t);
extern counter_t		error_retry_count_increment(vertex_hdl_t);
extern counter_t		error_retry_count_decrement(vertex_hdl_t);

/* Except for the PIO Read error typically the other errors are handled in
 * the context of an asynchronous error interrupt.
 */
#define	IS_ERROR_INTR_CONTEXT(_ec)	((_ec & IOECODE_DMA) 		|| \
					 (_ec == IOECODE_PIO_WRITE))

/* Some convenience macros on device state. This state is accessed only 
 * thru the calls the io error handling layer.
 */
#if defined(CONFIG_SGI_IO_ERROR_HANDLING)
extern boolean_t		is_device_shutdown(vertex_hdl_t);
#define IS_DEVICE_SHUTDOWN(_d) 	(is_device_shutdown(_d))
#endif

#endif /* __KERNEL__ */
#endif /* _ASM_IA64_SN_IOERROR_HANDLING_H */
