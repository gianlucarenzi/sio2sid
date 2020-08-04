# sio2sid

This is a semi-working demo of a SID Emulator using two PWM channels with Arduino UNO R3 and the StereoSID library.
It works good so far. Via serial (using a semi-emulated SIO protocol) it can receive SIO COMMAND FRAMES, DATA FRAMES and manage the SIO Protocol. (Or it should do that).
It works under Linux and the serial usb data with the Arduino UNO Port @ 19200.
The sio protocol emulator works fairly good (I checked the transmission/receving stuff using a Saleae Logic Signal Analyzer), so when attaching the real SIO data, I suppose it will work... Or I hope so. ;-)

Any help will be appreciated.

From the concept point of view, all 25 SID registers are emulated. Using some SID Dumper or similar stuff (look at the comment on the Arduino Driver) it is possible to play chip tunes updating all registers in a vertical blank period of 20 msecs (50 Hz PAL). The serial port speed for Atari is limited at 19200 bps, and a SIO data sent of 25 registers (plus a header and two checksums) so barely 31 bytes could be done in a little less of 15/16 msecs. So it is quite time critical... With a modified SIO routines ROM or a modified OS in RAM (XL/XE computers can do that), the speed improvement can be up to 57.6k or even 121kb/sec!!!

But for now this is an experimental project and when the ATARI 130XE will play the chiptunes, the goal will be reached.

After that, if there is enough request I can start to build up a custom board, but the goal will be another: programming the SID registers as in a working address space area, using the SPI65 project based on STM32F429 MCU... But this is another story...

Best Regards,
Gianluca GP Renzi
