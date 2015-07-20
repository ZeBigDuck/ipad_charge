#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libusb-1.0/libusb.h>

#define VERSION "1.1"

#define CTRL_OUT	(LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT)
#define VENDOR_HTC		0x0bb4
#define PRODUCT_M8_MTP		0x0f25
#define PRODUCT_M8_MTP_ADB		0x061a
#define PRODUCT_M8_MTP_UMS	0x0fb5
#define PRODUCT_M8_MTP_ADB_UMS		0x0fb4
#define PRODUCT_GENERIC_2	0x2008
#define PRODUCT_WINDOWS_8S	    0xf0ca
#define PRODUCT_M8_VERIZON    0x07cb
#define PRODUCT_M8       0x07ca

#define PRODUCT_ONE 0x07ae
#define PRODUCT_M8_GOOGLE 0x060b
#define PRODUCT_WINDOWS_8X1 0x0ba1
#define PRODUCT_WINDOWS_8X2 0x0ba2
#define PRODUCT_GENERIC_1 0x0c02
#define PRODUCT_ONE_S1 0x0cec
#define PRODUCT_ONE_S2 0x0df8
#define PRODUCT_ONE_S3 0x0df9

int set_charging_mode(libusb_device *dev, bool enable) {
	int ret;
	struct libusb_device_handle *dev_handle;

	if ((ret = libusb_open(dev, &dev_handle)) < 0) {
		fprintf(stderr, "htc_charge: unable to open device: error %d\n", ret);
		fprintf(stderr, "htc_charge: %s\n", libusb_strerror(ret));
		return ret;
	}

	if ((ret = libusb_claim_interface(dev_handle, 0)) < 0) {
		fprintf(stderr, "htc_charge: unable to claim interface: error %d\n", ret);
		fprintf(stderr, "htc_charge: %s\n", libusb_strerror(ret));
		goto out_close;
	}

	// the 3rd and 4th numbers are the extra current in mA that the Apple device may draw in suspend state.
	// Originally, the 4th was 0x6400, or 25600mA. I believe this was a bug and they meant 0x640, or 1600 mA which would be the max
	// for the MFi spec. Also the typical values for the 3nd listed in the MFi spec are 0, 100, 500 so I chose 500 for that.
	// And changed it to decimal to be clearer.
	if ((ret = libusb_control_transfer(dev_handle, CTRL_OUT, 0x40, 500, enable ? 2000 : 0, NULL, 0, 2000)) < 0) {
		fprintf(stderr, "htc_charge: unable to send command: error %d\n", ret);
		fprintf(stderr, "htc_charge: %s\n", libusb_strerror(ret));
		goto out_release;
	}
	
	ret = 0;

out_release:
	libusb_release_interface(dev_handle, 0);
out_close:
	libusb_close(dev_handle);

	return ret;
}

void help(char *progname) {
	printf("Usage: %s [OPTION]\n", progname);
	printf("HTC USB charging control utility\n\n");
	printf("Available OPTIONs:\n");
	printf("  -0, --off\t\t\tdisable charging instead of enabling it\n");
	printf("  -h, --help\t\t\tdisplay this help and exit\n");
	printf("  -V, --version\t\t\tdisplay version information and exit\n");
	printf("\nExamples:\n");
	printf("  htc_charge\t\t\t\tenable charging on all connected HTC devices\n");
	printf("  BUSNUM=004 DEVNUM=014 htc_charge -off\tdisable charging on HTC device connected on bus 4, device 14\n");
}

void version() {
	printf("htc_charge v%s - HTC USB charging control utility\n", VERSION);
	printf("Copyright (c) 2010 Ondrej Zary - http://www.rainbow-software.org\n");
	printf("Modifications: Copyright (c) 2015 Micah Waddoups - http://www.makesharp.net\n");
	printf("License: GLPv2\n");
}

int main(int argc, char *argv[]) {
	int ret, devnum = 0, busnum = 0;
	bool enable = 1;

	while (1) {
                struct option long_options[] = {
                        { .name = "off",	.has_arg = 0, .val = '0' },
                        { .name = "help",	.has_arg = 0, .val = 'h' },
                        { .name = "version",	.has_arg = 0, .val = 'V' },
                        { .name = NULL },
                };
                int opt = getopt_long(argc, argv, "0hV", long_options, NULL);
                if (opt < 0)
                        break;
                switch (opt) {
                case '0':
                        enable = 0;
                        break;
                case 'h':
                        help(argv[0]);
                        exit(0);
                case 'V':
                        version();
                        exit(0);
                default:
                        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                        exit(100);
                }
        }

	if (getenv("BUSNUM") && getenv("DEVNUM")) {
		busnum = atoi(getenv("BUSNUM"));
		devnum = atoi(getenv("DEVNUM"));
	}

	if (libusb_init(NULL) < 0) {
		fprintf(stderr, "htc_charge: failed to initialise libusb\n");
		exit(1);
	}

	libusb_device **devs;
	if (libusb_get_device_list(NULL, &devs) < 0) {
		fprintf(stderr, "htc_charge: unable to enumerate USB devices\n");
		ret = 2;
		goto out_exit;
	}

	libusb_device *dev;
	int i = 0, count = 0;
	/* if BUSNUM and DEVNUM were specified (by udev), find device by address */
	if (busnum && devnum) {
		while ((dev = devs[i++]) != NULL) {
			if (libusb_get_bus_number(dev) == busnum &&
			    libusb_get_device_address(dev) == devnum) {
			    	if (set_charging_mode(dev, enable) < 0)
			    		fprintf(stderr, "htc_charge: error setting charge mode\n");
				else
					count++;
				break;
			}
		}
	/* otherwise apply to all devices */
	} else {
		while ((dev = devs[i++]) != NULL) {
			struct libusb_device_descriptor desc;
			if ((ret = libusb_get_device_descriptor(dev, &desc)) < 0) {
				fprintf(stderr, "htc_charge: failed to get device descriptor: error %d\n", ret);
				fprintf(stderr, "htc_charge: %s\n", libusb_strerror(ret));
				continue;
			}
			if (desc.idVendor == VENDOR_HTC && (desc.idProduct == PRODUCT_IPAD1
					|| desc.idProduct == PRODUCT_IPAD2
					|| desc.idProduct == PRODUCT_IPAD2_3G
					|| desc.idProduct == PRODUCT_IPAD2_4
					|| desc.idProduct == PRODUCT_IPAD2_3GV
					|| desc.idProduct == PRODUCT_IPAD3
					|| desc.idProduct == PRODUCT_IPAD3_4G
					|| desc.idProduct == PRODUCT_IPOD_TOUCH_2G
					|| desc.idProduct == PRODUCT_IPHONE_3GS
					|| desc.idProduct == PRODUCT_IPHONE_4_GSM
					|| desc.idProduct == PRODUCT_IPOD_TOUCH_3G
					|| desc.idProduct == PRODUCT_IPHONE_4_CDMA
					|| desc.idProduct == PRODUCT_IPOD_TOUCH_4G
					|| desc.idProduct == PRODUCT_IPHONE_4S
					|| desc.idProduct == PRODUCT_IPHONE_5
					|| desc.idProduct == PRODUCT_IPAD4)) {

				if (set_charging_mode(dev, enable) < 0)
					fprintf(stderr, "htc_charge: error setting charge mode\n");
				else
					count++;
			}
		}
	}

#define CTRL_OUT	(LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT)
#define VENDOR_HTC		0x0bb4
#define PRODUCT_M8_MTP		0x0f25
#define PRODUCT_M8_MTP_ADB		0x061a
#define PRODUCT_M8_MTP_UMS	0x0fb5
#define PRODUCT_M8_MTP_ADB_UMS		0x0fb4
#define PRODUCT_GENERIC_2	0x2008
#define PRODUCT_WINDOWS_8S	    0xf0ca
#define PRODUCT_M8_VERIZON    0x07cb
#define PRODUCT_M8       0x07ca

#define PRODUCT_ONE 0x07ae
#define PRODUCT_M8_GOOGLE 0x060b
#define PRODUCT_WINDOWS_8X1 0x0ba1
#define PRODUCT_WINDOWS_8X2 0x0ba2
#define PRODUCT_GENERIC_1 0x0c02
#define PRODUCT_ONE_S1 0x0cec
#define PRODUCT_ONE_S2 0x0df8
#define PRODUCT_ONE_S3 0x0df9

	if (count < 1) {
		fprintf(stderr, "htc_charge: no such device or an error occured\n");
		ret = 3;
	} else
		ret = 0;

	libusb_free_device_list(devs, 1);
out_exit:
	libusb_exit(NULL);

	return ret;
}
