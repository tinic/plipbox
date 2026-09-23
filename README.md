plipbox
=======

Fork status
-----------

This fork adds a GCC-built Amiga driver and selective Ethernet multicast
reception for IPv6 neighbour discovery. IPv6 needs the matching fork firmware
and driver: the original 0.6 firmware does not program multicast memberships.
The multicast filter is populated by the driver's SANA-II join/leave requests;
the firmware admits only the selected hash buckets rather than all multicast
traffic. The original plipbox licence and attribution below remain in force.

The firmware's serial command `r` performs a soft reset. From the Amiga,
take the interface offline and online to re-establish its session; there is
currently no in-band hardware reset or firmware-update command. Flash over
the board's serial bootloader or ISP, following the hardware-specific
instructions in [Firmware](doc/src/firmware.md). Keep a verified backup of
the working firmware before flashing. Do not flash while the adapter is
connected to a powered Amiga parallel port.

GCC driver builds use LTO by default. They require a GCC 16.2.0b cross
toolchain with the HUNK LTO plugin fixes (tested with package revision 16.2.1);
older HUNK linkers can silently omit LTO code. The build treats linker warnings
as errors. It keeps LTO and `LTO=0` objects and outputs separate. Clean before
each build. The
68000 LTO driver measured 9,296 bytes versus 10,124 bytes without LTO on
this source revision; a live A3000 test passed IPv4 and IPv6 with the LTO
image. This is a modest code-size saving, not a measured performance gain.

AVR firmware builds also use LTO by default (`LTO=0` disables it). Use the
[AVR-GCC 16.1.0 bundle](https://github.com/ZakKemble/avr-gcc-build/releases/tag/v16.1.0-1)
with AVR binutils 2.46.1 and avr-libc 2.3.2. Verify its published SHA-256
before extraction (Linux x64 archive:
`8621ecc6514df50202b58b23b0f8f72f0e535ec35ee40426194e9a15c57692f6`).
From `avr/src`, run `make clean` before each build,
then `make BOARD=nano AVR_TOOLCHAIN=/path/to/avr-gcc-16.1.0-x64-linux`.
The compiler/linker bundle and its matching headers are selected together by
`AVR_TOOLCHAIN`. Firmware output names carry `-lto` when LTO is enabled.
With this source revision, the Nano image is 12,565 bytes of flash with LTO
versus 14,533 without; static SRAM use is 1,789 versus 1,806 bytes. The
ATmega328P still has only 259 bytes beyond static allocation for the stack,
so a build-size check alone is not a firmware stability test.

plipbox is an Arduino-based device that allows to connect low-end classic
Amigas via Ethernet to your local network. It bridges IP traffic received
via PLIP on the parallel port of the Amiga to the Ethernet port attached
to the Arduino.

Copyright (C) 2012-2015 Christian Vogelgsang <chris@vogelgsang.org>

Released under the GNU Public License V2 (see COPYING for details)

Introduction
------------

With the [plip2slip][1] project I already presented a device that uses a cheap
AVR 8 bit microcontroller (as found on the popular Arduino boards) to bridge
network traffic from the Amiga's parallel port (with the [MagPLIP][2] protocoll)
to another machine via a fast serial link.

plipbox extends the plip2slip project and replaces the serial link for IP traffic
with an on-board Ethernet port. This allows you to connect your Amiga directly
to your local network without any other machine assisting.

With the on-board Ethernet port the plipbox HW is more complex than the plip2slip
HW, but I tried to use common and easy available HW modules to simplify the
recreation of this device. This allows even novice users to build their own
plipbox. (See the hardware document for details).

The firmware for plipbox is open-source and hosted on [GitHub][3].
Clone this repository if you want to build the firmware yourself or if you
want to play around with it. 

[1]: http://lallafa.de/blog/amiga-projects/plip2slip/
[2]: http://aminet.net/package/comm/net/magPLIP38.1
[3]: https://github.com/cnvogelg/plipbox

Download Releases
-----------------

See my [plipbox blog page][4] for downloads of the current release archives.
These archives contain pre-built firmware and Amiga driver binaries in addtion
to the source code here.

[4]: http://lallafa.de/blog/amiga-projects/plipbox/

Documentation
-------------

 - [Change Log](ChangeLog.md): Changes in the releases
 - [Introduction](doc/src/intro.md): Introduction on plipbox
 - [Benchmarks](doc/src/benchmark.md): Performance measurements
 - [Hardware](doc/src/hardware.md): How to build the device hardware
 - [Firmware](doc/src/firmware.md): How to setup the firmware
 - [Amiga Setup](doc/src/amiga.md): How to setup plipbox.device on your Amiga
 - [Python Emulator](doc/src/python.md): A plipbox emulator if you run your Amiga in FS-UAE
