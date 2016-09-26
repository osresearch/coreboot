/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2007 Silicon Integrated Systems Corp. (SiS)
 * Written by Morgan Tsai <my_tsai@sis.com> for SiS.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "smbus.h"

#define SMBUS0_IO_BASE	0x8D0

static inline void smbus_delay(void)
{
	outb(0x80, 0x80);
}

int smbus_wait_until_ready(unsigned smbus_io_base)
{
	unsigned long loops;
	loops = SMBUS_TIMEOUT;
	do {
		unsigned char val;
		smbus_delay();
		val = inb(smbus_io_base + SMBHSTSTAT);
		val &= 0x1f;
		if (val == 0) {
			return 0;
		}
		outb(val,smbus_io_base + SMBHSTSTAT);
	} while (--loops);
	return -2;
}

int smbus_wait_until_done(unsigned smbus_io_base)
{
	unsigned long loops;
	loops = SMBUS_TIMEOUT;
	do {
		unsigned char val;
		smbus_delay();

		val = inb(smbus_io_base + 0x00);
		if ( (val & 0xff) != 0x02) {
			return 0;
		}
	} while (--loops);
	return -3;
}

int do_smbus_recv_byte(unsigned smbus_io_base, unsigned device)
{
	unsigned char global_status_register;
	unsigned char byte;

	/* set the device I'm talking too */
	outb(((device & 0x7f) << 1)|1 , smbus_io_base + SMBXMITADD);
	smbus_delay();

	/* byte data recv */
	outb(0x05, smbus_io_base + SMBHSTPRTCL);
	smbus_delay();

	/* poll for transaction completion */
	if (smbus_wait_until_done(smbus_io_base) < 0) {
		return -3;
	}

	global_status_register = inb(smbus_io_base + SMBHSTSTAT) & 0x80; /* lose check */

	/* read results of transaction */
	byte = inb(smbus_io_base + SMBHSTCMD);

	if (global_status_register != 0x80) { // lose check, otherwise it should be 0
		return -1;
	}
	return byte;
}

int do_smbus_send_byte(unsigned smbus_io_base, unsigned device, unsigned char val)
{
	unsigned global_status_register;

	outb(val, smbus_io_base + SMBHSTDAT0);
	smbus_delay();

	/* set the command... */
	outb(val, smbus_io_base + SMBHSTCMD);
	smbus_delay();

	/* set the device I'm talking too */
	outb(((device & 0x7f) << 1) | 0, smbus_io_base + SMBXMITADD);
	smbus_delay();

	/* set up for a byte data write */
	outb(0x04, smbus_io_base + SMBHSTPRTCL);
	smbus_delay();

	/* poll for transaction completion */
	if (smbus_wait_until_done(smbus_io_base) < 0) {
		return -3;
	}
	global_status_register = inb(smbus_io_base + SMBHSTSTAT) & 0x80; /* lose check */;

	if (global_status_register != 0x80) {
		return -1;
	}
	return 0;
}

static inline int do_smbus_read_byte(unsigned smbus_io_base, unsigned device, unsigned address)
{
	unsigned char global_status_register;
	unsigned char byte;

	outb(0xff, smbus_io_base + 0x00);
	smbus_delay();
	outb(0x20, smbus_io_base + 0x03);
	smbus_delay();

	outb(((device & 0x7f) << 1)|1 , smbus_io_base + 0x04);
	smbus_delay();
	outb(address & 0xff, smbus_io_base + 0x05);
	smbus_delay();
	outb(0x12, smbus_io_base + 0x03);
	smbus_delay();

	int i, j;
	for (i = 0;i < 0x1000; i++) {
		if (inb(smbus_io_base + 0x00) != 0x08) {
			smbus_delay();
			for (j=0;j<0xFFFF;j++);
		}
	}

	global_status_register = inb(smbus_io_base + 0x00);
	byte = inb(smbus_io_base + 0x08);

	if (global_status_register != 0x08) { // lose check, otherwise it should be 0
		printk(BIOS_DEBUG, "Fail\r\t");
		return -1;
	}
	printk(BIOS_DEBUG, "Success\r\t");
	return byte;
}


static inline int do_smbus_write_byte(unsigned smbus_io_base, unsigned device, unsigned address, unsigned char val)
{
	unsigned global_status_register;

	outb(val, smbus_io_base + SMBHSTDAT0);
	smbus_delay();

	/* set the device I'm talking too */
	outb(((device & 0x7f) << 1) | 0, smbus_io_base + SMBXMITADD);
	smbus_delay();

	outb(address & 0xff, smbus_io_base + SMBHSTCMD);
	smbus_delay();

	/* set up for a byte data write */
	outb(0x06, smbus_io_base + SMBHSTPRTCL);
	smbus_delay();

	/* poll for transaction completion */
	if (smbus_wait_until_done(smbus_io_base) < 0) {
		return -3;
	}
	global_status_register = inb(smbus_io_base + SMBHSTSTAT) & 0x80; /* lose check */;

	if (global_status_register != 0x80) {
		return -1;
	}
	return 0;
}



static const uint8_t SiS_LPC_init[34][3]={
{0x04, 0xF8, 0x07},					//Reg 0x04
{0x45, 0x00, 0x00},					//Reg 0x45			//Enable Rom Flash
{0x46, 0x00, 0x3D},					//Reg 0x46
{0x47, 0x00, 0xDD},					//Reg 0x47
{0x48, 0x00, 0x12},					//Reg 0x48
{0x64, 0x00, 0xFF},					//Reg 0x64
{0x65, 0x00, 0xC1},					//Reg 0x65
{0x68, 0x00, 0x89},					//Reg 0x68			//SB.ASM, START POST
{0x69, 0x00, 0x80},					//Reg 0x69
{0x6B, 0x00, 0x00},					//Reg 0x6B			//SBBB.ASM
{0x6C, 0xFF, 0x97},					//Reg 0x6C			//SBBB.ASM
{0x6E, 0x00, 0x00},					//Reg 0x6E			//SBBB.ASM			But in Early Post sets 0x04.
{0x6F, 0xFF, 0x14},					//Reg 0x6F			//SBBB.ASM
{0x77, 0x00, 0x0E},					//Reg 0x77			//SBOEM.ASM, EARLY POST
{0x78, 0x00, 0x20},					//Reg 0x78
{0x7B, 0x00, 0x88},					//Reg 0x7B
{0x7F, 0x00, 0x40},					//Reg 0x7F			//SBOEM.ASM, EARLY POST
{0xC1, 0x00, 0xF0},					//Reg 0xC1
{0xC2, 0x00, 0x01},					//Reg 0xC2
{0xC3, 0x00, 0x00},					//Reg 0xC3			//NBAGPBB.ASM
{0xC9, 0x00, 0x80},					//Reg 0xC9
{0xCF, 0x00, 0x45},					//Reg 0xCF
{0xD0, 0x00, 0x02},					//Reg 0xD0
{0xD4, 0x00, 0x44},					//Reg 0xD4
{0xD5, 0x00, 0x62},					//Reg 0xD5
{0xD6, 0x00, 0x32},					//Reg 0xD6
{0xD8, 0x00, 0x45},					//Reg 0xD8
{0xDA, 0x00, 0xDA},					//Reg 0xDA
{0xDB, 0x00, 0x61},					//Reg 0xDB
{0xDC, 0x00, 0xAA},					//Reg 0xDC
{0xDD, 0x00, 0xAA},					//Reg 0xDD
{0xDE, 0x00, 0xAA},					//Reg 0xDE
{0xDF, 0x00, 0xAA},					//Reg 0xDF
{0x00, 0x00, 0x00}					//End of table
};

static const uint8_t	SiS_NBPCIE_init[43][3]={
{0x3D, 0x00, 0x00},					//Reg 0x3D
{0x1C, 0xFE, 0x01},					//Reg 0x1C
{0x1D, 0xFE, 0x01},					//Reg 0x1D
{0x24, 0xFE, 0x01},					//Reg 0x24
{0x26, 0xFE, 0x01},					//Reg 0x26
{0x40, 0xFF, 0x10},					//Reg 0x40
{0x43, 0xFF, 0x78},					//Reg 0x43
{0x44, 0xFF, 0x02},					//Reg 0x44
{0x45, 0xFF, 0x10},					//Reg 0x45
{0x48, 0xFF, 0x52},					//Reg 0x48
{0x49, 0xFF, 0xE3},					//Reg 0x49
{0x5A, 0x00, 0x00},					//Reg 0x4A
{0x4B, 0x00, 0x16},					//Reg 0x4B
{0x4C, 0x00, 0x80},					//Reg 0x4C
{0x4D, 0x00, 0x02},					//Reg 0x4D
{0x4E, 0x00, 0x00},					//Reg 0x4E
{0x5C, 0x00, 0x52},					//Reg 0x5C
{0x5E, 0x00, 0x10},					//Reg 0x5E
{0x34, 0x00, 0xD0},					//Reg 0x34
{0xD0, 0x00, 0x01},					//Reg 0xD0
{0x4F, 0x00, 0x80},					//Reg 0x4F
{0xA1, 0x00, 0xF4},					//Reg 0xA1
{0xA2, 0x7F, 0x00},					//Reg 0xA2
{0xBD, 0x00, 0xA0},					//Reg 0xBD
{0xD1, 0xFF, 0x00},					//Reg 0xD1
{0xD3, 0xFE, 0x01},					//Reg 0xD3
{0xD4, 0x18, 0x20},					//Reg 0xD4
{0xD5, 0xF0, 0x00},					//Reg 0xD5
{0xDD, 0xFF, 0x00},					//Reg 0xDD
{0xDE, 0xEC, 0x10},					//Reg 0xDE
{0xDF, 0xFF, 0x00},					//Reg 0xDF
{0xE0, 0xF7, 0x00},					//Reg 0xE0
{0xE3, 0xEF, 0x10},					//Reg 0xE3
{0xE4, 0x7F, 0x80},					//Reg 0xE4
{0xE5, 0xFF, 0x00},					//Reg 0xE5
{0xE6, 0x06, 0x00},					//Reg 0xE6
{0xE7, 0xFF, 0x00},					//Reg 0xE7
{0xF5, 0x00, 0x00},					//Reg 0xF5
{0xF6, 0x3F, 0x00},					//Reg 0xF6
{0xF7, 0xFF, 0x00},					//Reg 0xF7
{0xFD, 0xFF, 0x00},					//Reg 0xFD
{0x4F, 0x00, 0x00},					//Reg 0x4F
{0x00, 0x00, 0x00}					//End of table
};

static const uint8_t	SiS_ACPI_init[10][3]={
{0x1B, 0xBF, 0x40},					//Reg 0x1B
{0x84, 0x00, 0x0E},					//Reg 0x84
{0x85, 0x00, 0x29},					//Reg 0x85
{0x86, 0x00, 0xCB},					//Reg 0x86
{0x87, 0x00, 0x55},					//Reg 0x87
{0x6B, 0x00, 0x00},					//Reg 0x6B
{0x6C, 0x68, 0x97},					//Reg 0x6C
{0x6E, 0x00, 0x00},					//Reg 0x6E
{0x6F, 0xFF, 0x14},					//Reg 0x6F
{0x00, 0x00, 0x00}					//End of table
};

static const uint8_t	SiS_SBPCIE_init[13][3]={
{0x48, 0x00 ,0x07},					//Reg 0x48
{0x49, 0x00 ,0x06},					//Reg 0x49
{0x4A, 0x00 ,0x0C},					//Reg 0x4A
{0x4B, 0x00 ,0x00},					//Reg 0x4B
{0x4E, 0x00 ,0x20},					//Reg 0x4E
{0x1C, 0x00 ,0xF1},					//Reg 0x1C
{0x1D, 0x00 ,0x01},					//Reg 0x1D
{0x24, 0x00 ,0x01},					//Reg 0x24
{0x26, 0x00 ,0x01},					//Reg 0x26
{0xF6, 0x00 ,0x02},					//Reg 0xF6
{0xF7, 0x00 ,0xC8},					//Reg 0xF7
{0x5B, 0x00 ,0x40},					//Reg 0x5B
{0x00, 0x00, 0x00}					//End of table
};

static const uint8_t	SiS_NB_init[56][3]={
{0x04, 0x00 ,0x07},					//Reg 0x04
{0x05, 0x00 ,0x00},					//Reg 0x05 // alex
{0x0D, 0x00 ,0x20},					//Reg 0x0D
{0x2C, 0x00 ,0x39},					//Reg 0x2C
{0x2D, 0x00 ,0x10},					//Reg 0x2D
{0x2E, 0x00 ,0x61},					//Reg 0x2E
{0x2F, 0x00 ,0x07},					//Reg 0x2F
{0x34, 0x00 ,0xA0},					//Reg 0x34
{0x40, 0x00 ,0x36},					//Reg 0x40
{0x42, 0x00 ,0xB9},					//Reg 0x42
{0x43, 0x00 ,0x8B},					//Reg 0x43
{0x44, 0x00 ,0x05},					//Reg 0x44
{0x45, 0x00 ,0xFF},					//Reg 0x45
{0x46, 0x00 ,0x90},					//Reg 0x46
{0x47, 0x00 ,0xA0},					//Reg 0x47
//{0x4C, 0xFF ,0x09},					//Reg 0x4C // SiS307 enable
{0x4E, 0x00 ,0x00},					//Reg 0x4E
{0x4F, 0x00 ,0x02},					//Reg 0x4F
{0x5B, 0x00 ,0x44},					//Reg 0x5B
{0x5D, 0x00 ,0x00},					//Reg 0x5D
{0x5E, 0x00 ,0x25},					//Reg 0x5E
{0x61, 0x00 ,0xB0},					//Reg 0x61
{0x65, 0x00 ,0xB0},					//Reg 0x65
{0x68, 0x00 ,0x4C},					//Reg 0x68
{0x69, 0x00 ,0xD0},					//Reg 0x69
{0x6B, 0x00 ,0x07},					//Reg 0x6B
{0x6C, 0x00 ,0xDD},					//Reg 0x6C
{0x6D, 0x00 ,0xAD},					//Reg 0x6D
{0x6E, 0x00 ,0xE8},					//Reg 0x6E
{0x6F, 0x00 ,0x4D},					//Reg 0x6F
{0x70, 0x00 ,0x00},					//Reg 0x70
{0x71, 0x00 ,0x80},					//Reg 0x71
{0x72, 0x00 ,0x00},					//Reg 0x72
{0x73, 0x00 ,0x00},					//Reg 0x73
{0x74, 0x00 ,0x01},					//Reg 0x74
{0x75, 0x00 ,0x10},					//Reg 0x75
{0x7E, 0x00 ,0x29},					//Reg 0x7E
{0x8B, 0x00 ,0x10},					//Reg 0x8B
{0x8D, 0x00 ,0x03},					//Reg 0x8D
{0xA1, 0x00 ,0xD0},					//Reg 0xA1
{0xA2, 0x00 ,0x30},					//Reg 0xA2
{0xA4, 0x00 ,0x0B},					//Reg 0xA4
{0xA9, 0x00 ,0x02},					//Reg 0xA9
{0xB0, 0x00 ,0x30},					//Reg 0xB0
{0xB4, 0x00 ,0x30},					//Reg 0xB4
{0x90, 0x00 ,0x00},					//Reg 0x90
{0x91, 0x00 ,0x00},					//Reg 0x91
{0x92, 0x00 ,0x00},					//Reg 0x92
{0x93, 0x00 ,0x00},					//Reg 0x93
{0x94, 0x00 ,0x00},					//Reg 0x94
{0x95, 0x00 ,0x00},					//Reg 0x95
{0x96, 0x00 ,0x00},					//Reg 0x96
{0x97, 0x00 ,0x00},					//Reg 0x97
{0x98, 0x00 ,0x00},					//Reg 0x98
{0x99, 0x00 ,0x00},					//Reg 0x99
{0x00, 0x00, 0x00}					//End of table
};

static const uint8_t SiS_NBAGP_init[34][3]={
{0xCF, 0xDF, 0x00},	//HT issue
{0x06, 0xDF, 0x20},
{0x1E, 0xDF, 0x20},
{0x50, 0x00, 0x02},
{0x51, 0x00, 0x00},
{0x54, 0x00, 0x09},
{0x55, 0x00, 0x00},
{0x56, 0x00, 0x80},
{0x58, 0x00, 0x08},
{0x60, 0x00, 0xB1},
{0x61, 0x00, 0x02},
{0x62, 0x00, 0x60},
{0x63, 0x00, 0x60},
{0x64, 0x00, 0xAA},
{0x65, 0x00, 0x18},
{0x68, 0x00, 0x23},
{0x69, 0x00, 0x23},
{0x6A, 0x00, 0xC8},
{0x6B, 0x00, 0x08},
{0x6C, 0x00, 0x00},
{0x6D, 0x00, 0x00},
{0x6E, 0x00, 0x08},
{0x6F, 0x00, 0x00},
{0xBB, 0x00, 0x00},
{0xB5, 0x00, 0x30},
{0xB0, 0x00, 0xDB},
{0xB6, 0x00, 0x73},
{0xB7, 0x00, 0x50},
{0xBA, 0xBF, 0x41},
{0xB4, 0x3F, 0xC0},
{0xBF, 0xF9, 0x06},
{0xBA, 0x00, 0x61},
{0xBD, 0x7F, 0x80},
{0x00, 0x00, 0x00}	//End of table
};

static const uint8_t	SiS_ACPI_2_init[56][3]={
{0x00, 0x00, 0xFF},					//Reg 0x00
{0x01, 0x00, 0xFF},					//Reg 0x01
{0x02, 0x00, 0x00},					//Reg 0x02
{0x03, 0x00, 0x00},					//Reg 0x03
{0x16, 0x00, 0x00},					//Reg 0x16
{0x20, 0x00, 0xFF},					//Reg 0x20
{0x21, 0x00, 0xFF},					//Reg 0x21
{0x22, 0x00, 0x00},					//Reg 0x22
{0x23, 0x00, 0x00},					//Reg 0x23
{0x24, 0x00, 0x55},					//Reg 0x24
{0x25, 0x00, 0x55},					//Reg 0x25
{0x26, 0x00, 0x55},					//Reg 0x26
{0x27, 0x00, 0x55},					//Reg 0x27
{0x2A, 0x00, 0x40},					//Reg 0x2A
{0x2B, 0x00, 0x10},					//Reg 0x2B
{0x2E, 0x00, 0xFF},					//Reg 0x2E
{0x30, 0x00, 0xFF},					//Reg 0x30
{0x31, 0x00, 0xFF},					//Reg 0x31
{0x32, 0x00, 0x00},					//Reg 0x32
{0x33, 0x00, 0x00},					//Reg 0x33
{0x40, 0x00, 0xFF},					//Reg 0x40
{0x41, 0x00, 0xFF},					//Reg 0x41
{0x42, 0x00, 0x00},					//Reg 0x42
{0x43, 0x00, 0x00},					//Reg 0x43
{0x4A, 0x00, 0x00},					//Reg 0x4A
{0x4E, 0x00, 0x0F},					//Reg 0x4E
{0x5A, 0x00, 0x00},					//Reg 0x5A
{0x5B, 0x00, 0x00},					//Reg 0x5B
{0x62, 0x00, 0x00},					//Reg 0x62
{0x63, 0x00, 0x04},					//Reg 0x63
{0x68, 0x00, 0xFF},					//Reg 0x68
{0x76, 0x00, 0xA0},					//Reg 0x76
{0x77, 0x00, 0x22},					//Reg 0x77
{0x78, 0xDF, 0x20},					//Reg 0x78
{0x7A, 0x00, 0x10},					//Reg 0x7A
{0x7C, 0x00, 0x45},					//Reg 0x7C
{0x7D, 0x00, 0xB8},					//Reg 0x7D
{0x7F, 0x00, 0x00},					//Reg 0x7F
{0x80, 0x00, 0x1C},					//Reg 0x80
{0x82, 0x00, 0x01},					//Reg 0x82
{0x84, 0x00, 0x0E},					//Reg 0x84
{0x85, 0x00, 0x29},					//Reg 0x85
{0x86, 0x00, 0xCB},					//Reg 0x86
{0x87, 0x00, 0x55},					//Reg 0x87
{0x88, 0x00, 0x04},					//Reg 0x88
{0x96, 0x00, 0x80},					//Reg 0x96
{0x99, 0x00, 0x80},					//Reg 0x99
{0x9A, 0x00, 0x15},					//Reg 0x9A
{0x9D, 0x00, 0x05},					//Reg 0x9D
{0x9E, 0x00, 0x00},					//Reg 0x9E
{0x9F, 0x00, 0x04},					//Reg 0x9F
{0xB0, 0x00, 0x6D},					//Reg 0xB0
{0xB1, 0x00, 0x8C},					//Reg 0xB1
{0xB9, 0x00, 0xFF},					//Reg 0xB9
{0xBA, 0x00, 0x3F},					//Reg 0xBA
{0x00, 0x00, 0x00}					//End of table
};

static const uint8_t	SiS_SiS1183_init[44][3]={
{0x04, 0x00, 0x05},
{0x09, 0x00, 0x05},
{0x2C, 0x00, 0x39},
{0x2D, 0x00, 0x10},
{0x2E, 0x00, 0x83},
{0x2F, 0x00, 0x11},
{0x90, 0x00, 0x40},
{0x91, 0x00, 0x00},	// set mode
{0x50, 0x00, 0xA2},
{0x52, 0x00, 0xA2},
{0x55, 0x00, 0x96},
{0x52, 0x00, 0xA2},
{0x55, 0xF7, 0x00},
{0x56, 0x00, 0xC0},
{0x57, 0x00, 0x14},
{0x67, 0x00, 0x28},
{0x81, 0x00, 0xB3},
{0x82, 0x00, 0x72},
{0x83, 0x00, 0x40},
{0x85, 0x00, 0xB3},
{0x86, 0x00, 0x72},
{0x87, 0x00, 0x40},
{0x88, 0x00, 0xDE},	// after set mode
{0x89, 0x00, 0xB3},
{0x8A, 0x00, 0x72},
{0x8B, 0x00, 0x40},
{0x8C, 0x00, 0xDE},
{0x8D, 0x00, 0xB3},
{0x8E, 0x00, 0x92},
{0x8F, 0x00, 0x40},
{0x93, 0x00, 0x00},
{0x94, 0x00, 0x80},
{0x95, 0x00, 0x08},
{0x96, 0x00, 0x80},
{0x97, 0x00, 0x08},
{0x9C, 0x00, 0x80},
{0x9D, 0x00, 0x08},
{0x9E, 0x00, 0x80},
{0x9F, 0x00, 0x08},
{0xA0, 0x00, 0x15},
{0xA1, 0x00, 0x15},
{0xA2, 0x00, 0x15},
{0xA3, 0x00, 0x15},
{0x00, 0x00, 0x00}	//End of table
};

/*       In => Share Memory size
 *                           => 00h :    0MBytes
 *                           => 02h :   32MBytes
 *                           => 03h :   64MBytes
 *                           => 03h :   64MBytes
 *                           => 04h :  128MBytes
 *                           => Others:  Reserved
 */
static void Init_Share_Memory(uint8_t ShareSize)
{
	pci_devfn_t dev;

	dev = pci_locate_device(PCI_ID(PCI_VENDOR_ID_SIS,
				PCI_DEVICE_ID_SIS_SIS761), 0);
	pci_write_config8(dev, 0x4C, (pci_read_config8(dev, 0x4C) & 0x1F) |
			(ShareSize << 5));
}

/* In:     => Aperture size
 *              => 00h :   32MBytes
 *              => 01h :   64MBytes
 *              => 02h :  128MBytes
 *              => 03h :  256MBytes
 *              => 04h :  512MBytes
 *              => Others:  Reserved
 */
static void Init_Aper_Size(uint8_t AperSize)
{
	pci_devfn_t dev;
	uint16_t SiSAperSizeTable[]={0x0F38, 0x0F30, 0x0F20, 0x0F00, 0x0E00};

	dev = pci_locate_device(PCI_ID(PCI_VENDOR_ID_AMD, 0x1103), 0);
	pci_write_config8(dev, 0x90, AperSize << 1);

	dev = pci_locate_device(PCI_ID(PCI_VENDOR_ID_SIS, PCI_DEVICE_ID_SIS_SIS761), 0);
	pci_write_config16(dev, 0xB4, SiSAperSizeTable[AperSize]);
}

static void sis_init_stage1(void)
{
	pci_devfn_t dev;
	uint8_t temp8;
	int	i;
	uint8_t	GUI_En;

// SiS_Chipset_Initialization
// ========================== NB =============================
	dev = pci_locate_device(PCI_ID(PCI_VENDOR_ID_SIS, PCI_DEVICE_ID_SIS_SIS761), 0);
	i=0;
	while (SiS_NB_init[i][0] != 0) {
		temp8 = pci_read_config8(dev, SiS_NB_init[i][0]);
		temp8 &= SiS_NB_init[i][1];
		temp8 |= SiS_NB_init[i][2];
		pci_write_config8(dev, SiS_NB_init[i][0], temp8);
		i++;
	};

// ========================== LPC =============================
	dev = pci_locate_device(PCI_ID(PCI_VENDOR_ID_SIS, PCI_DEVICE_ID_SIS_SIS966_LPC), 0);
	i=0;
	while (SiS_LPC_init[i][0] != 0) {
		temp8 = pci_read_config8(dev, SiS_LPC_init[i][0]);
		temp8 &= SiS_LPC_init[i][1];
		temp8 |= SiS_LPC_init[i][2];
		pci_write_config8(dev, SiS_LPC_init[i][0], temp8);
		i++;
	};
// ========================== ACPI =============================
	i=0;
	while (SiS_ACPI_init[i][0] != 0) {
		temp8 = inb(0x800 + SiS_ACPI_init[i][0]);
		temp8 &= SiS_ACPI_init[i][1];
		temp8 |= SiS_ACPI_init[i][2];
		outb(temp8, 0x800 + SiS_ACPI_init[i][0]);
		i++;
	};
// ========================== NBPCIE =============================
	dev = pci_locate_device(PCI_ID(PCI_VENDOR_ID_SIS, PCI_DEVICE_ID_SIS_SIS761), 0);	//Disable Internal GUI enable bit
	temp8 = pci_read_config8(dev, 0x4C);
	GUI_En = temp8 & 0x10;
	pci_write_config8(dev, 0x4C, temp8 & (~0x10));

	dev = pci_locate_device(PCI_ID(PCI_VENDOR_ID_SIS, PCI_DEVICE_ID_SIS_SIS761_PCIE), 0);
	i=0;
	while (SiS_NBPCIE_init[i][0] != 0) {
		temp8 = pci_read_config8(dev, SiS_NBPCIE_init[i][0]);
		temp8 &= SiS_NBPCIE_init[i][1];
		temp8 |= SiS_NBPCIE_init[i][2];
		pci_write_config8(dev, SiS_NBPCIE_init[i][0], temp8);
		i++;
	};
	dev = pci_locate_device(PCI_ID(PCI_VENDOR_ID_SIS, PCI_DEVICE_ID_SIS_SIS761), 0);	//Restore Internal GUI enable bit
	temp8 = pci_read_config8(dev, 0x4C);
	pci_write_config8(dev, 0x4C, temp8 | GUI_En);

	return;
}



static void sis_init_stage2(void)
{
	pci_devfn_t dev;
	msr_t	msr;
	int	i;
	uint8_t temp8;
	uint16_t temp16;


// ========================== NB_AGP =============================
	dev = pci_locate_device(PCI_ID(PCI_VENDOR_ID_SIS, PCI_DEVICE_ID_SIS_SIS761), 0);   //Enable Internal GUI enable bit
	pci_write_config8(dev, 0x4C, pci_read_config8(dev, 0x4C) | 0x10);

	dev = pci_locate_device(PCI_ID(PCI_VENDOR_ID_SIS, PCI_DEVICE_ID_SIS_AGP), 0);
	i=0;

	while (SiS_NBAGP_init[i][0] != 0) {
		temp8 = pci_read_config8(dev, SiS_NBAGP_init[i][0]);
		temp8 &= SiS_NBAGP_init[i][1];
		temp8 |= SiS_NBAGP_init[i][2];
		pci_write_config8(dev, SiS_NBAGP_init[i][0], temp8);
		i++;
	};

/**
  *   Share Memory size
  *             => 00h :    0MBytes
  *             => 02h :   32MBytes
  *             => 03h :   64MBytes
  *             => 04h :  128MBytes
  *             => Others:  Reserved
  *
  *   Aperture size
  *             => 00h :   32MBytes
  *             => 01h :   64MBytes
  *             => 02h :  128MBytes
  *             => 03h :  256MBytes
  *             => 04h :  512MBytes
  *             => Others:  Reserved
  */

	Init_Share_Memory(0x02);  //0x02 : 32M
	Init_Aper_Size(0x01);   //0x1 : 64M

// ========================== NB =============================

	printk(BIOS_DEBUG, "Init NorthBridge sis761 -------->\n");
	dev = pci_locate_device(PCI_ID(PCI_VENDOR_ID_SIS, PCI_DEVICE_ID_SIS_SIS761), 0);
	msr = rdmsr(0xC001001A);
	 printk(BIOS_DEBUG, "Memory Top Bound %x\n",msr.lo );

	temp16=(pci_read_config8(dev, 0x4C) & 0xE0) >> 5;
	temp16=0x0001<<(temp16-1);
	temp16<<=8;

	printk(BIOS_DEBUG, "Integrated VGA Shared memory size=%dM bytes\n", temp16 >> 4);
	pci_write_config16(dev, 0x8E, (msr.lo >> 16) -temp16*1);
	pci_write_config8(dev, 0x7F, 0x08);									// ACPI Base
	outb(inb(0x856) | 0x40, 0x856);										// Auto-Reset Function

// ========================== ACPI =============================
	i=0;
	printk(BIOS_DEBUG, "Init ACPI -------->\n");
	do {
		temp8 = inb(0x800 + SiS_ACPI_2_init[i][0]);
		temp8 &= SiS_ACPI_2_init[i][1];
		temp8 |= SiS_ACPI_2_init[i][2];
		outb(temp8, 0x800 + SiS_ACPI_2_init[i][0]);
		i++;
	} while (SiS_ACPI_2_init[i][0] != 0);

// ========================== Misc =============================
	printk(BIOS_DEBUG, "Init Misc -------->\n");
	dev = pci_locate_device(PCI_ID(PCI_VENDOR_ID_SIS, PCI_DEVICE_ID_SIS_SIS966_LPC), 0);

	/* R77h Internal PCI Device Enable 1 (Power On Value = 0h)
	 * bit5 : USB Emulation (1=enable)
	 * bit3 : Internal Keyboard Controller Port Access Control enable (1=enable)
	 * bit2 : Reserved
	 * bit1 : Mask USB A20M# Event (1:K8, 0:P4/K7)
	 */
	pci_write_config8(dev, 0x77, 0x2E);

	/* R7Ch Internal PCI Device Enable 2  (Power On Value = 0h)
	 * bit4 : SATA Controller Enable (0=enable)
	 * bit3 : IDE Controller Enable (0=enable)
	 * bit2 : MAC Controller Enable (0=enable)
	 * bit1 : MODEM Controller Enable (1=disable)
	 * bit0 : AC97 Controller Enable (1=disable)
	 */
	pci_write_config8(dev, 0x7C, 0x03);

	/* R7Eh Enable Azalia (Power On Value = 08h)
	 * bit3 : Azalia Controller Enable (0=enable)
	 */
	pci_write_config8(dev, 0x7E, 0x00);  // azalia controller enable
	temp8=inb(0x878)|0x4;   //bit2=1 enable Azalia  =0 enable AC97
	outb(temp8, 0x878);  // ACPI select AC97 or HDA controller
	printk(BIOS_DEBUG, "Audio select %x\n",inb(0x878));

	dev = pci_locate_device(PCI_ID(PCI_VENDOR_ID_SIS, PCI_DEVICE_ID_SIS_SIS966_SATA), 0);

	if (!dev)
		printk(BIOS_DEBUG, "SiS 1183 does not exist !!");
	// SATA Set Mode
	pci_write_config8(dev, 0x90, (pci_read_config8(dev, 0x90)&0x3F) | 0x40);

}



static void enable_smbus(void)
{
	pci_devfn_t dev;
	uint8_t temp8;
	printk(BIOS_DEBUG, "enable_smbus -------->\n");

	dev = pci_locate_device(PCI_ID(PCI_VENDOR_ID_SIS, PCI_DEVICE_ID_SIS_SIS966_LPC), 0);

	/* set smbus iobase && enable ACPI Space*/
	pci_write_config16(dev, 0x74, 0x0800);				// Set ACPI Base
	temp8=pci_read_config8(dev, 0x40);					// Enable ACPI Space
	pci_write_config8(dev, 0x40, temp8 | 0x80);
	temp8=pci_read_config8(dev, 0x76);					// Enable SMBUS
	pci_write_config8(dev, 0x76, temp8 | 0x03);

	printk(BIOS_DEBUG, "enable_smbus <--------\n");
}

int smbus_read_byte(unsigned device, unsigned address)
{
	return do_smbus_read_byte(SMBUS0_IO_BASE, device, address);
}
int smbus_write_byte(unsigned device, unsigned address, unsigned char val)
{
	return do_smbus_write_byte(SMBUS0_IO_BASE, device, address, val);
}
