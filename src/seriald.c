/*
 * SPDX-License-Identifier: GPL-3.0
 * Copyright (C) 2009-2016 Felipe Balbi <felipe.balbi@linux.intel.com>
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <poll.h>
#include <getopt.h>
#include <malloc.h>
#include <sys/prctl.h>

#include <sys/types.h>
#include <sys/stat.h>

static unsigned char debug;

#define ENABLE_DBG()	debug = 1
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

	int			new_session;
};

static int hangup;
static struct usb_serial_test _serial;

static void signal_hup(int sig)
{
	if (debug)
		printf("received signal %d\n", sig);
	hangup = 1;
	close(_serial.fd);
}

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
	if (ret < 0)
		goto err;

	ret = tcsetattr(fd, TCSANOW, &term);
	if (ret < 0)
		goto err;

err:
	return ret;
}

/**
 * do_write - writes our buffer to fd
 * @serial:	Serial Test Context
 */
static int do_write(struct usb_serial_test *serial, uint32_t bytes)
{
	unsigned int	done = 0;
	int		ret;

	char		*buf = serial->buf;

	while (done < bytes) {
		ret = write(serial->fd, buf + done, bytes - done);
		if (ret < 0)
			goto err;

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
	unsigned int	size = serial->size;
	unsigned int	done = 0;
	int		ret;

	char		*buf = serial->buf;

	while (done < size) {
		ret = read(serial->fd, buf + done, size - done);
		if (ret < 0)
			goto err;

		size = (buf[0] << 24)
			| (buf[1] << 16)
			| (buf[2] << 8)
			| (buf[3]);
		if (size > serial->size)
			goto err;

		done += ret;
		serial->amount_read += ret;
	}

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
	if (ret <= 0)
		goto err;

	return 0;

err:
	return ret;
}

static int do_open(const char *pathname, int flags)
{
	int		fd;
	struct stat	st;
	pid_t		pid;

	fd = open(pathname, flags);
	if (fd < 0)
		goto err0;

	/* Make sure the file is a character device */
	if (fstat(fd, &st))
		goto err1;

	if (!S_ISCHR(st.st_mode)) {
		errno = EBADF;
		goto err1;
	}

	if (isatty(fd)) {
		if (!_serial.new_session) {
			close(fd);

			pid = fork();
			if (pid > 0) {
				/*
				  Parent foreground process is killed
				  on demand disposing of child background
				  process, so we need it run
				*/
				while (1)
					sleep(1);
			} else if (pid == -1) {
				goto err0;
			}

			prctl(PR_SET_PDEATHSIG, SIGKILL);

			if (setsid() < 0)
				goto err0;

			fd = open(pathname, flags);
			if (fd < 0)
				goto err0;

			_serial.new_session = 1;
		}

		if (tty_init(fd) < 0)
			goto err1;
	}

	return fd;

err1:
	close(fd);
err0:
	return -1;
}

/**
 * do_test - poll, read the data and write it back
 * @serial:	Serial Test Context
 */
static int do_test(struct usb_serial_test *serial)
{
	uint32_t	bytes;
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
	{  } /* Terminating entry */
};

int main(int argc, char *argv[])
{
	struct usb_serial_test	*serial = &_serial;
	unsigned		size = 0;
	int			ret = 0;
	char			*file = NULL;
	struct sigaction	sa;

	sa.sa_handler = signal_hup;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;

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

	serial->fd = do_open(file, O_RDWR);
	if (serial->fd < 0) {
		fprintf(stderr, "%s: failed to open %s: %s\n",
			argv[0], file, strerror(errno));
		ret = 1;
		goto err0;
	}

	serial->size = size;

	serial->buf = malloc(size);
	if (!serial->buf) {
		fprintf(stderr, "%s: failed to allocate buffer\n", argv[0]);
		ret = 1;
		goto err1;
	}

	if (sigaction(SIGHUP, &sa, NULL) == -1)
		printf("failed to handle signal\n");

	while (1) {
		ret = do_test(serial);
		if (hangup) {
			hangup = 0;
			serial->fd = do_open(file, O_RDWR);
			if (serial->fd < 0)
				fprintf(stderr, "%s: reopen failed\n", argv[0]);
			else
				continue;
		}
		if (ret < 0) {
			fprintf(stderr, "%s: test failed: %s\n",
				__func__, strerror(errno));
			break;
		}
	}

	free(serial->buf);
err1:
	close(serial->fd);
err0:
	return ret;
}
