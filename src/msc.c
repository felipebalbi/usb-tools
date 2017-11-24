/*
 * SPDX-License-Identifier: GPL-3.0
 * Copyright (C) 2009-2016 Felipe Balbi <felipe.balbi@linux.intel.com>
 */

#include <config.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <malloc.h>
#include <time.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mount.h>
#include <sys/uio.h>

#include <openssl/sha.h>

#define __maybe_unused		__attribute__((unused))

#define false	0
#define true	!false

static unsigned debug;

/* for measuring throughput */
static struct timespec		start;
static struct timespec		end;

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
	unsigned	pattern;	/* pattern to use */
	unsigned	size;		/* buffer size */

	off_t		offset;		/* current offset */

	unsigned char	*txbuf;		/* send buffer */
	unsigned char	*rxbuf;		/* receive buffer*/
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
	MSC_RESERVED0,
	MSC_RESERCED1,
	MSC_TEST_PATTERNS,		/* write known patterns and read it back */
};

/* Patterns taken from linux/arch/x86/mm/memtest.c */
static uint8_t msc_patterns[] = {
	0x00, 0x11, 0x22, 0x33,
	0x44, 0x55, 0x66, 0x77,
	0x88, 0x99, 0xaa, 0xbb,
	0xcc, 0xdd, 0xee, 0xff,
};

/* ------------------------------------------------------------------------- */

/* units which will be used for pretty printing the amount of data
 * transferred
 */
static char units[] = {
	' ',
	'k',
	'M',
	'G',
	'T',
	'P',
	'E',
	'Z',
	'Y',
};

/**
 * init_buffer - initializes our TX buffer with known data
 * @buf:	Buffer to initialize
 */
static void init_buffer(struct usb_msc_test *msc)
{
	memset(msc->txbuf, 0x55, msc->size);
}

/**
 * alloc_buffer - allocates a @size buffer
 * @size:	Size of buffer
 */
static unsigned char *alloc_buffer(unsigned size)
{
	void			*tmp;
	int			ret;

	if (size == 0)
		return NULL;

	ret = posix_memalign(&tmp, getpagesize(), size);
	if (ret)
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

	msc->txbuf = alloc_buffer(msc->size);
	if (!msc->txbuf)
		goto err0;

	init_buffer(msc);

	msc->rxbuf = alloc_buffer(msc->size);
	if (!msc->rxbuf)
		goto err1;

	return 0;

err1:
	free(msc->txbuf);

err0:
	return ret;
}

static float throughput(struct timespec *start, struct timespec *end, size_t size)
{
	int64_t diff;

	diff = (end->tv_sec - start->tv_sec) * 1000000000;
	diff += end->tv_nsec - start->tv_nsec;

	return (float) size / ((diff / 1000000000.0) * 1024 * 1024);
}

/**
 * report_progess - reports the progress of @test
 * @msc:	Mass Storage Test Context
 * @test:	test case number
 *
 * each test case implementation is required to call this function
 * in order for the user to get progress report.
 */
static void report_progress(struct usb_msc_test *msc,
		enum usb_msc_test_case test, int show_tput)
{
	float		transferred = 0;
	unsigned int	i;
	char		unit = ' ';

	transferred = (float) msc->transferred;

	for (i = 0; i < ARRAY_SIZE(units); i++) {
		if (transferred >= 1024.0) {
			transferred /= 1024.0;
			continue;
		}
		unit = units[i];
		break;
	}

	if (!debug) {
		if (show_tput) {
			msc->read_tput /= msc->count;
			msc->write_tput /= msc->count;
			printf("\rtest %2d: sent %10.02f %cB read %10.02f MB/s write %10.02f MB/s ... ",
					test, transferred, unit, msc->read_tput, msc->write_tput);
		} else {
			printf("\rtest %2d: sent %10.02f %cB read            MB/s write            MB/s ... ",
					test, transferred, unit);
		}

		fflush(stdout);
	}
}

/* ------------------------------------------------------------------------- */

/**
 * do_write - Write txbuf to fd
 * @msc:	Mass Storage Test Context
 * @bytes:	Amount of bytes to write
 */
static int do_write(struct usb_msc_test *msc, unsigned bytes)
{
	unsigned int		done = 0;
	int			ret = -EINVAL;

	unsigned char		*buf = msc->txbuf;

	clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	while (done < bytes) {
		unsigned	size = bytes - done;

		if (size > msc->pempty)
			size = msc->pempty;

		ret = write(msc->fd, buf + done, size);
		if (ret < 0)
			goto err;

		done += ret;
		msc->pempty -= ret;

		if (msc->pempty == 0) {
			off_t		pos;

			msc->pempty = msc->psize;
			done = 0;
			pos = lseek(msc->fd, 0, SEEK_SET);
			if (pos < 0) {
				ret = (int) pos;
				goto err;
			}

		}
	}
	clock_gettime(CLOCK_MONOTONIC_RAW, &end);
	msc->write_tput += throughput(&start, &end, ret);


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
	unsigned int		done = 0;
	int			ret;

	unsigned char		*buf = msc->rxbuf;

	clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	while (done < bytes) {
		ret = read(msc->fd, buf + done, bytes - done);
		if (ret < 0) {
			perror("do_read");
			goto err;
		}

		if (ret == 0)
			goto err;

		done += ret;
		msc->transferred += ret;
	}
	clock_gettime(CLOCK_MONOTONIC_RAW, &end);
	msc->read_tput += throughput(&start, &end, bytes);

	return 0;

err:
	return ret;
}

static void __maybe_unused hexdump(char *buf, unsigned size)
{
	unsigned int		i;

	for (i = 0; i < size; i++) {
		if (i && ((i % 16) == 0))
			printf("\n");
		printf("%02x ", buf[i]);
	}
	printf("\n");
}

/**
 * do_verify - Verify consistency of data
 * @msc:	Mass Storage Test Context
 * @bytes:	Amount of data to verify
 */
static int do_verify(struct usb_msc_test *msc, unsigned bytes)
{
	unsigned char		tx_hash[SHA_DIGEST_LENGTH];
	unsigned char		rx_hash[SHA_DIGEST_LENGTH];
	unsigned char		*ret;

	ret = SHA1(msc->txbuf, bytes, tx_hash);
	if (!ret)
		return -EINVAL;

	ret = SHA1(msc->rxbuf, bytes, rx_hash);
	if (!ret)
		return -EINVAL;

	return strncmp((char *) tx_hash, (char *) rx_hash, SHA_DIGEST_LENGTH);
}

/**
 * do_writev - SG Write txbuf to fd
 * @msc:	Mass Storage Test Context
 * @iov:	iovec structure pointer
 * @count:	how many transfers
 */
static int do_writev(struct usb_msc_test *msc, const struct iovec *iov,
		unsigned count)
{
	off_t			pos;
	int			ret;

	clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	ret = writev(msc->fd, iov, count);
	if (ret < 0)
		goto err;

	clock_gettime(CLOCK_MONOTONIC_RAW, &end);
	msc->write_tput += throughput(&start, &end, ret);

	msc->pempty -= ret;

	if (msc->pempty == 0) {
		msc->pempty = msc->psize;
		pos = lseek(msc->fd, 0, SEEK_SET);
		if (pos < 0) {
			ret = (int) pos;
			goto err;
		}

	}

	pos = lseek(msc->fd, 0, SEEK_CUR);
	if (pos < 0) {
		ret = (int) pos;
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
 * @iov:	iovec structure pointer
 * @count:	how many transfers
 */
static int do_readv(struct usb_msc_test *msc, const struct iovec *iov,
		unsigned bytes)
{
	int			ret;

	clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	ret = readv(msc->fd, iov, bytes);
	if (ret < 0)
		goto err;

	clock_gettime(CLOCK_MONOTONIC_RAW, &end);
	msc->read_tput += throughput(&start, &end, ret);
	msc->transferred += ret;

	return 0;

err:
	return ret;
}

/* ------------------------------------------------------------------------- */

/*
 * do_test_patterns - write known pattern and read it back
 * @msc:	Mass Storage Test Context
 */
static int do_test_patterns(struct usb_msc_test *msc)
{
	int			ret = 0;
	int			i;

	for (i = 0; i < msc->count; i++) {
		uint8_t		pattern = msc_patterns[msc->pattern];
		off_t		pos;

		memset(msc->txbuf, pattern, msc->size);
		memset(msc->rxbuf, 0x00, msc->size);

		ret = do_write(msc, msc->size);
		if (ret < 0)
			break;

		pos = lseek(msc->fd, msc->offset - msc->size, SEEK_SET);
		if (pos < 0) {
			ret = (int) pos;
			break;
		}

		msc->offset = ret;

		ret = do_read(msc, msc->size);
		if (ret < 0)
			break;

		ret = do_verify(msc, msc->size);
		if (ret < 0)
			break;

		report_progress(msc, MSC_TEST_PATTERNS, false);
	}

	report_progress(msc, MSC_TEST_PATTERNS, true);

	return ret;
}

/**
 * do_test_sg_random_both - write and read several of random size
 * @msc:	Mass Storage Test Context
 */
static int do_test_sg_random_both(struct usb_msc_test *msc)
{
	unsigned char		*txbuf = msc->txbuf;
	unsigned char		*rxbuf = msc->rxbuf;

	unsigned		sect_size = msc->sect_size;
	unsigned		len = msc->size;

	off_t			pos;

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

	pos = lseek(msc->fd, 0, SEEK_CUR);
	if (pos < 0) {
		ret = (int) pos;
		goto err;
	}

	msc->offset = ret;

	for (i = 0; i < msc->count; i++) {
		memset(msc->rxbuf, 0x00, msc->size);

		ret = do_writev(msc, tiov, 8);
		if (ret < 0)
			goto err;

		pos = lseek(msc->fd, msc->offset - len, SEEK_SET);
		if (pos < 0) {
			ret = (int) pos;
			goto err;
		}

		ret = do_readv(msc, riov, 8);
		if (ret < 0)
			goto err;

		ret = do_verify(msc, len);
		if (ret < 0)
			goto err;

		report_progress(msc, MSC_TEST_SG_RANDOM_BOTH, false);
	}

	report_progress(msc, MSC_TEST_SG_RANDOM_BOTH, true);

err:
	return ret;
}

/**
 * do_test_sg_random_write - write several of random size, read 1 64k
 * @msc:	Mass Storage Test Context
 */
static int do_test_sg_random_write(struct usb_msc_test *msc)
{
	unsigned char		*txbuf = msc->txbuf;
	unsigned char		*rxbuf = msc->rxbuf;

	unsigned		sect_size = msc->sect_size;
	unsigned		len = msc->size;

	off_t			pos;

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

	pos = lseek(msc->fd, 0, SEEK_CUR);
	if (pos < 0) {
		ret = (int) pos;
		goto err;
	}

	msc->offset = ret;

	for (i = 0; i < msc->count; i++) {
		memset(msc->rxbuf, 0x00, msc->size);

		ret = do_writev(msc, tiov, 8);
		if (ret < 0)
			goto err;

		pos = lseek(msc->fd, msc->offset - len, SEEK_SET);
		if (pos < 0) {
			ret = (int) pos;
			goto err;
		}

		ret = do_readv(msc, riov, 1);
		if (ret < 0)
			goto err;

		ret = do_verify(msc, len);
		if (ret < 0)
			goto err;

		report_progress(msc, MSC_TEST_SG_RANDOM_WRITE, false);
	}

	report_progress(msc, MSC_TEST_SG_RANDOM_WRITE, true);

err:
	return ret;
}

/**
 * do_test_sg_random_read - write 1 64k sg and read in several of random size
 * @msc:	Mass Storage Test Context
 */
static int do_test_sg_random_read(struct usb_msc_test *msc)
{
	unsigned char		*txbuf = msc->txbuf;
	unsigned char		*rxbuf = msc->rxbuf;

	unsigned		sect_size = msc->sect_size;
	unsigned		len = msc->size;

	off_t			pos;

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

	pos = lseek(msc->fd, 0, SEEK_CUR);
	if (pos < 0) {
		ret = (int) pos;
		goto err;
	}

	msc->offset = ret;

	for (i = 0; i < msc->count; i++) {
		memset(msc->rxbuf, 0x00, msc->size);

		ret = do_writev(msc, tiov, 1);
		if (ret < 0)
			goto err;

		pos = lseek(msc->fd, msc->offset - len, SEEK_SET);
		if (pos < 0)
			goto err;

		ret = do_readv(msc, riov, 8);
		if (ret < 0)
			goto err;

		ret = do_verify(msc, len);
		if (ret < 0)
			goto err;

		report_progress(msc, MSC_TEST_SG_RANDOM_READ, false);
	}

	report_progress(msc, MSC_TEST_SG_RANDOM_READ, true);

err:
	return ret;
}

/**
 * do_test_write_past_last - attempt to write past last sector
 * @msc:	Mass Storage Test Context
 */
static int do_test_write_past_last(struct usb_msc_test *msc)
{
	off_t			pos;

	int			ret = 0;
	int			i;

	for (i = 0; i < msc->count; i++) {
		memset(msc->rxbuf, 0x00, msc->size);

		/* seek to one sector less then needed */
		pos = lseek(msc->fd, msc->psize - msc->size + msc->sect_size,
				SEEK_SET);
		if (pos < 0) {
			ret = (int) pos;
			goto err;
		}

		ret = do_write(msc, msc->size);
		if (ret >=  0) {
			ret = -EINVAL;
			goto err;
		} else {
			ret = 0;
		}

		report_progress(msc, MSC_TEST_WRITE_PAST_LAST, false);
	}

	report_progress(msc, MSC_TEST_WRITE_PAST_LAST, true);

err:
	return ret;
}

/**
 * do_test_lseek_past_last - attempt to read starting past the last sector
 * @msc:	Mass Storage Test Context
 */
static int do_test_lseek_past_last(struct usb_msc_test *msc)
{
	off_t			pos;

	int			ret = 0;
	int			i;

	for (i = 0; i < msc->count; i++) {
		pos = lseek(msc->fd, msc->psize + msc->sect_size,
				SEEK_SET);
		if (pos >= 0) {
			ret = -EINVAL;
			goto err;
		}

		report_progress(msc, MSC_TEST_LSEEK_PAST_LAST, false);
	}

	report_progress(msc, MSC_TEST_LSEEK_PAST_LAST, true);

err:
	return ret;
}

/**
 * do_test_read_past_last - attempt to read past last sector
 * @msc:	Mass Storage Test Context
 */
static int do_test_read_past_last(struct usb_msc_test *msc)
{
	off_t			pos;

	int			ret = 0;
	int			i;

	for (i = 0; i < msc->count; i++) {
		memset(msc->rxbuf, 0x00, msc->size);

		/* seek to one sector less then needed */
		pos = lseek(msc->fd, msc->psize - msc->size + msc->sect_size,
				SEEK_SET);
		if (pos < 0) {
			ret = (int) pos;
			goto err;
		}

		ret = do_read(msc, msc->size);
		if (ret > 0) {
			ret = -EINVAL;
			goto err;
		}

		report_progress(msc, MSC_TEST_READ_PAST_LAST, false);
	}

	report_progress(msc, MSC_TEST_READ_PAST_LAST, true);

err:
	return ret;
}

/**
 * do_test_sg_128sect - SG write/read/verify 8 sectors at a time
 * @msc:	Mass Storage Test Context
 */
static int do_test_sg_128sect(struct usb_msc_test *msc)
{
	unsigned char		*txbuf = msc->txbuf;
	unsigned char		*rxbuf = msc->rxbuf;

	unsigned		len = 128 * msc->sect_size;

	off_t			pos;

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

	pos = lseek(msc->fd, 0, SEEK_CUR);
	if (pos < 0) {
		ret = (int) pos;
		goto err;
	}

	msc->offset = ret;

	for (i = 0; i < msc->count; i++) {
		memset(msc->rxbuf, 0x00, msc->size);

		ret = do_writev(msc, tiov, 1);
		if (ret < 0)
			goto err;

		pos = lseek(msc->fd, msc->offset - len, SEEK_SET);
		if (pos < 0) {
			ret = (int) pos;
			goto err;
		}

		ret = do_readv(msc, riov, 1);
		if (ret < 0)
			goto err;

		ret = do_verify(msc, len);
		if (ret < 0)
			goto err;

		report_progress(msc, MSC_TEST_SG_128SECT, false);
	}

	report_progress(msc, MSC_TEST_SG_128SECT, true);

err:
	return ret;
}

/**
 * do_test_sg_64sect - SG write/read/verify 8 sectors at a time
 * @msc:	Mass Storage Test Context
 */
static int do_test_sg_64sect(struct usb_msc_test *msc)
{
	unsigned char		*txbuf = msc->txbuf;
	unsigned char		*rxbuf = msc->rxbuf;

	unsigned		len = 64 * msc->sect_size;

	off_t			pos;

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

	pos = lseek(msc->fd, 0, SEEK_CUR);
	if (pos < 0) {
		ret = (int) pos;
		goto err;
	}

	msc->offset = ret;

	for (i = 0; i < msc->count; i++) {
		memset(msc->rxbuf, 0x00, msc->size);

		ret = do_writev(msc, tiov, 1);
		if (ret < 0)
			goto err;

		pos = lseek(msc->fd, msc->offset - len, SEEK_SET);
		if (pos < 0) {
			ret = (int) pos;
			goto err;
		}

		ret = do_readv(msc, riov, 1);
		if (ret < 0)
			goto err;

		ret = do_verify(msc, len);
		if (ret < 0)
			goto err;

		report_progress(msc, MSC_TEST_SG_64SECT, false);
	}

	report_progress(msc, MSC_TEST_SG_64SECT, true);

err:
	return ret;
}

/**
 * do_test_sg_32sect - SG write/read/verify 8 sectors at a time
 * @msc:	Mass Storage Test Context
 */
static int do_test_sg_32sect(struct usb_msc_test *msc)
{
	unsigned char		*txbuf = msc->txbuf;
	unsigned char		*rxbuf = msc->rxbuf;

	unsigned		len = 32 * msc->sect_size;

	off_t			pos;

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

	pos = lseek(msc->fd, 0, SEEK_CUR);
	if (pos < 0) {
		ret = (int) pos;
		goto err;
	}

	msc->offset = ret;

	for (i = 0; i < msc->count; i++) {
		memset(msc->rxbuf, 0x00, msc->size);

		ret = do_writev(msc, tiov, 1);
		if (ret < 0)
			goto err;

		pos = lseek(msc->fd, msc->offset - len, SEEK_SET);
		if (pos < 0) {
			ret = (int) pos;
			goto err;
		}

		ret = do_readv(msc, riov, 1);
		if (ret < 0)
			goto err;

		ret = do_verify(msc, len);
		if (ret < 0)
			goto err;

		report_progress(msc, MSC_TEST_SG_32SECT, false);
	}

	report_progress(msc, MSC_TEST_SG_32SECT, true);

err:
	return ret;
}

/**
 * do_test_sg_8sect - SG write/read/verify 8 sectors at a time
 * @msc:	Mass Storage Test Context
 */
static int do_test_sg_8sect(struct usb_msc_test *msc)
{
	unsigned char		*txbuf = msc->txbuf;
	unsigned char		*rxbuf = msc->rxbuf;

	unsigned		len = 8 * msc->sect_size;

	off_t			pos;

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

	pos = lseek(msc->fd, 0, SEEK_CUR);
	if (pos < 0) {
		ret = (int) pos;
		goto err;
	}

	msc->offset = ret;

	for (i = 0; i < msc->count; i++) {
		memset(msc->rxbuf, 0x00, msc->size);

		ret = do_writev(msc, tiov, 1);
		if (ret < 0)
			goto err;

		pos = lseek(msc->fd, msc->offset - len, SEEK_SET);
		if (pos < 0) {
			ret = (int) pos;
			goto err;
		}

		ret = do_readv(msc, riov, 1);
		if (ret < 0)
			goto err;

		ret = do_verify(msc, len);
		if (ret < 0)
			goto err;

		report_progress(msc, MSC_TEST_SG_8SECT, false);
	}

	report_progress(msc, MSC_TEST_SG_8SECT, true);

err:
	return ret;
}

/**
 * do_test_sg_2sect - SG write/read/verify 2 sectors at a time
 * @msc:	Mass Storage Test Context
 */
static int do_test_sg_2sect(struct usb_msc_test *msc)
{
	unsigned char		*txbuf = msc->txbuf;
	unsigned char		*rxbuf = msc->rxbuf;

	unsigned		len = 2 * msc->sect_size;

	off_t			pos;

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

	pos = lseek(msc->fd, 0, SEEK_CUR);
	if (pos < 0) {
		ret = (int) pos;
		goto err;
	}

	msc->offset = ret;

	for (i = 0; i < msc->count; i++) {
		memset(msc->rxbuf, 0x00, msc->size);

		ret = do_writev(msc, tiov, 1);
		if (ret < 0)
			goto err;

		pos = lseek(msc->fd, msc->offset - len, SEEK_SET);
		if (pos < 0) {
			ret = (int) pos;
			goto err;
		}

		ret = do_readv(msc, riov, 1);
		if (ret < 0)
			goto err;

		ret = do_verify(msc, len);
		if (ret < 0)
			goto err;

		report_progress(msc, MSC_TEST_SG_2SECT, false);
	}

	report_progress(msc, MSC_TEST_SG_2SECT, true);

err:
	return ret;
}

/**
 * do_test_64sect - write/read/verify 64 sectors at a time
 * @msc:	Mass Storage Test Context
 */
static int do_test_64sect(struct usb_msc_test *msc)
{
	off_t			pos;

	int			ret = 0;
	int			i;

	pos = lseek(msc->fd, 0, SEEK_CUR);
	if (pos < 0) {
		ret = (int) pos;
		goto err;
	}

	msc->offset = ret;

	for (i = 0; i < msc->count; i++) {
		memset(msc->rxbuf, 0x00, msc->size);

		ret = do_write(msc, 64 * msc->sect_size);
		if (ret < 0)
			break;

		pos = lseek(msc->fd, msc->offset - 64 * msc->sect_size,
				SEEK_SET);
		if (pos < 0) {
			ret = (int) pos;
			goto err;
		}

		msc->offset = ret;

		ret = do_read(msc, 64 * msc->sect_size);
		if (ret < 0)
			break;

		ret = do_verify(msc, 64 * msc->sect_size);
		if (ret < 0)
			break;

		report_progress(msc, MSC_TEST_64SECT, false);
	}

	report_progress(msc, MSC_TEST_64SECT, true);

err:
	return ret;
}

/**
 * do_test_32sect - write/read/verify 32 sectors at a time
 * @msc:	Mass Storage Test Context
 */
static int do_test_32sect(struct usb_msc_test *msc)
{
	off_t			pos;

	int			ret = 0;
	int			i;

	pos = lseek(msc->fd, 0, SEEK_CUR);
	if (pos < 0) {
		ret = (int) pos;
		goto err;
	}

	msc->offset = ret;

	for (i = 0; i < msc->count; i++) {
		memset(msc->rxbuf, 0x00, msc->size);

		ret = do_write(msc, 32 * msc->sect_size);
		if (ret < 0)
			break;

		pos = lseek(msc->fd, msc->offset - 32 * msc->sect_size,
				SEEK_SET);
		if (pos < 0) {
			ret = (int) pos;
			goto err;
		}

		msc->offset = ret;

		ret = do_read(msc, 32 * msc->sect_size);
		if (ret < 0)
			break;

		ret = do_verify(msc, 32 * msc->sect_size);
		if (ret < 0)
			break;

		report_progress(msc, MSC_TEST_32SECT, false);
	}

	report_progress(msc, MSC_TEST_32SECT, true);

err:
	return ret;
}

/**
 * do_test_8sect - write/read/verify 8 sectors at a time
 * @msc:	Mass Storage Test Context
 */
static int do_test_8sect(struct usb_msc_test *msc)
{
	off_t			pos;

	int			ret = 0;
	int			i;

	pos = lseek(msc->fd, 0, SEEK_CUR);
	if (pos < 0) {
		ret = (int) pos;
		goto err;
	}

	msc->offset = ret;

	for (i = 0; i < msc->count; i++) {
		memset(msc->rxbuf, 0x00, msc->size);

		ret = do_write(msc, 8 * msc->sect_size);
		if (ret < 0)
			goto err;

		pos = lseek(msc->fd, msc->offset - 8 * msc->sect_size,
				SEEK_SET);
		if (pos < 0) {
			ret = (int) pos;
			goto err;
		}

		msc->offset = ret;

		ret = do_read(msc, 8 * msc->sect_size);
		if (ret < 0)
			goto err;

		ret = do_verify(msc, 8 * msc->sect_size);
		if (ret < 0)
			goto err;

		report_progress(msc, MSC_TEST_8SECT, false);
	}

	report_progress(msc, MSC_TEST_8SECT, true);

err:
	return ret;
}

/**
 * do_test_1sect - write/read/verify one sector at a time
 * @msc:	Mass Storage Test Context
 */
static int do_test_1sect(struct usb_msc_test *msc)
{
	off_t			pos;

	int			ret = 0;
	int			i;

	pos = lseek(msc->fd, 0, SEEK_CUR);
	if (pos < 0) {
		ret = (int) pos;
		goto err;
	}

	msc->offset = ret;

	for (i = 0; i < msc->count; i++) {
		memset(msc->rxbuf, 0x00, msc->size);

		ret = do_write(msc, msc->sect_size);
		if (ret < 0)
			goto err;

		pos = lseek(msc->fd, msc->offset - 1 * msc->sect_size,
				SEEK_SET);
		if (pos < 0) {
			ret = (int) pos;
			goto err;
		}

		msc->offset = ret;

		ret = do_read(msc, msc->sect_size);
		if (ret < 0)
			goto err;

		ret = do_verify(msc, msc->sect_size);
		if (ret < 0)
			goto err;

		report_progress(msc, MSC_TEST_1SECT, false);
	}

	report_progress(msc, MSC_TEST_1SECT, true);

err:
	return ret;
}

/**
 * do_test_simple - write/read/verify @size bytes
 * @msc:	Mass Storage Test Context
 */
static int do_test_simple(struct usb_msc_test *msc)
{
	off_t			pos;

	int			ret = 0;
	int			i;

	pos = lseek(msc->fd, 0, SEEK_CUR);
	if (pos < 0) {
		ret = (int) pos;
		goto err;
	}

	msc->offset = ret;

	for (i = 0; i < msc->count; i++) {
		memset(msc->rxbuf, 0x00, msc->size);

		ret = do_write(msc, msc->size);
		if (ret < 0)
			goto err;

		pos = lseek(msc->fd, msc->offset - msc->size, SEEK_SET);
		if (pos < 0) {
			ret = (int) pos;
			goto err;
		}

		msc->offset = ret;

		ret = do_read(msc, msc->size);
		if (ret < 0)
			goto err;

		ret = do_verify(msc, msc->size);
		if (ret < 0)
			goto err;

		report_progress(msc, MSC_TEST_SIMPLE, false);
	}

	report_progress(msc, MSC_TEST_SIMPLE, true);

err:
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
	case MSC_TEST_PATTERNS:
		ret = do_test_patterns(msc);
		break;
	default:
		printf("%s: test %d is not supported\n",
				__func__, test);
		ret = -ENOTSUP;
	}

	if (ret < 0)
		printf("failed\n");
	else
		printf("success\n");

	return ret;
}

/* ------------------------------------------------------------------------- */

static void usage(char *prog)
{
	printf("Usage: %s\n\
			--output, -o		Block device to write to\n\
			--test, -t		Test number [0 - 21]\n\
			--size, -s		Size of the internal buffers\n\
			--count, -c		Iteration count\n\
			--debug, -d		Enables debugging messages\n\
			--dsync, -n		Enables O_DSYNC\n\
			--pattern, -p		Pattern chosen\n\
			--help, -h		This help\n", prog);
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
		.name		= "pattern",	/* chosen pattern */
		.has_arg	= 1,
		.val		= 'p',
	},
	{
		.name		= "debug",
		.val		= 'd',
	},
	{
		.name		= "dsync",
		.val		= 'n',
	},
	{
		.name		= "help",
		.val		= 'h',
	},
	{  } /* Terminating entry */
};

int main(int argc, char *argv[])
{
	struct usb_msc_test	*msc;

	uint64_t		blksize;
	unsigned		pattern = 0;
	unsigned		sect_size;
	unsigned		size = 0;
	unsigned		mult = 1;
	unsigned		count = 100; /* 100 loops by default */
	int			flags = O_RDWR | O_DIRECT;
	int			ret = 0;

	enum usb_msc_test_case	test = MSC_TEST_SIMPLE; /* test simple */

	char			*output = NULL;
	char			*tmp;

	while (ARRAY_SIZE(msc_opts)) {
		int		opt_index = 0;
		int		opt;

		opt = getopt_long(argc, argv, "o:t:s:c:p:b:dnh", msc_opts, &opt_index);
		if (opt < 0)
			break;

		switch (opt) {
		case 'o':
			output = optarg;
			break;

		case 't':
			test = strtoul(optarg, NULL, 10);
			break;

		case 's':
			tmp = optarg;

			while (isdigit(*tmp))
				tmp++;

			switch (*tmp) {
			case 'G':
			case 'g':
				mult *= 1024;
				/* FALLTHROUGH */
			case 'M':
			case 'm':
				mult *= 1024;
				/* FALLTHROUGH */
			case 'k':
			case 'K':
				mult *= 1024;
				break;
			}
			*tmp = '\0';

			size = atoi(optarg);
			if (size == 0) {
				ret = -EINVAL;
				goto err0;
			}

			size *= mult;
			break;
		case 'c':
			count = atoi(optarg);
			if (count <= 0)
				goto err0;
			break;
		case 'n':
			flags |= O_DSYNC;
			break;
		case 'p':
			pattern = atoi(optarg);
			if (pattern > ARRAY_SIZE(msc_patterns))
				goto err0;
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
		ret = -EINVAL;
		goto err0;
	}

	msc = malloc(sizeof(*msc));
	if (!msc) {
		ret = -ENOMEM;
		goto err0;
	}

	memset(msc, 0x00, sizeof(*msc));

	msc->count = count;
	msc->size = size;
	msc->output = output;
	msc->pattern = pattern;

	ret = alloc_and_init_buffer(msc);
	if (ret < 0)
		goto err1;

	msc->fd = open(output, flags);
	if (msc->fd < 0) {
		ret = -1;
		goto err2;
	}

	ret = ioctl(msc->fd, BLKGETSIZE64, &blksize);
	if (ret < 0 || blksize == 0)
		goto err3;

	ret = ioctl(msc->fd, BLKSSZGET, &sect_size);
	if (ret < 0 || sect_size == 0)
		goto err3;

	msc->psize = blksize;
	msc->pempty = blksize;
	msc->sect_size = sect_size;

	/*
	 * sync before starting any test in order to get more
	 * reliable results out of the tests
	 */
	ret = fsync(msc->fd);
	if (ret)
		goto err3;

	ret = do_test(msc, test);

	if (ret < 0)
		goto err3;

	close(msc->fd);
	free(msc->txbuf);
	free(msc->rxbuf);
	free(msc);

	return 0;

err3:
	close(msc->fd);

err2:
	free(msc->txbuf);
	free(msc->rxbuf);

err1:
	free(msc);

err0:
	return ret;
}

