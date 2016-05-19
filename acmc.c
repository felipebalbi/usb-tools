/* $(CROSS_COMPILE)gcc -Wall -O2 -g -lusb-1.0 -o acmc acmc.c */
/**
 * acmc.c - Client for f_acm.c verification
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

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <poll.h>

#include <sys/ioctl.h>

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

static int doit(int fd, int i)
{
	struct pollfd	pfd;
	char		cmd[16];
	char		reply[16];
	ssize_t		val;
	int		ret;

	ret = snprintf(cmd, 3, "AT\r");
	if (ret < 0) {
		perror("snprintf");
		goto error;
	}

	if (write(fd, cmd, 3) != 3) {
		perror("write");
		goto error;
	}

	pfd.fd = fd;
	pfd.events = POLLIN;
	pfd.revents = 0;

	ret = poll(&pfd, 1, -1);
	if (ret < 1) {
		perror("pollin");
		goto error;
	}

	val = read(fd, reply, sizeof(reply));
	if (val < 0) {
		perror("read");
		goto error;
	}

	printf("%d. cmd %s reply %s\n", i, cmd, reply);

	return 0;

error:
	printf("failed\n");

	return -1;
}

int main(void)
{
	int		fd;
	uintmax_t	i;
	unsigned	control = 0;

	fd = open("/dev/ttyACM0", O_RDWR);
	if (fd < 0) {
		perror ("ACM0");
		return -1;
	}

	/* wait for carrier */
	while (1) {
		int	ret;

		ret = ioctl(fd, TIOCMGET, &control);
		if (ret < 0) {
			perror("ioctl");
			close(fd);
			goto out;
		}

		if ((control & TIOCM_CD) &&
				(control & TIOCM_DSR))
			break;

		printf("waiting DCD | DSR\n");
	}

	if (tty_init(fd))
		goto out;

	for (i = 1; i < UINTMAX_MAX; i++)
		doit(fd, i);

out:
	close(fd);

	return 0;
}

