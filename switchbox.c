/* $(CROSS_COMPILE)gcc -Wall -O2 -g -o switchbox switchbox.c */
/**
 * switchbox.c - Server for u_serial verification
 *
 * Copyright (C) 2009 Felipe Balbi <felipe.balbi@nokia.com>
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
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <termios.h>
#include <poll.h>
#include <getopt.h>
#include <malloc.h>

#include <sys/types.h>
#include <sys/stat.h>

static unsigned debug;

#define DBG(fmt, args...)				\
	if (debug)					\
		printf(fmt, ## args)

#define ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))

/**
 * struct switchbox - Switchbox representation
 * @term:		terminal settings
 * @fd:			opened file descriptor
 * @buf:		the buffer
 */
struct switchbox {
	struct termios		*term;
	int			fd;
	uint8_t			msg;
};

/**
 * tty_init - initializes an opened tty
 * @serial:	Serial Test Context
 */
static int tty_init(struct switchbox *box)
{
	int			ret;

	box->term = malloc(sizeof(*box->term));
	if (!box->term) {
		DBG("%s: could not allocate term\n", __func__);
		ret = -ENOMEM;
		goto err0;
	}

	tcgetattr(box->fd, box->term);

	cfmakeraw(box->term);

	ret = tcflush(box->fd, TCIOFLUSH);
	if (ret < 0) {
		DBG("%s: flush failed\n", __func__);
		goto err1;
	}

	ret = tcsetattr(box->fd, TCSANOW, box->term);
	if (ret < 0) {
		DBG("%s: couldn't set attributes\n", __func__);
		goto err1;
	}

	return 0;

err1:
	free(box->term);

err0:
	return ret;
}

static void usage(char *prog)
{
	printf("Usage: %s\n\
			--tty, -t	tty device to use\n\
			--number, -n	port selector\n\
			--power, -p	enable power\n\
			--usb, -u	enable usb\n\
			--debug, -d	Enables debugging messages\n\
			--help, -h	this help\n", prog);
}

static struct option switchbox_opts[] = {
	{
		.name		= "tty",
		.has_arg	= 1,
		.val		= 't',
	},
	{
		.name		= "number",
		.has_arg	= 1,
		.val		= 'n',
	},
	{
		.name		= "power",
		.val		= 'p',
	},
	{
		.name		= "usb",
		.val		= 'u',
	},
	{
		.name		= "debug",
		.val		= 'd',
	},
	{
		.name		= "help",
		.val		= 'h',
	},
	{  }	/* Terminating entry */
};

int main(int argc, char *argv[])
{
	struct switchbox	*box;
	int			ret = 0;
	char			*tty = NULL;

	unsigned		number = 0;
	unsigned		power = 0;
	unsigned		usb = 0;

	int			optidx = 0;
	int			opt;

	uint8_t			header = 0xc0;

	while (1) {
		opt = getopt_long(argc, argv, "t:n:pudh", switchbox_opts, &optidx);
		if (opt == -1)
			break;

		switch (opt) {
		case 't':
			tty = optarg;
			break;
		case 'n':
			number = atoi(optarg);
			break;
		case 'u':
			usb = 1;
			break;
		case 'p':
			power = 1;
			break;
		case 'd':
			debug = 1;
			break;
		case 'h': /* FALLTHROUGH */
		default:
			usage(argv[0]);
			return 0;
		}
	}

	if (!tty) {
		DBG("%s: need a tty to open\n", __func__);
		ret = -EINVAL;
		goto out0;
	}

	box = malloc(sizeof(*box));
	if (!box) {
		DBG("%s: unable to allocate memory\n", __func__);
		ret = -ENOMEM;
		goto out0;
	}

	memset(box, 0x00, sizeof(*box));

	box->fd = open(tty, O_RDWR | O_NOCTTY);
	if (box->fd < 0) {
		DBG("%s: could not open %s\n", __func__, tty);
		goto out1;
	}

	ret = tty_init(box);
	if (ret < 0) {
		DBG("%s: failed to initialize tty\n", __func__);
		goto out2;
	}

	box->msg |= power << number;
	box->msg |= usb << (number + 4);

	ret = write(box->fd, &header, 1);
	if (ret < 0) {
		DBG("%s: failed writting header\n", __func__);
		goto out2;
	}

	ret = write(box->fd, &box->msg, 1);
	if (ret < 0) {
		DBG("%s: failed writting command\n", __func__);
		goto out2;
	}

out2:
	close(box->fd);

out1:
	free(box);

out0:
	return ret;
}
