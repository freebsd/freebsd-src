#!/usr/bin/env python
# encoding: utf-8

'''
evdev example - input device event monitor
'''


from sys import argv, exit
from select import select
from evdev import ecodes, InputDevice, list_devices, AbsInfo


usage = 'usage: evtest <device> [<type> <value>]'
evfmt = 'time {:<16} type {} ({}), code {:<4} ({}), value {}'
device_dir = '/dev/input/'
query_type = None
query_value = None


def select_device():
    '''Select a device from the list of accessible input devices.'''

    devices = [InputDevice(i) for i in reversed(list_devices(device_dir))]
    if not devices:
        print('error: no input devices found (do you have rw permission on /dev/input/*?)')
        exit(1)

    dev_fmt = '{0:<3} {1.fn:<20} {1.name:<35} {1.phys}'
    dev_lns = [dev_fmt.format(n, d) for n, d in enumerate(devices)]

    print('ID  {:<20} {:<35} {}'.format('Device', 'Name', 'Phys'))
    print('-' * len(max(dev_lns, key=len)))
    print('\n'.join(dev_lns))
    print('')

    choice = input('Select device [0-{}]:'.format(len(dev_lns)-1))
    return devices[int(choice)]


def print_event(e):
    if e.type == ecodes.EV_SYN:
        if e.code == ecodes.SYN_MT_REPORT:
            print('time {:<16} +++++++++ {} ++++++++'.format(e.timestamp(), ecodes.SYN[e.code]))
        else:
            print('time {:<16} --------- {} --------'.format(e.timestamp(), ecodes.SYN[e.code]))
    else:
        if e.type in ecodes.bytype:
            codename = ecodes.bytype[e.type][e.code]
        else:
            codename = '?'

        print(evfmt.format(e.timestamp(), e.type, ecodes.EV[e.type], e.code, codename, e.value))


if len(argv) == 1:
    device = select_device()

elif len(argv) == 2:
    device = InputDevice(argv[1])

elif len(argv) == 4:
    device = InputDevice(argv[1])
    query_type = argv[2]
    query_value = argv[3]
else:
    print(usage)
    exit(1)

capabs = device.capabilities(verbose=True)

print('Device name: {.name}'.format(device))
print('Device info: {.info}'.format(device))
print('Repeat settings: {}'.format(device.repeat))

if ('EV_LED', ecodes.EV_LED) in capabs:
    print('Active LEDs: {}\n'.format(','.join(i[0] for i in device.leds(True))))

print('Device capabilities:')
for type, codes in capabs.items():
    print('  Type {} {}:'.format(*type))
    for i in codes:
        # i <- ('BTN_RIGHT', 273) or (['BTN_LEFT', 'BTN_MOUSE'], 272)
        if isinstance(i[1], AbsInfo):
            print('    Code {:<4} {}:'.format(*i[0]))
            print('      {}'.format(i[1]))
        else:
            # multiple names may resolve to one value
            s = ', '.join(i[0]) if isinstance(i[0], list) else i[0]
            print('    Code {:<4} {}'.format(s, i[1]))
    print('')


print('Listening for events ...\n')
while True:
    r, w, e = select([device], [], [])

    for ev in device.read():
        print_event(ev)
