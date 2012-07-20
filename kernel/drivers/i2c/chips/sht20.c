#include <linux/module.h>

#include <linux/init.h>

#include <linux/i2c.h>

#include <linux/slab.h>

#include <linux/mutex.h>

#include <linux/delay.h>



#define SHT20_DRV_NAME "sht20"



#define SHT20_T_HOLD     0xe3

#define SHT20_RH_HOLD    0xe5

#define SHT20_T_UNHOLD   0xf3

#define SHT20_RH_UNHOLD  0xf5

#define SHT20_USER_WR    0xe6

#define SHT20_USER_RD    0xe7

#define SHT20_SOFT_RESET 0xfe



struct sht20_data {

	struct i2c_client *client;

 	struct mutex update_lock; 

	unsigned int operating_mode : 2;

};



static const u8 SHT20_OPERATING_MODE[4] = {

	SHT20_T_HOLD, SHT20_RH_HOLD, SHT20_T_UNHOLD, SHT20_RH_UNHOLD,

};



/*static int sht20_get_value(struct i2c_client *client, int cmd)

{

	int ret;

	ret = i2c_smbus_read_word_data(client, SHT20_OPERATING_MODE[cmd]);

//	i2c_smbus_write_byte(client, SHT20_OPERATING_MODE[cmd]);

//	ret = i2c_smbus_read_byte(client);

	return ret;

}*/



/*show the temperature through the point tem in sysfs*/

static ssize_t sht20_show_tem(struct device *dev, struct device_attribute *attr, char *buf)   //¿char¿¿int

{

	struct i2c_client *client = to_i2c_client(dev);

	struct sht20_data *data = i2c_get_clientdata(client);

//	int ret = 0;

//	int tem = 0;

	int ret,tem,tem_l,tem_h;

//	int i=0;



	mutex_lock(&data->update_lock);

//while(i < 5) {

	

	tem = i2c_smbus_read_word_data(client, SHT20_OPERATING_MODE[0]);

//	tem = i2c_smbus_read_byte_data(client, SHT20_OPERATING_MODE[0]);

//	tem = tem&0xfffc;

//	tem = sht20_calculate_t(ret);

	tem_l = tem >> 8;         

	tem_h = tem & 0xff;       

	tem_l = tem_l & 0xfffffffc;

	tem = (tem_h<<8) & 0xff00 | tem_l;

//	ret = sprintf(buf, "%d %d %d\n", tem, tem_h,tem_l);

	ret = sprintf(buf, "%d\n", tem);

//	mdelay(100);

//	i++;

//	}

	mutex_unlock(&data->update_lock);

	

	return ret;

}



/*add sysfs*/

static DEVICE_ATTR(tem, S_IRUGO, sht20_show_tem, NULL);



/*show the relative humidity through the point rh in sysfs,you can get it by "cat rh"*/

static ssize_t sht20_show_rh(struct device *dev, struct device_attribute *attr, char *buf)   //¿char¿¿int

{

    struct i2c_client *client = to_i2c_client(dev);

    struct sht20_data *data = i2c_get_clientdata(client);

//  int ret = 0;

//  int tem = 0;

    int ret,rh,rh_h,rh_l;

//  int i=0;



    mutex_lock(&data->update_lock);

//while(i < 5) {

    

    rh = i2c_smbus_read_word_data(client, SHT20_OPERATING_MODE[1]);

//	rh = i2c_smbus_read_byte_data(client, SHT20_OPERATING_MODE[1]);

//  tem = tem&0xfffc;

//  tem = sht20_calculate_t(ret);

	rh_l = rh >> 8;         

 	rh_h = rh & 0xff;       

    rh_l = rh_l&0xfffffffc;

	rh = (rh_h<<8) & 0xff00 | rh_l;

	ret = sprintf(buf, "%d\n", rh);

//    ret = sprintf(buf, "%d,%d\n", rh_h,rh_l);

//	ret = sprintf(buf, "%d\n", rh);

//      return ret; 

//  mdelay(100);

//  i++;

//  }

    mutex_unlock(&data->update_lock);

    

    return ret;

}



static DEVICE_ATTR(rh, S_IRUGO, sht20_show_rh, NULL);





static ssize_t sht20_show_reg_cfg(struct device *dev, struct device_attribute *attr, char *buf)

{

	struct i2c_client *client = to_i2c_client(dev);

	int ret,reg;

//	i2c_smbus_write_byte_data(client, SHT20_USER_WR, 0x3b);

	reg = i2c_smbus_read_byte_data(client, SHT20_USER_RD);

	ret = sprintf(buf, "%d\n", reg);

	return ret;

}



static ssize_t sht20_store_reg_cfg(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)

{

	struct i2c_client *client = to_i2c_client(dev);

	struct sht20_data *data = i2c_get_clientdata(client);

	unsigned long val = simple_strtoul(buf, NULL, 10);

	int ret;

	mutex_lock(&data->update_lock);

	ret = i2c_smbus_write_byte_data(client, SHT20_USER_WR, val);

	mutex_unlock(&data->update_lock);

	

	if(ret < 0)

		return ret;

	

	return count;

}



static DEVICE_ATTR(reg_cfg, S_IRUGO | S_IWUSR, sht20_show_reg_cfg, sht20_store_reg_cfg);

 

static struct attribute *sht20_attributes[] = {

    &dev_attr_tem.attr,

	&dev_attr_reg_cfg.attr,

	&dev_attr_rh.attr,

    NULL

};





static const struct attribute_group sht20_attr_group = {

	.attrs = sht20_attributes,

};



static int __devinit sht20_probe(struct i2c_client *client, const struct i2c_device_id *id) {

	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);

    struct sht20_data *data;		//ÉÏÃæÁœŸäÊ²ÃŽÒâËŒ£¿£¿

	int err = 0;



	if(!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE | I2C_FUNC_SMBUS_READ_BYTE_DATA)) {

		err = -EIO;

		goto exit;

	}



	data = kzalloc(sizeof(struct sht20_data),GFP_KERNEL);

	if(!data){

		err = -ENOMEM;

		goto exit;

	}

	data->client = client;

	

	i2c_set_clientdata(client, data);

	

	mutex_init(&data->update_lock); 

	err = sysfs_create_group(&client->dev.kobj, &sht20_attr_group);

	if(err)

		goto exit_kfree;



	return 0;



exit_kfree:

	kfree(data);

exit:

	return err;

}



static int __devexit sht20_remove(struct i2c_client *client)

{

	sysfs_remove_group(&client->dev.kobj, &sht20_attr_group);



	kfree(i2c_get_clientdata(client));

	return 0;

}



static struct i2c_device_id sht20_id[] = {

	{"sht20", 0 },

	{	}

};

MODULE_DEVICE_TABLE(i2c, sht20_id);



static struct i2c_driver sht20_driver = {

	.driver = {

		.name = SHT20_DRV_NAME,

		.owner = THIS_MODULE,

	},

	.probe = sht20_probe,

	.remove = sht20_remove,

	.id_table = sht20_id,

};



static int __init sht20_init(void)

{

	return i2c_add_driver(&sht20_driver);

}



static void __exit sht20_exit(void)

{

	i2c_del_driver(&sht20_driver);

}



MODULE_AUTHOR("jerry <hliang1025@163.com>");

MODULE_DESCRIPTION("sht20 t_rh sensor driver");

MODULE_LICENSE("GPL");



module_init(sht20_init);

module_exit(sht20_exit);
