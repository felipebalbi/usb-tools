/* $(CROSS_COMPILE)gcc -Wall -O2 -g -o seriald seriald.c */
/**
 * seriald.c - Server for u_serial verification
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

#define _GNU_SOURCE

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
 * struct usb_serial_test - Context of our test
 * @term:		terminal settings
 * @pfd:		poll fd
 * @amount_read:	amount read until now
 * @amount_write:	amount written until now
 * @size:		buffer size
 * @fd:			opened file descriptor
 * @buf:		the buffer
 */
struct usb_serial_test {
	struct termios		*term;
	struct pollfd		*pfd;

	uint64_t		amount_read;
	uint64_t		amount_write;

	unsigned		size;
	int			fd;
	char			*buf;
};

/**
 * alloc_buffer - allocates a @size buffer
 * @size:	size of buffer
 */
static char *alloc_buffer(unsigned size)
{
	char			*tmp;

	if (size == 0) {
		DBG("%s: cannot allocate zero sized buffer\n", __func__);
		return NULL;
	}

	tmp = malloc(size);
	if (!tmp)
		return NULL;

	return tmp;
}

/**
 * tty_init - initializes an opened tty
 * @serial:	Serial Test Context
 */
static int tty_init(struct usb_serial_test *serial)
{
	int			ret;

	serial->term = malloc(sizeof(*serial->term));
	if (!serial->term) {
		DBG("%s: could not allocate term\n", __func__);
		ret = -ENOMEM;
		goto err0;
	}

	tcgetattr(serial->fd, serial->term);

	cfmakeraw(serial->term);

	ret = tcflush(serial->fd, TCIOFLUSH);
	if (ret < 0) {
		DBG("%s: flush failed\n", __func__);
		goto err1;
	}

	ret = tcsetattr(serial->fd, TCSANOW, serial->term);
	if (ret < 0) {
		DBG("%s: couldn't set attributes\n", __func__);
		goto err1;
	}

	return 0;

err1:
	free(serial->term);

err0:
	return ret;
}

/**
 * do_write - writes our buffer to fd
 * @serial:	Serial Test Context
 */
static int do_write(struct usb_serial_test *serial, uint16_t bytes)
{
	int			done = 0;
	int			ret;

	char			*buf = serial->buf;

	DBG("%s: writting %d bytes\n", __func__, bytes);

	while (done < bytes) {
		ret = write(serial->fd, buf + done, bytes - done);
		if (ret < 0) {
			DBG("%s: failed to write %u bytes\n",
					__func__, bytes);
			goto err;
		}

		done += ret;
		serial->amount_write += ret;
	}

	return done;

err:
	return ret;
}

/**
 * do_read - reads from fd to our buffer
 * @serial:	Serial Test Context
 */
static int do_read(struct usb_serial_test *serial)
{
	unsigned		size = serial->size;
	int			done = 0;
	int			ret;

	char			*buf = serial->buf;

	while (done < size) {
		ret = read(serial->fd, buf + done, size - done);
		if (ret < 0) {
			DBG("%s: failed to read\n", __func__);
			goto err;
		}

		size = (buf[0] << 8) | buf[1];

		done += ret;
		serial->amount_read += ret;
	}

	DBG("%s: read %d bytes\n", __func__, done);

	return done;

err:
	return ret;
}

/**
 * do_poll - polls our fd for incoming data
 * @serial:	Serial Test Context
 */
static int do_poll(struct usb_serial_test *serial)
{
	int			ret = -1;

	serial->pfd->fd = serial->fd;
	serial->pfd->events = POLLIN;

	ret = poll(serial->pfd, 1, -1);
	if (ret <= 0) {
		DBG("%s: poll failed\n", __func__);
		goto err;
	}

	return 0;

err:
	return ret;
}

/**
 * do_test - poll, read the data and write it back
 * @serial:	Serial Test Context
 */
static int do_test(struct usb_serial_test *serial)
{
	uint16_t		bytes;
	int			ret;

	ret = do_poll(serial);
	if (ret < 0)
		goto err;

	ret = do_read(serial);
	if (ret < 0)
		goto err;

	bytes = ret;

	ret = do_write(serial, bytes);
	if (ret < 0)
		goto err;

	return 0;

err:
	return ret;
}

static void usage(char *prog)
{
	printf("Usage: %s\n\
			--tty, -t	tty device to use\n\
			--size, -s	size of internal buffer\n\
			--debug, -d	Enables debugging messages\n\
			--help, -h	this help\n", prog);
}

static struct option serial_opts[] = {
	{
		.name		= "tty",
		.has_arg	= 1,
		.val		= 't',
	},
	{
		.name		= "size",
		.has_arg	= 1,
		.val		= 's',
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
	struct usb_serial_test	*serial;

	unsigned		size = 0;
	int			ret = 0;

	char			*tty = NULL;

	while (ARRAY_SIZE(serial_opts)) {
		int		optidx = 0;
		int		opt;

		opt = getopt_long(argc, argv, "t:s:dh", serial_opts, &optidx);
		if (opt < 0)
			break;

		switch (opt) {
		case 't':
			tty = optarg;
			break;
		case 's':
			size = atoi(optarg);
			if (size == 0) {
				DBG("%s: can't do it with zero length buffer\n",
						__func__);
				ret = -EINVAL;
				goto err0;
			}
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
		goto err0;
	}

	serial = malloc(sizeof(*serial));
	if (!serial) {
		DBG("%s: unable to allocate memory\n", __func__);
		ret = -ENOMEM;
		goto err0;
	}

	memset(serial, 0x00, sizeof(*serial));

	DBG("%s: buffer size %d\n", __func__, size);

	serial->size = size;

	serial->buf = alloc_buffer(size);
	if (!serial->buf) {
		DBG("%s: failed to allocate buffer\n", __func__);
		goto err1;
	}

	DBG("%s: opening %s\n", __func__, tty);

	serial->fd = open(tty, O_RDWR | O_NOCTTY);
	if (serial->fd < 0) {
		DBG("%s: could not open %s\n", __func__, tty);
		goto err2;
	}

	ret = tty_init(serial);
	if (ret < 0) {
		DBG("%s: failed to initialize tty\n", __func__);
		goto err3;
	}

	serial->pfd = malloc(sizeof(*serial->pfd));
	if (!serial->pfd) {
		DBG("%s: could not allocate pfd\n", __func__);
		ret = -ENOMEM;
		goto err4;
	}

	memset(serial->pfd, 0x00, sizeof(*serial->pfd));

	while (1) {
		ret = do_test(serial);
		if (ret < 0) {
			DBG("%s test failed\n", __func__);
			goto err5;
		}

		printf("[ using %s read %10.02f MB wrote %10.02f MB ]\r", tty,
				(float) serial->amount_read / 1024 / 1024,
				(float) serial->amount_write / 1024 / 1024);

		fflush(stdout);
	}

	free(serial->pfd);
	free(serial->term);
	close(serial->fd);
	free(serial->buf);
	free(serial);

	return 0;

err5:
	free(serial->pfd);

err4:
	free(serial->term);

err3:
	close(serial->fd);

err2:
	free(serial->buf);

err1:
	free(serial);

err0:
	return ret;
}

