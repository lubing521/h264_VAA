/*
 * Copyright (c) 2006-2008 Atheros Communications, Inc.
 * All rights reserved.
 * 
 *
 * 
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//
//
 * 
 */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/types.h>
#include <linux/if.h>
#include <linux/wireless.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#include <a_config.h>
#include <a_osapi.h>
#include "targetdef.h"
#include <wmi.h>
#include "athdrv_linux.h"
#include "bmi_msg.h"

/* Target-dependent addresses and definitions */
struct targetdef_s *targetdef;

/*
 * Access Board Data from Target-side EEPROM that is attached
 * to the Target via I2C Serial Interface.
 *
 * This implementation uses BMI to handle EEPROM read/write.
 * It is also possible to implement EEPROM access through the
 * Diagnostic Window.
 */

#if 0
#define DEBUG printf
#else
#define DEBUG(...)
#endif

#define MAC_ADDR_LEN              6

const char *prog_name;
A_UINT32 target_version;
A_UINT32 target_type;
unsigned char BMI_read_reg_cmd[8];
unsigned char BMI_write_reg_cmd[12];
unsigned char BMI_read_mem_cmd[12];
unsigned char BMI_write_mem_cmd[BMI_DATASZ_MAX+3*sizeof(A_UINT32)];
unsigned char mac_addr[MAC_ADDR_LEN];
unsigned int new_reg_val;
unsigned char new_reg[2];

char ifname[IFNAMSIZ];
int s; /* socket to Target */
struct ifreq ifr;

/* Command-line arguments specified */
A_BOOL read_specified = FALSE;
A_BOOL write_specified = FALSE;
A_BOOL file_specified = FALSE;
A_BOOL transfer_specified = FALSE;
A_BOOL force_specified = FALSE;
char *p_mac = NULL;
char *p_reg = NULL;

A_UINT32 sys_sleep_reg;
#define MAX_FILENAME 1023
#define EEPROM_WAIT_LIMIT 16 
char filename[MAX_FILENAME+1];
FILE *file;

const char cmd_args[] =
"arguments:\n\
  --read (read from EEPROM into a file)\n\
  --write (write from a file into EEPROM)\n\
  --transfer (read from EEPROM or file, then write to Target RAM)\n\
  --setmac (update mac addr in file, used with --transfer option)\n\
  --setreg (update reg data in file, used with --transfer option)\n\
  --file filename\n\
  --interface interface_name   (e.g. eth1)\n\
  --a16 (16 bit address, e.g. for AT24C512)\n\
  --Force\n\
\n";
void enable_SI(void);
void disable_SI(void);
void wait_for_eeprom_completion(void);

#define ERROR(args...) \
do { \
    fprintf(stderr, "%s: ", prog_name); \
    fprintf(stderr, args); \
    disable_SI(); \
    exit(1); \
} while (0)
A_UINT8 addr_width_16bit = 0;
A_UINT8 auto_detect_eeprom =1;
void
usage(void)
{
    fprintf(stderr, "usage:\n%s arguments...\n", prog_name);
    fprintf(stderr, "%s\n", cmd_args);
    exit(1);
}

/* Used with a --read operation -- open the output file for writing */
void
open_output_file(void)
{
    file = fopen(filename, "w+");
    if (!file) {
        ERROR("Cannot create/write output file, %s\n", filename);
    }
}

/* Used with a --write operation -- open the input file for reading */
void
open_input_file(void)
{
    file = fopen(filename, "r");
    if (!file) {
        ERROR("Cannot read input file, %s\n", filename);
    }
}

/* Wait for the Target to start. */
void
wait_for_target(void)
{
    unsigned char *buffer;

    DEBUG("wait_for_target\n");

    /* Verify that the Target is alive.  If not, wait for it. */
    {
        int rv;
        static int waiting_msg_printed = 0;

        buffer = (unsigned char *)malloc(12);
        ((int *)buffer)[0] = AR6000_XIOCTL_TARGET_INFO;
        ifr.ifr_data = (char *)buffer;
        while ((rv=ioctl(s, AR6000_IOCTL_EXTENDED, &ifr)) < 0)
        {
            if (errno == ENODEV) {
                /* 
                 * Give the Target device a chance to start.
                 * Then loop back and see if it's alive.
                 */
                if (!waiting_msg_printed) {
                    printf("%s is waiting for Target....\n", prog_name);
                    waiting_msg_printed = 1;
                }
                usleep(1000000); /* sleep 1 Second */
            } else {
                break; /* Some unexpected error */
            }
        }
        target_version = *((A_UINT32 *)(&buffer[0]));
        target_type = *((A_UINT32 *)(&buffer[4]));
        DEBUG("Target version/type is 0x%x/0x%x\n", target_version, target_type);
        free(buffer);
    }
    DEBUG("Target is ready.....proceed\n");
}

/* Read a Target register and return its value. */
void
BMI_read_reg(A_UINT32 address, A_UINT32 *pvalue)
{
    DEBUG("BMI_read_reg address=0x%x\n", address);

    ((int *)BMI_read_reg_cmd)[0] = AR6000_XIOCTL_BMI_READ_SOC_REGISTER;
    ((int *)BMI_read_reg_cmd)[1] = address;
    ifr.ifr_data = (char *)BMI_read_reg_cmd;
    if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0)
    {
        err(1, "%s", ifr.ifr_name);
    }
    *pvalue = ((int *)BMI_read_reg_cmd)[0];

    DEBUG("BMI_read_reg value read=0x%x\n", *pvalue);
}

/* Write a value to a Target register. */
void
BMI_write_reg(A_UINT32 address, A_UINT32 value)
{
    DEBUG("BMI_write_reg address=0x%x value=0x%x\n", address, value);

    ((int *)BMI_write_reg_cmd)[0] = AR6000_XIOCTL_BMI_WRITE_SOC_REGISTER;
    ((int *)BMI_write_reg_cmd)[1] = address;
    ((int *)BMI_write_reg_cmd)[2] = value;
    ifr.ifr_data = (char *)BMI_write_reg_cmd;
    if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0)
    {
        err(1, "%s", ifr.ifr_name);
    }
}

/* Read Target memory word and return its value. */
void
BMI_read_mem(A_UINT32 address, A_UINT32 *pvalue)
{
    DEBUG("BMI_read_mem address=0x%x\n", address);

    ((int *)BMI_read_mem_cmd)[0] = AR6000_XIOCTL_BMI_READ_MEMORY;
    ((int *)BMI_read_mem_cmd)[1] = address;
    ((int *)BMI_read_mem_cmd)[2] = sizeof(A_UINT32);
    ifr.ifr_data = (char *)BMI_read_mem_cmd;
    if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0)
    {
        err(1, "%s", ifr.ifr_name);
    }
    *pvalue = ((int *)BMI_read_mem_cmd)[0];

    DEBUG("BMI_read_mem value read=0x%x\n", *pvalue);
}

/* Write a word to a Target memory. */
void
BMI_write_mem(A_UINT32 address, A_UINT8 *data, A_UINT32 sz)
{
    int chunk_sz;

    while (sz) {
        chunk_sz = (BMI_DATASZ_MAX > sz) ? sz : BMI_DATASZ_MAX;
        DEBUG("BMI_write_mem address=0x%x data=%p sz=%d\n",
                address, data, chunk_sz);

        ((int *)BMI_write_mem_cmd)[0] = AR6000_XIOCTL_BMI_WRITE_MEMORY;
        ((int *)BMI_write_mem_cmd)[1] = address;
        ((int *)BMI_write_mem_cmd)[2] = chunk_sz;
        memcpy(&((int *)BMI_write_mem_cmd)[3], data, chunk_sz);
        ifr.ifr_data = (char *)BMI_write_mem_cmd;
        if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0)
        {
            err(1, "%s", ifr.ifr_name);
        }

        sz -= chunk_sz;
        data += chunk_sz;
        address += chunk_sz;
    }
}

/*
 * Enable and configure the Target's Serial Interface
 * so we can access the EEPROM.
 */
void
enable_SI(void)
{
    A_UINT32 regval;

    DEBUG("Enable Serial Interface\n");

    BMI_read_reg(RTC_BASE_ADDRESS+SYSTEM_SLEEP_OFFSET, &sys_sleep_reg);
    BMI_write_reg(RTC_BASE_ADDRESS+SYSTEM_SLEEP_OFFSET, SYSTEM_SLEEP_DISABLE_SET(1)); //disable system sleep temporarily

    BMI_read_reg(RTC_BASE_ADDRESS+CLOCK_CONTROL_OFFSET, &regval);
    regval &= ~CLOCK_CONTROL_SI0_CLK_MASK;
    BMI_write_reg(RTC_BASE_ADDRESS+CLOCK_CONTROL_OFFSET, regval);

    BMI_read_reg(RTC_BASE_ADDRESS+RESET_CONTROL_OFFSET, &regval);
    regval &= ~RESET_CONTROL_SI0_RST_MASK;
    BMI_write_reg(RTC_BASE_ADDRESS+RESET_CONTROL_OFFSET, regval);


    BMI_read_reg(GPIO_BASE_ADDRESS+GPIO_PIN0_OFFSET, &regval);
    regval &= ~GPIO_PIN0_CONFIG_MASK;
    BMI_write_reg(GPIO_BASE_ADDRESS+GPIO_PIN0_OFFSET, regval);

    BMI_read_reg(GPIO_BASE_ADDRESS+GPIO_PIN1_OFFSET, &regval);
    regval &= ~GPIO_PIN1_CONFIG_MASK;
    BMI_write_reg(GPIO_BASE_ADDRESS+GPIO_PIN1_OFFSET, regval);

    /* SI_CONFIG = 0x500a6; */
    regval =    SI_CONFIG_BIDIR_OD_DATA_SET(1)  |
                SI_CONFIG_I2C_SET(1)            |
                SI_CONFIG_POS_SAMPLE_SET(1)     |
                SI_CONFIG_INACTIVE_CLK_SET(1)   |
                SI_CONFIG_INACTIVE_DATA_SET(1)   |
                SI_CONFIG_DIVIDER_SET(6);
    BMI_write_reg(SI_BASE_ADDRESS+SI_CONFIG_OFFSET, regval);
    
}

void
disable_SI(void)
{
    A_UINT32 regval;

    BMI_read_reg(RTC_BASE_ADDRESS+RESET_CONTROL_OFFSET, &regval);
    regval |= RESET_CONTROL_SI0_RST_MASK;
    BMI_write_reg(RTC_BASE_ADDRESS+RESET_CONTROL_OFFSET, regval);

    BMI_read_reg(RTC_BASE_ADDRESS+CLOCK_CONTROL_OFFSET, &regval);
    regval |= CLOCK_CONTROL_SI0_CLK_MASK;
    BMI_write_reg(RTC_BASE_ADDRESS+CLOCK_CONTROL_OFFSET, regval);//Gate SI0 clock

    BMI_write_reg(RTC_BASE_ADDRESS+SYSTEM_SLEEP_OFFSET, sys_sleep_reg); //restore system sleep setting
}

/*
 * Tell the Target to start an 8-byte read from EEPROM,
 * putting the results in Target RX_DATA registers.
 */
void
request_8byte_read(int offset)
{
    A_UINT32 regval;

    DEBUG("request_8byte_read from offset 0x%x\n", offset);

    
    if (addr_width_16bit){
        regval =( (offset & 0xff)<<16)    |
                ( (offset & 0xff00))   |
                0xa0;
        BMI_write_reg(SI_BASE_ADDRESS+SI_TX_DATA0_OFFSET, regval);

        regval =SI_CS_START_SET(1)      |
                SI_CS_RX_CNT_SET(0)     |
                SI_CS_TX_CNT_SET(3);
        BMI_write_reg(SI_BASE_ADDRESS+SI_CS_OFFSET, regval);

        regval =0xa1;
        BMI_write_reg(SI_BASE_ADDRESS+SI_TX_DATA0_OFFSET, regval);

        regval =SI_CS_START_SET(1)      |
                SI_CS_RX_CNT_SET(8)     |
                SI_CS_TX_CNT_SET(1);
        BMI_write_reg(SI_BASE_ADDRESS+SI_CS_OFFSET, regval);
    
    } else {
    /* SI_TX_DATA0 = read from offset */
        regval =(0xa1<<16)|
                ((offset & 0xff)<<8)    |
                (0xa0 | ((offset & 0xff00)>>7));
    
        BMI_write_reg(SI_BASE_ADDRESS+SI_TX_DATA0_OFFSET, regval);

        regval = SI_CS_START_SET(1)      |
                SI_CS_RX_CNT_SET(8)     |
                SI_CS_TX_CNT_SET(3);
        BMI_write_reg(SI_BASE_ADDRESS+SI_CS_OFFSET, regval);
     }
}

/*
 * Tell the Target to start a 4-byte write to EEPROM,
 * writing values from Target TX_DATA registers.
 */
void
request_4byte_write(int offset, A_UINT32 data)
{
    A_UINT32 regval;

    DEBUG("request_4byte_write (0x%x) to offset 0x%x\n", data, offset);

    if(addr_width_16bit){
        /* SI_TX_DATA0 = write data to offset */
        regval =((data & 0xff) <<24)    |
                ((offset & 0xff)<<16)   |
                ((offset & 0xff00))     |
                0xa0;
        BMI_write_reg(SI_BASE_ADDRESS+SI_TX_DATA0_OFFSET, regval);

        regval =    data >> 8;
        BMI_write_reg(SI_BASE_ADDRESS+SI_TX_DATA1_OFFSET, regval);

        regval =    SI_CS_START_SET(1)      |
                SI_CS_RX_CNT_SET(0)     |
                SI_CS_TX_CNT_SET(7);
        BMI_write_reg(SI_BASE_ADDRESS+SI_CS_OFFSET, regval);

    } else {
        /* SI_TX_DATA0 = write data to offset */
        regval =    ((data & 0xffff) <<16)    |
                ((offset & 0xff)<<8)    |
                (0xa0 | ((offset & 0xff00)>>7));
        BMI_write_reg(SI_BASE_ADDRESS+SI_TX_DATA0_OFFSET, regval);

        regval =    data >> 16;
        BMI_write_reg(SI_BASE_ADDRESS+SI_TX_DATA1_OFFSET, regval);

        regval =    SI_CS_START_SET(1)      |
                SI_CS_RX_CNT_SET(0)     |
                SI_CS_TX_CNT_SET(6);
        BMI_write_reg(SI_BASE_ADDRESS+SI_CS_OFFSET, regval);
    }
}

/*
 * Check whether or not an EEPROM request that was started
 * earlier has completed yet.
 */
A_BOOL
request_in_progress(void)
{
    A_UINT32 regval;

    DEBUG("request in progress?\n");

    /* Wait for DONE_INT in SI_CS */
    BMI_read_reg(SI_BASE_ADDRESS+SI_CS_OFFSET, &regval);

    DEBUG("request in progress SI_CS=0x%x\n", regval);
    if (regval & SI_CS_DONE_ERR_MASK) {
        ERROR("EEPROM signaled ERROR (0x%x)\n", regval);
    }

    return (!(regval & SI_CS_DONE_INT_MASK));
}
/*
 * try to detect the type of EEPROM,16bit address or 8bit address
 */

void eeprom_type_detect(void)
{
    A_UINT32 regval;
    A_UINT8 i = 0;

    request_8byte_read(0x100);
   /* Wait for DONE_INT in SI_CS */
    do{
        BMI_read_reg(SI_BASE_ADDRESS+SI_CS_OFFSET, &regval);
        if (regval & SI_CS_DONE_ERR_MASK) {
            addr_width_16bit = !addr_width_16bit;   //if ERROR occur,we think address type was wrongly set 
            break;
        }
        if (i++ == EEPROM_WAIT_LIMIT) {
            ERROR("EEPROM not responding\n");
        }
    } while(!(regval & SI_CS_DONE_INT_MASK));
}
/*
 * Extract the results of a completed EEPROM Read request
 * and return them to the caller.
 */
void
read_8byte_results(A_UINT32 *data)
{
    DEBUG("read_8byte_results\n");

    /* Read SI_RX_DATA0 and SI_RX_DATA1 */
    BMI_read_reg(SI_BASE_ADDRESS+SI_RX_DATA0_OFFSET, &data[0]);
    BMI_read_reg(SI_BASE_ADDRESS+SI_RX_DATA1_OFFSET, &data[1]);
}


/*
 * Wait for a previously started command to complete.
 * Timeout if the command is takes "too long".
 */
void
wait_for_eeprom_completion(void)
{
    int i=0;

    while (request_in_progress()) {
        if (i++ == EEPROM_WAIT_LIMIT) {
            ERROR("EEPROM not responding\n");
        }
    }
}

/*
 * High-level function which starts an 8-byte read,
 * waits for it to complete, and returns the result.
 */
void
fetch_8bytes(int offset, A_UINT32 *data)
{
    request_8byte_read(offset);
    wait_for_eeprom_completion();
    read_8byte_results(data);

    /* Clear any pending intr */
    BMI_write_reg(SI_BASE_ADDRESS+SI_CS_OFFSET, SI_CS_DONE_INT_MASK);
}

/*
 * High-level function which starts a 4-byte write,
 * and waits for it to complete.
 */
void
commit_4bytes(int offset, A_UINT32 data)
{
    request_4byte_write(offset, data);
    wait_for_eeprom_completion();
}

static int
wmic_ether_aton(const char *orig, A_UINT8 *eth)
{
    const char *bufp;   
    int i;

    i = 0;
    for (bufp = orig; *bufp != '\0'; ++bufp) {
        unsigned int val;
        unsigned char c = *bufp++;
        if (isdigit(c)) val = c - '0';
        else if (c >= 'a' && c <= 'f') val = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') val = c - 'A' + 10;
        else break;

        val <<= 4;
        c = *bufp++;
        if (isdigit(c)) val |= c - '0';
        else if (c >= 'a' && c <= 'f') val |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') val |= c - 'A' + 10;
        else break;

        eth[i] = (unsigned char) (val & 0377);
        if(++i == MAC_ADDR_LEN) {
            /* That's it.  Any trailing junk? */
            if (*bufp != '\0') {
                return 0;
            }
            return 1;
        }
        if (*bufp != ':') {
                break;
        }
    }
    return 0;
}


/* Process command-line arguments */
void
scan_args(int argc, char **argv)
{
    int c;
    int num_actions = 0;

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"read", 0, NULL, 'r'},
            {"write", 0, NULL, 'w'},
            {"transfer", 0, NULL, 't'},
            {"setmac", 1, NULL, 's'},
            {"setreg", 1, NULL, 'd'},
            {"a16", 0, NULL, 'a'},
            {"file", 1, NULL, 'f'},
            {"interface", 1, NULL, 'i'},
            {"Force", 0, NULL, 'F'},
            {0, 0, 0, 0}
        };

        c = getopt_long (argc, argv, "rwaf:i:",
                         long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'r': /* Read from EEPROM to file */
            read_specified = TRUE;
            num_actions++;
            break;

        case 'w': /* Write from file to EEPROM */
            write_specified = TRUE;
            num_actions++;
            break;

        case 't': /* Read from EEPROM and load into Target RAM */
            transfer_specified = TRUE;
            num_actions++;
            break;

        case 's': /* Update MAC address in the file */
            p_mac = optarg;
            break;

        case 'd':
            p_reg = optarg;
            /*
             * Get the regulatory numbers from cmd line, and store them in an
             * array of 2 bytes.  This is the only way to maintain sanity in a system
             * where the elements are packed and may not be word-aligned, because
             * if I do it this way, I can use memcpy to copy the regulatory data
             * into host RAM.
             */
            sscanf(p_reg, "%x", &new_reg_val); 
            new_reg[0] = new_reg_val & (0xFF);
            new_reg[1] = (new_reg_val & (0xFF00)) >> 8;
            break;

        case 'f': /* Filename to use with --read or --write */
            memset(filename, '\0', MAX_FILENAME+1);
            strncpy(filename, optarg, MAX_FILENAME);
            file_specified = TRUE;
            break;

        case 'i': /* interface name */
            memset(ifname, '\0', IFNAMSIZ);
            strncpy(ifname, optarg, IFNAMSIZ-1);
            break;
        case 'a':
            addr_width_16bit = 1;
            auto_detect_eeprom =0;
            break;
        case 'F': /* Force */
            force_specified = TRUE;
            break;

        default:
            usage();
        }
    }

    if (num_actions != 1) {
        ERROR("Must specify exactly one of --read, --write, --transfer.\n");
    }

    if (p_mac) {
        if (!wmic_ether_aton(p_mac, mac_addr)) {
            ERROR("invalid MAC address in option --setmac\n");
        }
    }

    if (read_specified) {
        if (file_specified) {
            open_output_file();
        } else {
            ERROR("Must specify a filename with --read\n");
        }
    }

    if (write_specified) {
        if (file_specified) {
            open_input_file();
        } else {
            ERROR("Must specify a filename with --write\n");
        }
    }

    if (transfer_specified) {
        if (file_specified) {
            open_input_file();
        }
    }
}

#define HOST_INTEREST_ITEM_ADDRESS(item) \
        ((target_type == TARGET_TYPE_AR6002) ? AR6002_HOST_INTEREST_ITEM_ADDRESS(item) : \
        ((target_type == TARGET_TYPE_AR6003) ? AR6003_HOST_INTEREST_ITEM_ADDRESS(item) : 0))

/*
 * Update the regulatory data in host RAM
 */
static void
update_reg(unsigned char* eeprom, A_UINT32 sz, unsigned char* new_reg_ptr)
{
    A_UINT32 i;
    A_UINT16 *ptr_checksum;
    A_UINT16 *ptr_eeprom;
    A_UINT16 checksum;
    unsigned char *ptr_reg;

    if (target_type == TARGET_TYPE_AR6003) {
        ptr_checksum = (A_UINT16 *)(eeprom + 4);
        ptr_reg  = eeprom + 12;
    } else {
      ERROR("Updating regulatory domain only supported on AR6003");
    }

    memcpy(ptr_reg, new_reg_ptr, 2); // Regulatory domain is 2 bytes
 
    /* Clear current checksum and recalculate it */ 
    *ptr_checksum = 0;
    ptr_eeprom = (A_UINT16*)eeprom;

    checksum = 0;
    for (i=0;i<sz;i+=2) {
        checksum ^= *ptr_eeprom;
        ptr_eeprom++;
    }
    checksum = ~checksum;

    *ptr_checksum = checksum;
    return;
}

/*
 * Update the MAC address in host RAM
 */
static void
update_mac(unsigned char* eeprom, A_UINT32 sz, unsigned char* macaddr)
{
    A_UINT32 i;
    A_UINT16 *ptr_checksum;
    A_UINT16 *ptr_eeprom;
    A_UINT16 checksum;
    unsigned char *ptr_macaddr;
   
    if (target_type == TARGET_TYPE_AR6001) {
        ptr_checksum = (A_UINT16 *)(eeprom + 0);
        ptr_macaddr  = eeprom + 6;
    } else if (target_type == TARGET_TYPE_AR6002) {
        ptr_checksum = (A_UINT16 *)(eeprom + 4);
        ptr_macaddr  = eeprom + 10;
    } else if (target_type == TARGET_TYPE_AR6003) {
        ptr_checksum = (A_UINT16 *)(eeprom + 4);
        ptr_macaddr  = eeprom + 22;
    } else {
        ERROR("invalid target type \n");
    }

    memcpy(ptr_macaddr,macaddr,MAC_ADDR_LEN);

    /* Clear current checksum and recalculate it */ 
    *ptr_checksum = 0;
    ptr_eeprom = (A_UINT16*)eeprom;

    checksum = 0;
    for (i=0;i<sz;i+=2) {
        checksum ^= *ptr_eeprom;
        ptr_eeprom++;
    }
    checksum = ~checksum;

    *ptr_checksum = checksum;
    return;
}

struct targetdef_s *NOdef = NULL;
#pragma weak AR6002def = NOdef
#pragma weak AR6003def = NOdef

int
main(int argc, char *argv[])
{
    A_UCHAR eeprom_data[BOARD_DATA_SZ_MAX];
    A_UCHAR eeprom_ext_data[BOARD_DATA_SZ_MAX];
    int i;
    struct stat fstat_buf;
    A_BOOL ext_data_avail;

    prog_name = argv[0];
    strcpy(ifname, "eth1"); /* default ifname */

    scan_args(argc, argv);

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) err(1, "socket");
    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    wait_for_target();
    ext_data_avail = FALSE;

    switch (target_type) {
    case TARGET_TYPE_AR6002:
        targetdef = AR6002def;
        break;
    case TARGET_TYPE_AR6003:
        targetdef = AR6003def;
        break;
    default:
        break;
    }

    if (!targetdef) {
        ERROR("Unsupported Target Type\n");
    }

    enable_SI();
    if(auto_detect_eeprom){
        eeprom_type_detect();
    }
    if (write_specified) {
        /*
         * Commit EEPROM_SZ Bytes of Board Data, 4 bytes at a time.
         */
        for (i=0; i<EEPROM_SZ; i+=4) {
            if (fread(eeprom_data, 1, 4, file) != 4) {
                ERROR("Read 4 bytes from local file failed\n");
            }
            DEBUG("COMMIT DATA: 0x%x\n", *(A_UINT32 *)(eeprom_data));
            commit_4bytes(i, *(A_UINT32 *)(eeprom_data));

            /*
             * Wait for the maximum write time,
             * which really depends on the EEPROM part.
             *
             * TBDXXX: 
             * Would be nice to find a way to eliminate this ugly wait.
             */
            usleep(5000); /* 5Ms */
        }
        /* Extended eeprom data not supported on writes */
    } else { /* read_specified or transfer_specified */
        if (transfer_specified && file_specified) {
            /*
             * Transfer from file to Target RAM.
             * Fetch source data from file.
             */
            if (fread(eeprom_data, 1, EEPROM_SZ, file) != EEPROM_SZ) {
               ERROR("Read from local file failed\n");
            }

            if (fstat(fileno(file), &fstat_buf)) {
               ERROR("File stats for local file failed\n");
            }

            if (fstat_buf.st_size > EEPROM_SZ) {
                if ((EEPROM_EXT_SZ) &&
                    (fread(eeprom_ext_data, 1, EEPROM_EXT_SZ, file) != EEPROM_EXT_SZ)) 
                {
                    ERROR("Read from local file failed for extended data\n");
                }
                ext_data_avail = TRUE;
            }
        } else {
            /*
             * Read from EEPROM to file OR transfer from EEPROM to Target RAM.
             * Fetch EEPROM_SZ Bytes of Board Data, 8 bytes at a time.
             */

            fetch_8bytes(0, (A_UINT32 *)(&eeprom_data[0]));
            if (!force_specified) {
                /* Check the first word of EEPROM for validity */
                A_UINT32 first_word = *((A_UINT32 *)eeprom_data);

                if ((first_word == 0) || (first_word == 0xffffffff)) {
                    ERROR("Did not find EEPROM with valid Board Data.\n");
                }
            }

            for (i=8; i<EEPROM_SZ; i+=8) {
                fetch_8bytes(i, (A_UINT32 *)(&eeprom_data[i]));
            }
        }

        if (read_specified) {
            /* Update MAC address in RAM */
            if (p_mac) {
                update_mac(eeprom_data, EEPROM_SZ, mac_addr);
            }

            /* Update regulatory data in RAM */
            if (p_reg) {
                update_reg(eeprom_data, EEPROM_SZ, new_reg);
            }

            /* Write EEPROM data to a local file */
            if (fwrite(eeprom_data, 1, EEPROM_SZ, file) != EEPROM_SZ) {
                ERROR("write to local file failed\n");
            }
        } else { /* transfer_specified */
            A_UINT32 board_data_addr;

            /* Determine where in Target RAM to write Board Data */
            BMI_read_mem(HOST_INTEREST_ITEM_ADDRESS(hi_board_data),
                         &board_data_addr);
            if (board_data_addr == 0) {
                ERROR("hi_board_data is zero\n");
            }

            /* Update MAC address in RAM */
            if (p_mac) {
                update_mac(eeprom_data, EEPROM_SZ, mac_addr);
            }

            /* Update regulatory data in RAM */
            if (p_reg) {
	        update_reg(eeprom_data, EEPROM_SZ, new_reg);
            }

            /* Write EEPROM data to Target RAM */
            BMI_write_mem(board_data_addr, ((A_UINT8 *)eeprom_data), EEPROM_SZ);

            /* Record the fact that Board Data IS initialized */
            {
                A_UINT32 one = 1;
                BMI_write_mem(HOST_INTEREST_ITEM_ADDRESS(hi_board_data_initialized),
                        (A_UINT8 *)&one, sizeof(A_UINT32));
            }

            if (ext_data_avail) {
                /* Determine where in Target RAM to write Board Data */
                BMI_read_mem(HOST_INTEREST_ITEM_ADDRESS(hi_board_ext_data),
                         &board_data_addr);
                if (board_data_addr == 0) {
                    ERROR("hi_board_ext_data is zero\n");
                }

                /* Write EEPROM data to Target RAM */
                BMI_write_mem(board_data_addr, ((A_UINT8 *)eeprom_ext_data), EEPROM_EXT_SZ);

                /* Record the fact that Board Data IS initialized */
                {
                    A_UINT32 one = 1;
                    BMI_write_mem(HOST_INTEREST_ITEM_ADDRESS(hi_board_ext_data_initialized),
                         (A_UINT8 *)&one, sizeof(A_UINT32));
                }
            }
        }
    }

    disable_SI();

    return 0;
}
