#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h> //for file operations (open, close , read device files, etc ...)
#include <linux/cdev.h> //to register char device to the kernel
#include <linux/semaphore.h> // to allow synchronization, to prevent data curruption.
#include <asm/uaccess.h> // copy data from user space to kernel and wise versa

//(1) Create a structure for our fake device
struct fake_device{
	char data[100];
	struct semaphore sem;
} virtual_device;

// (2) to later register our device we need a cdev object and some other variables
struct cdev * mcdev;
int major_number;
int ret;

dev_t dev_num;
#define DEVICE_NAME "fakeCharacterDevice"


//(7)

int device_open(struct inode *inode,struct file *filep){

	//only allow one process to open this device by using a semaphore as mutual exclusive lock- mutex 
	if(down_interruptible(&virtual_device.sem) != 0){
		printk(KERN_ALERT "fakeCharDevice could not lock device during open");
		return -1;
	}
	printk(KERN_INFO "fakeCharDevice opened Device");
	return 0;
}

//(8)

ssize_t device_read(struct file *filp, char *bufStoreData,size_t bufCount,loff_t *curOffSet){

	//take data from kernel space(device) to user space(process)
	//copy_to_user(destination,source,sizeToTransfer)
	printk(KERN_INFO "fakeCharDevice: reading from device");
	ret = copy_to_user(bufStoreData,virtual_device.data,bufCount);
	return ret;
}

//(9)

ssize_t device_write(struct file *filp,const char *bufSourceData,size_t bufCount,loff_t *curOffset){
	//send data from user to kernel
	// copy_from_user(dest,source,count)

	printk(KERN_INFO "fakeCharDevice writing to device");
	ret = copy_from_user(virtual_device.data,bufSourceData,bufCount);
	return ret;
}

//(10)
int device_close(struct inode *inode,struct file *filp){
	//by calling up, which opposite of fown for semaphore, we release the mutex that we obtained at device open
	//this has the effect of allowing other process to use the device now
	up(&virtual_device.sem);
	printk(KERN_INFO"fakeCharDevice closed Device");
	return 0;

}

//Here
//(6)
struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = device_open,
	.release = device_close,
	.write = device_write,
	.read = device_read
};

static int driver_entry(void){

	//(3) Register our device with the system; a two step process
		//(1) use dynamic allocation to assign our device a major number
	ret = alloc_chrdev_region(&dev_num,0,1,DEVICE_NAME);
	if(ret < 0){
		printk(KERN_ALERT "fakeCharacterDevice failed to allocate a major number");
		return ret; //propagate error
	}

	major_number = MAJOR(dev_num);
	printk(KERN_INFO "fakeCharacterDevice : major numbr is %d",major_number);
	printk(KERN_INFO "\tuse \"mknod /dev/%s c %d 0 \" for device file",DEVICE_NAME,major_number);

		//(2)
	mcdev = cdev_alloc(); //create our cdev structure, innitialized our cdev
	mcdev->ops = &fops;
	mcdev->owner = THIS_MODULE;
	// now that we created cdev, we have to add it to the kernel
	// int cdev_add(struct cdev* dev,dev_t num,unsigned int count)
	ret = cdev_add(mcdev,dev_num,1);
	if(ret <0){
		printk(KERN_ALERT "fakeCharDevice : unable to add cdev to kernel");
		return ret;
	}

	//(4) initialize our semaphore
	sema_init(&virtual_device.sem,1); // initial value of 1
	printk(KERN_INFO "FakeCharDevice Entry Success");
	return 0;
}

static void driver_exit(void){

	//(5) unregister everything in reverse order
	//(a)

	cdev_del(mcdev);
	
	//(b)
	unregister_chrdev_region(dev_num,1);
	printk(KERN_ALERT "fakeCharDevice unloaded module");

}

module_init(driver_entry);
module_exit(driver_exit);
