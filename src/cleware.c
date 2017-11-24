/*
 * SPDX-License-Identifier: GPL-3.0
 * Copyright (C) 2010-2016 Felipe Balbi <felipe.balbi@linux.intel.com>
 */

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <hidapi.h>
#include <wchar.h>

#define false 0
#define true !false

/*
 * It takes a long time for the internal device state to change, meanwhile we
 * have to continue reading from Interrupt In endpoint.
 */
#define CLEWARE_NUM_READS	50

/*
 * At least USB Cutter is flakey in that it doesn't always switch if we issue a
 * single Switch command.
 */
#define CLEWARE_NUM_WRITES	5

#define CLEWARE_HID_REPORT_SIZE	65
#define CLEWARE_VENDOR_ID	0x0d50

#define CLEWARE_USB_SWITCH	0x0008
#define CLEWARE_USB_SWITCH8	0x0030

#define TIMEOUT			1000	/* ms */
#define ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))

struct cleware {
	struct hid_device_info *dev;
	hid_device *handle;
	wchar_t *serial;

	unsigned short vendor_id;
	unsigned short product_id;

	unsigned char report[65];
	unsigned int report_size;

	unsigned short type;

	unsigned int inverted:1;
};

#define for_each_device(devs, current)		\
	for ((current) = (devs); (current); (current) = (current)->next)

#define OPTION(n, h, v)		\
{				\
	.name		= #n,	\
	.has_arg	= h,	\
	.val		= v,	\
}

static const struct option cleware_opts[] = {
	OPTION("read",		0, 	'r'),
	OPTION("on",		0,	'0'),
	OPTION("off",		0,	'1'),
	OPTION("serial-number",	1,	's'),
	OPTION("port",		1,	'p'),
	OPTION("list",		0,	'l'),
	OPTION("help",		0,	'h'),
	{  } /* Terminating entry */
};

static void usage(const char *name)
{
	fprintf(stdout, "usage: %s [[-l] | [[-p port] [-0 | -1] [-s serial-number]] | [-h]]\n\
			-0, --off		Turn switch off\n\
			-1, --on		Turn switch on\n\
			-p, --port		Port number\n\
			-r, --read		Read switch state\n\
			-s, --serial-number	Device's serial number\n\
			-h, --help		Show this help\n", name);
}

static void cleware_list_devices(struct hid_device_info *devs)
{
	struct hid_device_info *current;

	for_each_device(devs, current) {
		printf("Found: %ls\n", current->product_string);
		printf("    vendor id: %04x\n", current->vendor_id);
		printf("    product id: %04x\n", current->product_id);
		printf("    manufacturer: %ls\n", current->manufacturer_string);
		printf("    serial number: %ls\n", current->serial_number);
		printf("    path: %s\n", current->path);
		printf("\n");
	}
}

static int cleware_open(struct cleware *c)
{
	struct hid_device_info *dev = c->dev;

	c->handle = hid_open(dev->vendor_id, dev->product_id, c->serial);
	if (!c->handle) {
		fprintf(stderr, "can't open device %04x:%04x\n",
				dev->vendor_id, dev->product_id);
		return -1;
	}

	switch (dev->product_id) {
	case CLEWARE_USB_SWITCH8:
		c->type = 0x03;
		break;
	case CLEWARE_USB_SWITCH:
	default:
		c->type = 0x00;
		break;
	}

	if (wcsstr(dev->product_string, L"Cutter"))
		c->inverted = true;

	return 0;
}

static int cleware_write(struct cleware *c)
{
	int ret;

	ret =  hid_write(c->handle, c->report,
			c->report_size + 1);

	memset(c->report, 0x00, CLEWARE_HID_REPORT_SIZE);

	return ret;
}

static int cleware_read(struct cleware *c)
{
	memset(c->report, 0x00, CLEWARE_HID_REPORT_SIZE);

	return hid_read(c->handle, c->report, CLEWARE_HID_REPORT_SIZE);
}

static int cleware_set_led(struct cleware *c, unsigned int led, unsigned int on)
{
	c->report[1] = 0x00;
	c->report[2] = led;
	c->report[3] = on ? 0x00 : 0x0f;

	c->report_size = 3;

	return cleware_write(c);
}

static int cleware_set_switch(struct cleware *c, unsigned int port,
		unsigned int on)
{
	int ret;
	int i;

	for (i = 0; i < CLEWARE_NUM_WRITES; i++) {
		c->report[1] = c->type;
		c->report[2] = port + 0x10;
		c->report[3] = !!on;

		c->report_size = 3;

		if (c->type == 0x03) {
			c->report[4] = 0x00;
			c->report[5] = (1 << port);
			c->report_size = 5;
		}

		ret = cleware_write(c);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int cleware_get_switch(struct cleware *c, unsigned int port)
{
	int state;
	int ret;
	int i;

	for (i = 0; i < CLEWARE_NUM_READS; i++) {
		ret = cleware_read(c);
		if (ret < 0)
			return ret;
	}


	state = (c->report[0] & (1 << port));

	if (c->inverted)
		state = !state;

	fprintf(stdout, "%d: %s\n", port, state ? "ON" : "OFF");

	return 0;
}

static int cleware_set_power(struct cleware *c, unsigned int port,
		unsigned int on)
{
	unsigned int state = on;
	int ret;

	if (c->inverted)
		state = !state;

	ret = cleware_set_switch(c, port - 1, state);
	if (ret < 0)
		return ret;

	if (port == 1) {
		ret = cleware_set_led(c, port - 1, state);
		if (ret < 0)
			return ret;

		ret = cleware_set_led(c, port, !state);
		if (ret < 0)
			return ret;
	}

	ret = cleware_get_switch(c, port - 1);
	if (ret < 0)
		return ret;

	return 0;
}

int main(int argc, char *argv[])
{
	struct cleware *c;
	struct hid_device_info *devs;
	struct hid_device_info *dev = NULL;
	wchar_t *serial = NULL;
	int serial_length;
	int read = 0;
	int port = 1;
	int on = 0;
	int ret = 0;

	if (argc < 2) {
		usage(argv[0]);
		return 0;
	}

	ret = hid_init();
	if (ret)
		goto out0;

	devs = hid_enumerate(CLEWARE_VENDOR_ID, 0);
	while (ARRAY_SIZE(cleware_opts)) {
		int		optidx = 0;
		int		opt;

		opt = getopt_long(argc, argv, "l01rp:s:h", cleware_opts, &optidx);
		if (opt < 0)
			break;

		switch (opt) {
		case 'l':
			cleware_list_devices(devs);
			goto out1;
		case '0':
			on = 0;
			break;
		case '1':
			on = 1;
			break;
		case 'p':
			port = atoi(optarg);
			break;
		case 'r':
			read = 1;
			break;
		case 's':
			serial_length = strlen(optarg) * sizeof(wchar_t) +
				sizeof(wchar_t);
			serial = malloc(serial_length);
			if (!serial)
				goto out1;

			ret = swprintf(serial, serial_length, L"%hs", optarg);
			if (ret < 0)
				goto out1;
			break;
		case 'h': /* FALLTHROUGH */
		default:
			usage(argv[0]);
			goto out1;
		}
	}

	/*
	 * If serial is set, we need to go over the list of devices and find the
	 * one which has the serial number passed via command line. If we can't
	 * find such a device, then we should print and error message and show
	 * list of devices again.
	 *
	 * Now, if serial is *NOT* set, then we will just open the first device
	 * in the list.
	 */
	if (serial) {
		struct hid_device_info *current;

		for_each_device(devs, current) {
			if (wcscasecmp(current->serial_number, serial) == 0) {
				dev = current;
				break;
			}
		}
	} else {
		dev = devs;
	}

	if (!dev) {
		fprintf(stderr, "device not found\n");
		cleware_list_devices(devs);
		ret = -1;
		goto out2;
	}

	c = malloc(sizeof(*c));
	if (!c) {
		ret = -1;
		goto out2;
	}

	c->dev = dev;
	c->vendor_id = dev->vendor_id;
	c->product_id = dev->product_id;
	c->serial = serial;

	ret = cleware_open(c);
	if (ret) {
		cleware_list_devices(devs);
		goto out2;
	}

	if (read) {
		ret = cleware_get_switch(c, port - 1);
		if (ret)
			goto out3;
	} else {
		ret = cleware_set_power(c, port, on);
		if (ret)
			goto out3;
	}

out3:
	free(c);

out2:
	free(serial);

out1:
	hid_free_enumeration(devs);

out0:
	hid_exit();

	return ret;
}
