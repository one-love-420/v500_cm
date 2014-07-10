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
#include <mach/board.h>
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
	int store_level_used;
	int delay_for_shaking;

	int exp_min_value;

	int percentage;
	int percentage_current;
	unsigned long duration_step;
};

static struct i2c_bl_device *main_i2c_bl_dev;

static const struct i2c_device_id i2c_bl_id[] = {
	{ I2C_BL_NAME, 0 },
	{ },
};

static int cur_main_lcd_level;
static int saved_main_lcd_level;
static int backlight_status = BL_ON;

static void update_level_scale(struct work_struct *work);

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

static DECLARE_DELAYED_WORK(update_level_scale_work, update_level_scale);

static void update_level_scale(struct work_struct *work)
{
	int percentage = main_i2c_bl_dev->percentage;
	int percentage_current = main_i2c_bl_dev->percentage_current;

	if (percentage > percentage_current)
		percentage_current++;
	else if	(percentage < percentage_current)
		percentage_current--;
	else
		return;

	main_i2c_bl_dev->percentage_current = percentage_current;
	i2c_bl_lcd_backlight_set_level_export(cur_main_lcd_level);

	if (percentage != percentage_current)
		schedule_delayed_work(&update_level_scale_work, msecs_to_jiffies(main_i2c_bl_dev->duration_step));
}

void i2c_bl_lcd_backlight_set_level_scale(int percentage, unsigned long duration)
{
	if (main_i2c_bl_dev != NULL &&
		main_i2c_bl_dev->client != NULL) {
		main_i2c_bl_dev->percentage = percentage;

		if (duration <= 0) {
			main_i2c_bl_dev->percentage_current = percentage;
			main_i2c_bl_dev->duration_step = 0;
			i2c_bl_lcd_backlight_set_level_export(cur_main_lcd_level);
		} else {
			int percentage_current = main_i2c_bl_dev->percentage_current;

			if(delayed_work_pending(&update_level_scale_work))
				cancel_delayed_work(&update_level_scale_work);
			if (percentage < percentage_current)
				main_i2c_bl_dev->duration_step = duration/(percentage_current - percentage);
			else if (percentage > percentage_current)
				main_i2c_bl_dev->duration_step = duration/(percentage - percentage_current);
			else
				main_i2c_bl_dev->duration_step = 0;

			if (main_i2c_bl_dev->duration_step>0)
				schedule_delayed_work(&update_level_scale_work, 0);
		}
	}
}
EXPORT_SYMBOL(i2c_bl_lcd_backlight_set_level_scale);

static void i2c_bl_hw_reset(struct i2c_client *client)
{
	//Disable warning: struct i2c_bl_device *i2c_bl_dev = (struct i2c_bl_device *)i2c_get_clientdata(client);
	struct i2c_bl_platform_data *pdata = client->dev.platform_data;
	int gpio = pdata->gpio;

	if (gpio_is_valid(gpio)) {
		gpio_direction_output(gpio, 1);
		gpio_set_value_cansleep(gpio, 1);
		mdelay(10);
	}
	else
		pr_err("%s: gpio is not valid !!\n", __func__);
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
	int err;
	u8 buf[2];
	struct i2c_msg msg = {
		client->addr, 0, 2, buf
	};

	buf[0] = reg;
	buf[1] = val;

	err = i2c_transfer(client->adapter, &msg, 1);
	if (err < 0)
		dev_err(&client->dev, "i2c write error\n");

	return err;
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

	if((pdata->factory_mode) && level)
	{
		level = pdata->factory_brightness;
	}
	if (level == -1)
		level = i2c_bl_dev->default_brightness;

	cur_main_lcd_level = level;
	i2c_bl_dev->bl_dev->props.brightness = cur_main_lcd_level;

	i2c_bl_dev->store_level_used = 0;

	if (level != 0) {
		if (pdata->blmap != NULL) {
			if (level < pdata->blmap_size)
				cal_value = pdata->blmap[level];
			else {
				pr_err("Out of blmap range, wanted=%d, limit=%d\n", level, pdata->blmap_size);
				cal_value = level;
			}
		} else
			cal_value = level;


		if (i2c_bl_dev->percentage_current != 100) {
			cal_value = (cal_value * i2c_bl_dev->percentage_current)/100;
		}

		i2c_bl_set_regs(client, pdata->set_brightness_cmds, pdata->set_brightness_cmds_size, cal_value);
	} else {
		i2c_bl_write_regs(client, pdata->deinit_cmds, pdata->deinit_cmds_size);
		backlight_status = BL_OFF;
	}
	mdelay(1);
       //pr_info("[LCD][DEBUG] %s : backlight level=%d, cal_value=%d\n", __func__, level, i2c_bl_dev->cal_value);
	   pr_info(" backlight level=%d, cal_value=%d\n", level, cal_value);
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

	i2c_bl_dev->store_level_used = 1;

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
		i2c_bl_backlight_off(client);
	else
		i2c_bl_backlight_on(client, level);

}

static int bl_set_intensity(struct backlight_device *bd)
{
	struct i2c_client *client = to_i2c_client(bd->dev.parent);
	struct i2c_bl_platform_data *pdata = client->dev.platform_data;

	int brightness = bd->props.brightness;

	if ((bd->props.state & BL_CORE_FBBLANK) ||
			(bd->props.state & BL_CORE_SUSPENDED))
		brightness = 0;
	else if (brightness == 0)
		brightness = pdata->default_brightness;
	
	i2c_bl_lcd_backlight_set_level(client, brightness);
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
	pr_info("%s: new state: %d\n", __func__, state.event);

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
static ssize_t lcd_backlight_show_exp_min_value(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct i2c_bl_device *i2c_bl_dev = (struct i2c_bl_device *)i2c_get_clientdata(client);

	return snprintf(buf, PAGE_SIZE, "LCD Backlight  : %d\n", i2c_bl_dev->exp_min_value);
}

static ssize_t lcd_backlight_store_exp_min_value(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct i2c_bl_device *i2c_bl_dev = (struct i2c_bl_device *)i2c_get_clientdata(client);
	int value;

	if (!count)
		return -EINVAL;

	value = simple_strtoul(buf, NULL, 10);
	i2c_bl_dev->exp_min_value = value;

	return count;
}

static ssize_t lcd_backlight_show_shaking_delay(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct i2c_bl_device *i2c_bl_dev = (struct i2c_bl_device *)i2c_get_clientdata(client);

	pr_debug("%s received (shaking delay : %d)\n", __func__, i2c_bl_dev->delay_for_shaking);

	return 0;
}

static ssize_t lcd_backlight_store_shaking_delay(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct i2c_bl_device *i2c_bl_dev = (struct i2c_bl_device *)i2c_get_clientdata(client);
	int delay;

	if (!count)
		return -EINVAL;

	delay = simple_strtoul(buf, NULL, 10);
	pr_debug("%s received (you input : %d for shaking delay)\n", __func__, delay);
	i2c_bl_dev->delay_for_shaking = delay;

	return count;

}

static ssize_t lcd_backlight_show_dump_reg(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct i2c_bl_platform_data *pdata = client->dev.platform_data;
	struct i2c_bl_cmd *dump_regs = pdata->dump_regs;

	int dump_regs_size = pdata->dump_regs_size;

	int r;
	int ret = 0;

	if (backlight_status==BL_OFF) {
		ret = snprintf(buf, PAGE_SIZE, "I2C BL are power down!!\n");
		return ret;
	}

	mutex_lock(&backlight_mtx);

	while (dump_regs_size--) {
		i2c_bl_read_reg(client, dump_regs->addr, &dump_regs->value);
		mdelay(3);
		r = snprintf(buf+ret, PAGE_SIZE-ret, "%02X: %02X, %s\n", (unsigned int)dump_regs->addr, (unsigned int)dump_regs->value, dump_regs->description);
		if (r>0)
			ret += r;
		else
			break;
		dump_regs++;
	}
	mutex_unlock(&backlight_mtx);

	return ret;
}

static ssize_t lcd_backlight_store_dump_reg(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);

	unsigned int addr, value, mask;
	unsigned char ovalue;

	if (backlight_status==BL_OFF)
		return 0;

	sscanf(buf, "%x %x %x", &addr, &value, &mask);

	if (mask==0)
		return count;

	mutex_lock(&backlight_mtx);
	if (mask==0xff)
		i2c_bl_write_reg(client, addr, value);
	else {
		i2c_bl_read_reg(client, addr, &ovalue);
		i2c_bl_write_reg(client, addr, (ovalue&(~mask))|(value&mask));
	}
	mutex_lock(&backlight_mtx);

	return count;
}

DEVICE_ATTR(i2c_bl_level, 0644, lcd_backlight_show_level, lcd_backlight_store_level);
DEVICE_ATTR(i2c_bl_backlight_on_off, 0644, lcd_backlight_show_on_off, lcd_backlight_store_on_off);
DEVICE_ATTR(i2c_bl_exp_min_value, 0644, lcd_backlight_show_exp_min_value, lcd_backlight_store_exp_min_value);
DEVICE_ATTR(i2c_bl_shaking_delay, 0644, lcd_backlight_show_shaking_delay, lcd_backlight_store_shaking_delay);
DEVICE_ATTR(i2c_bl_dump_reg, 0644, lcd_backlight_show_dump_reg, lcd_backlight_store_dump_reg);

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
		dev_err(&client->dev, "fail alloc for i2c_bl_device\n");
		return -ENOMEM;
	}

	pr_info("[LCD][DEBUG] %s: gpio = %d\n", __func__,pdata->gpio);

	

	main_i2c_bl_dev = i2c_bl_dev;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = pdata->max_brightness;

	bl_dev = backlight_device_register(I2C_BL_NAME, &i2c_dev->dev, NULL, &i2c_bl_ops, &props);

	if (IS_ERR(bl_dev)) {
		dev_err(&client->dev, "failed to register backlight\n");
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
	i2c_bl_dev->default_brightness = pdata->default_brightness;
	i2c_bl_dev->max_brightness = pdata->max_brightness;
	i2c_bl_dev->store_level_used = 0;
	i2c_bl_dev->delay_for_shaking = 50;
	i2c_bl_dev->exp_min_value = 150;
	i2c_bl_dev->percentage = 100;
	i2c_bl_dev->percentage_current = 100;
	i2c_bl_dev->duration_step = 0;
	i2c_set_clientdata(i2c_dev, i2c_bl_dev);


	if(lge_get_boot_cable_type() == LGE_BOOT_LT_CABLE_56K || lge_get_boot_cable_type() == LGE_BOOT_LT_CABLE_910K || lge_get_boot_cable_type() == LGE_BOOT_LT_CABLE_130K)
	{
		pr_info("is_factory_cable\n");
	    pdata->factory_mode = 1;
		pdata->factory_brightness = 3;
	}
    else pdata->factory_mode = 0;
#if 0
	if(pdata->factory_brightness <= 0)
		i2c_bl_dev->factory_brightness = DEFAULT_FTM_BRIGHTNESS;
	else
		i2c_bl_dev->factory_brightness = pdata->factory_brightness;
#endif
	if (gpio_is_valid(pdata->gpio)) {
		err = gpio_request(pdata->gpio, "i2c_bl reset");
		if (err < 0) {
			dev_err(&client->dev, "failed to register gpio\n");
			goto err_gpio_request;
		}
	}

	err = device_create_file(&i2c_dev->dev, &dev_attr_i2c_bl_level);
	err = device_create_file(&i2c_dev->dev, &dev_attr_i2c_bl_backlight_on_off);
	err = device_create_file(&i2c_dev->dev, &dev_attr_i2c_bl_exp_min_value);
	err = device_create_file(&i2c_dev->dev, &dev_attr_i2c_bl_shaking_delay);
	err = device_create_file(&i2c_dev->dev, &dev_attr_i2c_bl_dump_reg);

	return 0;

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
	device_remove_file(&client->dev, &dev_attr_i2c_bl_exp_min_value);
	device_remove_file(&client->dev, &dev_attr_i2c_bl_shaking_delay);
	device_remove_file(&client->dev, &dev_attr_i2c_bl_dump_reg);

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
