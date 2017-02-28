/**
 * acmd.c - Server for f_acm.c verification
 *
 * Copyright (C) 2010-2016 Felipe Balbi <felipe.balbi@linux.intel.com>
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
