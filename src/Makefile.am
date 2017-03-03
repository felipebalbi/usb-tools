dist_bin_SCRIPTS = msc.sh scptest.sh test.sh usbpwrtest

bin_PROGRAMS =		\
	acmc		\
	acmd		\
	cleware		\
	companion-desc	\
	control		\
	device-reset	\
	msc		\
	serialc		\
	seriald		\
	switchbox	\
	testmode	\
	testusb		\
	uda		\
	uda-client

GENERIC_CFLAGS = -O2 -Wall -Wextra

# These have no extra requirement
acmc_SOURCES = acmc.c
acmc_CFLAGS = $(GENERIC_CFLAGS)
acmc_LDFLAGS =

acmd_SOURCES = acmd.c
acmd_CFLAGS = $(GENERIC_CFLAGS)
acmd_LDFLAGS =

serialc_SOURCES = serialc.c
serialc_CFLAGS = $(GENERIC_CFLAGS)
serialc_LDFLAGS =

seriald_SOURCES = seriald.c
seriald_CFLAGS = $(GENERIC_CFLAGS)
seriald_LDFLAGS =

switchbox_SOURCES = switchbox.c
switchbox_CFLAGS = $(GENERIC_CFLAGS)
switchbox_LDFLAGS =

# This needs HIDAPI
cleware_SOURCES = cleware.c
cleware_CFLAGS = $(GENERIC_CFLAGS) $(hid_CFLAGS)
cleware_LDFLAGS = $(hid_LIBS)

# These need libusb-1.0
companion_desc_SOURCES = companion-desc.c
companion_desc_CFLAGS = $(GENERIC_CFLAGS) $(libusb_CFLAGS)
companion_desc_LDFLAGS = $(libusb_LIBS)

control_SOURCES = control.c
control_CFLAGS = $(GENERIC_CFLAGS) $(libusb_CFLAGS)
control_LDFLAGS = $(libusb_LIBS)

device_reset_SOURCES = device-reset.c
device_reset_CFLAGS = $(GENERIC_CFLAGS) $(libusb_CFLAGS)
device_reset_LDFLAGS = $(libusb_LIBS)

testmode_SOURCES = testmode.c
testmode_CFLAGS = $(GENERIC_CFLAGS) $(libusb_CFLAGS)
testmode_LDFLAGS = $(libusb_LIBS)

uda_SOURCES = uda.c
uda_CFLAGS = $(GENERIC_CFLAGS) $(libusb_CFLAGS)
uda_LDFLAGS = $(libusb_LIBS)

# This needs libssl
msc_SOURCES = msc.c
msc_CFLAGS = $(GENERIC_CFLAGS) $(ssl_CFLAGS)
msc_LDFLAGS = $(ssl_LIBS)

# These need libpthread
testusb_SOURCES = testusb.c
testusb_CFLAGS = $(GENERIC_CFLAGS) $(PTHREAD_CFLAGS)
testusb_LDFLAGS = $(PTHREAD_LIBS)

uda_client_SOURCES = uda-client.c
uda_client_CFLAGS = $(GENERIC_CFLAGS) $(PTHREAD_CFLAGS)
uda_client_LDFLAGS = $(PTHREAD_LIBS)


