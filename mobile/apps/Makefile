# Mobile Apps Makefile
# Builds all first-party apps for uOS(m) Mobile OS

.PHONY: all clean home_screen settings terminal browser contacts messages calculator notes

APPS = home_screen settings terminal browser contacts messages calculator notes

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

calculator:
	$(MAKE) -C calculator

notes:
	$(MAKE) -C notes

clean:
	$(MAKE) -C home_screen clean
	$(MAKE) -C settings clean
	$(MAKE) -C terminal clean
	$(MAKE) -C browser clean
	$(MAKE) -C contacts clean
	$(MAKE) -C messages clean
	$(MAKE) -C calculator clean
	$(MAKE) -C notes clean