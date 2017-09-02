PKG_CONFIG = pkg-config

all:
	@echo "Valid targets: gnome-ssh-askpass1 gnome-ssh-askpass2"

gnome-ssh-askpass1: gnome-ssh-askpass1.c
	$(CC) $(CFLAGS) `gnome-config --cflags gnome gnomeui` \
		gnome-ssh-askpass1.c -o gnome-ssh-askpass1 \
		`gnome-config --libs gnome gnomeui`

gnome-ssh-askpass2: gnome-ssh-askpass2.c
	$(CC) $(CFLAGS) `$(PKG_CONFIG) --cflags gtk+-2.0` \
		gnome-ssh-askpass2.c -o gnome-ssh-askpass2 \
		`$(PKG_CONFIG) --libs gtk+-2.0 x11`

gnome-ssh-askpass3: gnome-ssh-askpass2.c
	$(CC) $(CFLAGS) `$(PKG_CONFIG) --cflags gtk+-3.0` \
		gnome-ssh-askpass2.c -o gnome-ssh-askpass3 \
		`$(PKG_CONFIG) --libs gtk+-3.0 x11`

clean:
	rm -f *.o gnome-ssh-askpass gnome-ssh-askpass[123]
