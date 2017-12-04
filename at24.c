#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <asm/mach/irq.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux//i2c.h>
#include <linux/poll.h>
#include <asm/mach/irq.h>
#include <asm/uaccess.h>


#define IIC_MAJOR 123

static int i2c_major =  IIC_MAJOR;
module_param(i2c_major, int, S_IRUGO);


struct at24_data_dev {
	struct cdev cdev;
	struct mutex mutex;
	struct fasync_struct *async_queue;
	unsigned long slave_addr;
	struct i2c_client client;
};

struct at24_data_dev *at24_data_devp;


void at24_do_tasklet(unsigned long);
DECLARE_TASKLET(at24_tasklet, at24_do_tasklet, 0);

void at24_do_tasklet(unsigned long dat)
{
	if (at24_data_devp->async_queue) {
		kill_fasync(&at24_data_devp->async_queue, SIGIO, POLL_IN);
	}
}


irqreturn_t at24_interrupt(int irq, void *dev_id)
{
	tasklet_schedule(&at24_tasklet);

	return IRQ_HANDLED;
}


static int at24_fasync(int fd, struct file *filp, int mode)
{
	struct at24_data_dev *devp = filp->private_data;
	
	return fasync_helper(fd, filp, mode, &devp->async_queue);
}

static int at24_open (struct inode *inode, struct file *filp)
{
	filp->private_data = at24_data_devp;
	return 0;
}

static ssize_t at24_read(struct file *filp, char __user *buf, size_t count,loff_t *off)	
{
	int ret = 0;
	unsigned char address;
	unsigned char data;
	struct i2c_msg msg[2];
	struct at24_data_dev *dev = filp->private_data;

	mutex_lock(&dev->mutex);

	if (copy_from_user(&address, buf, sizeof(buf))) {
	ret = -EFAULT;
	goto out;
	}

	msg[0].addr = dev->client.addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &address;

	msg[1].addr = dev->client.addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = &data;
	msg[1].len = 1;

	if (i2c_transfer(dev->client.adapter, msg, 2) != 2)
		return -EIO;
	if (copy_to_user(buf, &data, sizeof(data))) {
		ret = -EFAULT;
		goto out;
	}
	mutex_unlock(&at24_data_devp->mutex);

	return ret;

out:
	mutex_unlock(&dev->mutex);
	return 0;
}


static ssize_t at24_write(struct file *filp, char __user *buf, size_t count, loff_t *off)	
{
	int ret;
	unsigned char val[2];
	struct i2c_msg msg[1];
	struct at24_data_dev *dev = filp->private_data;

	mutex_lock(&at24_data_devp->mutex);
	/*
		val[0] = address;
		val[1] = data;
	*/

	if (copy_from_user(val, buf, 2)) {
	ret = -EFAULT;
	goto out;
	}

	msg[0].addr = dev->client.addr;
	msg[0].flags = 0;
	msg[0].len = count;
	msg[0].buf = val;

	ret = i2c_transfer(dev->client.adapter, msg, 1);
	mutex_unlock(&at24_data_devp->mutex);
	return ret;

out:
	mutex_unlock(&at24_data_devp->mutex);
	return 0;
}


static int at24_release(struct inode *inode, struct file *filp)
{
	at24_fasync(-1, filp, 0);

	return 0;
}

static const  struct file_operations at24_fops = {
	.owner = THIS_MODULE,
	.open = at24_open,
	.release = at24_release,
	.fasync = at24_fasync,
	.write = at24_write,
	.read = at24_read,
};


static const struct i2c_device_id at24_ids[] = {
	{"at24", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, at24_ids);
static int at24_setup_cdev(struct at24_data_dev *dev , unsigned int num)
{
	int err, devno = MKDEV(i2c_major, num);
	
	cdev_init(&dev->cdev, &at24_fops);
	dev->cdev.owner = THIS_MODULE;
	if (request_irq(dev->client.irq, at24_interrupt, 0, "at24", NULL) != 0)
		goto err_irq;
	
	err = cdev_add(&dev->cdev, devno, 1);
	if (err)
		printk(KERN_NOTICE "Error %d adding globalfifo%d\n",err, num);
	return IRQ_HANDLED;	
err_irq:
	free_irq(dev->client.irq, 0);
	return 0;
}

static int at24_probe(struct i2c_client * client, const struct i2c_device_id * id)
{
	printk("Loaded driver successfully!\n");
	at24_data_devp->client = *client;
	at24_setup_cdev(at24_data_devp, 0);
	return 0;
}

static int at24_remove(struct i2c_client * client)
{
	printk("Remove driver successfully!\n");
	return 0;
}

static struct  i2c_driver at24_driver = {
	.driver = {
		.name = "at24",
		.owner = THIS_MODULE,
		},
	.probe = at24_probe,
	.remove = at24_remove,
	.id_table	=	at24_ids,
};

static int __init at24_init(void)
{	

	int ret;
	dev_t devno = MKDEV(i2c_major, 0);
	
	if (i2c_major)
		ret = register_chrdev_region(devno, 1, "at24");
	else {
		ret = alloc_chrdev_region(&devno, 0, 1, "at24");
		i2c_major = MAJOR(devno);	
	}
	
	if (ret < 0)
		return ret;
	
	at24_data_devp = kzalloc(sizeof(struct at24_data_dev), GFP_KERNEL);
	
	if (!at24_data_devp) {
		return -ENOMEM;
		goto fail_malloc;	
	}
	
	mutex_init(&at24_data_devp->mutex);
	ret = i2c_add_driver(&at24_driver);	//完成注册之后，进行下一步probe资源的分配   中断 等资源的分配
	return 0;

fail_malloc:
		unregister_chrdev_region(devno, 1);
		return ret;
}

module_init(at24_init);

static void __exit at24_exit(void)
{
	free_irq(at24_data_devp->client.irq, 0);
	cdev_del(&at24_data_devp->cdev);
	kfree(at24_data_devp);	
	unregister_chrdev_region(MKDEV(i2c_major, 0), 1);	
	
	i2c_del_driver(&at24_driver);
}
module_exit(at24_exit);


MODULE_AUTHOR("milk2we");
MODULE_LICENSE("GPL v2");

