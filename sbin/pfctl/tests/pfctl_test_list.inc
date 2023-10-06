/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2020 Alex Richardson <arichardson@FreeBSD.org>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
 *
 * This work was supported by Innovate UK project 105694, "Digital Security by
 * Design (DSbD) Technology Platform Prototype".
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * No include guards since this file is included multiple times by pfctl_test
 * to avoid duplicating code.
 */
PFCTL_TEST(0001, "Pass with labels")
PFCTL_TEST(0002, "Block/pass")
PFCTL_TEST(0003, "Block/pass with flags")
PFCTL_TEST(0004, "Block")
PFCTL_TEST(0005, "Block with variables")
PFCTL_TEST(0006, "Variables")
PFCTL_TEST(0007, "Block/pass with return")
PFCTL_TEST(0008, "Block with address list")
PFCTL_TEST(0009, "Block with interface list")
PFCTL_TEST(0010, "Block/pass with return")
PFCTL_TEST(0011, "Block/pass ICMP")
PFCTL_TEST(0012, "Pass to subnets")
PFCTL_TEST(0013, "Pass quick")
PFCTL_TEST(0014, "Pass quick IPv6")
PFCTL_TEST(0016, "Pass with no state")
PFCTL_TEST(0018, "Address lists")
PFCTL_TEST(0019, "Lists")
PFCTL_TEST(0020, "Lists")
PFCTL_TEST(0022, "Set options")
PFCTL_TEST(0023, "Block on negated interface")
PFCTL_TEST(0024, "Variable concatenation")
PFCTL_TEST(0025, "Antispoof")
PFCTL_TEST(0026, "Block from negated interface")
PFCTL_TEST(0028, "Block with log and quick")
PFCTL_TEST(0030, "Line continuation")
PFCTL_TEST(0031, "Block policy")
PFCTL_TEST(0032, "Pass to any")
PFCTL_TEST(0034, "Pass with probability")
PFCTL_TEST(0035, "Matching on TOS")
PFCTL_TEST(0038, "Pass with user")
PFCTL_TEST(0039, "Ordered opts")
PFCTL_TEST(0040, "Block/pass")
PFCTL_TEST(0041, "Anchors")
PFCTL_TEST(0047, "Pass with labels")
PFCTL_TEST(0048, "Tables")
PFCTL_TEST(0049, "Broadcast and network modifiers")
PFCTL_TEST(0050, "Double macro set")
PFCTL_TEST(0052, "Set optimization")
PFCTL_TEST(0053, "Pass with labels")
PFCTL_TEST(0055, "Set options")
PFCTL_TEST(0056, "State opts")
PFCTL_TEST(0057, "Variables")
PFCTL_TEST(0060, "Pass from multicast")
PFCTL_TEST(0061, "Dynaddr with netmask")
PFCTL_TEST(0065, "Antispoof with labels")
PFCTL_TEST(0067, "Tags")
PFCTL_TEST(0069, "Tags")
PFCTL_TEST(0070, "Tags")
PFCTL_TEST(0071, "Tags")
PFCTL_TEST(0072, "Tags")
PFCTL_TEST(0074, "Synproxy")
PFCTL_TEST(0075, "Block quick with tags")
PFCTL_TEST(0077, "Dynaddr with netmask")
PFCTL_TEST(0078, "Table with label")
PFCTL_TEST(0079, "No-route with label")
PFCTL_TEST(0081, "Address list and table list with no-route")
PFCTL_TEST(0082, "Pass with interface, table and no-route")
PFCTL_TEST(0084, "Source track")
PFCTL_TEST(0085, "Tag macro expansion")
PFCTL_TEST(0087, "Optimization rule reordering")
PFCTL_TEST(0088, "Optimization duplicate rules handling")
PFCTL_TEST(0089, "TCP connection tracking")
PFCTL_TEST(0090, "Log opts")
PFCTL_TEST(0091, "Nested anchors")
PFCTL_TEST(0092, "Comments")
PFCTL_TEST(0094, "Address ranges")
PFCTL_TEST(0095, "Include")
PFCTL_TEST(0096, "Variables")
PFCTL_TEST(0097, "Divert-to")
PFCTL_TEST(0098, "Pass")
PFCTL_TEST(0100, "Anchor with multiple path components")
PFCTL_TEST(0101, "Prio")
PFCTL_TEST(0102, "Address lists with mixed address family")
PFCTL_TEST(0104, "Divert-to with localhost")
PFCTL_TEST(1001, "Binat")
PFCTL_TEST(1002, "Set timeout interval")
PFCTL_TEST(1003, "ALTQ")
PFCTL_TEST(1004, "ALTQ with Codel")
PFCTL_TEST(1005, "PR 231323")
PFCTL_TEST(1006, "pfctl crashes with certain fairq configurations")
PFCTL_TEST(1007, "Basic ethernet rule")
PFCTL_TEST(1008, "Ethernet rule with mask length")
PFCTL_TEST(1009, "Ethernet rule with mask")
PFCTL_TEST(1010, "POM_STICKYADDRESS test")
PFCTL_TEST(1011, "Test disabling scrub fragment reassemble")
PFCTL_TEST(1012, "Test scrub fragment reassemble is default")
PFCTL_TEST(1013, "Ethernet rule with ridentifier")
PFCTL_TEST(1014, "Ethernet rule with one label")
PFCTL_TEST(1015, "Ethernet rule with several labels")
PFCTL_TEST(1016, "Ethernet rule with ridentifier and one label")
PFCTL_TEST(1017, "Ethernet rule with ridentifier and several labels")
PFCTL_TEST(1018, "Test dynamic address mask")
