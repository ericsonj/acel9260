// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>

static const struct i2c_device_id acel9260_i2c_id[] = {
    { "acel9260", 0 },
    { }
};

MODULE_DEVICE_TABLE(i2c, acel9260_i2c_id);

static const struct of_device_id acel9260_of_match[] = {
    { .compatible = "mse,acl9260" },
    { },
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
	msgs[0].flags = 0;	/* write */
	msgs[0].buf = &reg;
	msgs[0].len = 1;

	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].buf = data;
	msgs[1].len = 2;

    rv = i2c_transfer(client->adapter, msgs, 2);
    
    count =  (((int16_t)data[0]) << 8)  | data[1];
    count = ((count - 21) / 334 ) + 21;
    
    pr_info("Read temp: %d data: 0x%02X 0x%02X temp: %d C\n", rv, data[0], data[1], count);

    return 0;
}

static int acel9260_i2c_remove(struct i2c_client *client)
{
    pr_info("acel9260_remove\n");
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

MODULE_LICENSE("GPL");                      ///< The license type -- this affects available functionality
MODULE_AUTHOR("Ericson Joseph");            ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("My MPU9260 Drive");     ///< The description -- see modinfo
MODULE_VERSION("0.1");                      ///< A version number to inform users