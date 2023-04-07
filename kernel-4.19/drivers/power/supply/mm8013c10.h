#ifndef __MM8013C10_H__
#define __MM8013C10_H__

enum mm8013c10_bat_info {
    MM8013C10_BAT_SOC,
    MM8013C10_BAT_CC,
    MM8013C10_BAT_SOH,
    MM8013C10_BAT_ID,
};

int mm8013c10_get_bat_info(enum mm8013c10_bat_info);
const char *mm8013c10_get_battery_name(void);

#endif