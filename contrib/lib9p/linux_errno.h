/*
 * Copyright 2016 Chris Torek <torek@ixsystems.com>
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef LIB9P_LINUX_ERRNO_H
#define LIB9P_LINUX_ERRNO_H

/*
 * Linux error numbers that are outside of the original base range
 * (which ends with ERANGE).
 *
 * This is pretty much the same as Linux's errno.h except that the
 * names are prefixed with "LINUX_", and we add _STR with the
 * string name.
 *
 * The string expansions were obtained with a little program to
 * print every strerror().
 *
 * Note that BSD EDEADLK is 11 and BSD EAGAIN is 35, vs
 * Linux / Plan9 EAGAIN at 11.  So one value in the ERANGE
 * range still needs translation too.
 */

#define	LINUX_EAGAIN		11
#define	LINUX_EAGAIN_STR	"Resource temporarily unavailable"

#define	LINUX_EDEADLK		35
#define	LINUX_EDEADLK_STR	"Resource deadlock avoided"
#define	LINUX_ENAMETOOLONG	36
#define	LINUX_ENAMETOOLONG_STR	"File name too long"
#define	LINUX_ENOLCK		37
#define	LINUX_ENOLCK_STR	"No locks available"
#define	LINUX_ENOSYS		38
#define	LINUX_ENOSYS_STR	"Function not implemented"
#define	LINUX_ENOTEMPTY		39
#define	LINUX_ENOTEMPTY_STR	"Directory not empty"
#define	LINUX_ELOOP		40
#define	LINUX_ELOOP_STR		"Too many levels of symbolic links"
/*				41 unused */
#define	LINUX_ENOMSG		42
#define	LINUX_ENOMSG_STR	"No message of desired type"
#define	LINUX_EIDRM		43
#define	LINUX_EIDRM_STR		"Identifier removed"
#define	LINUX_ECHRNG		44
#define	LINUX_ECHRNG_STR	"Channel number out of range"
#define	LINUX_EL2NSYNC		45
#define	LINUX_EL2NSYNC_STR	"Level 2 not synchronized"
#define	LINUX_EL3HLT		46
#define	LINUX_EL3HLT_STR	"Level 3 halted"
#define	LINUX_EL3RST		47
#define	LINUX_EL3RST_STR	"Level 3 reset"
#define	LINUX_ELNRNG		48
#define	LINUX_ELNRNG_STR	"Link number out of range"
#define	LINUX_EUNATCH		49
#define	LINUX_EUNATCH_STR	"Protocol driver not attached"
#define	LINUX_ENOCSI		50
#define	LINUX_ENOCSI_STR	"No CSI structure available"
#define	LINUX_EL2HLT		51
#define	LINUX_EL2HLT_STR	"Level 2 halted"
#define	LINUX_EBADE		52
#define	LINUX_EBADE_STR		"Invalid exchange"
#define	LINUX_EBADR		53
#define	LINUX_EBADR_STR		"Invalid request descriptor"
#define	LINUX_EXFULL		54
#define	LINUX_EXFULL_STR	"Exchange full"
#define	LINUX_ENOANO		55
#define	LINUX_ENOANO_STR	"No anode"
#define	LINUX_EBADRQC		56
#define	LINUX_EBADRQC_STR	"Invalid request code"
#define	LINUX_EBADSLT		57
#define	LINUX_EBADSLT_STR	"Invalid slot"
/*				58 unused */
#define	LINUX_EBFONT		59
#define	LINUX_EBFONT_STR	"Bad font file format"
#define	LINUX_ENOSTR		60
#define	LINUX_ENOSTR_STR	"Device not a stream"
#define	LINUX_ENODATA		61
#define	LINUX_ENODATA_STR	"No data available"
#define	LINUX_ETIME		62
#define	LINUX_ETIME_STR		"Timer expired"
#define	LINUX_ENOSR		63
#define	LINUX_ENOSR_STR		"Out of streams resources"
#define	LINUX_ENONET		64
#define	LINUX_ENONET_STR	"Machine is not on the network"
#define	LINUX_ENOPKG		65
#define	LINUX_ENOPKG_STR	"Package not installed"
#define	LINUX_EREMOTE		66
#define	LINUX_EREMOTE_STR	"Object is remote"
#define	LINUX_ENOLINK		67
#define	LINUX_ENOLINK_STR	"Link has been severed"
#define	LINUX_EADV		68
#define	LINUX_EADV_STR		"Advertise error"
#define	LINUX_ESRMNT		69
#define	LINUX_ESRMNT_STR	"Srmount error"
#define	LINUX_ECOMM		70
#define	LINUX_ECOMM_STR		"Communication error on send"
#define	LINUX_EPROTO		71
#define	LINUX_EPROTO_STR	"Protocol error"
#define	LINUX_EMULTIHOP		72
#define	LINUX_EMULTIHOP_STR	"Multihop attempted"
#define	LINUX_EDOTDOT		73
#define	LINUX_EDOTDOT_STR	"RFS specific error"
#define	LINUX_EBADMSG		74
#define	LINUX_EBADMSG_STR	"Bad message"
#define	LINUX_EOVERFLOW		75
#define	LINUX_EOVERFLOW_STR	"Value too large for defined data type"
#define	LINUX_ENOTUNIQ		76
#define	LINUX_ENOTUNIQ_STR	"Name not unique on network"
#define	LINUX_EBADFD		77
#define	LINUX_EBADFD_STR	"File descriptor in bad state"
#define	LINUX_EREMCHG		78
#define	LINUX_EREMCHG_STR	"Remote address changed"
#define	LINUX_ELIBACC		79
#define	LINUX_ELIBACC_STR	"Can not access a needed shared library"
#define	LINUX_ELIBBAD		80
#define	LINUX_ELIBBAD_STR	"Accessing a corrupted shared library"
#define	LINUX_ELIBSCN		81
#define	LINUX_ELIBSCN_STR	".lib section in a.out corrupted"
#define	LINUX_ELIBMAX		82
#define	LINUX_ELIBMAX_STR	"Attempting to link in too many shared libraries"
#define	LINUX_ELIBEXEC		83
#define	LINUX_ELIBEXEC_STR	"Cannot exec a shared library directly"
#define	LINUX_EILSEQ		84
#define	LINUX_EILSEQ_STR	"Invalid or incomplete multibyte or wide character"
#define	LINUX_ERESTART		85
#define	LINUX_ERESTART_STR	"Interrupted system call should be restarted"
#define	LINUX_ESTRPIPE		86
#define	LINUX_ESTRPIPE_STR	"Streams pipe error"
#define	LINUX_EUSERS		87
#define	LINUX_EUSERS_STR	"Too many users"
#define	LINUX_ENOTSOCK		88
#define	LINUX_ENOTSOCK_STR	"Socket operation on non-socket"
#define	LINUX_EDESTADDRREQ	89
#define	LINUX_EDESTADDRREQ_STR	"Destination address required"
#define	LINUX_EMSGSIZE		90
#define	LINUX_EMSGSIZE_STR	"Message too long"
#define	LINUX_EPROTOTYPE	91
#define	LINUX_EPROTOTYPE_STR	"Protocol wrong type for socket"
#define	LINUX_ENOPROTOOPT	92
#define	LINUX_ENOPROTOOPT_STR	"Protocol not available"
#define	LINUX_EPROTONOSUPPORT	93
#define	LINUX_EPROTONOSUPPORT_STR "Protocol not supported"
#define	LINUX_ESOCKTNOSUPPORT	94
#define	LINUX_ESOCKTNOSUPPORT_STR "Socket type not supported"
#define	LINUX_EOPNOTSUPP	95
#define	LINUX_EOPNOTSUPP_STR	"Operation not supported"
#define	LINUX_EPFNOSUPPORT	96
#define	LINUX_EPFNOSUPPORT_STR	"Protocol family not supported"
#define	LINUX_EAFNOSUPPORT	97
#define	LINUX_EAFNOSUPPORT_STR	"Address family not supported by protocol"
#define	LINUX_EADDRINUSE	98
#define	LINUX_EADDRINUSE_STR	"Address already in use"
#define	LINUX_EADDRNOTAVAIL	99
#define	LINUX_EADDRNOTAVAIL_STR	"Cannot assign requested address"
#define	LINUX_ENETDOWN		100
#define	LINUX_ENETDOWN_STR	"Network is down"
#define	LINUX_ENETUNREACH	101
#define	LINUX_ENETUNREACH_STR	"Network is unreachable"
#define	LINUX_ENETRESET		102
#define	LINUX_ENETRESET_STR	"Network dropped connection on reset"
#define	LINUX_ECONNABORTED	103
#define	LINUX_ECONNABORTED_STR	"Software caused connection abort"
#define	LINUX_ECONNRESET	104
#define	LINUX_ECONNRESET_STR	"Connection reset by peer"
#define	LINUX_ENOBUFS		105
#define	LINUX_ENOBUFS_STR	"No buffer space available"
#define	LINUX_EISCONN		106
#define	LINUX_EISCONN_STR	"Transport endpoint is already connected"
#define	LINUX_ENOTCONN		107
#define	LINUX_ENOTCONN_STR	"Transport endpoint is not connected"
#define	LINUX_ESHUTDOWN		108
#define	LINUX_ESHUTDOWN_STR	"Cannot send after transport endpoint shutdown"
#define	LINUX_ETOOMANYREFS	109
#define	LINUX_ETOOMANYREFS_STR	"Too many references: cannot splice"
#define	LINUX_ETIMEDOUT		110
#define	LINUX_ETIMEDOUT_STR	"Connection timed out"
#define	LINUX_ECONNREFUSED	111
#define	LINUX_ECONNREFUSED_STR	"Connection refused"
#define	LINUX_EHOSTDOWN		112
#define	LINUX_EHOSTDOWN_STR	"Host is down"
#define	LINUX_EHOSTUNREACH	113
#define	LINUX_EHOSTUNREACH_STR	"No route to host"
#define	LINUX_EALREADY		114
#define	LINUX_EALREADY_STR	"Operation already in progress"
#define	LINUX_EINPROGRESS	115
#define	LINUX_EINPROGRESS_STR	"Operation now in progress"
#define	LINUX_ESTALE		116
#define	LINUX_ESTALE_STR	"Stale file handle"
#define	LINUX_EUCLEAN		117
#define	LINUX_EUCLEAN_STR	"Structure needs cleaning"
#define	LINUX_ENOTNAM		118
#define	LINUX_ENOTNAM_STR	"Not a XENIX named type file"
#define	LINUX_ENAVAIL		119
#define	LINUX_ENAVAIL_STR	"No XENIX semaphores available"
#define	LINUX_EISNAM		120
#define	LINUX_EISNAM_STR	"Is a named type file"
#define	LINUX_EREMOTEIO		121
#define	LINUX_EREMOTEIO_STR	"Remote I/O error"
#define	LINUX_EDQUOT		122
#define	LINUX_EDQUOT_STR	"Quota exceeded"
#define	LINUX_ENOMEDIUM		123
#define	LINUX_ENOMEDIUM_STR	"No medium found"
#define	LINUX_EMEDIUMTYPE	124
#define	LINUX_EMEDIUMTYPE_STR	"Wrong medium type"
#define	LINUX_ECANCELED		125
#define	LINUX_ECANCELED_STR	"Operation canceled"
#define	LINUX_ENOKEY		126
#define	LINUX_ENOKEY_STR	"Required key not available"
#define	LINUX_EKEYEXPIRED	127
#define	LINUX_EKEYEXPIRED_STR	"Key has expired"
#define	LINUX_EKEYREVOKED	128
#define	LINUX_EKEYREVOKED_STR	"Key has been revoked"
#define	LINUX_EKEYREJECTED	129
#define	LINUX_EKEYREJECTED_STR	"Key was rejected by service"
#define	LINUX_EOWNERDEAD	130
#define	LINUX_EOWNERDEAD_STR	"Owner died"
#define	LINUX_ENOTRECOVERABLE	131
#define	LINUX_ENOTRECOVERABLE_STR "State not recoverable"
#define	LINUX_ERFKILL		132
#define	LINUX_ERFKILL_STR	"Operation not possible due to RF-kill"
#define	LINUX_EHWPOISON		133
#define	LINUX_EHWPOISON_STR	"Memory page has hardware error"

#endif	/* LIB9P_LINUX_ERRNO_H */
