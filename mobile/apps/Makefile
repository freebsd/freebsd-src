# Mobile Apps Makefile
# Builds all first-party apps for uOS(m) Mobile OS

.PHONY: all clean home_screen settings terminal browser contacts messages

APPS = home_screen settings terminal browser contacts messages

all: $(APPS)

home_screen:
	$(MAKE) -C home_screen

settings:
	$(MAKE) -C settings

terminal:
	$(MAKE) -C terminal

browser:
	$(MAKE) -C browser

contacts:
	$(MAKE) -C contacts

messages:
	$(MAKE) -C messages

clean:
	$(MAKE) -C home_screen clean
	$(MAKE) -C settings clean
	$(MAKE) -C terminal clean
	$(MAKE) -C browser clean
	$(MAKE) -C contacts clean
	$(MAKE) -C messages clean