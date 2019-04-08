// SPDX-License-Identifier: GPL-2.0+
/*
 * Roccat Vulcan 100/120 driver for Linux
 *
 * Copyright (c) 2019 Tobias Buettner <t.linux@spiritcroc.de>
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/device.h>
#include <linux/input.h>
#include <linux/hid.h>
#include <linux/module.h>
#include "hid-ids.h"

#define VULCAN_SPECIAL_KEY_SEQUENCE_LENGTH	5

struct vulcan_drvdata {
	struct input_dev *input;
};

struct vulcan_special_key {
	unsigned int code;
	int value;
	u8 sequence[VULCAN_SPECIAL_KEY_SEQUENCE_LENGTH];
};

static int vulcan_probe(struct hid_device *hdev,
		const struct hid_device_id *id)
{
	int retval;
	struct vulcan_drvdata *drvdata;

	drvdata = devm_kzalloc(&hdev->dev, sizeof(*drvdata), GFP_KERNEL);
	if (drvdata == NULL)
		return -ENOMEM;

	hid_set_drvdata(hdev, drvdata);

	retval = hid_parse(hdev);
	if (retval) {
		hid_err(hdev, "parse failed\n");
		goto exit;
	}

	retval = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	if (retval) {
		hid_err(hdev, "hw start failed\n");
		goto exit;
	}

	if (!drvdata->input) {
		hid_err(hdev, "Roccat vulcan input not registered\n");
		retval = -ENOMEM;
		goto exit_stop;
	}

	return 0;
exit_stop:
	hid_hw_stop(hdev);
exit:
	return retval;
}

static void vulcan_remove(struct hid_device *hdev)
{
	hid_hw_stop(hdev);
}

/*
 * Media keys change reported data as soon as the initialization
 * sequence for controlling custom LED effects is sent.
 * Original scancodes work with the generic HID driver implementation,
 * so we just add the "new" data to send the same key event.
 */
#define MEDIA_KEY_PRESS(which, id) \
	{ .code = which, .value = 1, \
		.sequence = { 0x03, 0x00, 0x0b, id, 0x00 } }
#define MEDIA_KEY_RELEASE(which, id) \
	{ .code = which, .value = 0, \
		.sequence = { 0x03, 0x00, 0x0b, id, 0x01 } }
/*
 * Additional events that can be used as soon as the initialization
 * sequence for controlling custom LED effects is sent.
 */
#define FX_KEY_PRESS(which, id) \
	{ .code = which, .value = 1, \
		.sequence = { 0x03, 0x00, 0xfb, id, 0x01 } }
#define FX_KEY_RELEASE(which, id) \
	{ .code = which, .value = 0, \
		.sequence = { 0x03, 0x00, 0xfb, id, 0x00 } }
static const struct vulcan_special_key vulcan_key_map[] = {
	MEDIA_KEY_PRESS(KEY_PREVIOUSSONG, 0x21),
	MEDIA_KEY_RELEASE(KEY_PREVIOUSSONG, 0x21),
	MEDIA_KEY_PRESS(KEY_NEXTSONG, 0x22),
	MEDIA_KEY_RELEASE(KEY_NEXTSONG, 0x22),
	MEDIA_KEY_PRESS(KEY_PLAYPAUSE, 0x23),
	MEDIA_KEY_RELEASE(KEY_PLAYPAUSE, 0x23),
	MEDIA_KEY_PRESS(KEY_STOPCD, 0x24),
	MEDIA_KEY_RELEASE(KEY_STOPCD, 0x24),
	FX_KEY_PRESS(KEY_FN_F1, 0x10),
	FX_KEY_RELEASE(KEY_FN_F1, 0x10),
	FX_KEY_PRESS(KEY_FN_F2, 0x18),
	FX_KEY_RELEASE(KEY_FN_F2, 0x18),
	FX_KEY_PRESS(KEY_FN_F3, 0x21),
	FX_KEY_RELEASE(KEY_FN_F3, 0x21),
	FX_KEY_PRESS(KEY_FN_F4, 0x20),
	FX_KEY_RELEASE(KEY_FN_F4, 0x20),
	FX_KEY_PRESS(KEY_FN, 0x77),
	FX_KEY_RELEASE(KEY_FN, 0x77),
	{ .code = 0 },
};

static int vulcan_raw_event(struct hid_device *hdev,
		struct hid_report *report, u8 *data, int size)
{
	struct vulcan_drvdata *drvdata = hid_get_drvdata(hdev);
	int i;

	if (size != VULCAN_SPECIAL_KEY_SEQUENCE_LENGTH)
		return 0;

	for (i = 0; vulcan_key_map[i].code; i++) {
		if (memcmp(data, vulcan_key_map[i].sequence, size) == 0) {
			input_event(drvdata->input, EV_KEY,
					vulcan_key_map[i].code,
					vulcan_key_map[i].value);
			input_sync(drvdata->input);
			return 1;
		}
	}

	return 0;
}

static int vulcan_input_configured(struct hid_device *hdev,
		struct hid_input *hi)
{
	struct vulcan_drvdata *drvdata = hid_get_drvdata(hdev);
	const char *suffix = NULL;
	const char *name;


	drvdata->input = hi->input;

	// Profile keys
	hi->input->keycodemax = KEY_FN_F4;
	set_bit(KEY_FN_F1, hi->input->keybit);
	set_bit(KEY_FN_F2, hi->input->keybit);
	set_bit(KEY_FN_F3, hi->input->keybit);
	set_bit(KEY_FN_F4, hi->input->keybit);
	// FN key
	set_bit(KEY_FN, hi->input->keybit);

	// Device suffix
	switch (hi->application) {
	case HID_GD_KEYBOARD:
		suffix = "Main Keyboard";
		break;
	case HID_GD_MOUSE:
		suffix = "Extra Keyboard";
		break;
	case HID_GD_KEYPAD:
		suffix = "Misc Device";
		break;
	case HID_UP_GENDESK:
		suffix = "LED Device";
		break;
	default:
		break;
	}
	if (suffix) {
		name = devm_kasprintf(&hdev->dev, GFP_KERNEL, "%s %s",
				hdev->name, suffix);
		if (name)
			hi->input->name = name;
	}

	return 0;
}

static const struct hid_device_id vulcan_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_ROCCAT,
		USB_DEVICE_ID_ROCCAT_VULCAN_100) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ROCCAT,
		USB_DEVICE_ID_ROCCAT_VULCAN_120) },
	{ }

};
MODULE_DEVICE_TABLE(hid, vulcan_devices);

static struct hid_driver vulcan_driver = {
	.name = "roccat-vulcan",
	.id_table = vulcan_devices,
	.probe = vulcan_probe,
	.remove = vulcan_remove,
	.raw_event = vulcan_raw_event,
	.input_configured = vulcan_input_configured
};
module_hid_driver(vulcan_driver);

MODULE_AUTHOR("Tobias Buettner");
MODULE_DESCRIPTION("USB Roccat Vulcan 100/120 driver");
MODULE_LICENSE("GPL v2");
