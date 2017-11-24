/*
 * SPDX-License-Identifier: GPL-3.0
 * Copyright (C) 2013-2016 Felipe Balbi <felipe.balbi@linux.intel.com>
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
	{  } /* Terminating entry */
};

static void usage(char *cmd)
{
	fprintf(stdout, "%s -D VID:PID -c count\n", cmd);
}

int main(int argc, char *argv[])
{
	libusb_context		*context;
	libusb_device_handle	*udevh;

	unsigned int		count = 1;
	unsigned int		vid = 0;
	unsigned int		pid = 0;
	unsigned int		i;

	int			ret = 0;

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
