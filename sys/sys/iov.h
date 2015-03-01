/*-
 * Copyright (c) 2013-2015 Sandvine Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _SYS_IOV_H_
#define _SYS_IOV_H_

#include <sys/ioccom.h>

#define	PF_CONFIG_NAME		"PF"
#define	VF_SCHEMA_NAME		"VF"

#define	DRIVER_CONFIG_NAME	"DRIVER"
#define	IOV_CONFIG_NAME		"IOV"

#define	TYPE_SCHEMA_NAME	"TYPE"
#define	DEFAULT_SCHEMA_NAME	"DEFAULT"
#define	REQUIRED_SCHEMA_NAME	"REQUIRED"

struct pci_iov_arg
{
	int num_vfs;
	int passthrough;
};

/*
 * Because each PF device is expected to expose a unique set of possible
 * configurations, the SR-IOV infrastructure dynamically queries the PF
 * driver for its capabilities.  These capabilities are exposed to userland
 * with a configuration schema.  The schema is exported from the kernel as a
 * packed nvlist.  See nv(3) for the details of the nvlist API.  The expected
 * format of the nvlist is:
 *
 * BASIC RULES
 *   1) All keys are case-insensitive.
 *   2) No keys that are not specified below may exist at any level of the
 *      schema.
 *   3) All keys are mandatory unless explicitly documented as optional.  If a
 *      key is mandatory then the associated value is also mandatory.
 *   4) Order of keys is irrelevant.
 *
 * TOP LEVEL
 *   1) There must be a top-level key with the name PF_CONFIG_NAME.  The value
 *      associated with this key is a nvlist that follows the device schema
 *      node format.  The parameters in this node specify the configuration
 *      parameters that may be applied to a PF.
 *   2) There must be a top-level key with the name VF_SCHEMA_NAME.  The value
 *      associated with this key is a nvlist that follows the device schema
 *      node format.  The parameters in this node specify the configuration
 *      parameters that may be applied to a VF.
 *
 * DEVICE SCHEMA NODE
 *   1) There must be a key with the name DRIVER_CONFIG_NAME.  The value
 *      associated with this key is a nvlist that follows the device/subsystem
 *      schema node format.  The parameters in this node specify the
 *      configuration parameters that are specific to a particular device
 *      driver.
 *   2) There must be a key with the name IOV_CONFIG_NAME.  The value associated
 *      with this key is an nvlist that follows the device/subsystem schema node
 *      format.  The parameters in this node specify the configuration
 *      parameters that are applied by the SR-IOV infrastructure.
 *
 * DEVICE/SUBSYSTEM SCHEMA NODE
 *   1) All keys in the device/subsystem schema node are optional.
 *   2) Each key specifies the name of a valid configuration parameter that may
 *      be applied to the device/subsystem combination specified by this node.
 *      The value associated with the key specifies the format of valid
 *      configuration values, and must be a nvlist in parameter schema node
 *      format.
 *
 * PARAMETER SCHEMA NODE
 *   1) The parameter schema node must contain a key with the name
 *      TYPE_SCHEMA_NAME.  The value associated with this key must be a string.
 *      This string specifies the type of value that the parameter specified by
 *      this node must take.  The string must have one of the following values:
 *         - "bool"     - The configuration value must be a boolean.
 *         - "mac-addr" - The configuration value must be a binary value.  In
 *                         addition, the value must be exactly 6 bytes long and
 *                         the value must not be a multicast or broadcast mac.
 *         - "uint8_t"  - The configuration value must be a integer value in
 *                         the range [0, UINT8_MAX].
 *         - "uint16_t" - The configuration value must be a integer value in
 *                         the range [0, UINT16_MAX].
 *         - "uint32_t" - The configuration value must be a integer value in
 *                         the range [0, UINT32_MAX].
 *         - "uint64_t" - The configuration value must be a integer value in
 *                         the range [0, UINT64_MAX].
 *  2) The parameter schema may contain a key with the name
 *     REQUIRED_SCHEMA_NAME.  This key is optional.  If this key is present, the
 *     value associated with it must have a boolean type.  If the value is true,
 *     then the parameter specified by this schema is a required parameter.  All
 *     valid configurations must include all required parameters.
 *  3) The parameter schema may contain a key with the name DEFAULT_SCHEMA_NAME.
 *     This key is optional.  This key must not be present if the parameter
 *     specified by this schema is required.  If this key is present, the value
 *     associated with the parent key must follow all restrictions specified by
 *     the type specified by this schema.  If a configuration does not supply a
 *     value for the parameter specified by this schema, then the kernel will
 *     apply the value associated with this key in its place.
 *
 * The following is an example of a valid schema, as printed by nvlist_dump.
 * Keys are printed followed by the type of the value in parantheses.  The
 * value is displayed following a colon.  The indentation level reflects the
 * level of nesting of nvlists.  String values are displayed between []
 * brackets.  Binary values are shown with the length of the binary value (in
 * bytes) followed by the actual binary values.
 *
 *  PF (NVLIST):
 *      IOV (NVLIST):
 *          num_vfs (NVLIST):
 *              type (STRING): [uint16_t]
 *              required (BOOL): TRUE
 *          device (NVLIST):
 *              type (STRING): [string]
 *              required (BOOL): TRUE
 *      DRIVER (NVLIST):
 *  VF (NVLIST):
 *      IOV (NVLIST):
 *          passthrough (NVLIST):
 *              type (STRING): [bool]
 *              default (BOOL): FALSE
 *      DRIVER (NVLIST):
 *          mac-addr (NVLIST):
 *              type (STRING): [mac-addr]
 *              default (BINARY): 6 000000000000
 *          vlan (NVLIST):
 *               type (STRING): [uint16_t]
 *          spoof-check (NVLIST):
 *              type (STRING): [bool]
 *              default (BOOL): TRUE
 *          allow-set-mac (NVLIST):
 *              type (STRING): [bool]
 *              default (BOOL): FALSE
 */
struct pci_iov_schema
{
	void *schema;
	size_t len;
	int error;
};

#define	IOV_CONFIG	_IOWR('p', 10, struct pci_iov_arg)
#define	IOV_DELETE	_IO('p', 11)
#define	IOV_GET_SCHEMA	_IOWR('p', 12, struct pci_iov_schema)

#endif

