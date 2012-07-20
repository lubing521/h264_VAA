#! /bin/sh

while [ 1 ]
do
	sleep 1
	echo 1 > /sys/devices/platform/sep0611_led.2/leds/LED_Statue/brightness 
	sleep 1
	echo 0 > /sys/devices/platform/sep0611_led.2/leds/LED_Statue/brightness
done

