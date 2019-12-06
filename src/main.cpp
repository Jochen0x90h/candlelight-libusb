#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libusb.h>
#include <iostream>

// stm32f103xx data sheet: https://www.st.com/resource/en/datasheet/stm32f042c6.pdf
// stm32f103xx reference manual: https://www.st.com/content/ccc/resource/technical/document/reference_manual/c2/f8/8a/f2/18/e6/43/96/DM00031936.pdf/files/DM00031936.pdf/jcr:content/translations/en.DM00031936.pdf
//   can bit timing: section 29.7.7, page 829


// https://github.com/libusb/libusb/blob/master/examples/listdevs.c
static void printDevices(libusb_device **devs)
{
	libusb_device *dev;
	int i = 0, j = 0;
	uint8_t path[8]; 

	// iterate over devices
	while ((dev = devs[i++]) != NULL) {
		struct libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(dev, &desc);
		if (r < 0) {
			fprintf(stderr, "failed to get device descriptor");
			return;
		}

		printf("%04x:%04x (bus %d, device %d)",
			desc.idVendor, desc.idProduct,
			libusb_get_bus_number(dev), libusb_get_device_address(dev));

		r = libusb_get_port_numbers(dev, path, sizeof(path));
		if (r > 0) {
			printf(" path: %d", path[0]);
			for (j = 1; j < r; j++)
				printf(".%d", path[j]);
		}
		printf("\n");
	}
}

// https://github.com/libusb/libusb/blob/master/examples/testlibusb.c
static int printDevice(libusb_device *dev, int level)
{
	libusb_device_descriptor desc;
	libusb_device_handle *handle = NULL;
	unsigned char string[256];
	int ret;

	ret = libusb_get_device_descriptor(dev, &desc);
	if (ret < 0) {
		fprintf(stderr, "failed to get device descriptor");
		return -1;
	}

	printf("%04x:%04x\n", desc.idVendor, desc.idProduct);
	printf("\tBus: %d\n", libusb_get_bus_number(dev));
	printf("\tDevice: %d\n", libusb_get_device_address(dev));
	ret = libusb_open(dev, &handle);
	if (LIBUSB_SUCCESS == ret) {
		printf("\tOpen\n");

		// manufacturer
		if (desc.iManufacturer) {
			ret = libusb_get_string_descriptor_ascii(handle, desc.iManufacturer, string, sizeof(string));
			if (ret > 0)
				printf("\t\tManufacturer: %s\n", string);
		}

		// product
		if (desc.iProduct) {
			ret = libusb_get_string_descriptor_ascii(handle, desc.iProduct, string, sizeof(string));
			if (ret > 0)
				printf("\t\tProduct: %s\n", string);
		}
		
	
		libusb_close(handle);
	} else {
		printf("\tOpen error: %d\n", ret);
	}

		// configurations
		for (int i = 0; i < desc.bNumConfigurations; i++) {
			libusb_config_descriptor *config;
			ret = libusb_get_config_descriptor(dev, i, &config);
			if (LIBUSB_SUCCESS == ret) {
				printf("\tConfiguration[%d]\n", i);
				printf("\t\tTotalLength:         %d\n", config->wTotalLength);
				printf("\t\tNumInterfaces:       %d\n", config->bNumInterfaces);
				printf("\t\tConfigurationValue:  %d\n", config->bConfigurationValue);
				//printf("\t\tConfiguration:       %d\n", config->iConfiguration); // same as i
				printf("\t\tAttributes:          %02xh\n", config->bmAttributes);
				printf("\t\tMaxPower:            %d\n", config->MaxPower);

			}
			
			// interfaces
			for (int j = 0; j < config->bNumInterfaces; j++) {
				libusb_interface const & interface = config->interface[j];
				printf("\t\tInterface[%d]\n", j);

				// alternate settings
				for (int k = 0; k < interface.num_altsetting; k++) {
					libusb_interface_descriptor const & descriptor = interface.altsetting[k];

					printf("\t\t\tAltSetting[%d]\n", k);
					printf("\t\t\t\tInterfaceNumber:   %d\n", descriptor.bInterfaceNumber);
					printf("\t\t\t\tAlternateSetting:  %d\n", descriptor.bAlternateSetting);
					printf("\t\t\t\tNumEndpoints:      %d\n", descriptor.bNumEndpoints);
					printf("\t\t\t\tInterfaceClass:    %d\n", descriptor.bInterfaceClass);
					printf("\t\t\t\tInterfaceSubClass: %d\n", descriptor.bInterfaceSubClass);
					printf("\t\t\t\tInterfaceProtocol: %d\n", descriptor.bInterfaceProtocol);
					//printf("\t\t\t\tInterface:         %d\n", descriptor.iInterface); // same as j

					// endpoints
					for (int l = 0; l < descriptor.bNumEndpoints; l++) {
						libusb_endpoint_descriptor const & endpoint = descriptor.endpoint[l];
						
						printf("\t\t\t\tEndpoint[%d]\n", l);
						printf("\t\t\t\t\tEndpointAddress: %02xh\n", endpoint.bEndpointAddress);
						printf("\t\t\t\t\tAttributes:      %02xh\n", endpoint.bmAttributes);
						printf("\t\t\t\t\tMaxPacketSize:   %d\n", endpoint.wMaxPacketSize);
						printf("\t\t\t\t\tInterval:        %d\n", endpoint.bInterval);
						printf("\t\t\t\t\tRefresh:         %d\n", endpoint.bRefresh);
						printf("\t\t\t\t\tSynchAddress:    %d\n", endpoint.bSynchAddress);
					}
				}
			
			}
			
			libusb_free_config_descriptor(config);
		}


	return 0;
}

/*
 * USB directions
 *
 * This bit flag is used in endpoint descriptors' bEndpointAddress field.
 * It's also one of three fields in control requests bRequestType.
 */
static const int USB_DIR_OUT = 0; // to device
static const int USB_DIR_IN = 0x80; // to host

/*
 * USB types, the second of three bRequestType fields
 */
static const int USB_TYPE_MASK = (0x03 << 5);
static const int USB_TYPE_STANDARD = (0x00 << 5);
static const int USB_TYPE_CLASS = (0x01 << 5);
static const int USB_TYPE_VENDOR = (0x02 << 5);
static const int USB_TYPE_RESERVED = (0x03 << 5);

/*
 * USB recipients, the third of three bRequestType fields
 */
static const int USB_RECIP_MASK = 0x1f;
static const int USB_RECIP_DEVICE = 0x00;
static const int USB_RECIP_INTERFACE = 0x01;
static const int USB_RECIP_ENDPOINT = 0x02;
static const int USB_RECIP_OTHER = 0x03;
/* From Wireless USB 1.0 */
static const int USB_RECIP_PORT = 0x04;
static const int USB_RECIP_RPIPE = 0x05;

struct UsbDeviceDescriptor {
	uint8_t  bLength;
	uint8_t  bDescriptorType;
	uint16_t bcdUSB;
	uint8_t  bDeviceClass;
	uint8_t  bDeviceSubClass;
	uint8_t  bDeviceProtocol;
	uint8_t  bMaxPacketSize0;
	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t bcdDevice;
	uint8_t  iManufacturer;
	uint8_t  iProduct;
	uint8_t  iSerialNumber;
	uint8_t  bNumConfigurations;
} __attribute__((packed));

enum gs_usb_breq {
	GS_USB_BREQ_HOST_FORMAT = 0,
	GS_USB_BREQ_BITTIMING,
	GS_USB_BREQ_MODE,
	GS_USB_BREQ_BERR,
	GS_USB_BREQ_BT_CONST,
	GS_USB_BREQ_DEVICE_CONFIG,
	GS_USB_BREQ_TIMESTAMP,
	GS_USB_BREQ_IDENTIFY,
};

// bit timing constraints
struct gs_device_bt_const {
	uint32_t feature;
	uint32_t fclk_can;
	uint32_t tseg1_min;
	uint32_t tseg1_max;
	uint32_t tseg2_min;
	uint32_t tseg2_max;
	uint32_t sjw_max;
	uint32_t brp_min;
	uint32_t brp_max;
	uint32_t brp_inc;
};

// bit timing
struct gs_device_bittiming {
	uint32_t prop_seg;
	uint32_t phase_seg1;
	uint32_t phase_seg2;
	uint32_t sjw;
	uint32_t brp;
};

// can mode
enum gs_can_mode {
	// reset a channel. turns it off
	GS_CAN_MODE_RESET = 0,
	// starts a channel
	GS_CAN_MODE_START
};

// can mode flags
static const int GS_CAN_MODE_NORMAL = 0;
static const int GS_CAN_MODE_LISTEN_ONLY = (1<<0);
static const int GS_CAN_MODE_LOOP_BACK = (1<<1);
static const int GS_CAN_MODE_TRIPLE_SAMPLE = (1<<2);
static const int GS_CAN_MODE_ONE_SHOT = (1<<3);
static const int GS_CAN_MODE_HW_TIMESTAMP = (1<<4);
static const int GS_CAN_MODE_PAD_PKTS_TO_MAX_PKT_SIZE = (1<<7);

// can mode and flags
struct gs_device_mode {
	uint32_t mode;
	uint32_t flags;
};

// host frame
static const int CAN_EFF_FLAG = 0x80000000U; // EFF/SFF, 29 or 11 bit address
static const int CAN_RTR_FLAG = 0x40000000U; // remote transmission request
static const int CAN_ERR_FLAG = 0x20000000U; // error message frame
struct gs_host_frame {
	uint32_t echo_id;
	uint32_t can_id; // address on can bus, with flags

	uint8_t can_dlc; // data length [0, 8]
	uint8_t channel;
	uint8_t flags;
	uint8_t reserved;

	uint8_t data[8];

	uint32_t timestamp_us;
};

/**
 * List usb devices
 * Linux: lsusb
 * MacOS: system_profiler SPUSBDataType
 */
int main(void) {
	libusb_device **devs;
	int r;
	ssize_t cnt;

	r = libusb_init(NULL);
	if (r < 0)
		return r;

	cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0){
		libusb_exit(NULL);
		return (int) cnt;
	}

	// print list of devices
	printDevices(devs);

	// print detailed info for each device
	for (int i = 0; devs[i]; ++i) {
		printDevice(devs[i], 0);
	}


	for (int i = 0; devs[i]; ++i) {
		libusb_device *dev = devs[i];
		libusb_device_descriptor desc;

		int ret = libusb_get_device_descriptor(dev, &desc);
		if (ret < 0) {
			std::cerr << "failed to get device descriptor" << std::endl;
			return -1;
		}
		if (desc.idVendor == 0x1d50 && desc.idProduct == 0x606f) {
			libusb_device_handle *handle;
			ret = libusb_open(dev, &handle);
			if (ret == LIBUSB_SUCCESS) {
				// get first configuration
				libusb_config_descriptor *config;
				ret = libusb_get_config_descriptor(dev, 0, &config);

				// set first configuration (reset alt_setting, reset toggles)
				libusb_set_configuration(handle, config->bConfigurationValue);

				// claim interface with bInterfaceNumber = 0
				ret = libusb_claim_interface(handle, 0);
				std::cout << "claim interface " << ret << std::endl;

				ret = libusb_set_interface_alt_setting(handle, 0, 0);
				std::cout << "set alternate setting " << ret << std::endl;

/*
				// test: get device descriptor
				// https://www.beyondlogic.org/usbnutshell/usb5.shtml
				UsbDeviceDescriptor descriptor;
				ret = libusb_control_transfer(handle,
					USB_DIR_IN,
					0x06, // get descriptor
					0x01 << 8, // DESCRIPTOR_DEVICE
					0,
					reinterpret_cast<unsigned char*>(&descriptor),
					sizeof(descriptor),
					1000);
				std::cout << "get device descriptor " << ret << std::endl;
*/
				// disable
				gs_device_mode mode;
				mode.mode = GS_CAN_MODE_RESET;
				mode.flags = 0;
				ret = libusb_control_transfer(handle,
					USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_INTERFACE,
					GS_USB_BREQ_MODE,
					0,
					0,
					reinterpret_cast<unsigned char*>(&mode),
					sizeof(mode),
					1000);
				std::cout << "reset can " << ret << std::endl;

				// get bit timing constriants
				gs_device_bt_const bitTimingConstraints;
				ret = libusb_control_transfer(handle,
					USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_INTERFACE,
					GS_USB_BREQ_BT_CONST,
					0,
					0,
					reinterpret_cast<unsigned char*>(&bitTimingConstraints),
					sizeof(bitTimingConstraints),
					1000);
				std::cout << "get bit timing constraints " << ret << std::endl;

				/*
					set bit timing
					clock rate: 48MHz
					bit rate: 500kHz
					sample point at 87.5%
					values calculated using http://www.bittiming.can-wiki.info/
				*/
				gs_device_bittiming bitTiming;
				bitTiming.prop_seg = 0;
				bitTiming.phase_seg1 = 13; // prop_seg + phase_seg1 = [1, 16]
				bitTiming.phase_seg2 = 2; // [1, 8]
				bitTiming.sjw = 1; // [1, 4]
				bitTiming.brp = bitTimingConstraints.fclk_can / (500000 * 16); // [1, 1024]
				ret = libusb_control_transfer(handle,
					USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_INTERFACE,
					GS_USB_BREQ_BITTIMING,
					0,
					0,
					reinterpret_cast<unsigned char*>(&bitTiming),
					sizeof(bitTiming),
					1000);
				std::cout << "set bit timing " << ret << std::endl;

				// enable
				mode.mode = GS_CAN_MODE_START;
				mode.flags = 0;
				ret = libusb_control_transfer(handle,
					USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_INTERFACE,
					GS_USB_BREQ_MODE,
					0,
					0,
					reinterpret_cast<unsigned char*>(&mode),
					sizeof(mode),
					1000);
				std::cout << "enable can " << ret << std::endl;

				while (true) {
					// send frame
					// https://en.wikipedia.org/wiki/OBD-II_PIDs
					gs_host_frame frame = {};
					frame.can_id = 0x7DF;
					frame.can_dlc = 8;
					frame.data[0] = 2; // number of bytes
					frame.data[1] = 1; // show current data
					//frame.data[2] = 12; // engine rpm
					frame.data[2] = 13; // km/h
					frame.data[3] = 0x55;
					frame.data[4] = 0x55;
					frame.data[5] = 0x55;
					frame.data[6] = 0x55;
					frame.data[7] = 0x55;
					
					int transferred = 0;
					ret = libusb_bulk_transfer(handle,
						0x02,
						reinterpret_cast<unsigned char*>(&frame),
						sizeof(frame),
						&transferred,
						1000);
					std::cout << ret << " sent " << transferred << std::endl << std::endl;
					
					ret = libusb_bulk_transfer(handle,
						0x81,
						reinterpret_cast<unsigned char*>(&frame),
						sizeof(frame),
						&transferred,
						1000);
					std::cout << ret << " received " << transferred << std::endl;
					std::cout << "number of bytes " << int(frame.data[0]) << std::endl;
					std::cout << "service " << std::hex << int(frame.data[1]) << std::dec << std::endl;
					std::cout << "km/h " << int(frame.data[2]) << std::endl;
					
					usleep(1000000);
				}
			}
		}
	}


	libusb_free_device_list(devs, 1);

	libusb_exit(NULL);
	return 0;
}
