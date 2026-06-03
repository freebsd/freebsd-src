#include "evdev.h"
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <linux/input.h>

#define MAX_DEVICES 64

int evdev_open(const char *path)
{
    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        perror("evdev_open");
    }
    return fd;
}

void evdev_close(int fd)
{
    if (fd >= 0) {
        close(fd);
    }
}

struct input_event evdev_read_event(int fd, int timeout_ms)
{
    struct input_event ev;
    struct pollfd pfd;
    int ret;

    pfd.fd = fd;
    pfd.events = POLLIN;

    ret = poll(&pfd, 1, timeout_ms);
    if (ret > 0 && (pfd.revents & POLLIN)) {
        if (read(fd, &ev, sizeof(ev)) == sizeof(ev)) {
            return ev;
        }
    }

    /* Return an event with type 0 to indicate timeout or error */
    ev.type = 0;
    ev.code = 0;
    ev.value = 0;
    return ev;
}

int evdev_get_capabilities(int fd, uint32_t *key_bitmap, uint32_t *rel_bitmap, uint32_t *abs_bitmap, uint32_t *msc_bitmap)
{
    unsigned char key[KEY_MAX/8 + 1];
    unsigned char rel[REL_MAX/8 + 1];
    unsigned char abs[ABS_MAX/8 + 1];
    unsigned char msc[MSC_MAX/8 + 1];
    int ret;

    if (key_bitmap) {
        ret = ioctl(fd, EVIOCGKEY(sizeof(key)), key);
        if (ret < 0) {
            perror("evdev_get_capabilities: EVIOCGKEY");
            return ret;
        }
        memcpy(key_bitmap, key, sizeof(key));
    }
    if (rel_bitmap) {
        ret = ioctl(fd, EVIOCGREL(sizeof(rel)), rel);
        if (ret < 0) {
            perror("evdev_get_capabilities: EVIOCGREL");
            return ret;
        }
        memcpy(rel_bitmap, rel, sizeof(rel));
    }
    if (abs_bitmap) {
        ret = ioctl(fd, EVIOCGABS(sizeof(abs)), abs);
        if (ret < 0) {
            perror("evdev_get_capabilities: EVIOCGABS");
            return ret;
        }
        memcpy(abs_bitmap, abs, sizeof(abs));
    }
    if (msc_bitmap) {
        ret = ioctl(fd, EVIOCGMSC(sizeof(msc)), msc);
        if (ret < 0) {
            perror("evdev_get_capabilities: EVIOCGMSC");
            return ret;
        }
        memcpy(msc_bitmap, msc, sizeof(msc));
    }
    return 0;
}

int evdev_get_abs_info(int fd, unsigned int code, struct input_absinfo *info)
{
    int ret = ioctl(fd, EVIOGGABS(code), info);
    if (ret < 0) {
        perror("evdev_get_abs_info");
    }
    return ret;
}

const char *evdev_get_key_name(unsigned int code)
{
    /* We'll use a simple mapping for common keys, but in reality we'd use a table */
    switch (code) {
        case KEY_ESC: return "Escape";
        case KEY_1: return "1";
        case KEY_2: return "2";
        case KEY_3: return "3";
        case KEY_4: return "4";
        case KEY_5: return "5";
        case KEY_6: return "6";
        case KEY_7: return "7";
        case KEY_8: return "8";
        case KEY_9: return "9";
        case KEY_0: return "0";
        case KEY_MINUS: return "Minus";
        case KEY_EQUAL: return "Equal";
        case KEY_BACKSPACE: return "Backspace";
        case KEY_TAB: return "Tab";
        case KEY_Q: return "Q";
        case KEY_W: return "W";
        case KEY_E: return "E";
        case KEY_R: return "R";
        case KEY_T: return "T";
        case KEY_Y: return "Y";
        case KEY_U: return "U";
        case KEY_I: return "I";
        case KEY_O: return "O";
        case KEY_P: return "P";
        case KEY_LEFTBRACE: return "LeftBrace";
        case KEY_RIGHTBRACE: return "RightBrace";
        case KEY_ENTER: return "Enter";
        case KEY_LEFTCTRL: return "LeftCtrl";
        case KEY_A: return "A";
        case KEY_S: return "S";
        case KEY_D: return "D";
        case KEY_F: return "F";
        case KEY_G: return "G";
        case KEY_H: return "H";
        case KEY_J: return "J";
        case KEY_K: return "K";
        case KEY_L: return "L";
        case KEY_SEMICOLON: return "Semicolon";
        case KEY_APOSTROPHE: return "Apostrophe";
        case KEY_GRAVE: return "Grave";
        case KEY_LEFTSHIFT: return "LeftShift";
        case KEY_BACKSLASH: return "Backslash";
        case KEY_Z: return "Z";
        case KEY_X: return "X";
        case KEY_C: return "C";
        case KEY_V: return "V";
        case KEY_B: return "B";
        case KEY_N: return "N";
        case KEY_M: return "M";
        case KEY_COMMA: return "Comma";
        case KEY_DOT: return "Dot";
        case KEY_SLASH: return "Slash";
        case KEY_RIGHTSHIFT: return "RightShift";
        case KEY_KPASTERISK: return "KpAsterisk";
        case KEY_LEFTALT: return "LeftAlt";
        case KEY_SPACE: return "Space";
        case KEY_CAPSLOCK: return "CapsLock";
        case KEY_F1: return "F1";
        case KEY_F2: return "F2";
        case KEY_F3: return "F3";
        case KEY_F4: return "F4";
        case KEY_F5: return "F5";
        case KEY_F6: return "F6";
        case KEY_F7: return "F7";
        case KEY_F8: return "F8";
        case KEY_F9: return "F9";
        case KEY_F10: return "F10";
        case KEY_NUMLOCK: return "NumLock";
        case KEY_SCROLLLOCK: return "ScrollLock";
        case KEY_KP7: return "Kp7";
        case KEY_KP8: return "Kp8";
        case KEY_KP9: return "Kp9";
        case KEY_KPMINUS: return "KpMinus";
        case KEY_KP4: return "Kp4";
        case KEY_KP5: return "Kp5";
        case KEY_KP6: return "Kp6";
        case KEY_KPPLUS: return "KpPlus";
        case KEY_KP1: return "Kp1";
        case KEY_KP2: return "Kp2";
        case KEY_KP3: return "Kp3";
        case KEY_KP0: return "Kp0";
        case KEY_KPDOT: return "KpDot";
        case KEY_ZENKAKUHANKAKU: return "ZenkakuHankaku";
        case KEY_102ND: return "102nd";
        case KEY_F11: return "F11";
        case KEY_F12: return "F12";
        case KEY_RO: return "Ro";
        case KEY_KATAKANA: return "Katakana";
        case KEY_HIRAGANA: return "Hiragana";
        case KEY_HENKAN: return "Henkan";
        case KEY_KATAKANAHIRAGANA: return "KatakanaHiragana";
        case KEY_MUHENKAN: return "Muhенkan";
        case KEY_KPJPCOMMA: return "KpJpComma";
        case KEY_HOME: return "Home";
        case KEY_UP: return "Up";
        case KEY_PAGEUP: return "PageUp";
        case KEY_LEFT: return "Left";
        case KEY_RIGHT: return "Right";
        case KEY_END: return "End";
        case KEY_DOWN: return "Down";
        case KEY_PAGEDOWN: return "PageDown";
        case KEY_INSERT: return "Insert";
        case KEY_DELETE: return "Delete";
        case KEY_MACRO: return "Macro";
        case KEY_MUTE: return "Mute";
        case KEY_VOLUMEDOWN: return "VolumeDown";
        case KEY_VOLUMEUP: return "VolumeUp";
        case KEY_POWER: return "Power";
        case KEY_KPEQUAL: return "KpEqual";
        case KEY_KPPLUSMINUS: return "KpPlusMinus";
        case KEY_PAUSE: return "Pause";
        case KEY_SCALE: return "Scale";
        case KEY_KPCOMMA: return "KpComma";
        case KEY_HANGEUL: return "Hangeul";
        case KEY_HANGUEL: return "Hanguel";
        case KEY_HANJA: return "Hanja";
        case KEY_YEN: return "Yen";
        case KEY_LEFTMETA: return "LeftMeta";
        case KEY_RIGHTMETA: return "RightMeta";
        case KEY_COMPOSE: return "Compose";
        case KEY_STOP: return "Stop";
        case KEY_AGAIN: return "Again";
        case KEY_PROPS: return "Props";
        case KEY_UNDO: return "Undo";
        case KEY_FRONT: return "Front";
        case KEY_COPY: return "Copy";
        case KEY_OPEN: return "Open";
        case KEY_PASTE: return "Paste";
        case KEY_FIND: return "Find";
        case KEY_CUT: return "Cut";
        case KEY_HELP: return "Help";
        case KEY_MENU: return "Menu";
        case KEY_CALC: return "Calc";
        case KEY_SETUP: return "Setup";
        case KEY_SLEEP: return "Sleep";
        case KEY_WAKEUP: return "WakeUp";
        case KEY_FILE: return "File";
        case KEY_SENDFILE: return "SendFile";
        case KEY_DELETEFILE: return "DeleteFile";
        case KEY_XFER: return "Xfer";
        case KEY_PROG1: return "Prog1";
        case KEY_PROG2: return "Prog2";
        case KEY_WWW: return "Www";
        case KEY_MSDOS: return "MsDos";
        case KEY_COFFEE: return "Coffee";
        case KEY_SCREENLOCK: return "ScreenLock";
        case KEY_DIRECTION: return "Direction";
        case KEY_CYCLEWINDOWS: return "CycleWindows";
        case KEY_MAIL: return "Mail";
        case KEY_BOOKMARKS: return "Bookmarks";
        case KEY_COMPUTER: return "Computer";
        case KEY_BACK: return "Back";
        case KEY_FORWARD: return "Forward";
        case KEY_CLOSECD: return "CloseCD";
        case KEY_EJECTCD: return "EjectCD";
        case KEY_EJECTCLOSECD: return "EjectCloseCD";
        case KEY_NEXTSONG: return "NextSong";
        case KEY_PLAYPAUSE: return "PlayPause";
        case KEY_PREVIOUSSONG: return "PreviousSong";
        case KEY_STOPCD: return "StopCD";
        case KEY_RECORD: return "Record";
        case KEY_REWIND: return "Rewind";
        case KEY_PHONE: return "Phone";
        case KEY_ISO: return "Iso";
        case KEY_CONFIG: return "Config";
        case KEY_HOMEPAGE: return "HomePage";
        case KEY_REFRESH: return "Refresh";
        case KEY_EXIT: return "Exit";
        case KEY_MOVE: return "Move";
        case KEY_EDIT: return "Edit";
        case KEY_SCROLLUP: return "ScrollUp";
        case KEY_SCROLLDOWN: return "ScrollDown";
        case KEY_KPLEFTPAREN: return "KpLeftParen";
        case KEY_KPRIGHTPAREN: return "KpRightParen";
        case KEY_F13: return "F13";
        case KEY_F14: return "F14";
        case KEY_F15: return "F15";
        case KEY_F16: return "F16";
        case KEY_F17: return "F17";
        case KEY_F18: return "F18";
        case KEY_F19: return "F19";
        case KEY_F20: return "F20";
        case KEY_F21: return "F21";
        case KEY_F22: return "F22";
        case KEY_F23: return "F23";
        case KEY_F24: return "F24";
        case KEY_PLAYCD: return "PlayCD";
        case KEY_PAUSECD: return "PauseCD";
        case KEY_PROG3: return "Prog3";
        case KEY_PROG4: return "Prog4";
        case KEY_DASHBOARD: return "Dashboard";
        case KEY_SUSPEND: return "Suspend";
        case KEY_CLOSE: return "Close";
        case KEY_PLAY: return "Play";
        case KEY_FASTFORWARD: return "FastForward";
        case KEY_BASSBOOST: return "BassBoost";
        case KEY_PRINT: return "Print";
        case KEY_HP: return "Hp";
        case KEY_CAMERA: return "Camera";
        case KEY_SOUND: return "Sound";
        case KEY_QUESTION: return "Question";
        case KEY_EMAIL: return "Email";
        case KEY_CHAT: return "Chat";
        case KEY_SEARCH: return "Search";
        case KEY_CONNECT: return "Connect";
        case KEY_F15: return "F15"; /* Duplicate, but we keep */
        case KEY_BACK: return "Back"; /* Duplicate */
        case KEY_FORWARD: return "Forward"; /* Duplicate */
        case KEY_STOPCD: return "StopCD"; /* Duplicate */
        case KEY_RELOAD: return "Reload";
        case KEY_WINDOWS: return "Windows";
        case KEY_ALTERASE: return "Alterase";
        case KEY_NEXT: return "Next";
        case KEY_PREVIOUS: return "Previous";
        case KEY_CUT: return "Cut"; /* Duplicate */
        case KEY_COPY: return "Copy"; /* Duplicate */
        case KEY_PASTE: return "Paste"; /* Duplicate */
        case KEY_VOLUMEUP: return "VolumeUp"; /* Duplicate */
        case KEY_VOLUMEDOWN: return "VolumeDown"; /* Duplicate */
        case KEY_MUTE: return "Mute"; /* Duplicate */
        case KEY_WWW: return "Www"; /* Duplicate */
        case KEY_MAIL: return "Mail"; /* Duplicate */
        case KEY_BOOKMARKS: return "Bookmarks"; /* Duplicate */
        case KEY_COMPUTER: return "Computer"; /* Duplicate */
        case KEY_EJECT: return "Eject";
        case KEY_CDROM: return "CdRom";
        case KEY_ANGLEDOWN: return "AngleDown";
        case KEY_ANGLEUP: return "AngleUp";
        case KEY_LEFTSHIFT: return "LeftShift"; /* Duplicate */
        case KEY_RIGHTSHIFT: return "RightShift"; /* Duplicate */
        case KEY_LEFTCTRL: return "LeftCtrl"; /* Duplicate */
        case KEY_RIGHTCTRL: return "RightCtrl";
        case KEY_LEFTALT: return "LeftAlt"; /* Duplicate */
        case KEY_RIGHTALT: return "RightAlt";
        case KEY_LEFTMETA: return "LeftMeta"; /* Duplicate */
        case KEY_RIGHTMETA: return "RightMeta"; /* Duplicate */
        default: return "Unknown";
    }
}

int evdev_scan_devices(void (*callback)(const char *name, const char *phys, const char *devnode))
{
    DIR *dir;
    struct dirent *ent;
    char path[256];
    char name[256] = {0};
    char phys[256] = {0};
    int fd;
    int ret;

    dir = opendir("/sys/class/input");
    if (!dir) {
        perror("opendir /sys/class/input");
        return -1;
    }

    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, "event", 5) != 0)
            continue;

        snprintf(path, sizeof(path), "/sys/class/input/%s/device/name", ent->d_name);
        int name_fd = open(path, O_RDONLY);
        if (name_fd >= 0) {
            ret = read(name_fd, name, sizeof(name) - 1);
            if (ret > 0) {
                name[ret] = '\0';
                /* Remove newline */
                char *newline = strchr(name, '\n');
                if (newline) *newline = '\0';
            }
            close(name_fd);
        }

        snprintf(path, sizeof(path), "/sys/class/input/%s/device/phys", ent->d_name);
        int phys_fd = open(path, O_RDONLY);
        if (phys_fd >= 0) {
            ret = read(phys_fd, phys, sizeof(phys) - 1);
            if (ret > 0) {
                phys[ret] = '\0';
                char *newline = strchr(phys, '\n');
                if (newline) *newline = '\0';
            }
            close(phys_fd);
        }

        snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);
        fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd >= 0) {
            callback(name, phys, path);
            close(fd);
        }
    }

    closedir(dir);
    return 0;
}