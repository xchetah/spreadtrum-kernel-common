/*
 *  step_wise.c - A step-by-step Thermal throttling governor
 *
 *  Copyright (C) 2012 Intel Corp
 *  Copyright (C) 2012 Durgadoss R <durgadoss.r@intel.com>
 *
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <linux/thermal.h>

#include "thermal_core.h"

/*
 * If the temperature is higher than a trip point,
 *    a. if the trend is THERMAL_TREND_RAISING, use higher cooling
 *       state for this trip point
 *    b. if the trend is THERMAL_TREND_DROPPING, use lower cooling
 *       state for this trip point
 *    c. if the trend is THERMAL_TREND_RAISE_FULL, use upper limit
 *       for this trip point
 *    d. if the trend is THERMAL_TREND_DROP_FULL, use lower limit
 *       for this trip point
 * If the temperature is lower than a trip point,
 *    a. if the trend is THERMAL_TREND_RAISING, do nothing
 *    b. if the trend is THERMAL_TREND_DROPPING, use lower cooling
 *       state for this trip point, if the cooling state already
 *       equals lower limit, deactivate the thermal instance
 *    c. if the trend is THERMAL_TREND_RAISE_FULL, do nothing
 *    d. if the trend is THERMAL_TREND_DROP_FULL, use lower limit,
 *       if the cooling state already equals lower limit,
 *       deactive the thermal instance
 */
#if 0
static unsigned long get_target_state(struct thermal_instance *instance,
				enum thermal_trend trend, bool throttle)
{
	struct thermal_cooling_device *cdev = instance->cdev;
	unsigned long cur_state;

	cdev->ops->get_cur_state(cdev, &cur_state);
	dev_dbg(&cdev->device, "cur_state=%ld\n", cur_state);

	switch (trend) {
	case THERMAL_TREND_RAISING:
		if (throttle) {
			cur_state = cur_state < instance->upper ?
				    (cur_state + 1) : instance->upper;
			if (cur_state < instance->lower)
				cur_state = instance->lower;
		}
		break;
	case THERMAL_TREND_RAISE_FULL:
		if (throttle)
			cur_state = instance->upper;
		break;
	case THERMAL_TREND_DROPPING:
		if (cur_state == instance->lower) {
			if (!throttle)
				cur_state = -1;
		} else {
			cur_state -= 1;
			if (cur_state > instance->upper)
				cur_state = instance->upper;
		}
		break;
	case THERMAL_TREND_DROP_FULL:
		if (cur_state == instance->lower) {
			if (!throttle)
				cur_state = -1;
		} else
			cur_state = instance->lower;
		break;
	default:
		break;
	}

	return cur_state;
}

static void update_passive_instance(struct thermal_zone_device *tz,
				enum thermal_trip_type type, int value)
{
	/*
	 * If value is +1, activate a passive instance.
	 * If value is -1, deactivate a passive instance.
	 */
	if (type == THERMAL_TRIP_PASSIVE || type == THERMAL_TRIPS_NONE)
		tz->passive += value;
}

static void thermal_zone_trip_update(struct thermal_zone_device *tz, int trip)
{
	long trip_temp;
	enum thermal_trip_type trip_type;
	enum thermal_trend trend;
	struct thermal_instance *instance;
	bool throttle = false;
	int old_target;

	if (trip == THERMAL_TRIPS_NONE) {
		trip_temp = tz->forced_passive;
		trip_type = THERMAL_TRIPS_NONE;
	} else {
		tz->ops->get_trip_temp(tz, trip, &trip_temp);
		tz->ops->get_trip_type(tz, trip, &trip_type);
	}

	trend = get_tz_trend(tz, trip);

	if (tz->temperature >= trip_temp)
		throttle = true;

	dev_dbg(&tz->device, "Trip%d[type=%d,temp=%ld]:trend=%d,throttle=%d\n",
				trip, trip_type, trip_temp, trend, throttle);

	mutex_lock(&tz->lock);

	list_for_each_entry(instance, &tz->thermal_instances, tz_node) {
		if (instance->trip != trip)
			continue;

		old_target = instance->target;
		instance->target = get_target_state(instance, trend, throttle);
		dev_dbg(&instance->cdev->device, "old_target=%d, target=%d\n",
					old_target, (int)instance->target);

		/* Activate a passive thermal instance */
		if (old_target == THERMAL_NO_TARGET &&
			instance->target != THERMAL_NO_TARGET)
			update_passive_instance(tz, trip_type, 1);
		/* Deactivate a passive thermal instance */
		else if (old_target != THERMAL_NO_TARGET &&
			instance->target == THERMAL_NO_TARGET)
			update_passive_instance(tz, trip_type, -1);


		instance->cdev->updated = false; /* cdev needs update */
	}

	mutex_unlock(&tz->lock);
}
#else

static unsigned long get_hyst_target(struct thermal_zone_device *tz, int trip)
{
	int count;
	unsigned long trip_temp, trip_hyst, hyst_temp;

	if (tz->trips == 0 || !tz->ops->get_trip_hyst){
		return 0;
	}
	for (count = 0; count < tz->trips; count++) {
		tz->ops->get_trip_temp(tz, count, &trip_temp);
		tz->ops->get_trip_hyst(tz, count, &trip_hyst);
		hyst_temp = trip_temp - trip_hyst;
		printk("trip:%d, temp:%d, hyst_temp:%d\n", count, tz->temperature, hyst_temp);
		if (tz->temperature < (long)hyst_temp  + TRIP_TEMP_OFFSET)
			break;
	}
	if (trip == count) {
		return count;
	} else {
		return THERMAL_NO_TARGET;
	}
}

static unsigned long get_trip_target(struct thermal_zone_device *tz, int trip)
{
	int count = 0;
	unsigned long trip_temp;

	if (tz->trips == 0 || !tz->ops->get_trip_temp)
		return 0;

	for (count = tz->trips - 1; count >= 0; count--) {
		tz->ops->get_trip_temp(tz, count, &trip_temp);
		printk("trip:%d, temp:%d, trip_temp:%d\n", count, tz->temperature, trip_temp);
		if (tz->temperature > (long)trip_temp  - TRIP_TEMP_OFFSET)
			break;
	}
	if (trip == count) {
		return count + 1;
	} else {
		return THERMAL_NO_TARGET;
	}

}

static unsigned long get_step_wise_target(struct thermal_instance *instance,
		struct thermal_zone_device *tz, int trip)
{
	struct thermal_cooling_device *cdev = instance->cdev;
	enum thermal_trend trend;
	unsigned long cur_target;
	unsigned long cdev_state;
	unsigned long new_target;

	cur_target = instance->target;
	printk("%s:%s trip:%d cur_target:%d\n", __func__, cdev->type, trip, cur_target);
	cdev->ops->get_cur_state(cdev, &cdev_state);
	printk("%s:%s trip:%d cdev_state:%d\n", __func__, cdev->type, trip, cdev_state);
	if (cdev_state == THERMAL_NO_TARGET){
		cdev_state = instance->lower;
	}
	if (tz->ops->get_trend){
		tz->ops->get_trend(tz, trip, &trend);
	}else{
		new_target = get_trip_target(tz, trip);
		printk("%s: %s trip:%d new_target:%d\n", __func__, cdev->type, trip, new_target);
		if (new_target == THERMAL_NO_TARGET) {
			return THERMAL_NO_TARGET;
		}
		if (new_target < cdev_state){
			trend = THERMAL_TREND_DROPPING;
			if (!tz->ops->get_trip_hyst){
				goto dropping;
			}
		}else if(new_target >= cdev_state){
			goto raising;
		}
	}
	switch (trend){
		case THERMAL_TREND_RAISING:
			new_target = get_trip_target(tz, trip);
			printk("%s: %s trend raising, target:%d\n", __func__, cdev->type,  new_target);
			if (new_target == THERMAL_NO_TARGET) {
				return THERMAL_NO_TARGET;
			}
			goto raising;
		case THERMAL_TREND_DROPPING:
#if 0
			if (tz->ops->get_trip_hyst){
				new_target = get_hyst_target(tz, trip);
			}else{
				new_target = get_trip_target(tz, trip);
			}
			printk("%s: %s trend dropping, target:%d\n", cdev->type, __func__, new_target);
			if (new_target == THERMAL_NO_TARGET) {
				return THERMAL_NO_TARGET;
			}
#endif
			goto dropping;
		case THERMAL_TREND_STABLE:
		default:
			printk("%s: %s trend stable\n", cdev->type, __func__);
			return THERMAL_NO_TARGET;
	}
raising:
	if (cdev_state > trip){
		cdev_state = trip;
	}
	return cdev_state + 1;
dropping:
	if (cur_target != THERMAL_NO_TARGET && cur_target > 0){
		return cur_target - 1;
	}
	return cur_target;

}

static void thermal_zone_trip_update(struct thermal_zone_device *tz, int trip)
{
	struct thermal_instance *instance;
	unsigned long target;

	mutex_lock(&tz->lock);
	list_for_each_entry(instance, &tz->thermal_instances, tz_node) {
		if (instance->trip != trip)
			continue;
		target = get_step_wise_target(instance, tz, trip);
		printk("%s: get target_state: %d\n", instance->cdev->type, target);
		if (target == THERMAL_NO_TARGET){
			continue;
		}
		if (target < instance->lower){
			target = instance->lower;
		}
		if (target > instance->upper){
			target = instance->upper;
		}
		instance->target  = target;
		instance->cdev->updated = false; /* cdev needs update */
	}
	mutex_unlock(&tz->lock);

	return;
}
#endif

/**
 * step_wise_throttle - throttles devices asscciated with the given zone
 * @tz - thermal_zone_device
 * @trip - the trip point
 * @trip_type - type of the trip point
 *
 * Throttling Logic: This uses the trend of the thermal zone to throttle.
 * If the thermal zone is 'heating up' this throttles all the cooling
 * devices associated with the zone and its particular trip point, by one
 * step. If the zone is 'cooling down' it brings back the performance of
 * the devices by one step.
 */
static int step_wise_throttle(struct thermal_zone_device *tz, int trip)
{
	struct thermal_instance *instance;

	thermal_zone_trip_update(tz, trip);

	if (tz->forced_passive)
		thermal_zone_trip_update(tz, THERMAL_TRIPS_NONE);

	mutex_lock(&tz->lock);

	list_for_each_entry(instance, &tz->thermal_instances, tz_node)
		thermal_cdev_update(instance->cdev);

	mutex_unlock(&tz->lock);

	return 0;
}

static struct thermal_governor thermal_gov_step_wise = {
	.name		= "step_wise",
	.throttle	= step_wise_throttle,
};

int thermal_gov_step_wise_register(void)
{
	return thermal_register_governor(&thermal_gov_step_wise);
}

void thermal_gov_step_wise_unregister(void)
{
	thermal_unregister_governor(&thermal_gov_step_wise);
}
