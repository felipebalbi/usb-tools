/*
 * SPDX-License-Identifier: GPL-3.0
 * Copyright (C) 2016 Felipe Balbi <felipe.balbi@linux.intel.com>
 */

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
#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <libusb-1.0/libusb.h>

#define __maybe_unused	__attribute__((unused))

#define ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))
#define DEFAULT_TIMEOUT	5000	/* ms */

#define X_SIZE		1920
#define Y_SIZE		1080

#define FRAME_SIZE	((X_SIZE) * (Y_SIZE))
#define UV_SIZE		((FRAME_SIZE) / 2)

#define FULL_SIZE	((FRAME_SIZE) + (UV_SIZE))

static unsigned char frame[FULL_SIZE];

#define OPTION(n, h, v)			\
{					\
	.name		= #n,		\
	.has_arg	= h,		\
	.val		= v,		\
}

static struct option testmode_opts[] = {
	OPTION("device",	1, 'D'),
	OPTION("help",		0, 'h'),
	{  } /* Terminating entry */
};

static int do_test(libusb_device_handle *udevh)
{
	int				transferred;
	int				ret;
	int				i;

	ret = libusb_claim_interface(udevh, 0);
	if (ret < 0) {
		fprintf(stderr, "Can't claim interface 0\n");
		return ret;
	}


	for (i = 0; i < 10; i++) {
		ret = libusb_bulk_transfer(udevh, 1, frame, FULL_SIZE,
				&transferred, DEFAULT_TIMEOUT);
		if (ret < 0) {
			fprintf(stderr, "failed %d/%d -> %s\n", transferred,
					FULL_SIZE, libusb_error_name(ret));
			libusb_release_interface(udevh, 0);
			return ret;
		}

		if (transferred < FULL_SIZE) {
			fprintf(stderr, "Couldn't transfer all bytes %d/%d",
					transferred, FULL_SIZE);
			libusb_release_interface(udevh, 0);
			return -1;
		}
	}

	return 0;
}

static void usage(const char *cmd)
{
	fprintf(stdout, "Usage: %s -D vid:pid [-h]\n", cmd);
}

int main(int argc, char *argv[])
{
	libusb_context		*context;
	libusb_device_handle	*udevh;

	unsigned		vid = 0xaaaa;
	unsigned		pid = 0xbbbb;

	int			ret = 0;
	int			i;

	while (1) {
		int		optidx;
		int		opt;

		char		*token;

		opt = getopt_long(argc, argv, "hD:", testmode_opts, &optidx);
		if (opt == -1)
			break;

		switch (opt) {
		case 'D':
			token = strtok(optarg, ":");
			vid = strtoul(token, NULL, 16);
			token = strtok(NULL, ":");
			pid = strtoul(token, NULL, 16);
			break;
		case 'h':
			usage(argv[0]);
			exit(0);
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

	for (i = 0; i < FULL_SIZE; i++)
		frame[i] = (i % 94) + 33; /* only printable ascii characters */

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

