/*-
 * Copyright (c) 2018 Stormshield.
 * Copyright (c) 2018 Semihalf.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/random.h>
#include <dev/random/randomdev.h>

#include <machine/atomic.h>

#include "tpm20.h"

#define TPM_HARVEST_SIZE     16

#define	TPM2_ST_NO_SESSIONS	0x8001
#define	TPM2_RC_SUCCESS		0x0000
#define	TPM2_RC_INITIALIZE	0x0100
#define	TPM2_RC_TESTING		0x090a
#define	TPM2_RC_RETRY		0x0922

#define	TPM2_SU_CLEAR		0x0000
#define	TPM2_SU_STATE		0x0001

#define	TPM2_RETRY_INITIAL_MS	20
#define	TPM2_RETRY_MAX_MS	(TPM_TIMEOUT_B / 1000)
/*
 * Perform a harvest every 10 seconds.
 * Since discrete TPMs are painfully slow
 * we don't want to execute this too often
 * as the chip is likely to be used by others too.
 */
#define TPM_HARVEST_INTERVAL 10

MALLOC_DEFINE(M_TPM20, "tpm_buffer", "buffer for tpm 2.0 driver");

#if defined TPM_HARVEST || defined RANDOM_ENABLE_TPM
static void tpm20_harvest(void *arg, int unused);
#endif
static int  tpm20_command(device_t, uint32_t, uint16_t, uint32_t,
    const char *);
static int  tpm20_restart(device_t dev, bool clear);
static int  tpm20_save_state(device_t dev, bool suspend);

static d_open_t		tpm20_open;
static d_close_t	tpm20_close;
static d_read_t		tpm20_read;
static d_write_t	tpm20_write;
static d_ioctl_t	tpm20_ioctl;

static struct cdevsw tpm20_cdevsw = {
	.d_version = D_VERSION,
	.d_open = tpm20_open,
	.d_close = tpm20_close,
	.d_read = tpm20_read,
	.d_write = tpm20_write,
	.d_ioctl = tpm20_ioctl,
	.d_name = "tpm20",
};

int
tpm20_read(struct cdev *dev, struct uio *uio, int flags)
{
	struct tpm_sc *sc;
	struct tpm_priv *priv;
	size_t bytes_to_transfer;
	size_t offset;
	ssize_t resid;
	int result;

	sc = (struct tpm_sc *)dev->si_drv1;
	result = devfs_get_cdevpriv((void **)&priv);
	if (result != 0)
		return (result);

	sx_xlock(&priv->io_lock);
	sx_xlock(&sc->dev_lock);
	if (atomic_load_bool(&sc->dying)) {
		result = ENXIO;
		goto out_locked;
	}
	if (sc->suspended) {
		result = EBUSY;
		goto out_locked;
	}
	offset = priv->offset;
	bytes_to_transfer = MIN(priv->len, uio->uio_resid);
	sx_xunlock(&sc->dev_lock);

	if (bytes_to_transfer > 0) {
		resid = uio->uio_resid;
		result = uiomove((caddr_t)priv->buf + offset,
		    (int)bytes_to_transfer, uio);
		bytes_to_transfer = resid - uio->uio_resid;
		priv->offset += bytes_to_transfer;
		priv->len -= bytes_to_transfer;
	} else {
		result = 0;
	}
	sx_xunlock(&priv->io_lock);
	return (result);

out_locked:
	sx_xunlock(&sc->dev_lock);
	sx_xunlock(&priv->io_lock);
	return (result);
}

int
tpm20_write(struct cdev *dev, struct uio *uio, int flags)
{
	struct tpm_sc *sc;
	struct tpm_priv *priv;
	uint8_t *command;
	size_t byte_count;
	int result;

	sc = (struct tpm_sc *)dev->si_drv1;
	result = devfs_get_cdevpriv((void **)&priv);
	if (result != 0)
		return (result);

	byte_count = uio->uio_resid;
	if (byte_count < TPM_HEADER_SIZE) {
		device_printf(sc->dev,
		    "Requested transfer is too small\n");
		return (EINVAL);
	}

	if (byte_count > TPM_BUFSIZE) {
		device_printf(sc->dev,
		    "Requested transfer is too large\n");
		return (E2BIG);
	}

	command = malloc(byte_count, M_TPM20, M_WAITOK);
	sx_xlock(&priv->io_lock);
	result = uiomove(command, byte_count, uio);
	if (result != 0)
		goto out_priv;

	sx_xlock(&sc->dev_lock);
	if (atomic_load_bool(&sc->dying)) {
		result = ENXIO;
		goto out;
	}
	if (sc->suspended) {
		result = EBUSY;
		goto out;
	}

	memcpy(priv->buf, command, byte_count);
	result = TPM_TRANSMIT(sc->dev, priv, byte_count);

out:
	sx_xunlock(&sc->dev_lock);
	if (result != 0)
		uio->uio_resid = byte_count;
out_priv:
	sx_xunlock(&priv->io_lock);
	free(command, M_TPM20);
	return (result);
}

static struct tpm_priv *
tpm20_priv_alloc(void)
{
	struct tpm_priv *priv;

	priv = malloc(sizeof (*priv), M_TPM20, M_WAITOK | M_ZERO);
	sx_init(&priv->io_lock, "TPM per-open I/O lock");
	return (priv);
}

static void
tpm20_priv_dtor(void *data)
{
	struct tpm_priv *priv = data;

	sx_destroy(&priv->io_lock);
	free(priv, M_TPM20);
}

int
tpm20_open(struct cdev *dev, int flag, int mode, struct thread *td)
{
	struct tpm_sc *sc;
	struct tpm_priv *priv;
	int error;

	sc = (struct tpm_sc *)dev->si_drv1;
	sx_xlock(&sc->dev_lock);
	if (atomic_load_bool(&sc->dying)) {
		error = ENXIO;
		goto out;
	}
	if (sc->suspended) {
		error = EBUSY;
		goto out;
	}
	priv = tpm20_priv_alloc();
	error = devfs_set_cdevpriv(priv, tpm20_priv_dtor);
	if (error != 0)
		tpm20_priv_dtor(priv);

out:
	sx_xunlock(&sc->dev_lock);
	return (error);
}

int
tpm20_close(struct cdev *dev, int flag, int mode, struct thread *td)
{

	return (0);
}

int
tpm20_ioctl(struct cdev *dev, u_long cmd, caddr_t data,
    int flags, struct thread *td)
{

	return (ENOTTY);
}

#if defined TPM_HARVEST || defined RANDOM_ENABLE_TPM
static const struct random_source random_tpm = {
	.rs_ident = "TPM",
	.rs_source = RANDOM_PURE_TPM,
};
#endif

int
tpm20_init(struct tpm_sc *sc)
{
	struct make_dev_args args;
	int result;

	atomic_store_bool(&sc->dying, false);
	sc->suspended = false;
	sc->internal_priv = tpm20_priv_alloc();

	make_dev_args_init(&args);
	args.mda_devsw = &tpm20_cdevsw;
	args.mda_uid = UID_ROOT;
	args.mda_gid = GID_WHEEL;
	args.mda_mode = TPM_CDEV_PERM_FLAG;
	args.mda_si_drv1 = sc;
	result = make_dev_s(&args, &sc->sc_cdev, TPM_CDEV_NAME);
	if (result != 0)
		return (result);

#if defined TPM_HARVEST || defined RANDOM_ENABLE_TPM
	random_source_register(&random_tpm);
	TIMEOUT_TASK_INIT(taskqueue_thread, &sc->harvest_task, 0,
	    tpm20_harvest, sc);
	taskqueue_enqueue_timeout(taskqueue_thread, &sc->harvest_task, 0);
#endif
	sc->common_initialized = true;

	return (result);

}

void
tpm20_release(struct tpm_sc *sc)
{

	/* Publish teardown so a retrying command stops using the device. */
	atomic_store_bool(&sc->dying, true);
	sx_xlock(&sc->dev_lock);
	sx_xunlock(&sc->dev_lock);

	/* Stop and drain character-device methods before freeing their state. */
	if (sc->sc_cdev != NULL) {
		destroy_dev(sc->sc_cdev);
		sc->sc_cdev = NULL;
	}
	if (!sc->common_initialized)
		goto out;
#if defined TPM_HARVEST || defined RANDOM_ENABLE_TPM
	taskqueue_drain_timeout(taskqueue_thread, &sc->harvest_task);
	random_source_deregister(&random_tpm);
#endif
	sc->common_initialized = false;
out:
	if (sc->internal_priv != NULL) {
		tpm20_priv_dtor(sc->internal_priv);
		sc->internal_priv = NULL;
	}
	sx_destroy(&sc->dev_lock);
}

int
tpm20_resume(device_t dev)
{
	struct tpm_sc *sc;
	int error;

	sc = device_get_softc(dev);
	sx_xlock(&sc->dev_lock);
	if (atomic_load_bool(&sc->dying)) {
		error = ENXIO;
		goto out;
	}
	error = tpm20_restart(dev, false);
	if (error == 0)
		sc->suspended = false;
out:
	sx_xunlock(&sc->dev_lock);
	if (error != 0)
		return (error);

#if defined TPM_HARVEST || defined RANDOM_ENABLE_TPM
	taskqueue_enqueue_timeout(taskqueue_thread, &sc->harvest_task,
	    hz * TPM_HARVEST_INTERVAL);
#endif
	return (0);
}

int
tpm20_suspend(device_t dev)
{
	struct tpm_sc *sc;
	int error;

	sc = device_get_softc(dev);
#if defined TPM_HARVEST || defined RANDOM_ENABLE_TPM
	taskqueue_drain_timeout(taskqueue_thread, &sc->harvest_task);
#endif
	sx_xlock(&sc->dev_lock);
	if (atomic_load_bool(&sc->dying)) {
		error = ENXIO;
		goto out;
	}
	if (sc->suspended) {
		error = 0;
		goto out;
	}
	error = tpm20_save_state(dev, true);
	if (error == 0)
		sc->suspended = true;
out:
	sx_xunlock(&sc->dev_lock);
#if defined TPM_HARVEST || defined RANDOM_ENABLE_TPM
	if (error != 0)
		taskqueue_enqueue_timeout(taskqueue_thread, &sc->harvest_task,
		    hz * TPM_HARVEST_INTERVAL);
#endif
	return (error);
}

int
tpm20_shutdown(device_t dev)
{
	struct tpm_sc *sc;
	int error;

	sc = device_get_softc(dev);
	sx_xlock(&sc->dev_lock);
	if (atomic_load_bool(&sc->dying))
		error = ENXIO;
	else
		error = tpm20_save_state(dev, false);
	sx_xunlock(&sc->dev_lock);
	return (error);
}

#if defined TPM_HARVEST || defined RANDOM_ENABLE_TPM
/*
 * Get TPM_HARVEST_SIZE random bytes and add them
 * into system entropy pool.
 */
static void
tpm20_harvest(void *arg, int unused)
{
	struct tpm_sc *sc;
	struct tpm_priv *priv;
	unsigned char entropy[TPM_HARVEST_SIZE];
	uint16_t entropy_size;
	int result;
	uint8_t cmd[] = {
		0x80, 0x01,		/* TPM_ST_NO_SESSIONS tag*/
		0x00, 0x00, 0x00, 0x0c,	/* cmd length */
		0x00, 0x00, 0x01, 0x7b,	/* cmd TPM_CC_GetRandom */
		0x00, TPM_HARVEST_SIZE 	/* number of bytes requested */
	};

	sc = arg;
	sx_xlock(&sc->dev_lock);
	if (atomic_load_bool(&sc->dying) || sc->suspended) {
		sx_xunlock(&sc->dev_lock);
		return;
	}

	priv = sc->internal_priv;
	memcpy(priv->buf, cmd, sizeof(cmd));

	entropy_size = 0;
	result = TPM_TRANSMIT(sc->dev, priv, sizeof(cmd));
	if (result == 0) {
		/* The byte count is placed immediately after the header. */
		entropy_size = (uint16_t)priv->buf[TPM_HEADER_SIZE + 1];
		if (entropy_size > 0) {
			entropy_size = MIN(entropy_size, TPM_HARVEST_SIZE);
			memcpy(entropy,
			    priv->buf + TPM_HEADER_SIZE + sizeof(uint16_t),
			    entropy_size);
		}
	}
	taskqueue_enqueue_timeout(taskqueue_thread, &sc->harvest_task,
	    hz * TPM_HARVEST_INTERVAL);

	sx_xunlock(&sc->dev_lock);
	if (entropy_size > 0)
		random_harvest_queue(entropy, entropy_size, RANDOM_PURE_TPM);
}
#endif	/* TPM_HARVEST */

/*
 * Send a TPM 2.0 command whose successful response contains only a header.
 */
static int
tpm20_command(device_t dev, uint32_t command, uint16_t parameter,
    uint32_t alternate_rc, const char *name)
{
	struct tpm_priv *priv;
	struct tpm_sc *sc;
	uint32_t response_rc, response_size;
	uint8_t cmd[12];
	int delay_ms, error;

	sc = device_get_softc(dev);
	if (sc == NULL)
		return (ENXIO);

	sx_assert(&sc->dev_lock, SA_XLOCKED);
	if (atomic_load_bool(&sc->dying))
		return (ENXIO);
	priv = sc->internal_priv;

	be16enc(cmd, TPM2_ST_NO_SESSIONS);
	be32enc(cmd + 2, sizeof(cmd));
	be32enc(cmd + 6, command);
	be16enc(cmd + 10, parameter);

	delay_ms = TPM2_RETRY_INITIAL_MS;
	for (;;) {
		if (atomic_load_bool(&sc->dying))
			return (ENXIO);
		memcpy(priv->buf, cmd, sizeof(cmd));
		error = TPM_TRANSMIT(sc->dev, priv, sizeof(cmd));
		if (error != 0)
			break;
		if (priv->len < TPM_HEADER_SIZE) {
			error = EPROTO;
			break;
		}

		response_size = be32dec(priv->buf + 2);
		response_rc = be32dec(priv->buf + 6);
		if (be16dec(priv->buf) != TPM2_ST_NO_SESSIONS ||
		    response_size != TPM_HEADER_SIZE ||
		    priv->len != response_size) {
			error = EPROTO;
			break;
		}
		if (response_rc != TPM2_RC_RETRY &&
		    response_rc != TPM2_RC_TESTING)
			break;
		if (atomic_load_bool(&sc->dying))
			return (ENXIO);
		if (delay_ms > TPM2_RETRY_MAX_MS)
			break;
		pause("tpm2retry", MAX(hz * delay_ms / 1000, 1));
		delay_ms *= 2;
	}
	if (error != 0) {
		device_printf(dev, "%s command failed: %d\n", name, error);
		return (error);
	}
	if (response_rc != TPM2_RC_SUCCESS && response_rc != alternate_rc) {
		device_printf(dev, "%s failed: TPM error 0x%x\n", name,
		    response_rc);
		return (EIO);
	}
	return (0);
}

static int
tpm20_restart(device_t dev, bool clear)
{
	uint16_t startup_type;

	startup_type = clear ? TPM2_SU_CLEAR : TPM2_SU_STATE;
	return (tpm20_command(dev, TPM_CC_Startup, startup_type,
	    TPM2_RC_INITIALIZE, "Startup"));
}

static int
tpm20_save_state(device_t dev, bool suspend)
{
	uint16_t shutdown_type;

	shutdown_type = suspend ? TPM2_SU_STATE : TPM2_SU_CLEAR;
	return (tpm20_command(dev, TPM_CC_Shutdown, shutdown_type,
	    TPM2_RC_SUCCESS, "Shutdown"));
}

int32_t
tpm20_get_timeout(uint32_t command)
{
	int32_t timeout;

	switch (command) {
		case TPM_CC_CreatePrimary:
		case TPM_CC_Create:
		case TPM_CC_CreateLoaded:
			timeout = TPM_TIMEOUT_LONG;
			break;
		case TPM_CC_SequenceComplete:
		case TPM_CC_Startup:
		case TPM_CC_SequenceUpdate:
		case TPM_CC_GetCapability:
		case TPM_CC_PCR_Extend:
		case TPM_CC_EventSequenceComplete:
		case TPM_CC_HashSequenceStart:
			timeout = TPM_TIMEOUT_C;
			break;
		default:
			timeout = TPM_TIMEOUT_B;
			break;
	}
	return timeout;
}
