/*
 * SPDX-License-Identifier: GPL-3.0
 * Copyright (C) 2010-2016 Felipe Balbi <felipe.balbi@linux.intel.com>
 */

#include <config.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <libusb-1.0/libusb.h>

#define __maybe_unused	__attribute__((unused))

#define ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))
#define DEFAULT_TIMEOUT	2000	/* ms */

#define OPTION(n, h, v)			\
{					\
	.name		= #n,		\
	.has_arg	= h,		\
	.val		= v,		\
}

static struct option testmode_opts[] = {
	OPTION("device",	1, 'D'),
	{  } /* Terminating entry */
};

/* redefined here until libusb defines its own */
struct usb_ss_ep_comp_descriptor {
	uint8_t		bLength;
	uint8_t		bDescriptorType;

	uint8_t		bMaxBurst;
	uint8_t		bmAttributes;
	uint16_t	wBytesPerInterval;
} __attribute__ ((packed));

#define USB_SS_EP_COMP_SIZE	0x06
#define USB_DT_SS_ENDPOINT_COMP	0x30

static void __maybe_unused hexdump(const uint8_t *buf, unsigned size)
{
	unsigned int			i;

	for (i = 0; i < size; i++) {
		if (i && ((i % 16) == 0))
			printf("\n");
		printf("%02x ", buf[i]);
	}

	printf("\n");
}

static int check_for_companion(const uint8_t *buf, int size)
{
	struct usb_ss_ep_comp_descriptor comp;

	if (size != USB_SS_EP_COMP_SIZE)
		return -1;

	memcpy(&comp, buf, USB_SS_EP_COMP_SIZE);

	/* if size is wrong, fail */
	if (comp.bLength != USB_SS_EP_COMP_SIZE)
		return -1;

	/* if type is wrong, fail */
	if (comp.bDescriptorType != USB_DT_SS_ENDPOINT_COMP)
		return -1;

	return 0;
}

static int check_endpoints(const struct libusb_endpoint_descriptor *eps,
		int num)
{
	int				ret;
	int				i;

	for (i = 0; i < num; i++) {
		ret = check_for_companion(eps[i].extra, eps[i].extra_length);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int check_alt_settings(const struct libusb_interface_descriptor *alt,
		int num)
{
	int				ret;
	int				i;

	for (i = 0; i < num; i++) {
		ret = check_endpoints(alt->endpoint, alt->bNumEndpoints);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int check_interfaces(const struct libusb_interface *intf, int num)
{
	int				ret;
	int				i;

	for (i = 0; i < num; i++) {
		ret = check_alt_settings(intf->altsetting, intf->num_altsetting);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int check_configurations(libusb_device *udev, int num)
{
	struct libusb_config_descriptor *config;
	int				ret;
	int				i;

	for (i = 0; i < num; i++) {
		ret = libusb_get_config_descriptor(udev, i, &config);
		if (ret < 0) {
			perror("failed to get config descriptor");
			return ret;
		}

		ret = check_interfaces(config->interface, config->bNumInterfaces);
		if (ret < 0)
			return ret;

		libusb_free_config_descriptor(config);
	}

	return 0;
}

static int do_test(libusb_device_handle *udevh)
{
	struct libusb_device_descriptor	device_desc;
	struct libusb_device		*udev;
	int				ret;


	udev = libusb_get_device(udevh);

	ret = libusb_get_device_descriptor(udev, &device_desc);
	if (ret < 0) {
		perror("failed to get device descriptor");
		return ret;
	}

	return check_configurations(udev, device_desc.bNumConfigurations);
}

int main(int argc, char *argv[])
{
	libusb_context		*context;
	libusb_device_handle	*udevh;

	unsigned		vid = 0;
	unsigned		pid = 0;

	int			ret = 0;

	while (1) {
		int		optidx;
		int		opt;

		char		*token;

		opt = getopt_long(argc, argv, "D:", testmode_opts, &optidx);
		if (opt == -1)
			break;

		switch (opt) {
		case 'D':
			token = strtok(optarg, ":");
			vid = strtoul(token, NULL, 16);
			token = strtok(NULL, ":");
			pid = strtoul(token, NULL, 16);
			break;
		default:
			exit(-1);
		}
	}

	libusb_init(&context);

	udevh = libusb_open_device_with_vid_pid(context, vid, pid);
	if (!udevh) {
		perror("couldn't open device");
		ret = -ENODEV;
		goto out0;
	}

	printf("SuperSpeed Companion Descriptor Test...		");

	ret = do_test(udevh);
	if (ret < 0) {
		printf("failed\n");
		goto out1;
	}

	printf("passed\n");

out1:
	libusb_close(udevh);

out0:
	libusb_exit(context);

	return ret;
}

