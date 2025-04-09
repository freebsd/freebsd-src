#include <sys/types.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>

extern driver_t lkpi_80211_pm_driver;
DRIVER_MODULE(lkpi80211_pm, rtw89, lkpi_80211_pm_driver, 0, 0);

