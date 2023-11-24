
import threading
import logging
logging.getLogger("scapy").setLevel(logging.CRITICAL)
import scapy.all as sp

class Sniffer(threading.Thread):
	def __init__(self, args, check_function):
		threading.Thread.__init__(self)

		self._args = args
		self._recvif = args.recvif[0]
		self._check_function = check_function
		self.foundCorrectPacket = False
		self._endme = False

		self.start()

	def _checkPacket(self, packet):
		ret = self._check_function(self._args, packet)
		if ret:
			self.foundCorrectPacket = True
		return ret

	def setEnd(self):
		self._endme = True

	def stopFilter(self, pkt):
		if pkt is not None:
			self._checkPacket(pkt)
		if self.foundCorrectPacket or self._endme:
			return True
		else:
			return False

	def run(self):
		while True:
			self.packets = sp.sniff(iface=self._recvif, store=False,
				stop_filter=self.stopFilter, timeout=90)
			if self.stopFilter(None):
				break
