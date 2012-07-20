/*
 * wifi_power.c
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include "wifi_power.h"

#if (WIFI_GPIO_POWER_CONTROL == 1)

#define BOARD_ANDROID_RK2800SDK		0
#define BOARD_ANDROID_RK2808SDK		1
#define BOARD_RK2800_DEMO					0
#define BOARD_RUIGUANG_T28				0
int wifi_external_eeprom=1;
int wifi_default_region=0x10;
unsigned long driver_ps_timeout=2*60*1000;

struct wifi_power power_gpio = 
{
    //POWER_USE_GPIO, POWER_GPIO_IOMUX, GPIOF5_APWM3_DPWM3_NAME,
    //0, GPIOPortF_Pin5, GPIO_HIGH
    0, 0, 0, 0, 0, 0
};

struct wifi_power power_save_gpio = 
{
	0, 0, 0, 0, 0, 0

};

struct wifi_power power_reset_gpio = 
{
	0, 0, 0, 0, 0, 0
};

#if 0
void wifi_pause_sdio(void)
{
	u32 val;

	printk("Pause SDIO for WiFi reset \n");

	/* Set them as GPIO mode */
	rockchip_mux_api_set(GPIOG_MMC1_SEL_NAME, IOMUXA_GPIO1_C237);
	rockchip_mux_api_set(GPIOG_MMC1D_SEL_NAME, IOMUXA_GPIO1_C456);

	/* Set CMD as GPIO OUTPUT & LOW */
	GPIOSetPinDirection(GPIOPortG_Pin2, GPIO_OUT);
	GPIOSetPinLevel(GPIOPortG_Pin2, GPIO_LOW);
	GPIOSetPinDirection(GPIOPortG_Pin3, GPIO_OUT);
	GPIOSetPinLevel(GPIOPortG_Pin3, GPIO_LOW);
	GPIOSetPinDirection(GPIOPortG_Pin4, GPIO_OUT);
	GPIOSetPinLevel(GPIOPortG_Pin4, GPIO_LOW);
	GPIOSetPinDirection(GPIOPortG_Pin5, GPIO_OUT);
	GPIOSetPinLevel(GPIOPortG_Pin5, GPIO_LOW);
	GPIOSetPinDirection(GPIOPortG_Pin6, GPIO_OUT);
	GPIOSetPinLevel(GPIOPortG_Pin6, GPIO_LOW);
	GPIOSetPinDirection(GPIOPortG_Pin7, GPIO_OUT);
	GPIOSetPinLevel(GPIOPortG_Pin7, GPIO_LOW);
	mdelay(9000);
	mdelay(9000);
	mdelay(9000);
	mdelay(9000);
	mdelay(9000);
	mdelay(9000);
}

void wifi_resume_sdio(void)
{
	u32 val;

	printk("Pause SDIO for WiFi reset \n");

	/* Set them as GPIO mode */
	rockchip_mux_api_set(GPIOG_MMC1_SEL_NAME, IOMUXA_SDMMC1_CMD_DATA0_CLKOUT);
	rockchip_mux_api_set(GPIOG_MMC1D_SEL_NAME, IOMUXA_SDMMC1_DATA123);

	mdelay(9000);
}

int wifi_gpio_operate(struct wifi_power *gpio, int flag)
{
	int value, sensitive;
	
	if (gpio->use_gpio == POWER_NOT_USE_GPIO)
		return 0;
	
	if (gpio->gpio_iomux == POWER_GPIO_IOMUX)
	{
		rockchip_mux_api_set(gpio->iomux_name, gpio->iomux_value);
	}
	
	GPIOSetPinDirection(gpio->gpio_id, GPIO_OUT);
	
	if (flag == GPIO_SWITCH_ON)
		sensitive = gpio->sensi_level;
	else
		sensitive = 1 - gpio->sensi_level;
		
	GPIOSetPinLevel(gpio->gpio_id, sensitive);
	
	value = GPIOGetPinLevel(gpio->gpio_id);
	if (value != sensitive)
	{
		return -1;
	}
	
	return 0;
}

void sn7325_gpio_set_output_by_name(char * name,int value);

#endif 

int wifi_turn_on_card(void)
{
	printk("Atheros Turning on SDIO card.\n");

    /*
	if (wifi_gpio_operate(&power_gpio, GPIO_SWITCH_ON) != 0)
	{
		printk("Couldn't set GPIO [ON] successfully for power supply.\n");
		return -1;
	}
    */

    sep0611_gpio_setpin(SEP0611_WIFI_EN,GPIO_HIGH);
	msleep(5);


//	msleep(1000);
	return 0;
}

int wifi_turn_off_card(void)
{
	printk("Turning off SDIO card.\n");
	
    
    /*
	if (wifi_gpio_operate(&power_gpio, GPIO_SWITCH_OFF) != 0)
	{
		printk("Couldn't set GPIO [OFF] successfully for power supply.\n");
		return -1;
	}
    */

    sep0611_gpio_setpin(SEP0611_WIFI_EN,GPIO_LOW);
	msleep(5);

//	msleep(1000);	
	return 0;
}


int wifi_power_up_wifi(void)
{
	int ret = 0;

	printk("Atheros Power up SDIO card.\n");

	sep0611_gpio_setpin(SEP0611_WIFI_NRST,GPIO_HIGH);
	msleep(50);
	sep0611_gpio_setpin(SEP0611_WIFI_NRST,GPIO_LOW);
	 msleep(200);
	sep0611_gpio_setpin(SEP0611_WIFI_NRST,GPIO_HIGH);
	 msleep(50);



    /*
	//wifi_pause_sdio();

	if (wifi_gpio_operate(&power_save_gpio, GPIO_SWITCH_ON) != 0)
	{
		printk("Couldn't set GPIO [ON] successfully for power up.\n");
		goto error;
	}
	mdelay(5);
	
	if (wifi_gpio_operate(&power_save_gpio, GPIO_SWITCH_OFF) != 0)
	{
		printk("Couldn't set GPIO [ON] successfully for power up.\n");
		goto error;
	}
	msleep(150);

	if (wifi_gpio_operate(&power_save_gpio, GPIO_SWITCH_ON) != 0)
	{
		printk("Couldn't set GPIO [ON] successfully for power up.\n");
		goto error;
	}
	msleep(50);
    */

	//wifi_resume_sdio();

	return 0;

error:
	//wifi_resume_sdio();
	return -1;
}

int wifi_power_down_wifi(void)
{
    /*
	if (wifi_gpio_operate(&power_save_gpio, GPIO_SWITCH_OFF) != 0)
	{
		printk("Couldn't set GPIO [OFF] successfully for power down.\n");
		return -1;
	}
    */
	
        printk("wifi_power_down_  add by \n");

	sep0611_gpio_setpin(SEP0611_WIFI_NRST,GPIO_LOW);

	return 0;
}

#if 0
int wifi_power_reset(void)
{
	int sensitive;
	struct wifi_power *gpio = &power_reset_gpio;
	
	printk("Atheros wifi_power_reset\n");

    /*

	if (gpio->use_gpio == POWER_NOT_USE_GPIO)
		return 0;
	
	if (gpio->gpio_iomux == POWER_GPIO_IOMUX)
	{
		rockchip_mux_api_set(gpio->iomux_name, gpio->iomux_value);
	}
	
	GPIOSetPinDirection(gpio->gpio_id, GPIO_OUT);
	
	sensitive = 1 - gpio->sensi_level;
	printk("Set RESET PIN to %d\n", sensitive);
	GPIOSetPinLevel(gpio->gpio_id, sensitive);
	mdelay(5000);

	sensitive = gpio->sensi_level;
	printk("Set RESET PIN to %d\n", sensitive);
	GPIOSetPinLevel(gpio->gpio_id, sensitive);
	mdelay(5000);

	sensitive = 1 - gpio->sensi_level;
	printk("Set RESET PIN to %d\n", sensitive);
	GPIOSetPinLevel(gpio->gpio_id, sensitive);
	mdelay(5000);
    */

	return 0;
}
#endif 
#endif /* WIFI_GPIO_POWER_CONTROL */

