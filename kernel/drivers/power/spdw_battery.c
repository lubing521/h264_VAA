#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/wakelock.h>
#include <linux/device.h>
#include <asm/gpio.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/usb.h>
#include <linux/spinlock.h>
#include <asm/io.h>
#include <board/board.h>

#define ENABLE_START_EN (1<<0)
#define ECFLG_END (1<<15)
#define REF_VOLTAGE 3000 // mV 
#define ESTIMATE_RATE (4095*1000/(REF_VOLTAGE*4))

#define MAXCAPACITY  2780
#define LOWLIMIT 2778 //(2600~2643, 43)
#define MINLIMIT 2628 // (2418~2469, 51)
#define EMPTLIMIT 2478 //
#define INTR 1


#define BATTVOLSAMPLE 1
unsigned long   gAvrVoltage[BATTVOLSAMPLE];
unsigned long   gIndex = 0;

static struct wake_lock vbus_wake_lock;

typedef enum {
        CHARGER_BATTERY = 0,
        CHARGER_USB,
        CHARGER_AC
} charger_type_t;

typedef enum {
     DISABLE_CHG = 0,
     ENABLE_SLOW_CHG,
     ENABLE_FAST_CHG
} batt_ctl_t;

typedef struct {
	int voltage_low;
	int voltage_high;
	int percentage;
}spdw_batt_vol;

const char *charger_tags[] = {"none", "USB", "AC"};

struct battery_info_reply {
	u32 batt_id;			/* Battery ID from ADC */
	u32 batt_vol;			/* Battery voltage from ADC */
	u32 batt_temp;			/* Battery Temperature (C) from formula and ADC */
	u32 batt_current;		/* Battery current from ADC */
	u32 level;				/* formula */
	u32 charging_source;	/* 0: no cable, 1:usb, 2:AC */
	u32 charging_enabled;	/* 0: Disable, 1: Enable */
	u32 full_bat;			/* Full capacity of battery (mAh) */
};

struct spdw_battery_info {
	struct platform_device *pdev;
	struct mutex lock;
	struct battery_info_reply rep;
	struct work_struct changed_work;
	int present;
	unsigned long update_time;
};

struct task_struct	*batt_thread_task;

//void usb_register_notifier(struct notifier_block *nb);

static int spdw_power_get_property(struct power_supply *psy, 
				  enum power_supply_property psp,
				  union power_supply_propval *val);

static int spdw_battery_get_property(struct power_supply *psy, 
				    enum power_supply_property psp,
				    union power_supply_propval *val);

static ssize_t spdw_battery_show_property(struct device *dev,
					 struct device_attribute *attr,
					 char *buf);

static struct spdw_battery_info spdw_batt_info;

static unsigned int cache_time = 1000;

static int spdw_battery_initial = 0;

static enum power_supply_property spdw_battery_properties[] = {
        POWER_SUPPLY_PROP_STATUS,
        POWER_SUPPLY_PROP_HEALTH,
        POWER_SUPPLY_PROP_PRESENT,
        POWER_SUPPLY_PROP_TECHNOLOGY,
        POWER_SUPPLY_PROP_CAPACITY,
};

static enum power_supply_property spdw_power_properties[] = {
        POWER_SUPPLY_PROP_ONLINE,
};

static char *supply_list[] = {
        "battery",
};

static struct power_supply spdw_power_supplies[] = {
    {
        .name = "battery",
        .type = POWER_SUPPLY_TYPE_BATTERY,
        .properties = spdw_battery_properties,
        .num_properties = ARRAY_SIZE(spdw_battery_properties),
        .get_property = spdw_battery_get_property,
    },
    {
        .name = "usb",
        .type = POWER_SUPPLY_TYPE_USB,
        .supplied_to = supply_list,
        .num_supplicants = ARRAY_SIZE(supply_list),
        .properties = spdw_power_properties,
        .num_properties = ARRAY_SIZE(spdw_power_properties),
        .get_property = spdw_power_get_property,
    },
    {
        .name = "ac",
        .type = POWER_SUPPLY_TYPE_MAINS,
        .supplied_to = supply_list,
        .num_supplicants = ARRAY_SIZE(supply_list),
        .properties = spdw_power_properties,
        .num_properties = ARRAY_SIZE(spdw_power_properties),
        .get_property = spdw_power_get_property,
    },
};

static void usb_status_notifier_func(struct notifier_block *self, unsigned long action, void *dev);
static int g_usb_online;

/*static struct notifier_block usb_status_notifier = {
   	.notifier_call = usb_status_notifier_func,
};*/

static unsigned long adcValue_sum;
static int first_adc = 30; //开机30秒每秒刷新

#ifdef CONFIG_SEP6200_TS
struct i2c_client ;
extern struct i2c_client *battery_bus;
extern int ad7879_read(struct i2c_client *client, u8 reg);
extern void adc_mode_switch(void);
#endif



//采样3次，取中间值
static unsigned long spdw_get_sample(void)
{
	unsigned long adcValue[3];
	int i, j;
	unsigned long temp;

	for(i=0; i<3; i++)
	{
		
		adcValue[i] = ad7879_read(battery_bus,12);
	
		j = i;
		while(j>0)
		{
			if(adcValue[j]>adcValue[j-1])
			{
				temp = adcValue[j-1];
				adcValue[j-1] = adcValue[j];
				adcValue[j] = temp; 
			}
			j--;
		}
		msleep(10);
	}


	if(first_adc>0)
	{
		int temp = 0;
		while(temp<BATTVOLSAMPLE)
		{
			gAvrVoltage[temp] = adcValue[1];
			temp ++;
		}
			
		adcValue_sum = adcValue[1] * BATTVOLSAMPLE;
		return 0;
	}

	gIndex = (gIndex+1)%BATTVOLSAMPLE;
	adcValue_sum = adcValue_sum -gAvrVoltage[gIndex];
	gAvrVoltage[gIndex] = adcValue[1];
	adcValue_sum = adcValue_sum + gAvrVoltage[gIndex];


	return 0;
}

#define SAMPLE 100
const spdw_batt_vol spdw_current_levels[] = {


	{4106,4900,100}, {4100,4105,99}, {4096,4100,98}, {4091,4095,97}, {4086,4090,96}, {4081,4085,95}, {4076,4080,94}, {4071,4075,93}, {4066,4070,92}, {4061,4065,91},
	{4052,4060, 90}, {4043,4051,89}, {4036,4042,88}, {4026,4035,87}, {4019,4025,86}, {4011,4018,85}, {4003,4010,84}, {3996,4002,82}, {3989,3995,82}, {3981,3988,81},
	{3971,3980,80}, {3961,3970,79}, {3951,3960,78}, {3941,3950,76}, {3931,3940,74}, {3921,3930,72}, {3914,3920,70}, {3906,3913,69}, {3899,3905,68}, {3891,3898,66},
	{3881,3890,64}, {3871,3880,62}, {3856,3870,60}, {3846,3855,57}, {3836,3845,54}, {3821,3835,52}, {3811,3820,50}, {3801,3810,47}, {3791,3800,43}, {3785,3790,40},
	{3779,3784,37}, {3771,3778,33}, {3766,3770,30}, {3756,3765,28}, {3746,3755,26}, {3741,3745,23}, {3726,3740,20}, {3716,3725,18}, {3701,3715,16}, {3691,3700,13},
	{3681,3690,10}, {3651,3680,8}, {3601,3650,5}, {3441,3600,3}, {3401,3440,1}, {0,3400,0}, 
};

const spdw_batt_vol spdw_battery_levels[] = {
	{4071,4500,100}, {4061,4070,99}, {4051,4060,98}, {4041,4050,97}, {4031,4040,96}, {4021,4030,95}, {4016,4020,94}, {4011,4015,93}, {4005,4010,92}, {4001,4005,91},
	{3985,4000, 90}, {3971,3985,89}, {3961,3970,88}, {3951,3960,87}, {3941,3950,86}, {3931,3940,85}, {3921,3930,84}, {3911,3920,82}, {3901,3910,82}, {3891,3900,81},
	{3876,3890,80}, {3861,3875,79}, {3846,3860,78}, {3831,3845,76}, {3821,3830,74}, {3811,3820,72}, {3801,3810,70}, {3791,3800,69}, {3781,3790,68}, {3771,3780,66},
	{3761,3770,64}, {3751,3760,62}, {3741,3750,60}, {3731,3740,57}, {3721,3730,54}, {3711,3720,52}, {3701,3710,50}, {3691,3700,47}, {3681,3690,43}, {3671,3680,40},
	{3661,3670,37}, {3651,3660,33}, {3641,3650,30}, {3631,3640,28}, {3616,3630,26}, {3601,3615,23}, {3581,3600,20}, {3561,3580,17}, {3556,3560,14}, {3546,3555,11},
	{3541,3545,8}, {3501,3540,5}, {3471,3500,1}, {3421,3470,0}, {3401,3420,0}, {0,3400,0}, 



	};	
struct battery_info_reply spdw_cur_battery_info =
{
	.batt_id = 0,			/* Battery ID from ADC */
	.batt_vol = 3600,			/* Battery voltage from ADC */
	.batt_temp = 100,			/* Battery Temperature (C) from formula and ADC */
	.batt_current = 100,		/* Battery current from ADC */
	.level = 100,				/* formula */
	.charging_source = 0,	/* 0: no cable, 1:usb, 2:AC */
	.charging_enabled  = 0,	/* 0: Disable, 1: Enable */
	.full_bat = 1100			/* Full capacity of battery (mAh) */
};


/* ADC raw data Update to Android framework */
void spdw_read_battery(struct battery_info_reply *info)
{
	memcpy(info, &spdw_cur_battery_info, sizeof(struct battery_info_reply));

	pr_debug("spdw_read_battery read batt id=%d,voltage=%d, temp=%d, current=%d, level=%d, charging source=%d, charging_enable=%d, full=%d\n", 
		info->batt_id, info->batt_vol, info->batt_temp, info->batt_current, info->level, info->charging_source, info->charging_enabled, info->full_bat);
}


int spdw_cable_status_update(int status)
{
	int rc = 0;
	unsigned last_source;
	//struct spdw_udc *pdev = the_controller;

	if (!spdw_battery_initial)
	     return 0;

	mutex_lock(&spdw_batt_info.lock);

	switch(status) {
	case CHARGER_BATTERY:
		printk("cable NOT PRESENT\n");
		spdw_batt_info.rep.charging_source = CHARGER_BATTERY;
		break;
	case CHARGER_USB:
		printk("cable USB\n");
		spdw_batt_info.rep.charging_source = CHARGER_USB;
		break;
	case CHARGER_AC:
		printk("cable AC\n");
		spdw_batt_info.rep.charging_source = CHARGER_AC;
		break;
	default:
		printk(KERN_ERR "%s: Not supported cable status received!\n", __FUNCTION__);
		rc = -EINVAL;
	}

	if ((status == CHARGER_USB) && (g_usb_online == 0)) {
		spdw_batt_info.rep.charging_source = CHARGER_AC;
	}

	if ((status == CHARGER_BATTERY) && (g_usb_online == 1)) {
		g_usb_online = 0;
	}
	

	last_source = spdw_batt_info.rep.charging_source;
	mutex_unlock(&spdw_batt_info.lock);
	
	if (0) {
		status = CHARGER_USB;
	} 

	if (last_source == CHARGER_USB) {
 		wake_lock(&vbus_wake_lock);
	} else {
		/* give userspace some time to see the uevent and update
	  	* LED state or whatnot...
	  	*/
	 	wake_lock_timeout(&vbus_wake_lock, HZ / 2);
	}

	/* if the power source changes, all power supplies may change state */
	power_supply_changed(&spdw_power_supplies[CHARGER_BATTERY]);
	power_supply_changed(&spdw_power_supplies[CHARGER_USB]);
	power_supply_changed(&spdw_power_supplies[CHARGER_AC]);
	
	return rc;
}


/* A9 reports USB charging when helf AC cable in and China AC charger. */
/* Work arround: notify userspace AC charging first,
and notify USB charging again when receiving usb connected notificaiton from usb driver. */
static void usb_status_notifier_func(struct notifier_block *self, unsigned long action, void *dev)
{
	mutex_lock(&spdw_batt_info.lock);

	if (action == USB_BUS_ADD) {
	 	if (!g_usb_online) {
			g_usb_online = 1;
			mutex_unlock(&spdw_batt_info.lock);
			spdw_cable_status_update(CHARGER_USB);
			mutex_lock(&spdw_batt_info.lock);
		}     
	} else {
		g_usb_online = 0;
		mutex_unlock(&spdw_batt_info.lock);
		
		if(0)
			spdw_cable_status_update(CHARGER_AC);
		else
			spdw_cable_status_update(CHARGER_BATTERY);
		
		mutex_lock(&spdw_batt_info.lock);
	}
	mutex_unlock(&spdw_batt_info.lock);
}

static int spdw_get_battery_info(struct battery_info_reply *buffer)
{
    struct battery_info_reply info;

	if (buffer == NULL) 
	     return -EINVAL;

	spdw_read_battery(&info);
                             
	buffer->batt_id                 = (info.batt_id);
	buffer->batt_vol                = (info.batt_vol);
	buffer->batt_temp               = (info.batt_temp);
	buffer->batt_current            = (info.batt_current);
	buffer->level                   = (info.level);

	buffer->charging_enabled        = (info.charging_enabled);
	buffer->full_bat                = (info.full_bat);
	
     return 0;
}


static int spdw_power_get_property(struct power_supply *psy, 
			                                  	enum power_supply_property psp,
			                                  	union power_supply_propval *val)
{
	charger_type_t charger;
	
	mutex_lock(&spdw_batt_info.lock);
	charger = spdw_batt_info.rep.charging_source;
	mutex_unlock(&spdw_batt_info.lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (psy->type == POWER_SUPPLY_TYPE_MAINS)
			val->intval = (charger ==  CHARGER_AC ? 1 : 0);
		else if (psy->type == POWER_SUPPLY_TYPE_USB)
			val->intval = (charger ==  CHARGER_USB ? 1 : 0);
		else
			val->intval = 0;
		break;
	default:
		return -EINVAL;
	}
	
	return 0;
}


static int spdw_battery_get_charging_status(void)
{
	u32 level;
	charger_type_t charger;	
	int ret;

	mutex_lock(&spdw_batt_info.lock);
	charger = spdw_batt_info.rep.charging_source;
	
	switch (charger) {
	case CHARGER_BATTERY:
		ret = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	case CHARGER_USB:
	case CHARGER_AC:
		level = spdw_batt_info.rep.level;
		if (level == 100)
			ret = POWER_SUPPLY_STATUS_FULL;
		else
			ret = POWER_SUPPLY_STATUS_CHARGING;
		break;
	default:
		ret = POWER_SUPPLY_STATUS_UNKNOWN;
	}

	mutex_unlock(&spdw_batt_info.lock);
	return ret;
}

EXPORT_SYMBOL(spdw_battery_get_charging_status);


static int spdw_battery_get_property(struct power_supply *psy, 
												    enum power_supply_property psp,
												    union power_supply_propval *val)
{

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = spdw_battery_get_charging_status();
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = spdw_batt_info.present;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		mutex_lock(&spdw_batt_info.lock);
		val->intval = spdw_batt_info.rep.level;
		mutex_unlock(&spdw_batt_info.lock);
		break;
	default:		
		return -EINVAL;
	}
	
	return 0;
}



static int spdw_battery_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

static int spdw_battery_resume(struct platform_device *dev)
{	
	(first_adc>3)?:(first_adc = 3);
	
	return 0;
}


#define SEP0611_BATTERY_ATTR(_name)                                            \
{                                                                            \
     .attr = { .name = #_name, .mode = S_IRUGO, .owner = THIS_MODULE },      \
     .show = spdw_battery_show_property,                                      \
     .store = NULL,                                                          \
}

static struct device_attribute spdw_battery_attrs[] = {
     SEP0611_BATTERY_ATTR(batt_id),
     SEP0611_BATTERY_ATTR(batt_vol),
     SEP0611_BATTERY_ATTR(batt_temp),
     SEP0611_BATTERY_ATTR(batt_current),
     SEP0611_BATTERY_ATTR(charging_source),
     SEP0611_BATTERY_ATTR(charging_enabled),
     SEP0611_BATTERY_ATTR(full_bat),
};

enum {
     BATT_ID = 0,
     BATT_VOL,
     BATT_TEMP,
     BATT_CURRENT,
     CHARGING_SOURCE,
     CHARGING_ENABLED,
     FULL_BAT,
};


static int spdw_battery_create_attrs(struct device * dev)
{
     int i, rc;
     for (i = 0; i < ARRAY_SIZE(spdw_battery_attrs); i++) {
         rc = device_create_file(dev, &spdw_battery_attrs[i]);
         if (rc)
                 goto spdw_attrs_failed;
     }
     
     goto succeed;
     
spdw_attrs_failed:
     while (i--)
     	device_remove_file(dev, &spdw_battery_attrs[i]);

succeed:        
     return rc;
}


static ssize_t spdw_battery_show_property(struct device *dev,
                                     struct device_attribute *attr,
                                     char *buf)
{
    int i = 0;
    const ptrdiff_t off = attr - spdw_battery_attrs;

    mutex_lock(&spdw_batt_info.lock);
    /* check cache time to decide if we need to update */
    if (spdw_batt_info.update_time &&
        time_before(jiffies, spdw_batt_info.update_time + msecs_to_jiffies(cache_time)))
            goto dont_need_update;
    
    if (spdw_get_battery_info(&spdw_batt_info.rep) < 0) {
            printk(KERN_ERR "%s: rpc failed!!!\n", __FUNCTION__);
    } else {
            spdw_batt_info.update_time = jiffies;
    }
dont_need_update:
    mutex_unlock(&spdw_batt_info.lock);

    mutex_lock(&spdw_batt_info.lock);
    switch (off) {
    case BATT_ID:
            i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
                           spdw_batt_info.rep.batt_id);
            break;
    case BATT_VOL:
            i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
                           spdw_batt_info.rep.batt_vol);
            break;
    case BATT_TEMP:
            i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
                           spdw_batt_info.rep.batt_temp);
            break;
    case BATT_CURRENT:
            i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
                           spdw_batt_info.rep.batt_current);
            break;
    case CHARGING_SOURCE:
            i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
                           spdw_batt_info.rep.charging_source);
            break;
    case CHARGING_ENABLED:
            i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
                           spdw_batt_info.rep.charging_enabled);
            break;          
    case FULL_BAT:
            i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
                           spdw_batt_info.rep.full_bat);
            break;
    default:
            i = -EINVAL;
    }       
    mutex_unlock(&spdw_batt_info.lock);
    
    return i;
}


static u32 temp_charging_source = CHARGER_BATTERY;
static int temp_level_count = 0;

static void spdw_battery_set_param(struct battery_info_reply* pinfo, unsigned long BattVoltage)
{	
	int source, count = 0, temp;
	spdw_batt_vol *spdw_batt;
	int size;
	pinfo->batt_id          = 0;
	temp                  	= BattVoltage *1000/ESTIMATE_RATE; //mv
	pinfo->batt_vol      	= temp;//BattVoltage *1000/ESTIMATE_RATE; //mv
	pinfo->charging_source	= spdw_batt_info.rep.charging_source;
	source			= spdw_batt_info.rep.charging_source;
 	spdw_batt = (source == 0) ? spdw_battery_levels : spdw_current_levels;
	size	 =  (source == 0) ? ARRAY_SIZE(spdw_battery_levels) : ARRAY_SIZE(spdw_current_levels);
	pinfo->level = 95;
	for (count = 0; count < size; count++)
	{	
		if ((temp >= spdw_batt[count].voltage_low) && (temp <= spdw_batt[count].voltage_high))
			pinfo->level = spdw_batt[count].percentage;
		
	}

	pinfo->batt_temp = 100;	
	pinfo->full_bat                	= 5800; // 电池大小1100mAh
	pinfo->charging_enabled = 0;

	if(pinfo->level < 100 && (source == CHARGER_AC||source==CHARGER_USB)){
		pinfo->charging_enabled = 1;
	} else {
		pinfo->charging_enabled = 0;
	}
#if 0
		printk("read ::batt id=%d,voltage=%d, temp=%d, current=%d, level=%d, charging source=%d, charging_enable=%d, full=%d\n", 
		pinfo->batt_id, pinfo->batt_vol, pinfo->batt_temp, pinfo->batt_current, pinfo->level, pinfo->charging_source, pinfo->charging_enabled, pinfo->full_bat);
#endif
}

static void  spdw_battery_process(void) 
{
	unsigned long BattVoltage,temp;
	struct battery_info_reply info;
		
	BattVoltage = adcValue_sum/BATTVOLSAMPLE;
	gAvrVoltage[gIndex%BATTVOLSAMPLE] = BattVoltage;
	
	temp = BattVoltage*1000/ESTIMATE_RATE;
	if(temp<=3200)
		printk("!!!!!!!low battery ![%d]mV\n", temp);

	// 1秒采样一次，30秒更新一次电池信息
	if(first_adc>0)
	{
		first_adc--;
		spdw_battery_set_param(&info, BattVoltage);
		memcpy(&spdw_cur_battery_info, &info, sizeof(struct battery_info_reply));
		power_supply_changed(&spdw_power_supplies[info.charging_source]);
	}else if(gIndex%BATTVOLSAMPLE == 0)
	{
		spdw_battery_set_param(&info, BattVoltage);

		memcpy(&spdw_cur_battery_info, &info, sizeof(struct battery_info_reply));
		power_supply_changed(&spdw_power_supplies[info.charging_source]);
	}
	temp_charging_source = spdw_batt_info.rep.charging_source;
	
	#if 0
	printk("batt id=%d,voltage=%d, temp=%d, current=%d, level=%d, charging source=%d, charging_enable=%d, full=%d\n", 
		info.batt_id, info.batt_vol, info.batt_temp, info.batt_current, info.level, info.charging_source, info.charging_enabled, info.full_bat);
	#endif
}


static int spdw_battery_thread(void *v)
{
	struct task_struct *tsk = current;	
	daemonize("spdw-battery");
	allow_signal(SIGKILL);
	int charge = 0;

	while (!signal_pending(tsk)) {
		 if(charge=sep0611_gpio_getpin(SEP0611_DC_DET))//to do:获得充电状态
		{
			spdw_batt_info.rep.charging_source = CHARGER_AC;
		}else if(charge = sep0611_gpio_getpin(SEP0611_VBUS_DET))
		{ 
			
			spdw_batt_info.rep.charging_source = CHARGER_USB;
		}
		  else
		{
			spdw_batt_info.rep.charging_source = CHARGER_BATTERY;
		}
	
		if(temp_charging_source != spdw_batt_info.rep.charging_source) 
		{
			(first_adc>3)?:(first_adc = 3);
		}

		adc_mode_switch();	

   		if(spdw_get_sample() == 0)
   		{
			spdw_battery_process();
   		}

		msleep(1000);
	}

	return 0;
}


static int spdw_battery_probe(struct platform_device *pdev)
{
    int i, err;

    spdw_batt_info.update_time = jiffies;
    spdw_battery_initial = 1;

    /* init power supplier framework */
    for (i = 0; i < ARRAY_SIZE(spdw_power_supplies); i++) {
        err = power_supply_register(&pdev->dev, &spdw_power_supplies[i]);
        if (err)
        	printk(KERN_ERR "Failed to register power supply (%d)\n", err);
    }

    /* create spdw detail attributes */
    spdw_battery_create_attrs(spdw_power_supplies[CHARGER_BATTERY].dev);

    if (0)
    	spdw_batt_info.rep.charging_source = CHARGER_USB;
    else
    	spdw_batt_info.rep.charging_source = CHARGER_BATTERY;

    spdw_battery_initial = 1;
	
    // Get current Battery info
    if (spdw_get_battery_info(&spdw_batt_info.rep) < 0)
    	printk(KERN_ERR "%s: get info failed\n", __FUNCTION__);

    spdw_batt_info.present = 1;
    spdw_batt_info.update_time = jiffies;
    spdw_batt_info.pdev   = pdev;
    spdw_batt_info.rep.level = 100;
    spdw_batt_info.update_time = jiffies;

    //GPIO DC_DET 
    sep0611_gpio_cfgpin(SEP0611_DC_DET,SEP0611_GPIO_IO);        
    sep0611_gpio_dirpin(SEP0611_DC_DET,SEP0611_GPIO_IN);
     
    //GPIO USB_DET
    sep0611_gpio_cfgpin(SEP0611_VBUS_DET,SEP0611_GPIO_IO);        
    sep0611_gpio_dirpin(SEP0611_VBUS_DET,SEP0611_GPIO_IN);

     //avoid usb votage 
 //   sep0611_gpio_cfgpin(SEP0611_USB_OTG_DRV,SEP0611_GPIO_IO);        
 //   sep0611_gpio_dirpin(SEP0611_USB_OTG_DRV,SEP0611_GPIO_IN);
 //   sep0611_gpio_setpin(SEP0611_USB_OTG_DRV,GPIO_LOW);

    sep0611_gpio_dirpin(SEP0611_CHG_OK,SEP0611_GPIO_IN);
	
    kernel_thread(spdw_battery_thread, NULL, CLONE_KERNEL);

    printk("SEP0611 Battery Driver Load.\n");
	
    return 0;
}

static int spdw_battery_remove(struct platform_device *pdev)
{
	return 0;
}


static struct platform_driver spdw_battery_driver = {
    .driver.name	= "spdw-battery",
	.driver.owner	= THIS_MODULE,
	.probe			= spdw_battery_probe,
	.remove 			= spdw_battery_remove,
	.suspend			= spdw_battery_suspend,
	.resume			= spdw_battery_resume,
};


static int __init spdw_battery_init(void)
{
	printk("%s\n", __func__);

	wake_lock_init(&vbus_wake_lock, WAKE_LOCK_SUSPEND, "vbus_present");

	mutex_init(&spdw_batt_info.lock);

	//usb_register_notifier(&usb_status_notifier);

	return platform_driver_register(&spdw_battery_driver);
}


static void __exit spdw_battery_exit(void)
{
	platform_driver_unregister(&spdw_battery_driver);
	//usb_unregister_notify(&usb_status_notifier);
}


module_init(spdw_battery_init);
MODULE_DESCRIPTION("Spread win Battery Driver");
MODULE_AUTHOR("allenseu@gmail.com");
MODULE_LICENSE("GPL");
