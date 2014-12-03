T-Bone
======
Electronic for up to 5 axes DIY desktop manufacturing platforms

LICENSE Files
=============
"LICENSE_HW" --> Attribution-ShareAlike 4.0 International license effective for the hardware part
"LICENSE_SW" --> agpl-3.0 license effective for the software part

"Software/ArduinoClient" Folder
===============================
The ArduinoClient folder  contains the Arduino software.

"Documentation/SoftwareDocumentation.pdf" File
==============================================
Documentation for the implemented protocol to communicate with the T-Bone via the UART interface.

"Documentation/T-bone_command_documentation.txt" File
=====================================================
Example file for the communication between Beagle Bone Black and the T-Bone (Arduino Leonardo)

Arduino Leonardo Pinout
=======================
Adapted from pins.h in the Arduino software.

Arduino Pin – Atmel Pin & function - connection

    D0 - PD2, RXD1/INT2 - Serial BeagleBone (UART1_TXD)
    D1 - PD3 TXD1/INT3 - Serial BeagleBone (UART1_RXD)
    D2 - PD1 SDA SDA/INT1 - Interrupt 4361 Nr.2
    D3# - PD0 PWM8/SCL OC0B/SCL/INT0 - Interrupt 4361 Nr.1
    D4 - A6 - PD4 ADC8 - CS 4361 Nr. 1
    D5# - PC6 ??? OC3A/#OC4A - PWM3, Fan
    D6# A7 - PD7 FastPWM #OC4D/ADC10 - CS 5031 Nr. 3
    D7 - PE6 INT6/AIN0 - Interrupt 4361 Nr.3
    D8 A8 - PB4 ADC11/PCINT4 - Interrupt 5041
    D9# A9 - PB5 PWM16 OC1A/#OC4B/ADC12/PCINT5 - PWM 1
    D10# A10 - PB6 PWM16 OC1B/0c4B/ADC13/PCINT6 - PWM 2
    D11# - PB7 PWM8/16 0C0A/OC1C/#RTS/PCINT7 - CS 5041
    D12 A11 - PD6 T1/#OC4D/ADC9 - CS 4361 Nr. 2
    D13# - PC7 PWM10 CLK0/OC4A - clock output do not use, never!
    A0 - D18 PF7 ADC7 - Temperature Sensor 1
    A1 - D19 PF6 ADC6 - Temperature Sensor 2
    A2 - D20 PF5 ADC5 - Temperature Sensor 3
    A3 - D21 PF4 ADC4 - Start Signal
    A4 - D22 PF1 ADC1 - Current PWM 1
    A5 - D23 PF0 ADC0 - Current PWM 2
    MISO - D14 PB3 MISO,PCINT3 - MISO
    SCK - D15 PB1 SCK,PCINT1 - SCK
    MOSI - D16 PB2 MOSI,PCINT2 - MOSI
    SS - D17 PB0 RXLED,SS/PCINT0 - RX LED
    TXLED - PD5 . TX LED
    HWB - PE2 HWB - Reset for motion drivers

"Software/ArduinoLibraries" Folder
==================================
This folder contains the used Arduino Libraries needed to compile the code.
!!Attention!!
The used libraries are standard ones, BUT customized. So be sure to use these!
!!Attention!!

For information on installing libraries, see: http://arduino.cc/en/Guide/Libraries

"Software/t_bone_server" Folder
===============================
This folder contains the software running on the Beagle Bone Black for the Deluxe version.

It's prepared to run a 3D-Printer Hardware. But can be customized to control different kind of desktop manufacturing platforms.

"Hardware/T-Bone Folder"
========================
Contains schematic and board design of the latest version of the T-Bone HW

"Hardware/Programming board" Folder
===================================
Contains schematic of a programming board adapter

"Hardware/BBQ" Folder
=====================
Contains schematic and board design for BBQ breakout board to evaluate TMC chipsets

"Hardware/TMC2100" Folder
=========================
Contains startup instructions for TMC2100 breakout board and schematics + board design


LJ

The T-Bone Team
2014