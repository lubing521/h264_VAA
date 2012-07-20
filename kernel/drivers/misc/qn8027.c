#include <linux/module.h>
#include <linux/i2c.h> 
#include <linux/delay.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif


#if 1
#define FMDBG(x...)         printk(x)
#else
#define FMDBG(x...)
#endif

#define DRV_NAME            "fmt_qn8027"

#define QN_ADDR             0x2C

#define FREQ2CHREG(freq)    ((freq-7600)/5)
#define CHREG2FREQ(ch)      (ch*5+7600)

#define SYSTEM1             0x00
#define SYSTEM2             0x00 
#define CH                  0x01
#define CH_STEP             0x00 
#define CH_CH               0x03

static int fmt_status = 0;
static int fmt_freq = 10300;
static int fmt_power = 0;

static struct i2c_client * fmt_client;


static int qn8027_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int qn8027_remove(struct i2c_client *client);


static u32 QN8027_WriteReg (u8 addr, u8 data)
{
    i2c_smbus_write_byte_data(fmt_client, addr, data);
    return 0;
}

static u32 QN8027_ReadReg (u8 addr, u8 * data)
{
    u8 buf[8];
    struct i2c_msg msgs[2] = {
        {fmt_client->addr, 0, 1, &addr},
        {fmt_client->addr, 1, 1, buf},
    };
    buf[0] = addr;

    i2c_transfer(fmt_client->adapter, msgs, 2);

    *data = buf[0];

    return buf[0];

}

static void QNF_SetRegBit(u8 reg, u8 bitMask, u8 data_val)
{
    u8 temp;
    QN8027_ReadReg(reg, &temp);
    temp &= (u8)(~bitMask);
    temp |= data_val & bitMask;

    QN8027_WriteReg(reg, temp);
}


static void QN_ChipInitialization ()
{
    QN8027_WriteReg(0x00, 0x80);
    msleep(20);

    QNF_SetRegBit(0x00, 0x40, 0x40);
    QNF_SetRegBit(0x00, 0x40, 0x00);
    msleep(20);
    QN8027_WriteReg(0x18, 0xe4);
    QN8027_WriteReg(0x1b, 0xf0);

    //disable auto PA off
    QNF_SetRegBit(0x02, 0x18, 0x18);

    QNF_SetRegBit(0x00, 0x20, 0x20);
}

static void QN_EnterSleepMode ()
{
    QNF_SetRegBit(0x00, 0x20, 0x00);
}

static void QN_LeaveSleepMode ()
{
    QNF_SetRegBit(0x00, 0x20, 0x20);
}

static void QND_TXSetPower(u8 gain)
{
    u8 value = 0;
    value |= 0x40;
    value |= gain;
    QN8027_WriteReg(0x1F, value);
}

static u8 QNF_SetCh(u16 freq)
{
    u8 tStep;
    u8 tCh;
    u16 f;

    f = FREQ2CHREG(freq);
    tCh = (u8)f;
    QN8027_WriteReg(CH, tCh);

    QN8027_ReadReg(CH_STEP, &tStep);
    tStep &= ~CH_CH;
    tStep |= ((u8)(f >> 8) & CH_CH);
    QN8027_WriteReg(CH_STEP, tStep);

    return 1;
}

static u16 QNF_GetCh ()
{
    u8 tCh;
    u8 tStep;
    u16 ch = 0;

    QN8027_ReadReg(CH_STEP, &tStep);
    tStep &= CH_CH;
    ch = tStep;
    QN8027_ReadReg(CH, &tCh);
    ch = (ch << 8) + tCh;
    return CHREG2FREQ(ch);
}

static ssize_t qn8027_on_read(struct device *dev, struct device_attribute * attr, char * buf)
{
    if (fmt_status)
        return sprintf(buf, "on\n");
    else
        return sprintf(buf, "off\n");
}

static ssize_t qn8027_on_write(struct device *dev, struct device_attribute * attr, const char * buf, size_t count)
{
    FMDBG("write %s %d\n", buf, count);
    if (!strncmp (buf, "on\n", count))
    {
        fmt_status = 1;
        //close speaker in fmt mode
#ifdef SEP0611_SPK_CTL
        gpio_set_value(SEP0611_SPK_CTL, 0);
#endif
        
        QN_LeaveSleepMode ();
        FMDBG("fmt on\n");



    }
    else
    {
        fmt_status = 0;

#ifdef SEP0611_SPK_CTL
        gpio_set_value(SEP0611_SPK_CTL, 1);
#endif

        QN_EnterSleepMode();

        //close fm
        FMDBG("fmt off\n");
    }

    return count;

}

static ssize_t qn8027_freq_read(struct device *dev, struct device_attribute * attr, char * buf)
{

    return sprintf(buf, "freq %d, %d\n", fmt_freq/10, QNF_GetCh());
}


static ssize_t qn8027_freq_write(struct device *dev, struct device_attribute * attr, const char * buf, size_t count)
{
    sscanf(buf, "%d", &fmt_freq);
    fmt_freq *= 10;
    QNF_SetCh(fmt_freq);
    FMDBG("set freq %d\n", fmt_freq);
    return count;

}


static ssize_t qn8027_power_read(struct device *dev, struct device_attribute * attr, char * buf)
{
    return sprintf(buf, "power %d\n", fmt_power);

}

static ssize_t qn8027_power_write(struct device *dev, struct device_attribute * attr, const char * buf, size_t count)
{
    sscanf(buf, "%d", &fmt_power);
    QND_TXSetPower(fmt_power);
    FMDBG("set power %d\n", fmt_power);

    return count;
}


#ifdef CONFIG_HAS_EARLYSUSPEND
static void qn8027_suspend(struct early_suspend *h)
{
    //do nothing, pm will cut down power before real suspend
    //QN_EnterSleepMode();
}

static void qn8027_resume(struct early_suspend *h)
{


    if (fmt_status == 0) {
        QN_ChipInitialization ();
        QNF_SetCh(fmt_freq); 
        QND_TXSetPower(fmt_power);

        QN_EnterSleepMode();
    }
    else {
        //close speaker in fmt mode
#ifdef SEP0611_SPK_CTL
        gpio_set_value(SEP0611_SPK_CTL, 0);
#endif
    }


    //QN_LeaveSleepMode();
}

static struct early_suspend qn8027_early_suspend;

#endif

static DEVICE_ATTR(on, 0666, qn8027_on_read, qn8027_on_write);
static DEVICE_ATTR(freq, 0666, qn8027_freq_read, qn8027_freq_write);
static DEVICE_ATTR(txpower, 0666, qn8027_power_read, qn8027_power_write);

static struct attribute *qn8027_attributes[] =
{
    &dev_attr_on.attr,
    &dev_attr_freq.attr,
    &dev_attr_txpower.attr,
    NULL
};

static const struct attribute_group qn8027_group  = {
    .attrs = qn8027_attributes,
};


static const struct i2c_device_id qn8027_ids[] = {
    { "qn8027", 0 }
};

static struct i2c_driver qn8027_driver = {
    .driver = {
        .name = DRV_NAME,
    },
    .probe = qn8027_probe,
    .remove = __devexit_p(qn8027_remove),
    .id_table = qn8027_ids,
};

static int qn8027_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int error;

    FMDBG("%s\n", __FUNCTION__);

    fmt_client = client;
//    strlcpy(client->name, DRV_NAME, I2C_NAME_SIZE);

    error = sysfs_create_group(&client->dev.kobj, &qn8027_group);
    if (error)
    {
        FMDBG("%s create sysfs group error\n", __FUNCTION__);
        goto failout;
    }

#ifdef CONFIG_HAS_EARLYSUSPEND

    qn8027_early_suspend.suspend = qn8027_suspend;
    qn8027_early_suspend.resume = qn8027_resume;
    qn8027_early_suspend.level = 0x2;
    register_early_suspend(&qn8027_early_suspend);

#endif

    //open fm at startup, and enter sleep
    QN_ChipInitialization ();
    QNF_SetCh(fmt_freq); 
    QND_TXSetPower(fmt_power);
    QN_EnterSleepMode();

    return 0;

failout:
    return error;
}

static int __devexit qn8027_remove(struct i2c_client *client)
{
#ifdef CONFIG_HAS_EARLYSUSPEND
    unregister_early_suspend(&qn8027_early_suspend);
#endif

    sysfs_remove_group(&client->dev.kobj, &qn8027_group);

    return 0;
}

static int __init qn8027_init(void)
{
    int tmp = i2c_add_driver(&qn8027_driver);

    FMDBG("i2c add driver qn8027 result %d\n", tmp);

    return 0;
}

static void __exit qn8027_exit(void)
{
    i2c_del_driver(&qn8027_driver);
}



MODULE_DESCRIPTION("QN8027 FM TRANSMITTER");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

module_init(qn8027_init);
module_exit(qn8027_exit);
