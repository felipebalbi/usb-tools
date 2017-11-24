/*
 * SPDX-License-Identifier: GPL-3.0
 * Copyright (C) 2017 Felipe Balbi <felipe.balbi@linux.intel.com>
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

static struct option control_opts[] = {
	OPTION("bmRequestType",	1, 't'),
	OPTION("bRequest",	1, 'r'),
	OPTION("wValue",	1, 'v'),
	OPTION("wIndex",	1, 'i'),
	OPTION("wLength",	1, 'l'),
	OPTION("count",		1, 'c'),
	OPTION("help",		0, 'h'),
	OPTION("device",	1, 'D'),
	{  } /* Terminating entry */
};

static void hexdump(char *buf, unsigned size)
{
	unsigned int		i;
	char			ascii[17];

	ascii[16] = '\0';

	if (!size)
		return;

	for (i = 0; i < size; i++) {
		if ((unsigned char) buf[i] >= ' ' &&
		    (unsigned char) buf[i] <= '~') {
			ascii[i % 16] = (unsigned char) buf[i];
		} else {
			ascii[i % 16] = '.';
		}

		if (i && ((i % 16) == 0))
			printf(" | %s\n", ascii);

		printf("%02x ", (uint8_t) buf[i]);
	}
	printf("\n");
}

int main(int argc, char *argv[])
{
	libusb_context		*context;
	libusb_device_handle	*udevh;
	uint8_t			*buf;

	int			ret = 0;
	int			i;

	libusb_init(&context);

	udevh = libusb_open_device_with_vid_pid(context, 0x8087, 0xbeef);
	if (!udevh) {
		perror("couldn't open device");
		ret = -ENODEV;
		goto out;
	}

	libusb_claim_interface(udevh, 0);

	buf = malloc(4096);
	if (!buf) {
		printf("can't allocate\n");
		return -1;
	}

	for (i = 100; i; --i) {
		int transferred;

		ret = libusb_bulk_transfer(udevh, LIBUSB_ENDPOINT_IN | 2,
					   buf, 4096, &transferred,
					   1000);
		if (ret < 0) {
			if (ret == LIBUSB_ERROR_TIMEOUT)
				continue;

			printf("%s: can't transmit\n", libusb_error_name(ret));
			break;
		}

		hexdump(buf, transferred);
	}

	libusb_close(udevh);

out:
	libusb_exit(context);
	free(buf);

	return ret;
}
