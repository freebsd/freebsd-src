#-
# Copyright (c) 2006, Sam Leffler
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#

#include <sys/malloc.h>
#include <opencrypto/cryptodev.h>

INTERFACE cryptodev;

CODE {
	static int null_freesession(device_t dev,
	    crypto_session_t crypto_session)
	{
		return 0;
	}
};

/**
 * @brief Probe to see if a crypto driver supports a session.
 *
 * The crypto framework invokes this method on each crypto driver when
 * creating a session for symmetric crypto operations to determine if
 * the driver supports the algorithms and mode requested by the
 * session.
 *
 * If the driver does not support a session with the requested
 * parameters, this function should fail with an error.
 *
 * If the driver does support a session with the requested parameters,
 * this function should return a negative value indicating the
 * priority of this driver.  These negative values should be derived
 * from one of the CRYPTODEV_PROBE_* constants in
 * <opencrypto/cryptodev.h>.
 *
 * This function's return value is similar to that used by
 * DEVICE_PROBE(9).  However, a return value of zero is not supported
 * and should not be used.
 *
 * @param dev		the crypto driver device
 * @param csp		crypto session parameters
 *
 * @retval negative	if the driver supports this session - the
 *			least negative value is used to select the
 *			driver for the session
 * @retval EINVAL	if the driver does not support the session
 * @retval positive	if some other error occurs
 */
METHOD int probesession {
	device_t	dev;
	const struct crypto_session_params *csp;
};

/**
 * @brief Initialize a new crypto session object
 *
 * Invoked by the crypto framework to initialize driver-specific data
 * for a crypto session.  The framework allocates and zeroes the
 * driver's per-session memory object prior to invoking this method.
 * The driver is able to access it's per-session memory object via
 * crypto_get_driver_session().
 *
 * @param dev		the crypto driver device
 * @param crypto_session session being initialized
 * @param csp		crypto session parameters
 *
 * @retval 0		success
 * @retval non-zero	if some kind of error occurred
 */
METHOD int newsession {
	device_t	dev;
	crypto_session_t crypto_session;
	const struct crypto_session_params *csp;
};

/**
 * @brief Destroy a crypto session object
 *
 * The crypto framework invokes this method when tearing down a crypto
 * session.  After this callback returns, the framework will explicitly
 * zero and free the drvier's per-session memory object.  If the
 * driver requires additional actions to destroy a session, it should
 * perform those in this method.  If the driver does not require
 * additional actions it does not need to provide an implementation of
 * this method.
 *
 * @param dev		the crypto driver device
 * @param crypto_session session being destroyed
 */
METHOD void freesession {
	device_t	dev;
	crypto_session_t crypto_session;
} DEFAULT null_freesession;

/**
 * @brief Perform a crypto operation
 *
 * The crypto framework invokes this method for each crypto
 * operation performed on a session.  A reference to the containing
 * session is stored as a member of 'struct cryptop'.  This routine
 * should not block, but queue the operation if necessary.
 *
 * This method may return ERESTART to indicate that any internal
 * queues are full so the operation should be queued in the crypto
 * framework and retried in the future.
 *
 * To report errors with a crypto operation, 'crp_etype' should be set
 * and the operation completed by calling 'crypto_done'.  This method
 * should then return zero.
 *
 * @param dev		the crypto driver device
 * @param op		crypto operation to perform
 * @param flags		set to CRYPTO_HINT_MORE if additional symmetric
 *			crypto operations are queued for this driver;
 *			otherwise set to zero.
 *
 * @retval 0		success
 * @retval ERESTART	internal queue is full
 */
METHOD int process {
	device_t	dev;
	struct cryptop	*op;
	int		flags;
};
