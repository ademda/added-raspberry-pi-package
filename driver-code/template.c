#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>
#include <linux/ioctl.h>
#include <asm/uaccess.h>
#include "aesd_ioctl.h"
#include "aesdchar.h"


#define TEMP_BUFFER_SIZE 0
#define TEMP_CHUNK_SIZE 40


int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Daniel Mendez"); 
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
	//Setup fil-->private data
	
	struct aesd_dev *dev;
	
	dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
	filp->private_data = dev;
	
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
	
	//Nothing to do as the memory was allocated in init_module
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    struct aesd_dev *dev = (struct aesd_dev *)filp->private_data;
    struct aesd_circular_buffer *circular_buffer = &dev->circular_buffer;
    struct aesd_buffer_entry *entry = NULL;
    size_t bytes_to_read;
    ssize_t retval = 0;
    size_t entry_offset_byte;
    
    if (buf == NULL) {
        retval = -EFAULT;
        goto exit;
    }

    mutex_lock(&dev->lock);
	
	PDEBUG("read %d bytes with fps %lld",count,*f_pos);

    entry = aesd_circular_buffer_find_entry_offset_for_fpos(circular_buffer, *f_pos, &entry_offset_byte);
    
    if(entry == NULL) {
        retval = 0;
        goto exit;
    }
	
    bytes_to_read = entry->size - entry_offset_byte;
	PDEBUG("Entry size %d ,Entry offset byte %d, Bytes to read %d ",entry->size,entry_offset_byte,bytes_to_read);
    if (bytes_to_read > count) {
        bytes_to_read = count;
    }


    if (copy_to_user(buf, entry->buffptr + entry_offset_byte, bytes_to_read)) {
        retval = -EFAULT;
        goto exit;
    }

    *f_pos += bytes_to_read;
    retval = bytes_to_read;

    exit:
        mutex_unlock(&dev->lock);
        return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	int res;
	int bytes_scanned = 0;
    ssize_t retval = -ENOMEM;
	
	
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    
	//Get the aesd device structure
	struct aesd_dev *dev = filp->private_data;
	//First check to see if there is a partial command ongoing
	
	//Allocate for the incoming data
	char * new_data = (char *)kmalloc(count,GFP_KERNEL);
	if(new_data == NULL){
		goto exit ;
	}
	//Set the new data to zeros
	memset(new_data,0,count)		;
	//Copy over the user data to the newly allocated memory
	res =  copy_from_user(new_data,buf,count);
	
	
	//Lock the mutex
	res = mutex_lock_interruptible(&(dev->lock));
	if(res){
		return -ERESTARTSYS;
	}
	
	//Iterate until we scanned everything inside
	while(bytes_scanned <= (count - 1)){
		//We realloc to add a byte and increase the size by one
		dev->temp_buffer = krealloc(dev->temp_buffer,++dev->temp_buffer_size,GFP_KERNEL);
		//Check if allocation successful
		if(!dev->temp_buffer){
			retval = -ENOMEM;
			goto exit;
		}
		//Store the current character at the end of the temp buffer
		dev->temp_buffer[dev->temp_buffer_size-1] = new_data[bytes_scanned];
		
		
		//Check to see if the new character added was a newline
		if(new_data[bytes_scanned] == '\n'){
			//Now we have to copy the temp buffer into the circular buffer
			//Create a temporary new entry object
			//We first allocate a buffer for the current size of the temp buffer
			//We allocate for the new entry
			struct aesd_buffer_entry new_entry;
			new_entry.buffptr = (char *)kmalloc(dev->temp_buffer_size,GFP_KERNEL);
			new_entry.size = dev->temp_buffer_size;
			//Now copy over the data
			memcpy(new_entry.buffptr,dev->temp_buffer,dev->temp_buffer_size);
			
	
			//First check if the circular buffer is full, then deallocate the current location
			if(dev->circular_buffer.full){
				//Free the oldest entry
				if(dev->circular_buffer.entry[dev->circular_buffer.in_offs].buffptr)
				kfree(dev->circular_buffer.entry[dev->circular_buffer.in_offs].buffptr);
				//No need to change the size since it would immediately be overwritten?			
			}
			
			//Store it into the circular buffer directly
			aesd_circular_buffer_add_entry(&(dev->circular_buffer),&new_entry);
			
			//After inserting we can reallocate the temporary buffer to be zero and set the size to be zero
			dev->temp_buffer = krealloc(dev->temp_buffer,0,GFP_KERNEL);
			dev->temp_buffer_size = 0;
			
		}
		//Increase the characters scanned by one
		bytes_scanned++;
		
	}
	retval = bytes_scanned;
	*f_pos += bytes_scanned;
	
	exit:
		mutex_unlock(&dev->lock);
		return retval;
}

loff_t aesd_llseek (struct file * filp, loff_t offset, int whence){
	
	
	//Get the aesd device structure
	struct aesd_dev *dev = filp->private_data;
	loff_t new_position;
	size_t total_buffer_size = 0;
	uint8_t index;
	struct aesd_buffer_entry *entryptr;
	ssize_t retval = 0;


	//First we lock the structure
	int res = mutex_lock_interruptible(&(dev->lock));
	if(res){
		return -ERESTARTSYS;
	}
	
	//Calculate the size of the circular buffer by iterating through the entries
	AESD_CIRCULAR_BUFFER_FOREACH(entryptr,&dev->circular_buffer,index){
		if(entryptr->buffptr){
			total_buffer_size+= entryptr->size;
		}	
	}
	//Unlock the mutex
	mutex_unlock(&dev->lock);
	PDEBUG("Total size of buffer is %zu",total_buffer_size);
	//Now call generic llseek
	new_position = fixed_size_llseek(filp, offset, whence,total_buffer_size);
	
	PDEBUG("New position is %zu",new_position);
	if(new_position < 0){
		retval = -EINVAL;
	}
	
	
	return retval;
}

long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){
	
	 //Get the aesd device structure
	struct aesd_dev *dev = filp->private_data;
	//Cast the incoming argument
	struct aesd_seekto temp_aesd_seekto ;
	uint8_t cmd_ptr=0;
	int cmds_scanned = -1;
	ssize_t retval = 0;
	long new_fpos = 0;
	
	PDEBUG("AESD IOCTL successfully called");
	
	//Check to make sure the magic number used is matched
	if(_IOC_TYPE(cmd) != AESD_IOC_MAGIC){
		return -ENOTTY;
	}
	PDEBUG("Magic number checked passed");
	//Check to make sure the number of commands is less than whats requested
	 if (_IOC_NR(cmd) > AESDCHAR_IOC_MAXNR){
		 return -ENOTTY; 
	 } 
	 PDEBUG("Num commands check passed passed");
	 //Check to make sure it is a Read and Write direction
	 if(_IOC_DIR(cmd) != (_IOC_READ |_IOC_WRITE)){
		 return -ENOTTY;
	 }
	  PDEBUG("Read and write direction check passed");
	// //Check to make sure it is safe to access the argument
	//if( access_ok(VERIFY_WRITE, &arg, sizeof(aesd_seekto) )){
	//	return -EFAULT;
	//}
	//Now copy the incoming argument from the user, 8 bytes since two unint32_t
	if(copy_from_user(&temp_aesd_seekto,(const void __user *)arg,sizeof(temp_aesd_seekto)) != 0){
		return -EFAULT;
	};
	 PDEBUG("Copy of datastruct passed args are %d , %d",temp_aesd_seekto.write_cmd,temp_aesd_seekto.write_cmd_offset);
	//Now we lock the structure
	int res = mutex_lock_interruptible(&(dev->lock));
	if(res){
		return -ERESTARTSYS;
	} 
	//Initialize the cmd_ptr to be the out_offset
	
	cmd_ptr = dev->circular_buffer.out_offs;
	PDEBUG(" BEFORE WHILE CMDS_SCANNED %d temp_aesd_seekto.write_cmd %d",cmds_scanned,temp_aesd_seekto.write_cmd);
	while(cmds_scanned < ((int)temp_aesd_seekto.write_cmd) ){

		//Increment cmds_scanned
		cmds_scanned++;
		struct aesd_buffer_entry entry = dev->circular_buffer.entry[cmd_ptr];
		if(entry.buffptr == NULL){
			PDEBUG("No command found at is too short");
			retval = -EINVAL;
			goto exit;
		}
		
		PDEBUG("CMDS_SCANNED %d temp_aesd_seekto.write_cmd %d",cmds_scanned,temp_aesd_seekto.write_cmd);
		//If we are not at the right command, we simply add the size of the current cmd to the new fpos
		if(cmds_scanned != temp_aesd_seekto.write_cmd)
		{
			PDEBUG("Adding %d from cmd located at %d",entry.size,cmds_scanned);
			new_fpos+= entry.size;
		}
		//Else we are at the right command 
		else{
			//We check to see if the write_cmd_offset is valid
			if(entry.size < (temp_aesd_seekto.write_cmd_offset +1)){
				PDEBUG("Command is too short");
				retval = -EINVAL;
				goto exit;
			}
			PDEBUG("Found command %d, adding %d, while command is %d long",cmds_scanned,temp_aesd_seekto.write_cmd_offset,entry.size);
			//Else we simply add the write_cmd_offset to the fpos
			new_fpos+= temp_aesd_seekto.write_cmd_offset;
			
		}
		PDEBUG("One command searched, iterating ,cmd_prt is %d",cmd_ptr);
		//We increment the cmd_ptr circularly
		cmd_ptr = increment_pointer(cmd_ptr);
		
		
	}
	//Now we update the filpos
	
	filp->f_pos = new_fpos;
	PDEBUG("f_pos has been updated to %d",new_fpos);
	//Unlock and exit

	exit:
		mutex_unlock(&dev->lock);
		return retval;
	
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
	.llseek =   aesd_llseek,
    .release =  aesd_release,
	.unlocked_ioctl = aesd_ioctl,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    //Initialize the mutex used
	mutex_init(&aesd_device.lock);
	
	//Initalize the circular buffer
	aesd_circular_buffer_init(&aesd_device.circular_buffer); // Probably not necessary since already set to zero?
	
	//Alloate memory for the temporary entry buffer
	aesd_device.temp_buffer = (char *)kmalloc(TEMP_BUFFER_SIZE,GFP_KERNEL);
	
	//Set the size to zero
	aesd_device.temp_buffer_size = 0;
	
	
    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);
	
	uint8_t index;
	struct aesd_buffer_entry *entryptr;

	
	//Free the temp pointer
	if(aesd_device.temp_buffer)
		kfree(aesd_device.temp_buffer);
	
	//Free all allocated memory in the circular buffer
	AESD_CIRCULAR_BUFFER_FOREACH(entryptr,&aesd_device.circular_buffer,index){
		if(entryptr->buffptr)
			kfree(entryptr->buffptr);
	}
	//Destory the mutex
	mutex_destroy(&aesd_device.lock);

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);