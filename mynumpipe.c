#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include<linux/semaphore.h>
#include<linux/miscdevice.h>
#include<linux/moduleparam.h>
#include<linux/string.h>


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Character Device for PC problem");
MODULE_AUTHOR("Nitin Mahadik");


int consumer_ptr=0, producer_ptr=0, intBuffSize = 0;
char *devBuffer;
int openDevicesCount = 0;

module_param(intBuffSize, int, S_IRUSR | S_IWUSR);		//	To accept parameters for Character Device	//

DEFINE_SEMAPHORE(empty);		// to keep track of empty integer buffer slots
DEFINE_SEMAPHORE(full);			//to keep track of full integer buffer slots
DEFINE_SEMAPHORE(mutex);		//mutex for critical region



static int device_open(struct inode *, struct file *);
int __init init_module(void);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static int device_close(struct inode *, struct file *);
void __exit cleanup_module(void);


static struct file_operations fops = {
	.open = device_open,
	.read = device_read,
	.write = device_write,
	.release = device_close
};

static struct miscdevice mynumpipe = {
	.name = "mynumpipe",
	.minor = MISC_DYNAMIC_MINOR,
	.fops = &fops,
};

// OPEN module
static int device_open(struct inode *inode, struct file *filep)
{
	printk(KERN_INFO "mynumpipe opened success");
	openDevicesCount++;
    return 0;
}


//	INIT Module

int __init init_module(void)
{
	int checkRegistration = misc_register(&mynumpipe);
	intBuffSize = intBuffSize*4;
	
	if(checkRegistration){
		printk("\nError in misc_register, code: %d", checkRegistration);
		return checkRegistration;
	}
	printk(KERN_INFO "\nmynumpipe registered success, intBuffSize = %d\n", intBuffSize);
	
	//	INIT Semaphore
	sema_init(&empty, intBuffSize);
	sema_init(&full, 0);
    sema_init(&mutex, 1);
	
	
	devBuffer = kmalloc(intBuffSize, GFP_KERNEL);
	printk(KERN_INFO "intBuffSize = %d\n",intBuffSize);
	
	if (!devBuffer){
		printk(KERN_ALERT "ERROR during kmalloc\n");
    	return -1;
    }

	return 0;
}



static ssize_t device_read(struct file *fp, char *userBuffer, size_t len, loff_t *offset){
	int userBuffer_index=0;
	
	
	//Decrementing FULL semaphore and acquiring MUTEX.
	if(down_interruptible(&full) < 0 ){
			printk(KERN_ALERT "User interrupted and exited\n");
			return -1;
		}
	if(down_interruptible(&mutex) < 0){
			printk(KERN_ALERT "Mutex Error\n");
			return -1;
		}
	
	//CRITICAL section
	while(userBuffer_index < len){	
		
		if(copy_to_user((userBuffer + userBuffer_index), (devBuffer + consumer_ptr), 1) < 0)
		{
	     	printk(KERN_ALERT "Error in Copy_to_User");
			return -1;
		}


		consumer_ptr++;
		userBuffer_index++;
	}
	
	
	//looping to start of the buffer to overwrite if the consumer_ptr goes beyond last index.
	if(consumer_ptr>intBuffSize)
			consumer_ptr = 0;
	printk(KERN_ALERT "devBuffer: Read Success\n");
	
	up(&mutex);	
	up(&empty);
	
	return userBuffer_index;
}


static ssize_t device_write(struct file *fp, const char *userBuffer, size_t len, loff_t *offset){
	int userBuffer_index=0, error_count = 0;
	
	//Decrementing FULL semaphore and acquiring MUTEX.
		if(down_interruptible(&empty)<0){			
			printk(KERN_ALERT "User interrupted and exited");
			return -1;
		}
		if(down_interruptible(&mutex)){				
			printk(KERN_ALERT "Mutex Error\n");
			return -1;
		}
	
	
		//CRITICAL section
		while(userBuffer_index < len){		
			error_count = copy_from_user((devBuffer + producer_ptr), (userBuffer + userBuffer_index), 1);
			if( error_count < 0){
				printk(KERN_ALERT "Error in Copy_from_User");
				return -1;
		}

		
		producer_ptr++;
		userBuffer_index++;
	}
	
	
	//looping to start of the buffer to overwrite if the producer_ptr goes beyond last index.
	if(producer_ptr>intBuffSize)
		producer_ptr = 0;
	printk(KERN_ALERT "devBuffer: Write Success\n");
	
	
	up(&mutex);	
	up(&full);
	
	return userBuffer_index;
}



static int device_close(struct inode *inode, struct file *filep){
	openDevicesCount--;
	return 0;
}




void __exit cleanup_module(){
	misc_deregister(&mynumpipe);
	printk(KERN_ALERT "Goodbye, Cruel World\n");
	//time =  current_kernel_time();
	//printk(KERN_ALERT "\nTime during exit: %ld\t%ld",time.tv_sec,time.tv_nsec);
	kfree(devBuffer);
}


