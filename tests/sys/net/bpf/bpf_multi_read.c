/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Rubicon Communications, LLC (Netgate)
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
 */

#include <err.h>
#include <stdio.h>
#include <pcap.h>
#include <unistd.h>

static void
callback(u_char *arg __unused, const struct pcap_pkthdr *hdr __unused,
    const unsigned char *bytes __unused)
{
}

int
main(int argc, const char **argv)
{
	pcap_t *pcap;
	const char *interface;
	char errbuf[PCAP_ERRBUF_SIZE] = { 0 };
	int ret;

	if (argc != 2)
		err(1, "Usage: %s <interface>\n", argv[0]);

	interface = argv[1];

	pcap = pcap_create(interface, errbuf);
	if (! pcap)
		perror("Failed to pcap interface");

	ret = pcap_set_snaplen(pcap, 86);
	if (ret != 0)
		perror("Failed to set snaplen");

	ret = pcap_set_timeout(pcap, 100);
	if (ret != 0)
		perror("Failed to set timeout");

	ret = pcap_activate(pcap);
	if (ret != 0)
		perror("Failed to activate");

	/* So we have two readers on one /dev/bpf fd */
	fork();

	printf("Interface open\n");
	pcap_loop(pcap, 0, callback, NULL);

	return (0);
}
