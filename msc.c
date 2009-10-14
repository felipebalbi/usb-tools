/* $(CROSS_COMPILE)gcc -Wall -O2 -g -o msc msc.c */
/**
 * msc.c - USB Mass Storage Class Verification Tool
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
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <malloc.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mount.h>

static unsigned debug;

/* for measuring throughput */
static struct timeval		start;
static struct timeval		end;

#define DBG(fmt, args...)				\
	if (debug)					\
		printf(fmt, ## args)

#define ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))

struct usb_msc_test {
	uint64_t	transferred;	/* amount of data transferred so far */
	uint64_t	psize;		/* partition size */
	uint64_t	pempty;		/* what needs to be filled up still */

	float		read_tput;	/* read throughput */
	float		write_tput;	/* write throughput */

	int		fd;		/* /dev/sd?? */

	unsigned	size;		/* buffer size */

	off_t		offset;		/* current offset */

	char		*txbuf;		/* send buffer */
	char		*rxbuf;		/* receive buffer*/
};

/**
 * How it works:
 *
 * 1. Fill up a partition with a known buffer made by a sequencial
 *    numbering scheme.
 *
 * 2. Read back every 512-bytes from each block and compare with our magic
 *    buffer to see if the data was sucessfully written.
 *
 * 3. During the operation also measure throughput.
 *
 * 4. Steps 1-3 will be repeated until we send a signal to the application
 *    to stop. At that time we will free() every allocated resource and close
 *    the fd.
 */


/**
 * init_buffer - initializes our TX buffer with known data
 * @buf:	Buffer to initialize
 */
static void init_buffer(struct usb_msc_test *msc)
{
	int			i;
	char			*buf = msc->txbuf;

	for (i = 0; i < msc->size; i++)
		buf[i] = i % msc->size;
}

/**
 * alloc_buffer - allocates a @size buffer
 * @size:	Size of buffer
 */
static char *alloc_buffer(unsigned size)
{
	char			*tmp;

	if (size == 0) {
		DBG("%s: cannot allocate a zero sized buffer\n", __func__);
		return NULL;
	}

	tmp = memalign(getpagesize(), size);
	if (!tmp)
		return NULL;

	return tmp;
}

/**
 * alloc_and_init_buffer - Allocates and initializes both buffers
 * @msc:	Mass Storage Test Context
 */
static int alloc_and_init_buffer(struct usb_msc_test *msc)
{
	int			ret = -ENOMEM;
	char			*tmp;

	tmp = alloc_buffer(msc->size);
	if (!tmp) {
		DBG("%s: unable to allocate txbuf\n", __func__);
		goto err0;
	}

	msc->txbuf = tmp;
	init_buffer(msc);

	tmp = alloc_buffer(msc->size);
	if (!tmp) {
		DBG("%s: unable to allocate rxbuf\n", __func__);
		goto err1;
	}

	msc->rxbuf = tmp;

	return 0;

err1:
	free(msc->txbuf);

err0:
	return ret;
}

static float throughput(struct timeval *start, struct timeval *end, size_t size)
{
	int64_t diff;

	diff = (end->tv_sec - start->tv_sec) * 1000000;
	diff += end->tv_usec - start->tv_usec;

	return (float) size / ((diff / 1000000.0) * 1024);
}

/**
 * do_write - Write txbuf to fd
 * @msc:	Mass Storage Test Context
 */
static int do_write(struct usb_msc_test *msc)
{
	int			done = 0;
	int			ret;

	char			*buf = msc->txbuf;

	while (done < msc->size) {
		unsigned	size = msc->size - done;

		if (size > msc->pempty) {
			DBG("%s: size too big\n", __func__);
			size = msc->pempty;
		}

		gettimeofday(&start, NULL);
		ret = write(msc->fd, buf + done, size);
		if (ret < 0) {
			perror("do_write");
			goto err;
		}
		gettimeofday(&end, NULL);

		msc->write_tput = throughput(&start, &end, ret);

		done += ret;
		msc->pempty -= ret;

		if (msc->pempty == 0) {
			DBG("%s: restarting\n", __func__);
			msc->pempty = msc->psize;
			done = 0;
			ret = lseek(msc->fd, 0, SEEK_SET);
			if (ret < 0) {
				DBG("%s: couldn't seek the start\n", __func__);
				goto err;
			}

		}
	}

	ret = lseek(msc->fd, 0, SEEK_CUR);
	if (ret < 0) {
		DBG("%s: couldn't seek current offset\n", __func__);
		goto err;
	}

	msc->offset = ret;

	return 0;

err:
	return ret;
}

/**
 * do_read - Read from fd to rxbuf
 * @msc:	Mass Storage Test Context
 */
static int do_read(struct usb_msc_test *msc)
{
	unsigned		size = msc->size;
	int			done = 0;
	int			ret;

	off_t			previous = msc->offset - msc->size;

	char			*buf = msc->rxbuf;

	ret = lseek(msc->fd, previous, SEEK_SET);
	if (ret < 0) {
		DBG("%s: could not seek previous block\n", __func__);
		goto err;
	}

	while (done < size) {
		gettimeofday(&start, NULL);
		ret = read(msc->fd, buf + done, size - done);
		if (ret < 0) {
			perror("do_read");
			goto err;
		}
		gettimeofday(&end, NULL);

		done += ret;
		msc->transferred += ret;
		msc->read_tput = throughput(&start, &end, ret);
	}

	return 0;

err:
	return ret;
}

/**
 * do_verify - Verify consistency of data
 * @msc:	Mass Storage Test Context
 */
static int do_verify(struct usb_msc_test *msc)
{
	unsigned		size = msc->size;
	int			i;

	for (i = 0; i < size; i++)
		if (msc->txbuf[i] != msc->rxbuf[i]) {
			printf("%s: byte %d failed [%02x %02x]\n", __func__,
					i, msc->txbuf[i], msc->rxbuf[i]);
			return -EINVAL;
		}

	return 0;
}

/**
 * do_test - Write, Read and Verify
 * @msc:	Mass Storage Test Context
 */
static int do_test(struct usb_msc_test *msc)
{
	int			ret;

	ret = do_write(msc);
	if (ret < 0)
		goto err;

	ret = do_read(msc);
	if (ret < 0)
		goto err;

	ret = do_verify(msc);
	if (ret < 0)
		goto err;

	return 0;

err:
	return ret;
}

static void usage(char *prog)
{
	printf("Usage: %s\n\
			--output, -o	Block device to write to\n\
			--size, -s	Size of the internal buffers\n\
			--debug, -d	Enables debugging messages\n\
			--help, -h	This help\n", prog);
}

static struct option msc_opts[] = {
	{
		.name		= "output",
		.has_arg	= 1,
		.val		= 'o',
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
	struct usb_msc_test	*msc;

	uint64_t		blksize;
	unsigned		size = 0;
	int			ret = 0;

	char			*output = NULL;

	while (ARRAY_SIZE(msc_opts)) {
		int		opt_index = 0;
		int		opt;

		opt = getopt_long(argc, argv, "o:s:dh", msc_opts, &opt_index);
		if (opt < 0)
			break;

		switch (opt) {
		case 'o':
			output = optarg;
			break;

		case 's':
			size = atoi(optarg);
			if (size == 0) {
				DBG("%s: can't do it with zero length buffer\n",
						__func__);
				ret = -EINVAL;
				goto err0;
			}

			if (size % getpagesize()) {
				DBG("%s: unaligned size\n", __func__);
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

	if (!output) {
		DBG("%s: need a file to open\n", __func__);
		ret = -EINVAL;
		goto err0;
	}

	msc = malloc(sizeof(*msc));
	if (!msc) {
		DBG("%s: unable to allocate memory\n", __func__);
		ret = -ENOMEM;
		goto err0;
	}

	memset(msc, 0x00, sizeof(*msc));

	DBG("%s: buffer size %d\n", __func__, size);

	msc->size = size;

	ret = alloc_and_init_buffer(msc);
	if (ret < 0) {
		DBG("%s: failed to alloc and initialize buffers\n", __func__);
		goto err1;
	}

	DBG("%s: opening %s\n", __func__, output);

	msc->fd = open(output, O_RDWR | O_DIRECT);
	if (msc->fd < 0) {
		DBG("%s: could not open %s\n", __func__, output);
		goto err2;
	}

	ret = ioctl(msc->fd, BLKGETSIZE64, &blksize);
	if (ret < 0 || blksize == 0) {
		DBG("%s: could not get block device size\n", __func__);
		goto err3;
	}

	msc->psize = blksize;
	msc->pempty = blksize;

	DBG("%s: file descriptor %d size %.2f MB\n", __func__, msc->fd,
			(float) msc->psize / 1024 / 1024);

	/* sync before starting any test in order to getter more
	 * reliable results out of the tests
	 */
	sync();

	while (1) {
		ret = do_test(msc);
		if (ret < 0) {
			DBG("%s: test failed\n", __func__);
			goto err3;
		}

		printf("[ using %s written %10.02f MB read %10.02f kB/s write %10.02f kB/s ]\r",
				output, (float) msc->transferred / 1024 / 1024,
				msc->read_tput, msc->write_tput);

		fflush(stdout);
	}

	close(msc->fd);
	free(msc->rxbuf);
	free(msc);

	return 0;

err3:
	close(msc->fd);

err2:
	free(msc->rxbuf);

err1:
	free(msc);

err0:
	return ret;
}

