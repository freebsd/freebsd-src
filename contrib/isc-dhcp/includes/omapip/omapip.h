/* omapip.h

   Definitions for the object management API and protocol... */

/*
 * Copyright (c) 1996-2001 Internet Software Consortium.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon in cooperation with Vixie Enterprises and Nominum, Inc.
 * To learn more about the Internet Software Consortium, see
 * ``http://www.isc.org/''.  To learn more about Vixie Enterprises,
 * see ``http://www.vix.com''.   To learn more about Nominum, Inc., see
 * ``http://www.nominum.com''.
 */

#ifndef _OMAPIP_H_
#define _OMAPIP_H_
#include <isc-dhcp/result.h>

typedef unsigned int omapi_handle_t;

struct __omapi_object;
typedef struct __omapi_object omapi_object_t;

typedef enum {
	omapi_datatype_int,
	omapi_datatype_string,
	omapi_datatype_data,
	omapi_datatype_object
} omapi_datatype_t;

typedef struct {
	int refcnt;
	omapi_datatype_t type;
	union {
		struct {
			unsigned len;
#define OMAPI_TYPED_DATA_NOBUFFER_LEN (sizeof (int) + \
				       sizeof (omapi_datatype_t) + \
				       sizeof (int))
			unsigned char value [1];
		} buffer;
#define OMAPI_TYPED_DATA_OBJECT_LEN (sizeof (int) + \
				     sizeof (omapi_datatype_t) + \
				     sizeof (omapi_object_t *))
		omapi_object_t *object;
#define OMAPI_TYPED_DATA_REF_LEN (sizeof (int) + \
				  sizeof (omapi_datatype_t) + \
				  3 * sizeof (void *))
		struct {
			void *ptr;
			isc_result_t (*reference) (void *,
						   void *, const char *, int);
			isc_result_t (*dereference) (void *,
						     const char *, int);
		} ref;
#define OMAPI_TYPED_DATA_INT_LEN (sizeof (int) + \
				  sizeof (omapi_datatype_t) + \
				  sizeof (int))
		int integer;
	} u;
} omapi_typed_data_t;

typedef struct {
	int refcnt;
	unsigned len;
#define OMAPI_DATA_STRING_EMPTY_SIZE (2 * sizeof (int))
	unsigned char value [1];
} omapi_data_string_t;

typedef struct {
	int refcnt;
	omapi_data_string_t *name;
	omapi_typed_data_t *value;
} omapi_value_t;

typedef struct __omapi_object_type_t {
	const char *name;
	struct __omapi_object_type_t *next;
	
	isc_result_t (*set_value) (omapi_object_t *, omapi_object_t *,
				   omapi_data_string_t *,
				   omapi_typed_data_t *);
	isc_result_t (*get_value) (omapi_object_t *,
				   omapi_object_t *,
				   omapi_data_string_t *, omapi_value_t **);
	isc_result_t (*destroy) (omapi_object_t *, const char *, int);
	isc_result_t (*signal_handler) (omapi_object_t *,
					const char *, va_list);
	isc_result_t (*stuff_values) (omapi_object_t *,
				      omapi_object_t *, omapi_object_t *);
	isc_result_t (*lookup) (omapi_object_t **, omapi_object_t *,
				omapi_object_t *);
	isc_result_t (*create) (omapi_object_t **, omapi_object_t *);
	isc_result_t (*remove) (omapi_object_t *, omapi_object_t *);
	isc_result_t (*freer) (omapi_object_t *, const char *, int);
	isc_result_t (*allocator) (omapi_object_t **, const char *, int);
	isc_result_t (*sizer) (size_t);
	size_t size;
	int rc_flag;
	isc_result_t (*initialize) (omapi_object_t *, const char *, int);
} omapi_object_type_t;

#define OMAPI_OBJECT_PREAMBLE \
	omapi_object_type_t *type; \
	int refcnt; \
	omapi_handle_t handle; \
	omapi_object_t *outer, *inner

/* The omapi handle structure. */
struct __omapi_object {
	OMAPI_OBJECT_PREAMBLE;
};

/* The port on which applications should listen for OMAPI connections. */
#define OMAPI_PROTOCOL_PORT	7911

typedef struct {
	unsigned addrtype;
	unsigned addrlen;
	unsigned char address [16];
	unsigned port;
} omapi_addr_t;

typedef struct {
	int refcnt;
	unsigned count;
	omapi_addr_t *addresses;
} omapi_addr_list_t;

typedef struct auth_key {
	OMAPI_OBJECT_PREAMBLE;
	char *name;
	char *algorithm;
	omapi_data_string_t *key;
} omapi_auth_key_t;

#define OMAPI_CREATE          1
#define OMAPI_UPDATE          2
#define OMAPI_EXCL            4
#define OMAPI_NOTIFY_PROTOCOL 8

#define OMAPI_OBJECT_ALLOC(name, stype, type) \
isc_result_t name##_allocate (stype **p, const char *file, int line)	      \
{									      \
	return omapi_object_allocate ((omapi_object_t **)p,		      \
				      type, 0, file, line);		      \
}									      \
									      \
isc_result_t name##_reference (stype **pptr, stype *ptr,		      \
			       const char *file, int line)		      \
{									      \
	return omapi_object_reference ((omapi_object_t **)pptr,		      \
				       (omapi_object_t *)ptr, file, line);    \
}									      \
									      \
isc_result_t name##_dereference (stype **ptr, const char *file, int line)     \
{									      \
	return omapi_object_dereference ((omapi_object_t **)ptr, file, line); \
}

#define OMAPI_OBJECT_ALLOC_DECL(name, stype, type) \
isc_result_t name##_allocate (stype **p, const char *file, int line); \
isc_result_t name##_reference (stype **pptr, stype *ptr, \
			       const char *file, int line); \
isc_result_t name##_dereference (stype **ptr, const char *file, int line);

typedef isc_result_t (*omapi_array_ref_t) (char **, char *, const char *, int);
typedef isc_result_t (*omapi_array_deref_t) (char **, const char *, int);

/* An extensible array type. */
typedef struct {
	char **data;
	omapi_array_ref_t ref;
	omapi_array_deref_t deref;
	int count;
	int max;
} omapi_array_t;

#define OMAPI_ARRAY_TYPE(name, stype)					      \
isc_result_t name##_array_allocate (omapi_array_t **p,			      \
				    const char *file, int line)		      \
{									      \
	return (omapi_array_allocate					      \
		(p,							      \
		 (omapi_array_ref_t)name##_reference,			      \
		 (omapi_array_deref_t)name##_dereference,		      \
		 file, line));						      \
}									      \
									      \
isc_result_t name##_array_free (omapi_array_t **p,			      \
				const char *file, int line)		      \
{									      \
	return omapi_array_free (p, file, line);			      \
}									      \
									      \
isc_result_t name##_array_extend (omapi_array_t *pptr, stype *ptr, int *index,\
				  const char *file, int line)		      \
{									      \
	return omapi_array_extend (pptr, (char *)ptr, index, file, line);     \
}									      \
									      \
isc_result_t name##_array_set (omapi_array_t *pptr, stype *ptr,	int index,    \
			       const char *file, int line)		      \
{									      \
	return omapi_array_set (pptr, (char *)ptr, index, file, line);	      \
}									      \
									      \
isc_result_t name##_array_lookup (stype **ptr, omapi_array_t *pptr,	      \
				  int index, const char *file, int line)      \
{									      \
	return omapi_array_lookup ((char **)ptr, pptr, index, file, line);    \
}

#define OMAPI_ARRAY_TYPE_DECL(name, stype) \
isc_result_t name##_array_allocate (omapi_array_t **, const char *, int);     \
isc_result_t name##_array_free (omapi_array_t **, const char *, int);	      \
isc_result_t name##_array_extend (omapi_array_t *, stype *, int *,	      \
				  const char *, int);			      \
isc_result_t name##_array_set (omapi_array_t *,				      \
			       stype *, int, const char *, int);	      \
isc_result_t name##_array_lookup (stype **,				      \
				  omapi_array_t *, int, const char *, int)

#define	omapi_array_foreach_begin(array, stype, var)			      \
	{								      \
		int omapi_array_foreach_index;				      \
		stype *var = (stype *)0;				      \
		for (omapi_array_foreach_index = 0;			      \
			     array &&					      \
			     omapi_array_foreach_index < (array) -> count;    \
		     omapi_array_foreach_index++) {			      \
			if ((array) -> data [omapi_array_foreach_index]) {    \
				((*(array) -> ref)			      \
				 ((char **)&var,			      \
				  (array) -> data [omapi_array_foreach_index],\
				  MDL));

#define	omapi_array_foreach_end(array, stype, var)			      \
				(*(array) -> deref) ((char **)&var, MDL);     \
			}						      \
		}							      \
	}

isc_result_t omapi_protocol_connect (omapi_object_t *,
				     const char *, unsigned, omapi_object_t *);
isc_result_t omapi_connect_list (omapi_object_t *, omapi_addr_list_t *,
				 omapi_addr_t *);
isc_result_t omapi_protocol_listen (omapi_object_t *, unsigned, int);
isc_boolean_t omapi_protocol_authenticated (omapi_object_t *);
isc_result_t omapi_protocol_configure_security (omapi_object_t *,
						isc_result_t (*)
						(omapi_object_t *,
						 omapi_addr_t *),
						isc_result_t (*)
						(omapi_object_t *,
						 omapi_auth_key_t *));
isc_result_t omapi_protocol_accept (omapi_object_t *);
isc_result_t omapi_protocol_send_intro (omapi_object_t *, unsigned, unsigned);
isc_result_t omapi_protocol_ready (omapi_object_t *);
isc_result_t omapi_protocol_add_auth (omapi_object_t *, omapi_object_t *,
				      omapi_handle_t);
isc_result_t omapi_protocol_lookup_auth (omapi_object_t **, omapi_object_t *,
					 omapi_handle_t);
isc_result_t omapi_protocol_set_value (omapi_object_t *, omapi_object_t *,
				       omapi_data_string_t *,
				       omapi_typed_data_t *);
isc_result_t omapi_protocol_get_value (omapi_object_t *, omapi_object_t *,
				       omapi_data_string_t *,
				       omapi_value_t **); 
isc_result_t omapi_protocol_stuff_values (omapi_object_t *,
					  omapi_object_t *,
					  omapi_object_t *);

isc_result_t omapi_protocol_destroy (omapi_object_t *, const char *, int);
isc_result_t omapi_protocol_send_message (omapi_object_t *,
					  omapi_object_t *,
					  omapi_object_t *,
					  omapi_object_t *);
isc_result_t omapi_protocol_signal_handler (omapi_object_t *,
					    const char *, va_list);
isc_result_t omapi_protocol_listener_set_value (omapi_object_t *,
						omapi_object_t *,
						omapi_data_string_t *,
						omapi_typed_data_t *);
isc_result_t omapi_protocol_listener_get_value (omapi_object_t *,
						omapi_object_t *,
						omapi_data_string_t *,
						omapi_value_t **); 
isc_result_t omapi_protocol_listener_destroy (omapi_object_t *,
					      const char *, int);
isc_result_t omapi_protocol_listener_signal (omapi_object_t *,
					     const char *, va_list);
isc_result_t omapi_protocol_listener_stuff (omapi_object_t *,
					    omapi_object_t *,
					    omapi_object_t *);
isc_result_t omapi_protocol_send_status (omapi_object_t *, omapi_object_t *,
					 isc_result_t, unsigned, const char *);
isc_result_t omapi_protocol_send_open (omapi_object_t *, omapi_object_t *,
				       const char *, omapi_object_t *,
				       unsigned);
isc_result_t omapi_protocol_send_update (omapi_object_t *, omapi_object_t *,
					 unsigned, omapi_object_t *);

isc_result_t omapi_connect (omapi_object_t *, const char *, unsigned);
isc_result_t omapi_disconnect (omapi_object_t *, int);
int omapi_connection_readfd (omapi_object_t *);
int omapi_connection_writefd (omapi_object_t *);
isc_result_t omapi_connection_connect (omapi_object_t *);
isc_result_t omapi_connection_reader (omapi_object_t *);
isc_result_t omapi_connection_writer (omapi_object_t *);
isc_result_t omapi_connection_reaper (omapi_object_t *);
isc_result_t omapi_connection_output_auth_length (omapi_object_t *,
                                                  unsigned *);
isc_result_t omapi_connection_set_value (omapi_object_t *, omapi_object_t *,
					 omapi_data_string_t *,
					 omapi_typed_data_t *);
isc_result_t omapi_connection_get_value (omapi_object_t *, omapi_object_t *,
					 omapi_data_string_t *,
					 omapi_value_t **); 
isc_result_t omapi_connection_destroy (omapi_object_t *, const char *, int);
isc_result_t omapi_connection_signal_handler (omapi_object_t *,
					      const char *, va_list);
isc_result_t omapi_connection_stuff_values (omapi_object_t *,
					    omapi_object_t *,
					    omapi_object_t *);
isc_result_t omapi_connection_write_typed_data (omapi_object_t *,
						omapi_typed_data_t *);
isc_result_t omapi_connection_put_name (omapi_object_t *, const char *);
isc_result_t omapi_connection_put_string (omapi_object_t *, const char *);
isc_result_t omapi_connection_put_handle (omapi_object_t *c,
					  omapi_object_t *h);

isc_result_t omapi_listen (omapi_object_t *, unsigned, int);
isc_result_t omapi_listen_addr (omapi_object_t *,
				omapi_addr_t *, int);
isc_result_t omapi_listener_accept (omapi_object_t *);
int omapi_listener_readfd (omapi_object_t *);
isc_result_t omapi_accept (omapi_object_t *);
isc_result_t omapi_listener_configure_security (omapi_object_t *,
						isc_result_t (*)
						(omapi_object_t *,
						 omapi_addr_t *));
isc_result_t omapi_listener_set_value (omapi_object_t *, omapi_object_t *,
				       omapi_data_string_t *,
				       omapi_typed_data_t *);
isc_result_t omapi_listener_get_value (omapi_object_t *, omapi_object_t *,
				       omapi_data_string_t *,
				       omapi_value_t **); 
isc_result_t omapi_listener_destroy (omapi_object_t *, const char *, int);
isc_result_t omapi_listener_signal_handler (omapi_object_t *,
					    const char *, va_list);
isc_result_t omapi_listener_stuff_values (omapi_object_t *,
					  omapi_object_t *,
					  omapi_object_t *);

isc_result_t omapi_register_io_object (omapi_object_t *,
				       int (*)(omapi_object_t *),
				       int (*)(omapi_object_t *),
				       isc_result_t (*)(omapi_object_t *),
				       isc_result_t (*)(omapi_object_t *),
				       isc_result_t (*)(omapi_object_t *));
isc_result_t omapi_unregister_io_object (omapi_object_t *);
isc_result_t omapi_dispatch (struct timeval *);
isc_result_t omapi_wait_for_completion (omapi_object_t *, struct timeval *);
isc_result_t omapi_one_dispatch (omapi_object_t *, struct timeval *);
isc_result_t omapi_io_set_value (omapi_object_t *, omapi_object_t *,
				 omapi_data_string_t *,
				 omapi_typed_data_t *);
isc_result_t omapi_io_get_value (omapi_object_t *, omapi_object_t *,
				 omapi_data_string_t *, omapi_value_t **); 
isc_result_t omapi_io_destroy (omapi_object_t *, const char *, int);
isc_result_t omapi_io_signal_handler (omapi_object_t *, const char *, va_list);
isc_result_t omapi_io_stuff_values (omapi_object_t *,
				    omapi_object_t *,
				    omapi_object_t *);
isc_result_t omapi_waiter_signal_handler (omapi_object_t *,
					  const char *, va_list);
isc_result_t omapi_io_state_foreach (isc_result_t (*func) (omapi_object_t *,
							   void *),
				     void *p);

isc_result_t omapi_generic_new (omapi_object_t **, const char *, int);
isc_result_t omapi_generic_set_value  (omapi_object_t *, omapi_object_t *,
				       omapi_data_string_t *,
				       omapi_typed_data_t *);
isc_result_t omapi_generic_get_value (omapi_object_t *, omapi_object_t *,
				      omapi_data_string_t *,
				      omapi_value_t **); 
isc_result_t omapi_generic_destroy (omapi_object_t *, const char *, int);
isc_result_t omapi_generic_signal_handler (omapi_object_t *,
					   const char *, va_list);
isc_result_t omapi_generic_stuff_values (omapi_object_t *,
					 omapi_object_t *,
					 omapi_object_t *);
isc_result_t omapi_generic_clear_flags (omapi_object_t *);

isc_result_t omapi_message_new (omapi_object_t **, const char *, int);
isc_result_t omapi_message_set_value  (omapi_object_t *, omapi_object_t *,
				       omapi_data_string_t *,
				       omapi_typed_data_t *);
isc_result_t omapi_message_get_value (omapi_object_t *, omapi_object_t *,
				      omapi_data_string_t *,
				      omapi_value_t **); 
isc_result_t omapi_message_destroy (omapi_object_t *, const char *, int);
isc_result_t omapi_message_signal_handler (omapi_object_t *,
					   const char *, va_list);
isc_result_t omapi_message_stuff_values (omapi_object_t *,
					 omapi_object_t *,
					 omapi_object_t *);
isc_result_t omapi_message_register (omapi_object_t *);
isc_result_t omapi_message_unregister (omapi_object_t *);
isc_result_t omapi_message_process (omapi_object_t *, omapi_object_t *);

OMAPI_OBJECT_ALLOC_DECL (omapi_auth_key,
			 omapi_auth_key_t, omapi_type_auth_key)
isc_result_t omapi_auth_key_new (omapi_auth_key_t **, const char *, int);
isc_result_t omapi_auth_key_destroy (omapi_object_t *, const char *, int);
isc_result_t omapi_auth_key_enter (omapi_auth_key_t *);
isc_result_t omapi_auth_key_lookup_name (omapi_auth_key_t **, const char *);
isc_result_t omapi_auth_key_lookup (omapi_object_t **,
				    omapi_object_t *,
				    omapi_object_t *);
isc_result_t omapi_auth_key_get_value (omapi_object_t *, omapi_object_t *,
				       omapi_data_string_t *,
				       omapi_value_t **); 
isc_result_t omapi_auth_key_stuff_values (omapi_object_t *,
					  omapi_object_t *,
					  omapi_object_t *);

extern omapi_object_type_t *omapi_type_connection;
extern omapi_object_type_t *omapi_type_listener;
extern omapi_object_type_t *omapi_type_io_object;
extern omapi_object_type_t *omapi_type_generic;
extern omapi_object_type_t *omapi_type_protocol;
extern omapi_object_type_t *omapi_type_protocol_listener;
extern omapi_object_type_t *omapi_type_waiter;
extern omapi_object_type_t *omapi_type_remote;
extern omapi_object_type_t *omapi_type_message;
extern omapi_object_type_t *omapi_type_auth_key;

extern omapi_object_type_t *omapi_object_types;

void omapi_type_relinquish (void);
isc_result_t omapi_init (void);
isc_result_t omapi_object_type_register (omapi_object_type_t **,
					 const char *,
					 isc_result_t (*)
						(omapi_object_t *,
						 omapi_object_t *,
						 omapi_data_string_t *,
						 omapi_typed_data_t *),
					 isc_result_t (*)
						(omapi_object_t *,
						 omapi_object_t *,
						 omapi_data_string_t *,
						 omapi_value_t **),
					 isc_result_t (*) (omapi_object_t *,
							   const char *, int),
					 isc_result_t (*) (omapi_object_t *,
							   const char *,
							   va_list),
					 isc_result_t (*) (omapi_object_t *,
							   omapi_object_t *,
							   omapi_object_t *),
					 isc_result_t (*) (omapi_object_t **,
							   omapi_object_t *,
							   omapi_object_t *),
					 isc_result_t (*) (omapi_object_t **,
							   omapi_object_t *),
					 isc_result_t (*) (omapi_object_t *,
							   omapi_object_t *),
					 isc_result_t (*) (omapi_object_t *,
							   const char *, int),
					 isc_result_t (*) (omapi_object_t **,
							   const char *, int),
					 isc_result_t (*) (size_t), size_t,
					 isc_result_t (*) (omapi_object_t *,
							   const char *, int),
					 int);
isc_result_t omapi_signal (omapi_object_t *, const char *, ...);
isc_result_t omapi_signal_in (omapi_object_t *, const char *, ...);
isc_result_t omapi_set_value (omapi_object_t *, omapi_object_t *,
			      omapi_data_string_t *,
			      omapi_typed_data_t *);
isc_result_t omapi_set_value_str (omapi_object_t *, omapi_object_t *,
				  const char *, omapi_typed_data_t *);
isc_result_t omapi_set_boolean_value (omapi_object_t *, omapi_object_t *,
				      const char *, int);
isc_result_t omapi_set_int_value (omapi_object_t *, omapi_object_t *,
				  const char *, int);
isc_result_t omapi_set_object_value (omapi_object_t *, omapi_object_t *,
				     const char *, omapi_object_t *);
isc_result_t omapi_set_string_value (omapi_object_t *, omapi_object_t *,
				     const char *, const char *);
isc_result_t omapi_get_value (omapi_object_t *, omapi_object_t *,
			      omapi_data_string_t *,
			      omapi_value_t **); 
isc_result_t omapi_get_value_str (omapi_object_t *, omapi_object_t *,
				  const char *, omapi_value_t **); 
isc_result_t omapi_stuff_values (omapi_object_t *,
				 omapi_object_t *,
				 omapi_object_t *);
isc_result_t omapi_object_create (omapi_object_t **, omapi_object_t *,
				  omapi_object_type_t *);
isc_result_t omapi_object_update (omapi_object_t *, omapi_object_t *,
				  omapi_object_t *, omapi_handle_t);
int omapi_data_string_cmp (omapi_data_string_t *, omapi_data_string_t *);
int omapi_ds_strcmp (omapi_data_string_t *, const char *);
int omapi_td_strcmp (omapi_typed_data_t *, const char *);
int omapi_td_strcasecmp (omapi_typed_data_t *, const char *);
isc_result_t omapi_make_value (omapi_value_t **, omapi_data_string_t *,
			       omapi_typed_data_t *, const char *, int);
isc_result_t omapi_make_const_value (omapi_value_t **, omapi_data_string_t *,
				     const unsigned char *,
				     unsigned, const char *, int);
isc_result_t omapi_make_int_value (omapi_value_t **, omapi_data_string_t *,
				   int, const char *, int);
isc_result_t omapi_make_uint_value (omapi_value_t **, omapi_data_string_t *,
				    unsigned int, const char *, int);
isc_result_t omapi_make_object_value (omapi_value_t **, omapi_data_string_t *,
				      omapi_object_t *, const char *, int);
isc_result_t omapi_make_handle_value (omapi_value_t **, omapi_data_string_t *,
				      omapi_object_t *, const char *, int);
isc_result_t omapi_make_string_value (omapi_value_t **, omapi_data_string_t *,
				      const char *, const char *, int);
isc_result_t omapi_get_int_value (unsigned long *, omapi_typed_data_t *);

isc_result_t omapi_object_handle (omapi_handle_t *, omapi_object_t *);
isc_result_t omapi_handle_lookup (omapi_object_t **, omapi_handle_t);
isc_result_t omapi_handle_td_lookup (omapi_object_t **, omapi_typed_data_t *);

void * dmalloc (unsigned, const char *, int);
void dfree (void *, const char *, int);
#if defined (DEBUG_MEMORY_LEAKAGE) || defined (DEBUG_MALLOC_POOL) || \
		defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
void dmalloc_reuse (void *, const char *, int, int);
void dmalloc_dump_outstanding (void);
#else
#define dmalloc_reuse(x,y,l,z)
#endif
#define MDL __FILE__, __LINE__
#if defined (DEBUG_RC_HISTORY)
void dump_rc_history (void *);
void rc_history_next (int);
#endif
void omapi_print_dmalloc_usage_by_caller (void);
isc_result_t omapi_object_allocate (omapi_object_t **,
				    omapi_object_type_t *,
				    size_t, const char *, int);
isc_result_t omapi_object_initialize (omapi_object_t *,
				      omapi_object_type_t *,
				      size_t, size_t, const char *, int);
isc_result_t omapi_object_reference (omapi_object_t **,
				     omapi_object_t *, const char *, int);
isc_result_t omapi_object_dereference (omapi_object_t **, const char *, int);
isc_result_t omapi_typed_data_new (const char *, int, omapi_typed_data_t **,
				   omapi_datatype_t, ...);
isc_result_t omapi_typed_data_reference (omapi_typed_data_t **,
					 omapi_typed_data_t *,
					 const char *, int);
isc_result_t omapi_typed_data_dereference (omapi_typed_data_t **,
					   const char *, int);
isc_result_t omapi_data_string_new (omapi_data_string_t **,
				    unsigned, const char *, int);
isc_result_t omapi_data_string_reference (omapi_data_string_t **,
					  omapi_data_string_t *,
					  const char *, int);
isc_result_t omapi_data_string_dereference (omapi_data_string_t **,
					    const char *, int);
isc_result_t omapi_value_new (omapi_value_t **, const char *, int);
isc_result_t omapi_value_reference (omapi_value_t **,
				    omapi_value_t *, const char *, int);
isc_result_t omapi_value_dereference (omapi_value_t **, const char *, int);
isc_result_t omapi_addr_list_new (omapi_addr_list_t **, unsigned,
				  const char *, int);
isc_result_t omapi_addr_list_reference (omapi_addr_list_t **,
					omapi_addr_list_t *,
					const char *, int);
isc_result_t omapi_addr_list_dereference (omapi_addr_list_t **,
					  const char *, int);

isc_result_t omapi_array_allocate (omapi_array_t **, omapi_array_ref_t,
				   omapi_array_deref_t, const char *, int);
isc_result_t omapi_array_free (omapi_array_t **, const char *, int);
isc_result_t omapi_array_extend (omapi_array_t *, char *, int *,
				 const char *, int);
isc_result_t omapi_array_set (omapi_array_t *, void *, int, const char *, int);
isc_result_t omapi_array_lookup (char **,
				 omapi_array_t *, int, const char *, int);
OMAPI_ARRAY_TYPE_DECL(omapi_object, omapi_object_t);
#endif /* _OMAPIP_H_ */
