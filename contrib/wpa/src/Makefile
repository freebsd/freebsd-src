SUBDIRS=ap common crypto drivers eapol_auth eapol_supp eap_common eap_peer eap_server l2_packet p2p pae radius rsn_supp tls utils wps
SUBDIRS += fst

all:
	for d in $(SUBDIRS); do [ -d $$d ] && $(MAKE) -C $$d; done

clean:
	$(Q)for d in $(SUBDIRS); do [ -d $$d ] && $(MAKE) -C $$d clean; done
	$(Q)rm -f *~

install:
	for d in $(SUBDIRS); do [ -d $$d ] && $(MAKE) -C $$d install; done
