/* $(CROSS_COMPILE)gcc -Wall -O2 -g -lusb-1.0 -o cleware cleware.c */
/**
 * cleware.c - Cleware USB-Controlled Power Switch
 *
 * Copyright (C) 2010 - Felipe Balbi <felipe.balbi@nokia.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <libusb-1.0/libusb.h>

#define CLEWARE_VENDOR_ID	0x0d50
#define CLEWARE_PRODUCT_ID	0x0008

static int debug;

#define DBG(fmt, args...)			\
	if (debug)				\
		fprintf(stdout, fmt, ## args)

#define TIMEOUT			1000	/* ms */
#define ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))

static int find_and_claim_interface(libusb_device_handle *udevh)
{
	int			ret;

	if (libusb_kernel_driver_active(udevh, 0)) {
		ret = libusb_detach_kernel_driver(udevh, 0);
		if (ret < 0) {
			DBG("%s: couldn't detach kernel driver\n", __func__);
			goto err1;
		}
	}

	ret = libusb_set_configuration(udevh, 1);
	if (ret < 0) {
		DBG("%s: couldn't set configuration 1\n", __func__);
		goto err1;
	}

	ret = libusb_claim_interface(udevh, 0);
	if (ret < 0) {
		DBG("%s: couldn't claim interface 0\n", __func__);
		goto err1;
	}

	ret = libusb_set_interface_alt_setting(udevh, 0, 0);
	if (ret < 0) {
		DBG("%s: couldn't set alternate setting 0\n", __func__);
		goto err2;
	}

	return 0;

err2:
	libusb_release_interface(udevh, 0);

err1:
	return ret;
}

static void release_interface(libusb_device_handle *udevh)
{
	libusb_release_interface(udevh, 0);
}

static int cleware_switch(libusb_device_handle *udevh, unsigned on)
{
	int			ret;
	unsigned char		data[3];

	if (on) {
		data[0] = 0x00;
		data[1] = 0x10;
		data[2] = 0x01;

		ret = libusb_control_transfer(udevh,
				0x21, 0x09, 0x200, 0x00, data, sizeof(data), TIMEOUT);
		if (ret < 0) {
			DBG("%s: couldn't turn on device\n", __func__);
			goto err1;
		}

		data[0] = 0x00;
		data[1] = 0x00;
		data[2] = 0x00;

		ret = libusb_control_transfer(udevh,
				0x21, 0x09, 0x200, 0x00, data, sizeof(data), TIMEOUT);
		if (ret < 0) {
			DBG("%s: couldn't turn on device\n", __func__);
			goto err1;
		}

		data[0] = 0x00;
		data[1] = 0x01;
		data[2] = 0x0f;

		ret = libusb_control_transfer(udevh,
				0x21, 0x09, 0x200, 0x00, data, sizeof(data), TIMEOUT);
		if (ret < 0) {
			DBG("%s: couldn't turn on device\n", __func__);
			goto err1;
		}
	} else {
		data[0] = 0x00;
		data[1] = 0x10;
		data[2] = 0x00;

		ret = libusb_control_transfer(udevh,
				0x21, 0x09, 0x200, 0x00, data, sizeof(data), TIMEOUT);
		if (ret < 0) {
			DBG("%s: couldn't turn on device\n", __func__);
			goto err1;
		}

		data[0] = 0x00;
		data[1] = 0x00;
		data[2] = 0x0f;

		ret = libusb_control_transfer(udevh,
				0x21, 0x09, 0x200, 0x00, data, sizeof(data), TIMEOUT);
		if (ret < 0) {
			DBG("%s: couldn't turn on device\n", __func__);
			goto err1;
		}

		data[0] = 0x00;
		data[1] = 0x01;
		data[2] = 0x00;

		ret = libusb_control_transfer(udevh,
				0x21, 0x09, 0x200, 0x00, data, sizeof(data), TIMEOUT);
		if (ret < 0) {
			DBG("%s: couldn't turn on device\n", __func__);
			goto err1;
		}
	}

	return 0;

err1:
	return ret;
}

int main(int argc, char *argv[])
{
	libusb_context		*context;
	libusb_device_handle	*udevh;
	int			ret = 0;

	if (argc < 2) {
		ret = -EINVAL;
		goto out0;
	}

	/* initialize libusb */
	libusb_init(&context);

	/* get rid of debug messages */
	libusb_set_debug(context, 0);

	udevh = libusb_open_device_with_vid_pid(context, CLEWARE_VENDOR_ID,
			CLEWARE_PRODUCT_ID);
	if (!udevh) {
		DBG("%s: couldn't find device 0x%02x:0x%02x\n", __func__,
				CLEWARE_VENDOR_ID, CLEWARE_PRODUCT_ID);
		ret = -ENODEV;
		goto out1;
	}

	ret = find_and_claim_interface(udevh);
	if (ret < 0) {
		DBG(":s: couldn't claim interface\n");
		goto out2;
	}

	ret = cleware_switch(udevh, atoi(argv[1]));
	if (ret < 0) {
		DBG("%s: couldn't switch power %s\n", __func__,
				atoi(argv[1]) ? "on" : "off");
		goto out3;
	}

out3:
	release_interface(udevh);

out2:
	libusb_close(udevh);

out1:
	libusb_exit(context);

out0:
	return ret;
}

