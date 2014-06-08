# encoding: utf-8

from evdev import events, ecodes, util


def test_categorize():
    e = events.InputEvent(1036996631, 984417, ecodes.EV_KEY, ecodes.KEY_A, 0)
    assert isinstance(util.categorize(e), events.KeyEvent)

    e = events.InputEvent(1036996631, 984417, ecodes.EV_ABS, 0, 0)
    assert isinstance(util.categorize(e), events.AbsEvent)

    e = events.InputEvent(1036996631, 984417, ecodes.EV_REL, 0, 0)
    assert isinstance(util.categorize(e), events.RelEvent)

    e = events.InputEvent(1036996631, 984417, ecodes.EV_MSC, 0, 0)
    assert e == util.categorize(e)

def test_keyevent():
    e = events.InputEvent(1036996631, 984417, ecodes.EV_KEY, ecodes.KEY_A, 2)
    k = events.KeyEvent(e)

    assert k.keystate == events.KeyEvent.key_hold
    assert k.event == e
    assert k.scancode == ecodes.KEY_A
    assert k.keycode == 'KEY_A' # :todo:

