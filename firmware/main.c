#include <stddef.h>
#include <stdint.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/st_usbfs.h>
#include <libopencm3/stm32/can.h>

// stm32f042xx data sheet: https://www.st.com/resource/en/datasheet/stm32f042c6.pdf
// stm32f042xx reference manual: https://www.st.com/content/ccc/resource/technical/document/reference_manual/c2/f8/8a/f2/18/e6/43/96/DM00031936.pdf/files/DM00031936.pdf/jcr:content/translations/en.DM00031936.pdf
//   usb: chapter 30, page 858
//   can: chapter 29, page 814
//     can bit timing: section 29.7.7, page 829

// USB
// pins: USB_DM on PA11, USB_DP on PA12
// usb overview: https://www.beyondlogic.org/usbnutshell/usb5.shtml
// libopencm3 example: https://github.com/libopencm3/libopencm3-examples/blob/master/examples/stm32/f1/stm32-maple/usb_cdcacm/cdcacm.c
// other usb example: https://github.com/Erlkoenig90/f1usb
// usbmon: https://www.kernel.org/doc/Documentation/usb/usbmon.txt

// CAN
// pins: CAN_RX on PB8, CAN_TX on PB9
// bit timing: http://www.bittiming.can-wiki.info/


static void ledPwrOn() {
	gpio_set(GPIOB, GPIO0);
}

static void ledPwrOff() {
	gpio_clear(GPIOB, GPIO0);
}

static void ledPwrToggle() {
	gpio_toggle(GPIOB, GPIO0);
}

static void ledActOn() {
	gpio_set(GPIOB, GPIO1);
}

static void ledActOff() {
	gpio_clear(GPIOB, GPIO1);
}

static void ledActToggle() {
	gpio_toggle(GPIOB, GPIO1);
}


inline int min(int a, int b) {
	return a < b ? a : b;
}


// USB
// ------------------------------------

// transfer direction
enum UsbDirection {
	USB_OUT = 0, // to device
	USB_IN = 0x80 // to host
};

enum UsbDescriptorType {
	USB_DESCRIPTOR_DEVICE = 0x01,
	USB_DESCRIPTOR_CONFIGURATION = 0x02,
	USB_DESCRIPTOR_INTERFACE = 0x04,
	USB_DESCRIPTOR_ENDPOINT = 0x05
};

enum UsbEndpointType {
	USB_ENDPOINT_CONTROL = 0,
	USB_ENDPOINT_ISOCHRONOUS = 1,
	USB_ENDPOINT_BULK = 2,
	USB_ENDPOINT_INTERRUPT = 3
};

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

struct UsbConfigDescriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t wTotalLength;
	uint8_t bNumInterfaces;
	uint8_t bConfigurationValue;
	uint8_t iConfiguration;
	uint8_t bmAttributes;
	uint8_t bMaxPower;
} __attribute__((packed));

struct UsbInterfaceDescriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bInterfaceNumber;
	uint8_t bAlternateSetting;
	uint8_t bNumEndpoints;
	uint8_t bInterfaceClass;
	uint8_t bInterfaceSubClass;
	uint8_t bInterfaceProtocol;
	uint8_t iInterface;
} __attribute__((packed));

struct UsbEndpointDescriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bEndpointAddress;
	uint8_t bmAttributes;
	uint16_t wMaxPacketSize;
	uint8_t bInterval;
} __attribute__((packed));


// device descriptor
static const struct UsbDeviceDescriptor usbDevice = {
	.bLength = sizeof(struct UsbDeviceDescriptor),
	.bDescriptorType = USB_DESCRIPTOR_DEVICE,
	.bcdUSB = 0x0200, // USB 2.0
	.bDeviceClass = 0xff, // no class
	.bDeviceSubClass = 0xff,
	.bDeviceProtocol = 0xff,
	.bMaxPacketSize0 = 64, // max packet size for endpoint 0
	.idVendor = 0x0483, // STMicroelectronics
	.idProduct = 0x5722, // Bulk Demo
	.bcdDevice = 0x0100, // device version
	.iManufacturer = 0, // index into string table
	.iProduct = 0, // index into string table
	.iSerialNumber = 0, // index into string table
	.bNumConfigurations = 1
};

// configuration descriptor
struct UsbConfiguration {
	struct UsbConfigDescriptor config;
	struct UsbInterfaceDescriptor interface;
	struct UsbEndpointDescriptor endpoint1;
	struct UsbEndpointDescriptor endpoint2;
} __attribute__((packed));

static const struct UsbConfiguration usbConfiguration = {
	.config = {
		.bLength = sizeof(struct UsbConfigDescriptor),
		.bDescriptorType = USB_DESCRIPTOR_CONFIGURATION,
		.wTotalLength = sizeof(struct UsbConfiguration),
		.bNumInterfaces = 1,
		.bConfigurationValue = 1,
		.iConfiguration = 0,
		.bmAttributes = 0x80, // bus powered
		.bMaxPower = 50 // 100 mA
	},
	.interface = {
		.bLength = sizeof(struct UsbInterfaceDescriptor),
		.bDescriptorType = USB_DESCRIPTOR_INTERFACE,
		.bInterfaceNumber = 0,
		.bAlternateSetting = 0,
		.bNumEndpoints = 2,
		.bInterfaceClass = 0xff, // no class
		.bInterfaceSubClass = 0xff,
		.bInterfaceProtocol = 0xff,
		.iInterface = 0
	},
	.endpoint1 = {
		.bLength = sizeof(struct UsbEndpointDescriptor),
		.bDescriptorType = USB_DESCRIPTOR_ENDPOINT,
		.bEndpointAddress = USB_IN | 1, // in 1 (tx)
		.bmAttributes = USB_ENDPOINT_BULK,
		.wMaxPacketSize = 16,
		.bInterval = 1 // polling interval
	},
	.endpoint2 = {
		.bLength = sizeof(struct UsbEndpointDescriptor),
		.bDescriptorType = USB_DESCRIPTOR_ENDPOINT,
		.bEndpointAddress = USB_OUT | 2, // out 2 (rx)
		.bmAttributes = USB_ENDPOINT_BULK,
		.wMaxPacketSize = 16,
		.bInterval = 1 // polling interval
	}
};

// control request type  
enum UsbRequestType {
	USB_REQUEST_TYPE_MASK = (0x03 << 5),
	USB_REQUEST_TYPE_STANDARD = (0x00 << 5),
	USB_REQUEST_TYPE_CLASS = (0x01 << 5),
	USB_REQUEST_TYPE_VENDOR = (0x02 << 5),
	USB_REQUEST_TYPE_RESERVED = (0x03 << 5)
};

// control request recipient
enum UsbRequestRecipient {
	USB_RECIPIENT_MASK = 0x1f,
	USB_RECIPIENT_DEVICE = 0x00,
	USB_RECIPIENT_INTERFACE = 0x01,
	USB_RECIPIENT_ENDPOINT = 0x02,
	USB_RECIPIENT_OTHER = 0x03
};

// control request data, transferred in the setup packet
struct UsbRequest {
	uint8_t bmRequestType; // combination of UsbDirection, UsbRequestType and UsbRequestRecipient
	uint8_t bRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;
};

// setup usb and control endpoints (assumes that usb just exited reset state)
void usbSetup() {
	// clear interrupts of usb
	SET_REG(USB_ISTR_REG, 0);

	// packet memory layout
	// offset | size | description
	//      0 |   32 | buffer table for 4 endpoints
	//     32 |   64 | tx buffer of control endpoint 0
	//     96 |   64 | rx buffer of control endpoint 0
	//    160 |   16 | tx buffer of bulk endpoint 1 (in to host)
	//    176 |   16 | rx buffer of bulk endpoint 2 (out from host)

	// set buffer table address inside packet memory (relative to USB_PMA_BASE)
	SET_REG(USB_BTABLE_REG, 0);
	
	// setup buffers for endpoint 0 (tx count is set when actually sending data)
	SET_REG(USB_EP_TX_ADDR(0), 32);
	SET_REG(USB_EP_RX_ADDR(0), 96);
	SET_REG(USB_EP_RX_COUNT(0), 0x8000 | (1 << 10)); // rx buffer size is 64

	// setup control endpoint 0
	SET_REG(USB_EP_REG(0), USB_EP_TYPE_CONTROL | USB_EP_RX_STAT_VALID | 0);

	// enable usb at usb address 0
	SET_REG(USB_DADDR_REG, USB_DADDR_EF | 0);
}

// setup the data endpoints
void usbSetupEndpoints() {
	// setup buffers for endpoint 1 (tx count is set when actually sending data)
	SET_REG(USB_EP_TX_ADDR(1), 160);
	SET_REG(USB_EP_RX_ADDR(2), 176);
	SET_REG(USB_EP_RX_COUNT(2), 8 << 10); // rx buffer size is 16

	// clear rx and tx flags, endpoint type, kind and address
	uint16_t clear = USB_EP_RX_CTR | USB_EP_TX_CTR | USB_EP_TYPE | USB_EP_KIND | USB_EP_ADDR;

	// set endpoint type
	uint16_t set = USB_EP_TYPE_BULK;

	// tx (in) endpoint 1: stall send, clear other toggle bits
	uint16_t epReg = GET_REG(USB_EP_REG(1));
	SET_REG(USB_EP_REG(1), ((epReg ^ USB_EP_TX_STAT_STALL) & ~clear) | set | 1);

	// rx (out) endpoint 2: ready to receive, clear other toggle bits
	epReg = GET_REG(USB_EP_REG(2));
	SET_REG(USB_EP_REG(2), ((epReg ^ USB_EP_RX_STAT_VALID) & ~clear) | set | 2);
}

/**
	Note:
	These flags of USB_EP_REG toggle when written with 1 and don't change when written with 0
	USB_EP_RX_DTOG
	USB_EP_RX_STAT
	USB_EP_TX_DTOG
	USB_EP_TX_STAT
	These flags can only be cleared and should be written with 1 to keep current state
	USB_EP_RX_CTR
	USB_EP_TX_CTR
*/

// send data to the host
void usbSend(int ep, const void *data, int size) {
	// copy data from flash into tx buffer
	const uint16_t * src = (const uint16_t*)data;
	uint16_t * dst = (uint16_t*)USB_GET_EP_TX_BUFF(ep);
	int s = (size + 1) / 2;
	for (int i = 0; i < s; ++i) {
		*dst = *src;
		++src;
		dst += 2; // ABP1 bus is 32 bit only
	}
	
	// set size of packet in tx buffer
	SET_REG(USB_EP_TX_COUNT(ep), size);
	
	// clear tx flag and don't change other toggle flags (see note above)
	uint16_t clear = USB_EP_TX_CTR | USB_EP_RX_DTOG | USB_EP_TX_DTOG | USB_EP_RX_STAT;

	// don't clear rx flag (see note above)
	uint16_t set = USB_EP_RX_CTR;

	// indicate that we are ready to send
	uint16_t epReg = GET_REG(USB_EP_REG(ep));
	SET_REG(USB_EP_REG(ep), ((epReg ^ USB_EP_TX_STAT_VALID) & ~clear) | set);
}

// acknowledge send requests with a stall to indicate unsupported request
void usbSendStall() {
	// clear tx flag and don't change other toggle flags (see note above)
	uint16_t clear = USB_EP_TX_CTR | USB_EP_RX_DTOG | USB_EP_TX_DTOG | USB_EP_RX_STAT;

	// don't clear rx flag (see note above)
	uint16_t set = USB_EP_RX_CTR;

	// indicate that we are ready to send
	uint16_t epReg = GET_REG(USB_EP_REG(0));
	SET_REG(USB_EP_REG(0), ((epReg ^ USB_EP_TX_STAT_STALL) & ~clear) | set);
}

// indicate that we want to receive data from the host
void usbReceive(int ep) {
	// clear rx flag and don't change other toggle flags (see note above)
	uint16_t clear = USB_EP_RX_CTR | USB_EP_RX_DTOG | USB_EP_TX_DTOG | USB_EP_TX_STAT;

	// don't clear tx flag (see note above)
	uint16_t set = USB_EP_TX_CTR;

	// indicate that we are ready to receive
	uint16_t epReg = GET_REG(USB_EP_REG(ep));
	SET_REG(USB_EP_REG(ep), ((epReg ^ USB_EP_RX_STAT_VALID) & ~clear) | set);
}

// the current operating mode of the usb device handler code
enum UsbMode {
	IDLE,
	SET_ADDRESS,
	AWAIT_TX,
	GET_DESCRIPTOR,
};

int main(void) {
	// SYSCLK = 48MHz from internal 48MHz oscillator
	rcc_clock_setup_in_hsi48_out_48mhz();

	// gpio
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_GPIOC);

	// usb
	crs_autotrim_usb_enable();
	rcc_set_usbclk_source(RCC_HSI48);
	rcc_periph_clock_enable(RCC_USB);

	// can
	//rcc_periph_clock_enable(RCC_CAN1);
	
	
	// set PB1 to output for the blue LED
	gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO0 | GPIO1);
	gpio_set_output_options(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_LOW, GPIO0 | GPIO1);
	ledPwrOn();
	ledActOff();

/*	
	// init CAN
	
	// map CAN to port B, RX on PB8, TX on PB9
	AFIO_MAPR |= AFIO_MAPR_CAN1_REMAP_PORTB;
	
	// configure RX: input pull-up
	gpio_set_mode(GPIO_BANK_CAN1_PB_RX, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, GPIO_CAN1_PB_RX);
	gpio_set(GPIO_BANK_CAN1_PB_RX, GPIO_CAN1_PB_RX);

	// Configure TX: fast output push-pull
	gpio_set_mode(GPIO_BANK_CAN1_PB_TX, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_CAN1_PB_TX);
		      
	// reset
	can_reset(CAN1);
	
	// Can frequency = APB1 / BRP / (1 + TS1 + TS2)
	// -> 36MHz / 2 = 18MHz
	// -> 18MHz / (1 + 10 + 7) = 1MHz
	// http://libopencm3.org/docs/latest/stm32f1/html/group__can__defines.html#ga801fb4aace625ef7270c1eb0944c9578
	if (can_init(CAN1,
		false, // TTCM: Time triggered comm mode?
		true, // ABOM: Automatic bus-off management?
		false, // AWUM: Automatic wakeup mode?
		false, // NART: No automatic retransmission?
		false, // RFLM: Receive FIFO locked mode?
		false, // TXFP: Transmit FIFO priority?
		CAN_BTR_SJW_1TQ, // Resynchronization time quanta jump width
		CAN_BTR_TS1_10TQ, // Number of time quanta for time segment 1 (TS1)
		CAN_BTR_TS2_7TQ, // Number of time quanta for time segment 2 (TS2)
		2, // Baud rate prescaler (BRP)
		false,
		false))
	{
		ledOff();

		// die because we failed to initialize
		while (1)
			__asm__("nop");
	}

	// init filter
	can_filter_id_mask_32bit_init(
		0, // Filter ID
		0, // CAN ID
		0, // CAN ID mask
		0, // FIFO assignment (here: FIFO0)
		true); // Enable the filter.
*/

	// init USB
	// reference manual: 23.4.2 System and power-on reset
	
	// switch on usb transceiver, but keep reset
	SET_REG(USB_CNTR_REG, USB_CNTR_FRES);
	
	// wait for at least 1us (see data sheet: Table 43. USB startup time)
	for (int i = 0; i < 72; i++)
		__asm__("nop");
			
	// exit reset of usb
	SET_REG(USB_CNTR_REG, 0);

	// setup in default state
	usbSetup();

	// set usb operating mode
	enum UsbMode usbMode = IDLE;
	
	// temp variable for usb address
	uint8_t usbAddress = 0;

	// wait for incoming request or reset
	while (1) {
		// check reset
		if (GET_REG(USB_ISTR_REG) & USB_ISTR_RESET) {
			// reset detected: setup in default state
			usbSetup();
		}

		// check control endpoint
		uint16_t ep0 = GET_REG(USB_EP_REG(0));
		if (ep0 & USB_EP_RX_CTR) {
ledActOn();
			if (ep0 & USB_EP_SETUP) {
				// received a setup packet from the host
				if ((GET_REG(USB_EP_RX_COUNT(0)) & 0x3ff) >= sizeof(struct UsbRequest)) {
					struct UsbRequest request;

					// copy request from rx buffer to system memory
					uint16_t *src = (uint16_t*)USB_GET_EP_RX_BUFF(0);
					uint16_t *dst = (uint16_t*)&request;
					for (int i = 0; i < sizeof(struct UsbRequest) / 2; ++i) {
						*dst = *src;
						src += 2; // ABP1 bus is 32 bit only, therefore skip over upper 16 bit
						++dst;
					}

					// check request type
					// https://www.beyondlogic.org/usbnutshell/usb6.shtml			
					switch (request.bmRequestType) {
					case USB_OUT | USB_REQUEST_TYPE_STANDARD | USB_RECIPIENT_DEVICE:
						// write request to standard device
						if (request.bRequest == 0x05) {
							// set address, but store in memory until zlp was sent
							usbMode = SET_ADDRESS;
							usbAddress = request.wValue;

							// setup zero length packet (zlp) in tx buffer for status stage
							usbSend(0, NULL, 0);
						} else if (request.bRequest == 0x09) {
							// set configuration
							usbMode = AWAIT_TX;
							uint8_t bConfigurationValue = request.wValue;
							usbSetupEndpoints();

							// send first data
							usbSend(1, &usbDevice, 4);

							// setup zero length packet (zlp) in tx buffer for status stage
							usbSend(0, NULL, 0);
						} else {
							// unsupported request: stall
							usbSendStall();
						}
						break;
					case USB_IN | USB_REQUEST_TYPE_STANDARD | USB_RECIPIENT_DEVICE:
						// read request to standard device
						if (request.bRequest == 0x06) {
							// get descriptor
							uint8_t descriptorType = request.wValue >> 8;
							if (descriptorType == USB_DESCRIPTOR_DEVICE) {
								// send device descriptor
								usbMode = GET_DESCRIPTOR;
								int size = min(sizeof(struct UsbDeviceDescriptor), request.wLength);
								usbSend(0, &usbDevice, size);
							} else if (descriptorType == USB_DESCRIPTOR_CONFIGURATION) {
								// send configuration descriptor
								usbMode = GET_DESCRIPTOR;
								int size = min(sizeof(struct UsbConfiguration), request.wLength);
								usbSend(0, &usbConfiguration, size);
//ledOn();
							} else {
								// unsupported descriptor type: stall
								usbSendStall();
							}
						} else {
							// unsupported request: stall
							usbSendStall();
						}
						break;
					case USB_OUT | USB_REQUEST_TYPE_STANDARD | USB_RECIPIENT_INTERFACE:
						// write request to standard interface
						if (request.bRequest == 0x0b) {
							// set interface
							usbMode = AWAIT_TX;
							uint8_t bInterface = request.wIndex;
							uint8_t bAlternateSetting = request.wValue;

							// setup zero length packet in tx buffer for status stage
							usbSend(0, NULL, 0);
						} else {
							// unsupported request: stall
							usbSendStall(0);
						}
						break;
					case USB_OUT | USB_REQUEST_TYPE_STANDARD | USB_RECIPIENT_ENDPOINT:
						// write request to standard endpoint
						if (request.bRequest == 0x01) {
							// clear feature
							usbMode = AWAIT_TX;

							// setup zero length packet in tx buffer for status stage
							usbSend(0, NULL, 0);
						} else {
							// unsupported request: stall
							usbSendStall(0);
						}
						break;
					default:
						// unsupported request type: stall
						usbSendStall(0);
					}
				} else {
					// request too short: stall
					usbSendStall(0);
				}
			} else {
				// received a packet from the host
				switch (usbMode) {
				case GET_DESCRIPTOR:
					// zlp received (out status stage)
//ledOff();
					usbMode = IDLE;
					break;
				}
			}

			// enable receiving again
			usbReceive(0);
		}
		if (ep0 & USB_EP_TX_CTR) {
			// last send to host has completed
			switch (usbMode) {
			case SET_ADDRESS:
				// zlp sent (out status stage), now we can set the usb address
				SET_REG(USB_DADDR_REG, USB_DADDR_EF | usbAddress);
				usbMode = IDLE;
				usbSendStall();
				break;
			case AWAIT_TX:
				// zlp sent (out status stage)
				usbMode = IDLE;
				usbSendStall();
				break;
			case GET_DESCRIPTOR:
				// todo: prepare next data packet (out data stage), not needed if descriptor is < 64 bytes
				usbSend(0, NULL, 0);
				break;
			default:
				usbSendStall();				
			}
		}


		// check tx (in) endpoint 1
		uint16_t ep1 = GET_REG(USB_EP_REG(1));
		if (ep1 & USB_EP_TX_CTR) {
			// last send to host has completed
			ledActToggle();

			// send next data
			usbSend(1, &usbDevice, 4);
		}
		
		// check rx (out) endpoint 2
		uint16_t ep2 = GET_REG(USB_EP_REG(2));
		if (ep2 & USB_EP_RX_CTR) {
			// received data from the host
			if (*USB_GET_EP_RX_BUFF(2))
				ledActOn();
			else
				ledActOff();

			// receive next data
			usbReceive(2);
		}
	}
	return 0;
}
