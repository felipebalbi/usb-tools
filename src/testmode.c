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

static unsigned debug;

#define ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))
#define DEFAULT_TIMEOUT	2000	/* ms */

#define OPTION(n, h, v)			\
{					\
	.name		= #n,		\
	.has_arg	= h,		\
	.val		= v,		\
}

#define TEST_MODE			0x02

#define TEST_J				0x01
#define TEST_K				0x02
#define TEST_SE0_NAK			0x03
#define TEST_PACKET			0x04
#define TEST_FORCE_HS			0xc0
#define TEST_FORCE_FS			0xc1

#define TEST_BAD_DESCRIPTOR		0xff

static struct option testmode_opts[] = {
	OPTION("debug",		0, 'd'),
	OPTION("help",		0, 'h'),
	OPTION("test",		1, 't'),
	OPTION("device",	1, 'D'),
	{  } /* Terminating entry */
};

static int bad_descriptor_test(libusb_device_handle *udevh)
{
	struct libusb_device_descriptor desc;
	int			ret;
	uint8_t			buf[1024];

	memset(buf, 0x00, sizeof(buf));

	ret = libusb_get_descriptor(udevh, 0xcc, 0, buf, sizeof(buf));
	if (ret >= 0)
		return -EINVAL;

	ret = libusb_get_device_descriptor(libusb_get_device(udevh),
			&desc);
	if (ret < 0)
		return ret;

	return libusb_reset_device(udevh);
}

static int do_test(libusb_device_handle *udevh, int test)
{
	if (test == TEST_BAD_DESCRIPTOR)
		return bad_descriptor_test(udevh);

	return libusb_control_transfer(udevh,
			LIBUSB_RECIPIENT_DEVICE | LIBUSB_REQUEST_TYPE_STANDARD,
			LIBUSB_REQUEST_SET_FEATURE,
			TEST_MODE, (test << 8), NULL, 0, DEFAULT_TIMEOUT);
}

static int start_testmode(libusb_device_handle *udevh,
		char *testmode)
{
	int			test = 0;
	int			ret = -EINVAL;

	printf("Test \"%s\":        ", testmode);

	if (!strncmp(testmode, "test_j", 6))
		test = TEST_J;

	if (!strncmp(testmode, "test_k", 6))
		test = TEST_K;

	if (!strncmp(testmode, "test_se0_nak", 12))
		test = TEST_SE0_NAK;

	if (!strncmp(testmode, "test_packet", 11))
		test = TEST_PACKET;

	if (!strncmp(testmode, "test_force_hs", 13))
		test = TEST_FORCE_HS;

	if (!strncmp(testmode, "test_force_fs", 13))
		test = TEST_FORCE_FS;

	if (!strncmp(testmode, "bad_descriptor", 14))
		test = TEST_BAD_DESCRIPTOR;

	ret = do_test(udevh, test);
	if (ret < 0)
		printf("failed\n");
	else
		printf("success\n");

	return ret;
}

static void usage(char *cmd)
{
	fprintf(stdout, "%s -D VID:PID -t testmode\n\
		test_packet, test_se0_nak, test_k, test_j,\n\
		test_force_hs, test_force_fs\n", cmd);
}

int main(int argc, char *argv[])
{
	libusb_context		*context;
	libusb_device_handle	*udevh;

	unsigned		vid = 0;
	unsigned		pid = 0;

	int			ret = 0;

	char			*testmode = NULL;

	while (1) {
		int		optidx;
		int		opt;

		char		*token;

		opt = getopt_long(argc, argv, "t:D:dh", testmode_opts, &optidx);
		if (opt == -1)
			break;

		switch (opt) {
		case 't':
			testmode = optarg;
			break;
		case 'D':
			token = strtok(optarg, ":");
			vid = strtoul(token, NULL, 16);
			token = strtok(NULL, ":");
			pid = strtoul(token, NULL, 16);
			break;
		case 'd':
			debug = 1;
			break;
		case 'h': /* FALLTHROUGH */
		default:
			usage(argv[0]);
			exit(-1);
		}
	}

	libusb_init(&context);

	udevh = libusb_open_device_with_vid_pid(context, vid, pid);
	if (!udevh) {
		ret = -ENODEV;
		goto out0;
	}

	ret = start_testmode(udevh, testmode);
	if (ret < 0)
		goto out1;

out1:
	libusb_close(udevh);

out0:
	libusb_exit(context);

	return ret;
}

