#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2017 Kristof Provost <kp@FreeBSD.org>
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

import threading
import scapy.all as sp
import sys

class Sniffer(threading.Thread):
	def __init__(self, args, check_function, recvif, timeout=3, defrag=False):
		threading.Thread.__init__(self)

		self._sem = threading.Semaphore(0)
		self._args = args
		self._timeout = timeout
		self._recvif = recvif
		self._check_function = check_function
		self._defrag = defrag
		self.correctPackets = 0

		self.start()
		if not self._sem.acquire(timeout=30):
			raise Exception("Failed to start sniffer")

	def _checkPacket(self, packet):
		ret = self._check_function(self._args, packet)
		if ret:
			self.correctPackets += 1
		return ret

	def _startedCb(self):
		self._sem.release()

	def run(self):
		self.packets = []
		if self._defrag:
			# With fragment reassembly we can't stop the sniffer after catching
			# the good packets, as those have not been reassembled. We must
			#  wait for sniffer to finish and check returned packets instead.
			self.packets = sp.sniff(session=sp.IPSession, iface=self._recvif,
				timeout=self._timeout, started_callback=self._startedCb)
			for p in self.packets:
				self._checkPacket(p)
		else:
			self.packets = sp.sniff(iface=self._recvif,
				stop_filter=self._checkPacket, timeout=self._timeout,
				started_callback=self._startedCb)
