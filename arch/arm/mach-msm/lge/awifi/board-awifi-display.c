/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 * Copyright (c) 2012, LGE Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/bootmem.h>
#include <linux/msm_ion.h>
#include <asm/mach-types.h>
#include <mach/msm_memtypes.h>
#include <mach/board_lge.h>
#include <mach/board.h>
#include <mach/gpio.h>
#include <mach/gpiomux.h>
#include <mach/ion.h>
#include <mach/msm_bus_board.h>
#include <mach/socinfo.h>

#include <msm/msm_fb.h>
#include <msm/msm_fb_def.h>
#include <msm/mipi_dsi.h>
#include <msm/mdp.h>

#include "devices.h"
#include "board-awifi.h"

#if defined(CONFIG_BACKLIGHT_I2C_BL)
#include <linux/i2c_bl.h>
#endif

#ifdef CONFIG_FB_MSM_TRIPLE_BUFFER
/* prim = 1366 x 768 x 3(bpp) x 3(pages) */
#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WUXGA_PT) ||\
	defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WUXGA_INVERSE_PT)
#define MSM_FB_PRIM_BUF_SIZE roundup(1208 * 1920 * 4 * 3, 0x10000)
#else
#define MSM_FB_PRIM_BUF_SIZE roundup(1920 * 1088 * 4 * 3, 0x10000)
#endif
#else
/* prim = 1366 x 768 x 3(bpp) x 2(pages) */
#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WUXGA_PT) ||\
	defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WUXGA_INVERSE_PT)
#define MSM_FB_PRIM_BUF_SIZE roundup(1208 * 1920 * 4 * 2, 0x10000)
#else
#define MSM_FB_PRIM_BUF_SIZE roundup(1920 * 1088 * 4 * 2, 0x10000)
#endif
#endif /*CONFIG_FB_MSM_TRIPLE_BUFFER */

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
#define MSM_FB_EXT_BUF_SIZE \
		(roundup((1920 * 1088 * 2), 4096) * 1) /* 2 bpp x 1 page */
#elif defined(CONFIG_FB_MSM_TVOUT)
#define MSM_FB_EXT_BUF_SIZE \
		(roundup((720 * 576 * 2), 4096) * 2) /* 2 bpp x 2 pages */
#else
#define MSM_FB_EXT_BUF_SIZE	0
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL */

#ifdef CONFIG_FB_MSM_WRITEBACK_MSM_PANEL
#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WUXGA_PT) ||\
	defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WUXGA_INVERSE_PT)
#define MSM_FB_WFD_BUF_SIZE \
		(roundup((1920 * 1208 * 2), 4096) * 3) /* 2 bpp x 3 page */
#else		
#define MSM_FB_WFD_BUF_SIZE \
		(roundup((1280 * 736 * 2), 4096) * 3) /* 2 bpp x 3 page */
#endif
#else
#define MSM_FB_WFD_BUF_SIZE     0
#endif

#define MSM_FB_SIZE \
	roundup(MSM_FB_PRIM_BUF_SIZE + \
		MSM_FB_EXT_BUF_SIZE + MSM_FB_WFD_BUF_SIZE, 4096)

#ifdef CONFIG_FB_MSM_OVERLAY0_WRITEBACK
	#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WUXGA_PT) ||\
	defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WUXGA_INVERSE_PT)
	#define MSM_FB_OVERLAY0_WRITEBACK_SIZE roundup((1208 * 1920 * 3 * 2), 4096)
	#else
	#define MSM_FB_OVERLAY0_WRITEBACK_SIZE (0)
	#endif
#else
#define MSM_FB_OVERLAY0_WRITEBACK_SIZE (0)
#endif  /* CONFIG_FB_MSM_OVERLAY0_WRITEBACK */

#ifdef CONFIG_FB_MSM_OVERLAY1_WRITEBACK
#define MSM_FB_OVERLAY1_WRITEBACK_SIZE roundup((1920 * 1088 * 3 * 2), 4096)
#else
#define MSM_FB_OVERLAY1_WRITEBACK_SIZE (0)
#endif  /* CONFIG_FB_MSM_OVERLAY1_WRITEBACK */

static struct resource msm_fb_resources[] = {
	{
		.flags = IORESOURCE_DMA,
	}
};

#define LVDS_CHIMEI_PANEL_NAME "lvds_chimei_wxga"
#define LVDS_FRC_PANEL_NAME "lvds_frc_fhd"
#define MIPI_VIDEO_TOSHIBA_WSVGA_PANEL_NAME "mipi_video_toshiba_wsvga"
#define MIPI_VIDEO_CHIMEI_WXGA_PANEL_NAME "mipi_video_chimei_wxga"
#define HDMI_PANEL_NAME "hdmi_msm"
#define MHL_PANEL_NAME "hdmi_msm,mhl_8334"
#define TVOUT_PANEL_NAME "tvout_msm"

#define LVDS_PIXEL_MAP_PATTERN_1	1
#define LVDS_PIXEL_MAP_PATTERN_2	2

static int msm_fb_detect_panel(const char *name)
{
	return 0;
}

#ifdef CONFIG_LCD_KCAL
struct kcal_data kcal_value;
#endif

#ifdef CONFIG_UPDATE_LCDC_LUT
unsigned int lcd_color_preset_lut[256] = {
	/* default linear qlut */
	0x00000000, 0x00010101, 0x00020202, 0x00030303,
	0x00040404, 0x00050505, 0x00060606, 0x00070707,
	0x00080808, 0x00090909, 0x000a0a0a, 0x000b0b0b,
	0x000c0c0c, 0x000d0d0d, 0x000e0e0e, 0x000f0f0f,
	0x00101010, 0x00111111, 0x00121212, 0x00131313,
	0x00141414, 0x00151515, 0x00161616, 0x00171717,
	0x00181818, 0x00191919, 0x001a1a1a, 0x001b1b1b,
	0x001c1c1c, 0x001d1d1d, 0x001e1e1e, 0x001f1f1f,
	0x00202020, 0x00212121, 0x00222222, 0x00232323,
	0x00242424, 0x00252525, 0x00262626, 0x00272727,
	0x00282828, 0x00292929, 0x002a2a2a, 0x002b2b2b,
	0x002c2c2c, 0x002d2d2d, 0x002e2e2e, 0x002f2f2f,
	0x00303030, 0x00313131, 0x00323232, 0x00333333,
	0x00343434, 0x00353535, 0x00363636, 0x00373737,
	0x00383838, 0x00393939, 0x003a3a3a, 0x003b3b3b,
	0x003c3c3c, 0x003d3d3d, 0x003e3e3e, 0x003f3f3f,
	0x00404040, 0x00414141, 0x00424242, 0x00434343,
	0x00444444, 0x00454545, 0x00464646, 0x00474747,
	0x00484848, 0x00494949, 0x004a4a4a, 0x004b4b4b,
	0x004c4c4c, 0x004d4d4d, 0x004e4e4e, 0x004f4f4f,
	0x00505050, 0x00515151, 0x00525252, 0x00535353,
	0x00545454, 0x00555555, 0x00565656, 0x00575757,
	0x00585858, 0x00595959, 0x005a5a5a, 0x005b5b5b,
	0x005c5c5c, 0x005d5d5d, 0x005e5e5e, 0x005f5f5f,
	0x00606060, 0x00616161, 0x00626262, 0x00636363,
	0x00646464, 0x00656565, 0x00666666, 0x00676767,
	0x00686868, 0x00696969, 0x006a6a6a, 0x006b6b6b,
	0x006c6c6c, 0x006d6d6d, 0x006e6e6e, 0x006f6f6f,
	0x00707070, 0x00717171, 0x00727272, 0x00737373,
	0x00747474, 0x00757575, 0x00767676, 0x00777777,
	0x00787878, 0x00797979, 0x007a7a7a, 0x007b7b7b,
	0x007c7c7c, 0x007d7d7d, 0x007e7e7e, 0x007f7f7f,
	0x00808080, 0x00818181, 0x00828282, 0x00838383,
	0x00848484, 0x00858585, 0x00868686, 0x00878787,
	0x00888888, 0x00898989, 0x008a8a8a, 0x008b8b8b,
	0x008c8c8c, 0x008d8d8d, 0x008e8e8e, 0x008f8f8f,
	0x00909090, 0x00919191, 0x00929292, 0x00939393,
	0x00949494, 0x00959595, 0x00969696, 0x00979797,
	0x00989898, 0x00999999, 0x009a9a9a, 0x009b9b9b,
	0x009c9c9c, 0x009d9d9d, 0x009e9e9e, 0x009f9f9f,
	0x00a0a0a0, 0x00a1a1a1, 0x00a2a2a2, 0x00a3a3a3,
	0x00a4a4a4, 0x00a5a5a5, 0x00a6a6a6, 0x00a7a7a7,
	0x00a8a8a8, 0x00a9a9a9, 0x00aaaaaa, 0x00ababab,
	0x00acacac, 0x00adadad, 0x00aeaeae, 0x00afafaf,
	0x00b0b0b0, 0x00b1b1b1, 0x00b2b2b2, 0x00b3b3b3,
	0x00b4b4b4, 0x00b5b5b5, 0x00b6b6b6, 0x00b7b7b7,
	0x00b8b8b8, 0x00b9b9b9, 0x00bababa, 0x00bbbbbb,
	0x00bcbcbc, 0x00bdbdbd, 0x00bebebe, 0x00bfbfbf,
	0x00c0c0c0, 0x00c1c1c1, 0x00c2c2c2, 0x00c3c3c3,
	0x00c4c4c4, 0x00c5c5c5, 0x00c6c6c6, 0x00c7c7c7,
	0x00c8c8c8, 0x00c9c9c9, 0x00cacaca, 0x00cbcbcb,
	0x00cccccc, 0x00cdcdcd, 0x00cecece, 0x00cfcfcf,
	0x00d0d0d0, 0x00d1d1d1, 0x00d2d2d2, 0x00d3d3d3,
	0x00d4d4d4, 0x00d5d5d5, 0x00d6d6d6, 0x00d7d7d7,
	0x00d8d8d8, 0x00d9d9d9, 0x00dadada, 0x00dbdbdb,
	0x00dcdcdc, 0x00dddddd, 0x00dedede, 0x00dfdfdf,
	0x00e0e0e0, 0x00e1e1e1, 0x00e2e2e2, 0x00e3e3e3,
	0x00e4e4e4, 0x00e5e5e5, 0x00e6e6e6, 0x00e7e7e7,
	0x00e8e8e8, 0x00e9e9e9, 0x00eaeaea, 0x00ebebeb,
	0x00ececec, 0x00ededed, 0x00eeeeee, 0x00efefef,
	0x00f0f0f0, 0x00f1f1f1, 0x00f2f2f2, 0x00f3f3f3,
	0x00f4f4f4, 0x00f5f5f5, 0x00f6f6f6, 0x00f7f7f7,
	0x00f8f8f8, 0x00f9f9f9, 0x00fafafa, 0x00fbfbfb,
	0x00fcfcfc, 0x00fdfdfd, 0x00fefefe, 0x00ffffff
};
int update_preset_lcdc_lut(void)
{
	struct fb_cmap cmap;
	int ret = 0;

	cmap.start = 0;
	cmap.len = 256;
	cmap.transp = NULL;

#ifdef CONFIG_LCD_KCAL
	cmap.red = (uint16_t *)&(kcal_value.red);
	cmap.green = (uint16_t *)&(kcal_value.green);
	cmap.blue = (uint16_t *)&(kcal_value.blue);
#else
	cmap.red = NULL;
	cmap.green = NULL;
	cmap.blue = NULL;
#endif

	ret = mdp_preset_lut_update_lcdc(&cmap, lcd_color_preset_lut);
	if (ret)
		pr_err("%s: failed to set lut! %d\n", __func__, ret);

	return ret;
}
#endif

static struct msm_fb_platform_data msm_fb_pdata = {
	.detect_client = msm_fb_detect_panel,
	.update_lcdc_lut = update_preset_lcdc_lut,
};

static struct platform_device msm_fb_device = {
	.name              = "msm_fb",
	.id                = 0,
	.num_resources     = ARRAY_SIZE(msm_fb_resources),
	.resource          = msm_fb_resources,
	.dev.platform_data = &msm_fb_pdata,
};

void __init apq8064_allocate_fb_region(void)
{
	void *addr;
	unsigned long size;

	size = MSM_FB_SIZE;
	addr = alloc_bootmem_align(size, 0x1000);
	msm_fb_resources[0].start = __pa(addr);
	msm_fb_resources[0].end = msm_fb_resources[0].start + size - 1;
	pr_info("allocating %lu bytes at %p (%lx physical) for fb\n",
			size, addr, __pa(addr));
}

#define MDP_VSYNC_GPIO 0

static struct msm_bus_vectors mdp_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};

static struct msm_bus_vectors mdp_ui_vectors[] = {
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 577474560 * 2,//.ab = 2000000000,    // 602603520 * 2,
		.ib = 866211840 * 2,//.ib = 2000000000,    // 753254400 * 2,
	},
};

static struct msm_bus_vectors mdp_vga_vectors[] = {
	/* VGA and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 605122560 * 2,//.ab = 2000000000,    // 602603520 * 2,
		.ib = 756403200 * 2,//.ib = 2000000000,    // 753254400 * 2,
	},
};

static struct msm_bus_vectors mdp_720p_vectors[] = {
	/* 720p and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 660418560 * 2,//.ab = 2000000000,    // 602603520 * 2,
		.ib = 825523200 * 2,//.ib = 2000000000,    // 753254400 * 2,
	},
};

static struct msm_bus_vectors mdp_1080p_vectors[] = {
	/* 1080p and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 764098560 * 2,//.ab = 2000000000,    // 602603520 * 2,
		.ib = 955123200 * 2,//.ib = 2000000000,    // 753254400 * 2,
	},
};

static struct msm_bus_paths mdp_bus_scale_usecases[] = {
	{
		ARRAY_SIZE(mdp_init_vectors),
		mdp_init_vectors,
	},
	{
		ARRAY_SIZE(mdp_ui_vectors),
		mdp_ui_vectors,
	},
	{
		ARRAY_SIZE(mdp_ui_vectors),
		mdp_ui_vectors,
	},
	{
		ARRAY_SIZE(mdp_vga_vectors),
		mdp_vga_vectors,
	},
	{
		ARRAY_SIZE(mdp_720p_vectors),
		mdp_720p_vectors,
	},
	{
		ARRAY_SIZE(mdp_1080p_vectors),
		mdp_1080p_vectors,
	},
};

static struct msm_bus_scale_pdata mdp_bus_scale_pdata = {
	mdp_bus_scale_usecases,
	ARRAY_SIZE(mdp_bus_scale_usecases),
	.name = "mdp",
};

static struct msm_panel_common_pdata mdp_pdata = {
	.gpio = MDP_VSYNC_GPIO,
	.mdp_max_clk = 266667000,
	.mdp_max_bw = 2000000000u,
#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WUXGA_PT)
	.mdp_bw_ab_factor = 115,
	.mdp_bw_ib_factor = 290,
#else
	.mdp_bw_ab_factor = 200,
	.mdp_bw_ib_factor = 210,
#endif
	.mdp_bus_scale_table = &mdp_bus_scale_pdata,
	.mdp_rev = MDP_REV_44,
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	.mem_hid = BIT(ION_CP_MM_HEAP_ID),
#else
	.mem_hid = MEMTYPE_EBI1,
#endif
	/* for early backlight on for APQ8064 */
	.cont_splash_enabled = 0x01,
	.mdp_iommu_split_domain = 1,
};

void __init apq8064_mdp_writeback(struct memtype_reserve* reserve_table)
{
	mdp_pdata.ov0_wb_size = MSM_FB_OVERLAY0_WRITEBACK_SIZE;
	mdp_pdata.ov1_wb_size = MSM_FB_OVERLAY1_WRITEBACK_SIZE;
#if defined(CONFIG_ANDROID_PMEM) && !defined(CONFIG_MSM_MULTIMEDIA_USE_ION)
	reserve_table[mdp_pdata.mem_hid].size +=
		mdp_pdata.ov0_wb_size;
	reserve_table[mdp_pdata.mem_hid].size +=
		mdp_pdata.ov1_wb_size;
#endif
}

#ifdef CONFIG_LCD_KCAL
int kcal_set_values(int kcal_r, int kcal_g, int kcal_b)
{
	kcal_value.red = kcal_r;
	kcal_value.green = kcal_g;
	kcal_value.blue = kcal_b;
	return 0;
}

static int kcal_get_values(int *kcal_r, int *kcal_g, int *kcal_b)
{
	*kcal_r = kcal_value.red;
	*kcal_g = kcal_value.green;
	*kcal_b = kcal_value.blue;
	return 0;
}

static int kcal_refresh_values(void)
{
	return update_preset_lcdc_lut();
}

static struct kcal_platform_data kcal_pdata = {
	.set_values = kcal_set_values,
	.get_values = kcal_get_values,
	.refresh_display = kcal_refresh_values
};

static struct platform_device kcal_platrom_device = {
	.name   = "kcal_ctrl",
	.dev = {
		.platform_data = &kcal_pdata,
	}
};
#endif

static struct resource hdmi_msm_resources[] = {
	{
		.name  = "hdmi_msm_qfprom_addr",
		.start = 0x00700000,
		.end   = 0x007060FF,
		.flags = IORESOURCE_MEM,
	},
	{
		.name  = "hdmi_msm_hdmi_addr",
		.start = 0x04A00000,
		.end   = 0x04A00FFF,
		.flags = IORESOURCE_MEM,
	},
	{
		.name  = "hdmi_msm_irq",
		.start = HDMI_IRQ,
		.end   = HDMI_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static int hdmi_enable_5v(int on);
static int hdmi_core_power(int on, int show);
static int hdmi_cec_power(int on);
static int hdmi_gpio_config(int on);
static int hdmi_panel_power(int on);

static struct msm_hdmi_platform_data hdmi_msm_data = {
	.irq = HDMI_IRQ,
	.enable_5v = hdmi_enable_5v,
	.core_power = hdmi_core_power,
	.cec_power = hdmi_cec_power,
	.panel_power = hdmi_panel_power,
	.gpio_config = hdmi_gpio_config,
};

static struct platform_device hdmi_msm_device = {
	.name = "hdmi_msm",
	.id = 0,
	.num_resources = ARRAY_SIZE(hdmi_msm_resources),
	.resource = hdmi_msm_resources,
	.dev.platform_data = &hdmi_msm_data,
};

static char wfd_check_mdp_iommu_split_domain(void)
{
	return mdp_pdata.mdp_iommu_split_domain;
}

#ifdef CONFIG_FB_MSM_WRITEBACK_MSM_PANEL
static struct msm_wfd_platform_data wfd_pdata = {
	.wfd_check_mdp_iommu_split = wfd_check_mdp_iommu_split_domain,
};

static struct platform_device wfd_panel_device = {
	.name = "wfd_panel",
	.id = 0,
	.dev.platform_data = NULL,
};

static struct platform_device wfd_device = {
	.name          = "msm_wfd",
	.id            = -1,
	.dev.platform_data = &wfd_pdata,
};
#endif

/* HDMI related GPIOs */
#define HDMI_CEC_VAR_GPIO	69
#define HDMI_DDC_CLK_GPIO	70
#define HDMI_DDC_DATA_GPIO	71
#define HDMI_HPD_GPIO		72

struct lcd_delay {
	unsigned lcdvdd_lcdvdd;
	unsigned lcdvdd_iovcc;
	unsigned iovcc_vdda;
	unsigned vdda;

	unsigned vdda_iovcc;
	unsigned iovcc_lcdvdd;
};

/* Define to delay for power sequence */
static struct lcd_delay lcd_power_sequence_delay_LD089WU1 = {
	.lcdvdd_lcdvdd = 400, /* 400 ms  */
	.lcdvdd_iovcc = 120, /* 120 ms */
	.iovcc_vdda = 0, /* no delay */
	.vdda = 0, /* no delay */

	.vdda_iovcc = 1, /* 1 ms */
	.iovcc_lcdvdd = 10, /* 10ms */
};

static struct lcd_delay lcd_power_sequence_delay_LD083WU1 = {
	.lcdvdd_lcdvdd = 400, /* 400 ms  */
	.lcdvdd_iovcc = 0, /* no delay */
	.iovcc_vdda = 0, /* no delay */
	.vdda = 180, /* 180 ms */

	.vdda_iovcc = 1, /* 1 ms */
	.iovcc_lcdvdd = 10, /* 10ms */
};

static struct lcd_delay *lcd_power_sequence_delay = &lcd_power_sequence_delay_LD083WU1;

static bool dsi_power_on = false;
static int mipi_dsi_panel_power(int on)
{
	static struct regulator *reg_l2, *reg_lvs6;
	static int gpio26;	// LCD_VDD_EN (PM8921_GPIO_26)
	static u64 p_down = 0;
	static bool p_down_first = true;
	int rc;

	pr_debug("%s: state : %d\n", __func__, on);

	if (!dsi_power_on) /* LCD initial start (power side) */
       {
	       printk(KERN_INFO "[LCD][DEBUG] %s: mipi lcd power initial\n", __func__);

		reg_lvs6 = regulator_get(&msm_mipi_dsi1_device.dev, "dsi_iovcc");
		if (IS_ERR(reg_lvs6)) {
			pr_err("could not get 8921_lvs6, rc = %ld\n",
				 PTR_ERR(reg_lvs6));
			return -ENODEV;
		}

		reg_l2 = regulator_get(&msm_mipi_dsi1_device.dev, "dsi_vdda");
		if (IS_ERR(reg_l2)) {
			pr_err("could not get 8921_l2, rc = %ld\n",
				PTR_ERR(reg_l2));
			return -ENODEV;
		}

		rc = regulator_set_voltage(reg_l2, 1200000, 1200000);
		if (rc) {
			pr_err("set_voltage l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}

		gpio26 = PM8921_GPIO_PM_TO_SYS(26);

		rc = gpio_request(gpio26, "lcd_vdd_en");
		if (rc) {
			pr_err("request gpio 26 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		dsi_power_on = true;
	}

	if (on) /* LCD on start (power side) */
	{
		printk(KERN_INFO "[LCD][DEBUG] %s: lcd power on status (status=%d)\n", __func__, on);

		/* Enable delay between LCD_VDD to LCD_VDD. */
		if ((p_down_first == false) && (lcd_power_sequence_delay->lcdvdd_lcdvdd > 0)) {
			u64 dur_jiffies;
			u64 cur_jiffies = jiffies_64;

			if (cur_jiffies < p_down) {
				dur_jiffies = p_down - (~cur_jiffies) + 1;
			} else
				dur_jiffies = cur_jiffies - p_down;

#if HZ!=1000
			dur_jiffies = div_u64((dur_jiffies)*1000,HZ);
#endif
			if(dur_jiffies<lcd_power_sequence_delay->lcdvdd_lcdvdd) {
				mdelay(lcd_power_sequence_delay->lcdvdd_lcdvdd-dur_jiffies);
			}
		}

		/* Enable LDO for LCD_VDD 3.3V*/
		gpio_direction_output(gpio26, 1);

		/* Delay between LCDVCC to IOVCC. */
		if (lcd_power_sequence_delay->lcdvdd_iovcc)
			mdelay(lcd_power_sequence_delay->lcdvdd_iovcc);

		/* Set DSI VDDA current to 100mA */
		rc = regulator_set_optimum_mode(reg_l2, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}

		/* Enable DSI IOVCC 1.8V */
		rc = regulator_enable(reg_lvs6);
		if (rc) {
			pr_err("enable lvs6 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		/* Delay between IOVCC to VDDA. */
		if (lcd_power_sequence_delay->iovcc_vdda)
			mdelay(lcd_power_sequence_delay->iovcc_vdda);

		/* Enable DSI VDDA 1.2V*/
		rc = regulator_enable(reg_l2);
		if (rc) {
			pr_err("enable l2 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		/* Delay for VDDA. */
		if (lcd_power_sequence_delay->vdda)
			mdelay(lcd_power_sequence_delay->vdda);
              }
       else /* LCD off start (power side) */
       {
              printk(KERN_INFO "[LCD][DEBUG] %s: lcd power off (status=%d)\n", __func__, on);
		/* Disable DSI VDDA */
		rc = regulator_disable(reg_l2);
		if (rc) {
			pr_err("disable reg_l2  failed, rc=%d\n", rc);
			return -ENODEV;
		}

		/* Delay between VDDA to IOVCC. */
		if (lcd_power_sequence_delay->vdda_iovcc)
			mdelay(lcd_power_sequence_delay->vdda_iovcc);

		/* Disable DSI IOVCC */
		rc = regulator_disable(reg_lvs6);
		if (rc) {
		    pr_err("disable lvs6 failed, rc=%d\n", rc);
		    return -ENODEV;
		}

		/* Set DSI VDDA current to 100uA for power consumption */
		rc = regulator_set_optimum_mode(reg_l2, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}

		/* Delay between IOVCC to LCD_VDDA. */
		if (lcd_power_sequence_delay->iovcc_lcdvdd)
			mdelay(lcd_power_sequence_delay->iovcc_lcdvdd);

		/* Disable LDO for LCD_VDD */
		gpio_direction_output(gpio26, 0);

		if (lcd_power_sequence_delay->lcdvdd_lcdvdd > 0) {
			p_down = jiffies_64;
			p_down_first = false;
		}
	}
	return 0;
}

static char mipi_dsi_splash_is_enabled(void)
{
       return mdp_pdata.cont_splash_enabled;
}

static struct mipi_dsi_platform_data mipi_dsi_pdata = {
	.dsi_power_save = mipi_dsi_panel_power,
	.splash_is_enabled = mipi_dsi_splash_is_enabled,
};

static struct msm_bus_vectors dtv_bus_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};

static struct msm_bus_vectors dtv_bus_def_vectors[] = {
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WUXGA_PT) ||\
	defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WUXGA_INVERSE_PT)
		.ab = 2000000000,
		.ib = 2000000000,
#else
		.ab = 566092800 * 2,
		.ib = 707616000 * 2,
#endif
	},
};

static struct msm_bus_paths dtv_bus_scale_usecases[] = {
	{
		ARRAY_SIZE(dtv_bus_init_vectors),
		dtv_bus_init_vectors,
	},
	{
		ARRAY_SIZE(dtv_bus_def_vectors),
		dtv_bus_def_vectors,
	},
};
static struct msm_bus_scale_pdata dtv_bus_scale_pdata = {
	dtv_bus_scale_usecases,
	ARRAY_SIZE(dtv_bus_scale_usecases),
	.name = "dtv",
};

static struct lcdc_platform_data dtv_pdata = {
	.bus_scale_table = &dtv_bus_scale_pdata,
	.lcdc_power_save = hdmi_panel_power,
};

static int hdmi_panel_power(int on)
{
	int rc;

	pr_debug("%s: HDMI Core: %s\n", __func__, (on ? "ON" : "OFF"));
	rc = hdmi_core_power(on, 1);
	if (rc)
		rc = hdmi_cec_power(on);

	pr_debug("%s: HDMI Core: %s Success\n", __func__, (on ? "ON" : "OFF"));
	return rc;
}

static int hdmi_enable_5v(int on)
{
#ifdef CONFIG_HDMI_MVS
	/* TBD: PM8921 regulator instead of 8901 */
	static struct regulator *reg_8921_hdmi_mvs;	/* HDMI_5V */
	static int prev_on;
	int rc;

	if (on == prev_on)
		return 0;

	if (!reg_8921_hdmi_mvs) {
		reg_8921_hdmi_mvs = regulator_get(&hdmi_msm_device.dev,
			"hdmi_mvs");
		if (IS_ERR(reg_8921_hdmi_mvs)) {
			pr_err("could not get reg_8921_hdmi_mvs, rc = %ld\n",
				PTR_ERR(reg_8921_hdmi_mvs));
			reg_8921_hdmi_mvs = NULL;
			return -ENODEV;
		}
	}

	if (on) {
		rc = regulator_enable(reg_8921_hdmi_mvs);
		if (rc) {
			pr_err("'%s' regulator enable failed, rc=%d\n",
				"8921_hdmi_mvs", rc);
			return rc;
		}
		pr_debug("%s(on): success\n", __func__);
	} else {
		rc = regulator_disable(reg_8921_hdmi_mvs);
		if (rc)
			pr_warning("'%s' regulator disable failed, rc=%d\n",
				"8921_hdmi_mvs", rc);
		pr_debug("%s(off): success\n", __func__);
	}

	prev_on = on;
#endif

	return 0;
}

static int hdmi_core_power(int on, int show)
{
#ifdef CONFIG_HDMI_MVS
	static struct regulator *reg_8921_lvs7, *reg_8921_s4, *reg_ext_3p3v;
#else
	static struct regulator *reg_8921_lvs7;
#endif
	static int prev_on;
	int rc;

	if (on == prev_on)
		return 0;

#ifdef CONFIG_HDMI_MVS
	/* TBD: PM8921 regulator instead of 8901 */
	if (!reg_ext_3p3v) {
		reg_ext_3p3v = regulator_get(&hdmi_msm_device.dev,
					     "hdmi_mux_vdd");
		if (IS_ERR_OR_NULL(reg_ext_3p3v)) {
			pr_err("could not get reg_ext_3p3v, rc = %ld\n",
			       PTR_ERR(reg_ext_3p3v));
			reg_ext_3p3v = NULL;
			return -ENODEV;
		}
	}
#endif

	if (!reg_8921_lvs7) {
		reg_8921_lvs7 = regulator_get(&hdmi_msm_device.dev,
					      "hdmi_vdda");
		if (IS_ERR(reg_8921_lvs7)) {
			pr_err("could not get reg_8921_lvs7, rc = %ld\n",
				PTR_ERR(reg_8921_lvs7));
			reg_8921_lvs7 = NULL;
			return -ENODEV;
		}
	}
#ifdef CONFIG_HDMI_MVS
	if (!reg_8921_s4) {
		reg_8921_s4 = regulator_get(&hdmi_msm_device.dev,
					    "hdmi_lvl_tsl");
		if (IS_ERR(reg_8921_s4)) {
			pr_err("could not get reg_8921_s4, rc = %ld\n",
				PTR_ERR(reg_8921_s4));
			reg_8921_s4 = NULL;
			return -ENODEV;
		}
		rc = regulator_set_voltage(reg_8921_s4, 1800000, 1800000);
		if (rc) {
			pr_err("set_voltage failed for 8921_s4, rc=%d\n", rc);
			return -EINVAL;
		}
	}
#endif

	if (on) {
		/*
		 * Configure 3P3V_BOOST_EN as GPIO, 8mA drive strength,
		 * pull none, out-high
		 */
#ifdef CONFIG_HDMI_MVS
		rc = regulator_set_optimum_mode(reg_ext_3p3v, 290000);
		if (rc < 0) {
			pr_err("set_optimum_mode ext_3p3v failed, rc=%d\n", rc);
			return -EINVAL;
		}

		rc = regulator_enable(reg_ext_3p3v);
		if (rc) {
			pr_err("enable reg_ext_3p3v failed, rc=%d\n", rc);
			return rc;
		}
#endif
		rc = regulator_enable(reg_8921_lvs7);
		if (rc) {
			pr_err("'%s' regulator enable failed, rc=%d\n",
				"hdmi_vdda", rc);
			goto error1;
		}
#ifdef CONFIG_HDMI_MVS
		rc = regulator_enable(reg_8921_s4);
		if (rc) {
			pr_err("'%s' regulator enable failed, rc=%d\n",
				"hdmi_lvl_tsl", rc);
			goto error2;
		}
		pr_debug("%s(on): success\n", __func__);
#endif
	} else {
#ifdef CONFIG_HDMI_MVS
		rc = regulator_disable(reg_ext_3p3v);
		if (rc) {
			pr_err("disable reg_ext_3p3v failed, rc=%d\n", rc);
			return -ENODEV;
		}
#else
		rc = regulator_disable(reg_8921_lvs7);
		if (rc) {
			pr_err("disable reg_8921_l23 failed, rc=%d\n", rc);
			return -ENODEV;
		}
#endif

#ifdef CONFIG_HDMI_MVS
		rc = regulator_disable(reg_8921_s4);
		if (rc) {
			pr_err("disable reg_8921_s4 failed, rc=%d\n", rc);
			return -ENODEV;
		}
#endif
		pr_debug("%s(off): success\n", __func__);
	}

	prev_on = on;

	return 0;

#ifdef CONFIG_HDMI_MVS
error2:
	regulator_disable(reg_8921_lvs7);
error1:
	regulator_disable(reg_ext_3p3v);
	return rc;
#else
error1:
	return rc;
#endif
}

static int hdmi_gpio_config(int on)
{
	int rc = 0;
	static int prev_on;
#ifdef CONFIG_HDMI_MVS
	int pmic_gpio14 = PM8921_GPIO_PM_TO_SYS(14);
#endif

	if (on == prev_on)
		return 0;

	if (on) {
		rc = gpio_request(HDMI_DDC_CLK_GPIO, "HDMI_DDC_CLK");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_DDC_CLK", HDMI_DDC_CLK_GPIO, rc);
			goto error1;
		}
		rc = gpio_request(HDMI_DDC_DATA_GPIO, "HDMI_DDC_DATA");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_DDC_DATA", HDMI_DDC_DATA_GPIO, rc);
			goto error2;
		}
		rc = gpio_request(HDMI_HPD_GPIO, "HDMI_HPD");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_HPD", HDMI_HPD_GPIO, rc);
			goto error3;
		}
#ifdef CONFIG_HDMI_MVS
		if (machine_is_apq8064_liquid()) {
			rc = gpio_request(pmic_gpio14, "PMIC_HDMI_MUX_SEL");
			if (rc) {
				pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
					"PMIC_HDMI_MUX_SEL", 14, rc);
				goto error4;
			}
			gpio_set_value_cansleep(pmic_gpio14, 0);
		}
		pr_debug("%s(on): success\n", __func__);
#endif
	} else {
		gpio_free(HDMI_DDC_CLK_GPIO);
		gpio_free(HDMI_DDC_DATA_GPIO);
		gpio_free(HDMI_HPD_GPIO);

#ifdef CONFIG_HDMI_MVS
		if (machine_is_apq8064_liquid()) {
			gpio_set_value_cansleep(pmic_gpio14, 1);
			gpio_free(pmic_gpio14);
		}
#endif
		pr_debug("%s(off): success\n", __func__);
	}

	prev_on = on;
	return 0;

#ifdef CONFIG_HDMI_MVS
error4:
	gpio_free(HDMI_HPD_GPIO);
#endif
error3:
	gpio_free(HDMI_DDC_DATA_GPIO);
error2:
	gpio_free(HDMI_DDC_CLK_GPIO);
error1:
	return rc;
}

static int hdmi_cec_power(int on)
{
	static int prev_on;
	int rc;

	if (on == prev_on)
		return 0;

	if (on) {
		rc = gpio_request(HDMI_CEC_VAR_GPIO, "HDMI_CEC_VAR");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_CEC_VAR", HDMI_CEC_VAR_GPIO, rc);
			goto error;
		}
		pr_debug("%s(on): success\n", __func__);
	} else {
		gpio_free(HDMI_CEC_VAR_GPIO);
		pr_debug("%s(off): success\n", __func__);
	}

	prev_on = on;

	return 0;
error:
	return rc;
}

#if defined (CONFIG_BACKLIGHT_I2C_BL)
extern void i2c_bl_lcd_backlight_set_level_export( int level);
#endif

static int mipi_lgit_backlight_level(int level, int max, int min)
{
#if defined (CONFIG_BACKLIGHT_I2C_BL)
	i2c_bl_lcd_backlight_set_level_export(level);
#endif
	return 0;
}

/* LGE_CHANGE_START:ilhwan.ahn@lge.com, 2013.05.22, Change the initial sest for AWiFi */
static char exit_sleep_mode             [2] = {0x11,0x00};
static char display_on                  [2] = {0x29,0x00};
static char display_off                 [2] = {0x28,0x00};
static char enter_sleep_mode            [2] = {0x10,0x00};
static char set_address_mode            [2] = {0x36,0x40};

#define PF_16BIT 0x50
#define PF_18BIT 0x60
#define PF_24BIT 0x70
static char pixel_format		[2] = {0x3A, PF_24BIT};

#if defined(CONFIG_LGE_BACKLIGHT_CABC)
/* Enable CABC block */
static char cabc_enable_LD083WU1	[2] = {0xb9, 0x01};

/* Disable CABC block */
static char cabc_disable_LD083WU1	[2] = {0xb9, 0x00};

/* Set PWM duty */
static char set_pwm_duty_LD083WU1	[2] = {0xbb, 0xff};

/* Enable CE algorithm */
//static char ce_enable_LD083WU1		[2] = {0xb8, 0x01};

/* Disable CE algorithm */
//static char ce_disable_LD083WU1		[2] = {0xb8, 0x00};
#endif

#if defined(CONFIG_LGE_BACKLIGHT_CABC)
/* Disable bypass PWMI, Enable PWMO, Pol=High, 3 frame mask, Enable internal freq. gen. */
//static char cabc_reg0_LD089WU1 			  [2] = {0xe0, (1<<7)|(0<<6)|(1<<0)};

/* Disable bypass PWMI, Enable PWMO, Pol=High, 2 frame mask, Enable internal freq. gen. */
static char cabc_reg0_LD089WU1			[2] = {0xe0, (0<<7)|(0<<6)|(1<<5)|(2<<1)|(0<<0)};

/* PWMO freq. equal to internal gen. freq., PWM freq = 1.2KHz */
static char cabc_reg1_LD089WU1			[2] = {0xe1, (1<<7)|(1<<6)|(0<<4)|((20>>8)<<0)};

/* prd_divider[7:0]:20*prd_sel:1.2KHz */
static char cabc_reg2_LD089WU1			[2] = {0xe2, (20<<0)};

/* cabc_dither_enable:1: Enable CABC dither (for smooth), Modify_rgb:1:Modify RGB value according to PWM value */
static char cabc_reg4_LD089WU1			[2] = {0xe4, (1<<7)|(1<<6)};

/* Abrupt_threshold: 0:disable this function */
static char cabc_reg5_LD089WU1			[2] = {0xe5, 0};

/* Adjust_frame_rate:7, Adjust_step:7 */
static char cabc_reg6_LD089WU1			[2] = {0xe6, (7<<4)|(7<<0)};

/* Pwm_min:0x88 */
static char cabc_reg7_LD089WU1			[2] = {0xe7, 0x88};

/* Pwm_ref:0: by Pwm_set, en_inv_pwmo:0:PWMO */
static char cabc_reg8_LD089WU1			[2] = {0xe8, (0<<7)|(0<<4)};

/* Pwm_Set:45%, Maximum duty of PWMO when pwm_ref =0 */
static char cabc_reg9_LD089WU1			[2] = {0xe9, (unsigned char)(45*255/100)};

/* Average_ratio:0 : maximum Luminance, Allow_distort:255-(15)*2 */
static char cabc_rega_LD089WU1			[2] = {0xea, (0<<4)|(15<<0)};

/* Bypass_pwmi:0:disable, En_bfrm_maxpwm:0:disable */
static char cabc_regb_LD089WU1			[2] = {0xeb, (0<<6)|(0<<5)};

/* CABC_En:1:Enable */
static char cabc_reg0e_LD089WU1			[2] = {0x0e, (1<<3)};

/* Enter Page 0 access mode */
static char change_page0_mode_LD089WU1		[2] = {0xf3, 0xa0};

/* Enter Page 6 access mode */
static char change_page6_mode_LD089WU1		[2] = {0xf3, 0xa6};

/* Enter Page 7 access mode */
static char change_page7_mode_LD089WU1		[2] = {0xf3, 0xa7};


#if 0
static char cabc_lut1_LD089WU1			[65] = {0x00,
							0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C,
							0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C,
							0x40, 0x44, 0x48, 0x4C, 0x50, 0x55, 0x59, 0x5D,
							0x61, 0x65, 0x69, 0x6D, 0x71, 0x75, 0x79, 0x7D,
							0x81, 0x85, 0x89, 0x8D, 0x91, 0x95, 0x99, 0x9D,
							0xA1, 0xA5, 0xAA, 0xAE, 0xB2, 0xB6, 0xBA, 0xBE,
							0xC2, 0xC6, 0xCA, 0xCE, 0xD2, 0xD6, 0xDA, 0xDE,
							0xE2, 0xE6, 0xEA, 0xEE, 0xF2, 0xF6, 0xFA, 0xFF};
#endif

static char cabc_lut1_LD089WU1			[] = {0x00,
							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,

							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,

							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,

							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
							0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/* Enter DSI command mode */
static char change_dsi_cmd_mode_LD089WU1 	[2] = {0x00, 0x00};
#endif

/* initialize device */
static struct dsi_cmd_desc lgit_power_on_set_1_LD089WU1[] = {
	/*  Display Initial Set */
	{DTYPE_DCS_WRITE1, 1, 0, 0, 20, sizeof(set_address_mode),set_address_mode},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 20, sizeof(pixel_format),pixel_format},
};

static struct dsi_cmd_desc lgit_power_on_set_2_LD089WU1[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(exit_sleep_mode), exit_sleep_mode},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(display_on), display_on},
};

static struct dsi_cmd_desc lgit_power_off_set_LD089WU1[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 20, sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 1, 0, 0, 100, sizeof(enter_sleep_mode), enter_sleep_mode},
};

static struct dsi_cmd_desc lgit_shutdown_set_LD089WU1[] = {
       {DTYPE_DCS_WRITE, 1, 0, 0, 0,  sizeof(display_off), display_off},
};

#if defined(CONFIG_LGE_BACKLIGHT_CABC)
static struct dsi_cmd_desc lgit_power_on_set_3_LD089WU1[] = {
	{DTYPE_GEN_WRITE2,  1, 0, 0, 20, sizeof(change_page0_mode_LD089WU1), change_page0_mode_LD089WU1},
	{DTYPE_GEN_WRITE2,  1, 0, 0, 0, sizeof(cabc_reg0_LD089WU1), cabc_reg0_LD089WU1},
	{DTYPE_GEN_WRITE2,  1, 0, 0, 0, sizeof(cabc_reg1_LD089WU1), cabc_reg1_LD089WU1},
	{DTYPE_GEN_WRITE2,  1, 0, 0, 0, sizeof(cabc_reg2_LD089WU1), cabc_reg2_LD089WU1},
	{DTYPE_GEN_WRITE2,  1, 0, 0, 0, sizeof(cabc_reg4_LD089WU1), cabc_reg4_LD089WU1},
	{DTYPE_GEN_WRITE2,  1, 0, 0, 0, sizeof(cabc_reg5_LD089WU1), cabc_reg5_LD089WU1},
	{DTYPE_GEN_WRITE2,  1, 0, 0, 0, sizeof(cabc_reg6_LD089WU1), cabc_reg6_LD089WU1},
	{DTYPE_GEN_WRITE2,  1, 0, 0, 0, sizeof(cabc_reg7_LD089WU1), cabc_reg7_LD089WU1},
	{DTYPE_GEN_WRITE2,  1, 0, 0, 0, sizeof(cabc_reg8_LD089WU1), cabc_reg8_LD089WU1},
	{DTYPE_GEN_WRITE2,  1, 0, 0, 0, sizeof(cabc_reg9_LD089WU1), cabc_reg9_LD089WU1},
	{DTYPE_GEN_WRITE2,  1, 0, 0, 0, sizeof(cabc_rega_LD089WU1), cabc_rega_LD089WU1},
	{DTYPE_GEN_WRITE2,  1, 0, 0, 0, sizeof(cabc_regb_LD089WU1), cabc_regb_LD089WU1},
	{DTYPE_GEN_WRITE2,  1, 0, 0, 0, sizeof(cabc_reg0e_LD089WU1), cabc_reg0e_LD089WU1},

	{DTYPE_GEN_WRITE2,  1, 0, 0, 0, sizeof(change_page6_mode_LD089WU1), change_page6_mode_LD089WU1},
	{DTYPE_GEN_LWRITE,  1, 0, 0, 0, sizeof(cabc_lut1_LD089WU1), cabc_lut1_LD089WU1},

	{DTYPE_GEN_WRITE2,  1, 0, 0, 0, sizeof(change_page7_mode_LD089WU1), change_page7_mode_LD089WU1},
	{DTYPE_GEN_LWRITE,  1, 0, 0, 0, sizeof(cabc_lut1_LD089WU1), cabc_lut1_LD089WU1},

	{DTYPE_GEN_WRITE,   1, 0, 0, 20, 0, change_dsi_cmd_mode_LD089WU1},
};
#endif

static struct msm_panel_common_pdata mipi_lgit_pdata_LD089WU1 = {
	.backlight_level = mipi_lgit_backlight_level,
	.power_on_set_1 = lgit_power_on_set_1_LD089WU1,
	.power_on_set_size_1 = ARRAY_SIZE(lgit_power_on_set_1_LD089WU1),
	.power_on_set_2 = lgit_power_on_set_2_LD089WU1,
	.power_on_set_size_2 = ARRAY_SIZE(lgit_power_on_set_2_LD089WU1),
#if defined(CONFIG_LGE_BACKLIGHT_CABC)
	.power_on_set_3 = lgit_power_on_set_3_LD089WU1,
	.power_on_set_size_3 = ARRAY_SIZE(lgit_power_on_set_3_LD089WU1),
#endif
	.power_off_set_1 = lgit_power_off_set_LD089WU1,
	.power_off_set_size_1 = ARRAY_SIZE(lgit_power_off_set_LD089WU1),
	.power_off_set_2 = lgit_shutdown_set_LD089WU1,
	.power_off_set_size_2 = ARRAY_SIZE(lgit_shutdown_set_LD089WU1),
};

static struct dsi_cmd_desc lgit_power_on_set_1_LD083WU1[] = {
	/* Display Initial Set */
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(set_address_mode),set_address_mode},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(pixel_format),pixel_format},
};

static struct dsi_cmd_desc lgit_power_on_set_2_LD083WU1[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 10, sizeof(exit_sleep_mode), exit_sleep_mode},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(display_on), display_on},
};

#if defined(CONFIG_LGE_BACKLIGHT_CABC)
static struct dsi_cmd_desc lgit_power_on_set_3_LD083WU1[] = {
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(set_pwm_duty_LD083WU1),set_pwm_duty_LD083WU1},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(cabc_enable_LD083WU1),cabc_enable_LD083WU1},
	//{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(ce_enable_LD083WU1),ce_enable_LD083WU1},
};

static struct dsi_cmd_desc lgit_power_on_set_3_LD083WU1_noCABC[] = {
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(set_pwm_duty_LD083WU1),set_pwm_duty_LD083WU1},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(cabc_enable_LD083WU1),cabc_disable_LD083WU1},
};

#endif

static struct dsi_cmd_desc lgit_power_off_set_LD083WU1[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(enter_sleep_mode), enter_sleep_mode},
};

static struct dsi_cmd_desc lgit_shutdown_set_LD083WU1[] = {
       {DTYPE_DCS_WRITE, 1, 0, 0, 0,  sizeof(display_off), display_off},
};

static struct msm_panel_common_pdata mipi_lgit_pdata_LD083WU1 = {
	.backlight_level = mipi_lgit_backlight_level,
	.power_on_set_1 = lgit_power_on_set_1_LD083WU1,
	.power_on_set_size_1 = ARRAY_SIZE(lgit_power_on_set_1_LD083WU1),
	.power_on_set_2 = lgit_power_on_set_2_LD083WU1,
	.power_on_set_size_2 = ARRAY_SIZE(lgit_power_on_set_2_LD083WU1),
#if defined(CONFIG_LGE_BACKLIGHT_CABC)
	.power_on_set_3 = lgit_power_on_set_3_LD083WU1,
	.power_on_set_size_3 = ARRAY_SIZE(lgit_power_on_set_3_LD083WU1),
	.power_on_set_3_noCABC = lgit_power_on_set_3_LD083WU1_noCABC,
	.power_on_set_size_3_noCABC = ARRAY_SIZE(lgit_power_on_set_3_LD083WU1_noCABC),
#endif
	.power_off_set_1 = lgit_power_off_set_LD083WU1,
	.power_off_set_size_1 = ARRAY_SIZE(lgit_power_off_set_LD083WU1),
	.power_off_set_2 = lgit_shutdown_set_LD083WU1,
	.power_off_set_size_2 = ARRAY_SIZE(lgit_shutdown_set_LD083WU1),
};

static struct msm_panel_common_pdata mipi_lgit_pdata_LD083WU1_noCABC = {
	.backlight_level = mipi_lgit_backlight_level,
	.power_on_set_1 = lgit_power_on_set_1_LD083WU1,
	.power_on_set_size_1 = ARRAY_SIZE(lgit_power_on_set_1_LD083WU1),
	.power_on_set_2 = lgit_power_on_set_2_LD083WU1,
	.power_on_set_size_2 = ARRAY_SIZE(lgit_power_on_set_2_LD083WU1),
#if defined(CONFIG_LGE_BACKLIGHT_CABC)
	.power_on_set_3 = lgit_power_on_set_3_LD083WU1_noCABC,
	.power_on_set_size_3 = ARRAY_SIZE(lgit_power_on_set_3_LD083WU1_noCABC),
	.power_on_set_3_noCABC = lgit_power_on_set_3_LD083WU1_noCABC,
	.power_on_set_size_3_noCABC = ARRAY_SIZE(lgit_power_on_set_3_LD083WU1_noCABC),
#endif
	.power_off_set_1 = lgit_power_off_set_LD083WU1,
	.power_off_set_size_1 = ARRAY_SIZE(lgit_power_off_set_LD083WU1),
	.power_off_set_2 = lgit_shutdown_set_LD083WU1,
	.power_off_set_size_2 = ARRAY_SIZE(lgit_shutdown_set_LD083WU1),
};


static struct platform_device mipi_dsi_lgit_panel_device = {
	.name = "mipi_lgit",
	.id = 0,
	.dev = {
		.platform_data = &mipi_lgit_pdata_LD083WU1,
	}
};
#if defined(CONFIG_LGE_BACKLIGHT_CABC)
static struct platform_device mipi_dsi_lgit_panel_device_noCABC = {
	.name = "mipi_lgit",
	.id = 0,
	.dev = {
		.platform_data = &mipi_lgit_pdata_LD083WU1_noCABC,
	}
};
#endif
/* LGE_CHANGE_END:ilhwan.ahn@lge.com, 2013.05.22, Change the initial sest for AWiFi */

static struct platform_device *awifi_panel_devices[] __initdata = {
#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WUXGA_PT) ||\
	defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WUXGA_INVERSE_PT)
	&mipi_dsi_lgit_panel_device,
#endif
#ifdef CONFIG_LCD_KCAL
	&kcal_platrom_device,
#endif
};

#if defined(CONFIG_LGE_BACKLIGHT_CABC)
static struct platform_device *awifi_panel_devices_noCABC[] __initdata = {
#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WUXGA_PT) ||\
	defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WUXGA_INVERSE_PT)
	&mipi_dsi_lgit_panel_device_noCABC,
#endif
#ifdef CONFIG_LCD_KCAL
	&kcal_platrom_device,
#endif
};
#endif

void __init apq8064_init_fb(void)
{
    hw_rev_type lge_board_rev;

	platform_device_register(&msm_fb_device);

#ifdef CONFIG_FB_MSM_WRITEBACK_MSM_PANEL
	platform_device_register(&wfd_panel_device);
	platform_device_register(&wfd_device);
#endif /* CONFIG_FB_MSM_WRITEBACK_MSM_PANEL */

/* LGE_CHANGE_START: ilhwan.ahn@lge.com, 2013.05.27,  For the one-binary, runtime configuration */
    lge_board_rev = lge_get_board_revno();

	if (lge_board_rev == HW_REV_EVB2) {
		lcd_power_sequence_delay = &lcd_power_sequence_delay_LD089WU1;
		mipi_dsi_lgit_panel_device.dev.platform_data = &mipi_lgit_pdata_LD089WU1;
	}
/* LGE_CHANGE_END: ilhwan.ahn@lge.com, 2013.05.27,  For the one-binary, runtime configuration */
#if defined(CONFIG_LGE_BACKLIGHT_CABC)
	if (lge_board_rev > HW_REV_1_0) {
	    platform_add_devices(awifi_panel_devices,ARRAY_SIZE(awifi_panel_devices));
    } else {
        platform_add_devices(awifi_panel_devices_noCABC,ARRAY_SIZE(awifi_panel_devices_noCABC));
    }
#else
    platform_add_devices(awifi_panel_devices,ARRAY_SIZE(awifi_panel_devices));
#endif
	msm_fb_register_device("mdp", &mdp_pdata);
	msm_fb_register_device("mipi_dsi", &mipi_dsi_pdata);
	platform_device_register(&hdmi_msm_device);
	msm_fb_register_device("dtv", &dtv_pdata);
}

/**
 * Set MDP clocks to high frequency to avoid DSI underflow
 * when using high resolution 1200x1920 WUXGA panels
 */

#define I2C_SURF 1
#define I2C_FFA  (1 << 1)
#define I2C_RUMI (1 << 2)
#define I2C_SIM  (1 << 3)
#define I2C_LIQUID (1 << 4)

struct i2c_registry {
	u8                     machs;
	int                    bus;
	struct i2c_board_info *info;
	int                    len;
};

#if defined(CONFIG_LGE_BACKLIGHT_CABC)
#define PWM_SIMPLE_EN 0xA0
#define PWM_BRIGHTNESS 0x20
#endif

struct backlight_platform_data {
	void (*platform_init)(void);
	int gpio;
	unsigned int mode;
	int max_current;
	int init_on_boot;
	int min_brightness;
	int max_brightness;
	int default_brightness;
	int factory_brightness;
};

#if defined(CONFIG_BACKLIGHT_I2C_BL)
static char i2c_bl_mapped_lm3532_value[256] = {
	  119,119,119,119,119,119,119,119,124,124,124,124,124,124,124,
	124,124,124,124,124,124,124,128,128,128,128,128,128,128,128,
	128,128,128,128,128,131,131,131,131,131,131,131,135,135,138,
	138,141,141,143,143,146,146,148,148,151,151,151,153,153,153,
	153,155,155,155,155,157,157,159,159,159,161,161,162,162,162,
	164,164,165,165,166,166,168,168,168,170,170,171,171,172,172,
	173,173,174,174,176,176,177,178,179,180,180,181,181,182,183,
	183,184,184,185,185,186,186,187,187,188,188,189,189,189,190,
	190,190,191,191,192,193,193,194,194,195,195,196,197,197,198,
	198,199,200,201,201,202,202,203,203,204,204,205,206,206,207,
	208,209,210,211,212,213,214,215,215,215,215,216,216,216,217,
	217,218,218,219,219,220,220,220,221,221,222,222,222,223,223,
	224,224,225,225,226,226,227,227,228,228,228,229,229,230,230,
	231,232,233,233,233,234,234,235,235,236,236,236,237,237,237,
	238,238,239,239,240,240,241,241,242,242,243,243,243,244,244,
	244,245,245,245,246,246,246,247,247,248,248,248,249,249,249,
	250,250,250,251,251,251,252,252,253,253,253,254,254,254,254,
	255,
};

static struct i2c_bl_cmd i2c_bl_init_lm3532_cmd[] = {
	{0x10, 0x00, 0xff, "ILED1, ILED2, and ILED3 is controlled by Control A PWM and Control A Brightness Registers"},
	{0x1d, 0x01, 0xff, "Enable LED A"},
#if defined(CONFIG_LGE_BACKLIGHT_CABC)
	{0x13, 0x06, 0xff, "Active PWM input is enabled in Zone 0, active high polarity, PWM2 is mapped to Control Bank A"},
	{0x16, 0x01, 0xff, "Control A Zone Target 0, Exponential Mapping, I2C Current Control"},
#else
	{0x16, 0x01, 0xff, "Control A Zone Target 0, Exponential Mapping, I2C Current Control"},
#endif // CABC apply
	{0x17, 0x13, 0xff, "Full-Scale Current (20.2mA) of BANK A"},
};

static struct i2c_bl_cmd i2c_bl_deinit_lm3532_cmd[] = {
	{0x1d, 0x00, 0xff, "Disable LED A"},
};

static struct i2c_bl_cmd i2c_bl_dump_lm3532_regs[] = {
	{0x10, 0x00, 0xff, "Output Configuration Register"},
	{0x11, 0x00, 0xff, "Startup/Shutdown Ramp Rate Register"},
	{0x12, 0x00, 0xff, "Run Time Ramp Rate Register"},
	{0x13, 0x00, 0xff, "Control A PWM Register"},
	{0x14, 0x00, 0xff, "Control B PWM Register"},
	{0x15, 0x00, 0xff, "Control C PWM Register"},
	{0x16, 0x00, 0xff, "Control A Brightness Configuration Register"},
	{0x18, 0x00, 0xff, "Control B Brightness Configuration Register"},
	{0x1a, 0x00, 0xff, "Control C Brightness Configuration Register"},
	{0x17, 0x00, 0xff, "Control A Full-Scale Current Registers"},
	{0x19, 0x00, 0xff, "Control B Full-Scale Current Registers"},
	{0x1b, 0x00, 0xff, "Control C Full-Scale Current Registers"},
	{0x1c, 0x00, 0xff, "Feedback Enable Register"},
	{0x1d, 0x00, 0xff, "Control Enable Register"},
	{0x70, 0x00, 0xff, "Control A Zone Target Register 0 maps directly to Zone 0"},
	{0x75, 0x00, 0xff, "Control B Zone Target Register 0 maps directly to Zone 0"},
	{0x7a, 0x00, 0xff, "Control C Zone Target Register 0 maps directly to Zone 0"},
};

static struct i2c_bl_cmd i2c_bl_set_get_brightness_lm3532_cmds[] = {
	{0x70, 0x00, 0xff, "Set/Get brightness"},
};

static struct i2c_bl_platform_data lm3532_i2c_bl_data = {
	.gpio = PM8921_GPIO_PM_TO_SYS(24),
	.i2c_addr = 0x38,
	.min_brightness = 0x78,
	.max_brightness = 0x05,
	.default_brightness = 0x9C,
	.factory_brightness = 0x78,

	.init_cmds = i2c_bl_init_lm3532_cmd,
	.init_cmds_size = ARRAY_SIZE(i2c_bl_init_lm3532_cmd),

	.deinit_cmds = i2c_bl_deinit_lm3532_cmd,
	.deinit_cmds_size = ARRAY_SIZE(i2c_bl_deinit_lm3532_cmd),

	.dump_regs = i2c_bl_dump_lm3532_regs,
	.dump_regs_size = ARRAY_SIZE(i2c_bl_dump_lm3532_regs),

	.set_brightness_cmds = i2c_bl_set_get_brightness_lm3532_cmds,
	.set_brightness_cmds_size = ARRAY_SIZE(i2c_bl_set_get_brightness_lm3532_cmds),

	.get_brightness_cmds = i2c_bl_set_get_brightness_lm3532_cmds,
	.get_brightness_cmds_size = ARRAY_SIZE(i2c_bl_set_get_brightness_lm3532_cmds),

	.blmap = i2c_bl_mapped_lm3532_value,
	.blmap_size = ARRAY_SIZE(i2c_bl_mapped_lm3532_value),
};

static char i2c_bl_mapped_lp8556_value[256] = {
	  3,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  4,  4,   // 14
	  4,  4,  4,  4,  4,  4,  4,  5,  5,  5,  5,  5,  5,  5,  5,   // 29
	  5,  5,  5,  5,  5,  6,  6,  6,  6,  6,  6,  6,  6,  7,  7,   // 44
	  7,  8,  8,  8,  9,  9,  9,  9,  9, 10, 10, 10, 11, 11, 11,   // 59
	 12, 12, 12, 12, 13, 13, 13, 13, 14, 14, 15, 15, 15, 16, 16,   // 74
	 17, 17, 17, 18, 18, 18, 19, 19, 20, 21, 22, 22, 23, 24, 24,   // 89
	 25, 25, 26, 26, 27, 28, 28, 29, 29, 30, 30, 31, 31, 32, 32,   // 104
	 33, 34, 35, 35, 36, 36, 37, 38, 39, 39, 40, 41, 41, 42, 43,   // 119
	 44, 45, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57,   // 134
	 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72,   // 149
	 73, 74, 75, 76, 76, 77, 78, 80, 81, 82, 83, 85, 86, 87, 88,   // 164
	 89, 90, 91, 93, 95, 96, 97, 99,100,102,103,104,106,107,108,   // 199
	109,110,112,114,115,117,119,121,123,125,127,128,129,130,132,   // 204
	133,135,136,138,139,140,142,144,146,148,150,151,153,154,156,   // 219
	157,158,159,161,163,164,165,167,168,170,173,175,177,180,184,   // 224
	186,188,191,194,197,199,201,203,205,207,209,211,213,215,217,   // 239
	219,221,223,225,227,228,230,232,235,238,240,243,246,249,252,   // 244
	255
};

static struct i2c_bl_cmd i2c_bl_init_lp8556_cmd[] = {
#if defined(CONFIG_LGE_BACKLIGHT_CABC) && 0 /* Always disable CABC for 8.9" panel */
	{0x01, 0x01|(0x01<<1), 0xff, "Device enable, PWM input and Brightness register (combined before shaper block)"},
#else
	{0x01, 0x01|(0x02<<1), 0xff,"Device enable, Brightness register only"},
#endif
	{0x16, 0x1f, 0xff, "LED string enable: 0 to 4"},
	{0xa0, (((int)(17.5/20*4095)))&0xff, 0xff, "Current = 17.499"},
	{0xa1, (0x0<<7)|(0x03<<4)|(((int)(17.5/20*4095))>>8), 0xff, "Current_Max = 20mA"},
	{0xa2, 0x20, 0xff, "ISET_EN=0, BOOST_FSET_EN=0, UVLO_EN=1"},
	{0x9e, 0x20, 0x20, "VBOOST_RANGE=0"},
	{0xa5, (2<<4)|(0x08<<0), 0x7f,"PS_MODE[2:0] = 1,5 Phase, 5 drivers, PWM_FREQ=0x0F(38,464Hz), 0x08(15,626Hz)"},
};

static struct i2c_bl_cmd i2c_bl_deinit_lp8556_cmd[] = {
	{0x01, 0x00, 0xff, "Device enable, Brightness register only"},
};

static struct i2c_bl_cmd i2c_bl_dump_lp8556_regs[] = {
	{0x00, 0x00, 0xff, "Brightness Control register"},
	{0x01, 0x00, 0xff, "Device Control register"},
	{0x02, 0x00, 0xff, "Fault register"},
	{0x03, 0x00, 0xff, "Identification register"},
	{0x04, 0x00, 0xff, "Direct Control register"},
	{0x16, 0x00, 0xff, "Temp LSB register"},
	{0x98, 0x00, 0xff, "CFG98 register"},
	{0x9e, 0x00, 0xff, "CFG9E register"},
	{0xa0, 0x00, 0xff, "CFG0 register"},
	{0xa1, 0x00, 0xff, "CFG1 register"},
	{0xa2, 0x00, 0xff, "CFG2 register"},
	{0xa3, 0x00, 0xff, "CFG3 register"},
	{0xa4, 0x00, 0xff, "CFG4 register"},
	{0xa5, 0x00, 0xff, "CFG5 register"},
	{0xa6, 0x00, 0xff, "CFG6 register"},
	{0xa7, 0x00, 0xff, "CFG7 register"},
	{0xa9, 0x00, 0xff, "CFG9 register"},
	{0xaa, 0x00, 0xff, "CFGA register"},
	{0xae, 0x00, 0xff, "CFGE register"},
	{0xaf, 0x00, 0xff, "CFGF register"},
};

static struct i2c_bl_cmd i2c_bl_set_get_brightness_lp8556_cmds[] = {
	{0x00, 0x00, 0xff, "Brightness Control register"},
};

static struct i2c_bl_platform_data lp8556_i2c_bl_data = {
	.gpio = PM8921_GPIO_PM_TO_SYS(24),
	.i2c_addr = 0x2c,
	.min_brightness = 0x05,
	.max_brightness = 0xFF,
	.default_brightness = 0x9C,
	.factory_brightness = 0x78,

	.init_cmds = i2c_bl_init_lp8556_cmd,
	.init_cmds_size = ARRAY_SIZE(i2c_bl_init_lp8556_cmd),

	.deinit_cmds = i2c_bl_deinit_lp8556_cmd,
	.deinit_cmds_size = ARRAY_SIZE(i2c_bl_deinit_lp8556_cmd),

	.dump_regs = i2c_bl_dump_lp8556_regs,
	.dump_regs_size = ARRAY_SIZE(i2c_bl_dump_lp8556_regs),

	.set_brightness_cmds = i2c_bl_set_get_brightness_lp8556_cmds,
	.set_brightness_cmds_size = ARRAY_SIZE(i2c_bl_set_get_brightness_lp8556_cmds),

	.get_brightness_cmds = i2c_bl_set_get_brightness_lp8556_cmds,
	.get_brightness_cmds_size = ARRAY_SIZE(i2c_bl_set_get_brightness_lp8556_cmds),

	.blmap = i2c_bl_mapped_lp8556_value,
	.blmap_size = ARRAY_SIZE(i2c_bl_mapped_lp8556_value),
};
#endif

static struct i2c_board_info msm_i2c_backlight_info[] = {
#if defined(CONFIG_BACKLIGHT_I2C_BL)
	{ I2C_BOARD_INFO("i2c_bl", 0x38), .platform_data = &lm3532_i2c_bl_data, },
#endif
};
static struct i2c_registry apq8064_i2c_backlight_device[] __initdata = {

	{
		I2C_SURF | I2C_FFA | I2C_RUMI | I2C_SIM | I2C_LIQUID,
		APQ_8064_GSBI1_QUP_I2C_BUS_ID,
		msm_i2c_backlight_info,
		ARRAY_SIZE(msm_i2c_backlight_info),
	},
};

void __init register_i2c_backlight_devices(void)
{
	u8 mach_mask = 0;
	int i;

	/* Build the matching 'supported_machs' bitmask */
	if (machine_is_apq8064_cdp())
		mach_mask = I2C_SURF;
	else if (machine_is_apq8064_mtp())
		mach_mask = I2C_FFA;	
	else if (machine_is_apq8064_liquid())
		mach_mask = I2C_LIQUID;
	else if (machine_is_apq8064_rumi3())
		mach_mask = I2C_RUMI;
	else if (machine_is_apq8064_sim())
		mach_mask = I2C_SIM;
	else if (machine_is_apq8064_awifi())
		mach_mask = I2C_FFA;
	else
		pr_err("unmatched machine ID in register_i2c_devices\n");

/* LGE_CHANGE_START: ilhwan.ahn@lge.com, 2013.05.27,  For the one-binary */
#if defined(CONFIG_BACKLIGHT_I2C_BL)
	if (lge_get_board_revno() == HW_REV_EVB2) {
		struct i2c_board_info *board_info = msm_i2c_backlight_info;
		while(board_info<&msm_i2c_backlight_info[ARRAY_SIZE(msm_i2c_backlight_info)]) {
			if (strcmp(board_info->type, "i2c_bl") == 0) {
				board_info->addr = lp8556_i2c_bl_data.i2c_addr;
				board_info->platform_data = &lp8556_i2c_bl_data;
				break;
			}
		}
	}
#endif
/* LGE_CHANGE_END: ilhwan.ahn@lge.com, 2013.05.27,  For the one-binary */

	/* Run the array and install devices as appropriate */
	for (i = 0; i < ARRAY_SIZE(apq8064_i2c_backlight_device); ++i) {
		if (apq8064_i2c_backlight_device[i].machs & mach_mask)
			i2c_register_board_info(apq8064_i2c_backlight_device[i].bus,
						apq8064_i2c_backlight_device[i].info,
						apq8064_i2c_backlight_device[i].len);
	}
}
