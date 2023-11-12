#include<linux/module.h>
#include<linux/fs.h>
#include<linux/cdev.h>
#include<linux/device.h>
#include<linux/kdev_t.h>
#include<linux/uaccess.h>

#undef pr_fmt
#define pr_fmt(fmt) "%s: " fmt,__func__

#define DEV_MEM_SIZE 512


char device_buffer[DEV_MEM_SIZE];

//holds the device number
dev_t device_number;

//cdev variable
struct cdev pcd_cdev;

loff_t pcd_lseek (struct file *filp, loff_t offset, int whence)
{
	loff_t temp;

	pr_info("Lseek Requested  ..\n");
	pr_info("Current Value of file position = %lld\n",filp->f_pos);
	switch(whence){
		case SEEK_SET:
			if ((offset> DEV_MEM_SIZE) || (offset < 0))
				return -EINVAL;
			filp->f_pos = offset;
			break;
		case SEEK_CUR:
			temp =  filp->f_pos + offset ;
			if((temp> DEV_MEM_SIZE) || (temp< 0))
				return -EINVAL;
			filp->f_pos = temp;
			break;
		case SEEK_END:
			temp =  DEV_MEM_SIZE + offset ;
			if((temp> DEV_MEM_SIZE) || (temp< 0))
				return -EINVAL;
			filp->f_pos = DEV_MEM_SIZE + offset;
			break;
		default:
			return -EINVAL;
	}
	pr_info("New value of file position = %lld\n",filp->f_pos);
	return filp->f_pos;
}


ssize_t pcd_read (struct file *filp, char __user *buff, size_t count, loff_t *f_pos)
{
	pr_info("Read request fr %zu bytres \n",count);
	pr_info("Current File position pos = %lld \n",*f_pos);
	//Adjust count
	if((*f_pos + count) > DEV_MEM_SIZE )
		count =  DEV_MEM_SIZE - *f_pos;

	if(!count){
		return -ENOMEM;
	}
	//Copy to user
	if(copy_to_user(buff,&device_buffer[*f_pos],count)){
		return -EFAULT;
	}

	//update current file position
	*f_pos = count;

	pr_info("Number of bytes sucessfully written %zu bytres \n",count);
	pr_info("Updated File pos = %lld \n",*f_pos);

	return count;
}


ssize_t pcd_write (struct file *filp, const char __user *buff, size_t count, loff_t *f_pos)
{
	pr_info("Write request fr %zu bytres \n",count);
	pr_info("Current File position pos = %lld \n",*f_pos);
	//Adjust count
	if((*f_pos + count) > DEV_MEM_SIZE )
		count =  DEV_MEM_SIZE - *f_pos;

	//Copy from user
	if(copy_from_user(&device_buffer[*f_pos],buff,count)){
		return -EFAULT;
	}

	//update current file position
	*f_pos = count;

	pr_info("Number of bytes sucessfully read  %zu bytres \n",count);
	pr_info("Updated File pos = %lld \n",*f_pos);

	return count;
}


int pcd_open (struct inode *inode, struct file *filp)
{
	pr_info("Open was sucessfull ......\n");
	return 0;
}


int pcd_release (struct inode *inode, struct file *filp)
{
	pr_info("Release was sucessfull ......\n");
	return 0;
}


//file operations of the driver
struct file_operations pcd_fops = 
{
	.open =  pcd_open,
	.write =  pcd_write,
	.read = pcd_read,
	.llseek = pcd_lseek,
	.release = pcd_release,
	.owner = THIS_MODULE,
};

struct class *class_pcd;
struct device *device_pcd;

static int __init pcd_driver_init(void){

	int ret;

	//1. Dyanamically allocate a device number
	ret = alloc_chrdev_region(&device_number,0,1,"pcd_devices");
	if(ret < 0)
		goto out;

	pr_info("Device number <major>:<minor> = %d : %d \n",MAJOR(device_number),MINOR(device_number));

	//2. Initiate cdev with fops
	cdev_init(&pcd_cdev,&pcd_fops);

	//3 register cdev struct with vfs
	pcd_cdev.owner =  THIS_MODULE;
	ret = cdev_add(&pcd_cdev,device_number,1);
	if(ret < 0)
		goto unreg_chrdev;

	//create device class under /sys/class 
	class_pcd = class_create(THIS_MODULE,"pcd_class");
	if(IS_ERR(class_pcd)){
		pr_err("Class creation Failure ....\n");
		ret = PTR_ERR(class_pcd);
		goto cdev_del;
	}

	//populate the sysfs with device information
	device_pcd= device_create(class_pcd,NULL,device_number,NULL,"pcd");
	if(IS_ERR(class_pcd)){
		pr_err("Device creation Failure ....\n");
		ret = PTR_ERR(device_pcd);
		goto class_destroy;
	}	

	pr_info("Modulated initiated sucessfully \n");
	
	return 0;

class_destroy:
	class_destroy(class_pcd);
	
cdev_del:
	cdev_del(&pcd_cdev);

unreg_chrdev:
	unregister_chrdev_region(device_number,1);

out:
	return ret;

}


static void __exit pcd_driver_cleanup(void){
	device_destroy(class_pcd,device_number);
	class_destroy(class_pcd);
	cdev_del(&pcd_cdev);
	unregister_chrdev_region(device_number,1);
	pr_info("Module unloader...\n");

}


module_init(pcd_driver_init);
module_exit(pcd_driver_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SURESH");
MODULE_DESCRIPTION("simple character driver");
