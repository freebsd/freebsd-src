#!/usr/bin/env python3

'''
This is an example of using pyudev[1] alongside evdev.
[1]: https://pyudev.readthedocs.org/
'''

import functools
import pyudev

from select import select
from evdev import InputDevice

context = pyudev.Context()
monitor = pyudev.Monitor.from_netlink(context)
monitor.filter_by(subsystem='input')
monitor.start()

fds = {monitor.fileno(): monitor}
finalizers = []

while True:
    r, w, x = select(fds, [], [])

    if monitor.fileno() in r:
        r.remove(monitor.fileno())

        for udev in iter(functools.partial(monitor.poll, 0), None):
            # we're only interested in devices that have a device node
            # (e.g. /dev/input/eventX)
            if not udev.device_node:
                break

            # find the device we're interested in and add it to fds
            for name in (i['NAME'] for i in udev.ancestors if 'NAME' in i):
                # I used a virtual input device for this test - you
                # should adapt this to your needs
                if u'py-evdev-uinput' in name:
                    if udev.action == u'add':
                        print('Device added: %s' % udev)
                        fds[dev.fd] = InputDevice(udev.device_node)
                        break
                    if udev.action == u'remove':
                        print('Device removed: %s' % udev)

                        def helper():
                            global fds
                            fds = {monitor.fileno(): monitor}

                        finalizers.append(helper)
                        break

    for fd in r:
        dev = fds[fd]
        for event in dev.read():
            print(event)

    for i in range(len(finalizers)):
        finalizers.pop()()
