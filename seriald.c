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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <poll.h>
#include <getopt.h>
#include <malloc.h>

#include <sys/types.h>
#include <sys/stat.h>

static unsigned char debug;

#define ENABLE_DBG()	debug = 1

#define DBG(fmt, args...)				\
	if (debug)					\
		printf(fmt, ## args)

#define ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))

/**
 * struct usb_serial_test - Context of our test
 * @fd:			opened file descriptor
 * @amount_read:	amount read until now
 * @amount_write:	amount written until now
 * @size:		buffer size
 * @buf:		the buffer
 */
struct usb_serial_test {
	int			fd;

	uint64_t		amount_read;
	uint64_t		amount_write;

	unsigned		size;
	char			*buf;
};

/**
 * tty_init - initializes an opened tty
 * @fd:		Device Handle
 */
static int tty_init(int fd)
{
	int 		ret = 0;
	struct termios	term;

	tcgetattr(fd, &term);

	cfmakeraw(&term);

	ret = tcflush(fd, TCIOFLUSH);
	if (ret < 0) {
		DBG("%s: flush failed\n", __func__);
		goto err;
	}

	ret = tcsetattr(fd, TCSANOW, &term);
	if (ret < 0) {
		DBG("%s: couldn't set attributes\n", __func__);
		goto err;
	}
err:
	return ret;
}

/**
 * do_write - writes our buffer to fd
 * @serial:	Serial Test Context
 */
static int do_write(struct usb_serial_test *serial, uint16_t bytes)
{
	int		done = 0;
	int		ret;

	char		*buf = serial->buf;

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

	fsync(serial->fd);

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
	unsigned	size = serial->size;
	int		done = 0;
	int		ret;

	char		*buf = serial->buf;

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
	int		ret = -1;
	struct pollfd	pfd;

	pfd.fd = serial->fd;
	pfd.events = POLLIN;

	ret = poll(&pfd, 1, -1);
	if (ret <= 0) {
		DBG("%s: poll failed\n", __func__);
		goto err;
	}

	return 0;

err:
	return ret;
}

static struct usb_serial_test *do_open(const char *pathname, int flags)
{
	struct usb_serial_test	*serial;
	struct stat		st;

	serial = malloc(sizeof(*serial));
	if (!serial) {
		DBG("%s: could not allocate memory\n", __func__);
		goto err0;
	}

	serial->fd = open(pathname, flags);
	if (serial->fd < 0) {
		DBG("%s: open failed\n", __func__);
		goto err1;
	}

	/* Make sure the file is a character device */
	if (fstat(serial->fd, &st)) {
		DBG("%s failed to stat %s\n", __func__, pathname);
		goto err2;
	}
	if (!S_ISCHR(st.st_mode)) {
		DBG("%s: \"%s\" is not character device\n",
			__func__, pathname);
		errno = EBADF;
		goto err2;
	}

	if (isatty(serial->fd)) {
		DBG("%s is tty\n", pathname);

		if (tty_init(serial->fd) < 0) {
			DBG("%s: tty_init failed\n", __func__);
			goto err2;
		}
	}

	return serial;

err2:
	close(serial->fd);
err1:
	free(serial);
err0:
	return NULL;
}

/**
 * do_test - poll, read the data and write it back
 * @serial:	Serial Test Context
 */
static int do_test(struct usb_serial_test *serial)
{
	uint16_t	bytes;
	int		ret;

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
	fprintf(stderr, "Usage: %s\n"
		"	--file, -f	character device to use\n"
		"	--size, -s	size of internal buffer\n"
		"	--debug, -d	Enables debugging messages\n"
		"	--help, -h	this help\n", prog);
}

static struct option serial_opts[] = {
	{
		.name		= "file",
		.has_arg	= 1,
		.val		= 'f',
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
	char			*file = NULL;

	while (ARRAY_SIZE(serial_opts)) {
		int		optidx = 0;
		int		opt;

		opt = getopt_long(argc, argv, "f:s:dh", serial_opts, &optidx);
		if (opt < 0)
			break;

		switch (opt) {
		case 'f':
			file = optarg;
			break;
		case 's':
			size = atoi(optarg);
			break;
		case 'd':
			ENABLE_DBG();
			break;
		case 'h': /* FALLTHROUGH */
		default:
			usage(argv[0]);
			return 1;
		}
	}

	if (!file || !size) {
		fprintf(stderr, "%s%s",
			file ? "" : "seriald: need a file to open\n",
			size ? "" : "seriald: need size for the buffer\n");
		fprintf(stderr, "Try `%s --help' for more information.\n",
			argv[0]);
		ret = 1;
		goto err0;
	}

	DBG("%s: opening %s\n", __func__, file);

	serial = do_open(file, O_RDWR | O_NOCTTY);
	if (!serial) {
		fprintf(stderr, "%s: failed to open %s: %s\n",
			argv[0], file, strerror(errno));
		ret = 1;
		goto err0;
	}

	DBG("%s: buffer size %d\n", __func__, size);

	serial->size = size;

	serial->buf = malloc(size);
	if (!serial->buf) {
		fprintf(stderr, "%s: failed to allocate buffer\n", argv[0]);
		ret = 1;
		goto err1;
	}

	while (1) {
		ret = do_test(serial);
		if (ret < 0) {
			fprintf(stderr, "%s: test failed: %s\n",
				__func__, strerror(errno));
			break;
		}
	}

	free(serial->buf);
err1:
	close(serial->fd);
	free(serial);
err0:
	return ret;
}
