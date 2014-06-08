# encoding: utf-8

from select import select
from pytest import raises

from evdev import uinput, ecodes, events, device, util


uinput_options = {
    'name'      : 'test-py-evdev-uinput',
    'bustype'   : ecodes.BUS_USB,
    'vendor'    : 0x1100,
    'product'   : 0x2200,
    'version'   : 0x3300,
}


def pytest_funcarg__c(request):
    return uinput_options.copy()


def device_exists(bustype, vendor, product, version):
    match = 'I: Bus=%04hx Vendor=%04hx Product=%04hx Version=%04hx' % \
            (bustype, vendor, product, version)

    for line in open('/proc/bus/input/devices'):
        if line.strip() == match: return True

    return False


def test_open(c):
    ui = uinput.UInput(**c)
    args = (c['bustype'], c['vendor'], c['product'], c['version'])
    assert device_exists(*args)
    ui.close()
    assert not device_exists(*args)

def test_open_context(c):
    args = (c['bustype'], c['vendor'], c['product'], c['version'])
    with uinput.UInput(**c):
        assert device_exists(*args)
    assert not device_exists(*args)

def test_maxnamelen(c):
    with raises(uinput.UInputError):
        c['name'] = 'a' * 150
        uinput.UInput(**c)

def test_enable_events(c):
    e = ecodes
    c['events'] = {e.EV_KEY : [e.KEY_A, e.KEY_B, e.KEY_C]}

    with uinput.UInput(**c) as ui:
        cap = ui.capabilities()
        assert e.EV_KEY in cap
        assert sorted(cap[e.EV_KEY]) == sorted(c['events'][e.EV_KEY])

def test_abs_values(c):
    e = ecodes
    c['events'] = {
        e.EV_KEY : [e.KEY_A, e.KEY_B],
        e.EV_ABS : [(e.ABS_X, (0, 255, 0, 0)),
                    (e.ABS_Y, device.AbsInfo(0, 255, 5, 10, 0, 0))],
    }

    with uinput.UInput(**c) as ui:
        c = ui.capabilities()
        abs = device.AbsInfo(value=0, min=0, max=255, fuzz=0, flat=0, resolution=0)
        assert c[e.EV_ABS][0] == (0, abs)
        abs = device.AbsInfo(value=0, min=0, max=255, fuzz=5, flat=10, resolution=0)
        assert c[e.EV_ABS][1] == (1, abs)

        c = ui.capabilities(verbose=True)
        abs = device.AbsInfo(value=0, min=0, max=255, fuzz=0, flat=0, resolution=0)
        assert c[('EV_ABS', 3)][0] == (('ABS_X', 0), abs)

        c = ui.capabilities(verbose=False, absinfo=False)
        assert c[e.EV_ABS] == list((0, 1))

def test_write(c):
    with uinput.UInput(**c) as ui:
        d = ui.device
        wrote = False

        while True:
            r, w, x = select([d], [d], [])

            if w and not wrote:
                ui.write(ecodes.EV_KEY, ecodes.KEY_P, 1) # KEY_P down
                ui.write(ecodes.EV_KEY, ecodes.KEY_P, 1) # KEY_P down
                ui.write(ecodes.EV_KEY, ecodes.KEY_P, 0) # KEY_P up
                ui.write(ecodes.EV_KEY, ecodes.KEY_A, 1) # KEY_A down
                ui.write(ecodes.EV_KEY, ecodes.KEY_A, 2) # KEY_A hold
                ui.write(ecodes.EV_KEY, ecodes.KEY_A, 0) # KEY_P up
                ui.syn()
                wrote = True

            if r:
                evs = list(d.read())

                assert evs[0].code == ecodes.KEY_P and evs[0].value == 1
                assert evs[1].code == ecodes.KEY_P and evs[1].value == 0
                assert evs[2].code == ecodes.KEY_A and evs[2].value == 1
                assert evs[3].code == ecodes.KEY_A and evs[3].value == 2
                assert evs[4].code == ecodes.KEY_A and evs[4].value == 0
                break
