#!/bin/sh

##
# SPDX-License-Identifier: GPL-3.0
# Copyright (C) 2009-2016 Felipe Balbi <felipe.balbi@linux.intel.com>
##

OUTPUT=""
COUNT=1024

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

RESULT=0

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

do_test() {
  test=$1
  size=$2

  msc -n -o $OUTPUT -c $COUNT $* 1> /dev/null 2> /dev/null
  if [ $? -ne 0 ]; then
    RESULT=$(($RESULT + 1))
    echo "${RED}FAIL${NC}"
  else
    echo "${GREEN}OK${NC}"
  fi
}

# Simple read and write
printf "test 0a: simple 4k read/write				"
do_test -t 0 -s 4k

printf "test 0b: simple 8k read/write				"
do_test -t 0 -s 8k

printf "test 0c: simple 16k read/write				"
do_test -t 0 -s 16k

printf "test 0d: simple 32k read/write				"
do_test -t 0 -s 32k

printf "test 0e: simple 64k read/write				"
do_test -t 0 -s 64k

printf "test 0f: simple 128k read/write				"
do_test -t 0 -s 128k

printf "test 0g: simple 256k read/write				"
do_test -t 0 -s 256k

printf "test 0h: simple 512k read/write				"
do_test -t 0 -s 512k

printf "test 0i: simple 1M read/write				"
do_test -t 0 -s 1M

printf "test 0j: simple 2M read/write				"
do_test -t 0 -s 2M

printf "test 0k: simple 4M read/write				"
do_test -t 0 -s 4M

# 1-sector read/write
printf "test 1: simple 1-sector read/write			"
do_test -t 1 -s 64k

# 8-sectors read/write
printf "test 2: simple 8-sectors read/write			"
do_test -t 2 -s 64k

# 32-sectors read/write
printf "test 3: simple 32-sectors read/write			"
do_test -t 3 -s 64k

# 64-sectors read/write
printf "test 4: simple 64-sectors read/write			"
do_test -t 4 -s 64k

# SG 2-sectors read/write
printf "test 5a: scatter/gather for 2-sectors buflen 4k		"
do_test -t 5 -s 4k

printf "test 5b: scatter/gather for 2-sectors buflen 8k		"
do_test -t 5 -s 8192

printf "test 5c: scatter/gather for 2-sectors buflen 16k	"
do_test -t 5 -s 16384

printf "test 5d: scatter/gather for 2-sectors buflen 32k	"
do_test -t 5 -s 32768

printf "test 5e: scatter/gather for 2-sectors buflen 64k	"
do_test -t 5 -s 64k

printf "test 5f: scatter/gather for 2-sectors buflen 128k	"
do_test -t 5 -s 131072

printf "test 5g: scatter/gather for 2-sectors buflen 256k	"
do_test -t 5 -s 262144

printf "test 5h: scatter/gather for 2-sectors buflen 512k	"
do_test -t 5 -s 524288

printf "test 5i: scatter/gather for 2-sectors buflen 1M		"
do_test -t 5 -s 1048576

# SG 8-sectors read/write
printf "test 6a: scatter/gather for 8-sectors buflen 4k		"
do_test -t 6 -s 4k

printf "test 6b: scatter/gather for 8-sectors buflen 8k		"
do_test -t 6 -s 8k

printf "test 6c: scatter/gather for 8-sectors buflen 16k	"
do_test -t 6 -s 16k

printf "test 6d: scatter/gather for 8-sectors buflen 32k	"
do_test -t 6 -s 32k

printf "test 6e: scatter/gather for 8-sectors buflen 64k	"
do_test -t 6 -s 64k

printf "test 6f: scatter/gather for 8-sectors buflen 128k	"
do_test -t 6 -s 128k

printf "test 6g: scatter/gather for 8-sectors buflen 256k	"
do_test -t 6 -s 256k

printf "test 6h: scatter/gather for 8-sectors buflen 512k	"
do_test -t 6 -s 512k

printf "test 6i: scatter/gather for 8-sectors buflen 1M		"
do_test -t 6 -s 1M

# SG 32-sectors read/write
printf "test 7a: scatter/gather for 32-sectors buflen 16k	"
do_test -t 7 -s 16k

printf "test 7b: scatter/gather for 32-sectors buflen 32k	"
do_test -t 7 -s 32k

printf "test 7c: scatter/gather for 32-sectors buflen 64k	"
do_test -t 7 -s 64k

# SG 64-sectors read/write
printf "test 8a: scatter/gather for 64-sectors buflen 32k	"
do_test -t 8 -s 32k

printf "test 8b: scatter/gather for 64-sectors buflen 64k	"
do_test -t 8 -s 64k

printf "test 8c: scatter/gather for 64-sectors buflen 128k	"
do_test -t 8 -s 128k

printf "test 8d: scatter/gather for 64-sectors buflen 256k	"
do_test -t 8 -s 256k

printf "test 8e: scatter/gather for 64-sectors buflen 512k	"
do_test -t 8 -s 512k

printf "test 8f: scatter/gather for 64-sectors buflen 1M	"
do_test -t 8 -s 1M

# SG 128-sectors read/write
printf "test 9: scatter/gather for 128-sectors buflen 64k	"
do_test -t 9 -s 64k

# Read past the last sector
printf "test 10: read over the end of the block device		"
do_test -t 10 -s 64k

# Lseek past the last sector
printf "test 11: lseek past the end of the block device		"
do_test -t 11 -s 64k

# Write past the last sector
printf "test 12: write over the end of the block device		"
do_test -t 12 -s 64k

# write 1 sg, read in 8 random size sgs
printf "test 13: write 1 sg, read 8 random size sgs		"
do_test -t 13 -s 64k

# write 8 random size sgs, read 1 sg
printf "test 14: write 8 random size sgs, read 1 sg		"
do_test -t 14 -s 64k

# write and read 8 random size sgs
printf "test 15: write and read 8 random size sgs		"
do_test -t 15 -s 64k

# write known patterns and read it back
printf "test 18a: write 0x00 and read it back			"
do_test -t 18 -s 64k -p 0

printf "test 18b: write 0x11 and read it back			"
do_test -t 18 -s 64k -p 1

printf "test 18c: write 0x22 and read it back			"
do_test -t 18 -s 64k -p 2

printf "test 18d: write 0x33 and read it back			"
do_test -t 18 -s 64k -p 3

printf "test 18e: write 0x44 and read it back			"
do_test -t 18 -s 64k -p 4

printf "test 18f: write 0x55 and read it back			"
do_test -t 18 -s 64k -p 5

printf "test 18g: write 0x66 and read it back			"
do_test -t 18 -s 64k -p 6

printf "test 18h: write 0x77 and read it back			"
do_test -t 18 -s 64k -p 7

printf "test 18i: write 0x88 and read it back			"
do_test -t 18 -s 64k -p 8

printf "test 18j: write 0x99 and read it back			"
do_test -t 18 -s 64k -p 9

printf "test 18k: write 0xaa and read it back			"
do_test -t 18 -s 64k -p 10

printf "test 18l: write 0xbb and read it back			"
do_test -t 18 -s 64k -p 11

printf "test 18m: write 0xcc and read it back			"
do_test -t 18 -s 64k -p 12

printf "test 18n: write 0xdd and read it back			"
do_test -t 18 -s 64k -p 13

printf "test 18o: write 0xee and read it back			"
do_test -t 18 -s 64k -p 14

printf "test 18p: write 0xff and read it back			"
do_test -t 18 -s 64k -p 15

exit $RESULT
