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
#include <sys/uio.h>

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
	int		count;		/* iteration count */

	unsigned	sect_size;	/* sector size */
	unsigned	size;		/* buffer size */

	off_t		offset;		/* current offset */

	char		*txbuf;		/* send buffer */
	char		*rxbuf;		/* receive buffer*/
	char		*output;	/* writing to... */
};

enum usb_msc_test_case {
	MSC_TEST_SIMPLE = 0,		/* simple */
	MSC_TEST_1SECT,			/* 1 sector at a time */
	MSC_TEST_8SECT,			/* 8 sectors at a time */
	MSC_TEST_32SECT,		/* 32 sectors at a time */
	MSC_TEST_64SECT,		/* 64 sectors at a time */
	MSC_TEST_SG_2SECT,		/* SG 2 sectors at a time */
	MSC_TEST_SG_8SECT,		/* SG 8 sectors at a time */
	MSC_TEST_SG_32SECT,		/* SG 32 sectors at a time */
	MSC_TEST_SG_64SECT,		/* SG 64 sectors at a time */
	MSC_TEST_SG_128SECT,            /* SG 128 sectors at a time */
	MSC_TEST_READ_PAST_LAST,	/* read extends over the last sector */
	MSC_TEST_LSEEK_PAST_LAST,	/* lseek over the end of the block device */
	MSC_TEST_WRITE_PAST_LAST,	/* write start over the last sector */
	MSC_TEST_SG_RANDOM_READ,	/* write, read random SG 2 - 8 sectors */
	MSC_TEST_SG_RANDOM_WRITE,	/* write random SG 2 - 8 sectors, read */
	MSC_TEST_SG_RANDOM_BOTH,	/* write and read random SG 2 - 8 sectors */
	MSC_TEST_READ_DIFF_BUF,		/* read using differently allocated buffers */
	MSC_TEST_WRITE_DIFF_BUF,	/* write using differently allocated buffers */
};

/* ------------------------------------------------------------------------- */

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
 * report_progess - reports the progress of @test
 * @msc:	Mass Storage Test Context
 * @test:	test case number
 *
 * each test case implementation is required to call this function
 * in order for the user to get progress report.
 */
static void report_progress(struct usb_msc_test *msc, enum usb_msc_test_case test)
{
	float		transferred = 0;
	int		i;
	char		*unit = NULL;

	transferred = (float) msc->transferred;

	for (i = 0; i < ARRAY_SIZE(units); i++) {
		if (transferred > 1024.0) {
			transferred /= 1024.0;
			continue;
		}
		unit = units[i];
		break;
	}

	printf("\rtest %d: sent %10.04f %sByte%s read %10.02f kB/s write %10.02f kB/s ... ",
			test, transferred, unit, transferred > 1 ? "s" : " ",
			msc->read_tput, msc->write_tput);

	fflush(stdout);
}

/* ------------------------------------------------------------------------- */

/**
 * do_write - Write txbuf to fd
 * @msc:	Mass Storage Test Context
 * @bytes:	Amount of bytes to write
 */
static int do_write(struct usb_msc_test *msc, unsigned bytes)
{
	int			done = 0;
	int			ret;

	char			*buf = msc->txbuf;

	while (done < bytes) {
		unsigned	size = bytes - done;

		if (size > msc->pempty) {
			DBG("%s: size too big\n", __func__);
			size = msc->pempty;
		}

		gettimeofday(&start, NULL);
		ret = write(msc->fd, buf + done, size);
		if (ret < 0) {
			DBG("%s: write failed\n", __func__);
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
 * @bytes:	Amount of data to read
 */
static int do_read(struct usb_msc_test *msc, unsigned bytes)
{
	int			done = 0;
	int			ret;

	off_t			previous = msc->offset - bytes;

	char			*buf = msc->rxbuf;

	ret = lseek(msc->fd, previous, SEEK_SET);
	if (ret < 0) {
		DBG("%s: could not seek previous block\n", __func__);
		goto err;
	}

	while (done < bytes) {
		gettimeofday(&start, NULL);
		ret = read(msc->fd, buf + done, bytes - done);
		if (ret < 0) {
			DBG("%s: read failed\n", __func__);
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
 * @bytes:	Amount of data to verify
 */
static int do_verify(struct usb_msc_test *msc, unsigned bytes)
{
	int			i;

	for (i = 0; i < bytes; i++)
		if (msc->txbuf[i] != msc->rxbuf[i]) {
			DBG("%s: byte %d failed [%02x %02x]\n", __func__,
					i, msc->txbuf[i], msc->rxbuf[i]);
			return -EINVAL;
		}

	return 0;
}

/**
 * do_writev - SG Write txbuf to fd
 * @msc:	Mass Storage Test Context
 * @count:	how many transfers
 */
static int do_writev(struct usb_msc_test *msc, const struct iovec *iov,
		unsigned count)
{
	int			ret;

	gettimeofday(&start, NULL);
	ret = writev(msc->fd, iov, count);
	if (ret < 0) {
		DBG("%s: writev failed\n", __func__);
		goto err;
	}
	gettimeofday(&end, NULL);

	msc->write_tput = throughput(&start, &end, ret);

	msc->pempty -= ret;

	if (msc->pempty == 0) {
		DBG("%s: restarting\n", __func__);
		msc->pempty = msc->psize;
		ret = lseek(msc->fd, 0, SEEK_SET);
		if (ret < 0) {
			DBG("%s: couldn't seek the start\n", __func__);
			goto err;
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
 * do_read - SG Read from fd to rxbuf
 * @msc:	Mass Storage Test Context
 * @count:	how many transfers
 */
static int do_readv(struct usb_msc_test *msc, const struct iovec *iov,
		unsigned bytes)
{
	int			done = 0;
	int			ret;

	gettimeofday(&start, NULL);
	ret = readv(msc->fd, iov, bytes);
	if (ret < 0) {
		DBG("%s: readv failed\n", __func__);
		goto err;
	}
	gettimeofday(&end, NULL);

	done += ret;
	msc->transferred += ret;
	msc->read_tput = throughput(&start, &end, ret);

	return 0;

err:
	return ret;
}

/* ------------------------------------------------------------------------- */

/**
 * do_test_sg_random_both - write and read several of random size
 * @msc:	Mass Storage Test Context
 */
static int do_test_sg_random_both(struct usb_msc_test *msc)
{
	char			*txbuf = msc->txbuf;
	char			*rxbuf = msc->rxbuf;

	unsigned		sect_size = msc->sect_size;
	unsigned		len = msc->size;

	int			ret = 0;
	int			i;

	const struct iovec	tiov[] = {
		{
			.iov_base	= txbuf,
			.iov_len	= 8 * sect_size,
		},
		{
			.iov_base	= txbuf + 8 * sect_size,
			.iov_len	= 1 * sect_size,
		},
		{
			.iov_base	= txbuf + 9 * sect_size,
			.iov_len	= 3 * sect_size,
		},
		{
			.iov_base	= txbuf + 12 * sect_size,
			.iov_len	= 32 * sect_size,
		},
		{
			.iov_base	= txbuf + 44 * sect_size,
			.iov_len	= 20 * sect_size,
		},
		{
			.iov_base	= txbuf + 64 * sect_size,
			.iov_len	= 14 * sect_size,
		},
		{
			.iov_base	= txbuf + 78 * sect_size,
			.iov_len	= 16 * sect_size,
		},
		{
			.iov_base	= txbuf + 94 * sect_size,
			.iov_len	= 34 * sect_size,
		},
	};

	const struct iovec	riov[] = {
		{
			.iov_base	= rxbuf,
			.iov_len	= 8 * sect_size,
		},
		{
			.iov_base	= rxbuf + 8 * sect_size,
			.iov_len	= 1 * sect_size,
		},
		{
			.iov_base	= rxbuf + 9 * sect_size,
			.iov_len	= 3 * sect_size,
		},
		{
			.iov_base	= rxbuf + 12 * sect_size,
			.iov_len	= 32 * sect_size,
		},
		{
			.iov_base	= rxbuf + 44 * sect_size,
			.iov_len	= 20 * sect_size,
		},
		{
			.iov_base	= rxbuf + 64 * sect_size,
			.iov_len	= 14 * sect_size,
		},
		{
			.iov_base	= rxbuf + 78 * sect_size,
			.iov_len	= 16 * sect_size,
		},
		{
			.iov_base	= rxbuf + 94 * sect_size,
			.iov_len	= 34 * sect_size,
		},
	};

	for (i = 0; i < msc->count; i++) {
		ret = lseek(msc->fd, 0, SEEK_CUR);
		if (ret < 0) {
			DBG("%s: lseek failed\n", __func__);
			goto err;
		}

		msc->offset = ret;

		ret = do_writev(msc, tiov, 8);
		if (ret < 0)
			goto err;

		ret = lseek(msc->fd, msc->offset - len, SEEK_SET);
		if (ret < 0) {
			DBG("%s: lseek failed\n", __func__);
			goto err;
		}

		ret = do_readv(msc, riov, 8);
		if (ret < 0)
			goto err;

		ret = do_verify(msc, len);
		if (ret < 0)
			goto err;

		report_progress(msc, MSC_TEST_SG_RANDOM_BOTH);
	}

	printf("success\n");

	return 0;

err:
	printf("failed\n");

	return ret;
}

/**
 * do_test_sg_random_write - write several of random size, read 1 64k
 * @msc:	Mass Storage Test Context
 */
static int do_test_sg_random_write(struct usb_msc_test *msc)
{
	char			*txbuf = msc->txbuf;
	char			*rxbuf = msc->rxbuf;

	unsigned		sect_size = msc->sect_size;
	unsigned		len = msc->size;

	int			ret = 0;
	int			i;

	const struct iovec	tiov[] = {
		{
			.iov_base	= txbuf,
			.iov_len	= 8 * sect_size,
		},
		{
			.iov_base	= txbuf + 8 * sect_size,
			.iov_len	= 1 * sect_size,
		},
		{
			.iov_base	= txbuf + 9 * sect_size,
			.iov_len	= 3 * sect_size,
		},
		{
			.iov_base	= txbuf + 12 * sect_size,
			.iov_len	= 32 * sect_size,
		},
		{
			.iov_base	= txbuf + 44 * sect_size,
			.iov_len	= 20 * sect_size,
		},
		{
			.iov_base	= txbuf + 64 * sect_size,
			.iov_len	= 14 * sect_size,
		},
		{
			.iov_base	= txbuf + 78 * sect_size,
			.iov_len	= 16 * sect_size,
		},
		{
			.iov_base	= txbuf + 94 * sect_size,
			.iov_len	= 34 * sect_size,
		},
	};

	const struct iovec	riov[] = {
		{
			.iov_base	= rxbuf,
			.iov_len	= len,
		},
	};

	for (i = 0; i < msc->count; i++) {
		ret = lseek(msc->fd, 0, SEEK_CUR);
		if (ret < 0) {
			DBG("%s: lseek failed\n", __func__);
			goto err;
		}

		msc->offset = ret;

		ret = do_writev(msc, tiov, 8);
		if (ret < 0)
			goto err;

		ret = lseek(msc->fd, msc->offset - len, SEEK_SET);
		if (ret < 0) {
			DBG("%s: lseek failed\n", __func__);
			goto err;
		}

		ret = do_readv(msc, riov, 1);
		if (ret < 0)
			goto err;

		ret = do_verify(msc, len);
		if (ret < 0)
			goto err;

		report_progress(msc, MSC_TEST_SG_RANDOM_WRITE);
	}

	printf("success\n");

	return 0;

err:
	printf("failed\n");

	return ret;
}

/**
 * do_test_sg_random_read - write 1 64k sg and read in several of random size
 * @msc:	Mass Storage Test Context
 */
static int do_test_sg_random_read(struct usb_msc_test *msc)
{
	char			*txbuf = msc->txbuf;
	char			*rxbuf = msc->rxbuf;

	unsigned		sect_size = msc->sect_size;
	unsigned		len = msc->size;

	int			ret = 0;
	int			i;

	const struct iovec	tiov[] = {
		{
			.iov_base	= txbuf,
			.iov_len	= len,
		},
	};

	const struct iovec	riov[] = {
		{
			.iov_base	= rxbuf,
			.iov_len	= 8 * sect_size,
		},
		{
			.iov_base	= rxbuf + 8 * sect_size,
			.iov_len	= 1 * sect_size,
		},
		{
			.iov_base	= rxbuf + 9 * sect_size,
			.iov_len	= 3 * sect_size,
		},
		{
			.iov_base	= rxbuf + 12 * sect_size,
			.iov_len	= 32 * sect_size,
		},
		{
			.iov_base	= rxbuf + 44 * sect_size,
			.iov_len	= 20 * sect_size,
		},
		{
			.iov_base	= rxbuf + 64 * sect_size,
			.iov_len	= 14 * sect_size,
		},
		{
			.iov_base	= rxbuf + 78 * sect_size,
			.iov_len	= 16 * sect_size,
		},
		{
			.iov_base	= rxbuf + 94 * sect_size,
			.iov_len	= 34 * sect_size,
		},
	};

	for (i = 0; i < msc->count; i++) {
		ret = lseek(msc->fd, 0, SEEK_CUR);
		if (ret < 0) {
			DBG("%s: lseek failed\n", __func__);
			goto err;
		}

		msc->offset = ret;

		ret = do_writev(msc, tiov, 1);
		if (ret < 0)
			goto err;

		ret = lseek(msc->fd, msc->offset - len, SEEK_SET);
		if (ret < 0) {
			DBG("%s: lseek failed\n", __func__);
			goto err;
		}

		ret = do_readv(msc, riov, 8);
		if (ret < 0)
			goto err;

		ret = do_verify(msc, len);
		if (ret < 0)
			goto err;

		report_progress(msc, MSC_TEST_SG_RANDOM_READ);
	}

	printf("success\n");

	return 0;

err:
	printf("failed\n");

	return ret;
}

/**
 * do_test_write_past_last - attempt to write past last sector
 * @msc:	Mass Storage Test Context
 */
static int do_test_write_past_last(struct usb_msc_test *msc)
{
	int			ret = 0;
	int			i;

	for (i = 0; i < msc->count; i++) {
		/* seek to one sector less then needed */
		ret = lseek(msc->fd, msc->psize - msc->size + msc->sect_size,
				SEEK_SET);
		if (ret < 0) {
			DBG("%s: lseek failed\n", __func__);
			ret = -EINVAL;
			goto err;
		}

		ret = do_write(msc, msc->size);
		if (ret >=  0) {
			ret = -EINVAL;
			goto err;
		}

		report_progress(msc, MSC_TEST_WRITE_PAST_LAST);
	}

	printf("success\n");

	return 0;

err:
	printf("failed\n");

	return ret;
}

/**
 * do_test_lseek_past_last - attempt to read starting past the last sector
 * @msc:	Mass Storage Test Context
 */
static int do_test_lseek_past_last(struct usb_msc_test *msc)
{
	int			ret = 0;
	int			i;

	for (i = 0; i < msc->count; i++) {
		ret = lseek(msc->fd, msc->psize + msc->sect_size,
				SEEK_SET);
		if (ret == 0) {
			DBG("%s: lseek passed\n", __func__);
			ret = -EINVAL;
			goto err;
		}

		report_progress(msc, MSC_TEST_LSEEK_PAST_LAST);
	}

	printf("success\n");

	return 0;

err:
	printf("failed\n");

	return ret;
}

/**
 * do_test_read_past_last - attempt to read past last sector
 * @msc:	Mass Storage Test Context
 */
static int do_test_read_past_last(struct usb_msc_test *msc)
{
	int			ret = 0;
	int			i;

	for (i = 0; i < msc->count; i++) {
		/* seek to one sector less then needed */
		ret = lseek(msc->fd, msc->psize - msc->size + msc->sect_size,
				SEEK_SET);
		if (ret < 0) {
			DBG("%s: lseek failed\n", __func__);
			ret = -EINVAL;
			goto err;
		}

		ret = do_read(msc, msc->size);
		if (ret >=  0) {
			ret = -EINVAL;
			goto err;
		}

		report_progress(msc, MSC_TEST_READ_PAST_LAST);
	}

	printf("success\n");

	return 0;

err:
	printf("failed\n");

	return ret;
}

/**
 * do_test_sg_128sect - SG write/read/verify 8 sectors at a time
 * @msc:	Mass Storage Test Context
 */
static int do_test_sg_128sect(struct usb_msc_test *msc)
{
	char			*txbuf = msc->txbuf;
	char			*rxbuf = msc->rxbuf;

	unsigned		len = 128 * msc->sect_size;

	int			ret = 0;
	int			i;

	const struct iovec	tiov[] = {
		{
			.iov_base	= txbuf,
			.iov_len	= len,
		},
	};

	const struct iovec	riov[] = {
		{
			.iov_base	= rxbuf,
			.iov_len	= len,
		},
	};

	for (i = 0; i < msc->count; i++) {
		ret = lseek(msc->fd, 0, SEEK_CUR);
		if (ret < 0) {
			DBG("%s: lseek failed\n", __func__);
			break;
		}

		msc->offset = ret;

		ret = do_writev(msc, tiov, 1);
		if (ret < 0)
			break;

		ret = lseek(msc->fd, msc->offset - len, SEEK_SET);
		if (ret < 0) {
			DBG("%s: lseek failed\n", __func__);
			break;
		}

		ret = do_readv(msc, riov, 1);
		if (ret < 0)
			break;

		ret = do_verify(msc, len);
		if (ret < 0)
			break;

		report_progress(msc, MSC_TEST_SG_128SECT);
		i++;
	}

	if (ret == 0)
		printf("success\n");
	else
		printf("failed\n");

	return ret;
}

/**
 * do_test_sg_64sect - SG write/read/verify 8 sectors at a time
 * @msc:	Mass Storage Test Context
 */
static int do_test_sg_64sect(struct usb_msc_test *msc)
{
	char			*txbuf = msc->txbuf;
	char			*rxbuf = msc->rxbuf;

	unsigned		len = 64 * msc->sect_size;

	int			ret = 0;
	int			i;

	const struct iovec	tiov[] = {
		{
			.iov_base	= txbuf,
			.iov_len	= len,
		},
	};

	const struct iovec	riov[] = {
		{
			.iov_base	= rxbuf,
			.iov_len	= len,
		},
	};

	for (i = 0; i < msc->count; i++) {
		ret = lseek(msc->fd, 0, SEEK_CUR);
		if (ret < 0) {
			DBG("%s: lseek failed\n", __func__);
			break;
		}

		msc->offset = ret;

		ret = do_writev(msc, tiov, 1);
		if (ret < 0)
			break;

		ret = lseek(msc->fd, msc->offset - len, SEEK_SET);
		if (ret < 0) {
			DBG("%s: lseek failed\n", __func__);
			break;
		}

		ret = do_readv(msc, riov, 1);
		if (ret < 0)
			break;

		ret = do_verify(msc, len);
		if (ret < 0)
			break;

		report_progress(msc, MSC_TEST_SG_64SECT);
		i++;
	}

	if (ret == 0)
		printf("success\n");
	else
		printf("failed\n");

	return ret;
}

/**
 * do_test_sg_32sect - SG write/read/verify 8 sectors at a time
 * @msc:	Mass Storage Test Context
 */
static int do_test_sg_32sect(struct usb_msc_test *msc)
{
	char			*txbuf = msc->txbuf;
	char			*rxbuf = msc->rxbuf;

	unsigned		len = 32 * msc->sect_size;

	int			ret = 0;
	int			i;

	const struct iovec	tiov[] = {
		{
			.iov_base	= txbuf,
			.iov_len	= len,
		},
	};

	const struct iovec	riov[] = {
		{
			.iov_base	= rxbuf,
			.iov_len	= len,
		},
	};

	for (i = 0; i < msc->count; i++) {
		ret = lseek(msc->fd, 0, SEEK_CUR);
		if (ret < 0) {
			DBG("%s: lseek failed\n", __func__);
			break;
		}

		msc->offset = ret;

		ret = do_writev(msc, tiov, 1);
		if (ret < 0)
			break;

		ret = lseek(msc->fd, msc->offset - len, SEEK_SET);
		if (ret < 0) {
			DBG("%s: lseek failed\n", __func__);
			break;
		}

		ret = do_readv(msc, riov, 1);
		if (ret < 0)
			break;

		ret = do_verify(msc, len);
		if (ret < 0)
			break;

		report_progress(msc, MSC_TEST_SG_32SECT);
		i++;
	}

	if (ret == 0)
		printf("success\n");
	else
		printf("failed\n");

	return ret;
}

/**
 * do_test_sg_8sect - SG write/read/verify 8 sectors at a time
 * @msc:	Mass Storage Test Context
 */
static int do_test_sg_8sect(struct usb_msc_test *msc)
{
	char			*txbuf = msc->txbuf;
	char			*rxbuf = msc->rxbuf;

	unsigned		len = 8 * msc->sect_size;

	int			ret = 0;
	int			i;

	const struct iovec	tiov[] = {
		{
			.iov_base	= txbuf,
			.iov_len	= len,
		},
	};

	const struct iovec	riov[] = {
		{
			.iov_base	= rxbuf,
			.iov_len	= len,
		},
	};

	for (i = 0; i < msc->count; i++) {
		ret = lseek(msc->fd, 0, SEEK_CUR);
		if (ret < 0) {
			DBG("%s: lseek failed\n", __func__);
			break;
		}

		msc->offset = ret;

		ret = do_writev(msc, tiov, 1);
		if (ret < 0)
			break;

		ret = lseek(msc->fd, msc->offset - len, SEEK_SET);
		if (ret < 0) {
			DBG("%s: lseek failed\n", __func__);
			break;
		}

		ret = do_readv(msc, riov, 1);
		if (ret < 0)
			break;

		ret = do_verify(msc, len);
		if (ret < 0)
			break;

		report_progress(msc, MSC_TEST_SG_8SECT);
		i++;
	}

	if (ret == 0)
		printf("success\n");
	else
		printf("failed\n");

	return ret;
}

/**
 * do_test_sg_2sect - SG write/read/verify 2 sectors at a time
 * @msc:	Mass Storage Test Context
 */
static int do_test_sg_2sect(struct usb_msc_test *msc)
{
	char			*txbuf = msc->txbuf;
	char			*rxbuf = msc->rxbuf;

	unsigned		len = 2 * msc->sect_size;

	int			ret = 0;
	int			i;

	const struct iovec	tiov[] = {
		{
			.iov_base	= txbuf,
			.iov_len	= len,
		},
	};

	const struct iovec	riov[] = {
		{
			.iov_base	= rxbuf,
			.iov_len	= len,
		},
	};

	for (i = 0; i < msc->count; i++) {
		ret = lseek(msc->fd, 0, SEEK_CUR);
		if (ret < 0) {
			DBG("%s: lseek failed\n", __func__);
			break;
		}

		msc->offset = ret;

		ret = do_writev(msc, tiov, 1);
		if (ret < 0)
			break;

		ret = lseek(msc->fd, msc->offset - len, SEEK_SET);
		if (ret < 0) {
			DBG("%s: lseek failed\n", __func__);
			break;
		}

		ret = do_readv(msc, riov, 1);
		if (ret < 0)
			break;

		ret = do_verify(msc, len);
		if (ret < 0)
			break;

		report_progress(msc, MSC_TEST_SG_2SECT);
		i++;
	}

	if (ret == 0)
		printf("success\n");
	else
		printf("failed\n");

	return ret;
}

/**
 * do_test_64sect - write/read/verify 64 sectors at a time
 * @msc:	Mass Storage Test Context
 */
static int do_test_64sect(struct usb_msc_test *msc)
{
	int			ret = 0;
	int			i;

	for (i = 0; i < msc->count; i++) {
		ret = do_write(msc, 64 * msc->sect_size);
		if (ret < 0)
			break;

		ret = do_read(msc, 64 * msc->sect_size);
		if (ret < 0)
			break;

		ret = do_verify(msc, 64 * msc->sect_size);
		if (ret < 0)
			break;

		report_progress(msc, MSC_TEST_64SECT);
		i++;
	}

	if (ret == 0)
		printf("success\n");
	else
		printf("failed\n");

	return ret;
}

/**
 * do_test_32sect - write/read/verify 32 sectors at a time
 * @msc:	Mass Storage Test Context
 */
static int do_test_32sect(struct usb_msc_test *msc)
{
	int			ret = 0;
	int			i;

	for (i = 0; i < msc->count; i++) {
		ret = do_write(msc, 32 * msc->sect_size);
		if (ret < 0)
			break;

		ret = do_read(msc, 32 * msc->sect_size);
		if (ret < 0)
			break;

		ret = do_verify(msc, 32 * msc->sect_size);
		if (ret < 0)
			break;

		report_progress(msc, MSC_TEST_32SECT);
		i++;
	}

	if (ret == 0)
		printf("success\n");
	else
		printf("failed\n");

	return ret;
}

/**
 * do_test_8sect - write/read/verify 8 sectors at a time
 * @msc:	Mass Storage Test Context
 */
static int do_test_8sect(struct usb_msc_test *msc)
{
	int			ret = 0;
	int			i;

	for (i = 0; i < msc->count; i++) {
		ret = do_write(msc, 8 * msc->sect_size);
		if (ret < 0)
			break;

		ret = do_read(msc, 8 * msc->sect_size);
		if (ret < 0)
			break;

		ret = do_verify(msc, 8 * msc->sect_size);
		if (ret < 0)
			break;

		report_progress(msc, MSC_TEST_8SECT);
		i++;
	}

	if (ret == 0)
		printf("success\n");
	else
		printf("failed\n");

	return ret;
}

/**
 * do_test_1sect - write/read/verify one sector at a time
 * @msc:	Mass Storage Test Context
 */
static int do_test_1sect(struct usb_msc_test *msc)
{
	int			ret = 0;
	int			i;

	for (i = 0; i < msc->count; i++) {
		ret = do_write(msc, msc->sect_size);
		if (ret < 0)
			break;

		ret = do_read(msc, msc->sect_size);
		if (ret < 0)
			break;

		ret = do_verify(msc, msc->sect_size);
		if (ret < 0)
			break;

		report_progress(msc, MSC_TEST_1SECT);
		i++;
	}

	if (ret == 0)
		printf("success\n");
	else
		printf("failed\n");

	return ret;
}

/**
 * do_test_simple - write/read/verify @size bytes
 * @msc:	Mass Storage Test Context
 */
static int do_test_simple(struct usb_msc_test *msc)
{
	int			ret = 0;
	int			i;

	for (i = 0; i < msc->count; i++) {
		ret = do_write(msc, msc->size);
		if (ret < 0)
			break;

		ret = do_read(msc, msc->size);
		if (ret < 0)
			break;

		ret = do_verify(msc, msc->size);
		if (ret < 0)
			break;

		report_progress(msc, MSC_TEST_SIMPLE);
		i++;
	}

	if (ret == 0)
		printf("success\n");
	else
		printf("failed\n");

	return ret;
}

/* ------------------------------------------------------------------------- */

/**
 * do_test - Write, Read and Verify
 * @msc:	Mass Storage Test Context
 * @test:	test number
 */
static int do_test(struct usb_msc_test *msc, enum usb_msc_test_case test)
{
	int			ret = 0;

	switch (test) {
	case MSC_TEST_SIMPLE:
		ret = do_test_simple(msc);
		break;
	case MSC_TEST_1SECT:
		ret = do_test_1sect(msc);
		break;
	case MSC_TEST_8SECT:
		ret = do_test_8sect(msc);
		break;
	case MSC_TEST_32SECT:
		ret = do_test_32sect(msc);
		break;
	case MSC_TEST_64SECT:
		ret = do_test_64sect(msc);
		break;
	case MSC_TEST_SG_2SECT:
		ret = do_test_sg_2sect(msc);
		break;
	case MSC_TEST_SG_8SECT:
		ret = do_test_sg_8sect(msc);
		break;
	case MSC_TEST_SG_32SECT:
		ret = do_test_sg_32sect(msc);
		break;
	case MSC_TEST_SG_64SECT:
		ret = do_test_sg_64sect(msc);
		break;
	case MSC_TEST_SG_128SECT:
		ret = do_test_sg_128sect(msc);
		break;
	case MSC_TEST_READ_PAST_LAST:
		ret = do_test_read_past_last(msc);
		break;
	case MSC_TEST_LSEEK_PAST_LAST:
		ret = do_test_lseek_past_last(msc);
		break;
	case MSC_TEST_WRITE_PAST_LAST:
		ret = do_test_write_past_last(msc);
		break;
	case MSC_TEST_SG_RANDOM_READ:
		ret = do_test_sg_random_read(msc);
		break;
	case MSC_TEST_SG_RANDOM_WRITE:
		ret = do_test_sg_random_write(msc);
		break;
	case MSC_TEST_SG_RANDOM_BOTH:
		ret = do_test_sg_random_both(msc);
		break;
	default:
		printf("%s: test %d not implemented yet\n",
				__func__, test);
		ret = -ENOTSUP;
	}

	return ret;
}

/* ------------------------------------------------------------------------- */

static void usage(char *prog)
{
	printf("Usage: %s\n\
			--output, -o	Block device to write to\n\
			--test, -t	Test number [0 - 21]\n\
			--size, -s	Size of the internal buffers\n\
			--count, -c	Iteration count\n\
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
		.name		= "test",	/* test number */
		.has_arg	= 1,
		.val		= 't',
	},
	{
		.name		= "size",	/* rx/tx buffer sizes */
		.has_arg	= 1,
		.val		= 's',
	},
	{
		.name		= "count",	/* how many iterations */
		.has_arg	= 1,
		.val		= 'c',
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
	unsigned		sect_size;
	unsigned		size = 0;
	unsigned		count = 100; /* 100 loops by default */
	int			ret = 0;

	enum usb_msc_test_case	test = MSC_TEST_SIMPLE; /* test simple */

	char			*output = NULL;


	while (ARRAY_SIZE(msc_opts)) {
		int		opt_index = 0;
		int		opt;

		opt = getopt_long(argc, argv, "o:t:s:c:dh", msc_opts, &opt_index);
		if (opt < 0)
			break;

		switch (opt) {
		case 'o':
			output = optarg;
			break;

		case 't':
			test = atoi(optarg);
			if (test < 0) {
				DBG("%s: invalid parameter\n", __func__);
				goto err0;
			}
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

		case 'c':
			count = atoi(optarg);
			if (count <= 0) {
				DBG("%s: invalid parameter\n", __func__);
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

	msc->count = count;
	msc->size = size;
	msc->output = output;

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

	ret = ioctl(msc->fd, BLKSSZGET, &sect_size);
	if (ret < 0 || sect_size == 0) {
		DBG("%s: could not get sector size\n", __func__);
		goto err3;
	}

	msc->psize = blksize;
	msc->pempty = blksize;
	msc->sect_size = sect_size;

	DBG("%s: file descriptor %d size %.2f MB\n", __func__, msc->fd,
			(float) msc->psize / 1024 / 1024);

	/* sync before starting any test in order to getter more
	 * reliable results out of the tests
	 */
	sync();

	ret = do_test(msc, test);
	if (ret < 0) {
		DBG("%s: test failed\n", __func__);
		goto err3;
	}

	printf("\n");

	close(msc->fd);
	free(msc->txbuf);
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

