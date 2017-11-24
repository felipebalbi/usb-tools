/*
 * SPDX-License-Identifier: GPL-3.0
 * Copyright (C) 2016 Felipe Balbi <felipe.balbi@linux.intel.com>
 */

#include <config.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <ctype.h>
#include <errno.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#define __maybe_unused	__attribute__((unused))

#define ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))

#define X_SIZE		1920
#define Y_SIZE		1080

#define FRAME_SIZE	((X_SIZE) * (Y_SIZE))
#define UV_SIZE		((FRAME_SIZE) / 2)

#define FULL_SIZE	((FRAME_SIZE) + (UV_SIZE))

#define NUM_THREADS	10
#define FILE_PATH	"/dev/uda_data0"

#define error(err, msg)	do { errno = (err); perror(msg); } while (0)

struct uda_thread_info {
	pthread_t	thread_id;
	int		thread_num;

	int		uda_fd;

	unsigned char	*frame;
};

static void *uda_thread_start(void *_info)
{
	struct uda_thread_info	*info = _info;
	int			*size;
	int			ret = 0;

	size = malloc(sizeof(*size));
	if (!size) {
		error(-ENOMEM, "malloc");
		ret = -1;
		goto out0;
	}

	*size = 0;

	info->frame = malloc(FULL_SIZE);
	if (!info->frame) {
		error(-ENOMEM, "thread start");
		ret = -1;
		goto out1;
	}

	ret = read(info->uda_fd, info->frame, FULL_SIZE);
	if (ret < 0) {
		error(errno, "read");
		goto out1;
	}

	*size = ret;

out1:
	free(info->frame);

out0:
	close(info->uda_fd);

	return size;
}

int main(void)
{
	struct uda_thread_info	*info;

	pthread_attr_t		attr;

	void			*thread_res;
	int			ret;
	int			fd;
	int			i;

	info = calloc(NUM_THREADS, sizeof(*info));
	if (!info)
		goto err0;

	ret = pthread_attr_init(&attr);
	if (ret != 0) {
		error(ret, "pthread_attr_init");
		goto err1;
	}

	fd = open(FILE_PATH, O_RDONLY);
	if (fd < 0) {
		error(errno, "open");
		goto err2;
	}

	for (i = 0; i < NUM_THREADS; i++) {
		info[i].thread_num = i + 1;
		info[i].uda_fd = dup(fd);

		ret = pthread_create(&info[i].thread_id, &attr,
				&uda_thread_start, &info[i]);
		if (ret != 0) {
			error(ret, "pthread_create");
			goto err2;
		}
	}

	ret = pthread_attr_destroy(&attr);
	if (ret != 0) {
		error(ret, "pthread_attr_destroy");
		goto err1;
	}

	for (i = 0; i < NUM_THREADS; i++) {
		ret = pthread_join(info[i].thread_id, &thread_res);
		if (ret != 0) {
			error(ret, "pthread_join");
			goto err1;
		}

		printf("thread #%lu read %d bytes\n", info[i].thread_id,
				*((int *) thread_res));

		free(thread_res);
	}

	close(fd);
	free(info);
	exit(EXIT_SUCCESS);

	return 0;

err2:
	pthread_attr_destroy(&attr);

err1:
	free(info);

err0:
	exit(EXIT_FAILURE);
}

