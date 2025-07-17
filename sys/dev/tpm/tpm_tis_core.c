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

#include <sys/cdefs.h>
#include "tpm20.h"
#include "tpm_if.h"

/*
 * TIS register space as defined in
 * TCG_PC_Client_Platform_TPM_Profile_PTP_2.0_r1.03_v22
 */
#define	TPM_ACCESS			0x0
#define	TPM_INT_ENABLE			0x8
#define	TPM_INT_VECTOR			0xc
#define	TPM_INT_STS			0x10
#define	TPM_INTF_CAPS			0x14
#define	TPM_STS				0x18
#define	TPM_DATA_FIFO			0x24
#define	TPM_INTF_ID			0x30
#define	TPM_XDATA_FIFO			0x80
#define	TPM_DID_VID			0xF00
#define	TPM_RID				0xF04

#define	TPM_ACCESS_LOC_REQ		BIT(1)
#define	TPM_ACCESS_LOC_Seize		BIT(3)
#define	TPM_ACCESS_LOC_ACTIVE		BIT(5)
#define	TPM_ACCESS_LOC_RELINQUISH	BIT(5)
#define	TPM_ACCESS_VALID		BIT(7)

#define	TPM_INT_ENABLE_GLOBAL_ENABLE	BIT(31)
#define	TPM_INT_ENABLE_CMD_RDY		BIT(7)
#define	TPM_INT_ENABLE_LOC_CHANGE	BIT(2)
#define	TPM_INT_ENABLE_STS_VALID	BIT(1)
#define	TPM_INT_ENABLE_DATA_AVAIL	BIT(0)

#define	TPM_INT_STS_CMD_RDY		BIT(7)
#define	TPM_INT_STS_LOC_CHANGE		BIT(2)
#define	TPM_INT_STS_VALID		BIT(1)
#define	TPM_INT_STS_DATA_AVAIL		BIT(0)

#define	TPM_INTF_CAPS_VERSION		0x70000000
#define	TPM_INTF_CAPS_TPM20		0x30000000

#define	TPM_STS_VALID			BIT(7)
#define	TPM_STS_CMD_RDY			BIT(6)
#define	TPM_STS_CMD_START		BIT(5)
#define	TPM_STS_DATA_AVAIL		BIT(4)
#define	TPM_STS_DATA_EXPECTED		BIT(3)
#define	TPM_STS_BURST_MASK		0xFFFF00
#define	TPM_STS_BURST_OFFSET		0x8

static int tpmtis_transmit(device_t dev, size_t length);

static int tpmtis_detach(device_t dev);

static void tpmtis_intr_handler(void *arg);

static void tpmtis_setup_intr(struct tpm_sc *sc);

static bool tpmtis_read_bytes(struct tpm_sc *sc, size_t count, uint8_t *buf);
static bool tpmtis_write_bytes(struct tpm_sc *sc, size_t count, uint8_t *buf);
static bool tpmtis_request_locality(struct tpm_sc *sc, int locality);
static void tpmtis_relinquish_locality(struct tpm_sc *sc);
static bool tpmtis_go_ready(struct tpm_sc *sc);

static bool tpm_wait_for_u32(struct tpm_sc *sc, bus_size_t off,
    uint32_t mask, uint32_t val, int32_t timeout);

static uint16_t tpmtis_wait_for_burst(struct tpm_sc *sc);

int
tpmtis_attach(device_t dev)
{
	struct tpm_sc *sc;
	int result;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->intr_type = -1;

	sx_init(&sc->dev_lock, "TPM driver lock");
	sc->buf = malloc(TPM_BUFSIZE, M_TPM20, M_WAITOK);

	sc->irq_rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->irq_rid,
		    RF_ACTIVE | RF_SHAREABLE);
	if (sc->irq_res == NULL)
		goto skip_irq;

	result = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
		    NULL, tpmtis_intr_handler, sc, &sc->intr_cookie);
	if (result != 0) {
		bus_release_resource(dev, SYS_RES_IRQ, sc->irq_rid, sc->irq_res);
		goto skip_irq;
	}
	tpmtis_setup_intr(sc);

skip_irq:
	result = tpm20_init(sc);
	if (result != 0)
		tpmtis_detach(dev);

	return (result);
}

static int
tpmtis_detach(device_t dev)
{
	struct tpm_sc *sc;

	sc = device_get_softc(dev);
	tpm20_release(sc);

	if (sc->intr_cookie != NULL)
		bus_teardown_intr(dev, sc->irq_res, sc->intr_cookie);

	if (sc->irq_res != NULL)
		bus_release_resource(dev, SYS_RES_IRQ,
		    sc->irq_rid, sc->irq_res);

	if (sc->mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    sc->mem_rid, sc->mem_res);

	return (0);
}

/*
 * Test if the advertisted interrupt actually works.
 * This sends a simple command. (GetRandom)
 * Interrupts are then enabled in the handler.
 */
static void
tpmtis_test_intr(struct tpm_sc *sc)
{
	uint8_t cmd[] = {
		0x80, 0x01,		/* TPM_ST_NO_SESSIONS tag*/
		0x00, 0x00, 0x00, 0x0c,	/* cmd length */
		0x00, 0x00, 0x01, 0x7b,	/* cmd TPM_CC_GetRandom */
		0x00, 0x01 		/* number of bytes requested */
	};

	sx_xlock(&sc->dev_lock);
	memcpy(sc->buf, cmd, sizeof(cmd));
	tpmtis_transmit(sc->dev, sizeof(cmd));
	sc->pending_data_length = 0;
	sx_xunlock(&sc->dev_lock);
}

static void
tpmtis_setup_intr(struct tpm_sc *sc)
{
	uint32_t reg;
	uint8_t irq;

	irq = bus_get_resource_start(sc->dev, SYS_RES_IRQ, sc->irq_rid);

	/*
	 * SIRQ has to be between 1 - 15.
	 * I found a system with ACPI table that reported a value of 0x2d.
	 * An attempt to use such value resulted in an interrupt storm.
	 */
	if (irq == 0 || irq > 0xF)
		return;

	if(!tpmtis_request_locality(sc, 0))
		sc->interrupts = false;

	TPM_WRITE_1(sc->dev, TPM_INT_VECTOR, irq);

	/* Clear all pending interrupts. */
	reg = TPM_READ_4(sc->dev, TPM_INT_STS);
	TPM_WRITE_4(sc->dev, TPM_INT_STS, reg);

	reg = TPM_READ_4(sc->dev, TPM_INT_ENABLE);
	reg |= TPM_INT_ENABLE_GLOBAL_ENABLE |
	    TPM_INT_ENABLE_DATA_AVAIL |
	    TPM_INT_ENABLE_LOC_CHANGE |
	    TPM_INT_ENABLE_CMD_RDY |
	    TPM_INT_ENABLE_STS_VALID;
	TPM_WRITE_4(sc->dev, TPM_INT_ENABLE, reg);

	tpmtis_relinquish_locality(sc);
	tpmtis_test_intr(sc);
}

static void
tpmtis_intr_handler(void *arg)
{
	struct tpm_sc *sc;
	uint32_t status;

	sc = (struct tpm_sc *)arg;
	status = TPM_READ_4(sc->dev, TPM_INT_STS);

	TPM_WRITE_4(sc->dev, TPM_INT_STS, status);

	/* Check for stray interrupts. */
	if (sc->intr_type == -1 || (sc->intr_type & status) == 0)
		return;

	sc->interrupts = true;
	wakeup(sc);
}

static bool
tpm_wait_for_u32(struct tpm_sc *sc, bus_size_t off, uint32_t mask, uint32_t val,
    int32_t timeout)
{

	/* Check for condition */
	if ((TPM_READ_4(sc->dev, off) & mask) == val)
		return (true);

	/* If interrupts are enabled sleep for timeout duration */
	if(sc->interrupts && sc->intr_type != -1) {
		tsleep(sc, PWAIT, "TPM WITH INTERRUPTS", timeout / tick);

		sc->intr_type = -1;
		return ((TPM_READ_4(sc->dev, off) & mask) == val);
	}

	/* If we don't have interrupts poll the device every tick */
	while (timeout > 0) {
		if ((TPM_READ_4(sc->dev, off) & mask) == val)
			return (true);

		pause("TPM POLLING", 1);
		timeout -= tick;
	}
	return (false);
}

static uint16_t
tpmtis_wait_for_burst(struct tpm_sc *sc)
{
	int timeout;
	uint16_t burst_count;

	timeout = TPM_TIMEOUT_A;

	while (timeout-- > 0) {
		burst_count = (TPM_READ_4(sc->dev, TPM_STS) & TPM_STS_BURST_MASK) >>
		    TPM_STS_BURST_OFFSET;
		if (burst_count > 0)
			break;

		DELAY(1);
	}
	return (burst_count);
}

static bool
tpmtis_read_bytes(struct tpm_sc *sc, size_t count, uint8_t *buf)
{
	uint16_t burst_count;

	while (count > 0) {
		burst_count = tpmtis_wait_for_burst(sc);
		if (burst_count == 0)
			return (false);

		burst_count = MIN(burst_count, count);
		count -= burst_count;

		while (burst_count-- > 0)
			*buf++ = TPM_READ_1(sc->dev, TPM_DATA_FIFO);
	}

	return (true);
}

static bool
tpmtis_write_bytes(struct tpm_sc *sc, size_t count, uint8_t *buf)
{
	uint16_t burst_count;

	while (count > 0) {
		burst_count = tpmtis_wait_for_burst(sc);
		if (burst_count == 0)
			return (false);

		burst_count = MIN(burst_count, count);
		count -= burst_count;

		while (burst_count-- > 0)
			TPM_WRITE_1(sc->dev, TPM_DATA_FIFO, *buf++);
	}

	return (true);
}

static bool
tpmtis_request_locality(struct tpm_sc *sc, int locality)
{
	uint8_t mask;
	int timeout;

	/* Currently we only support Locality 0 */
	if (locality != 0)
		return (false);

	mask = TPM_ACCESS_LOC_ACTIVE | TPM_ACCESS_VALID;
	timeout = TPM_TIMEOUT_A;
	sc->intr_type = TPM_INT_STS_LOC_CHANGE;

	TPM_WRITE_1(sc->dev, TPM_ACCESS, TPM_ACCESS_LOC_REQ);
	TPM_WRITE_BARRIER(sc->dev, TPM_ACCESS, 1);
	if(sc->interrupts) {
		tsleep(sc, PWAIT, "TPMLOCREQUEST with INTR", timeout / tick);
		return ((TPM_READ_1(sc->dev, TPM_ACCESS) & mask) == mask);
	} else  {
		while(timeout > 0) {
			if ((TPM_READ_1(sc->dev, TPM_ACCESS) & mask) == mask)
				return (true);

			pause("TPMLOCREQUEST POLLING", 1);
			timeout -= tick;
		}
	}

	return (false);
}

static void
tpmtis_relinquish_locality(struct tpm_sc *sc)
{

	/*
	 * Interrupts can only be cleared when a locality is active.
	 * Clear them now in case interrupt handler didn't make it in time.
	 */
	if(sc->interrupts)
		AND4(sc, TPM_INT_STS, TPM_READ_4(sc->dev, TPM_INT_STS));

	OR1(sc, TPM_ACCESS, TPM_ACCESS_LOC_RELINQUISH);
}

static bool
tpmtis_go_ready(struct tpm_sc *sc)
{
	uint32_t mask;

	mask = TPM_STS_CMD_RDY;
	sc->intr_type = TPM_INT_STS_CMD_RDY;

	TPM_WRITE_4(sc->dev, TPM_STS, TPM_STS_CMD_RDY);
	TPM_WRITE_BARRIER(sc->dev, TPM_STS, 4);
	if (!tpm_wait_for_u32(sc, TPM_STS, mask, mask, TPM_TIMEOUT_B))
		return (false);

	return (true);
}

static int
tpmtis_transmit(device_t dev, size_t length)
{
	struct tpm_sc *sc;
	size_t bytes_available;
	uint32_t mask, curr_cmd;
	int timeout;

	sc = device_get_softc(dev);
	sx_assert(&sc->dev_lock, SA_XLOCKED);

	if (!tpmtis_request_locality(sc, 0)) {
		device_printf(dev,
		    "Failed to obtain locality\n");
		return (EIO);
	}
	if (!tpmtis_go_ready(sc)) {
		device_printf(dev,
		    "Failed to switch to ready state\n");
		return (EIO);
	}
	if (!tpmtis_write_bytes(sc, length, sc->buf)) {
		device_printf(dev,
		    "Failed to write cmd to device\n");
		return (EIO);
	}

	mask = TPM_STS_VALID;
	sc->intr_type = TPM_INT_STS_VALID;
	if (!tpm_wait_for_u32(sc, TPM_STS, mask, mask, TPM_TIMEOUT_C)) {
		device_printf(dev,
		    "Timeout while waiting for valid bit\n");
		return (EIO);
	}
	if (TPM_READ_4(dev, TPM_STS) & TPM_STS_DATA_EXPECTED) {
		device_printf(dev,
		    "Device expects more data even though we already"
		    " sent everything we had\n");
		return (EIO);
	}

	/*
	 * Calculate timeout for current command.
	 * Command code is passed in bytes 6-10.
	 */
	curr_cmd = be32toh(*(uint32_t *) (&sc->buf[6]));
	timeout = tpm20_get_timeout(curr_cmd);

	TPM_WRITE_4(dev, TPM_STS, TPM_STS_CMD_START);
	TPM_WRITE_BARRIER(dev, TPM_STS, 4);

	mask = TPM_STS_DATA_AVAIL | TPM_STS_VALID;
	sc->intr_type = TPM_INT_STS_DATA_AVAIL;
	if (!tpm_wait_for_u32(sc, TPM_STS, mask, mask, timeout)) {
		device_printf(dev,
		    "Timeout while waiting for device to process cmd\n");
		/*
		 * Switching to ready state also cancels processing
		 * current command
		 */
		if (!tpmtis_go_ready(sc))
			return (EIO);

		/*
		 * After canceling a command we should get a response,
		 * check if there is one.
		 */
		sc->intr_type = TPM_INT_STS_DATA_AVAIL;
		if (!tpm_wait_for_u32(sc, TPM_STS, mask, mask, TPM_TIMEOUT_C))
			return (EIO);
	}
	/* Read response header. Length is passed in bytes 2 - 6. */
	if(!tpmtis_read_bytes(sc, TPM_HEADER_SIZE, sc->buf)) {
		device_printf(dev,
		    "Failed to read response header\n");
		return (EIO);
	}
	bytes_available = be32toh(*(uint32_t *) (&sc->buf[2]));

	if (bytes_available > TPM_BUFSIZE || bytes_available < TPM_HEADER_SIZE) {
		device_printf(dev,
		    "Incorrect response size: %zu\n",
		    bytes_available);
		return (EIO);
	}
	if(!tpmtis_read_bytes(sc, bytes_available - TPM_HEADER_SIZE,
	    &sc->buf[TPM_HEADER_SIZE])) {
		device_printf(dev,
		    "Failed to read response\n");
		return (EIO);
	}
	tpmtis_relinquish_locality(sc);
	sc->pending_data_length = bytes_available;
	sc->total_length = bytes_available;

	return (0);
}

/* ACPI Driver */
static device_method_t tpmtis_methods[] = {
	DEVMETHOD(device_attach,	tpmtis_attach),
	DEVMETHOD(device_detach,	tpmtis_detach),
	DEVMETHOD(device_shutdown,	tpm20_shutdown),
	DEVMETHOD(device_suspend,	tpm20_suspend),
	DEVMETHOD(tpm_transmit,		tpmtis_transmit),
	DEVMETHOD_END
};

DEFINE_CLASS_0(tpmtis, tpmtis_driver, tpmtis_methods, sizeof(struct tpm_sc));
