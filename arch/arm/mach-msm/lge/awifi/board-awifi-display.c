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
#if defined(CONFIG_BACKLIGHT_I2C_BL)
#include <linux/i2c_bl.h>
#endif
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

#include "devices.h"
#include "board-awifi.h"

#ifdef CONFIG_LGE_KCAL
#ifdef CONFIG_LGE_QC_LCDC_LUT
extern int set_qlut_kcal_values(int kcal_r, int kcal_g, int kcal_b);
extern int refresh_qlut_display(void);
#else
#error only kcal by Qucalcomm LUT is supported now!!!
#endif
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

static int msm_fb_detect_panel(const char *name)
{
	return 0;
}

static struct msm_fb_platform_data msm_fb_pdata = {
	.detect_client = msm_fb_detect_panel,
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

#ifdef CONFIG_LGE_KCAL
extern int set_kcal_values(int kcal_r, int kcal_g, int kcal_b);
extern int refresh_kcal_display(void);
extern int get_kcal_values(int *kcal_r, int *kcal_g, int *kcal_b);

static struct kcal_platform_data kcal_pdata = {
	.set_values = set_kcal_values,
	.get_values = get_kcal_values,
	.refresh_display = refresh_kcal_display
};

static struct platform_device kcal_platrom_device = {
	.name   = "kcal_ctrl",
	.dev = {
		.platform_data = &kcal_pdata,
	}
};
#endif /* CONFIG_LGE_KCAL */

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

#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WUXGA_PT)
static struct lcd_delay lcd_power_sequence_delay_LD083WU1 = {
	.lcdvdd_lcdvdd = 400, /* 400 ms  */
	.lcdvdd_iovcc = 0, /* no delay */
	.iovcc_vdda = 0, /* no delay */
	.vdda = 180, /* 180 ms */

	.vdda_iovcc = 1, /* 1 ms */
	.iovcc_lcdvdd = 10, /* 10ms */
};

static struct lcd_delay *lcd_power_sequence_delay = &lcd_power_sequence_delay_LD083WU1;
#endif

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
	} else { /* LCD off start (power side) */
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
	return 0;
}

static int hdmi_core_power(int on, int show)
{
	static struct regulator *reg_8921_lvs7;
	static int prev_on;
	int rc;

	if (on == prev_on)
		return 0;

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

	if (on) {
		/*
		 * Configure 3P3V_BOOST_EN as GPIO, 8mA drive strength,
		 * pull none, out-high
		 */
		rc = regulator_enable(reg_8921_lvs7);
		if (rc) {
			pr_err("'%s' regulator enable failed, rc=%d\n",
				"hdmi_vdda", rc);
			goto error1;
		}
	} else {
		rc = regulator_disable(reg_8921_lvs7);
		if (rc) {
			pr_err("disable reg_8921_l23 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		pr_debug("%s(off): success\n", __func__);
	}

	prev_on = on;

	return 0;

error1:
	return rc;
}

static int hdmi_gpio_config(int on)
{
	int rc = 0;
	static int prev_on;

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
	} else {
		gpio_free(HDMI_DDC_CLK_GPIO);
		gpio_free(HDMI_DDC_DATA_GPIO);
		gpio_free(HDMI_HPD_GPIO);

		pr_debug("%s(off): success\n", __func__);
	}

	prev_on = on;
	return 0;

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
static char enter_sleep_mode            [2] = {0x10,0x00};

static char display_on                  [2] = {0x29,0x00};
static char display_off                 [2] = {0x28,0x00};

static char set_address_mode            [2] = {0x36,0x40};

static char p_gamma_r_setting[10] = {0xD0, 0x72, 0x15, 0x76, 0x00, 0x00, 0x00, 0x50, 0x30, 0x02};
static char n_gamma_r_setting[10] = {0xD1, 0x72, 0x15, 0x76, 0x00, 0x00, 0x00, 0x50, 0x30, 0x02};
static char p_gamma_g_setting[10] = {0xD2, 0x72, 0x15, 0x76, 0x00, 0x00, 0x00, 0x50, 0x30, 0x02};
static char n_gamma_g_setting[10] = {0xD3, 0x72, 0x15, 0x76, 0x00, 0x00, 0x00, 0x50, 0x30, 0x02};
static char p_gamma_b_setting[10] = {0xD4, 0x72, 0x15, 0x76, 0x00, 0x00, 0x00, 0x50, 0x30, 0x02};
static char n_gamma_b_setting[10] = {0xD5, 0x72, 0x15, 0x76, 0x00, 0x00, 0x00, 0x50, 0x30, 0x02};

#define PF_16BIT 0x50
#define PF_18BIT 0x60
#define PF_24BIT 0x70
static char pixel_format		[2] = {0x3A, PF_24BIT};

#if defined(CONFIG_LGE_BACKLIGHT_CABC)
/*                   */
static char cabc_enable_LD083WU1	[2] = {0xb9, 0x01};

/*                    */
static char cabc_disable_LD083WU1	[2] = {0xb9, 0x00};

/*              */
static char set_pwm_duty_LD083WU1	[2] = {0xbb, 0xff};
                                                  
#endif

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

	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(p_gamma_r_setting), p_gamma_r_setting},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(n_gamma_r_setting), n_gamma_r_setting},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(p_gamma_g_setting), p_gamma_g_setting},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(n_gamma_g_setting), n_gamma_g_setting},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(p_gamma_b_setting), p_gamma_b_setting},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(n_gamma_b_setting), n_gamma_b_setting},
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
	.power_on_set_2 = lgit_power_on_set_2_LD083WU1,
	.power_on_set_3 = lgit_power_on_set_3_LD083WU1,
	.power_on_set_3_noCABC = lgit_power_on_set_3_LD083WU1_noCABC,

	.power_on_set_size_1 = ARRAY_SIZE(lgit_power_on_set_1_LD083WU1),
	.power_on_set_size_2 = ARRAY_SIZE(lgit_power_on_set_2_LD083WU1),
	.power_on_set_size_3 = ARRAY_SIZE(lgit_power_on_set_3_LD083WU1),
	.power_on_set_size_3_noCABC = ARRAY_SIZE(lgit_power_on_set_3_LD083WU1_noCABC),

	.power_off_set_1 = lgit_power_off_set_LD083WU1,
	.power_off_set_2 = lgit_shutdown_set_LD083WU1,

	.power_off_set_size_1 = ARRAY_SIZE(lgit_power_off_set_LD083WU1),
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
#ifdef CONFIG_LGE_KCAL
	&kcal_platrom_device,
#endif
};

#if defined(CONFIG_LGE_BACKLIGHT_CABC)
static struct platform_device *awifi_panel_devices_noCABC[] __initdata = {
#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WUXGA_PT) ||\
	defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WUXGA_INVERSE_PT)
	&mipi_dsi_lgit_panel_device_noCABC,
#endif
#ifdef CONFIG_LGE_KCAL
	&kcal_platrom_device,
#endif
};
#endif

void __init apq8064_init_fb(void)
{
#if defined(CONFIG_LGE_BACKLIGHT_CABC)
	hw_rev_type lge_board_rev = lge_get_board_revno();
#endif

	platform_device_register(&msm_fb_device);
#ifdef CONFIG_FB_MSM_WRITEBACK_MSM_PANEL
	platform_device_register(&wfd_panel_device);
	platform_device_register(&wfd_device);
#endif /* CONFIG_FB_MSM_WRITEBACK_MSM_PANEL */

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
	.min_brightness = 0x05,
	.max_brightness = 0xFF,
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

#endif
static struct i2c_board_info msm_i2c_backlight_info[] = {
#if defined(CONFIG_BACKLIGHT_I2C_BL)
	{ I2C_BOARD_INFO("i2c_bl", 0x38), .platform_data = &lm3532_i2c_bl_data, },
#endif
};

static struct i2c_registry apq8064_i2c_backlight_device[] __initdata = {
	{
		I2C_FFA,
		APQ_8064_GSBI1_QUP_I2C_BUS_ID,
		msm_i2c_backlight_info,
		ARRAY_SIZE(msm_i2c_backlight_info),
	},
};

void __init register_i2c_backlight_devices(void)
{
	int i;

	/* Run the array and install devices as appropriate */
	for (i = 0; i < ARRAY_SIZE(apq8064_i2c_backlight_device); ++i) {
		i2c_register_board_info(apq8064_i2c_backlight_device[i].bus,
					apq8064_i2c_backlight_device[i].info,
					apq8064_i2c_backlight_device[i].len);
	}
}
