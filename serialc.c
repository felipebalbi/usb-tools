/* $(CROSS_COMPILE)gcc -Wall -O2 -g -lusb-1.0 -o serialc serialc.c */
/**
 * serialc.c - Client for u_serial verification
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

#include <libusb-1.0/libusb.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <malloc.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

static unsigned debug;

/* for measuring throughtput */
static struct timeval		start;
static struct timeval		end;

#define DBG(fmt, args...)				\
	if (debug)					\
		printf(fmt, ## args)

#define ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))
#define TIMEOUT		1000	/* ms */

/**
 * usb_serial_test - USB u_serial Test Context
 * @transferred:	amount of data transferred so far
 * @read_tput:		read throughput
 * @write_tput:		write throughput
 * @eprx:		rx endpoint number
 * @eptx:		tx endpoint number
 * @size:		buffer size
 * @txbuf:		tx buffer
 * @rxbuf:		rx buffer
 */
struct usb_serial_test {
	libusb_device_handle	*udevh;
	uint64_t		transferred;

	float			read_tput;
	float			write_tput;

	unsigned		size;
	unsigned		vid;
	unsigned		pid;

	uint8_t			eprx;
	uint8_t			eptx;

	unsigned char		*txbuf;
	unsigned char		*rxbuf;
};

/* units which will be used for pretty printing the amount of data
 * transferred
 */
static char	*units[] = {
	"",
	"k",
	"M",
	"G",
	"T",
	"P",
	"E",
	"Z",
	"Y",
};

/**
 * init_buffer - initializes our TX buffer with known data
 * @buf:	Buffer to initialize
 */
static void init_buffer(struct usb_serial_test *serial)
{
	int			i;
	unsigned char		*buf = serial->txbuf;

	for (i = 0; i < serial->size; i++)
		buf[i] = i % serial->size;
}

/**
 * alloc_buffer - allocates a @size buffer
 * @size:	Size of buffer
 */
static unsigned char *alloc_buffer(unsigned size)
{
	unsigned char		*tmp;

	if (size == 0) {
		DBG("%s: cannot allocate a zero sized buffer\n", __func__);
		return NULL;
	}

	tmp = malloc(size);
	if (!tmp)
		return NULL;

	return tmp;
}

/**
 * alloc_and_init_buffer - Allocates and initializes both buffers
 * @serial:	Serial Test Context
 */
static int alloc_and_init_buffer(struct usb_serial_test *serial)
{
	int			ret = -ENOMEM;
	unsigned char		*tmp;

	tmp = alloc_buffer(serial->size);
	if (!tmp) {
		DBG("%s: unable to allocate txbuf\n", __func__);
		goto err0;
	}

	serial->txbuf = tmp;
	init_buffer(serial);

	tmp = alloc_buffer(serial->size);
	if (!tmp) {
		DBG("%s: unable to allocate rxbuf\n", __func__);
		goto err1;
	}

	serial->rxbuf = tmp;

	return 0;

err1:
	free(serial->txbuf);

err0:
	return ret;
}

/**
 * find_and_claim_interface - Find the interface we want
 * @serial:		Serial Test Context
 */
static int find_and_claim_interface(struct usb_serial_test *serial)
{
	int			ret;

	/* FIXME for now this will only work with g_nokia, but we will
	 * extend this later in order to work with any u_serial
	 * gadget driver
	 */

	ret = libusb_claim_interface(serial->udevh, 3);
	if (ret < 0) {
		DBG("%s: couldn't claim interface\n", __func__);
		goto err0;
	}

	ret = libusb_set_interface_alt_setting(serial->udevh, 3, 1);
	if (ret < 0) {
		DBG("%s: couldn't set altsetting\n", __func__);
		goto err1;
	}

	serial->eprx = 0x82;
	serial->eptx = 0x02;

	return 0;

err1:
	libusb_release_interface(serial->udevh, 3);

err0:
	return ret;
}

static void release_interface(struct usb_serial_test *serial)
{
	libusb_release_interface(serial->udevh, 3);
}

static float throughput(struct timeval *start, struct timeval *end, size_t size)
{
	int64_t			diff;

	diff = (end->tv_sec - start->tv_sec) * 1000000;
	diff += end->tv_usec - start->tv_usec;

	return (float) size / ((diff / 1000000.0) * 1024);
}

/**
 * do_write - Write txbuf to fd
 * @serial:	Serial Test Context
 * @bytes:	amount of data to write
 */
static int do_write(struct usb_serial_test *serial, uint16_t bytes)
{
	int			transferred = 0;
	int			done = 0;
	int			ret;

	serial->txbuf[0] = bytes >> 8;
	serial->txbuf[1] = (bytes << 8) >> 8;

	while (done < bytes) {
		gettimeofday(&start, NULL);
		ret = libusb_bulk_transfer(serial->udevh, serial->eptx,
				serial->txbuf + done, bytes - done,
				&transferred, TIMEOUT);
		if (ret < 0) {
			DBG("%s: failed to send data\n", __func__);
			goto err;
		}
		gettimeofday(&end, NULL);

		serial->write_tput = throughput(&start, &end, transferred);
		serial->transferred += transferred;
		done += transferred;
	}

	return 0;

err:
	return ret;
}

/**
 * do_read - Read from fd to rxbuf
 * @serial:	Serial Test Context
 * @bytes:	amount of data to read
 */
static int do_read(struct usb_serial_test *serial, uint16_t bytes)
{
	int			transferred = 0;
	int			done = 0;
	int			ret;

	while (done < bytes) {
		gettimeofday(&start, NULL);
		ret = libusb_bulk_transfer(serial->udevh, serial->eprx,
				serial->rxbuf + done, bytes - done,
				&transferred, TIMEOUT);
		if (ret < 0) {
			DBG("%s: failed to receive data\n", __func__);
			goto err;
		}
		gettimeofday(&end, NULL);

		serial->read_tput = throughput(&start, &end, transferred);
		done += transferred;
	}

	return 0;

err:
	return ret;
}

/**
 * do_verify - Verify consistency of data
 * @serial:	Serial Test Context
 * @bytes:	amount of data to verify
 */
static int do_verify(struct usb_serial_test *serial, uint16_t bytes)
{
	int			i;

	for (i = 0; i < bytes; i++)
		if (serial->txbuf[i] != serial->rxbuf[i]) {
			printf("%s: byte %d failed [%02x %02x]\n", __func__,
					i, serial->txbuf[i], serial->rxbuf[i]);
			return -EINVAL;
		}

	return 0;
}

/**
 * do_test - Write, Read and Verify
 * @serial:	Serial Test Context
 * @bytes:	amount of data to transfer
 */
static int do_test(struct usb_serial_test *serial, uint16_t bytes)
{
	int			ret;
	unsigned		n;

	ret = do_write(serial, bytes);
	if (ret < 0)
		goto err;

	ret = do_read(serial, bytes);
	if (ret < 0)
		goto err;

	ret = do_verify(serial, bytes);
	if (ret < 0)
		goto err;

	return 0;

err:
	return ret;
}

static void usage(char *prog)
{
	printf("Usage: %s\n\
			--vid, -v	USB Vendor ID\n\
			--pid, -p	USB Product ID\n\
			--size, -s	Internal buffer size\n\
			--debug, -d	Enables debugging messages\n\
			--help, -h	This help\n", prog);
}

static struct option serial_opts[] = {
	{
		.name		= "vid",
		.has_arg	= 1,
		.val		= 'v',
	},
	{
		.name		= "pid",
		.has_arg	= 1,
		.val		= 'p',
	},
	{
		.name		= "size",	/* rx/tx buffer sizes */
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
	libusb_context		*context;
	struct usb_serial_test	*serial;
	unsigned		size = 0;
	int			ret = 0;

	unsigned		vid = 0xffff;
	unsigned		pid = 0xffff;

	char			*vendor = NULL;
	char			*product = NULL;

	while (ARRAY_SIZE(serial_opts)) {
		int		optidx = 0;
		int		opt;

		opt = getopt_long(argc, argv, "v:p:s:dh", serial_opts, &optidx);
		if (opt < 0)
			break;

		switch (opt) {
		case 'v':
			vid = strtoul(optarg, &vendor, 16);
			break;
		case 'p':
			pid = strtoul(optarg, &product, 16);
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

	serial = malloc(sizeof(*serial));
	if (!serial) {
		DBG("%s: unable to allocate memory\n", __func__);
		ret = -ENOMEM;
		goto err0;
	}

	memset(serial, 0x00, sizeof(*serial));

	serial->size = size;
	serial->vid = vid;
	serial->pid = pid;

	ret = alloc_and_init_buffer(serial);
	if (ret < 0) {
		DBG("%s: failed to allocate buffers\n", __func__);
		goto err1;
	}

	/* initialize libusb */
	libusb_init(&context);

	serial->udevh = libusb_open_device_with_vid_pid(context, vid, pid);
	if (!serial->udevh) {
		DBG("%s: couldn't find device V%02x P%02x\n", __func__,
				vid, pid);
		goto err2;
	}

	/* get descriptors, find correct interface to claim and
	 * set correct alternate setting
	 */
	ret = find_and_claim_interface(serial);
	if (ret < 0) {
		DBG("%s: unable to claim interface\n", __func__);
		goto err2;
	}

	srand(1024);

	while (1) {
		float		transferred = 0;
		int		i;
		unsigned	n;
		char		*unit = NULL;

		n = random() % (serial->size + 1);

		DBG("%s sending %d bytes\n", __func__, n);

		ret = do_test(serial, n);
		if (ret < 0) {
			DBG("%s: test failed\n", __func__);
			goto err3;
		}

		transferred = (float) serial->transferred;

		for (i = 0; i < ARRAY_SIZE(units); i++) {
			if (transferred > 1024.0) {
				transferred /= 1024.0;
				continue;
			}
			unit = units[i];
			break;
		}

		printf("[ V%04x P%04x written %10.04f %sByte%s read %10.02f kB/s write %10.02f kB/s ]\r",
				vid, pid, transferred, unit, transferred > 1 ? "s" : "",
				serial->read_tput, serial->write_tput);

		fflush(stdout);
	}

	release_interface(serial);
	libusb_exit(context);
	free(serial->rxbuf);
	free(serial->txbuf);
	free(serial);

	return 0;

err3:
	release_interface(serial);

err2:
	libusb_exit(context);
	free(serial->rxbuf);

err1:
	free(serial);

err0:
	return ret;
}

