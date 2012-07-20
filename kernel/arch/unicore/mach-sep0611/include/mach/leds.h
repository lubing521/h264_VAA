#ifndef __ARCH_LEDS_H
#define __ARCH_LEDS_H

#define SEP0611_LEDF_ACTLOW		(1<<0)		/* LED is on when GPIO low */
#define SEP0611_LEDF_ACTHIGH	(1<<1)		/* LED is on when GPIO high */

struct sep0611_led_platdata {
	unsigned int gpio;
	unsigned int flags;
	
	char *name;
	char *def_trigger;
};

#endif
