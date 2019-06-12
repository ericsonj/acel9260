// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#define DEVICE_NAME "acel9260" ///< The device will appear at /dev/acel9260 using this value
#define CLASS_NAME "acel"      ///< The device class -- this is a character device driver

static int majorNumber;                      ///< Stores the device number -- determined automatically
static char message[256] = {0};              ///< Memory for the string that is passed from userspace
static short size_of_message;                ///< Used to remember the size of the string stored
static int numberOpens = 0;                  ///< Counts the number of times the device is opened
static struct class *acelcharClass = NULL;   ///< The device-driver class struct pointer
static struct device *acelcharDevice = NULL; ///< The device-driver device struct pointer

static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations fops =
    {
        .open = dev_open,
        .read = dev_read,
        .write = dev_write,
        .release = dev_release,
};

static const struct i2c_device_id acel9260_i2c_id[] = {
    {"acel9260", 0},
    {}};

MODULE_DEVICE_TABLE(i2c, acel9260_i2c_id);

static const struct of_device_id acel9260_of_match[] = {
    {.compatible = "mse,acl9260"},
    {},
};

MODULE_DEVICE_TABLE(of, acel9260_of_match);

/* Add your code here */
static int acel9260_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int rv;
    char buf[2];
    u8 data[2];
    int16_t count;
    struct i2c_msg msgs[2];
    u8 reg = 0x41;

    pr_info("acel9260_probe\n");

    majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
    if (majorNumber < 0)
    {
        printk(KERN_ALERT "acel9260 failed to register a major number\n");
        return majorNumber;
    }
    printk(KERN_INFO "acel9260: registered correctly with major number %d\n", majorNumber);

    // Register the device class
    acelcharClass = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(acelcharClass))
    { // Check for error and clean up if there is
        unregister_chrdev(majorNumber, DEVICE_NAME);
        printk(KERN_ALERT "Failed to register device class\n");
        return PTR_ERR(acelcharClass); // Correct way to return an error on a pointer
    }
    printk(KERN_INFO "acelChar: device class registered correctly\n");

    // Register the device driver
    acelcharDevice = device_create(acelcharClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
    if (IS_ERR(acelcharDevice))
    {                                 // Clean up if there is an error
        class_destroy(acelcharClass); // Repeated code but the alternative is goto statements
        unregister_chrdev(majorNumber, DEVICE_NAME);
        printk(KERN_ALERT "Failed to create the device\n");
        return PTR_ERR(acelcharDevice);
    }
    printk(KERN_INFO "acelChar: device class created correctly\n"); // Made it! device was initialized

    // #define MPU9250_ACCEL_CONFIG 	     0x1C
    // #define MPU9250_ACCEL_FS_SEL_16G      0x18
    buf[0] = 0x1C;
    buf[1] = 0x18;
    rv = i2c_master_send(client, buf, 2);
    pr_info("Configure accelerometer: %d\n", rv);

    // #define MPU9250_GYRO_CONFIG           0x1B
    // #define MPU9250_GYRO_FS_SEL_2000DPS   0x18
    buf[0] = 0x1B;
    buf[1] = 0x18;
    rv = i2c_master_send(client, buf, 2);
    pr_info("Configure gyroscope: %d\n", rv);

    // enable accelerometer and gyro
    // #define MPU9250_PWR_MGMNT_2           0x6C
    // #define MPU9250_SEN_ENABLE            0x00
    buf[0] = 0x6C;
    buf[1] = 0x00;
    rv = i2c_master_send(client, buf, 2);
    pr_info("Enable accelerometer and gyro: %d\n", rv);

    // #define MPU9250_ACCEL_OUT             0x3B
    /*
    char data[20];
    uint16_t i;
    for (i = 0; i < rv; i++) {
        data[i] = 0x00;   
    }

    buf[1] = 0x3B;
    rv = i2c_master_send(client, buf, 1);
    pr_info("Get accelerometer: %d\n", rv);
    rv = i2c_master_recv(client, data, 20);
    for (i = 0; i < rv; i++) {
        pr_info("i2c_master_recv: %d data: 0x%02X\n", rv, data[i]);    
    }
    int16_t acel_x = (((int16_t)data[2]) << 8)  | data[3];
	int16_t acel_y = (((int16_t)data[4]) << 8)  | data[5];
	int16_t acel_z = (((int16_t)data[6]) << 8)  | data[7];
    pr_info("Accelerometer: %d %d %d\n", acel_x, acel_y, acel_z); 
    */

    // Read Temperature
    // #define REG_TEMPERATURE         0x41

    msgs[0].addr = client->addr;
    msgs[0].flags = 0; /* write */
    msgs[0].buf = &reg;
    msgs[0].len = 1;

    msgs[1].addr = client->addr;
    msgs[1].flags = I2C_M_RD;
    msgs[1].buf = data;
    msgs[1].len = 2;

    rv = i2c_transfer(client->adapter, msgs, 2);

    count = (((int16_t)data[0]) << 8) | data[1];
    count = ((count - 21) / 334) + 21;

    pr_info("Read temp: %d data: 0x%02X 0x%02X temp: %d C\n", rv, data[0], data[1], count);

    return 0;
}

static int acel9260_i2c_remove(struct i2c_client *client)
{
    device_destroy(acelcharClass, MKDEV(majorNumber, 0)); // remove the device
    class_unregister(acelcharClass);                      // unregister the device class
    class_destroy(acelcharClass);                         // remove the device class
    unregister_chrdev(majorNumber, DEVICE_NAME);         // unregister the major number
    printk(KERN_INFO "acelChar: Goodbye from the LKM!\n");
    return 0;
}

static struct i2c_driver acel9260_i2c_driver = {
    .driver = {
        .name = "acel9260_i2c",
        .of_match_table = acel9260_of_match,
    },
    .probe = acel9260_i2c_probe,
    .remove = acel9260_i2c_remove,
    .id_table = acel9260_i2c_id,
};

module_i2c_driver(acel9260_i2c_driver);

/** @brief The device open function that is called each time the device is opened
 *  This will only increment the numberOpens counter in this case.
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_open(struct inode *inodep, struct file *filep){
   numberOpens++;
   printk(KERN_INFO "acelChar: Device has been opened %d time(s)\n", numberOpens);
   return 0;
}

/** @brief This function is called whenever device is being read from user space i.e. data is
 *  being sent from the device to the user. In this case is uses the copy_to_user() function to
 *  send the buffer string to the user and captures any errors.
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 *  @param buffer The pointer to the buffer to which this function writes the data
 *  @param len The length of the b
 *  @param offset The offset if required
 */
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
   int error_count = 0;
   // copy_to_user has the format ( * to, *from, size) and returns 0 on success
   error_count = copy_to_user(buffer, message, size_of_message);
 
   if (error_count==0){            // if true then have success
      printk(KERN_INFO "acelChar: Sent %d characters to the user\n", size_of_message);
      return (size_of_message=0);  // clear the position to the start and return 0
   }
   else {
      printk(KERN_INFO "acelChar: Failed to send %d characters to the user\n", error_count);
      return -EFAULT;              // Failed -- return a bad address message (i.e. -14)
   }
}

/** @brief This function is called whenever the device is being written to from user space i.e.
 *  data is sent to the device from the user. The data is copied to the message[] array in this
 *  LKM using the sprintf() function along with the length of the string.
 *  @param filep A pointer to a file object
 *  @param buffer The buffer to that contains the string to write to the device
 *  @param len The length of the array of data that is being passed in the const char buffer
 *  @param offset The offset if required
 */
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){
   // sprintf(message, "%s(%zu letters)", buffer, len);   // appending received string with its length
   // size_of_message = strlen(message);                 // store the length of the stored message
   int error_count = 0;
   error_count = copy_from_user(message, buffer, len);
   if (error_count == 0) {
       printk(KERN_INFO "acelChar: write %s\n", message);
       size_of_message = strlen(message);
   }
   printk(KERN_INFO "acelChar: Received %zu characters from the user\n", len);
   return len;
}

/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_release(struct inode *inodep, struct file *filep){
   printk(KERN_INFO "acelChar: Device successfully closed\n");
   return 0;
}

MODULE_LICENSE("GPL");                  ///< The license type -- this affects available functionality
MODULE_AUTHOR("Ericson Joseph");        ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("My MPU9260 Drive"); ///< The description -- see modinfo
MODULE_VERSION("0.1");                  ///< A version number to inform users