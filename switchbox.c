/* $(CROSS_COMPILE)gcc -Wall -O2 -g -o switchbox switchbox.c */
/**
 * switchbox.c - Server for u_serial verification
 *
 * Copyright (C) 2009-2016 Felipe Balbi <felipe.balbi@linux.intel.com>
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

#define SWITCHBOX_CMD_READ	0x80
#define SWITCHBOX_CMD_WRITE	0xc0

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

/**
 * switchbox_read - issue a read command to switchbox
 * @box:	the box we want to read from
 */
static int switchbox_read(struct switchbox *box)
{
	struct pollfd		pfd;
	int			ret;
	uint8_t			cmd;

	cmd = SWITCHBOX_CMD_READ;

	ret = write(box->fd, &cmd, 1);
	if (ret < 0) {
		DBG("%s: failed to send READ command\n", __func__);
		return ret;
	}

	/* start polling for data available */
	pfd.fd = box->fd;
	pfd.events = POLLIN;
	pfd.revents = 0;

	ret = poll(&pfd, 1, -1);
	if (ret <= 0) {
		DBG("%s: failed polling for data available\n", __func__);
		return -EINVAL;
	}

	ret = read(box->fd, &box->msg, 1);
	if (ret < 0) {
		DBG("%s: failed to read status\n", __func__);
		return ret;
	}

	return 0;
}

/**
 * switchbox_write - issoe a write command to switchbox
 * @box:	the box we want to write to
 */
static int switchbox_write(struct switchbox *box)
{
	struct pollfd		pfd;
	int			ret;
	uint8_t			cmd;

	cmd = SWITCHBOX_CMD_WRITE;

	ret = write(box->fd, &cmd, 1);
	if (ret < 0) {
		DBG("%s: failed to send WRITE command\n", __func__);
		return ret;
	}

	pfd.fd = box->fd;
	pfd.events = POLLOUT;
	pfd.revents = 0;

	ret = poll(&pfd, 1, -1);
	if (ret <= 0) {
		DBG("%s: failed polling for write\n", __func__);
		return -EINVAL;
	}

	ret = write(box->fd, &box->msg, 1);
	if (ret < 0) {
		DBG("%s: failed writting message\n", __func__);
		return ret;
	}

	return 0;
}

static void usage(char *prog)
{
	printf("Usage: %s\n\
			--tty, -t	tty device to use\n\
			--number, -n	port selector\n\
			--power, -p	enable power\n\
			--usb, -u	enable usb\n\
			--debug, -d	Enables debugging messages\n\
			--help, -h	this help\n\
			\n\
		Several port numbers can be set simultaneously e.g.:\n\
		%s -n0 -n1 -n2 -n3 -pu\n\
		The above will turn on all power and usb ports\n", prog, prog);
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

	while (1) {
		opt = getopt_long(argc, argv, "t:n:pudh", switchbox_opts, &optidx);
		if (opt == -1)
			break;

		switch (opt) {
		case 't':
			tty = optarg;
			break;
		case 'n':
			number |= (1 << atoi(optarg));
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

	ret = switchbox_read(box);
	if (ret < 0) {
		DBG("%s: failed to read\n", __func__);
		goto out2;
	}

	if (power)
		box->msg |= number;
	else
		box->msg &= ~(number);

	if (usb)
		box->msg |= (number << 4);
	else
		box->msg &= ~(number << 4);

	ret = switchbox_write(box);
	if (ret < 0)
		DBG("%s: failed to write\n", __func__);

out2:
	close(box->fd);

out1:
	free(box);

out0:
	return ret;
}
