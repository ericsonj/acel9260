// SPDX-License-Identifier: GPL-2.0
#include "acel9260.h"
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "acel9260"
#define CLASS_NAME "i2c"

/**
 * Store the device number
 */
static int majorNumber;
/**
 * Message store
 */
static char message[256] = {0};
static short size_of_message;

/**
 * Counts the number of times the device is opened
 */
static int numberOpens = 0;

/**
 * The device-driver class struct pointer
 */
static struct class *acelcharClass = NULL;

/**
 *  The device-driver device struct pointer
 */
static struct device *acelcharDevice = NULL;

/**
 * Copy of struct i2c_client
 */
static struct i2c_client *i2cClient = NULL;

/**
 *  Character device functions
 */
static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations fops = {
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

static const struct i2c_device_id acel9260_i2c_id[] = {{"acel9260", 0}, {}};

MODULE_DEVICE_TABLE(i2c, acel9260_i2c_id);

static const struct of_device_id acel9260_of_match[] = {
    {.compatible = "mse,acl9260"},
    {},
};

MODULE_DEVICE_TABLE(of, acel9260_of_match);

/**
 * Get data from mpu9260
 */
static int acel9260GetData(struct i2c_client *client, u8 regAddr, u8 *buff,
			   u16 len)
{

	int rv;
	struct i2c_msg msgs[2];
	u8 reg = regAddr;

	msgs[0].addr = client->addr;
	msgs[0].flags = 0; /* write */
	msgs[0].buf = &reg;
	msgs[0].len = 1;

	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].buf = buff;
	msgs[1].len = len;

	rv = i2c_transfer(client->adapter, msgs, 2);

	return rv;
}


static int acel9260_i2c_probe(struct i2c_client *client,
			      const struct i2c_device_id *id)
{
	int rv;
	char buf[2];
	i2cClient = client;

	pr_info("acel9260: probe\n");

	majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
	if (majorNumber < 0) {
		printk(KERN_ALERT
		       "acel9260: failed to register a major number\n");
		return majorNumber;
	}
	printk(KERN_INFO
	       "acel9260: registered correctly with major number %d\n",
	       majorNumber);

	// Register the device class
	acelcharClass = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(acelcharClass)) { // Check for error and clean up if there is
		unregister_chrdev(majorNumber, DEVICE_NAME);
		printk(KERN_ALERT "acel9260: Failed to register device class\n");
		return PTR_ERR(acelcharClass); // Correct way to return an error
					       // on a pointer
	}
	printk(KERN_INFO "acel9260: device class registered correctly\n");

	// Register the device driver
	acelcharDevice = device_create(
	    acelcharClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
	if (IS_ERR(acelcharDevice)) {	 // Clean up if there is an error
		class_destroy(acelcharClass); // Repeated code but the
					      // alternative is goto statements
		unregister_chrdev(majorNumber, DEVICE_NAME);
		printk(KERN_ALERT "acel9260: Failed to create the device\n");
		return PTR_ERR(acelcharDevice);
	}
	printk(KERN_INFO
	       "acel9260: device class created correctly\n"); // Made it! device
							      // was initialized

	buf[0] = MPU9250_ACCEL_CONFIG;
	buf[1] = MPU9250_ACCEL_FS_SEL_16G;
	rv = i2c_master_send(client, buf, MPU9250_CONFIG_SIZE);
	pr_info("acel9260: configure accelerometer: %d OK\n", rv);

	buf[0] = MPU9250_GYRO_CONFIG;
	buf[1] = MPU9250_GYRO_FS_SEL_2000DPS;
	rv = i2c_master_send(client, buf, MPU9250_CONFIG_SIZE);
	pr_info("acel9260: configure gyroscope: %d OK\n", rv);

	buf[0] = MPU9250_PWR_MGMNT_2;
	buf[1] = MPU9250_SEN_ENABLE;
	rv = i2c_master_send(client, buf, MPU9250_CONFIG_SIZE);
	pr_info("acel9260: enable accelerometer and gyro: %d OK\n", rv);

	return 0;
}

static int acel9260_i2c_remove(struct i2c_client *client)
{
	device_destroy(acelcharClass,
		       MKDEV(majorNumber, 0)); // remove the device
	class_unregister(acelcharClass);       // unregister the device class
	class_destroy(acelcharClass);	  // remove the device class
	unregister_chrdev(majorNumber,
			  DEVICE_NAME); // unregister the major number
	printk(KERN_INFO "acel9260: goodbye from the LKM!\n");
	return 0;
}

static struct i2c_driver acel9260_i2c_driver = {
    .driver =
	{
	    .name = "acel9260_i2c",
	    .of_match_table = acel9260_of_match,
	},
    .probe = acel9260_i2c_probe,
    .remove = acel9260_i2c_remove,
    .id_table = acel9260_i2c_id,
};

module_i2c_driver(acel9260_i2c_driver);

/** @brief The device open function that is called each time the device is
 * opened This will only increment the numberOpens counter in this case.
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_open(struct inode *inodep, struct file *filep)
{
	numberOpens++;
	printk(KERN_INFO "acel9260: device has been opened %d time(s)\n",
	       numberOpens);
	return 0;
}

/** @brief This function is called whenever device is being read from user space
 * i.e. data is being sent from the device to the user. In this case is uses the
 * copy_to_user() function to send the buffer string to the user and captures
 * any errors.
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 *  @param buffer The pointer to the buffer to which this function writes the
 * data
 *  @param len The length of the b
 *  @param offset The offset if required
 */
static ssize_t dev_read(struct file *filep, char *buffer, size_t len,
			loff_t *offset)
{
	int error_count = 0;
	int rv;

	if (i2cClient != NULL) {
		rv = acel9260GetData(i2cClient, REG_TEMPERATURE, message, 2);
		size_of_message = 2;
		rv = acel9260GetData(i2cClient, REG_ACCELEROMETER,
				     (message + 2), 6);
		size_of_message += 6;
		rv =
		    acel9260GetData(i2cClient, REG_GYROSCOPE, (message + 8), 6);
		size_of_message += 6;
	}
	message[8] = '\n';

	// copy_to_user has the format ( * to, *from, size) and returns 0 on
	// success
	error_count = copy_to_user(buffer, message, size_of_message);

	if (error_count == 0) { // if true then have success
		printk(KERN_INFO "acel9260: sent %d characters to the user\n",
		       size_of_message);
		return size_of_message; // clear the position to the start and
					// return 0
	} else {
		printk(KERN_INFO
		       "acel9260: failed to send %d characters to the user\n",
		       error_count);
		return -EFAULT; // Failed -- return a bad address message (i.e.
				// -14)
	}
}

/** @brief This function is called whenever the device is being written to from
 * user space i.e. data is sent to the device from the user. The data is copied
 * to the message[] array in this LKM using the sprintf() function along with
 * the length of the string.
 *  @param filep A pointer to a file object
 *  @param buffer The buffer to that contains the string to write to the device
 *  @param len The length of the array of data that is being passed in the const
 * char buffer
 *  @param offset The offset if required
 */
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len,
			 loff_t *offset)
{
	int rv;
	int error_count = 0; 
	char buf[MPU9250_CONFIG_SIZE];

	if(len != MPU9250_CONFIG_SIZE){
		return 0;
	}

	error_count = copy_from_user(buf, buffer, len);
	if(error_count != 0){
		return 0;
	}

	rv = i2c_master_send(i2cClient, buf, MPU9250_CONFIG_SIZE);

	return rv;
}

/** @brief The device release function that is called whenever the device is
 * closed/released by the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_release(struct inode *inodep, struct file *filep)
{
	printk(KERN_INFO "acel9260: device successfully closed\n");
	return 0;
}

MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("Ericson Joseph"); 
MODULE_DESCRIPTION("My MPU9260 Drive"); 
MODULE_VERSION("0.1");			