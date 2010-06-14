##
# Makefile - make support for usb-tools
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

CROSS_COMPILE = arm-linux-
CC = gcc
CFLAGS = -Wall -O3 -g -finline-functions -fno-strict-aliasing \
	 -D_GNU_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
LIBUSB= -lusb-1.0
LIBPTHREAD= -lpthread

PROGRAMS = cleware msc serialc

#
# Pretty print
#
V             = @
Q             = $(V:1=)
QUIET_CC      = $(Q:@=@echo    '     CC       '$@;)
QUIET_CLEAN   = $(Q:@=@echo    '     CLEAN    '$@;)

all: cleware msc serialc seriald testusb acmc acmd testmode switchbox

testmode:
	$(QUIET_CC)$(CC) $(CFLAGS) $(LIBUSB) -o $@ $@.c

cleware:
	$(QUIET_CC)$(CC) $(CFLAGS) $(LIBUSB) -o $@ $@.c

serialc:
	$(QUIET_CC)$(CC) $(CFLAGS) -o $@ $@.c

acmc:
	$(QUIET_CC)$(CC) $(CFLAGS) -o $@ $@.c

acmd:
	$(QUIET_CC)$(CROSS_COMPILE)$(CC) $(CFLAGS) -o $@ $@.c

msc:
	$(QUIET_CC)$(CC) $(CFLAGS) -o $@ $@.c

seriald:
	$(QUIET_CC)$(CROSS_COMPILE)$(CC) $(CFLAGS) -o $@ $@.c

testusb:
	$(QUIET_CC)$(CC) $(CFLAGS) $(LIBPTHREAD) -o $@ $@.c

switchbox:
	$(QUIET_CC)$(CC) $(CFLAGS) -o $@ $@.c

clean:
	$(QUIET_CLEAN) rm -f cleware msc serialc seriald testusb acmc acmd testmode switchbox

