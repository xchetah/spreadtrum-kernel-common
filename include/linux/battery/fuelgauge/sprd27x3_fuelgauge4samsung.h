
#ifndef _SPRD27X3_FUELGAUGE4SAMSUNG_H_
#define _SPRD27X3_FUELGAUGE4SAMSUNG_H_

#include <linux/types.h>
#include <linux/battery/sec_charger.h>
#include <linux/battery/sec_fuelgauge.h>
#include <linux/sprd_battery_common.h>
#include "../power/sprd_2713_fgu.h"
#include "../power/sprd_battery.h"


struct battery_data_t {
    struct sprd_battery_platform_data *pdata;
};

#endif

