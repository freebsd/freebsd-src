# Mobile Power Subsystem

BSD-licensed power management for the mobile OS.

## Modules

### cpufreq
CPU frequency scaling with ondemand, conservative, powersave,
performance, and schedutil governors.  Uses `sysfs` for
freq/governor reads and writes.

### thermal
Thermal zone and cooling device management using sysfs.
Intercepts hot and passive trips and handles fan/speed changes.
Shuts down in critical mode.

### suspend
Suspend-to-idle/standby/memory with device save/restore
callbacks, wake-source tracking, and suspend blockers
to prevent suspend when other work is outstanding.

### display_power
Backlight control (0-255), power-on/standby/suspend/off
blanking, ambient-light driven auto-brightness,
doze (always-on) mode, auto-dimming after inactivity.

Kernel features
--------------
  fw_mod_mobile_power: enable via `kldload mobile_power`.

Sysfs paths
----------
  policyN/scaling_governor
  policyN/scaling_available_frequencies
  policyN/scaling_cur_freq
  /sys/class/thermal/thermal_zoneN/temp
  /sys/class/thermal/cooling_deviceN/cur_state
