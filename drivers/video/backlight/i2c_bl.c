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
#include <linux/i2c_bl.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/mutex.h>
#include <linux/i2c.h>

#define I2C_BL_NAME                              "i2c_bl"
 
#define BL_ON        1
#define BL_OFF       0

unsigned int basekk_val	= 0;

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
	int blmap_size;
};

static struct i2c_bl_device *main_i2c_bl_dev;

static const struct i2c_device_id i2c_bl_id[] = {
	{ I2C_BL_NAME, 0 },
	{ },
};

static int i2c_bl_write_reg(struct i2c_client *client,
		unsigned char reg, unsigned char val);

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

static int cur_main_lcd_level;
static int saved_main_lcd_level;
static int backlight_status = BL_ON;

static void i2c_bl_hw_reset(struct i2c_client *client)
{
	//Disable warning: struct i2c_bl_device *i2c_bl_dev = (struct i2c_bl_device *)i2c_get_clientdata(client);
	struct i2c_bl_device *i2c_bl_dev = (struct i2c_bl_device *)i2c_get_clientdata(client);
	//struct i2c_bl_platform_data *pdata = client->dev.platform_data;
	int gpio = i2c_bl_dev->gpio;

	if (gpio_is_valid(gpio)) {
		gpio_direction_output(gpio, 1);
		gpio_set_value_cansleep(gpio, 1);
		mdelay(1);
	}
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

static void i2c_bl_set_main_current_level(struct i2c_client *client, int level)
{
	struct i2c_bl_device *i2c_bl_dev = (struct i2c_bl_device *)i2c_get_clientdata(client);
	//struct i2c_bl_platform_data *pdata = (struct i2c_bl_platform_data *)client->dev.platform_data;

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
				i2c_bl_write_reg(client, 0x70,
						i2c_bl_dev->blmap[cal_value]);
			} else {
				dev_warn(&client->dev, "invalid index %d:%d\n",
						i2c_bl_dev->blmap_size,
						cal_value);
			} 
		} else {
			i2c_bl_write_reg(client, 0x70, cal_value);
		}
	} else
		i2c_bl_write_reg(client, 0x1d, 0x00);
		
	mdelay(1);
}

void i2c_bl_backlight_on(struct i2c_client *client, int level)
{
	//struct i2c_bl_platform_data *pdata = (struct i2c_bl_platform_data *)client->dev.platform_data;
	char base;

	if (basekk_val == 1) {
		base = 0x03;
	} else if (basekk_val == 0) {
		base = 0x01;
	} else {
		base = 0x01;
	}

	mutex_lock(&backlight_mtx);
	if (backlight_status == BL_OFF) {
		pr_info("%s, ++ i2c_bl_backlight_on  \n",__func__);
		i2c_bl_hw_reset(client);

		i2c_bl_write_reg(client, 0x70, 0x00); //brightness = 0

		i2c_bl_write_reg(client, 0x10, 0x00); //initialize lm3532
		i2c_bl_write_reg(client, 0x1d, 0x01);
		i2c_bl_write_reg(client, 0x13, 0x06);
		i2c_bl_write_reg(client, 0x16, base);
		i2c_bl_write_reg(client, 0x17, 0x13);
	}

	i2c_bl_set_main_current_level(client, level);
	backlight_status = BL_ON;
	mutex_unlock(&backlight_mtx);
}

static void i2c_bl_backlight_off(struct i2c_client *client)
{
	//struct i2c_bl_platform_data *pdata = (struct i2c_bl_platform_data *)client->dev.platform_data;
	struct i2c_bl_device *i2c_bl_dev = (struct i2c_bl_device *)i2c_get_clientdata(client);
	int gpio = i2c_bl_dev->gpio;

	pr_info("%s, on: %d\n", __func__, backlight_status);

	mutex_lock(&backlight_mtx);
	if (backlight_status == BL_OFF) {
		mutex_unlock(&backlight_mtx);
		return;
	}

	saved_main_lcd_level = cur_main_lcd_level;
	i2c_bl_set_main_current_level(client, 0);
	backlight_status = BL_OFF;

	gpio_tlmm_config(GPIO_CFG(gpio, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL,
			GPIO_CFG_2MA), GPIO_CFG_ENABLE);
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

void i2c_bl_lcd_backlight_pwm_disable(void)
{
	//struct i2c_bl_device *i2c_bl_dev = (struct i2c_bl_device *)i2c_get_clientdata(client);
	char base;

	if (basekk_val == 1) {
		base = 0x03;
	} else if (basekk_val == 0) {
		base = 0x01;
	} else {
		base = 0x01;
	}

	if (backlight_status == BL_OFF)
		return;

	i2c_bl_write_reg(main_i2c_bl_dev->client, 0x10, 0x00); //initialize lm3532
	i2c_bl_write_reg(main_i2c_bl_dev->client, 0x1d, 0x01);
	i2c_bl_write_reg(main_i2c_bl_dev->client, 0x13, 0x06);
	i2c_bl_write_reg(main_i2c_bl_dev->client, 0x16, base);
	i2c_bl_write_reg(main_i2c_bl_dev->client, 0x17, 0x13);
}
EXPORT_SYMBOL(i2c_bl_lcd_backlight_pwm_disable);

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
	return snprintf(buf, PAGE_SIZE, "LCD Backlight Level is : %d\n",
			cur_main_lcd_level);
}

static ssize_t lcd_backlight_store_level(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int level;
	struct i2c_client *client = to_i2c_client(dev);

	if (!count)
		return -EINVAL;

	level = simple_strtoul(buf, NULL, 10);
	i2c_bl_set_main_current_level(client, level);

	return count;
}

static int i2c_bl_resume(struct i2c_client *client)
{
	i2c_bl_backlight_on(client, saved_main_lcd_level);
	return 0;
}

static int i2c_bl_suspend(struct i2c_client *client, pm_message_t state)
{
	pr_info("%s: new state: %d\n", __func__, state.event);

	i2c_bl_backlight_off(client);

	return 0;
}

static ssize_t lcd_backlight_show_on_off(struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_info("%s received (prev backlight_status: %s)\n", __func__,
			backlight_status ? "ON" : "OFF");

	return 0;
}

static ssize_t lcd_backlight_store_on_off(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int on_off;
	struct i2c_client *client = to_i2c_client(dev);

	if (!count)
		return -EINVAL;

	pr_info("%s received (prev backlight_status: %s)\n", __func__,
			backlight_status ? "ON" : "OFF");

	on_off = simple_strtoul(buf, NULL, 10);

	pr_info("%d", on_off);

	if (on_off == 1)
		i2c_bl_resume(client);
	else if (on_off == 0)
		i2c_bl_suspend(client, PMSG_SUSPEND);

	return count;

}

static ssize_t basekk_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", basekk_val);
}

static ssize_t basekk_store(struct device * dev, struct device_attribute * attr, const char * buf, size_t size)
{
    int base_val;

	sscanf(buf, "%d", &base_val);

	if (base_val != basekk_val) {
		if (base_val <= 0)
			base_val = 0;
		else if (base_val > 0)
			base_val = 1;
		pr_info("New base: %d\n", base_val);
		basekk_val = base_val;
	}

    return size;
}

DEVICE_ATTR(i2c_bl_level, 0644, lcd_backlight_show_level,
		lcd_backlight_store_level);
DEVICE_ATTR(i2c_bl_backlight_on_off, 0644, lcd_backlight_show_on_off,
		lcd_backlight_store_on_off);
DEVICE_ATTR(basekk, 0777, basekk_show, basekk_store);

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
	int err = 0;

	pdata = i2c_dev->dev.platform_data;
	if (!pdata)
		return -ENODEV;

	i2c_bl_dev = kzalloc(sizeof(struct i2c_bl_device), GFP_KERNEL);
	if (!i2c_bl_dev) {
		dev_err(&i2c_dev->dev, "fail alloc for i2c_bl_device\n");
		return -ENOMEM;
	}

	main_i2c_bl_dev = i2c_bl_dev;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = pdata->max_brightness;

	bl_dev = backlight_device_register(I2C_BL_NAME,
			&i2c_dev->dev, NULL, &i2c_bl_ops, &props);
	if (IS_ERR(bl_dev)) {
		dev_err(&i2c_dev->dev, "failed to register backlight\n");
		err = PTR_ERR(bl_dev);
		goto err_backlight_device_register;
	}
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

#if 0	
	if(lge_get_boot_cable_type() == LGE_BOOT_LT_CABLE_56K || lge_get_boot_cable_type() == LGE_BOOT_LT_CABLE_910K || lge_get_boot_cable_type() == LGE_BOOT_LT_CABLE_130K)
	{
		pr_info("is_factory_cable\n");
		pdata->factory_mode = 1;
		pdata->factory_brightness = 3;
	}
    	else pdata->factory_mode = 0;
#endif

	if (gpio_is_valid(i2c_bl_dev->gpio)) {
		err = gpio_request(i2c_bl_dev->gpio, "i2c_bl reset");
		if (err < 0) {
			dev_err(&i2c_dev->dev, "failed to request gpio\n");
			goto err_gpio_request;
		}
	}

	err = device_create_file(&i2c_dev->dev, &dev_attr_i2c_bl_level);
	if (err < 0) {
		dev_err(&i2c_dev->dev, "failed to create 1st sysfs\n");
		goto err_device_create_file_1;
	}
	err = device_create_file(&i2c_dev->dev, &dev_attr_i2c_bl_backlight_on_off);
	if (err < 0) {
		dev_err(&i2c_dev->dev, "failed to create 2nd sysfs\n");
		goto err_device_create_file_2;
	}
	err = device_create_file(&i2c_dev->dev, &dev_attr_basekk);
	if (err < 0) {
		dev_err(&i2c_dev->dev, "failed to create 3rd sysfs\n");
		goto err_device_create_file_3;
	}

	pr_info("i2c_bl probed\n");
	return 0;

err_device_create_file_3:
	device_remove_file(&i2c_dev->dev, &dev_attr_basekk);
err_device_create_file_2:
	device_remove_file(&i2c_dev->dev, &dev_attr_i2c_bl_level);
err_device_create_file_1:
	if (gpio_is_valid(i2c_bl_dev->gpio))
		gpio_free(i2c_bl_dev->gpio);
err_gpio_request:
	backlight_device_unregister(i2c_bl_dev->bl_dev);
err_backlight_device_register:
	kfree(i2c_bl_dev);

	return err;
}

static int i2c_bl_remove(struct i2c_client *client)
{
	struct i2c_bl_device *i2c_bl_dev = (struct i2c_bl_device *)i2c_get_clientdata(client);

	device_remove_file(&client->dev, &dev_attr_i2c_bl_level);
	device_remove_file(&client->dev, &dev_attr_i2c_bl_backlight_on_off);
	device_remove_file(&client->dev, &dev_attr_basekk);
	i2c_set_clientdata(client, NULL);

	if (gpio_is_valid(i2c_bl_dev->gpio))
		gpio_free(i2c_bl_dev->gpio);

	backlight_device_unregister(i2c_bl_dev->bl_dev);
	kfree(i2c_bl_dev);

	return 0;
}

static struct i2c_driver main_i2c_bl_driver = {
	.probe = i2c_bl_probe,
	.remove = i2c_bl_remove,
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
