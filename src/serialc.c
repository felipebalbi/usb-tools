/*
 * SPDX-License-Identifier: GPL-3.0
 * Copyright (C) 2009-2016 Felipe Balbi <felipe.balbi@linux.intel.com>
 */

#include <config.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <malloc.h>
#include <dirent.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <sys/ioctl.h>
#include <linux/usbdevice_fs.h>

static unsigned debug;

/* for measuring throughtput */
static struct timeval		start;
static struct timeval		end;

static int alive = 1;

#define ARRAY_SIZE(x)		(sizeof(x) / sizeof((x)[0]))
#define TIMEOUT			2000	/* ms */
#define	MAX_USBFS_BUFFER_SIZE	16384
#define MIN_TPUT		0xffffffff

/* #include <linux/usb_ch9.h> */

struct usb_device_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;
	__u16 bcdUSB;
	__u8  bDeviceClass;
	__u8  bDeviceSubClass;
	__u8  bDeviceProtocol;
	__u8  bMaxPacketSize0;
	__u16 idVendor;
	__u16 idProduct;
	__u16 bcdDevice;
	__u8  iManufacturer;
	__u8  iProduct;
	__u8  iSerialNumber;
	__u8  bNumConfigurations;
} __attribute__ ((packed));

/**
 * usb_serial_test - USB u_serial Test Context
 * @transferred:	amount of data transferred so far
 * @read_usecs:		number of microseconds spent in reads
 * @write_usecs:	number of microseconds spent in writes
 * @read_tput:		read throughput
 * @write_tput:		write throughput
 * @read_maxtput:	read max throughput
 * @write_maxtput:	write max throughput
 * @read_mintput:	read min throughput
 * @write_maxtput:	write min throughput
 * @read_avgtput:	read average throughput
 * @write_avgtput:	write average throughput
 * @size		size of the serial buffer
 * @interface_num:	interface number
 * @alt_setting:	alternate setting
 * @eprx:		rx endpoint number
 * @eptx:		tx endpoint number
 * @size:		buffer size
 * @txbuf:		tx buffer
 * @rxbuf:		rx buffer
 */
struct usb_serial_test {
	int			udevh;
	uint64_t		transferred;

	uint64_t		read_usecs;
	uint64_t		write_usecs;

	float			read_tput;
	float			write_tput;

	float			read_maxtput;
	float			write_maxtput;

	float			read_mintput;
	float			write_mintput;

	float			read_avgtput;
	float			write_avgtput;

	unsigned		size;

	int			interface_num;
	int			alt_setting;

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
 * signal_exit - set alive to 0
 * @sig:	Signal caught
 */
void signal_exit(int sig)
{
	if (debug)
		printf("received signal %d\n", sig);
	alive = 0;
}

/**
 * init_buffer - initializes our TX buffer with pseudo random data
 * @buf:	Buffer to initialize
 */
static void init_buffer(struct usb_serial_test *serial)
{
	unsigned int		i;
	unsigned char		*buf = serial->txbuf;

	for (i = 0; i < serial->size; i++)
		buf[i] = rand() & 0xFF;
}

/**
 * alloc_buffer - allocates a @size buffer
 * @size:	Size of buffer
 */
static unsigned char *alloc_buffer(unsigned size)
{
	unsigned char		*tmp;

	if (size == 0)
		return NULL;

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
	if (!tmp)
		goto err0;

	serial->txbuf = tmp;
	init_buffer(serial);

	tmp = alloc_buffer(serial->size);
	if (!tmp)
		goto err1;

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
	struct usbdevfs_setinterface setintf;

	/* FIXME for now this will only work with g_nokia, but we will
	 * extend this later in order to work with any u_serial
	 * gadget driver
	 */

	ret = ioctl(serial->udevh, USBDEVFS_CLAIMINTERFACE,
			&serial->interface_num);
	if (ret < 0)
		goto err0;

	setintf.interface = serial->interface_num;
	setintf.altsetting = serial->alt_setting;

	ret = ioctl(serial->udevh, USBDEVFS_SETINTERFACE, &setintf);
	if (ret < 0)
		goto err1;

	return 0;

err1:
	ioctl(serial->udevh, USBDEVFS_RELEASEINTERFACE, &serial->interface_num);

err0:
	return ret;
}

static void release_interface(struct usb_serial_test *serial)
{
	ioctl(serial->udevh, USBDEVFS_RELEASEINTERFACE, &serial->interface_num);
}

/**
 * throughput - Calculate throughput in megabits per second
 * @usecs:	Number of microseconds taken
 * @size:	Size of data transfered
 */
static float throughput(int64_t usecs, uint64_t size)
{
	float bits = (float) size * 8.0;
	float secs = (float) usecs / 1000000.0;
	return (bits / secs) / 1000000.0;
}

/**
 * usecs - Calculate number of microseconds between two timevals
 * @start:	Start time
 * @end:	End time
 */
static int64_t usecs(struct timeval *start, struct timeval *end)
{
	int64_t			diff;

	diff = (end->tv_sec - start->tv_sec) * 1000000;
	diff += end->tv_usec - start->tv_usec;

	return diff;
}

/**
 * do_write - Write txbuf to fd
 * @serial:	Serial Test Context
 * @bytes:	amount of data to write
 */
static int do_write(struct usb_serial_test *serial, uint32_t bytes)
{
	unsigned int			transferred = 0;
	unsigned int			done = 0;
	int				ret;
	struct usbdevfs_bulktransfer	bulk;

	serial->txbuf[0] = (bytes >> 24) & 0xFF;
	serial->txbuf[1] = (bytes >> 16) & 0xFF;
	serial->txbuf[2] = (bytes >>  8) & 0xFF;
	serial->txbuf[3] = (bytes >>  0) & 0xFF;
	if (bytes > 4)
		serial->txbuf[bytes-1] = 0xff;

	gettimeofday(&start, NULL);
	while (done < bytes) {
		bulk.ep = serial->eptx;
		bulk.len = bytes - done;
		if (bulk.len > MAX_USBFS_BUFFER_SIZE)
			bulk.len = MAX_USBFS_BUFFER_SIZE;
		bulk.timeout = TIMEOUT;
		bulk.data = serial->txbuf + done;

		ret = ioctl(serial->udevh, USBDEVFS_BULK, &bulk);
		if (ret < 0)
			goto err;
		transferred = ret;

		serial->transferred += transferred;
		done += transferred;
	}
	gettimeofday(&end, NULL);
	serial->write_tput = throughput(usecs(&start, &end), done);
	if (done > 0)
		serial->write_usecs += usecs(&start, &end);

	/* total average over duration of test */
	serial->write_avgtput = throughput(serial->write_usecs,
					serial->transferred);

	if (serial->write_tput > serial->write_maxtput)
		serial->write_maxtput = serial->write_tput;

	if (serial->write_tput < serial->write_mintput)
		serial->write_mintput = serial->write_tput;

	if (!(bytes % 512)) {
		bulk.ep = serial->eptx;
		bulk.len = 0;
		bulk.timeout = TIMEOUT;
		bulk.data = serial->txbuf + done;
		ret = ioctl(serial->udevh, USBDEVFS_BULK, &bulk);
		if (ret < 0)
			goto err;
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
static int do_read(struct usb_serial_test *serial, uint32_t bytes)
{
	unsigned int			transferred = 0;
	unsigned int			done = 0;
	int				ret;
	struct usbdevfs_bulktransfer	bulk;

	gettimeofday(&start, NULL);
	while (done < bytes) {
		bulk.ep = serial->eprx;
		bulk.len = bytes - done;
		if (bulk.len > MAX_USBFS_BUFFER_SIZE)
			bulk.len = MAX_USBFS_BUFFER_SIZE;
		bulk.timeout = TIMEOUT;
		bulk.data = (unsigned char *)serial->rxbuf + done;

		ret = ioctl(serial->udevh, USBDEVFS_BULK, &bulk);
		if (ret < 0)
			goto err;
		transferred = ret;

		done += transferred;
	}
	gettimeofday(&end, NULL);
	serial->read_tput = throughput(usecs(&start, &end), done);
	if (done > 0)
		serial->read_usecs += usecs(&start, &end);

	/* total average over duration of test */
	serial->read_avgtput = throughput(serial->read_usecs,
					serial->transferred);

	if (serial->read_tput > serial->read_maxtput)
		serial->read_maxtput = serial->read_tput;

	if (serial->read_tput < serial->read_mintput)
		serial->read_mintput = serial->read_tput;

	return 0;

err:
	return ret;
}

/**
 * do_verify - Verify consistency of data
 * @serial:	Serial Test Context
 * @bytes:	amount of data to verify
 */
static int do_verify(struct usb_serial_test *serial, uint32_t bytes)
{
	unsigned int		i;

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
static int do_test(struct usb_serial_test *serial, uint32_t bytes)
{
	int			ret;

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

static int open_with_vid_pid(int vid, int pid)
{
	DIR				*dir, *subdir;
	struct				dirent *ent, *subent;
	struct usb_device_descriptor	desc;
	char				path[22];
	int				fd = -1;

	dir = opendir("/dev/bus/usb");
	if (!dir)
		return -1;

	while ((ent = readdir(dir))) {
		if (ent->d_name[0] == '.')
			continue;

		sprintf(path, "/dev/bus/usb/%s", ent->d_name);

		subdir = opendir(path);
		if (!subdir)
			continue;

		while ((subent = readdir(subdir))) {
			if (subent->d_name[0] == '.')
				continue;

			sprintf(path, "/dev/bus/usb/%s/%s",
				ent->d_name, subent->d_name);

			fd = open(path, O_RDWR);
			if (fd < 0)
				continue;

			if ((read(fd, &desc, sizeof desc) == sizeof desc)
			&& (desc.idVendor == vid && desc.idProduct == pid)) {
				closedir(subdir);
				goto ret;
			}

			close(fd);
		}

		closedir(subdir);
	}

	fd = -1;
ret:
	closedir(dir);

	return fd;
}

static void usage(char *prog)
{
	fprintf(stderr, "Usage: %s\n"
			"\t--vid, -v       USB Vendor ID\n"
			"\t--pid, -p       USB Product ID\n"
			"\t--inum, -i	interface number\n"
			"\t--alt, -a	alternate setting\n"
			"\t--rxep, -r	rx endpoint number\n"
			"\t--txep, -t	tx endpoint number\n"
			"\t--size, -s	Internal buffer size\n"
			"\t--fixed, -f	Use fixed transfer size\n"
			"\t--debug, -d	Enables debugging messages\n"
			"\t--help, -h	This help\n", prog);
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
		.name		= "inum",
		.has_arg	= 1,
		.val		= 'i',
	},
	{
		.name		= "alt",
		.has_arg	= 1,
		.val		= 'a',
	},
	{
		.name		= "rxep",
		.has_arg	= 1,
		.val		= 'r',
	},
	{
		.name		= "txep",
		.has_arg	= 1,
		.val		= 't',
	},
	{
		.name		= "size",	/* rx/tx buffer sizes */
		.has_arg	= 1,
		.val		= 's',
	},
	{
		.name		= "fixed",
		.val		= 'f',
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
	struct usb_serial_test	*serial;
	unsigned		size = 0;
	uint8_t			fixed = 0;
	int			if_num = 0;
	int			alt_set = 0;
	uint8_t			eprx = 0;
	uint8_t			eptx = 0;
	int			ret = 0;
	unsigned		vid = 0;
	unsigned		pid = 0;
	struct sigaction	sa;

	sa.sa_handler = signal_exit;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;

	if (sigaction(SIGINT, &sa, NULL) == -1)
		printf("failed to handle signal\n");

	(void) signal(SIGINT, signal_exit);

	while (ARRAY_SIZE(serial_opts)) {
		int		optidx = 0;
		int		opt;

	opt = getopt_long(argc, argv, "v:p:s:i:a:r:t:dhf", serial_opts, &optidx);
	if (opt < 0)
			break;

		switch (opt) {
		case 'v':
			vid = strtoul(optarg, NULL, 16);
			break;
		case 'p':
			pid = strtoul(optarg, NULL, 16);
			break;
		case 'r':
			eprx = strtol(optarg,NULL, 16);
			break;
		case 't':
			eptx = strtol(optarg, NULL, 16);
			break;
		case 'i':
			if_num = atoi(optarg);
			break;
		case 'a':
			alt_set = atoi(optarg);
			break;
		case 's':
			size = atoi(optarg);
			if (size == 0) {
				ret = -EINVAL;
				goto err0;
			}
			break;
		case 'f':
			fixed = 1;
			break;
		case 'd':
			debug = 1;
			break;
		case 'h': /* FALLTHROUGH */
		default:
			usage(argv[0]);
			return 1;
		}
	}

	if (!vid || !pid) {
		fprintf(stderr, "%s: missing arguments\n"
			"Try `%s --help' for more information\n",
			argv[0], argv[0]);
		return 1;
	}

	serial = malloc(sizeof(*serial));
	if (!serial) {
		fprintf(stderr, "%s: unable to allocate memory\n", argv[0]);
		ret = -ENOMEM;
		goto err0;
	}

	memset(serial, 0x00, sizeof(*serial));

	serial->size = size;
	serial->eprx = eprx;
	serial->eptx = eptx;
	serial->interface_num = if_num;
	serial->alt_setting = alt_set;

	serial->write_mintput = MIN_TPUT;
	serial->read_mintput = MIN_TPUT;

	ret = alloc_and_init_buffer(serial);
	if (ret < 0) {
		fprintf(stderr, "%s: failed to allocate buffers\n", argv[0]);
		goto err1;
	}

	serial->udevh = open_with_vid_pid(vid, pid);
	if (serial->udevh < 0) {
		fprintf(stderr, "%s: open failed %s\n",
			argv[0], strerror(errno));
		goto err2;
	}

	/* get descriptors, find correct interface to claim and
	 * set correct alternate setting
	 */
	ret = find_and_claim_interface(serial);
	if (ret < 0) {
		fprintf(stderr, "%s: unable to claim interface: %s\n",
			argv[0], strerror(errno));
		goto err2;
	}

	if (!fixed)
		srandom(time(NULL));

	printf("\n");
	do {
		float		transferred = 0;
		unsigned int	i;
		unsigned int	n;
		char		*unit = NULL;

		if (!fixed) {
			/* We want packet size to be in range [4 , serial->size],
			*  as first four bytes are holding the packet size */
			n = random() % (serial->size - 3) + 4;
		} else {
			n = size;
		}

		ret = do_test(serial, n);
		if (ret < 0) {
			fprintf(stderr, "%s: test failed: %s\n",
				argv[0], strerror(errno));
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

		if (debug == 0) {
			printf("[ V%04x P%04x Transferred %10.04f %sByte%s read %10.02f Mb/s write %10.02f Mb/s ]\n",
					vid, pid, transferred, unit, transferred > 1 ? "s" : "",
					serial->read_tput, serial->write_tput);

			printf("[ read min: %10.02f Mb/s - max:  %10.02f Mb/s - avg: %10.02f Mb/s ]\n",
				serial->read_mintput, serial->read_maxtput, serial->read_avgtput);

			printf("[ write min: %10.02f Mb/s - max: %10.02f Mb/s - avg: %10.02f Mb/s ]\n",
				serial->write_mintput, serial->write_maxtput, serial->write_avgtput);

			printf("\033[3A");
			fflush(stdout);
		}
	} while (alive);

	printf("\n\n\n");

	release_interface(serial);
	close(serial->udevh);
	free(serial->rxbuf);
	free(serial->txbuf);
	free(serial);

	return 0;

err3:
	release_interface(serial);

err2:
	close(serial->udevh);
	free(serial->rxbuf);
	free(serial->txbuf);

err1:
	free(serial);

err0:
	return ret;
}

