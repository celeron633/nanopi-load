#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libusb.h>
#include <errno.h>
#include "s5p_usb.h"
#include "s5p_boot.h"


typedef unsigned char uint8;
typedef unsigned int  uint32;

static unsigned char mem[10 * 1024 * 1024];

static void write32LE(uint8 *addr, uint32 data)
{
	addr[0] = (uint8)(data & 0xff);
	addr[1] = (uint8)((data >> 8) & 0xff);
	addr[2] = (uint8)((data >> 16) & 0xff);
	addr[3] = (uint8)((data >> 24) & 0xff);
}

static unsigned read32LE(const uint8 *addr)
{
    return addr[0] + (addr[1] << 8) + (addr[2] << 16) + (addr[3] << 24);
}

static void initBootloaderHeader(uint8 *buf, int is64bitBoot,
        int isArtik, unsigned size, unsigned loadAddr, unsigned launchAddr)
{
    static const unsigned arm64bootcode[15] = {
        0xE59F0030, // ldr r0, =0xc0011000
        0xE590113C, // ldr r1, [r0, #0x13c]
        0xE3811A0F, // orr r1, r1, #0xf000
        0xE580113C, // str r1, [r0, #0x13c]
        0xE59F1024, // ldr r1, =#0x3fffc080  @ launchAddr >> 2
        0xE5801140, // str r1, [r0, #0x140]
        0xE5101D54, // ldr r1, [r0, #-0xd54] @ 0cx00102ac
        0xE3811001, // orr r1, r1, #1
        0xE5001D54, // str r1, [r0, #-0xd54] @ 0cx00102ac
        0xE5901138, // ldr r1, [r0, #0x138]
        0xE381160F, // orr r1, r1, #0xf00000
        0xE5801138, // str r1, [r0, #0x138]
        0xE320F003, // wfi
        0xEAFFFFFE, // b   .
        0xC0011000
    };
    unsigned i;

    if( is64bitBoot ) {
        for(i = 0; i < sizeof(arm64bootcode) / sizeof(arm64bootcode[0]); ++i)
            write32LE(buf + 4 * i,  arm64bootcode[i]);
        write32LE(buf + 4 * i, launchAddr >> 2);
        launchAddr = loadAddr;  // launch the CPU reset
    }
    if( isArtik ) {
        write32LE(buf + 0x50, size);        // load size
        write32LE(buf + 0x58, loadAddr);    // load address
        write32LE(buf + 0x60, launchAddr);  // launch address
    }else{
        write32LE(buf + 0x44, size);        // load size
        write32LE(buf + 0x48, loadAddr);    // load address
        write32LE(buf + 0x4c, launchAddr);  // launch address
    }
	memcpy(buf + 0x1fc, "NSIH", 4);     // signature
}

static int checkBootloaderHeader(uint8 *buf, unsigned readSize,
        int isArtik, int isVerbose, int fixSize)
{
    unsigned size;
    int res = !fixSize;
    unsigned headerSize, sizeOffset, loadAddrOffset, launchAddrOffset;

    if( isArtik ) {
        headerSize = 1024;
        sizeOffset = 0x50;
        loadAddrOffset = 0x58;
        launchAddrOffset = 0x60;
    }else{
        headerSize = 512;
        sizeOffset = 0x44;
        loadAddrOffset = 0x48;
        launchAddrOffset = 0x4c;
    }
    if( readSize >= headerSize ) {
        if( !memcmp(buf + 0x1fc, "NSIH", 4) ) {
            size = read32LE(buf + sizeOffset);
            if( fixSize )
                write32LE(buf + sizeOffset, readSize - headerSize);
            if( isVerbose ) {
                printf("Boot Header:\n");
                printf("  size:           %u ", size);
                if( size == readSize - headerSize )
                    printf("(OK)\n");
                else{
                    printf("(real: %u%s)\n", readSize - headerSize,
                            fixSize ? ", fixed" : "");
                }
                printf("  load address:   0x%x\n",
                        read32LE(buf + loadAddrOffset));
                printf("  launch address: 0x%x\n",
                        read32LE(buf + launchAddrOffset));
                printf("  signature:      OK\n");
            }else{
                if( !fixSize && size != readSize - headerSize ) {
                    printf("warning: wrong load size in header: %u, real: %u\n",
                            size, readSize - headerSize);
                }
            }
            res = 1;
        }else{
            printf("%s: bad signature in header\n",
                    fixSize ? "error" : "warning");
        }
    }else{
        printf("%s: read data size (%u) is less than boot header size\n",
                fixSize ? "error" : "warning", readSize);
    }
    return res;
}

static int readBin(uint8 *buf, unsigned bufsize, const char *filepath)
{
	FILE *fin;
	int size;

    if( !strcmp(filepath, "-") )
        fin = stdin;
    else{
        fin = fopen(filepath, "rb");
        if (fin == NULL) {
            printf("unable to open %s: %s\n", filepath,
                    strerror(errno));
            return -1;
        }
    }
	size = fread(buf, 1, bufsize, fin);
	if( fin != stdin )
		fclose(fin);
	if(size < 0) {
        fprintf(stderr, "fread: %s\n", strerror(errno));
		return size;
    }

	// Size must be a multiple of 16 bytes
	if (size % 16 != 0)
		size = ((size / 16) + 1) * 16;
	return size;
}

static int usbBoot(uint8 *buf, int size, int isVerbose)
{
	libusb_context        *ctx;
	libusb_device_handle  *dev_handle;

	int ret, boot_ret = 1;
	int transferred;

	ret = libusb_init(&ctx);
	if(ret < 0) {
        fprintf(stderr, "libusb_init: %s\n", libusb_strerror(ret));
		return 1;
    }
	dev_handle = libusb_open_device_with_vid_pid(ctx, S5P6818_VID, S5P6818_PID);
	if(dev_handle != NULL) {
        if((ret = libusb_claim_interface(dev_handle, S5P6818_INTERFACE)) == 0) {
            if( isVerbose )
                printf("Start transfer, size=%d\n", size);
            ret = libusb_bulk_transfer(dev_handle, S5P6818_EP_OUT, buf,
                    size, &transferred, 0);
            if (ret >= 0) {
                if( isVerbose )
                    printf("Finish transfer, transferred=%d\n", transferred);
                if (size == transferred) {
                    if( isVerbose )
                        printf("OK\n");
                }else
                    fprintf(stderr, "short load: size=%d, transferred=%d\n",
                            size, transferred);
                boot_ret = 0;
            }else{
                fprintf(stderr, "libusb_bulk_transfer: %s\n",
                        libusb_strerror(ret));
            }
            libusb_release_interface(dev_handle, 0);
        }else{
            fprintf(stderr, "libusb_claim_interface: %s\n",
                    libusb_strerror(ret));
        }
    }else{
        fprintf(stderr, "failed to open device\n");
    }
	libusb_close(dev_handle);
	libusb_exit(ctx);
    return boot_ret;
}

static void usage(void)
{
    printf("\n"
"usage: nanopi-load [-Acfhivx] <bootloader.bin> [<loadaddr> [<startaddr>]]\n"
"\n"
"options:\n"
"  -A - Boot Header format for Samsung ARTIK boot loader\n"
"  -c - dry run (implies -v)\n"
"  -f - fix load size in Boot Header\n"
"  -h - print this help\n"
"  -i - embed Boot Header in first 512 bytes read\n"
"  -v - be verbose\n"
"  -x - add code for iROMBOOT to Boot Header that switches to 64-bit\n"
"\n"
"If <loadaddr> is specified, image is prepended with 512-byte Boot Header\n"
"If <startaddr> is not specified, assume <loadaddr> + 0x200\n"
"If <bootloader.bin> is \"-\", stdin is read\n"
    "\n");
}

int main(int argc, const char *argv[])
{
	int offset, size, argBin = 1;
    int is64bitBoot = 0, isInPlace = 0, dryRun = 0, isVerbose = 0, fixSize = 0;
    int isArtik = 0;
	unsigned int load_addr;
	unsigned int launch_addr;

    if( argc == 1 ) {
        usage();
        return 0;
    }
    while( argBin < argc && argv[argBin][0] == '-' && argv[argBin][1] ) {
        for(int i = 1; argv[argBin][i]; ++i) {
            switch( argv[argBin][i] ) {
            case 'A':
                isArtik = 1;
                break;
            case 'c':
                dryRun = 1;
                isVerbose = 1;
                break;
            case 'f':
                fixSize = 1;
                break;
            case 'h':
                usage();
                return 0;
            case 'i':
                isInPlace = 1;
                break;
            case 'v':
                isVerbose = 1;
                break;
            case 'x':
                is64bitBoot = 1;
                break;
            default:
                printf("error: unrecognized option -%c\n", argv[argBin][i]);
                return 1;
            }
        }
        ++argBin;
    }
    if( argc == argBin ) {
        printf("error: input file not provided\n");
        return 1;
    }
    if( argc == argBin + 1 )
        offset = 0;
    else
        offset = isArtik ? 0x400 : 0x200;
    if( isInPlace ) {
        size = readBin(mem, sizeof(mem), argv[argBin]);
        if( size < 0 )
            return 1;
        if( size < offset ) {
            printf("error: input file too short\n");
            return 1;
        }
        size -= offset;
    }else{
        memset(mem, 0, offset);
        size = readBin(mem + offset, sizeof(mem) - offset, argv[argBin]);
        if( size < 0 )
            return 1;
    }
    if( argc > argBin + 1 ) {
        load_addr = strtoul(argv[argBin+1], NULL, 16);
        launch_addr = load_addr + offset;
        if( argc > argBin + 2 )
            launch_addr = strtoul(argv[argBin + 2], NULL, 16);
        initBootloaderHeader(mem, is64bitBoot, isArtik, size,
                load_addr, launch_addr);
        if( isVerbose ) {
            printf("%s Boot Header: load=0x%x, launch=0x%x size=%u%s\n",
                    isInPlace ? "Embedded" : "Added", load_addr, launch_addr,
                    size, is64bitBoot ? " 64-bit" : "");
        }
        size += offset;
        offset = 0;
    }else{
        if( ! checkBootloaderHeader(mem, size, isArtik, isVerbose, fixSize) )
            return 1;
    }
	return dryRun ? 0 : usbBoot(mem, size, isVerbose);
}

