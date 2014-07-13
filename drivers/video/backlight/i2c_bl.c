/* drivers/video/backlight/i2c_bl.c
  *
  * Copyright (C) 2013 LGE, Inc.
  *
  * This software is licensed under the terms of the GNU General Public
  * License version 2, as published by the Free Software Foundation, and
  * may be copied, distributed, and modified under those terms.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  *
  */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/i2c_bl.h>
#include <linux/workqueue.h>

#include <mach/board_lge.h>

#define I2C_BL_NAME                              "i2c_bl"
 
#define BL_ON        1
#define BL_OFF       0

static DEFINE_MUTEX(backlight_mtx);

struct i2c_bl_device {
	struct i2c_client *client;
	struct backlight_device *bl_dev;
	int gpio;
	int max_current;
	int min_brightness;
	int max_brightness;
	int default_brightness;
	int factory_brightness;
	char *blmap;
	int blmap_size
};


static int cur_main_lcd_level;
static int saved_main_lcd_level;
static int backlight_status = BL_ON;

static struct i2c_bl_device *main_i2c_bl_dev;

static const struct i2c_device_id i2c_bl_id[] = {
	{ I2C_BL_NAME, 0 },
	{ },
};

static int i2c_bl_read_reg(struct i2c_client *client, u8 reg, u8 *buf);
static int i2c_bl_write_reg(struct i2c_client *client, unsigned char reg, unsigned char val);
static int i2c_bl_write_regs(struct i2c_client *client, struct i2c_bl_cmd *bl_cmds, int size);
static int i2c_bl_set_regs(struct i2c_client *client, struct i2c_bl_cmd *bl_cmds, int size, unsigned char value);

static void i2c_bl_lcd_backlight_set_level(struct i2c_client *client, int level);

void i2c_bl_lcd_backlight_set_level_export(int level)
{
	if (main_i2c_bl_dev != NULL &&
		main_i2c_bl_dev->client != NULL) {
		i2c_bl_lcd_backlight_set_level(main_i2c_bl_dev->client, level);
	} else {
		pr_err("%s(): No client\n", __func__);
	}
}
EXPORT_SYMBOL(i2c_bl_lcd_backlight_set_level_export);

static void i2c_bl_hw_reset(struct i2c_client *client)
{
	//Disable warning: struct i2c_bl_device *i2c_bl_dev = (struct i2c_bl_device *)i2c_get_clientdata(client);
	struct i2c_bl_platform_data *pdata = client->dev.platform_data;
	int gpio = pdata->gpio;

	if (gpio_is_valid(gpio)) {
		gpio_direction_output(gpio, 1);
		gpio_set_value_cansleep(gpio, 1);
		mdelay(1);
	}
}

static int i2c_bl_read_reg(struct i2c_client *client, u8 reg, u8 *buf)
{
    s32 ret;

    pr_debug("[LCD][DEBUG] reg: %x\n", reg);

    ret = i2c_smbus_read_byte_data(client, reg);

    if(ret < 0)
           pr_err("[LCD][DEBUG] error\n");

    *buf = ret;

    return ret;
}

static int i2c_bl_write_reg(struct i2c_client *client, unsigned char reg, unsigned char val)
{
	u8 buf[2];
	struct i2c_msg msg = {
		client->addr, 0, 2, buf
	};

	buf[0] = reg;
	buf[1] = val;

	if (i2c_transfer(client->adapter, &msg, 1) < 0)
		dev_err(&client->dev, "i2c write error\n");

	return 0;
}

static int i2c_bl_write_regs(struct i2c_client *client, struct i2c_bl_cmd *bl_cmds, int size)
{
	if(bl_cmds!=NULL && size>0) {
		while (size--) {
			unsigned char addr, ovalue, value, mask;

			addr = bl_cmds->addr;
			value = bl_cmds->value;
			mask = bl_cmds->mask;

			bl_cmds++;

			if(mask==0)
				continue;

			if(mask==0xff)
				i2c_bl_write_reg(client, addr, value);
			else {
				i2c_bl_read_reg(client, addr, &ovalue);
				i2c_bl_write_reg(client, addr, (ovalue&(~mask))|(value&mask));
			}
		}
	}

	return 0;
}

static int i2c_bl_set_regs(struct i2c_client *client, struct i2c_bl_cmd *bl_cmds, int size, unsigned char value)
{
	if(bl_cmds!=NULL && size>0) {
		while (size--) {
			unsigned char addr, ovalue, mask;

			addr = bl_cmds->addr;
			mask = bl_cmds->mask;

			bl_cmds++;

			if(mask==0)
				continue;

			if(mask==0xff)
				i2c_bl_write_reg(client, addr, value);
			else {
				i2c_bl_read_reg(client, addr, &ovalue);
				i2c_bl_write_reg(client, addr, (ovalue&(~mask))|(value&mask));
			}
		}
	}

	return 0;
}

static void i2c_bl_set_main_current_level(struct i2c_client *client, int level)
{
	struct i2c_bl_device *i2c_bl_dev = (struct i2c_bl_device *)i2c_get_clientdata(client);
	struct i2c_bl_platform_data *pdata = (struct i2c_bl_platform_data *)client->dev.platform_data;

	int cal_value = 0;
	int min_brightness = i2c_bl_dev->min_brightness;
	int max_brightness = i2c_bl_dev->max_brightness;

	i2c_bl_dev->bl_dev->props.brightness = cur_main_lcd_level = level;

	if (level != 0) {		
		if (level > 0 && level <= min_brightness)
			cal_value = min_brightness;
		else if (level > min_brightness && level <= max_brightness)
			cal_value = level;
		else if (level > max_brightness)
			cal_value = max_brightness;
 
		if (i2c_bl_dev->blmap) {
			if (cal_value < i2c_bl_dev->blmap_size) {
				i2c_bl_set_regs(client, pdata->set_brightness_cmds, pdata->set_brightness_cmds_size, i2c_bl_dev->blmap[cal_value]);
			} else {
				pr_err("Out of blmap range, wanted=%d, limit=%d\n", level, i2c_bl_dev->blmap_size);
				cal_value = level;
			} 
		} else
			i2c_bl_set_regs(client, pdata->set_brightness_cmds, pdata->set_brightness_cmds_size, cal_value);
	} else
		i2c_bl_write_regs(client, pdata->deinit_cmds, pdata->deinit_cmds_size);
		
	mdelay(1);
}

static void i2c_bl_set_main_current_level_no_mapping(struct i2c_client *client, int level)
{
	struct i2c_bl_device *i2c_bl_dev = (struct i2c_bl_device *)i2c_get_clientdata(client);
	struct i2c_bl_platform_data *pdata = client->dev.platform_data;

	if (level > 255)
		level = 255;
	else if (level < 0)
		level = 0;

	cur_main_lcd_level = level;
	i2c_bl_dev->bl_dev->props.brightness = cur_main_lcd_level;

	if (level != 0) {
		i2c_bl_set_regs(client, pdata->set_brightness_cmds, pdata->set_brightness_cmds_size, level);
	} else {
		i2c_bl_write_regs(client, pdata->deinit_cmds, pdata->deinit_cmds_size);
		backlight_status = BL_OFF;
	}
}

void i2c_bl_backlight_on(struct i2c_client *client, int level)
{
	struct i2c_bl_platform_data *pdata = (struct i2c_bl_platform_data *)client->dev.platform_data;

	mutex_lock(&backlight_mtx);
	if (backlight_status == BL_OFF) {
		i2c_bl_hw_reset(client);
		i2c_bl_write_regs(client, pdata->init_cmds, pdata->init_cmds_size);
	}
	mdelay(1);

	i2c_bl_set_main_current_level(client, level);
	backlight_status = BL_ON;
	mutex_unlock(&backlight_mtx);
}

static void i2c_bl_backlight_off(struct i2c_client *client)
{
	struct i2c_bl_platform_data *pdata = (struct i2c_bl_platform_data *)client->dev.platform_data;
	int gpio = pdata->gpio;

	pr_info("%s, on: %d\n", __func__, backlight_status);

	mutex_lock(&backlight_mtx);
	if (backlight_status == BL_OFF) {
		mutex_unlock(&backlight_mtx);
		return;
	}

	saved_main_lcd_level = cur_main_lcd_level;
	i2c_bl_set_main_current_level(client, 0);
	backlight_status = BL_OFF;

	gpio_direction_output(gpio, 0);
	msleep(6);
	mutex_unlock(&backlight_mtx);
}

static void i2c_bl_lcd_backlight_set_level(struct i2c_client *client, int level)
{
	//struct i2c_bl_device *i2c_bl_dev = (struct i2c_bl_device *)i2c_get_clientdata(client);
	//struct i2c_bl_platform_data *pdata = (struct i2c_bl_platform_data *)client->dev.platform_data;
	struct i2c_bl_platform_data *pdata = client->dev.platform_data;
	
	if (!client) {
		pr_warn("%s: not yet enabled\n", __func__);
		return;
	}

	if (level > pdata->max_brightness)
		level = pdata->max_brightness;

	pr_debug("%s: level %d\n", __func__, level);
	if (level)
		i2c_bl_backlight_on(client, level);
	else
		i2c_bl_backlight_off(client);
}

static int bl_set_intensity(struct backlight_device *bd)
{
	struct i2c_bl_device *i2c_bl_dev = (struct i2c_bl_device *)i2c_get_clientdata(main_i2c_bl_dev->client);
	int brightness = bd->props.brightness;

	if ((bd->props.state & BL_CORE_FBBLANK) ||
			(bd->props.state & BL_CORE_SUSPENDED))
		brightness = 0;
	else if (brightness == 0)
		brightness = i2c_bl_dev->default_brightness;

	i2c_bl_lcd_backlight_set_level(main_i2c_bl_dev->client, brightness);
	return 0;
}

static int bl_get_intensity(struct backlight_device *bd)
{
	return cur_main_lcd_level;
}

static ssize_t lcd_backlight_show_level(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "LCD Backlight Level is : %d\n", cur_main_lcd_level);
}

static ssize_t lcd_backlight_store_level(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int level;
	struct i2c_client *client = to_i2c_client(dev);

	if (!count)
		return -EINVAL;

	level = simple_strtoul(buf, NULL, 10);

	i2c_bl_set_main_current_level_no_mapping(client, level);
	pr_debug("[LCD][DEBUG] write %d direct to backlight register\n", level);

	return count;
}

static int i2c_bl_resume(struct i2c_client *client)
{
	i2c_bl_backlight_on(client, saved_main_lcd_level);
	return 0;
}

static int i2c_bl_suspend(struct i2c_client *client, pm_message_t state)
{
	pr_debug("[LCD][DEBUG] %s: new state: %d\n", __func__, state.event);

	i2c_bl_backlight_off(client);

	return 0;
}

static ssize_t lcd_backlight_show_on_off(struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_info("%s received (prev backlight_status: %s)\n", __func__, backlight_status ? "ON" : "OFF");

	return 0;
}

static ssize_t lcd_backlight_store_on_off(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int on_off;
	struct i2c_client *client = to_i2c_client(dev);

	if (!count)
		return -EINVAL;

	pr_info("%s received (prev backlight_status: %s)\n", __func__, backlight_status ? "ON" : "OFF");

	on_off = simple_strtoul(buf, NULL, 10);

	pr_debug("[LCD][DEBUG] %d", on_off);

	if (on_off == 1) {
		i2c_bl_resume(client);
	} else if (on_off == 0)
	    i2c_bl_suspend(client, PMSG_SUSPEND);

	return count;

}

DEVICE_ATTR(i2c_bl_level, 0644, lcd_backlight_show_level, lcd_backlight_store_level);
DEVICE_ATTR(i2c_bl_backlight_on_off, 0644, lcd_backlight_show_on_off, lcd_backlight_store_on_off);

static struct backlight_ops i2c_bl_ops = {
	.update_status = bl_set_intensity,
	.get_brightness = bl_get_intensity,
};

static int i2c_bl_probe(struct i2c_client *i2c_dev, const struct i2c_device_id *id)
{
	struct i2c_bl_platform_data *pdata;
	struct i2c_bl_device *i2c_bl_dev;
	struct backlight_device *bl_dev;
	struct backlight_properties props;
	int err;

	pr_info("[LCD][DEBUG] %s: i2c probe start\n", __func__);

	pdata = i2c_dev->dev.platform_data;

	i2c_bl_dev = kzalloc(sizeof(struct i2c_bl_device), GFP_KERNEL);
	if (i2c_bl_dev == NULL) {
		dev_err(&i2c_dev->dev, "fail alloc for i2c_bl_device\n");
		return 0;
	}

	pr_info("[LCD][DEBUG] %s: gpio = %d\n", __func__,pdata->gpio);

	if (pdata->gpio && gpio_request(pdata->gpio, "i2c_bl reset") != 0) {
		return -ENODEV;
	}

	main_i2c_bl_dev = i2c_bl_dev;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = pdata->max_brightness;

	bl_dev = backlight_device_register(I2C_BL_NAME, &i2c_dev->dev, NULL, &i2c_bl_ops, &props);
	bl_dev->props.max_brightness = pdata->max_brightness;
	bl_dev->props.brightness = pdata->default_brightness;
	bl_dev->props.power = FB_BLANK_UNBLANK;

	i2c_bl_dev->bl_dev = bl_dev;
	i2c_bl_dev->client = i2c_dev;
	i2c_bl_dev->gpio = pdata->gpio;
	i2c_bl_dev->min_brightness = pdata->min_brightness;
	i2c_bl_dev->max_brightness = pdata->max_brightness;
	i2c_bl_dev->default_brightness = pdata->default_brightness;
	i2c_bl_dev->blmap = pdata->blmap;
	i2c_bl_dev->blmap_size = pdata->blmap_size;
	i2c_set_clientdata(i2c_dev, i2c_bl_dev);

	if(lge_get_boot_cable_type() == LGE_BOOT_LT_CABLE_56K || lge_get_boot_cable_type() == LGE_BOOT_LT_CABLE_910K || lge_get_boot_cable_type() == LGE_BOOT_LT_CABLE_130K)
	{
		pr_info("is_factory_cable\n");
	    pdata->factory_mode = 1;
		pdata->factory_brightness = 3;
	}
    else pdata->factory_mode = 0;

	err = device_create_file(&i2c_dev->dev, &dev_attr_i2c_bl_level);
	err = device_create_file(&i2c_dev->dev, &dev_attr_i2c_bl_backlight_on_off);
	
	return 0;
}

static int i2c_bl_remove(struct i2c_client *client)
{
	struct i2c_bl_device *i2c_bl_dev = (struct i2c_bl_device *)i2c_get_clientdata(client);
	int gpio;

	device_remove_file(&client->dev, &dev_attr_i2c_bl_level);
	device_remove_file(&client->dev, &dev_attr_i2c_bl_backlight_on_off);

	gpio = i2c_bl_dev->gpio;

	backlight_device_unregister(i2c_bl_dev->bl_dev);
	i2c_set_clientdata(client, NULL);

	if (gpio_is_valid(gpio))
		gpio_free(gpio);

	return 0;
}

static struct i2c_driver main_i2c_bl_driver = {
	.probe = i2c_bl_probe,
	.remove = i2c_bl_remove,
	.suspend = NULL,
	.resume = NULL,
	.id_table = i2c_bl_id,
	.driver = {
		.name = I2C_BL_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init lcd_backlight_init(void)
{
	return i2c_add_driver(&main_i2c_bl_driver);
}

module_init(lcd_backlight_init);

MODULE_DESCRIPTION("I2C_BL Backlight Control");
MODULE_AUTHOR("Gilbert Ahn");
MODULE_LICENSE("GPL");
