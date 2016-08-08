/**
 * device-reset.c - Infinite loop to reset a USB device.
 *
 * Copyright (C) 2013-2016 Felipe Balbi <felipe.balbi@linux.intel.com>
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

#include <libusb.h>

#define ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))
#define DEFAULT_TIMEOUT	2000	/* ms */

#define OPTION(n, h, v)			\
{					\
	.name		= n,		\
	.has_arg	= h,		\
	.val		= v,		\
}

static struct option device_reset_opts[] = {
	OPTION("help",		0, 'h'),
	OPTION("count",		1, 'c'),
	OPTION("device",	1, 'D'),
	{  }	/* Terminating entry */
};

static void usage(char *cmd)
{
	fprintf(stdout, "%s -D VID:PID -c count\n", cmd);
}

int main(int argc, char *argv[])
{
	libusb_context		*context;
	libusb_device_handle	*udevh;

	unsigned		count = 1;
	unsigned		vid = 0;
	unsigned		pid = 0;

	int			ret = 0;
	int			i;

	while (1) {
		int		optidx;
		int		opt;

		char		*token;

		opt = getopt_long(argc, argv, "D:c:h", device_reset_opts, &optidx);
		if (opt == -1)
			break;

		switch (opt) {
		case 'D':
			token = strtok(optarg, ":");
			vid = strtoul(token, NULL, 16);
			token = strtok(NULL, ":");
			pid = strtoul(token, NULL, 16);
			break;
		case 'c':
			count = strtoul(optarg, NULL, 10);
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
		perror("open");
		ret = -ENODEV;
		goto out0;
	}

	for (i = 0; i < count; i++) {
		ret = libusb_reset_device(udevh);
		printf("Reset #%d: ", i + 1);

		if (ret < 0) {
			printf("FAILED\n");
		} else {
			printf("PASSED\n");
		}
	}

	libusb_close(udevh);

out0:
	libusb_exit(context);

	return ret;
}
