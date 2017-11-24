/*
 * SPDX-License-Identifier: GPL-3.0
 * Copyright (C) 2010-2016 Felipe Balbi <felipe.balbi@linux.intel.com>
 */

#include <config.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <poll.h>

static int tty_init(int fd)
{
	int		ret = 0;
	struct termios	term;

	tcgetattr(fd, &term);

	cfmakeraw(&term);

	ret = tcflush(fd, TCIOFLUSH);
	if (ret < 0) {
		perror("tcflush");
		goto out;
	}

	ret = tcsetattr(fd, TCSANOW, &term);
	if (ret < 0) {
		perror("tcsetattr");
		goto out;
	}

out:
	return ret;
}

int main(int argc, char *argv[])
{
	struct pollfd	pfd;
	int		fd;
	int		ret = 0;
	char		buf[16];
	ssize_t		val;

	if (argc < 2) {
		fprintf(stderr, "need filename\n");
		return -1;
	}

retry:
	fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		perror(argv[1]);
		ret = fd;
		goto out1;
	}

	ret = tty_init(fd);
	if (ret < 0)
		goto out2;

	pfd.fd = fd;
	pfd.events = POLLIN | POLLHUP;
	pfd.revents = 0;

	while (1) {
		ret = poll(&pfd, 1, -1);
		if (ret < 0)
			goto out2;

		if (pfd.revents & POLLHUP) {
			close(fd);
			goto retry;
		}

		val = read(fd, buf, sizeof(buf));
		if (val < 0) {
			ret = val;
			goto out2;
		}

		printf("%s\n", buf);

		ret = write(fd, buf, val);
		if (ret < 0)
			goto out2;
	}

out2:
	close(fd);

out1:
	return ret;
}
