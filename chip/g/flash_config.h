/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef __EC_CHIP_G_FLASH_CONFIG_H
#define __EC_CHIP_G_FLASH_CONFIG_H

#define FLASH_REGION_EN_ALL ((1 << GC_GLOBALSEC_FLASH_REGION0_CTRL_EN_LSB) |\
			     (1 << GC_GLOBALSEC_FLASH_REGION0_CTRL_RD_EN_LSB) |\
			     (1 << GC_GLOBALSEC_FLASH_REGION0_CTRL_WR_EN_LSB))

/*
 * The below structure describes a single flash region (the hardware supports
 * up to eight). The reg_size field is the actual region size, The reg_perms
 * bits are as used in the above macro, allowing to enable the region and its
 * read and write accesses separately.
 */
struct g_flash_region {
	uint32_t reg_base;
	uint32_t reg_size;
	uint32_t reg_perms;
};

/*
 * This function is provided by the board layer to describe necessary flash
 * regions' configuration to allow the flash driver to set the regions
 * properly.
 *
 * The function is passed an array of the g_flash_region structures of the
 * max_regions size, it fills as many entties as necessary and returns the
 * number of set up entries.
 */
int flash_regions_to_enable(struct g_flash_region *regions,
			    int max_regions);

#endif  /* ! __EC_CHIP_G_FLASH_CONFIG_H */

