/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

#define LOG_TAG "PowerHal"
#include <utils/Log.h>

#include <cutils/properties.h>
#include <hardware/power.h>

#define PROFILE_POWER_SAVE       0
#define PROFILE_BALANCED         1
#define PROFILE_HIGH_PERFORMANCE 2

#define POWER_PROFILE_PROPERTY  "sys.perf.profile"
#define POWER_SAVE_PROP         "0"
#define BALANCED_PROP           "1"
#define HIGH_PERFORMANCE_PROP   "2"

static int current_power_profile = PROFILE_BALANCED;
static power_hint_t current_power_hint = 0;

static int sysfs_write(const char *path, char *s)
{
    char buf[80];
    int len;
    int fd = open(path, O_WRONLY);

    if (fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error opening %s: %s\n", path, buf);
        return -1;
    }

    len = write(fd, s, strlen(s));
    if (len < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error writing to %s: %s\n", path, buf);
        return -1;
    }

    close(fd);
    return 0;
}

static void power_init(struct power_module *module __unused)
{
    ALOGI("%s", __func__);
}

static void power_set_interactive(struct power_module *module __unused,
                int on __unused)
{
}

static void set_power_profile(int profile)
{
    if (profile == current_power_profile) {
        return;
    }

    switch (profile) {
        case PROFILE_POWER_SAVE:
            property_set(POWER_PROFILE_PROPERTY, POWER_SAVE_PROP);
            break;
        case PROFILE_BALANCED:
            property_set(POWER_PROFILE_PROPERTY, BALANCED_PROP);
            break;
        case PROFILE_HIGH_PERFORMANCE:
            property_set(POWER_PROFILE_PROPERTY, HIGH_PERFORMANCE_PROP);
            break;
    }

    current_power_profile = profile;
}

static void power_hint(struct power_module *module __unused, power_hint_t hint,
                void *data __unused)
{
    if (hint == current_power_hint) {
        return;
    }

    switch (hint) {
        case POWER_HINT_LOW_POWER:
            set_power_profile(PROFILE_POWER_SAVE);
            break;
        default:
            if (current_power_hint == POWER_HINT_LOW_POWER) {
                set_power_profile(PROFILE_BALANCED);
            }
    }

    current_power_hint = hint;
}

static void set_feature(struct power_module *module __unused,
                feature_t feature, int state)
{
#ifdef TAP_TO_WAKE_NODE
    char tmp_str[64];

    if (feature == POWER_FEATURE_DOUBLE_TAP_TO_WAKE) {
        snprintf(tmp_str, 64, "%d", state);
        sysfs_write(TAP_TO_WAKE_NODE, tmp_str);
    }
#endif
}

static int power_open(const hw_module_t* module __unused, const char* name,
                hw_device_t** device)
{
    ALOGD("%s: enter; name=%s", __func__, name);

    if (strcmp(name, POWER_HARDWARE_MODULE_ID)) {
        return -EINVAL;
    }

    power_module_t *dev = (power_module_t *)calloc(1,
            sizeof(power_module_t));

    if (!dev) {
        ALOGD("%s: failed to allocate memory", __func__);
        return -ENOMEM;
    }

    dev->common.tag = HARDWARE_MODULE_TAG;
    dev->common.module_api_version = POWER_MODULE_API_VERSION_0_3;
    dev->common.hal_api_version = HARDWARE_HAL_API_VERSION;

    dev->init = power_init;
    dev->powerHint = power_hint;
    dev->setInteractive = power_set_interactive;
    dev->setFeature = set_feature;

    *device = (hw_device_t*)dev;

    ALOGD("%s: exit", __func__);

    return 0;
}

static struct hw_module_methods_t power_module_methods = {
    .open = power_open,
};

struct power_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = POWER_MODULE_API_VERSION_0_3,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = POWER_HARDWARE_MODULE_ID,
        .name = "Onyx Power HAL",
        .author = "The CyanogenMod Project",
        .methods = &power_module_methods,
    },
    .init = power_init,
    .powerHint = power_hint,
    .setInteractive = power_set_interactive,
    .setFeature = set_feature
};
