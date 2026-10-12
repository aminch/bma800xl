# Third-Party Software

BMA800XL incorporates software and other materials from third-party projects. Third-party components remain subject to their respective copyright and license terms.

The original copyright, license, and attribution notices supplied with these components are retained with their source where applicable.

## atari800

BMA800XL incorporates the [atari800 project](https://github.com/atari800/atari800), version 7.2.1.

See [`third_party/atari800/`](https://github.com/atari800/atari800/tree/ATARI800_7_2_1) for the version used by BMA800XL and its accompanying copyright and license information.

## Altirra OS and Altirra BASIC

BMA800XL incorporates the Altirra OS and Altirra BASIC ROM replacements by Avery Lee, as included with atari800.

See [`src/roms/`](https://github.com/atari800/atari800/tree/ATARI800_7_2_1/src/roms) in atari800 for the files and their accompanying copyright and license information.

## circle-stdlib

BMA800XL uses the [circle-stdlib project](https://github.com/smuehlst/circle-stdlib), v21, which includes [Circle](https://github.com/rsta2/circle) by Rene Stange and the newlib C library.

See [`third_party/circle-stdlib/`](https://github.com/smuehlst/circle-stdlib/tree/v21) for the version used by BMA800XL and its accompanying copyright and license information.

## BMC64

BMA800XL incorporates source code from the [BMC64 project](https://github.com/randyrossi/bmc64) by Randy Rossi and contributors.

See [BMC64](https://github.com/randyrossi/bmc64) for its source and its accompanying copyright and license information.

## Raspberry Pi Firmware

BMA800XL includes the Raspberry Pi boot firmware (`bootcode.bin`, `start.elf`, `fixup.dat`, and for the Raspberry Pi 4 `start4.elf`, `fixup4.dat` and its device trees) from the [Raspberry Pi firmware repository](https://github.com/raspberrypi/firmware).

See the [Raspberry Pi firmware repository](https://github.com/raspberrypi/firmware/tree/master/boot) for its accompanying copyright and license information.

## crt-pi

BMA800XL uses the crt-pi CRT shader by davej, as included in BMC64, under the [GNU General Public License version 2 or later](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html).

## Circle ARM stub

For the Raspberry Pi 4, BMA800XL includes `armstub8-rpi4.bin`, built from [Circle's `boot/armstub`](https://github.com/rsta2/circle/tree/master/boot/armstub) source, under Circle's [GNU General Public License version 3](https://github.com/rsta2/circle/blob/master/COPYING).

## Wi-Fi Firmware

BMA800XL includes the Wi-Fi firmware for the Raspberry Pi's wireless chip from the [RPi-Distro firmware-nonfree repository](https://github.com/RPi-Distro/firmware-nonfree), downloaded when BMA800XL is built.

See the `firmware/` folder on the SD card for the Broadcom, Cypress and Synaptics licenses that accompany it.

## Other Third-Party Components

Additional third-party components and supporting materials may be present in the BMA800XL source tree. Where such material is accompanied by its own copyright, license, or attribution notices, those notices and terms continue to apply.
