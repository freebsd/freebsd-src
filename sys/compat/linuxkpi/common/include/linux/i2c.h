/*-
 * Copyright (c) 2021 Beckhoff Automation GmbH & Co. KG
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _LINUX_I2C_H_
#define	_LINUX_I2C_H_

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/systm.h>

#include <linux/device.h>

#define	I2C_MAX_ADAPTER_NAME_LENGTH	32

#define	I2C_M_RD	0x0001
#define	I2C_M_NOSTART	0x0002
#define	I2C_M_STOP	0x0004

/* No need for us */
#define	I2C_FUNC_I2C			0
#define	I2C_FUNC_SMBUS_EMUL		0
#define	I2C_FUNC_SMBUS_READ_BLOCK_DATA	0
#define	I2C_FUNC_SMBUS_BLOCK_PROC_CALL	0
#define	I2C_FUNC_10BIT_ADDR		0

#define	I2C_CLASS_HWMON	0x1
#define	I2C_CLASS_DDC	0x8
#define	I2C_CLASS_SPD	0x80

struct i2c_adapter {
	struct module *owner;
	unsigned int class;

	char name[I2C_MAX_ADAPTER_NAME_LENGTH];
	struct device dev;

	const struct i2c_lock_operations *lock_ops;
	const struct i2c_algorithm *algo;
	const struct i2c_adapter_quirks *quirks;
	void *algo_data;

	int retries;
	void *data;
};

struct i2c_msg {
	uint16_t addr;
	uint16_t flags;
	uint16_t len;
	uint8_t *buf;
};

struct i2c_algorithm {
	int (*master_xfer)(struct i2c_adapter *, struct i2c_msg *, int);
	uint32_t (*functionality)(struct i2c_adapter *);
};

struct i2c_lock_operations {
	void (*lock_bus)(struct i2c_adapter *, unsigned int);
	int (*trylock_bus)(struct i2c_adapter *, unsigned int);
	void (*unlock_bus)(struct i2c_adapter *, unsigned int);
};

struct i2c_adapter_quirks {
	uint64_t flags;
	int max_num_msgs;
	uint16_t max_write_len;
	uint16_t max_read_len;
	uint16_t max_comb_1st_msg_len;
	uint16_t max_comb_2nd_msg_len;
};

#define	I2C_AQ_COMB			BIT(0)
#define	I2C_AQ_COMB_WRITE_FIRST		BIT(1)
#define	I2C_AQ_COMB_READ_SECOND		BIT(2)
#define	I2C_AQ_COMB_SAME_ADDR		BIT(3)
#define	I2C_AQ_COMB_WRITE_THEN_READ \
    (I2C_AQ_COMB | I2C_AQ_COMB_WRITE_FIRST | \
    I2C_AQ_COMB_READ_SECOND | I2C_AQ_COMB_SAME_ADDR)
#define	I2C_AQ_NO_CLK_STRETCH		BIT(4)
#define	I2C_AQ_NO_ZERO_LEN_READ		BIT(5)
#define	I2C_AQ_NO_ZERO_LEN_WRITE	BIT(6)
#define	I2C_AQ_NO_ZERO_LEN \
    (I2C_AQ_NO_ZERO_LEN_READ | I2C_AQ_NO_ZERO_LEN_WRITE)
#define	I2C_AQ_NO_REP_START		BIT(7)

int lkpi_i2c_add_adapter(struct i2c_adapter *adapter);
int lkpi_i2c_del_adapter(struct i2c_adapter *adapter);

int lkpi_i2cbb_transfer(struct i2c_adapter *adapter, struct i2c_msg *msgs, int nmsgs);

#define	i2c_add_adapter(adapter)	lkpi_i2c_add_adapter(adapter)
#define	i2c_del_adapter(adapter)	lkpi_i2c_del_adapter(adapter)

#define	i2c_get_adapter(x)	NULL
#define	i2c_put_adapter(x)

static inline int
do_i2c_transfer(struct i2c_adapter *adapter, struct i2c_msg *msgs, int nmsgs)
{
	int ret, retries;

	retries = adapter->retries == 0 ? 1 : adapter->retries;
	for (; retries != 0; retries--) {
		if (adapter->algo != NULL && adapter->algo->master_xfer != NULL)
			ret = adapter->algo->master_xfer(adapter, msgs, nmsgs);
		else
			ret = lkpi_i2cbb_transfer(adapter, msgs, nmsgs);
		if (ret != -EAGAIN)
			break;
	}

	return (ret);
}

static inline int
i2c_transfer(struct i2c_adapter *adapter, struct i2c_msg *msgs, int nmsgs)
{
	int ret;

	if (adapter->algo == NULL && adapter->algo_data == NULL)
		return (-EOPNOTSUPP);

	if (adapter->lock_ops)
		adapter->lock_ops->lock_bus(adapter, 0);

	ret = do_i2c_transfer(adapter, msgs, nmsgs);

	if (adapter->lock_ops)
		adapter->lock_ops->unlock_bus(adapter, 0);

	return (ret);
}

/* Unlocked version of i2c_transfer */
static inline int
__i2c_transfer(struct i2c_adapter *adapter, struct i2c_msg *msgs, int nmsgs)
{
	return (do_i2c_transfer(adapter, msgs, nmsgs));
}

static inline void
i2c_set_adapdata(struct i2c_adapter *adapter, void *data)
{
	adapter->data = data;
}

static inline void *
i2c_get_adapdata(struct i2c_adapter *adapter)
{
	return (adapter->data);
}

#endif	/* _LINUX_I2C_H_ */
