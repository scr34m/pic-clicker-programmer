#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <libusb-1.0/libusb.h>

#include "types.h"
#include "ihex/kk_ihex_read.h"

libusb_device_handle *handle = NULL;
int kernelDriverDetached     = 0;
unsigned char *buff_write;
unsigned char *buff_read;

void dump(const void* data, size_t size);
int usb_write(unsigned char *data, int len);
int usb_read(unsigned char *data);
static int LIBUSB_CALL hotplug_callback(libusb_context *ctx, libusb_device *dev, libusb_hotplug_event event, void *user_data);

typedef struct ihex_block {
    char *buff;
    int length;
    unsigned long address;
    struct ihex_block * next;
} ihex_block_t;

ihex_block_t *ihex_head = NULL;
ihex_block_t *ihex_last = NULL;

ihex_bool_t ihex_data_read(struct ihex_state *ihex, ihex_record_type_t type, ihex_bool_t error) {
    if (error) {
        (void) fprintf(stderr, "Checksum error\n");
        exit(1);
    }
    if ((error = (ihex->length < ihex->line_length))) {
        fprintf(stderr, "Line length error\n");
        exit(1);
    }
    if (type == IHEX_EXTENDED_LINEAR_ADDRESS_RECORD) {
        ihex_block_t *block;
        block = malloc(sizeof(ihex_block_t));
        block->buff = malloc(65*1024);
        memset(block->buff, 0xFF, 65*1024);
        block->address = (((ihex_address_t) ihex->data[0]) << 24) | (((ihex_address_t) ihex->data[1]) << 16);
        block->next = ihex_last;

        if (ihex_head == NULL) {
            ihex_head = block;
        }
        
        if (ihex_last != NULL) {
            ihex_last->next = block;
        }
    
        ihex_last = block;
        ihex_last->next = NULL;

    } else if (type == IHEX_DATA_RECORD) {
        unsigned long address = (unsigned long) IHEX_LINEAR_ADDRESS(ihex);
        memcpy(ihex_last->buff + (address - ihex_last->address), ihex->data, ihex->length);
        ihex_last->length += ihex->length;
    } else if (type == IHEX_END_OF_FILE_RECORD) {
        // nothing to do
    }
    return 1;
}

int ihex_open(const char *filename)
{
    struct ihex_state ihex;
    FILE *infile;
    ihex_count_t count;
    char buf[256];

    infile = fopen (filename, "r");
    if (infile == NULL) {
        fprintf (stderr, "hex open error");
        return -1;
    }

    ihex_read_at_address(&ihex, 0);
    while (fgets(buf, sizeof(buf), infile)) {
        count = (ihex_count_t) strlen(buf);
        ihex_read_bytes(&ihex, buf, count);
    }
    ihex_end_read(&ihex);

    fclose(infile);

    return 0;
}

int main(int argc, char*argv[])
{
    libusb_hotplug_callback_handle hp[2];

    if (argc > 4 || argc <= 1) {
        printf("usage: %s <hex file> [vid, default 0x1234] [pid, default 0x0001]\n", argv[0]);
        return -1;
    }

    int rc = ihex_open(argv[1]);
    if (rc < 0) {
        return -1;
    }

    int vendor_id  = (argc > 2) ? (int)strtol (argv[2], NULL, 0) : 0x1234;
    int product_id = (argc > 3) ? (int)strtol (argv[3], NULL, 0) : 0x0001;

    rc = libusb_init (NULL);
    if (rc < 0)
    {
        printf("failed to initialise libusb: %s\n", libusb_error_name(rc));
        return -1;
    }

    if (!libusb_has_capability (LIBUSB_CAP_HAS_HOTPLUG)) {
        printf ("Hotplug capabilites are not supported on this platform\n");
        libusb_exit (NULL);
        return -1;
    }

    rc = libusb_hotplug_register_callback (NULL, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, 0, vendor_id, product_id, LIBUSB_HOTPLUG_MATCH_ANY, hotplug_callback, NULL, &hp[0]);
    if (LIBUSB_SUCCESS != rc) {
        fprintf (stderr, "Error registering callback 0\n");
        libusb_exit (NULL);
        return -1;
    }

    // TODO check already attached devices
    // TODO handle detach
    
    printf("[-] Waiting for the device 0x%04x:0x%04x\n", vendor_id, product_id);

    rc = libusb_handle_events (NULL);
    if (rc < 0)
        printf("libusb_handle_events() failed: %s\n", libusb_error_name(rc));

    buff_write = (unsigned char *)calloc(64, 1);
    buff_read = (unsigned char *)calloc(64, 1);

    printf("[-] Get bootloader mode\n");
    buff_write[0] = STX;
    buff_write[1] = cmdINFO;
    usb_write(buff_write, 2);
    usb_read(buff_read);
    TBootInfo BootInfo;
    memcpy(&BootInfo, buff_read, sizeof(BootInfo));
    /*
    BootInfo packet detailed:
    size: 2B
    type: 01, value: 03
    type: 08, value: F8 FF 01 00
    type: 03, value: 00 04
    type: 04, value: 40 00
    type: 05, value: 00 13
    type: 06, value: 00 E0 01 00
    type: 07, value: 50 49 43 20 43 6C 69 63 6B 65 72 00 00 00 00 00 00 00 00 00
    */
    printf("    McuType (%d): %d\n", BootInfo.bMcuType.fFieldType, BootInfo.bMcuType.fValue);
    printf("    McuSize (%d): %d\n", BootInfo.ulMcuSize.fFieldType, BootInfo.ulMcuSize.fValue);
    printf("    EraseBlock (%d): %d\n", BootInfo.uiEraseBlock.fFieldType, BootInfo.uiEraseBlock.fValue);
    printf("    WriteBlock (%d): %d\n", BootInfo.uiWriteBlock.fFieldType, BootInfo.uiWriteBlock.fValue);
    printf("    BootRev (%d): %d\n", BootInfo.uiBootRev.fFieldType, BootInfo.uiBootRev.fValue);
    printf("    BootStart (%d): %d\n", BootInfo.ulBootStart.fFieldType, BootInfo.ulBootStart.fValue);
    printf("    DevDsc (%d): %s\n", BootInfo.sDevDsc.fFieldType, BootInfo.sDevDsc.fValue);

    printf("[-] Set bootloader mode\n");
    buff_write[0] = STX;
    buff_write[1] = cmdBOOT;
    usb_write(buff_write, 2);
    usb_read(buff_read);
    if (buff_read[0] != STX || buff_read[1] != cmdBOOT) {
        fprintf (stderr, "Error entering bootload mode\n");
        libusb_exit (NULL);
        return -1;
    }

    printf("[-] Sync PC\n");
    buff_write[0] = STX;
    buff_write[1] = cmdSYNC;
    usb_write(buff_write, 2);
    usb_read(buff_read);
    if (buff_read[0] != STX || buff_read[1] != cmdSYNC) {
        fprintf (stderr, "Error sync PC\n");
        libusb_exit (NULL);
        return -1;
    }

    int i = 0;
    uint16_t count;
    uint16_t length;
    int blocks;

    uint8_t unk1;
    uint8_t unk2;
    uint8_t unk3;
    uint8_t unk4;

    ihex_block_t *current = ihex_head;
    while (current != NULL) {
        if (current->address == 0) {
            count = 1;

            printf("[-] Erasing...\n");
            buff_write[0] = STX;
            buff_write[1] = cmdERASE;
            buff_write[2] = (current->address & 0xff); // start addr 4 byte
            buff_write[3] = (current->address >> 8 & 0xff);
            buff_write[4] = (current->address >> 16 & 0xff);
            buff_write[5] = (current->address >> 24 & 0xff);
            buff_write[6] = (count & 0xff); // erase block count
            buff_write[7] = (count >> 8 & 0xff);
            usb_write(buff_write, 8);
            usb_read(buff_read);
            if (buff_read[0] != STX || buff_read[1] != cmdERASE) {
                fprintf (stderr, "Error Erasing\n");
                libusb_exit (NULL);
                return -1;
            }

            count = (int) ceil((float)current->length / BootInfo.uiWriteBlock.fValue);
            length = BootInfo.uiWriteBlock.fValue * count;

            printf("[-] Writing...\n");
            buff_write[0] = STX;
            buff_write[1] = cmdWRITE;
            buff_write[2] = (current->address & 0xff); // start addr 4 byte
            buff_write[3] = (current->address >> 8 & 0xff);
            buff_write[4] = (current->address >> 16 & 0xff);
            buff_write[5] = (current->address >> 24 & 0xff);
            buff_write[6] = (length & 0xff); // data length
            buff_write[7] = (length >> 8 & 0xff);
            usb_write(buff_write, 8);

            unk1 = current->buff[0];
            current->buff[0] = 0x00;
            unk2 = current->buff[1];
            current->buff[1] = 0xEF;
            unk3 = current->buff[2];
            current->buff[2] = 0xF0;
            unk4 = current->buff[3];
            current->buff[3] = 0xF0;

            blocks = length / 64;
            for (i=0;i<blocks;i++) {
                memcpy(buff_write, current->buff + (i * 64), 64);
                usb_write(buff_write, 64);
            }

            usb_read(buff_read);
            if (buff_read[0] != STX || buff_read[1] != cmdWRITE) {
                fprintf (stderr, "Error writing\n");
                libusb_exit (NULL);
                return -1;
            }
        }
        current = current->next;
    }

    count = 1;

    printf("[-] Erasing...\n");
    buff_write[0] = STX;
    buff_write[1] = cmdERASE;
    buff_write[2] = 0x00; // lo, start addr 4 byte
    buff_write[3] = 0xDC; // hi
    buff_write[4] = 0x01; // higher
    buff_write[5] = 0x00; // highest
    buff_write[6] = (count & 0xff); // erase block count
    buff_write[7] = (count >> 8 & 0xff);
    usb_write(buff_write, 8);
    usb_read(buff_read);
    if (buff_read[0] != STX || buff_read[1] != cmdERASE) {
        fprintf (stderr, "Error Erasing\n");
        libusb_exit (NULL);
        return -1;
    }

    length = BootInfo.uiEraseBlock.fValue * count;

    printf("[-] Writing...\n");
    buff_write[0] = STX;
    buff_write[1] = cmdWRITE;
    buff_write[2] = 0x00; // lo, start addr 4 byte
    buff_write[3] = 0xDC; // hi
    buff_write[4] = 0x01; // higher
    buff_write[5] = 0x00; // highest
    buff_write[6] = (length & 0xff); // data length
    buff_write[7] = (length >> 8 & 0xff);
    usb_write(buff_write, 8);

    blocks = length / 64;
    memset(buff_write, 0xFF, 64);
    for (i=0;i<blocks;i++) {
        if (i + 1 == blocks ) {
            buff_write[60] = unk1;
            buff_write[61] = unk2;
            buff_write[62] = unk3;
            buff_write[63] = unk4;
        }
        usb_write(buff_write, 64);
    }

    usb_read(buff_read);
    if (buff_read[0] != STX || buff_read[1] != cmdWRITE) {
        fprintf (stderr, "Error writing\n");
        libusb_exit (NULL);
        return -1;
    }

    printf("[-] Reboot device\n");
    buff_write[0] = STX;
    buff_write[1] = cmdREBOOT;
    usb_write(buff_write, 2);
    // no ack

    libusb_release_interface(handle, 0);
    libusb_close (handle);
    libusb_exit (NULL);

    return 0;
}

void dump(const void* data, size_t size) {
    char ascii[17];
    size_t i, j;
    ascii[16] = '\0';
    for (i = 0; i < size; ++i) {
        printf("%02X ", ((unsigned char*)data)[i]);
        if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
            ascii[i % 16] = ((unsigned char*)data)[i];
        } else {
            ascii[i % 16] = '.';
        }
        if ((i + 1) % 8 == 0 || i + 1 == size) {
            printf(" ");
            if ((i + 1) % 16 == 0) {
                printf("|  %s \n", ascii);
            } else if (i + 1 == size) {
                ascii[(i + 1) % 16] = '\0';
                if ((i + 1) % 16 <= 8) {
                    printf(" ");
                }
                for (j = (i + 1) % 16; j < 16; ++j) {
                    printf("   ");
                }
                printf("|  %s \n", ascii);
            }
        }
    }
}

int usb_write(unsigned char *data, int len)
{
    int actual_length;

    int r = libusb_interrupt_transfer(handle, 1, data, len, &actual_length, 0);
    if (r < 0) {
        printf("Write error: %s\n", libusb_error_name(r));
        return r;
    } else {
        return actual_length;
    }
}

int usb_read(unsigned char *data)
{
    int actual_length;

    int r = libusb_interrupt_transfer(handle, 0x81, data, 64, &actual_length, 0);
    if (r < 0) {
        printf("Read error: %s\n", libusb_error_name(r));
        return r;
    } else {
        return actual_length;
    }
}

static int LIBUSB_CALL hotplug_callback(libusb_context *ctx, libusb_device *dev, libusb_hotplug_event event, void *user_data)
{
    struct libusb_device_descriptor desc;
    int rc;

    rc = libusb_get_device_descriptor(dev, &desc);
    if (LIBUSB_SUCCESS != rc) {
        fprintf (stderr, "Error getting device descriptor\n");
    }

    printf ("Device attached: %04x:%04x\n", desc.idVendor, desc.idProduct);

    if (handle) {
        libusb_close (handle);
        handle = NULL;
    }

    rc = libusb_open (dev, &handle);
    if (LIBUSB_SUCCESS != rc) {
        fprintf (stderr, "Error opening device\n");
    }

    // Check whether a kernel driver is attached to interface #0. If so, we'll need to detach it.
    /* linux only
    if (libusb_kernel_driver_active(handle, 0)) {
        printf("A\n");
        rc = libusb_detach_kernel_driver(handle, 0);
        if (rc == 0) {
        printf("B\n");
            kernelDriverDetached = 1;
        } else {
            fprintf(stderr, "Error detaching kernel driver.\n");
            return 1;
        }
    }
    */

    rc = libusb_set_configuration(handle, 1);
    if (rc < 0) {
        fprintf(stderr, "libusb_set_configuration error %d\n", rc);
        return 1;
    }

    // Claim interface #0
    rc = libusb_claim_interface(handle, 0);
    if (rc != 0) {
        fprintf(stderr, "Error claiming interface %d\n", rc);
        return 1;
    }

    return 0;
}