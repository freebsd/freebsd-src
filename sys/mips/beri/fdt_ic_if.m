#-
# Copyright (c) 2013 SRI International
# Copyright (c) 1998-2004 Doug Rabson
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
# $FreeBSD$
#

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/bus.h>

/**
 * @defgroup FST_IC fdt_ic - KObj methods for interrupt controllers
 * @brief A set of methods required device drivers that are interrupt
 * controllers.  Derived from sys/kern/bus_if.m.
 * @{
 */
INTERFACE fdt_ic;

/**
 * @brief Allocate an interrupt resource
 *
 * This method is called by child devices of an interrupt controller to
 * allocate an interrup. The meaning of the resource-ID field varies
 * from bus to bus and is opaque to the interrupt controller. If a
 * resource was allocated and the caller did not use the RF_ACTIVE
 * to specify that it should be activated immediately, the caller is
 * responsible for calling FDT_IC_ACTIVATE_INTR() when it actually uses
 * the interupt.
 *
 * @param _dev		the interrupt-parent device of @p _child
 * @param _child	the device which is requesting an allocation
 * @param _rid		a pointer to the resource identifier
 * @param _irq		interrupt source to allocate
 * @param _flags	any extra flags to control the resource
 *			allocation - see @c RF_XXX flags in
 *			<sys/rman.h> for details
 * 
 * @returns		the interrupt which was allocated or @c NULL if no
 *			resource could be allocated
 */
METHOD struct resource * alloc_intr {
	device_t	_dev;
	device_t	_child;
	int	       *_rid;
	u_long		_irq;
	u_int		_flags;
};

/**
 * @brief Activate an interrupt
 *
 * Activate an interrupt previously allocated with FDT_IC_ALLOC_INTR().
 *
 * @param _dev		the parent device of @p _child
 * @param _r		interrupt to activate
 */
METHOD int activate_intr {
	device_t	_dev;
	struct resource *_r;
};

/**
 * @brief Deactivate an interrupt
 *
 * Deactivate a resource previously allocated with FDT_IC_ALLOC_INTR().
 *
 * @param _dev		the parent device of @p _child
 * @param _r		the interrupt to deactivate
 */
METHOD int deactivate_intr {
	device_t	_dev;
	struct resource *_r;
};

/**
 * @brief Release an interrupt
 *
 * Free an interupt allocated by the FDT_IC_ALLOC_INTR.
 *
 * @param _dev		the parent device of @p _child
 * @param _r		the resource to release
 */
METHOD int release_intr {
	device_t	_dev;
	struct resource *_res;
};

/**
 * @brief Install an interrupt handler
 *
 * This method is used to associate an interrupt handler function with
 * an irq resource. When the interrupt triggers, the function @p _intr
 * will be called with the value of @p _arg as its single
 * argument. The value returned in @p *_cookiep is used to cancel the
 * interrupt handler - the caller should save this value to use in a
 * future call to FDT_IC_TEARDOWN_INTR().
 * 
 * @param _dev		the interrupt-parent device of @p _child
 * @param _child	the device which allocated the resource
 * @param _irq		the resource representing the interrupt
 * @param _flags	a set of bits from enum intr_type specifying
 *			the class of interrupt
 * @param _intr		the function to call when the interrupt
 *			triggers
 * @param _arg		a value to use as the single argument in calls
 *			to @p _intr
 * @param _cookiep	a pointer to a location to recieve a cookie
 *			value that may be used to remove the interrupt
 *			handler
 */
METHOD int setup_intr {
	device_t	_dev;
	device_t	_child;
	struct resource *_irq;
	int		_flags;
	driver_filter_t	*_filter;
	driver_intr_t	*_intr;
	void		*_arg;
	void		**_cookiep;
};

/**
 * @brief Uninstall an interrupt handler
 *
 * This method is used to disassociate an interrupt handler function
 * with an irq resource. The value of @p _cookie must be the value
 * returned from a previous call to FDT_IC_SETUP_INTR().
 * 
 * @param _dev		the interrupt-parent device of @p _child
 * @param _child	the device which allocated the resource
 * @param _irq		the resource representing the interrupt
 * @param _cookie	the cookie value returned when the interrupt
 *			was originally registered
 */
METHOD int teardown_intr {
	device_t	_dev;
	device_t	_child;
	struct resource	*_irq;
	void		*_cookie;
};

/**
 * @brief Allow drivers to request that an interrupt be bound to a specific
 * CPU.
 * 
 * @param _dev		the interrupt-parent device of @p _child
 * @param _child	the device which allocated the resource
 * @param _irq		the resource representing the interrupt
 * @param _cpu		the CPU to bind the interrupt to
 */
METHOD int bind_intr {
	device_t	_dev;
	device_t	_child;
	struct resource *_irq;
	int		_cpu;
};

/**
 * @brief Allow drivers to specify the trigger mode and polarity
 * of the specified interrupt.
 * 
 * @param _dev		the interrupt-parent device
 * @param _irq		the interrupt number to modify
 * @param _trig		the trigger mode required
 * @param _pol		the interrupt polarity required
 */
METHOD int config_intr {
	device_t	_dev;
	int		_irq;
	enum intr_trigger _trig;
	enum intr_polarity _pol;
};

/**
 * @brief Allow drivers to associate a description with an active
 * interrupt handler.
 *
 * @param _dev		the interrupt-parent device of @p _child
 * @param _child	the device which allocated the resource
 * @param _irq		the resource representing the interrupt
 * @param _cookie	the cookie value returned when the interrupt
 *			was originally registered
 * @param _descr	the description to associate with the interrupt
 */
METHOD int describe_intr {
	device_t	_dev;
	device_t	_child;
	struct resource *_irq;
	void		*_cookie;
	const char	*_descr;
};

/**
 * @brief Notify an ic that specified child's IRQ should be remapped.
 *
 * @param _dev		the interrupt-parent device
 * @param _child	the child device
 * @param _irq		the irq number
 */
METHOD int remap_intr {
	device_t	_dev;
	device_t	_child;
	u_int		_irq;
};

/**
 * @brief Enable an IPI source.
 *
 * @param _dev		the interrupt controller
 * @param _tid		the thread ID (relative to the interrupt controller)
 *			to enable IPIs for
 * @param _ipi_irq	hardware IRQ to send IPIs to
 */
METHOD void setup_ipi {
	device_t	_dev;
	u_int		_tid;
	u_int		_irq;
};

/**
 * @brief Send an IPI to the specified thread.
 *
 * @param _dev		the interrupt controller
 * @param _tid		the thread ID (relative to the interrupt controller)
 *			to send IPIs to
 */
METHOD void send_ipi {
	device_t	_dev;
	u_int		_tid;
};

/**
 * @brief Clear the IPI on the specfied thread.  Only call with the
 * local hardware thread or interrupts may be lost!
 *
 * @param _dev		the interrupt controller
 * @param _tid		the thread ID (relative to the interrupt controller)
 *			to clear the IPI on
 */
METHOD void clear_ipi {
	device_t	_dev;
	u_int		_tid;
};
