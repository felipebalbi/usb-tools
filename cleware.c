/* $(CROSS_COMPILE)gcc -Wall -O2 -g -lusb-1.0 -o cleware cleware.c */
/**
 * cleware.c - Cleware USB-Controlled Power Switch
 *
 * Copyright (C) 2010 Felipe Balbi <felipe.balbi@nokia.com>
 *
 * This file is part of the USB Verification Tools Project
 *
 * USB Tools is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public Liicense as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * USB Tools is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with USB Tools. If not, see <http://www.gnu.org/licenses/>.
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

struct usb_device_id {
	unsigned		idVendor;
	unsigned		idProduct;
};

#define USB_DEVICE(v, p)		\
{					\
	.idVendor		= v,	\
	.idProduct		= p,	\
}

static struct usb_device_id cleware_id[] = {
	USB_DEVICE(CLEWARE_VENDOR_ID, CLEWARE_PRODUCT_ID),
};

static int debug;

#define DBG(fmt, args...)			\
	if (debug)				\
		fprintf(stdout, fmt, ## args)

#define TIMEOUT			1000	/* ms */
#define ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))

static int match_device_id(libusb_device *udev)
{
	struct libusb_device_descriptor	desc;
	int				match = 0;
	int				ret;
	int				i;

	ret = libusb_get_device_descriptor(udev, &desc);
	if (ret < 0) {
		DBG("%s: failed to get device descriptor\n", __func__);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(cleware_id); i++) {
		DBG("%s: idVendor %02x idProduct %02x\n",__func__,
				desc.idVendor, desc.idProduct);
		if ((desc.idVendor == cleware_id[i].idVendor) &&
				(desc.idProduct == cleware_id[i].idProduct))
			match = 1;
	}

	return match;
}

static int match_device_serial_number(libusb_device *udev, unsigned iSerial)
{
	struct libusb_device_descriptor	desc;

	libusb_device_handle		*tmp;

	unsigned char			serial;
	int				ret;

	ret = libusb_open(udev, &tmp);
	if (ret < 0 || !tmp) {
		DBG("%s: couldn't open device\n", __func__);
		goto out0;
	}

	if (libusb_kernel_driver_active(tmp, 0)) {
		ret = libusb_detach_kernel_driver(tmp, 0);
		if (ret < 0) {
			DBG("%s: couldn't detach kernel driver\n", __func__);
			goto out1;
		}
	}

	ret = libusb_get_string_descriptor(tmp, desc.iSerialNumber,
			0x0409, &serial, sizeof(serial));

	if (ret < 0) {
		DBG("%s: failed to get string descriptor\n", __func__);
		goto out1;
	}

	if (serial != iSerial) {
		DBG("%s: not the serial number we want\n", __func__);
		goto out2;
	}

out2:
	libusb_attach_kernel_driver(tmp, 0);

out1:
	libusb_close(tmp);

out0:
	return ret;
}

static libusb_device_handle *find_and_open_device(libusb_device **list,
		ssize_t count, unsigned iSerial)
{
	libusb_device_handle		*udevh = NULL;
	libusb_device			*found = NULL;
	int				ret;
	int				i;

	for (i = 0; i < count; i++) {
		libusb_device *udev = list[i];

		ret = match_device_id(udev);
		if (ret <= 0) {
			DBG("%s: couldn't match device id\n", __func__);
			goto out;
		}

		if (iSerial) {
			ret = match_device_serial_number(udev, iSerial);
			if (ret < 0) {
				DBG("%s: couldn't match serial number\n", __func__);
				found = NULL;
				break;
			}
		}

		found = udev;
		break;
	}

	ret = libusb_open(found, &udevh);
	if (ret < 0 || !found) {
		DBG("%s: couldn't open device\n", __func__);
		goto out;
	}

out:
	return udevh;
}

static int find_and_claim_interface(libusb_device_handle *udevh)
{
	int			ret;

	if (libusb_kernel_driver_active(udevh, 0)) {
		ret = libusb_detach_kernel_driver(udevh, 0);
		if (ret < 0) {
			DBG("%s: couldn't detach kernel driver\n", __func__);
			goto out0;
		}
	}

	ret = libusb_set_configuration(udevh, 1);
	if (ret < 0) {
		DBG("%s: couldn't set configuration 1\n", __func__);
		goto out0;
	}

	ret = libusb_claim_interface(udevh, 0);
	if (ret < 0) {
		DBG("%s: couldn't claim interface 0\n", __func__);
		goto out0;
	}

	ret = libusb_set_interface_alt_setting(udevh, 0, 0);
	if (ret < 0) {
		DBG("%s: couldn't set alternate setting 0\n", __func__);
		goto out1;
	}

	return 0;

out1:
	libusb_release_interface(udevh, 0);

out0:
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

	/*
	 * the following sequence was sniffed from the example
	 * application provided by the manufacturer.
	 *
	 * it's known to work with the following device:
	 * http://www.cleware.de/produkte/p-usbswitch-E.html
	 */

	if (on) {
		data[0] = 0x00;
		data[1] = 0x10;
		data[2] = 0x01;

		ret = libusb_control_transfer(udevh,
				0x21, 0x09, 0x200, 0x00, data, sizeof(data), TIMEOUT);
		if (ret < 0) {
			DBG("%s: couldn't turn on device\n", __func__);
			goto out;
		}

		data[0] = 0x00;
		data[1] = 0x00;
		data[2] = 0x00;

		ret = libusb_control_transfer(udevh,
				0x21, 0x09, 0x200, 0x00, data, sizeof(data), TIMEOUT);
		if (ret < 0) {
			DBG("%s: couldn't turn on device\n", __func__);
			goto out;
		}

		data[0] = 0x00;
		data[1] = 0x01;
		data[2] = 0x0f;

		ret = libusb_control_transfer(udevh,
				0x21, 0x09, 0x200, 0x00, data, sizeof(data), TIMEOUT);
		if (ret < 0) {
			DBG("%s: couldn't turn on device\n", __func__);
			goto out;
		}
	} else {
		data[0] = 0x00;
		data[1] = 0x10;
		data[2] = 0x00;

		ret = libusb_control_transfer(udevh,
				0x21, 0x09, 0x200, 0x00, data, sizeof(data), TIMEOUT);
		if (ret < 0) {
			DBG("%s: couldn't turn on device\n", __func__);
			goto out;
		}

		data[0] = 0x00;
		data[1] = 0x00;
		data[2] = 0x0f;

		ret = libusb_control_transfer(udevh,
				0x21, 0x09, 0x200, 0x00, data, sizeof(data), TIMEOUT);
		if (ret < 0) {
			DBG("%s: couldn't turn on device\n", __func__);
			goto out;
		}

		data[0] = 0x00;
		data[1] = 0x01;
		data[2] = 0x00;

		ret = libusb_control_transfer(udevh,
				0x21, 0x09, 0x200, 0x00, data, sizeof(data), TIMEOUT);
		if (ret < 0) {
			DBG("%s: couldn't turn on device\n", __func__);
			goto out;
		}
	}

out:
	return ret;
}

static void usage(char *name)
{
	fprintf(stdout, "usage: %s [0 | 1] [serial number]\n", name);
}

int main(int argc, char *argv[])
{
	libusb_context		*context;
	libusb_device_handle	*udevh;
	libusb_device		**list;

	unsigned		iSerial = 0;

	ssize_t			count;

	int			ret = 0;
	char			*serial = NULL;

	if (argc < 2) {
		usage(argv[0]);
		ret = -EINVAL;
		goto out0;
	}

	if (argc == 3)
		iSerial = strtoul(argv[2], &serial, 16);

	/* initialize libusb */
	libusb_init(&context);

	/* get rid of debug messages */
	libusb_set_debug(context, 0);

	count = libusb_get_device_list(context, &list);
	if (count < 0) {
		DBG("%s: couldn't get device list\n", __func__);
		goto out1;
	}

	udevh = find_and_open_device(list, count, iSerial);
	if (!udevh) {
		DBG("%s: couldn't find a suitable device\n", __func__);
		goto out2;
	}

	ret = find_and_claim_interface(udevh);
	if (ret < 0) {
		DBG("%s: couldn't claim interface\n", __func__);
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
	libusb_free_device_list(list, 1);

out1:
	libusb_exit(context);

out0:
	return ret;
}

