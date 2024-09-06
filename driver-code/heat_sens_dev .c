#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> 
#include <linux/slab.h>
#include <linux/ioctl.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>
#include <linux/delay.h>

MODULE_AUTHOR("Adem Daly"); 
MODULE_LICENSE("MIT");

#define  HEAT_SENSOR_PIN 2


struct sens_dev
{
    char heat_buffer [1024];
	struct mutex lock; 
    struct cdev cdev;     
};

struct file_operations sensd_fops = {
    .owner =    THIS_MODULE,
    .read =     heat_read,
    .open =     heat_open,
    .close =    heat_close
};

int drv_minor = 0;
int drv_major;
dev_t dev;
struct sens_dev *sensors_device;

static int heat_open(struct inode *inode, struct file *filep) {
    struct sens_dev *dev;
    dev = container_of(inode->i_cdev, struct sens_dev, cdev);
    filep->private_data = dev; 
    return 0;  
}

static int heat_close(struct inode *inode, struct file *filep) {
    struct sens_dev *dev;
    dev = filep->private_data;
    return 0;  
}

int dht11_read_data(uint8_t *humidity, uint8_t *temperature) {
    int i;
    uint8_t data[5] = {0, 0, 0, 0, 0};

    gpio_direction_output(HEAT_SENSOR_PIN, 0);
    msleep(18);
    gpio_direction_input(HEAT_SENSOR_PIN);
    udelay(30);

    if (gpio_get_value(HEAT_SENSOR_PIN) == 0) {
        udelay(80);
        if (gpio_get_value(HEAT_SENSOR_PIN) == 1) {
            udelay(80);

            for (i = 0; i < 40; i++) {
                while (gpio_get_value(HEAT_SENSOR_PIN) == 0);  
                udelay(30);  

                if (gpio_get_value(HEAT_SENSOR_PIN) == 1) {
                    data[i/8] |= (1 << (7 - (i % 8)));
                }
                while (gpio_get_value(HEAT_SENSOR_PIN) == 1);  
            }

            if ((data[0] + data[1] + data[2] + data[3]) == data[4]) {
                *humidity = data[0];
                *temperature = data[2];
                return 0;  
            } else {
                return -EIO;  
            }
        }
    }
    return -EIO;  
}


ssize_t heat_read(struct file *filep, char __user *buf, size_t len, loff_t *offset) {
    char value_str[64];
    uint8_t humidity, temperature;
    int ret;
    mutex_lock(&dev->lock);
    ret = dht11_read_data(&humidity, &temperature);
    if (ret) {
        printk(KERN_ERR "Error reading data from sensor\n");
        return -EFAULT;
    }

    ret = snprintf(value_str, sizeof(value_str), "%u %u\n", humidity, temperature);
    if (ret < 0 || ret >= sizeof(value_str)) {
        printk(KERN_ERR "Error formatting sensor data\n");
        return -EFAULT;
    }

    if (copy_to_user(buf, value_str, ret)) {
        printk(KERN_ERR "Error copying to user sensor value\n");
        return -EFAULT;
    }
    mutex_unlock(&dev->lock);
    return ret;
}

static void sensd_setup_cdev(){
    int err;
    cdev_init(&(sensors_device->cdev), &sensd_fops);
    sensors_device->cdev.owner=THIS_MODULE;
    err=cdev_add (&(sensors_device->cdev), dev, 1);
    if (err) {
        printk(KERN_ERR "Error %d sensor device struct", err);
        unregister_chrdev_region(dev, 1);
    }
    else {
        printk(KERN_INFO "sensor device struct  added succesfully");
    }
}


int __init sensmod_init_module(void){
    int result;
    result = alloc_chrdev_region(&dev, drv_minor, 1,"sensd");
    drv_major=MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", drv_major);
        return result;
    }
    sensors_device = kmalloc(sizeof(struct sens_dev), GFP_KERNEL);
    if (!sensors_device) {
        unregister_chrdev_region(dev, 1);
        printk(KERN_ERR "Failed to allocate memory for sensor device\n");
        return -ENOMEM;
    }
    memset(sensors_device,0,sizeof(struct sens_dev));
    mutex_init(&sensors_device->lock);
    sensd_setup_cdev();
    if(gpio_request(HEAT_SENSOR_PIN,"heat sensor gpio 2")){
        cdev_del(&sensors_device->cdev);
        mutex_destroy(&sensors_device->lock);
        unregister_chrdev_region(dev, 1);
        kfree(sensors_device);
        printk(KERN_ERR "Error requesting from gpio\n");
        return -ENOMEM;
    }
    if (gpio_direction_input(HEAT_SENSOR_PIN)){
        cdev_del(&sensors_device->cdev);
        mutex_destroy(&sensors_device->lock);
        unregister_chrdev_region(dev, 1);
        kfree(sensors_device);
        gpio_free(HEAT_SENSOR_PIN);
        printk(KERN_ERR "Error setting  heat gpio direction\n");
        return -ENOMEM;
    }
    return 0;
}

void __exit sensmod_cleanup_module(void){
    
    cdev_del(&sensors_device->cdev);
    mutex_destroy(&sensors_device->lock);
    unregister_chrdev_region(dev, 1);
    kfree(sensors_device);
    gpio_free(HEAT_SENSOR_PIN);
}

module_init(sensmod_init_module);
module_exit(sensmod_cleanup_module);