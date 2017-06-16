# nanopi-load
## Program for booting NanoPi M3 from USB.

Program allows to boot NanoPi M3 using USB connection instead of fusing
SD card. It allows to upload through USB a _bl1_ boot loader (for boot stage 1),
_u-boot_ (for boot stage 2) and then Linux kernel.

To perform the uploads, device must be connected to PC by micro-USB port (such
used for powering device). To play with boot loaders it is also good to have
connected serial console.

Boot loaders on _s5p6818_ are using some common protocol. The protocol
specifies 512-byte NSIH header which is sent before image. The NSIH header
contains various information used for device initialization, but, in fact, only
original _bl1_ firmware from Samsung utilizes all the data from header. Other
boot loaders need at most three fields in this header, namely:

 * load address      - destination address of the loaded data
 * load size         - number of bytes to load
 * start address     - where to jump to after load

The _nanopi-load_ program can create NSIH header on the fly.

## compilation

Ensure libusb-1.0 development package is installed. On Debian/Ubuntu please
install _libusb-1.0-0-dev_.

After package installation run _make_.

## usage

The command usage is as follows:

        nanopi-load [options...] <bootloader.bin> [<loadaddr> [<startaddr>]]

Where:

        <bootloader.bin>  - the file to upload,
        <loadaddr>        - the image load address
        <startaddr>       - the image start address

When the command is invoked with file parameter only, the file is
uploaded "as is", without any additional data. When load address is also
specified, the command inserts NSIH header before image. Note that
_&lt;loadaddr&gt;_ parameter value is stored in NSIH header exactly as
specified.  Some boot loaders are loading NSIH header at this address.  In this
case actual image is loaded 512 bytes further. It depends on loader currently
running on the device. Don't be surprised.

When _&lt;startaddr&gt;_ parameter is not specified, it defaults to
_&lt;loadaddr&gt; + 0x200_

Most of the options are described below.

### boot ROM

If boot stage 1 image (_bl1_) cannot be read by boot ROM from some reason
(for example, SD card slot is empty), the boot ROM attempts to read image
from USB. Loaded image is stored always at address `0xFFFF0000`. To be
precise, NSIH header is stored at this address. Actual code is stored at
address `0xFFFF0200`.

Start address is honored by boot ROM, i.e. code jumps to the address provided.

### bl1 loader

The boot ROM loads _bl1_ loader. FriendlyARM ships the file as _2nboot.bin_.
This loader may be compiled to load next stage image from USB. There is also
[a boot loader alternative provided by _metro94_
](https://github.com/metro94/s5p6818\_bootloader), which may be used to
load image from USB.

These boot loaders are initializing DDR RAM and then are loading _u-boot_.
Samsung boot loader stores NSIH header at _load address_ specified in
command line - the _u-boot_ image is stored 512 bytes further.  On the other
hand, boot loader provided by _metro94_ stores _u-boot_ image at the
_load address_ - NSIH header is discarded.

### u-boot

_u-boot_ for NanoPi M3 (also
[u-boot-artik](https://github.com/SamsungARTIK/u-boot-artik))
has _udown_ command which allows to download images through USB.
Although _udown_ command has _load address_ parameter, but the _u-boot_ needs
NSIH header anyway, as of image size is read from it. The _nanopi-load_
command can be used to upload the image. To add NSIH header, some load
address should be provided. It is ignored by u-boot, but the _nanopi-load_
calculates also the load size and stores in header. Start address is also
stored in NSIH header but it is also ignored by u-boot.

Note that _u-boot_ stores image without NSIH header at the specified
address. The NSIH header is discarded.

The _nanopi-load_ command must be invoked after run the _udown_ command on
_u-boot_. When the _nanopi-load_ command finishes, the _udown_ command should
finish, too. If it does not finish, it means that something went wrong.

Note that NanoPi device is not visible by PC before invoke the _udown_
command. When the _udown_ command finishes, the NanoPi device disconnects.

Note also that sometimes PC is unable to enumerate the device when
the _udown_ command starts. I have encountered such problem
on _u-boot-artik_. It seems to be a bug in _u-boot_. I'm working on this ;)

### fixing load size

Sometimes the image to upload already contains NSIH header, for example it is a
boot loader image ready to embed on SD card. To upload such image,
 _&lt;loadaddr&gt;_ parameter should **not** be specified. But such images
have often load size value in NSIH header far too big. It is not a problem when
image is loaded from SD card.  Additional data read from SD card is simply
ignored.  But the big size is a problem when image is uploaded by USB.  Boot
loader hangs, waiting for more data. For such case _-f_ option may be specified
in _nanopi-load_ command line, which fixes the image size in NSIH header
before upload.

### dry run

Option _-c_ may be used for testing what will be actually done. The
_nanopi-load_ program invoked with this option performs all the preparations
but does not run the upload.

### 64-bit mode

First stage boot loader (started by boot ROM) is always started in 32-bit
mode. To switch into 64-bit mode, a special reset of the device must be
performed. 64-bit boot loaders perform such special reset in 32-bit mode
before jumping to the actual 64-bit code.  When _-x_ option is specified
in _nanopi-load_ command line, such special code is added to NSIH header, so it
is possible to compile 64-bit program and start it using the option without
care about the 32-bit piece of code performing reset.

Note that the code is added only when _nanopi-load_ program adds NSIH header
(i.e. load address parameter is specified). If the header is not added by
_nanopi-load_, switch is ignored.

### reading image from standard input

If minus sign ("-") is specified as image file, image is read from standard
input.

