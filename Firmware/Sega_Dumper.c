/*
------------------------------------------------------
STM32F4 MD Dumper ARM Code
------------------------------------------------------
V2.0 10/04/2023
Public Version

*Upated Flash algo R/W
*Add Extra Hardware Game Detection
*Add Sega Lock-ON support
*Add SSFII classic support

*/

#include <string.h>
#include <stdlib.h>

// include only used LibopenCM3 lib

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/dwc/otg_fs.h>

// MD Dumper Specific Pinout

#define D0 GPIO0   // PA0   MD Data lines
#define D1 GPIO1   // PA1 ok
#define D2 GPIO2   // PA2 ok
#define D3 GPIO3   // PA3 ok
#define D4 GPIO4   // PA4 ok
#define D5 GPIO5   // PA5 ok
#define D6 GPIO6   // PA6 ok
#define D7 GPIO7   // PA7 ok

#define D8 GPIO1   // PB1  ok || WR in DMC Mode
#define D9 GPIO3   // PB3  ok  || RD in DMC Mode
#define D10 GPIO4   // PB4  ok 
#define D11 GPIO15   // PB15  ok
#define D12 GPIO13   // PB13  ok 
#define D13 GPIO0   // PB0  ok 

#define D14 GPIO5   //   PB5 ok
#define D15 GPIO6   //   PB6 ok 

//#define CLK_CLEAR 	GPIO0    // PB0 ok
#define CLK1 		GPIO12   // PB12 ok
#define CLK2 		GPIO10   // PB10 ok
#define CLK3 		GPIO14   // PB14 ok

#define OE 			GPIO7  // PB7 ok 
#define CE 			GPIO8  // PB8 ok
#define WE 			GPIO9  // PB9 ok 
#define RST			GPIO9  // PA9 ok
//#define MARK3 		GPIO5  // PB5 ok  voir pour remplacer par PA9

#define UWR			GPIO15  // PA15 ok 
#define TIME 		GPIO10  // PA10 ok  
#define ASTRB 		GPIO8  // PA8 ok   

#define LED_PIN 	GPIO13 // PC13  STM32 Led

// MD Dumper Special Command

#define WAKEUP     		0x10
#define READ_MD         0x11
#define READ_MD_SAVE  	0x12
#define WRITE_MD_SAVE 	0x13
#define WRITE_MD_FLASH 	0x14
#define ERASE_MD_FLASH 	0x15
#define READ_SMS   		0x16
#define CFI_MODE   		0x17
#define INFOS_ID 		0x18
#define DEBUG_MODE 		0x19


// Version

#define MAX_VERSION 	2
#define MIN_VERSION 	0


//void CheckVIO(void);

// MD Dumper Specific VAR

static const unsigned char stmReady[] = {'R','E','A','D','Y','!'};
static unsigned char dump_running = 0;
static unsigned char read8 = 0;
static unsigned char result=0;
static unsigned int read16 = 0;
static unsigned long address = 0;
static uint32_t len=0;
static uint32_t k=0;
static unsigned int chipid = 0;
static unsigned int slotRegister = 0; //for SMS
static unsigned int slotAdr = 0; //for SMS
static uint32_t gpioreg_inputA=0;
static uint32_t gpioreg_inputB=0;
static uint32_t gpioreg_outputA=0;
static uint32_t gpioreg_outputB=0;


// USB Specific VAR

static char serial_no[25];
static uint8_t usb_buffer_IN[64];
static uint8_t usb_buffer_OUT[64];
uint8_t usbd_control_buffer[128];
//static const unsigned char stmReady[] = {'R','E','A','D','Y','!'};

// MD Dumper function prototype

void wait(long nb);
void setDataInput(void);
void setDataOutput(void);
void DirectWrite8(unsigned char val);
void DirectWrite16(unsigned short val);
void setAddress(unsigned long adr);
void DirectRead8(void);
void DirectRead16(void);
void readMd(void);
void sms_mapper_register(unsigned char slot);
void readSMS(void);
void SendNextPaquet(usbd_device *usbd_dev, uint8_t ep);
void SendARMBuffer(usbd_device *usbd_dev, uint8_t ep);
void readMdSave(void);
void EraseMdSave(void);
void writeMdSave(void);
void writeFlash16(int adr,unsigned short word);
void ResetFlash(void);
void CFIWordProgram(void);
void commandMdFlash(unsigned long adr, unsigned int val);
void reset_command(void);
void EraseFlash(void);
void EraseFlash2(void);
void writeMdFlash(void);
void infosId(void);


//  USB Specific Fonction /////

static const struct usb_device_descriptor dev =
{
    .bLength = USB_DT_DEVICE_SIZE,
    .bDescriptorType = USB_DT_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0xff,
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
    .bMaxPacketSize0 = 64,
    .idVendor = 0x0483,
    .idProduct = 0x5740,
    .bcdDevice = 0x0200,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

static const struct usb_endpoint_descriptor data_endp[] = {{
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = 0x01,
        .bmAttributes = USB_ENDPOINT_ATTR_BULK,
        .wMaxPacketSize = 64,
        .bInterval = 1,
    }, {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = 0x82,
        .bmAttributes = USB_ENDPOINT_ATTR_BULK,
        .wMaxPacketSize = 64,
        .bInterval = 1,
    }
};

static const struct usb_interface_descriptor data_iface[] = {{
        .bLength = USB_DT_INTERFACE_SIZE,
        .bDescriptorType = USB_DT_INTERFACE,
        .bInterfaceNumber = 1,
        .bAlternateSetting = 0,
        .bNumEndpoints = 2,
        .bInterfaceClass = 0xff,
        .bInterfaceSubClass = 0,
        .bInterfaceProtocol = 0,
        .iInterface = 0,

        .endpoint = data_endp,
    }
};

static const struct usb_interface ifaces[] = {{
        .num_altsetting = 1,
        .altsetting = data_iface,
    }
};

static const struct usb_config_descriptor config =
{
    .bLength = USB_DT_CONFIGURATION_SIZE,
    .bDescriptorType = USB_DT_CONFIGURATION,
    .wTotalLength = 0,
    .bNumInterfaces = 1,
    .bConfigurationValue = 1,
    .iConfiguration = 0,
    .bmAttributes = 0x80,
    .bMaxPower = 0x32,

    .interface = ifaces,
};

static void usb_setup(void)
{
    /* Enable clocks for USB */

    rcc_periph_clock_enable(RCC_OTGFS);

    // Cleaning USB Buffer

    int i=0;

    for(i = 0; i < 64; i++)
    {
        usb_buffer_IN[i]=0x00;
        usb_buffer_OUT[i]=0x00;
    }
}

static const char *usb_strings[] =
{
    "Sege4ever 2021",
    "MD Dumper STM32F4",
    serial_no,
};

static char *get_dev_unique_id(char *s)
{
    volatile uint8_t *unique_id = (volatile uint8_t *)0x1FFFF7E8;
    int i;

    // Fetch serial number from chip's unique ID
    for(i = 0; i < 24; i+=2)
    {
        s[i] = ((*unique_id >> 4) & 0xF) + '0';
        s[i+1] = (*unique_id++ & 0xF) + '0';
    }
    for(i = 0; i < 24; i++)
        if(s[i] > '9')
            s[i] += 'A' - '9' - 1;

    return s;
}


void SendNextPaquet(usbd_device *usbd_dev, uint8_t ep)
{
	
    unsigned char adr = 0;
    unsigned char adr16 = 0;
	
	gpio_set(GPIOA,TIME);
	
    while(adr<64)  // 64bytes/32word
    {
        setDataOutput();
        setAddress(address+adr16); //4,96µs
        setDataInput();

		gpio_clear(GPIOB,CE);
		gpio_clear(GPIOB,OE);
	

        wait(16);

        DirectRead16(); //save into read16 global
        usb_buffer_OUT[adr] = (read16 >> 8)&0xFF; //word to byte
        usb_buffer_OUT[(adr+1)] = read16 & 0xFF;

		gpio_set(GPIOB,OE);
		gpio_set(GPIOB,CE);
	


        adr+=2;  //buffer
        adr16++; //md adr word
  }
    
    usbd_ep_write_packet(usbd_dev,ep,usb_buffer_OUT,64);
    address += 32;
    len +=32;
    
}
 

/*
* This gets called whenever a new IN packet has arrived from PC to STM32
 */

static void usbdev_data_rx_cb(usbd_device *usbd_dev, uint8_t ep)
{
    (void)ep;
    (void)usbd_dev;

    usb_buffer_IN[0] = 0;
    usbd_ep_read_packet(usbd_dev, 0x01,usb_buffer_IN, 64); // Read Paquets from PC

    address = (usb_buffer_IN[3]<<16) | (usb_buffer_IN[2]<<8) | usb_buffer_IN[1];

    if (usb_buffer_IN[0] == WAKEUP)   // Wake UP
    {
        memcpy((unsigned char *)usb_buffer_OUT, (unsigned char *)stmReady, sizeof(stmReady));
        usb_buffer_OUT[20] = MAX_VERSION;
        usb_buffer_OUT[21] = MIN_VERSION;
        setDataInput();
        usbd_ep_write_packet(usbd_dev, 0x82,usb_buffer_OUT,64);
    }

    if (usb_buffer_IN[0] == READ_MD && usb_buffer_IN[4] != 1 )   // READ MD Exchange mode ( Low Speed)
    {
        dump_running = 0;
        readMd();
        usbd_ep_write_packet(usbd_dev, 0x82,usb_buffer_OUT,64);
    }

    if (usb_buffer_IN[0] == READ_MD && usb_buffer_IN[4] == 1 )   // READ MD Streaming mode ( High Speed)
    {
        dump_running = 1;
        SendNextPaquet(usbd_dev,0x82);
    }
	
	
    if (usb_buffer_IN[0] == READ_MD_SAVE)  						 // READ MD Save
    {
        dump_running = 0;
        usbd_ep_write_packet(usbd_dev, 0x82,usb_buffer_OUT,64);
    }

    if (usb_buffer_IN[0] == WRITE_MD_SAVE)   					// WRITE MD Save
    {
        dump_running = 0;
        if (usb_buffer_IN[7] == 0xBB)
        {
            EraseMdSave();
        }
        if (usb_buffer_IN[7] == 0xCC)
        {
            gpio_clear(GPIOC, GPIO13); // LED on
            writeMdSave();
        }
        usb_buffer_OUT[6]=0xAA;
        usbd_ep_write_packet(usbd_dev, 0x82,usb_buffer_OUT,64);
        usb_buffer_OUT[6]=0x00;
    }

    if (usb_buffer_IN[0] == ERASE_MD_FLASH)   					// ERASE MD Flash
    {
        dump_running = 0;
		if ( usb_buffer_IN[1] == 1){EraseFlash();}
        if ( usb_buffer_IN[1] == 2){EraseFlash2();}
        usb_buffer_OUT[0]=0xFF;
        usbd_ep_write_packet(usbd_dev, 0x82,usb_buffer_OUT,64);
        usb_buffer_OUT[6]=0x00;
    }

    if (usb_buffer_IN[0] == WRITE_MD_FLASH)   					// WRITE MD Flash
    {
        dump_running = 0;
        writeMdFlash();
        usbd_ep_write_packet(usbd_dev, 0x82,usb_buffer_OUT,64);
        usb_buffer_OUT[6]=0x00;
    }
	
	 if (usb_buffer_IN[0] == READ_SMS) // Read SMS
    {
		dump_running = 0;		
		readSMS();
		usbd_ep_write_packet(usbd_dev, 0x82,usb_buffer_OUT,64);	
    }

    if (usb_buffer_IN[0] == INFOS_ID)   // Chip Information
    {
        dump_running = 0;
        gpio_clear(GPIOB,WE);
        usb_buffer_OUT[6]=0xFF;
        infosId();
        usbd_ep_write_packet(usbd_dev, 0x82,usb_buffer_OUT,64);
        usb_buffer_OUT[6]=0x00;
    }



}

/*
* This gets called whenever a new OUT packet has been send from STM32 to PC
*/

static void usbdev_data_tx_cb(usbd_device *usbd_dev, uint8_t ep)
{
    (void)ep;
    (void)usbd_dev;

    if ( dump_running == 1 )
    {
        SendNextPaquet(usbd_dev,0x82);
    }
	
    gpio_set(GPIOB,WE);

}

static void usbdev_set_config(usbd_device *usbd_dev, uint16_t wValue)
{
    (void)wValue;
    (void)usbd_dev;

    usbd_ep_setup(usbd_dev, 0x01, USB_ENDPOINT_ATTR_BULK, 64, usbdev_data_rx_cb);
    usbd_ep_setup(usbd_dev, 0x82, USB_ENDPOINT_ATTR_BULK, 64, usbdev_data_tx_cb);
}


//  GPIO Fonction /////

static void gpio_setup(void)
{
    /* Init Master Clock */

    rcc_clock_setup_pll(&rcc_hse_25mhz_3v3[RCC_CLOCK_3V3_84MHZ]);

    /* Enable GPIOC clock. */
    /* Using API functions: */

    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_GPIOC);
    //rcc_osc_off(RCC_USART2); // Disable OSC32 in PC14 PC15

    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO11);
    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO12);
    gpio_mode_setup(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO8 | GPIO9 | GPIO10 );
    gpio_set_af(GPIOA, GPIO_AF10, GPIO11 | GPIO12); // Set GPIO 11 - 12 to USB mode

    /*gpio_mode_setup(GPIOA,GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,D0 | D1 | D2 | D3 | D4 | D5 | D6 | D7);
    gpio_set_output_options(GPIOA,GPIO_OTYPE_PP,GPIO_OSPEED_50MHZ,D0 | D1 | D2 | D3 | D4 | D5 | D6 | D7);

    gpio_clear(GPIOA,D0 | D1 | D2 | D3 | D4 | D5 | D6 | D7);*/

    gpio_mode_setup(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_NONE,D8 | D9 | D10 | D11 | D12 | D13 | D14 | D15);

    gpio_mode_setup(GPIOA,GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,TIME | ASTRB );
    gpio_set_output_options(GPIOA,GPIO_OTYPE_PP,GPIO_OSPEED_50MHZ, TIME | ASTRB);
    gpio_mode_setup(GPIOA,GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,UWR);
    gpio_set_output_options(GPIOA,GPIO_OTYPE_PP,GPIO_OSPEED_50MHZ,UWR);
    gpio_set(GPIOA,TIME);

    gpio_mode_setup(GPIOB,GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,OE | CE | WE  );
    gpio_set_output_options(GPIOB,GPIO_OTYPE_PP,GPIO_OSPEED_50MHZ,OE | CE | WE  );
    gpio_set(GPIOB,WE);
    gpio_clear(GPIOB,OE);
    gpio_clear(GPIOB,CE);

    gpio_mode_setup(GPIOB,GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,CLK1 | CLK2 | CLK3 );
    gpio_set_output_options(GPIOB,GPIO_OTYPE_PP,GPIO_OSPEED_50MHZ,CLK1 | CLK2 | CLK3 );
    gpio_clear(GPIOB,CLK1);
    gpio_clear(GPIOB,CLK2);
    gpio_clear(GPIOB,CLK3);

    gpio_mode_setup(GPIOC,GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED_PIN);

    // Reg GPIO config

    gpio_mode_setup(GPIOA,GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,D0 | D1 | D2 | D3 | D4 | D5 | D6 | D7);
    gpio_set_output_options(GPIOA,GPIO_OTYPE_PP,GPIO_OSPEED_50MHZ,D0 | D1 | D2 | D3 | D4 | D5 | D6 | D7);
    gpio_mode_setup(GPIOB,GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,D8 | D9 | D10 | D11 | D12 | D13 | D14 | D15);
    gpio_set_output_options(GPIOB,GPIO_OTYPE_PP,GPIO_OSPEED_50MHZ,D8 | D9 | D10 | D11 | D12 | D13 | D14 | D15 );
    gpioreg_outputA = GPIO_MODER(GPIOA);
    gpioreg_outputB = GPIO_MODER(GPIOB);

    gpio_mode_setup(GPIOA, GPIO_MODE_INPUT,GPIO_PUPD_PULLUP, D0 | D1 | D2 | D3 | D4 | D5 | D6 | D7 | RST );
    gpio_mode_setup(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_NONE, D8 | D9 | D10 | D11 | D12 | D13 | D14 | D15);
    gpioreg_inputA = GPIO_MODER(GPIOA);
    gpioreg_inputB = GPIO_MODER(GPIOB);


}


//  MD Dumper Specific Fonction /////

void wait(long nb)
{
    while(nb)
    {
        __asm__("nop");
        nb--;
    }
}

void setDataInput(void)
{
	 gpio_mode_setup(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, D0 | D1 | D2 | D3 | D4 | D5 | D6 | D7 );
    gpio_mode_setup(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_NONE, D8 | D9 | D10 | D11 | D12 | D13 | D14 | D15);
  /*  GPIO_MODER(GPIOA) = gpioreg_inputA;
    GPIO_MODER(GPIOB) = gpioreg_inputB;*/
}

void setDataOutput(void)
{
	   gpio_mode_setup(GPIOA,GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,D0 | D1 | D2 | D3 | D4 | D5 | D6 | D7);
    gpio_set_output_options(GPIOA,GPIO_OTYPE_PP,GPIO_OSPEED_50MHZ,D0 | D1 | D2 | D3 | D4 | D5 | D6 | D7);
    gpio_mode_setup(GPIOB,GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,D8 | D9 | D10 | D11 | D12 | D13 | D14 | D15);
    gpio_set_output_options(GPIOB,GPIO_OTYPE_PP,GPIO_OSPEED_50MHZ,D8 | D9 | D10 | D11 | D12 | D13 | D14 | D15 );
    /*GPIO_MODER(GPIOA) = gpioreg_outputA;
    GPIO_MODER(GPIOB) = gpioreg_outputB;*/
}


void DirectWrite8(unsigned char val)
{

     if((val>>0)&1)
    {
        gpio_set(GPIOA,D0);
    }
    else
    {
        gpio_clear(GPIOA,D0);
    }
    if((val>>1)&1)
    {
        gpio_set(GPIOA,D1);
    }
    else
    {
        gpio_clear(GPIOA,D1);
    }
    if((val>>2)&1)
    {
        gpio_set(GPIOA,D2);
    }
    else
    {
        gpio_clear(GPIOA,D2);
    }
    if((val>>3)&1)
    {
        gpio_set(GPIOA,D3);
    }
    else
    {
        gpio_clear(GPIOA,D3);
    }
    if((val>>4)&1)
    {
        gpio_set(GPIOA,D4);
    }
    else
    {
        gpio_clear(GPIOA,D4);
    }
    if((val>>5)&1)
    {
        gpio_set(GPIOA,D5);
    }
    else
    {
        gpio_clear(GPIOA,D5);
    }
    if((val>>6)&1)
    {
        gpio_set(GPIOA,D6);
    }
    else
    {
        gpio_clear(GPIOA,D6);
    }
    if((val>>7)&1)
    {
        gpio_set(GPIOA,D7);
    }
    else
    {
        gpio_clear(GPIOA,D7);
    }

}


void DirectWrite16(unsigned short val)
{

  //  GPIO_ODR(GPIOA) = val;
	
	  if((val>>0)&1)
    {
        gpio_set(GPIOA,D0);
    }
    else
    {
        gpio_clear(GPIOA,D0);
    }
    if((val>>1)&1)
    {
        gpio_set(GPIOA,D1);
    }
    else
    {
        gpio_clear(GPIOA,D1);
    }
    if((val>>2)&1)
    {
        gpio_set(GPIOA,D2);
    }
    else
    {
        gpio_clear(GPIOA,D2);
    }
    if((val>>3)&1)
    {
        gpio_set(GPIOA,D3);
    }
    else
    {
        gpio_clear(GPIOA,D3);
    }
    if((val>>4)&1)
    {
        gpio_set(GPIOA,D4);
    }
    else
    {
        gpio_clear(GPIOA,D4);
    }
    if((val>>5)&1)
    {
        gpio_set(GPIOA,D5);
    }
    else
    {
        gpio_clear(GPIOA,D5);
    }
    if((val>>6)&1)
    {
        gpio_set(GPIOA,D6);
    }
    else
    {
        gpio_clear(GPIOA,D6);
    }
    if((val>>7)&1)
    {
        gpio_set(GPIOA,D7);
    }
    else
    {
        gpio_clear(GPIOA,D7);
    }
	
    if((val>>8)&1)
    {
        gpio_set(GPIOB,D8);
    }
    else
    {
        gpio_clear(GPIOB,D8);
    }
    if((val>>9)&1)
    {
        gpio_set(GPIOB,D9);
    }
    else
    {
        gpio_clear(GPIOB,D9);
    }
    if((val>>10)&1)
    {
        gpio_set(GPIOB,D10);
    }
    else
    {
        gpio_clear(GPIOB,D10);
    }
    if((val>>11)&1)
    {
        gpio_set(GPIOB,D11);
    }
    else
    {
        gpio_clear(GPIOB,D11);
    }
    if((val>>12)&1)
    {
        gpio_set(GPIOB,D12);
    }
    else
    {
        gpio_clear(GPIOB,D12);
    }
    if((val>>13)&1)
    {
        gpio_set(GPIOB,D13);
    }
    else
    {
        gpio_clear(GPIOB,D13);
    }
    if((val>>14)&1)
    {
        gpio_set(GPIOB,D14);
    }
    else
    {
        gpio_clear(GPIOB,D14);
    }
    if((val>>15)&1)
    {
        gpio_set(GPIOB,D15);
    }
    else
    {
        gpio_clear(GPIOB,D15);
    }
}

void DirectRead8()
{
   // read8 = gpio_port_read(GPIOA);
   read8 = 0;
     if (gpio_get(GPIOA,D0))
    {
        read8 |= 1;
    }
    if (gpio_get(GPIOA,D1))
    {
        read8 |= 2;
    }
    if (gpio_get(GPIOA,D2))
    {
        read8 |= 4;
    }
    if (gpio_get(GPIOA,D3))
    {
        read8 |= 8;
    }
    if (gpio_get(GPIOA,D4))
    {
        read8 |= 16;
    }
    if (gpio_get(GPIOA,D5))
    {
        read8 |= 32;
    }
    if (gpio_get(GPIOA,D6))
    {
        read8 |= 64;
    }
    if (gpio_get(GPIOA,D7))
    {
        read8 |= 128;
    }
}

void DirectRead16()
{
    result=0;
    DirectRead8();
    if (gpio_get(GPIOB,D8))
    {
        result |= 1;
    }
    if (gpio_get(GPIOB,D9))
    {
        result |= 2;
    }
    if (gpio_get(GPIOB,D10))
    {
        result |= 4;
    }
    if (gpio_get(GPIOB,D11))
    {
        result |= 8;
    }
    if (gpio_get(GPIOB,D12))
    {
        result |= 16;
    }
    if (gpio_get(GPIOB,D13))
    {
        result |= 32;
    }
    if (gpio_get(GPIOB,D14))
    {
        result |= 64;
    }
    if (gpio_get(GPIOB,D15))
    {
        result |= 128;
    }
    read16= (result<< 8) | read8;
  // read16= result;
}

void setAddress(unsigned long adr)
{

    setDataOutput();

    DirectWrite8(adr&0xFF);
    gpio_clear(GPIOB,CLK1);
    gpio_set(GPIOB,CLK1);

    DirectWrite8((adr>>8)&0xFF);
    gpio_clear(GPIOB,CLK2);
    gpio_set(GPIOB,CLK2);

    DirectWrite8((adr>>16)&0xFF);
    gpio_clear(GPIOB,CLK3);
    gpio_set(GPIOB,CLK3);

}

void readMd()  // 0.253 m/s
{

    unsigned char adr = 0;
    unsigned char adr16 = 0;

     setDataOutput(); // 1,3 µs

  /*  GPIO_MODER(GPIOA) = gpioreg_outputA;
    GPIO_MODER(GPIOB) = gpioreg_outputB;*/

    /*	GPIOB_BSRR |= CE;
    	GPIOB_BSRR |= CLK1;
    	GPIOB_BSRR |= CLK2;
    	GPIOB_BSRR |= CLK3;
    	GPIOB_BSRR |= OE;*/

    gpio_set(GPIOB,CE);
	gpio_set(GPIOA,ASTRB);
	gpio_set(GPIOA,UWR);
    gpio_set(GPIOB,CLK1);
    gpio_set(GPIOB,CLK2);
    gpio_set(GPIOB,CLK3);
    gpio_set(GPIOB,OE);
	gpio_set(GPIOA,TIME);


    while(adr<64)  // 64bytes/32word 8,5 µs * 32
    {

        setAddress(address+adr16); //4,96µs

       /* GPIO_MODER(GPIOA) = gpioreg_outputA;
        GPIO_MODER(GPIOB) = gpioreg_outputB;

        DirectWrite8((address+adr16)&0xFF);
        gpio_clear(GPIOB,CLK1);
        gpio_set(GPIOB,CLK1);

        DirectWrite8(((address+adr16)>>8)&0xFF);
        gpio_clear(GPIOB,CLK2);
        gpio_set(GPIOB,CLK2);

        DirectWrite8(((address+adr16)>>16)&0xFF);
        gpio_clear(GPIOB,CLK3);
        gpio_set(GPIOB,CLK3);*/

		setDataInput();
       /* GPIO_MODER(GPIOA) = gpioreg_inputA;
        GPIO_MODER(GPIOB) = gpioreg_inputB;*/

        gpio_clear(GPIOB,CE);
        gpio_clear(GPIOB,OE);
		gpio_clear(GPIOA,ASTRB);

        wait(16);

        DirectRead16();

        usb_buffer_OUT[adr] = (read16 >> 8)&0xFF; //word to byte
        usb_buffer_OUT[(adr+1)] = read16 & 0xFF;
        gpio_set(GPIOB,OE);
		gpio_set(GPIOA,ASTRB);
        gpio_set(GPIOB,CE);

        adr+=2;  //buffer
        adr16++; //md adr word
    }
}

void sms_mapper_register(unsigned char slot)
{
   setDataOutput();
   setAddress(slotRegister);
   gpio_set(GPIOB,CE);
   gpio_clear(GPIOB,OE);
   gpio_clear(GPIOB,WE);
   DirectWrite8(slot);
   gpio_set(GPIOB,WE);
   gpio_set(GPIOB,OE);
   gpio_clear(GPIOB,CE);
}


void readSMS()
{
    unsigned char adr = 0;
	unsigned char pageNum = 0;

	gpio_set(GPIOA,TIME);
	gpio_set(GPIOB,CE);
	gpio_set(GPIOB,OE);
	
   	if(address < 0x4000){
  		slotRegister = 0xFFFD; 	// slot 0 sega
  		pageNum = 0;
   		slotAdr = address;
   	}else if(address < 0x8000){
  		slotRegister = 0xFFFE; 	// slot 1 sega
  		pageNum = 1;
   		slotAdr = address;
   	}else{
  		slotRegister = 0xFFFF; 	// slot 2 sega
  		pageNum = (address/0x4000); //page num max 0xFF - 32mbits
   		slotAdr = 0x8000 + (address & 0x3FFF);
   	}

	sms_mapper_register(pageNum);

	if(slotAdr > 0x7FFF){gpio_set(GPIOB,CE);} //CE md == A15 in SMS mode !

    while(adr<64){

		setAddress(slotAdr +adr);

	    setDataInput();
	    gpio_clear(GPIOB,OE);

	    wait(16);

	    DirectRead8(); //save into read8 global
        usb_buffer_OUT[adr] = read8;

	    gpio_set(GPIOB,OE);
		adr++;

	}
   
}

void readMdSave()
{

    unsigned char adr = 0;

    setDataOutput();

    /* GPIOA_BSRR |= CE | CLK1| CLK2 | CLK3 | WE_FLASH | (CLK_CLEAR<<16);
     GPIOB_BSRR |= OE;*/

    gpio_set(GPIOB,CE);
    gpio_set(GPIOB,CLK1);
    gpio_set(GPIOB,CLK2);
    gpio_set(GPIOB,CLK3);
    gpio_set(GPIOB,OE);
    gpio_set(GPIOB,WE);

    //gpio_set(GPIOA,TIME);

    while(adr<64)
    {

        setAddress(address+adr);

        gpio_set(GPIOA,D0);
        gpio_clear(GPIOA,TIME);
        gpio_set(GPIOA,TIME);

        setDataInput();

        gpio_clear(GPIOB,CE);
        gpio_clear(GPIOB,OE);

        wait(16); //utile ?
        DirectRead8(); //save into read8 global
        usb_buffer_OUT[adr] = read8;

        //inhib OE

        gpio_set(GPIOB,CE);
        gpio_set(GPIOB,OE);

        adr++;
    }

}

void EraseMdSave()
{
    unsigned short adr = 0;
    setDataOutput();

    gpio_set(GPIOB,CE);
    gpio_set(GPIOB,CLK1);
    gpio_set(GPIOB,CLK2);
    gpio_set(GPIOB,CLK3);
    gpio_set(GPIOB,OE);
    gpio_set(GPIOB,WE);

    // SRAM rom > 16Mbit

    gpio_set(GPIOA,D0);
    gpio_clear(GPIOA,TIME);
    gpio_set(GPIOA,TIME);

    while(adr < 1024*32)
    {
        setAddress((address+adr));
        setDataOutput();
        DirectWrite8(0xFF);

        gpio_clear(GPIOB,CE);
        gpio_clear(GPIOB,WE);

        gpio_set(GPIOB,WE);
        gpio_set(GPIOB,CE);
        setDataInput();
        adr++;
    }
}

void writeMdSave()
{

    gpio_clear(GPIOC, GPIO13); // LED on
    unsigned short adr = 0;
    unsigned char byte = 0;
    setDataOutput();

    gpio_set(GPIOB,CE);
    gpio_set(GPIOB,CLK1);
    gpio_set(GPIOB,CLK2);
    gpio_set(GPIOB,CLK3);
    gpio_set(GPIOB,OE);
    gpio_set(GPIOB,WE);

    // SRAM rom > 16Mbit

    gpio_set(GPIOA,D0);
    gpio_clear(GPIOA,TIME);
    gpio_set(GPIOA,TIME);

    while(adr < 32)
    {
        setAddress((address+adr));
        setDataOutput();

        byte = usb_buffer_IN[32+adr];
        DirectWrite8(byte);
        gpio_clear(GPIOB,CE);
        gpio_clear(GPIOB,WE);

        gpio_set(GPIOB,WE);
        gpio_set(GPIOB,CE);
        setDataInput();
        adr++;
    }
}

void writeFlash16(int adr,unsigned short word)
{
    gpio_clear(GPIOB,WE);

    setAddress(adr);
    setDataOutput();
    gpio_clear(GPIOB,CE); // CE 0
    gpio_set(GPIOB,OE);   // OE 1
    DirectWrite16(word);
    gpio_set(GPIOB,WE); // WE 1

    gpio_set(GPIOB,CE);  // CE 1
    setDataInput();
}

void ResetFlash(void)
{
    writeFlash16(0x5555,0xAAAA);
    writeFlash16(0x2AAA,0x5555);
    writeFlash16(0x5555,0xF0F0);
}


void CFIWordProgram(void)
{
    writeFlash16(0x5555, 0xAAAA);
    writeFlash16(0x2AAA, 0x5555);
    writeFlash16(0x5555, 0xA0A0);
}

void commandMdFlash(unsigned long adr, unsigned int val)
{

    setAddress(adr);
    gpio_clear(GPIOB,CE);
    gpio_clear(GPIOB,WE);
    DirectWrite16(val);
    gpio_set(GPIOB,WE);
    DirectWrite16(val);
    gpio_set(GPIOB,WE);
    gpio_set(GPIOB,CE);
}

void reset_command()
{
    setDataOutput();

    commandMdFlash(0x5555, 0xAAAA);
    commandMdFlash(0x2AAA, 0x5555);
    commandMdFlash(0x5555, 0xF0F0);

    commandMdFlash(0x5555, 0xAA);
    commandMdFlash(0x2AAA, 0x55);
    commandMdFlash(0x5555, 0xF0);

    wait(16);
}

void EraseFlash()
{
    /*
       compatible
	   SST39SF020
	   SST39SF040
    */
	
    setDataOutput();

    gpio_set(GPIOB,CE);
    gpio_set(GPIOB,CLK1);
    gpio_set(GPIOB,CLK2);
    gpio_set(GPIOB,CLK3);
    gpio_set(GPIOB,OE);
    gpio_set(GPIOB,WE);


    commandMdFlash(0x5555, 0xAAAA);
    commandMdFlash(0x2AAA, 0x5555);
    commandMdFlash(0x5555, 0x8080);
    commandMdFlash(0x5555, 0xAAAA);
    commandMdFlash(0x2AAA, 0x5555);
    commandMdFlash(0x5555, 0x1010);

    commandMdFlash(0x5555, 0xAA);
    commandMdFlash(0x2AAA, 0x55);
    commandMdFlash(0x5555, 0x80);
    commandMdFlash(0x5555, 0xAA);
    commandMdFlash(0x2AAA, 0x55);
    commandMdFlash(0x5555, 0x10);

    wait(2400000);
    gpio_clear(GPIOC, GPIO13);

    reset_command();
    usb_buffer_OUT[0] = 0xFF;
}

void EraseFlash2()
{
	 unsigned char poll_dq7=0;
	 unsigned long m=0;

    setDataOutput();

    gpio_set(GPIOB,CE);
    gpio_set(GPIOB,CLK1);
    gpio_set(GPIOB,CLK2);
    gpio_set(GPIOB,CLK3);
    gpio_set(GPIOB,OE);
    gpio_set(GPIOB,WE);


    commandMdFlash(0x5555, 0xAAAA);
    commandMdFlash(0x2AAA, 0x5555);
    commandMdFlash(0x5555, 0x8080);
    commandMdFlash(0x5555, 0xAAAA);
    commandMdFlash(0x2AAA, 0x5555);
    commandMdFlash(0x5555, 0x1010);

    commandMdFlash(0x5555, 0xAA);
    commandMdFlash(0x2AAA, 0x55);
    commandMdFlash(0x5555, 0x80);
    commandMdFlash(0x5555, 0xAA);
    commandMdFlash(0x2AAA, 0x55);
    commandMdFlash(0x5555, 0x10);

      //  reset_command();

        setAddress(0);
        setDataInput();
        gpio_clear(GPIOB,CE);
        gpio_clear(GPIOB,OE);
        wait(16);
		m=0;
         while(m<250)
         {
            // poll_dq7 = (GPIOA_IDR >> 3)&0x80; //test only dq7
			wait(2400000);
			m++;
         }
        gpio_set(GPIOB,CE);
        gpio_set(GPIOB,OE);
    
    reset_command();
	reset_command();
    usb_buffer_OUT[0] = 0xFF;
	
}

void writeMdFlash()
{
    /*
    compatible
    29LV160 (amd)
    29LV320 (amd)
    29LV640 (amd)
    29W320 (st)
    29F800 (hynix)
    */

    //write in WORD

    unsigned char adr16=0;
    unsigned char j=5;
    unsigned char poll_dq7=0;
    unsigned char true_dq7=0;
    unsigned int val16=0;

    setDataOutput();

    gpio_set(GPIOB,CE);
    gpio_set(GPIOB,CLK1);
    gpio_set(GPIOB,CLK2);
    gpio_set(GPIOB,CLK3);
    gpio_set(GPIOB,OE);
    gpio_set(GPIOB,WE);

    while(adr16 < (usb_buffer_IN[4]>>1))
    {

        val16 = ((usb_buffer_IN[j])<<8) | usb_buffer_IN[(j+1)];
        true_dq7 = (val16 & 0x80);
        poll_dq7 = ~true_dq7;

        if(val16!=0xFFFF)
        {
            setDataOutput();

            CFIWordProgram();
            commandMdFlash((address+adr16), val16);

            if(((chipid&0xFF00)>>8) == 0xBF)
            {
                wait(160); //SST Microchip
            }
            else
            {
                reset_command();

                gpio_clear(GPIOB,CE);
                gpio_clear(GPIOB,OE);
                setAddress((address+adr16));
				wait(16);
                setDataInput();
                gpio_set(GPIOB,OE);
                gpio_set(GPIOB,CE);
            }
        }
        j+=2;
        adr16++;
    }
}

void infosId()
{
    //seems to be valid only for 29LVxxx ?
    setDataOutput();

    gpio_set(GPIOB,CE);
    gpio_set(GPIOB,CLK1);
    gpio_set(GPIOB,CLK2);
    gpio_set(GPIOB,CLK3);
    gpio_set(GPIOB,OE);
    gpio_set(GPIOB,WE);

    commandMdFlash(0x5555, 0xAA);
    commandMdFlash(0x2AAA, 0x55);
    commandMdFlash(0x5555, 0x90);

    setAddress(0); //Manufacturer
    setDataInput();

    gpio_clear(GPIOB,CE);
    gpio_clear(GPIOB,OE);
    wait(16);
    DirectRead16();
    usb_buffer_OUT[0] = 0;
    usb_buffer_OUT[1] = read16&0xFF;
    gpio_set(GPIOB,OE);
    gpio_set(GPIOB,CE);

    reset_command();

    commandMdFlash(0x5555, 0xAA);
    commandMdFlash(0x2AAA, 0x55);
    commandMdFlash(0x5555, 0x90);

    setAddress(1); //Flash id
    setDataInput();
    gpio_clear(GPIOB,CE);
    gpio_clear(GPIOB,OE);
    wait(16);
    DirectRead16();
    usb_buffer_OUT[2] = 1;
    usb_buffer_OUT[3] = read16&0xFF;
    gpio_set(GPIOB,OE);
    gpio_set(GPIOB,CE);

    reset_command();

    chipid = (usb_buffer_OUT[1]<<8) | usb_buffer_OUT[3];
}


//  Main Fonction /////

int main(void)
{
    int i;
    usbd_device *usbd_dev;

    // Init Sega Dumper GPIO

    gpio_setup();

    for( i = 0; i < 0x800000; i++)
    {
        __asm__("nop");    //1sec
    }
	
	
    k=0;
    i=0;

    setDataInput();

	
	// Say thanks to chinese crappy USB boards
	
	gpio_mode_setup(GPIOA,GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,GPIO12 );
	gpio_clear(GPIOA, GPIO12);
	
	    for( i = 0; i < 0x800000; i++)
    {
        __asm__("nop");    //1sec
    }
	gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO11);
    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO12);
    gpio_set_af(GPIOA, GPIO_AF10, GPIO11 | GPIO12); // Set GPIO 11 - 12 to USB mode

    get_dev_unique_id(serial_no);
    usb_setup();

    usbd_dev = usbd_init(&otgfs_usb_driver, &dev, &config, usb_strings,
                         3, usbd_control_buffer, sizeof(usbd_control_buffer));
    OTG_FS_GCCFG |= OTG_GCCFG_NOVBUSSENS | OTG_GCCFG_PWRDWN;
    OTG_FS_GCCFG &= ~(OTG_GCCFG_VBUSBSEN | OTG_GCCFG_VBUSASEN);

    usbd_register_set_config_callback(usbd_dev, usbdev_set_config);

    for (i = 0; i < 0x800000; i++)
    {
        __asm__("nop");
    }

    gpio_clear(GPIOC, LED_PIN);	/* LED on */


    /* At this point Sega Dumper is ready */


    while (1)
    {

        usbd_poll(usbd_dev);

    }

    return 0;
}
