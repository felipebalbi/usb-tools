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

# Shows a nice rotating bar
spinner(){
	PROC=$1

	while [ -d /proc/$PROC ]; do
		echo -ne '\b/' ; sleep 0.05
		echo -ne '\b-' ; sleep 0.05
		echo -ne '\b\' ; sleep 0.05
		echo -ne '\b|' ; sleep 0.05
	done

	return 0
}

sudo ifconfig usb0 192.168.2.14 up

read -p "Please run the following command lines on your device:

# modprobe g_nokia
# /etc/init.d/networking start
# /etc/init.d/ssh start

Press <ENTER> when you're ready  "

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

echo "
Wait while creating test files
"

echo -n "test_file_100M.mp3        "
dd if=/dev/urandom of=test_file_100M.mp3 bs=100M count=1 &> /dev/null &
spinner $(pidof dd)
echo ""

echo -n "test_file_250x400K.mp3    "
dd if=/dev/urandom of=test_file_250x400K.mp3 bs=400K count=250 &> /dev/null &
spinner $(pidof dd)
echo ""

echo -n "test_file_10Kx10K.mp3     "
dd if=/dev/urandom of=test_file_10Kx10K.mp3 bs=10K count=10000 &> /dev/null &
spinner $(pidof dd)
echo "
"

echo -n "test 1: scp 100M file to device         "
scp -q test_file_100M.mp3 root@192.168.2.15:/home/user/MyDocs/
scp -q root@192.168.2.15:/home/user/MyDocs/test_file_100M.mp3 test_file_100Mx.mp3
if cmp test_file_100M.mp3 test_file_100Mx.mp3;
then
	echo "PASSED"
else
	echo "FAILED"
	exit;
fi

echo -n "test 2: scp 250x400K file to device     "
scp -q test_file_250x400K.mp3 root@192.168.2.15:/home/user/MyDocs/
scp -q root@192.168.2.15:/home/user/MyDocs/test_file_250x400K.mp3 test_file_250x400Kx.mp3
if cmp test_file_250x400K.mp3 test_file_250x400Kx.mp3
then
	echo "PASSED"
else
	echo "FAILED"
	exit;
fi

echo -n "test 3: scp 10Kx10K file to device      "
scp -q test_file_10Kx10K.mp3 root@192.168.2.15:/home/user/MyDocs/
scp -q root@192.168.2.15:/home/user/MyDocs/test_file_10Kx10K.mp3 test_file_10Kx10Kx.mp3
if cmp test_file_10Kx10K.mp3 test_file_10Kx10Kx.mp3
then
	echo "PASSED"
else
	echo "FAILED"
	exit;
fi

echo -n "test 4: scp 1000 small files to device  "
for i in `seq 1 1000`; do
	dd if=/dev/urandom of=test_small_file.mp3 bs=1 count=123 &> /dev/null;
	scp -q test_small_file.mp3 root@192.168.2.15:/home/user/MyDocs;
	scp -q root@192.168.2.15:/home/user/MyDocs/test_small_file.mp3 test_small_file_device.mp3
	if ! cmp test_small_file.mp3 test_small_file_device.mp3
	then
		echo "FAILED"
		exit;
	fi;
done

echo "PASSED"

rm *.mp3
