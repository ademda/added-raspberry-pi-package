#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> 
#include <linux/slab.h>
#include <linux/ioctl.h>
#include <asm/uaccess.h>


MODULE_AUTHOR("Adem Daly"); 
MODULE_LICENSE("MIT");

struct sens_dev
{
    char lum_buffer [1024];
	struct mutex lock; 
    struct cdev cdev;     
};

struct file_operations sensd_fops = {
    .owner =    THIS_MODULE,
    .read =     lum_read,
    .open =     lum_open,
};

int drv_minor = 0;
int drv_major;
dev_t dev;
struct sens_dev *sensors_device;

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
    result = alloc_chrdev_region(&dev, aesd_minor, 1,"sensd");
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
    return 0;
}

void __exit sensmod_cleanup_module(void){
    
    cdev_del(&sensors_device->cdev);
    mutex_destroy(&sensors_device->lock);
    unregister_chrdev_region(dev, 1);
    kfree(sensors_device);
}

module_init(sensmod_init_module);
module_exit(sensmod_cleanup_module);