# USB Tools

[![Build Status](https://travis-ci.org/felipebalbi/usb-tools.svg?branch=master)](https://travis-ci.org/felipebalbi/usb-tools)

## Introduction

This project aims at creating standalone and simplistic tools for
testing different details of the USB support on Linux.

Not all tools are going to be useful for everybody, but at least testusb
should be executed when writing a new USB Host Controller driver or a
new USB Peripheral Controller Driver.

The following sections will explain in a little more detail how to use
these tools.

## Compilation

Before compiling this project, you're going to need a few libraries. For
debian-based distros, the following command should do the right thing:

```
$ sudo apt-get update
$ sudo apt install libusb-1.0-0-dev libssl-dev libhidapi-dev
```

After these are installed, the usual autotools-based setup should be followed:

```
$ ./autogen.sh
$ ./configure --prefix=/usr
$ make
$ sudo make install
```

We also support cross compilation. Again, for debian-based distros, the steps
below should be enough (assuming armhf):

```
$ sudo dpkg --add-architecture armhf
$ sudo apt-get update
$ sudo apt install libusb-1.0-0-dev:armhf libssl-dev:armhf \
  libhidapi-dev:armhf gcc-arm-linux-gnueabihf
$ ./autogen.sh
$ ./configure --prefix=/usr --host=arm-linux-gnueabihf
$ make
$ sudo make install
```

Note that you might want to change --prefix to install resulting binaries to an
external USB stick or MMC card.

## Mass Storage Class (`msc.c` & `msc.sh`)

In reality, `msc.c` and `msc.sh` pair can test any Linux block device,
but the tool was written, originally, to test the g_mass_storage gadget.

To run this tool all you have to do is load `g_mass_storage` on the
peripheral side (or connect a USB mass storage to your host PC) and run
the test suite:

```
$ msc.sh -o /dev/foobar
```

Where /dev/foobar needs to be replaced with the actual block device that
should be used for testing. Note that the tool makes no effort to
prevent you from destroying your root filesystem and assumes that you
are, indeed, using the correct block device. Make sure you know what
you're doing.

You can also call `msc.c` directly (after compiling it to `msc` of
course) in order to isolate problems with particular test cases or, if
you wish, measure throughput with different block sizes.

Personally, I like to measure throughput when I make changes that could
impact `g_mass_storage` somehow. So, what I generally do is the
following:

```
# modprobe dwc3-pci
# modprobe dwc3
# dd if=/dev/zero of=/dev/shm/file bs=1M count=512
# modprobe g_mass_storage file=/dev/shm/file
```

After that, connecting a USB cable will cause g_mass_storage to be enumerated by
a host machine. At that point we can run `msc` with different block sizes to get
a feeling of throughput trend:

```
$ for size in 1k 2k 4k 8k 16k 32k 64k 128k 256k 512k 1M 2M 4M; do \
	msc -t 0 -s $size -c 1024 -o /dev/foobar -n;              \
  done
```

If you're just looking for a _stable_ testbench, just run msc.sh and you'll get
a report for each test. Like so:

```
$ msc.sh -o /dev/foobar
test 0a: simple 4k read/write				OK
test 0b: simple 8k read/write				OK
test 0c: simple 16k read/write				OK
test 0d: simple 32k read/write				OK
test 0e: simple 64k read/write				OK
test 0f: simple 128k read/write				OK
test 0g: simple 256k read/write				OK
test 0h: simple 512k read/write				OK
test 0i: simple 1M read/write				OK
test 0j: simple 2M read/write				OK
test 0k: simple 4M read/write				OK
test 1: simple 1-sector read/write			OK
test 2: simple 8-sectors read/write			OK
test 3: simple 32-sectors read/write			OK
test 4: simple 64-sectors read/write			OK
test 5a: scatter/gather for 2-sectors buflen 4k		OK
test 5b: scatter/gather for 2-sectors buflen 8k		OK
test 5c: scatter/gather for 2-sectors buflen 16k	OK
test 5d: scatter/gather for 2-sectors buflen 32k	OK
test 5e: scatter/gather for 2-sectors buflen 64k	OK
test 5f: scatter/gather for 2-sectors buflen 128k	OK
test 5g: scatter/gather for 2-sectors buflen 256k	OK
test 5h: scatter/gather for 2-sectors buflen 512k	OK
test 5i: scatter/gather for 2-sectors buflen 1M		OK
test 6a: scatter/gather for 8-sectors buflen 4k		OK
test 6b: scatter/gather for 8-sectors buflen 8k		OK
test 6c: scatter/gather for 8-sectors buflen 16k	OK
test 6d: scatter/gather for 8-sectors buflen 32k	OK
test 6e: scatter/gather for 8-sectors buflen 64k	OK
test 6f: scatter/gather for 8-sectors buflen 128k	OK
test 6g: scatter/gather for 8-sectors buflen 256k	OK
test 6h: scatter/gather for 8-sectors buflen 512k	OK
test 6i: scatter/gather for 8-sectors buflen 1M		OK
test 7a: scatter/gather for 32-sectors buflen 16k	OK
test 7b: scatter/gather for 32-sectors buflen 32k	OK
test 7c: scatter/gather for 32-sectors buflen 64k	OK
test 8a: scatter/gather for 64-sectors buflen 32k	OK
test 8b: scatter/gather for 64-sectors buflen 64k	OK
test 8c: scatter/gather for 64-sectors buflen 128k	OK
test 8d: scatter/gather for 64-sectors buflen 256k	OK
test 8e: scatter/gather for 64-sectors buflen 512k	OK
test 8f: scatter/gather for 64-sectors buflen 1M	OK
test 9: scatter/gather for 128-sectors buflen 64k	OK
test 10: read over the end of the block device		OK
test 11: lseek past the end of the block device		OK
test 12: write over the end of the block device		OK
test 13: write 1 sg, read 8 random size sgs		OK
test 14: write 8 random size sgs, read 1 sg		OK
test 15: write and read 8 random size sgs		OK
test 18a: write 0x00 and read it back			OK
test 18b: write 0x11 and read it back			OK
test 18c: write 0x22 and read it back			OK
test 18d: write 0x33 and read it back			OK
test 18e: write 0x44 and read it back			OK
test 18f: write 0x55 and read it back			OK
test 18g: write 0x66 and read it back			OK
test 18h: write 0x77 and read it back			OK
test 18i: write 0x88 and read it back			OK
test 18j: write 0x99 and read it back			OK
test 18k: write 0xaa and read it back			OK
test 18l: write 0xbb and read it back			OK
test 18m: write 0xcc and read it back			OK
test 18n: write 0xdd and read it back			OK
test 18o: write 0xee and read it back			OK
test 18p: write 0xff and read it back			OK
```

## `testusb` & `test.sh`

This tool helps exercising both host and peripheral stacks. The idea is the
following:

If you want to test the host stack, then use a USB peripheral controller
you can assume to be working, like `dwc3` (drivers/usb/dwc3/) available
on several platforms including Intel Edison, and load `g_zero.ko`:

```
# modprobe g_zero
```

Then connect the cable to the host and run `test.sh`:

```
# test.sh
```

If, on the other hand, you're trying to exercise a new USB Peripheral
controller, we need a Host controller which we can assume to be working,
like EHCI or xHCI (see Notes section below), and load `g_zero.ko`:

```
# modprobe g_zero
```

Then connect the cable to the host and run `test.sh`:

```
# test.sh
```

You'll notice that `test.sh` will run an infinite loop and that's by
design. To make sure your implementation of a host or peripheral
controller driver is working fine, `test.sh` needs to be able to run for
several weeks without any failures.

## `device-reset`

This tool tests that a USB peripheral can handle multiple resets in a
row. It's used like so:

First, you needt know the USB device's VendorID/ProductID pair. The
easiest way to get that is with `lsusb`:

```
$ lsusb
Bus 002 Device 001: ID 1d6b:0003 Linux Foundation 3.0 root hub
Bus 001 Device 008: ID abcd:abcd ABCD test device
Bus 001 Device 001: ID 1d6b:0002 Linux Foundation 2.0 root hub
```

Let's assume we want to test our ABCD test device, so we run:

```
# device-reset -D abcd:abcd -c 5000
```

This will run 5000 USB resets in a tight loop. Test passes if the USB
device re-enumerates fine after each and every iteration.

## `testmode`

This is used to put a USB device in USB 2.0 Test Modes as described by
the USB 2.0 specification. Valid test modes are:

1. test_j
2. test_k
3. test_se0_nak
4. test_packet
5. test_force_hs
6. test_force_fs
7. bad_descriptor

Example usage follows:

```
# testmode -D abcd:abcd -t test_j
```

## `acmc` & `acmd` and `serialc` & `seriald`

TODO

## `companion-desc`

TODO

## `control`

TODO

## `pingtest` & `scptest.sh`

TODO
