#!/bin/sh

##
# scptest.sh - scps files back and forth
#
# Copyright (C) 2010 Felipe Balbi <felipe.balbi@nokia.com>
#
# This file is part of the USB Verification Tools Project
#
# USB Tools is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public Liicense as published by
# the Free Software Foundation, either version 3 of the license, or
# (at your option) any later version.
#
# USB Tools is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with USB Tools. If not, see <http://www.gnu.org/licenses/>.
##

trap cleanup 1 2 3 6

cleanup()
{
	printf "INTERRUPTED EXITING\n"
	stop_spinner $pid
	exit 1
}

MAX=102400

start_spinner() {
	interval=0.05

	while true;
	do
		printf "\b-";
		sleep $interval;

		printf "\b\\";
		sleep $interval;

		printf "\b|";
		sleep $interval;

		printf "\b/";
		sleep $interval;
	done
}

stop_spinner() {
	exec 2>/dev/null
	kill $1
	printf "\b"
}

read -p "Please run the following command lines on your device:

# modprobe g_nokia
# /etc/init.d/networking start
# /etc/init.d/ssh start

Press <ENTER> when you're ready  " var

sudo ifconfig usb0 192.168.2.14 up
if [ $? != 0 ]; then
	exit 1;
fi

# Check whether mmc is mounted and if not,
# mount it on the correct location

MOUNTPOINT=$(ssh root@192.168.2.15 "df | grep mmcblk0p3" | awk '{ print $6 }')

if [ "$MOUNTPOINT" != "/home" ]; then
	MAJOR=$(ssh root@192.168.2.15 "cat /proc/partitions | grep mmcblk0p3" | awk '{ print $1 }')
	MINOR=$(ssh root@192.168.2.15 "cat /proc/partitions | grep mmcblk0p3" | awk '{ print $2 }')

	if [ ! -b /dev/mmcblk0p3 ]; then
		ssh root@192.168.2.15 "mknod /dev/mmcblk0p3 b $MAJOR $MINOR";
	fi

	ssh root@192.168.2.15 "mount /dev/mmcblk0p3 /home";
fi

# If we don't have /home/user/MyDocs, create it

if [ ! -d /home/user/MyDocs ]; then
	ssh root@192.168.2.15 "mkdir -p /home/user/MyDocs";
fi

printf "\nWait while creating test files\n"

printf "test_file_100M.mp3                      "
start_spinner &
pid=$!
dd if=/dev/urandom of=test_file_100M.mp3 bs=100M count=1 >/dev/null 2>&1
stop_spinner $pid
printf "DONE\n"

printf "test_file_250x400K.mp3                  "
start_spinner &
pid=$!
dd if=/dev/urandom of=test_file_250x400K.mp3 bs=400K count=250 >/dev/null 2>&1
stop_spinner $pid
printf "DONE\n"

printf "test_file_10Kx10K.mp3                   "
start_spinner &
pid=$!
dd if=/dev/urandom of=test_file_10Kx10K.mp3 bs=10K count=10000 >/dev/null 2>&1
stop_spinner $pid
printf "DONE\n"

printf "1000 random sized files                 "
if [ ! -d _output ]; then
	mkdir _output
fi

start_spinner &
pid=$!
for i in `seq 1 1000`; do
	number=$RANDOM
	let "number %= $MAX"

	dd if=/dev/urandom of=_output/test_random_$i.mp3 bs=$number count=1 >/dev/null 2>&1
done
stop_spinner $pid
printf "DONE\n"

printf "\n"

printf "test 1: scp 100M file to device         "

start_spinner &
pid=$!

scp -q test_file_100M.mp3 root@192.168.2.15:/home/user/MyDocs/
scp -q root@192.168.2.15:/home/user/MyDocs/test_file_100M.mp3 test_file_100Mx.mp3

stop_spinner $pid

if cmp -s test_file_100M.mp3 test_file_100Mx.mp3;
then
	printf "PASSED\n"
else
	printf "FAILED\n"
fi

printf "test 2: scp 250x400K file to device     "

start_spinner &
pid=$!

scp -q test_file_250x400K.mp3 root@192.168.2.15:/home/user/MyDocs/
scp -q root@192.168.2.15:/home/user/MyDocs/test_file_250x400K.mp3 test_file_250x400Kx.mp3

stop_spinner $pid

if cmp -s test_file_250x400K.mp3 test_file_250x400Kx.mp3
then
	printf "PASSED\n"
else
	printf "FAILED\n"
fi

printf "test 3: scp 10Kx10K file to device      "

start_spinner &
pid=$!

scp -q test_file_10Kx10K.mp3 root@192.168.2.15:/home/user/MyDocs/
scp -q root@192.168.2.15:/home/user/MyDocs/test_file_10Kx10K.mp3 test_file_10Kx10Kx.mp3

stop_spinner $pid

if cmp -s test_file_10Kx10K.mp3 test_file_10Kx10Kx.mp3
then
	printf "PASSED\n"
else
	printf "FAILED\n"
fi

printf "test 4: scp 1000 small files to device  "

start_spinner &
pid=$!

for i in `seq 1 1000`; do
	rm test_small*.mp3
	dd if=/dev/urandom of=test_small_file.mp3 bs=1 count=123 > /dev/null 2>&1
	scp -q test_small_file.mp3 root@192.168.2.15:/home/user/MyDocs
	scp -q root@192.168.2.15:/home/user/MyDocs/test_small_file.mp3 test_small_file_device.mp3
	if ! cmp -s test_small_file.mp3 test_small_file_device.mp3
	then
		printf "FAILED\n"
	fi;
done
stop_spinner $pid

printf "PASSED\n"

printf "test 5: pipe tar through ssh            "

start_spinner &
pid=$!

(tar cf - ./_output/* &> /dev/null | ssh -q root@192.168.2.15 sh -c "'(cd /home/user/MyDocs && tar -xf -)'") > /dev/null 2>&1

stop_spinner $pid

if ! $?; then
	printf "PASSED\n"
else
	printf "FAILED\n"
fi

rm -f *.mp3
rm -rf _output

