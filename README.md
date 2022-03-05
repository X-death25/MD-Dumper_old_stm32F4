# MD-Dumper

![MD_pcb](https://github.com/X-death25/MD-Dumper/blob/main/img/DSC_0553.JPG) 

What is it ?
-----

MD Dumper is an USB Sega Megadrive cartridge Dumper : <br>
Hardware is based on STM32F4 (BluePillV2) : STM32F4x1 MiniF4 https://github.com/WeActTC/MiniSTM32F4x1  <br>
New version also add some developpement feature witch can be usefull for Megadrive developper.  <br>

Feature :
-----

Firmware Version : 2.0

Software Version : 2.0


| Feature | Implemented |
| ------- | ----------- |
| ROM Dump up to 16 MEG  Classic              | :white_check_mark: |
| ROM Dump up to 32 MEG  Bankswitch           | :white_check_mark: |
| SRAM Backup Memory Classic                  | :white_check_mark: |
| SRAM Backup Memory Bankswitch               | :white_check_mark: |
| Rewrite Krikzz flashkit cart or compatible  | :white_check_mark: |
| Sega LOCK-ON                                | :white_check_mark: |
| EEPROM I²C Sega Backup Memory               |                    |
| EEPROM I²C Acclaim Backup Memory            |                    |
| EEPROM I²C Codemaster Backup Memory         |                    |
| Tiido's Microwire/SPI Backup Memory         |                    |
| Sega Mapper 315-5779 SSFII classic          | :white_check_mark: |
| SSFII Extended Overdrive / S2 Delta         | :white_check_mark: |
| SSFII full 512 Mb + SRAM  DOA               |                    |
| Mapper T5740                                | :white_check_mark: |
| DMC : Direct Megadrive Connection           | :white_check_mark: |
| DMC : Dump 68K RAM                          |                    |

How to use it ?
-----

You just need an USB-C to USB. <br>
Connect the dumper to the PC and wait blue led activation. Insert game cartridge.  <br>
Start the software. Enjoy ! <br>

Operating System Compatibility
-----

Microsoft Windows 10

Where i can buy it ?
-----

https://www.tindie.com/products/xdeath/md-dumper-usb-megadrive-cartridge-readerwriter/


Linked Repositories
-----

My other projects related to Megadrive Hardware/Software developpement

-=Python MD-Dumper GUI=-
Easy and multiplateform GUI for MD-Dumper <br>
https://github.com/X-death25/PyMD-Dumper

-=DMC Direct Megadrive Connection=-
SGDK lib for bi-directionnal communication between Megadrive and PC <br>
*Coming Soon*

-=SMD_01=-
Simple 5V TTL 8MB Rewritable cartridge for testing code on real hardware <br>
*Coming Soon*

-=X-Flash=-
Altera EPM240 CPLD based Megadrive cartridge used for advanced research <br>
https://github.com/X-death25/X-Flash
