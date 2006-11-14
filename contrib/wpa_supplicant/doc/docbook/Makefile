all: man html pdf

FILES += wpa_background
FILES += wpa_cli
FILES += wpa_passphrase
FILES += wpa_supplicant.conf
FILES += wpa_supplicant

man:
	for i in $(FILES); do docbook2man $$i.sgml; done

html:
	for i in $(FILES); do docbook2html $$i.sgml && \
		mv index.html $$i.html; done

pdf:
	for i in $(FILES); do docbook2pdf $$i.sgml; done


clean:
	rm -f wpa_background.8 wpa_cli.8 wpa_passphrase.8 wpa_supplicant.8
	rm -f wpa_supplicant.conf.5
	rm -f manpage.links manpage.refs
	rm -f $(FILES:%=%.pdf)
	rm -f $(FILES:%=%.html)
