# nanopi-load
A USB Loader for PCs used for booting secondary bootloader on NanoPi M3.

To build this project, libusb-1.0 library is needed.

## usage

Program allows to load through USB a _bl1_ boot loader (for boot stage 1),
u-boot (boot stage 2) and Linux kernel. To perform the upload, the device
must be connected to PC by micro-USB port (such used for powering device).

To play with the boot loaders it is good to have also a serial console.

The boot loader uses protocol common for these boot loaders used on s5p6818
platform. This protocol specifies a 512-byte NSIH header which is sent
before image. The header contains various information used for initialization,
but, in fact, only original _bl1_ firmware from samsung utilizes all the
data in header. Other boot loaders need at most three fields in
this header, namely:

	* load address      - destination address of the loaded data
	* load size         - number of bytes to load
	* start address     - where to jump to after load

The _nanopi-load_ program can create NSIH header automatically. It depends on
number of parameters provided. The command usage is as follows:

    nanopi-load [options...] <bootloader.bin> [<loadaddr> [<startaddr>]]

The _&lt;bootloader.bin&gt;_ is the file to upload. The _&lt;loadaddr&gt;_
is the image load address and _&lt;startaddr&gt;_ is the image start address.
When the command is invoked only with the file parameter, the file is
uploaded "as is". When the load address is provided, the commad adds
NSIH header before the image. Note that _&lt;loadaddr&gt;_ parameter
is embedded in header exactly as specified. Some boot loaders are loading
the added NSIH header at this address.  In this case the actual image
is loaded 512 bytes further. It depends on the loader currently running
on device. Don't be surprised.

When the _&lt;startaddr&gt;_ is not specified, it defaults to
_&lt;loadaddr&gt; + 0x200_


### boot stage 1

If the boot stage 1 image (_bl1_) cannot be read by boot ROM from some reason
(for example, the SD card slot is empty), the boot ROM attempts to read image
from USB. The loaded image is stored always at address `0xFFFF0000`. To be
precise, NSIH header is stored at this address. Actual code is stored at
address `0xFFFF0200`.

The start address is honored by the boot ROM, i.e. code jumps to the address
provided.

### u-boot

The u-boot for NanoPi M3 (also
[u-boot-artik](https://github.com/SamsungARTIK/u-boot-artik))
has _udown_ command which allows to download image through USB.
The _udown_ command has _load address_ parameter, but _u-boot_ needs the NSIH
header anyway, as of it reads the image size from it. The _nanopi-load_ command
can be used to upload the image. To add the NSIH header, the load address
shoule be provided (it is ignored by u-boot) but the load size is set
automatically by _nanopi-load_. Start address specified in NSIH header is also
ignored by the u-boot. 

Note that _u-boot_ stores actual image at the specified address. NSIH header
is discarded.

### fixing load size

Sometimes the image already contains NSIH header, for example it is
a boot loader image to embed into SD card. Such headers usually have 
load size set far too big. It is not a problem when the image is loaded
from SD card. The additional data read from SD card is simply ignored.
But the big size is a problem when the image is uploaded from USB.
The boot loader hangs, waiting for more data. For such case the _-f_
option may be specified in _nanopi-load_ command line, which fixes the
image size before upload.

### dry run

The _-c_ option may be used for testing what will be actually done.
It performs all the preparations but does not run the upload.

### 64-bit mode

The first stage boot loader (started by boot ROM) is always started in 32-bit
mode. To switch into 64-bit mode, a special reset of the device must be
performed. 64-bit boot loaders perform such special reset in 32-bit mode
before jumping to the actual 64-bit code.  When _-x_ option is specified, such
special code is added to NSIH header, so it is possible to compile a 64-bit
program and start it using the option without care about the 32-bit piece of
code performing the reset.

Note that the code is added only when _nanopi-load_ program adds NSIH header
(i.e. load address parameter is specified). If the header is not added by
_nanopi-load_ command, the switch is ignored.

### reading image from standard input

If the minus sign ("-") is specified as image file, the image is read from
standard input.

