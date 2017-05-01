/*
 * linux/drivers/input/vfd/vfd.c
 *
 * 7-segment LED display driver
 *
 * Copyright (C) 2011 tiejun_peng, Amlogic Corporation
 * Copyright (C) 2017 Andrew Zabolotny <zapparello@ya.ru>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <asm/irq.h>
#include <asm/io.h>

#include <linux/major.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

#include "vfd-priv.h"

//***//***//***//***//***//***// sysfs support //***//***//***//***//***//***//

static ssize_t vfd_key_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct vfd_t *vfd = dev_get_drvdata(dev);
	return sprintf(buf, "%u %u\n", vfd->last_scancode, vfd->last_keycode);
}

static ssize_t vfd_display_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct vfd_t *vfd = dev_get_drvdata(dev);
	memcpy (buf, vfd->display, vfd->display_len);
	return vfd->display_len;
}

static ssize_t vfd_display_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct vfd_t *vfd = dev_get_drvdata(dev);
	size_t n = (count > vfd->display_len) ? vfd->display_len : count;

	mutex_lock(&vfd->lock);
	/* pad with spaces */
	memset(vfd->display, ' ', sizeof (vfd->display));
	memcpy(vfd->display, buf, n);
	vfd->need_update = 1;
	mutex_unlock(&vfd->lock);

	return count;
}

static ssize_t vfd_overlay_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct vfd_t *vfd = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < ARRAY_SIZE (vfd->raw_overlay); i++)
		sprintf(buf + i * 5, "%04x ", vfd->raw_overlay [i]);
	buf [ARRAY_SIZE (vfd->raw_overlay) * 5 - 1] = 0;

	return ARRAY_SIZE (vfd->raw_overlay) * 5 - 1;
}

static ssize_t vfd_overlay_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct vfd_t *vfd = dev_get_drvdata(dev);

	int i;
	char *endp;
	const char *cur = skip_spaces (buf);
	u16 raw_overlay [ARRAY_SIZE (vfd->raw_overlay)];

	for (i = 0; i < ARRAY_SIZE (vfd->raw_overlay); i++) {
		u16 n = simple_strtoul (cur, &endp, 16);
		if (endp == cur)
			break;

		raw_overlay [i] = n;

		cur = skip_spaces (endp);
	}

	mutex_lock(&vfd->lock);
	memcpy (vfd->raw_overlay, raw_overlay, i * sizeof (u16));
	vfd->need_update = 1;
	mutex_unlock(&vfd->lock);

	return count;
}

static ssize_t vfd_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct vfd_t *vfd = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", vfd->enabled);
}

static ssize_t vfd_enable_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct vfd_t *vfd = dev_get_drvdata(dev);
	char *endp;
	unsigned ena;

	ena = (simple_strtoul(buf, &endp, 0) != 0) ? 1 : 0;
	if (endp == buf)
		return -EINVAL;

	mutex_lock(&vfd->lock);
	vfd->enabled = ena;
	hardware_brightness(vfd, vfd->brightness);
	mutex_unlock(&vfd->lock);

	return count;
}

static ssize_t vfd_brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct vfd_t *vfd = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", vfd->brightness);
}

static ssize_t vfd_brightness_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct vfd_t *vfd = dev_get_drvdata(dev);
	char *endp;
	unsigned bri;

	buf = skip_spaces (buf);

	bri = simple_strtoul(buf, &endp, 0);
	if (endp == buf)
		return -EINVAL;

	if (bri > vfd->brightness_max)
		bri = vfd->brightness_max;

	mutex_lock(&vfd->lock);
	hardware_brightness (vfd, vfd->brightness = bri);
	mutex_unlock(&vfd->lock);

	return count;
}

static ssize_t vfd_brightness_max_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct vfd_t *vfd = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", vfd->brightness_max);
}

static DEVICE_ATTR(key, S_IRUGO, vfd_key_show, NULL);
static DEVICE_ATTR(display, S_IRUGO | S_IWUSR, vfd_display_show, vfd_display_store);
static DEVICE_ATTR(overlay, S_IRUGO | S_IWUSR, vfd_overlay_show, vfd_overlay_store);
static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR, vfd_enable_show, vfd_enable_store);
static DEVICE_ATTR(brightness, S_IRUGO | S_IWUSR, vfd_brightness_show, vfd_brightness_store);
static DEVICE_ATTR(brightness_max, S_IRUGO, vfd_brightness_max_show, NULL);

static const struct device_attribute *all_attrs [] = {
	&dev_attr_key, &dev_attr_display, &dev_attr_overlay,
	&dev_attr_enable, &dev_attr_brightness, &dev_attr_brightness_max,
};

//***//***//***//***//***//***// input support //***//***//***//***//***//***//

#ifndef CONFIG_VFD_NO_KEY_INPUT
static const u8 MultiplyDeBruijnBitPosition[32] = 
{
	0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
	31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
};

static void vfd_scan_keys(struct vfd_t *vfd)
{
	// key emitted flag
	int i, emit = 0;
	// key state bitvector
	u32 keys;
	// find out which key states have changed
	u32 keydiff;

	mutex_lock(&vfd->lock);
	keys = hardware_keys (vfd);
	mutex_unlock(&vfd->lock);

	keydiff = keys ^ vfd->keystate;

	// emit EV_KEY events for changed keys
	while (keydiff != 0) {
		// separate the lowest 1 bit
		u32 v = keydiff & -keydiff;
		// find the number of trailing zeros in a 32-bit integer
		// ref: http://supertech.csail.mit.edu/papers/debruijn.pdf
		u32 sc = MultiplyDeBruijnBitPosition[((u32)((v & -v) * 0x077CB531U)) >> 27];

		DBG_PRINT ("key scancode %d is now %s\n", sc, (keys & v) ? "down" : "up");
		vfd->last_scancode = sc;
		vfd->last_keycode = 0;

		// convert scancode to keycode
		for (i = 0; i < vfd->num_keys; i++)
			if (sc == vfd->keys [i].scancode) {
				// report key up/down
				vfd->last_keycode = vfd->keys [i].keycode;
				input_report_key(vfd->input, vfd->last_keycode, keys & v);
				emit = 1;
			}

		// drop this bit in keydiff
		keydiff ^= v;
	}
	if (emit)
		input_sync(vfd->input);

	vfd->keystate = keys;
}
#endif

//***//***//***//***//***//***// timer interrupt //***//***//***//***//***//***//

void vfd_timer_sr(unsigned long data)
{
	struct vfd_t *vfd = (struct vfd_t *)data;

#ifndef CONFIG_VFD_NO_KEY_INPUT
	if (vfd->input)
		vfd_scan_keys(vfd);
#endif
	if (vfd->need_update) {
		vfd->need_update = 0;
		mutex_lock(&vfd->lock);
		hardware_display_update (vfd);
		mutex_unlock(&vfd->lock);
	}

	mod_timer(&vfd->timer, jiffies + msecs_to_jiffies(200));
}

//***//***//***//***// Platform device implementation //***//***//***//***//

/* Set up data bus GPIOs */
static __init int __setup_gpios (struct platform_device *pdev, struct vfd_t *vfd)
{
	int i, n, ret;
	struct gpio_desc *desc;

	for (i = 0; i < GPIO_MAX; i++) {
		desc = of_get_named_gpiod_flags(pdev->dev.of_node, "gpios", i, NULL);
		if (IS_ERR(desc)) {
			dev_err(&pdev->dev, "%d bus signal GPIOs must be defined", GPIO_MAX);
			return PTR_ERR (desc);
		}

		n = desc_to_gpio(desc);

		if ((ret = gpio_request(n, "vfd")) < 0) {
			dev_info(&pdev->dev, "failed to request gpio %d\n", n);
			return ret;
		}

		vfd->gpio_desc [i] = desc;
	}

	dev_info(&pdev->dev, "bus signals STB,CLK,DI/DO mapped to GPIOs %d,%d,%d\n",
		desc_to_gpio(vfd->gpio_desc[GPIO_STB]),
		desc_to_gpio(vfd->gpio_desc[GPIO_CLK]),
		desc_to_gpio(vfd->gpio_desc[GPIO_DIDO]));

	return 0;
}

/* Set up input device */
static __init int __setup_input (struct platform_device *pdev, struct vfd_t *vfd)
{
	int i, ret;
	u16 *tmp_keys;
	struct input_dev *input;

	/* parse key description from DTS */
	vfd->num_keys = of_property_count_strings (pdev->dev.of_node, "key_names");
	if (vfd->num_keys > 0) {
		tmp_keys = kmalloc (vfd->num_keys * 2 * sizeof (u16), GFP_KERNEL);

		if (of_property_read_u16_array(pdev->dev.of_node, "key_codes", tmp_keys, vfd->num_keys) != 0) {
			dev_err(&pdev->dev, "key_names defined, but no key_codes -> no input");
			vfd->num_keys = 0;
		} else {
			u16 *cur_key = tmp_keys;
			vfd->keys = kmalloc(vfd->num_keys * sizeof(struct vfd_key_t), GFP_KERNEL);
			for (i = 0; i < vfd->num_keys; i++) {
				vfd->keys[i].scancode = tmp_keys [*cur_key++];
				vfd->keys[i].keycode = tmp_keys [*cur_key++];
			}
		}

		kfree (tmp_keys);
	}

	if (vfd->num_keys == 0)
		return 0;

	vfd->input = input = input_allocate_device();
	if (!input)
		return -ENOMEM;

	/* input device generates only EV_KEY's */
	set_bit(EV_KEY, input->evbit);
	for (i = 0; i < vfd->num_keys; i++)
		set_bit (vfd->keys[i].keycode, input->keybit);

	input->name = "vfd_keypad";
	input->phys = "vfd_keypad/input0";
	input->dev.parent = &pdev->dev;

	input->id.bustype = BUS_ISA;
	input->id.vendor = 0x0001;
	input->id.product = 0x0001;
	input->id.version = 0x0100;

	if ((ret = input_register_device(input)) < 0) {
		printk(KERN_ERR "Unable to register vfdkeypad input device\n");
		return ret;
	}

	return 0;
}

static int __init vfd_probe(struct platform_device *pdev)
{
	int i, ret;
	struct vfd_t *vfd;

	vfd = kzalloc(sizeof(struct vfd_t), GFP_KERNEL);
	if (!vfd)
		return -ENOMEM;

	mutex_init(&vfd->lock);
	platform_set_drvdata(pdev, vfd);

	if ((ret = __setup_gpios (pdev, vfd)) < 0)
		goto err1;

	if ((ret = hardware_init(vfd)) != 0) {
		dev_err(&pdev->dev, "vfd hardware init failed!\n");
		goto err1;
	}

	/* display "boot" at boot */
	vfd_display_store(&pdev->dev, &dev_attr_display, "boot", 4);

	setup_timer(&vfd->timer, vfd_timer_sr, (unsigned long)vfd);
	mod_timer(&vfd->timer, jiffies+msecs_to_jiffies(100));

	/* register sysfs attributes */
	for (i = 0; i < ARRAY_SIZE (all_attrs); i++)
		if ((ret = device_create_file(&pdev->dev, all_attrs [i])) < 0)
			goto err2;

	/* set up input device, if needed */
	if ((ret = __setup_input (pdev, vfd)) < 0)
		goto err2;

	return 0;

err2:
	for (i = ARRAY_SIZE (all_attrs) - 1; i >= 0; i--)
		device_remove_file (&pdev->dev, all_attrs [i]);
err1:
	if (vfd->input != NULL)
		input_free_device(vfd->input);
	kfree(vfd);

	return ret;
}

static int vfd_remove(struct platform_device *pdev)
{
	int i;
	struct vfd_t *vfd = platform_get_drvdata(pdev);

	/* unregister everything */
	for (i = ARRAY_SIZE (all_attrs) - 1; i >= 0; i--)
		device_remove_file (&pdev->dev, all_attrs [i]);

	if (vfd->input != NULL)
		input_free_device(vfd->input);

	kfree(vfd);

	return 0;
}

static int vfd_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct vfd_t *vfd = platform_get_drvdata(pdev);
	hardware_suspend (vfd, 1);
	return 0;
}

static int vfd_resume(struct platform_device *pdev)
{
	struct vfd_t *vfd = platform_get_drvdata(pdev);
	hardware_suspend (vfd, 0);
	return 0;
}

static const struct of_device_id vfd_dt_match[]={
	{
		.compatible     = "amlogic,aml_vfd",
	},
	{},
};
static struct platform_driver vfd_driver = {
	.probe      = vfd_probe,
	.remove     = vfd_remove,
	.suspend    = vfd_suspend,
	.resume     = vfd_resume,
	.driver     = {
		.name   = "m1-vfd.0",
		.of_match_table = vfd_dt_match,
	},
};

static int __init vfd_init(void)
{
	return platform_driver_register(&vfd_driver);
}

static void __exit vfd_exit(void)
{
	platform_driver_unregister(&vfd_driver);
}

module_init(vfd_init);
module_exit(vfd_exit);

MODULE_AUTHOR("tiejun_peng");
MODULE_DESCRIPTION("Amlogic VFD Driver");
MODULE_LICENSE("GPL");
