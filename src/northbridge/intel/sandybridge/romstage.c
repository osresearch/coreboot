/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2007-2010 coresystems GmbH
 * Copyright (C) 2011 The ChromiumOS Authors.  All rights reserved.
 * Copyright (C) 2014 Vladimir Serbinenko
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdint.h>
#include <string.h>
#include <console/console.h>
#include <arch/io.h>
#include <lib.h>
#include <cpu/x86/lapic.h>
#include <timestamp.h>
#include "sandybridge.h"
#include <cpu/x86/bist.h>
#include <cpu/intel/romstage.h>
#include <device/pci_def.h>
#include <device/device.h>
#include <halt.h>
#include <tpm.h>
#include <tpm_lite/tlcl.h>
#include <program_loading.h>
#include <northbridge/intel/sandybridge/chip.h>
#include "southbridge/intel/bd82x6x/pch.h"
#include <southbridge/intel/common/gpio.h>

static void early_pch_init(void)
{
	u8 reg8;

	// reset rtc power status
	reg8 = pci_read_config8(PCH_LPC_DEV, 0xa4);
	reg8 &= ~(1 << 2);
	pci_write_config8(PCH_LPC_DEV, 0xa4, reg8);
}

/* Platform has no romstage entry point under mainboard directory,
 * so this one is named with prefix mainboard.
 */
void mainboard_romstage_entry(unsigned long bist)
{
	int s3resume = 0;

	if (MCHBAR16(SSKPD) == 0xCAFE) {
		outb(0x6, 0xcf9);
		halt ();
	}

	timestamp_init(get_initial_timestamp());
	timestamp_add_now(TS_START_ROMSTAGE);

	if (bist == 0)
		enable_lapic();

	pch_enable_lpc();

	/* Enable GPIOs */
	pci_write_config32(PCH_LPC_DEV, GPIO_BASE, DEFAULT_GPIOBASE|1);
	pci_write_config8(PCH_LPC_DEV, GPIO_CNTL, 0x10);

	setup_pch_gpios(&mainboard_gpio_map);

	/* Initialize superio */
	mainboard_config_superio();

	if (IS_ENABLED(CONFIG_MEASURED_BOOT) && IS_ENABLED(CONFIG_LPC_TPM)) {
		// we don't know if we are coming out of a resume
		// at this point, but want to setup the tpm ASAP
		init_tpm(0);
		const void * const bootblock = (const void*) 0xFFFFF800;
		const unsigned bootblock_size = 0x800;
		tlcl_measure(0, bootblock, bootblock_size);

		extern char _romstage, _eromstage;
		tlcl_measure(1, &_romstage, &_eromstage - &_romstage);
	}

	/* USB is inited in MRC if MRC is used.  */
	if (CONFIG_USE_NATIVE_RAMINIT) {
		early_usb_init(mainboard_usb_ports);
	}

	/* Initialize console device(s) */
	console_init();

	/* Halt if there was a built in self test failure */
	report_bist_failure(bist);

	/* Perform some early chipset initialization required
	 * before RAM initialization can work
	 */
	sandybridge_early_initialization(SANDYBRIDGE_MOBILE);
	printk(BIOS_DEBUG, "Back from sandybridge_early_initialization()\n");

	s3resume = southbridge_detect_s3_resume();

	post_code(0x38);

	mainboard_early_init(s3resume);

	/* Enable SPD ROMs and DDR-III DRAM */
	enable_smbus();

	post_code(0x39);

	perform_raminit(s3resume);

	timestamp_add_now(TS_AFTER_INITRAM);

	post_code(0x3b);
	/* Perform some initialization that must run before stage2 */
	early_pch_init();
	post_code(0x3c);

	southbridge_configure_default_intmap();
	rcba_config();

	post_code(0x3d);

	northbridge_romstage_finalize(s3resume);

	// the normal TPM init happens here, if we haven't already
	// set it up as part of the measured boot.
	if (!IS_ENABLED(CONFIG_MEASURED_BOOT) && IS_ENABLED(CONFIG_LPC_TPM)) {
		init_tpm(s3resume);
	}

	printk(BIOS_DEBUG, "%s: romstage complete\n", __FILE__);

	post_code(0x3f);
}


void platform_segment_loaded(uintptr_t start, size_t size, int flags)
{
	if (IS_ENABLED(CONFIG_MEASURED_BOOT))
	{
		tlcl_measure(2, (const void*) start, size);
	}
}

