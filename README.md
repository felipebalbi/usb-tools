# USB Tools

## Introduction

This project aims at creating standalone and simplistic tools for
testing different details of the USB support on Linux.

Not all tools are going to be useful for everybody, but at least testusb
should be executed when writing a new USB Host Controller driver or a
new USB Peripheral Controller Driver.

The following sections will explain in a little more detail how to use
these tools.

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
# dd if`/dev/zero of`/dev/shm/file bs`1M count`512
# modprobe g_mass_storage file`/dev/shm/file
```

After that, I connect USB cable and get the thing enumerated on a host
machine. At that point I can run `msc` with different block sizes to get
a feeling of throughput trend:

```
$ for size in 1k 2k 4k 8k 16k 32k 64k 128k 256k 512k 1M 2M 4M; do \
	msc -t 0 -s $size -c 1024 -o /dev/foobar -n;              \
	done
```

## `testusb` & `test.sh`

This tool helps exercising both host and peripheral stacks. The idea is
the following:

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

## Notes

### xHCI

At the time of this writing, Linux Kernel v4.7 or before is known to
have a buggy xHCI ring implementation which doesn't guarantee some
alignment conditions and, because of that, some tests in testusb hang
with xHCI. Situation is supposed to change in coming Linux Kernel
releases.

Felipe Balbi
