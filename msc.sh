#!/bin/sh

OUTPUT=""
COUNT=5120

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
			shift;;
		--)
			break;;
		*)
			echo "Interal error!";
			exit 1;;
	esac
done

while true;
do
	echo Starting test suite: $(date)

	# Simple read and write
	echo "test 0a: simple 4k read/write"
	./msc -t 0 -o $OUTPUT -s 4096 -c $COUNT

	echo "test 0b: simple 8k read/write"
	./msc -t 0 -o $OUTPUT -s 8192 -c $COUNT

	echo "test 0c: simple 16k read/write"
	./msc -t 0 -o $OUTPUT -s 16384 -c $COUNT

	echo "test 0d: simple 32k read/write"
	./msc -t 0 -o $OUTPUT -s 32768 -c $COUNT

	echo "test 0e: simple 64k read/write"
	./msc -t 0 -o $OUTPUT -s 65536 -c $COUNT

	# 1-sector read/write
	echo "test 1: simple 1-sector read/write"
	./msc -t 1 -o $OUTPUT -s 65536 -c $COUNT

	# 8-sectors read/write
	echo "test 2: simple 8-sectors read/write"
	./msc -t 2 -o $OUTPUT -s 65536 -c $COUNT

	# 32-sectors read/write
	echo "test 3: simple 32-sectors read/write"
	./msc -t 3 -o $OUTPUT -s 65536 -c $COUNT

	# 64-sectors read/write
	echo "test 4: simple 64-sectors read/write"
	./msc -t 4 -o $OUTPUT -s 65536 -c $COUNT

	# SG 2-sectors read/write
	echo "test 5a: scatter/gather for 2-sectors buflen 4k"
	./msc -t 5 -o $OUTPUT -s 4096 -c $COUNT

	echo "test 5b: scatter/gather for 2-sectors buflen 8k"
	./msc -t 5 -o $OUTPUT -s 8192 -c $COUNT

	echo "test 5c: scatter/gather for 2-sectors buflen 16k"
	./msc -t 5 -o $OUTPUT -s 16384 -c $COUNT

	echo "test 5d: scatter/gather for 2-sectors buflen 32k"
	./msc -t 5 -o $OUTPUT -s 32768 -c $COUNT

	echo "test 5e: scatter/gather for 2-sectors buflen 64k"
	./msc -t 5 -o $OUTPUT -s 65536 -c $COUNT

	# SG 8-sectors read/write
	echo "test 6a: scatter/gather for 8-sectors buflen 4k"
	./msc -t 6 -o $OUTPUT -s 4096 -c $COUNT

	echo "test 6b: scatter/gather for 8-sectors buflen 8k"
	./msc -t 6 -o $OUTPUT -s 8192 -c $COUNT

	echo "test 6c: scatter/gather for 8-sectors buflen 16k"
	./msc -t 6 -o $OUTPUT -s 16384 -c $COUNT

	echo "test 6d: scatter/gather for 8-sectors buflen 32k"
	./msc -t 6 -o $OUTPUT -s 32768 -c $COUNT

	echo "test 6e: scatter/gather for 8-sectors buflen 64k"
	./msc -t 6 -o $OUTPUT -s 65536 -c $COUNT

	# SG 32-sectors read/write
	echo "test 7a: scatter/gather for 32-sectors buflen 16k"
	./msc -t 7 -o $OUTPUT -s 16384 -c $COUNT

	echo "test 7b: scatter/gather for 32-sectors buflen 32k"
	./msc -t 7 -o $OUTPUT -s 32768 -c $COUNT

	echo "test 7c: scatter/gather for 32-sectors buflen 64k"
	./msc -t 7 -o $OUTPUT -s 65536 -c $COUNT

	# SG 64-sectors read/write
	echo "test 8a: scatter/gather for 64-sectors buflen 32k"
	./msc -t 8 -o $OUTPUT -s 32768 -c $COUNT

	echo "test 8b: scatter/gather for 64-sectors buflen 64k"
	./msc -t 8 -o $OUTPUT -s 65536 -c $COUNT

	# SG 128-sectors read/write
	echo "test 9: scatter/gather for 128-sectors buflen 64k"
	./msc -t 9 -o $OUTPUT -s 65536 -c $COUNT

	# Read past the last sector
	echo "test 10: read over the end of the block device"
	./msc -t 10 -o $OUTPUT -s 65536 -c $COUNT

	# Lseek past the last sector
	echo "test 11: lseek past the end of the block device"
	./msc -t 11 -o $OUTPUT -s 65536 -c $COUNT

	# Write past the last sector
	echo "test 12: write over the end of the block device"
	./msc -t 12 -o $OUTPUT -s 65536 -c $COUNT

	# write 1 sg, read in 8 random size sgs
	echo "test 13: write 1 sg, read 8 random size sgs"
	./msc -t 13 -o $OUTPUT -s 65536 -c $COUNT

	# write 8 random size sgs, read 1 sg
	echo "test 14: write 8 random size sgs, read 1 sg"
	./msc -t 14 -o $OUTPUT -s 65536 -c $COUNT

	# write and read 8 random size sgs
	echo "test 15: write and read 8 random size sgs"
	./msc -t 15 -o $OUTPUT -s 65536 -c $COUNT

	echo "Test suite ended: $(date)
	"
done

