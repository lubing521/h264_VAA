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

#define ESTIMATE_RATE 100//54
#define MIN_VOL       250  //add 2012-4-7
#define MAX_VOL       450  //add 2012-4-7
#define BATTVOLSAMPLE 30

extern int read_xpt2046(void);
unsigned long   gAvrVoltage[BATTVOLSAMPLE];
unsigned long   gIndex = 0;

static struct wake_lock vbus_wake_lock;

typedef enum {
        CHARGER_BATTERY = 0,
        CHARGER_USB,
        CHARGER_AC
} charger_type_t;

static u32 temp_charging_source = CHARGER_AC;
typedef enum {
     DISABLE_CHG = 0,
     ENABLE_SLOW_CHG,
     ENABLE_FAST_CHG
} batt_ctl_t;

typedef struct {
	int voltage_low;
	int voltage_high;
	int percentage;
}sep0611_batt_vol;

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

struct sep0611_battery_info {
	struct platform_device *pdev;
	struct mutex lock;
	struct battery_info_reply rep;
	struct work_struct changed_work;
	int present;
	unsigned long update_time;
	unsigned int suspended;
};

struct task_struct	*batt_thread_task;

//void usb_register_notifier(struct notifier_block *nb);

static int sep0611_power_get_property(struct power_supply *psy, 
				  enum power_supply_property psp,
				  union power_supply_propval *val);

static int sep0611_battery_get_property(struct power_supply *psy, 
				    enum power_supply_property psp,
				    union power_supply_propval *val);

static ssize_t sep0611_battery_show_property(struct device *dev,
					 struct device_attribute *attr,
					 char *buf);

static struct sep0611_battery_info sep0611_batt_info;

static unsigned int cache_time = 1000;

static int sep0611_battery_initial = 0;

static enum power_supply_property sep0611_battery_properties[] = {
        POWER_SUPPLY_PROP_STATUS,
        POWER_SUPPLY_PROP_HEALTH,
        POWER_SUPPLY_PROP_PRESENT,
        POWER_SUPPLY_PROP_TECHNOLOGY,
        POWER_SUPPLY_PROP_CAPACITY,
};

static enum power_supply_property sep0611_power_properties[] = {
        POWER_SUPPLY_PROP_ONLINE,
};

static char *supply_list[] = {
        "battery",
};

static struct power_supply sep0611_power_supplies[] = {
    {
        .name = "battery",
        .type = POWER_SUPPLY_TYPE_BATTERY,
        .properties = sep0611_battery_properties,
        .num_properties = ARRAY_SIZE(sep0611_battery_properties),
        .get_property = sep0611_battery_get_property,
    },
    {
        .name = "usb",
        .type = POWER_SUPPLY_TYPE_USB,
        .supplied_to = supply_list,
        .num_supplicants = ARRAY_SIZE(supply_list),
        .properties = sep0611_power_properties,
        .num_properties = ARRAY_SIZE(sep0611_power_properties),
        .get_property = sep0611_power_get_property,
    },
    {
        .name = "ac",
        .type = POWER_SUPPLY_TYPE_MAINS,
        .supplied_to = supply_list,
        .num_supplicants = ARRAY_SIZE(supply_list),
        .properties = sep0611_power_properties,
        .num_properties = ARRAY_SIZE(sep0611_power_properties),
        .get_property = sep0611_power_get_property,
    },
};

//static void usb_status_notifier_func(struct notifier_block *self, unsigned long action, void *dev);
static int g_usb_online;

/*static struct notifier_block usb_status_notifier = {
   	.notifier_call = usb_status_notifier_func,
};*/

static unsigned long adcValue_sum;
static int first_adc = 30; //开机30秒每秒刷新

#ifdef CONFIG_AD799X
extern int sep0611_read_ad7997(int channel);
#endif

int sep0611_battery_readadc(int channel)
{
	#ifdef CONFIG_AD799X
		return sep0611_read_ad7997(channel);
	#else
		return -1;
	#endif
}

static unsigned long sep0611_get_sample(void)
{
	unsigned long adcValue[3];
	int i, j;
	unsigned long temp;

	if(sep0611_batt_info.suspended  == 1)
		return -1;
	
	for(i=0; i<3; i++)
	{
		adcValue[i] =read_xpt2046();
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
	
    adcValue[1] = adcValue[1] + 10;	
    if(temp_charging_source==0)
        adcValue[1]+=10;
//	if((adcValue[1] <MIN_VOL) ||(adcValue[1] >MAX_VOL))
//        return -1;
	if (adcValue[1] < MIN_VOL)
		adcValue[1] = MIN_VOL;
	if (adcValue[1] > MAX_VOL)
		adcValue[1] = MAX_VOL;

	gAvrVoltage[gIndex] = adcValue[1];
    if(gIndex == 29)
	{
        adcValue_sum=0;
		int temp = 0;
		while(temp<BATTVOLSAMPLE)
		{
			adcValue_sum += gAvrVoltage[temp];
			temp ++;
		}
	}
	return 0;
}

//const sep0611_batt_vol sep0611_battery_levels[] = {
//    {341,MAX_VOL,100},
//    {331,340,90}, 
//    {321,330,80}, 
//    {311,320,70}, 
//    {301,310,60}, 
//    {291,300,50}, 
//    {281,290,40}, 
//    {271,280,30}, 
//    {261,270,20}, 
//    {256,260,10},
//    {MIN_VOL,255,0},
//	};

const sep0611_batt_vol sep0611_battery_levels[] = {
    {431,MAX_VOL,100},
    {413,430,90}, 
    {411,412,80}, 
    {408,410,70}, 
    {404,407,60}, 
    {398,403,50}, 
    {390,397,40}, 
    {383,389,30}, 
    {328,382,20}, 
    {308,327,10},
    {MIN_VOL,307,0},
	};
struct battery_info_reply sep0611_cur_battery_info =
{
	.batt_id = 0,			/* Battery ID from ADC */
	.batt_vol = 310,			/* Battery voltage from ADC */
	.batt_temp = 100,			/* Battery Temperature (C) from formula and ADC */
	.batt_current = 100,		/* Battery current from ADC */
	.level = 100,				/* formula */
	.charging_source = 0,	/* 0: no cable, 1:usb, 2:AC */
	.charging_enabled  = 0,	/* 0: Disable, 1: Enable */
	.full_bat = 350			/* Full capacity of battery (mAh) */
};


/* ADC raw data Update to Android framework */
void sep0611_read_battery(struct battery_info_reply *info)
{
	memcpy(info, &sep0611_cur_battery_info, sizeof(struct battery_info_reply));

	pr_debug("sep0611_read_battery read batt id=%d,voltage=%d, temp=%d, current=%d, level=%d, charging source=%d, charging_enable=%d, full=%d\n", 
		info->batt_id, info->batt_vol, info->batt_temp, info->batt_current, info->level, info->charging_source, info->charging_enabled, info->full_bat);
}


int sep0611_cable_status_update(int status)
{
	int rc = 0;
	unsigned last_source;
	//struct sep0611_udc *pdev = the_controller;

	if (!sep0611_battery_initial)
	     return 0;

	mutex_lock(&sep0611_batt_info.lock);

	switch(status) {
	case CHARGER_BATTERY:
		printk("cable NOT PRESENT\n");
		sep0611_batt_info.rep.charging_source = CHARGER_BATTERY;
		break;
	case CHARGER_USB:
		printk("cable USB\n");
		sep0611_batt_info.rep.charging_source = CHARGER_USB;
		break;
	case CHARGER_AC:
		printk("cable AC\n");
		sep0611_batt_info.rep.charging_source = CHARGER_AC;
		break;
	default:
		printk(KERN_ERR "%s: Not supported cable status received!\n", __FUNCTION__);
		rc = -EINVAL;
	}

	if ((status == CHARGER_USB) && (g_usb_online == 0)) {
		sep0611_batt_info.rep.charging_source = CHARGER_AC;
	}

	if ((status == CHARGER_BATTERY) && (g_usb_online == 1)) {
		g_usb_online = 0;
	}
	

	last_source = sep0611_batt_info.rep.charging_source;
	mutex_unlock(&sep0611_batt_info.lock);
	
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
	power_supply_changed(&sep0611_power_supplies[CHARGER_BATTERY]);
	power_supply_changed(&sep0611_power_supplies[CHARGER_USB]);
	power_supply_changed(&sep0611_power_supplies[CHARGER_AC]);
	
	return rc;
}

#if 0
/* A9 reports USB charging when helf AC cable in and China AC charger. */
/* Work arround: notify userspace AC charging first,
and notify USB charging again when receiving usb connected notificaiton from usb driver. */
static void usb_status_notifier_func(struct notifier_block *self, unsigned long action, void *dev)
{
	mutex_lock(&sep0611_batt_info.lock);

	if (action == USB_BUS_ADD) {
	 	if (!g_usb_online) {
			g_usb_online = 1;
			mutex_unlock(&sep0611_batt_info.lock);
			sep0611_cable_status_update(CHARGER_USB);
			mutex_lock(&sep0611_batt_info.lock);
		}     
	} else {
		g_usb_online = 0;
		mutex_unlock(&sep0611_batt_info.lock);
		
		if(0)
			sep0611_cable_status_update(CHARGER_AC);
		else
			sep0611_cable_status_update(CHARGER_BATTERY);
		
		mutex_lock(&sep0611_batt_info.lock);
	}
	mutex_unlock(&sep0611_batt_info.lock);
}
#endif
static int sep0611_get_battery_info(struct battery_info_reply *buffer)
{
    struct battery_info_reply info;

	if (buffer == NULL) 
	     return -EINVAL;

	sep0611_read_battery(&info);
                             
	buffer->batt_id                 = (info.batt_id);
	buffer->batt_vol                = (info.batt_vol);
	buffer->batt_temp               = (info.batt_temp);
	buffer->batt_current            = (info.batt_current);
	buffer->level                   = (info.level);
	buffer->charging_source         = (info.charging_source);
	buffer->charging_enabled        = (info.charging_enabled);
	buffer->full_bat                = (info.full_bat);
	
     return 0;
}


static int sep0611_power_get_property(struct power_supply *psy, 
			                                  	enum power_supply_property psp,
			                                  	union power_supply_propval *val)
{
	charger_type_t charger;
	
	mutex_lock(&sep0611_batt_info.lock);
	charger = sep0611_batt_info.rep.charging_source;
	mutex_unlock(&sep0611_batt_info.lock);

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


static int sep0611_battery_get_charging_status(void)
{
	u32 level;
	charger_type_t charger;	
	int ret;

	mutex_lock(&sep0611_batt_info.lock);
	charger = sep0611_batt_info.rep.charging_source;
	
	switch (charger) {
	case CHARGER_USB:
	case CHARGER_BATTERY:
		ret = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	case CHARGER_AC:
		level = sep0611_batt_info.rep.level;
		if (level == 100)
			ret = POWER_SUPPLY_STATUS_FULL;
		else
			ret = POWER_SUPPLY_STATUS_CHARGING;
		break;
	default:
		ret = POWER_SUPPLY_STATUS_UNKNOWN;
	}

	mutex_unlock(&sep0611_batt_info.lock);
	return ret;
}

EXPORT_SYMBOL(sep0611_battery_get_charging_status);


static int sep0611_battery_get_property(struct power_supply *psy, 
												    enum power_supply_property psp,
												    union power_supply_propval *val)
{

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = sep0611_battery_get_charging_status();
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = sep0611_batt_info.present;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		mutex_lock(&sep0611_batt_info.lock);
		val->intval = sep0611_batt_info.rep.level;
		mutex_unlock(&sep0611_batt_info.lock);
		break;
	default:		
		return -EINVAL;
	}
	
	return 0;
}



static int sep0611_battery_suspend(struct platform_device *dev, pm_message_t state)
{
	printk("%s\n", __func__);

	sep0611_batt_info.suspended = 1;
	
	return 0;
}

static int sep0611_battery_resume(struct platform_device *dev)
{	
	printk("%s\n", __func__);
	sep0611_batt_info.suspended = 0;
	
	return 0;
}


#define SEP0611_BATTERY_ATTR(_name)                                            \
{                                                                            \
     .attr = { .name = #_name, .mode = S_IRUGO, .owner = THIS_MODULE },      \
     .show = sep0611_battery_show_property,                                      \
     .store = NULL,                                                          \
}

static struct device_attribute sep0611_battery_attrs[] = {
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


static int sep0611_battery_create_attrs(struct device * dev)
{
     int i, rc;
     for (i = 0; i < ARRAY_SIZE(sep0611_battery_attrs); i++) {
         rc = device_create_file(dev, &sep0611_battery_attrs[i]);
         if (rc)
                 goto sep0611_attrs_failed;
     }
     
     goto succeed;
     
sep0611_attrs_failed:
     while (i--)
     	device_remove_file(dev, &sep0611_battery_attrs[i]);

succeed:        
     return rc;
}


static ssize_t sep0611_battery_show_property(struct device *dev,
                                     struct device_attribute *attr,
                                     char *buf)
{
    int i = 0;
    const ptrdiff_t off = attr - sep0611_battery_attrs;

    mutex_lock(&sep0611_batt_info.lock);
    /* check cache time to decide if we need to update */
    if (sep0611_batt_info.update_time &&
        time_before(jiffies, sep0611_batt_info.update_time + msecs_to_jiffies(cache_time)))
            goto dont_need_update;
    
    if (sep0611_get_battery_info(&sep0611_batt_info.rep) < 0) {
            printk(KERN_ERR "%s: rpc failed!!!\n", __FUNCTION__);
    } else {
            sep0611_batt_info.update_time = jiffies;
    }
dont_need_update:
    mutex_unlock(&sep0611_batt_info.lock);

    mutex_lock(&sep0611_batt_info.lock);
    switch (off) {
    case BATT_ID:
            i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
                           sep0611_batt_info.rep.batt_id);
            break;
    case BATT_VOL:
            i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
                           sep0611_batt_info.rep.batt_vol);
            break;
    case BATT_TEMP:
            i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
                           sep0611_batt_info.rep.batt_temp);
            break;
    case BATT_CURRENT:
            i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
                           sep0611_batt_info.rep.batt_current);
            break;
    case CHARGING_SOURCE:
            i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
                           sep0611_batt_info.rep.charging_source);
            break;
    case CHARGING_ENABLED:
            i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
                           sep0611_batt_info.rep.charging_enabled);
            break;          
    case FULL_BAT:
            i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
                           sep0611_batt_info.rep.full_bat);
            break;
    default:
            i = -EINVAL;
    }       
    mutex_unlock(&sep0611_batt_info.lock);
    
    return i;
}


static int temp_level_count = 0;

static void sep0611_battery_set_param(struct battery_info_reply* pinfo, unsigned long BattVoltage)
{	
	int source, count = 0, temp;
	int array_final = ARRAY_SIZE(sep0611_battery_levels) -1;

	pinfo->batt_id = 0;
	pinfo->batt_vol= BattVoltage * ESTIMATE_RATE/10;
	pinfo->charging_source	= sep0611_batt_info.rep.charging_source;
	source = sep0611_batt_info.rep.charging_source;

    for (count = 0; count < ARRAY_SIZE(sep0611_battery_levels); count++)
    {
        if ((BattVoltage >= sep0611_battery_levels[count].voltage_low) && (BattVoltage <= sep0611_battery_levels[count].voltage_high))
        {
            temp = temp_level_count;
            temp_level_count = count;
            if(count == array_final)
            {
                if(temp_level_count != array_final)
                    count = temp;
            }
            break;
        }
    }
    pinfo->level = sep0611_battery_levels[count].percentage;
	//printk("###########DEBUG percentage:%d  count:%d,battery_level:%d BattVoltage:%d\n",pinfo->level,count,sep0611_battery_levels[count].percentage,BattVoltage);
	pinfo->full_bat = 3500; 

	// source 
	if((pinfo->level < 100 )&& (source == CHARGER_AC)){
		pinfo->charging_enabled = 1;
	} else {
		pinfo->charging_enabled = 0;
    }	
}

static void  sep0611_battery_process(void) 
{
    unsigned long BattVoltage;
    struct battery_info_reply info;
    BattVoltage = gAvrVoltage[gIndex];
	//printk("%s,first_adc:%d,gIndex:%d\n",__func__,first_adc,gIndex);
    if(first_adc>0)
    {
        first_adc--;
        sep0611_battery_set_param(&info, BattVoltage);
        memcpy(&sep0611_cur_battery_info, &info, sizeof(struct battery_info_reply));
        memcpy(&sep0611_batt_info.rep, &info, sizeof(struct battery_info_reply));
    }
    else if(gIndex == 29)
    {
        BattVoltage = adcValue_sum/BATTVOLSAMPLE; 
        //printk("adcValue_sum = %d , BattVoltage = %d\n",adcValue_sum,BattVoltage);
        sep0611_battery_set_param(&info, BattVoltage);
        memcpy(&sep0611_cur_battery_info, &info, sizeof(struct battery_info_reply));
        memcpy(&sep0611_batt_info.rep, &info, sizeof(struct battery_info_reply));
    }
    gIndex = (gIndex+1)%BATTVOLSAMPLE;
    if(temp_charging_source != sep0611_batt_info.rep.charging_source)
    {
        temp_charging_source=sep0611_batt_info.rep.charging_source;
        power_supply_changed(&sep0611_power_supplies[temp_charging_source]);
        (first_adc>0)?:(first_adc = 3);
    }
}

static int sep0611_battery_thread(void *v)
{
    struct task_struct *tsk = current;	
    daemonize("sep0611-battery");
    allow_signal(SIGKILL);
    int charge;
    charge = 0;

    while (!signal_pending(tsk)) {
        sep0611_batt_info.rep.charging_source = CHARGER_BATTERY;

        if(sep0611_get_sample() == 0)
        {
            sep0611_battery_process();
        }

        msleep(5000);
    }

    return 0;
}


static int sep0611_battery_probe(struct platform_device *pdev)
{
    int i, err;
	printk("%s\n", __func__);

    sep0611_batt_info.update_time = jiffies;
    sep0611_battery_initial = 1;

    /* init power supplier framework */
    for (i = 0; i < ARRAY_SIZE(sep0611_power_supplies); i++) {
        err = power_supply_register(&pdev->dev, &sep0611_power_supplies[i]);
        if (err)
        	printk(KERN_ERR "Failed to register power supply (%d)\n", err);
    }

    /* create sep0611 detail attributes */
    sep0611_battery_create_attrs(sep0611_power_supplies[CHARGER_BATTERY].dev);
    sep0611_battery_initial = 1;
	
    // Get current Battery info
    sep0611_battery_set_param(&sep0611_batt_info.rep,read_xpt2046());

    sep0611_batt_info.present = 1;
    sep0611_batt_info.update_time = jiffies;
    sep0611_batt_info.pdev   = pdev;
    sep0611_batt_info.update_time = jiffies;
    sep0611_batt_info.suspended = 0;
    kernel_thread(sep0611_battery_thread, NULL, CLONE_KERNEL);

    printk("SEP0611 Battery Driver Load.\n");
	
    return 0;
}

static int sep0611_battery_remove(struct platform_device *pdev)
{
	return 0;
}


static struct platform_driver sep0611_battery_driver = {
    .driver.name	= "sep0611-battery",
	.driver.owner	= THIS_MODULE,
	.probe			= sep0611_battery_probe,
	.remove 		= sep0611_battery_remove,
	.suspend		= sep0611_battery_suspend,
	.resume			= sep0611_battery_resume,
};


static int __init sep0611_battery_init(void)
{
	printk("%s\n", __func__);

	wake_lock_init(&vbus_wake_lock, WAKE_LOCK_SUSPEND, "vbus_present");

	mutex_init(&sep0611_batt_info.lock);

	//usb_register_notifier(&usb_status_notifier);

	return platform_driver_register(&sep0611_battery_driver);
}


static void __exit sep0611_battery_exit(void)
{
	platform_driver_unregister(&sep0611_battery_driver);
	//usb_unregister_notify(&usb_status_notifier);
}


module_init(sep0611_battery_init);
MODULE_DESCRIPTION("SEP0611 Battery Driver");
MODULE_AUTHOR("lichun08@seuic.com");
MODULE_LICENSE("GPL");
