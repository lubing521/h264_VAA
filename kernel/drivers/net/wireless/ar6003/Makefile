
ATHEROS_DIR  := drivers/net/wireless/ar6003
REV ?= 2
EXTRA_CFLAGS += -I$(ATHEROS_DIR)/include -I$(ATHEROS_DIR)/host/include \
                  -I$(ATHEROS_DIR)/include/AR6002/hw4.0 \
                 -I$(ATHEROS_DIR)/host/os/linux/include \
                 -I$(ATHEROS_DIR)/host/wlan/include \
                 -I$(ATHEROS_DIR)/host/bmi/include \
                 -I$(ATHEROS_DIR)/host/hif/sdio/linux_sdio/include \

#EXTRA_CFLAGS += -D__linux__
#EXTRA_CFLAGS += -DFAKE_EEPROM_USED

#EXTRA_CFLAGS += -DHTC_RAW_INTERFACE
EXTRA_CFLAGS += -DKERNEL_2_6


EXTRA_CFLAGS += -DLINUX -DDEBUG -D__KERNEL__ \
                -DTCMD -DSEND_EVENT_TO_APP -DUSER_KEYS \
                -DNO_SYNC_FLUSH  -DHTC_EP_STAT_PROFILING -DWLAN_HEADERS\
                -DATH_AR6K_11N_SUPPORT -DWAPI_ENABLE \
                -DMULTIPLE_FRAMES_PER_INTERRUPT -DAR6000REV$(REV) \
                -DBLOCK_TX_PATH_FLAG -DCHECKSUM_OFFLOAD\

EXTRA_CFLAGS += -DANDROID_ENV -D__linux__ $(ATH_ANDROID_BUILD_FLAGS)
EXTRA_CFLAGS += -DINIT_MODE_DRV_ENABLED -DBMIENABLE_SET -DAR600x_SD31_XXX \
                -DATH6K_CONFIG_HIF_VIRTUAL_SCATTER
		  
EXTRA_CFLAGS += -DSDIO -DATH_DEBUG_MODULE

#EXTRA_CFLAGS += -DATH6K_CONFIG_CFG80211 

OSLINUX_PATH := host/os/linux/
OSLINUX_OBJS := $(OSLINUX_PATH)ar6000_drv.o $(OSLINUX_PATH)ar6000_raw_if.o $(OSLINUX_PATH)eeprom.o \
                $(OSLINUX_PATH)ioctl.o $(OSLINUX_PATH)netbuf.o $(OSLINUX_PATH)wireless_ext.o $(OSLINUX_PATH)ar6000_android.o \
                $(OSLINUX_PATH)ar6000_pm.o $(OSLINUX_PATH)hci_bridge.o $(OSLINUX_PATH)ar6k_pal.o \
                $(OSLINUX_PATH)wifi_power.o 
#

WMI_PATH := host/wmi/
WMI_OBJS := $(WMI_PATH)wmi.o

WLAN_PATH := host/wlan/src/
WLAN_OBJS := $(WLAN_PATH)wlan_node.o $(WLAN_PATH)wlan_recv_beacon.o $(WLAN_PATH)wlan_utils.o

BMI_PATH := host/bmi/src/
BMI_OBJS := $(BMI_PATH)bmi.o

HIF_PATH := host/hif/sdio/linux_sdio/src/
HIF_OBJS := $(HIF_PATH)hif.o $(HIF_PATH)hif_scatter.o

HTC_PATH := host/htc2/
HTC_OBJS := $(HTC_PATH)htc.o $(HTC_PATH)htc_recv.o $(HTC_PATH)htc_send.o $(HTC_PATH)htc_services.o \
            $(HTC_PATH)AR6000/ar6k.o $(HTC_PATH)AR6000/ar6k_events.o

MISC_PATH := host/miscdrv/
MISC_OBJS := $(MISC_PATH)common_drv.o $(MISC_PATH)credit_dist.o

REORDER_PATH := host/reorder/
REORDER_OBJS := $(REORDER_PATH)rcv_aggr.o

atheros-objs := $(OSLINUX_OBJS) $(WMI_OBJS) $(WLAN_OBJS) $(BMI_OBJS) $(HIF_OBJS) $(HTC_OBJS) $(MISC_OBJS) $(REORDER_OBJS)

obj-$(CONFIG_ATHEROS_AR6003)     += atheros.o
