#!/bin/sh

##
# msc.sh - Better setup for running msc tests
#
# Copyright (C) 2009 Felipe Balbi <felipe.balbi@nokia.com>
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

OUTPUT=""
COUNT=1000

TEMP=`getopt -o "o:c:h" -n 'msc.sh' -- "$@"`

eval set -- "$TEMP"

while true; do
	case $1 in
		-o)
			OUTPUT=$2;
			shift 2;;
		-c)
			COUNT=$2;
			shift 2;;
		-h)
			echo "$0:
	-o output
	-c count
	-h this help"
			exit 1;;
		--)
			break;;
		*)
			echo "Interal error!";
			exit 1;;
	esac
done

printf "Starting test suite: $(date)\n"

# Simple read and write
printf "test 0a: simple 4k read/write				\n"
msc -t 0 -o $OUTPUT -s 4096 -c $COUNT

printf "test 0b: simple 8k read/write				\n"
msc -t 0 -o $OUTPUT -s 8192 -c $COUNT

printf "test 0c: simple 16k read/write				\n"
msc -t 0 -o $OUTPUT -s 16384 -c $COUNT

printf "test 0d: simple 32k read/write				\n"
msc -t 0 -o $OUTPUT -s 32768 -c $COUNT

printf "test 0e: simple 64k read/write				\n"
msc -t 0 -o $OUTPUT -s 65536 -c $COUNT

# 1-sector read/write
printf "test 1: simple 1-sector read/write			\n"
msc -t 1 -o $OUTPUT -s 65536 -c $COUNT

# 8-sectors read/write
printf "test 2: simple 8-sectors read/write			\n"
msc -t 2 -o $OUTPUT -s 65536 -c $COUNT

# 32-sectors read/write
printf "test 3: simple 32-sectors read/write			\n"
msc -t 3 -o $OUTPUT -s 65536 -c $COUNT

# 64-sectors read/write
printf "test 4: simple 64-sectors read/write			\n"
msc -t 4 -o $OUTPUT -s 65536 -c $COUNT

# SG 2-sectors read/write
printf "test 5a: scatter/gather for 2-sectors buflen 4k		\n"
msc -t 5 -o $OUTPUT -s 4096 -c $COUNT

printf "test 5b: scatter/gather for 2-sectors buflen 8k		\n"
msc -t 5 -o $OUTPUT -s 8192 -c $COUNT

printf "test 5c: scatter/gather for 2-sectors buflen 16k	\n"
msc -t 5 -o $OUTPUT -s 16384 -c $COUNT

printf "test 5d: scatter/gather for 2-sectors buflen 32k	\n"
msc -t 5 -o $OUTPUT -s 32768 -c $COUNT

printf "test 5e: scatter/gather for 2-sectors buflen 64k	\n"
msc -t 5 -o $OUTPUT -s 65536 -c $COUNT

# SG 8-sectors read/write
printf "test 6a: scatter/gather for 8-sectors buflen 4k		\n"
msc -t 6 -o $OUTPUT -s 4096 -c $COUNT

printf "test 6b: scatter/gather for 8-sectors buflen 8k		\n"
msc -t 6 -o $OUTPUT -s 8192 -c $COUNT

printf "test 6c: scatter/gather for 8-sectors buflen 16k	\n"
msc -t 6 -o $OUTPUT -s 16384 -c $COUNT

printf "test 6d: scatter/gather for 8-sectors buflen 32k	\n"
msc -t 6 -o $OUTPUT -s 32768 -c $COUNT

printf "test 6e: scatter/gather for 8-sectors buflen 64k	\n"
msc -t 6 -o $OUTPUT -s 65536 -c $COUNT

# SG 32-sectors read/write
printf "test 7a: scatter/gather for 32-sectors buflen 16k	\n"
msc -t 7 -o $OUTPUT -s 16384 -c $COUNT

printf "test 7b: scatter/gather for 32-sectors buflen 32k	\n"
msc -t 7 -o $OUTPUT -s 32768 -c $COUNT

printf "test 7c: scatter/gather for 32-sectors buflen 64k	\n"
msc -t 7 -o $OUTPUT -s 65536 -c $COUNT

# SG 64-sectors read/write
printf "test 8a: scatter/gather for 64-sectors buflen 32k	\n"
msc -t 8 -o $OUTPUT -s 32768 -c $COUNT

printf "test 8b: scatter/gather for 64-sectors buflen 64k	\n"
msc -t 8 -o $OUTPUT -s 65536 -c $COUNT

# SG 128-sectors read/write
printf "test 9: scatter/gather for 128-sectors buflen 64k	\n"
msc -t 9 -o $OUTPUT -s 65536 -c $COUNT

# Read past the last sector
printf "test 10: read over the end of the block device		\n"
msc -t 10 -o $OUTPUT -s 65536 -c $COUNT

# Lseek past the last sector
printf "test 11: lseek past the end of the block device		\n"
msc -t 11 -o $OUTPUT -s 65536 -c $COUNT

# Write past the last sector
printf "test 12: write over the end of the block device		\n"
msc -t 12 -o $OUTPUT -s 65536 -c $COUNT

# write 1 sg, read in 8 random size sgs
printf "test 13: write 1 sg, read 8 random size sgs		\n"
msc -t 13 -o $OUTPUT -s 65536 -c $COUNT

# write 8 random size sgs, read 1 sg
printf "test 14: write 8 random size sgs, read 1 sg		\n"
msc -t 14 -o $OUTPUT -s 65536 -c $COUNT

# write and read 8 random size sgs
printf "test 15: write and read 8 random size sgs		\n"
msc -t 15 -o $OUTPUT -s 65536 -c $COUNT

# read with differently allocated buffers
printf "test 16a: read with heap allocated buffer		\n"
msc -t 16 -o $OUTPUT -s 65536 -c $COUNT -b heap

printf "test 16b: read with stack allocated buffer		\n"
msc -t 16 -o $OUTPUT -s 65536 -c $COUNT -b stack

# write with differently allocated buffers
printf "test 17a: write with heap allocated buffer		\n"
msc -t 17 -o $OUTPUT -s 65536 -c $COUNT -b heap

printf "test 17b: write with stack allocated buffer		\n"
msc -t 17 -o $OUTPUT -s 65536 -c $COUNT -b stack

# write known patterns and read it back
printf "test 18a: write 0x00 and read it back			\n"
msc -t 18 -o $OUTPUT -s 65536 -c $COUNT -p 0

printf "test 18b: write 0x11 and read it back			\n"
msc -t 18 -o $OUTPUT -s 65536 -c $COUNT -p 1

printf "test 18c: write 0x22 and read it back			\n"
msc -t 18 -o $OUTPUT -s 65536 -c $COUNT -p 2

printf "test 18d: write 0x33 and read it back			\n"
msc -t 18 -o $OUTPUT -s 65536 -c $COUNT -p 3

printf "test 18e: write 0x44 and read it back			\n"
msc -t 18 -o $OUTPUT -s 65536 -c $COUNT -p 4

printf "test 18f: write 0x55 and read it back			\n"
msc -t 18 -o $OUTPUT -s 65536 -c $COUNT -p 5

printf "test 18g: write 0x66 and read it back			\n"
msc -t 18 -o $OUTPUT -s 65536 -c $COUNT -p 6

printf "test 18h: write 0x77 and read it back			\n"
msc -t 18 -o $OUTPUT -s 65536 -c $COUNT -p 7

printf "test 18i: write 0x88 and read it back			\n"
msc -t 18 -o $OUTPUT -s 65536 -c $COUNT -p 8

printf "test 18j: write 0x99 and read it back			\n"
msc -t 18 -o $OUTPUT -s 65536 -c $COUNT -p 9

printf "test 18k: write 0xaa and read it back			\n"
msc -t 18 -o $OUTPUT -s 65536 -c $COUNT -p 10

printf "test 18l: write 0xbb and read it back			\n"
msc -t 18 -o $OUTPUT -s 65536 -c $COUNT -p 11

printf "test 18m: write 0xcc and read it back			\n"
msc -t 18 -o $OUTPUT -s 65536 -c $COUNT -p 12

printf "test 18n: write 0xdd and read it back			\n"
msc -t 18 -o $OUTPUT -s 65536 -c $COUNT -p 13

printf "test 18o: write 0xee and read it back			\n"
msc -t 18 -o $OUTPUT -s 65536 -c $COUNT -p 14

printf "test 18p: write 0xff and read it back			\n"
msc -t 18 -o $OUTPUT -s 65536 -c $COUNT -p 15

printf "Test suite ended: $(date)\n"

