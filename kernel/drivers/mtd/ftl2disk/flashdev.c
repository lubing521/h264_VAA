/*
 * flashdev.c - Multiple FTL disk driver - gzip-loading version - v. 0.8 beta.
 *
 * (C) Chad Page, Theodore Ts'o, et. al, 1995.
 *
 * This FTL disk is designed to have filesystems created on it and mounted
 * just like a regular floppy disk.
 *
 * It also does something suggested by Linus: use the buffer cache as the
 * FTL disk data.  This makes it possible to dynamically allocate the FTL disk
 * buffer - with some consequences I have to deal with as I write this.
 *
 * This code is based on the original flashdev.c, written mostly by
 * Theodore Ts'o (TYT) in 1991.  The code was largely rewritten by
 * Chad Page to use the buffer cache to store the FTL disk data in
 * 1995; Theodore then took over the driver again, and cleaned it up
 * for inclusion in the mainline kernel.
 *
 * The original CRAMDISK code was written by Richard Lyons, and
 * adapted by Chad Page to use the new FTL disk interface.  Theodore
 * Ts'o rewrote it so that both the compressed FTL disk loader and the
 * kernel decompressor uses the same inflate.c codebase.  The FTL disk
 * loader now also loads into a dynamic (buffer cache based) FTL disk,
 * not the old static FTL disk.  Support for the old static FTL disk has
 * been completely removed.
 *
 * Loadable module support added by Tom Dyas.
 *
 * Further cleanups by Chad Page (page0588@sundance.sjsu.edu):
 *	Cosmetic changes in #ifdef MODULE, code movement, etc.
 * 	When the FTL disk module is removed, free the protected buffers
 * 	Default FTL disk size changed to 2.88 MB
 *
 *  Added initrd: Werner Almesberger & Hans Lermen, Feb '96
 *
 * 4/25/96 : Made FTL disk size a parameter (default is now 4 MB)
 *		- Chad Page
 *
 * Add support for fs images split across >1 disk, Paul Gortmaker, Mar '98
 *
 * Make block size and block size shift for FTL disks a global macro
 * and set blk_size for -ENOSPC,     Werner Fink <werner@suse.de>, Apr '99
 */

#include <linux/string.h>
#include <linux/slab.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/buffer_head.h>
#include <linux/blkdev.h>
#include <linux/genhd.h>
#include <linux/blkpg.h>
#include <linux/hdreg.h>
#include <linux/times.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#include <linux/dma-mapping.h>
#include <asm/dma-mapping.h>
#include <asm/atomic.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/sep_ioctl.h>
#include "ftl_global.h"
#include "flash_if.h"
#include "ftl_if.h"

#define NO_SCHEDULER 1
#define USE_THREAD 0
#define NO_IOCTL 0

#define FTL_MAJOR 240
#define CONFIG_BLK_DEV_FTL_SIZE 4194304
#define CONFIG_BLK_DEV_FTL_BLOCK_SIZE 512
#define CONFIG_BLK_DEV_FTL_COUNT 1
#define CONFIG_BLK_DEV_FTL_MAX_SEC 128

#define CONFIG_BLK_DEV_PARTITION_NUM 6
#define TRUE 1
#define FALSE 0

#undef pr_debug

#if 0
#define pr_debug(fmt...) printk( fmt)
#define NAND_MSG(fmt...) printk( fmt)
#else
#define pr_debug(fmt...)
#define NAND_MSG(fmt...) LOG_OUT(fmt)
#endif
#ifdef _DEBUG
#include <asm/uaccess.h>
#include <linux/fs.h>
#define LOG_BUF_SIZE 4096
#define MAX_LOG_FILE_LEN 10485760
struct file* gLogfp;
char gLogBuf[LOG_BUF_SIZE];
char gLogStr[256];
unsigned int g_buf_len = 0;
loff_t gPosFp = 0;

inline void new_log_file(void) {
	struct timeval tv01;
	do_gettimeofday(&tv01);
	sprintf(gLogStr, "/mnt/sd/hd%010lu.log", tv01.tv_sec);
	gLogfp = filp_open(gLogStr, O_CREAT | O_WRONLY, 0);
	g_buf_len = 0;
	gPosFp = 0;
}

inline void log_to_file(void) {
	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);
	vfs_write(gLogfp,gLogBuf,g_buf_len,&gPosFp);
	set_fs(old_fs);
}

void log_page_to_file(char* logStr) {
	unsigned int str_len = strlen(gLogStr);
	unsigned int copy_len;
	if (g_buf_len + str_len > LOG_BUF_SIZE) {
		copy_len = LOG_BUF_SIZE-g_buf_len;
	} else {
		copy_len = str_len;
	}
	memcpy(gLogBuf+g_buf_len, logStr, copy_len); //copy chars up to LOG_BUF_SIZE
	g_buf_len += copy_len;
	if (g_buf_len > LOG_BUF_SIZE-1) {
		log_to_file();
		g_buf_len = str_len - copy_len;
		if (g_buf_len) { // copy the rest chars
			memcpy(gLogBuf, logStr+copy_len, g_buf_len);
		}
		if (gPosFp > MAX_LOG_FILE_LEN-1) {
			log_to_file();
			vfs_fsync(gLogfp, gLogfp->f_path.dentry, 1); //perform a fdatasync operation
			filp_close(gLogfp, current->files);
			new_log_file();
		}
	}
}
#endif

#define GLOBAL_DATA_BUF_SIZE (SECOTR_PART_SIZE<<SHIFT_SECTOR_PER_VPP)
unsigned long gpram_databuf;
static dma_addr_t databuf_dma_handle;
extern dma_addr_t nand_dma_handle; // store dma_handle of the current dma buf
extern unsigned char FTL_LowFormat_f(void);
//extern void main_test_FTL(void);
extern void DRIVER_test(void);
extern void main_test_FTL_time(void);
extern void main_test_Driver_time(void);
extern int SEP_Nand_Init_f(void);
extern void SEP_Nand_Release_f(void);
extern void memcpy_by_dma(unsigned int nDestAddr, unsigned int nSourceAddr, UINT16 dsize);

#define RUN_WITH_MTD 0
#if RUN_WITH_MTD
int nand_get_device(struct nand_chip *chip, struct mtd_info *mtd, int new_state);
void nand_release_device(struct mtd_info *mtd);
extern struct mtd_info *sep_mtd;
#else
DECLARE_COMPLETION_ONSTACK(nfc_doing); 
#define nand_get_device(x, y, z) do {wait_for_completion(&nfc_doing); INIT_COMPLETION(nfc_doing);} while(0)
#define nand_release_device(x) do {complete(&nfc_doing);} while(0)
#endif

#if CALC_READ_IN_WRITE
static unsigned int g_readreq_times = 0, g_writereq_times = 0;
#endif

typedef struct {
	u_char heads;
	u_short cylinders;
	u_char sectors;
	int unit;
} FTL_INFO;
static FTL_INFO ftl_info[CONFIG_BLK_DEV_FTL_COUNT];

//#if NO_SCHEDULER
typedef struct {
	struct list_head list;
	struct bio *bio;
	struct storage_direct* sdArg;
} FTL_FLASH_REQ;
//#endif
#define THRESH_REQ_NUM 4

DECLARE_COMPLETION_ONSTACK(write_doing);

#define MAX_PROCESS_NUM 12
#define LOW_PROCESS_NUM 6
#define DIRECT_BUF_LEN 16384
DECLARE_COMPLETION_ONSTACK(write_limited);

static struct storage_direct g_storage_d[MAX_PROCESS_NUM];
static unsigned int g_storage_idx = MAX_PROCESS_NUM-1;

/* Various static variables go here.  Most are used only in the FTL disk code.
 */
typedef struct {
	unsigned int g_major;
	struct gendisk *disk;
	struct request_queue *queue;
	spinlock_t queue_lock;

//#if NO_SCHEDULER
	struct list_head flash_req_head;
	spinlock_t frh_lock;
	struct task_struct *thread_wait_frh;
	volatile unsigned int flashReqNum;
	struct task_struct *thread_write_wait;
	struct task_struct *thread_read_wait;
//#endif

	volatile unsigned int needMoreReqs;
	struct task_struct *thread_req_proc;
	struct block_device *bdev;
} FTL_PRIVATE;
static FTL_PRIVATE g_ftl_data;


#if TIME_CALC
#define TIMES_CNT 0x40000
unsigned int* g_times = NULL;
unsigned int g_time_idx = 0;
static unsigned long sumusecFTLopen=0, sumusecFTLwrite=0, sumusecFTLread=0, sumusecReq=0, sumusecWaitReq=0;
unsigned int g_lastop;
extern void printmodes(void);
#endif
static char* format;
static char* test;
/*
 * Parameters for the boot-loading of the FTL disk.  These are set by
 * init/main.c (from arguments to the kernel command line) or from the
 * architecture-specific setup routine (from the stored boot sector
 * information).
 */
int ftl_size = CONFIG_BLK_DEV_FTL_SIZE;		/* Size of the FTL disks */
/*
 * It would be very desirable to have a soft-blocksize (that in the case
 * of the ftldisk driver is also the hardblocksize ;) of PAGE_SIZE because
 * doing that we'll achieve a far better MM footprint. Using a ftl_blocksize of
 * BLOCK_SIZE in the worst case we'll make PAGE_SIZE/BLOCK_SIZE buffer-pages
 * unfreeable. With a ftl_blocksize of PAGE_SIZE instead we are sure that only
 * 1 page will be protected. Depending on the size of the ftldisk you
 * may want to change the ftldisk blocksize to achieve a better or worse MM
 * behaviour. The default is still BLOCK_SIZE (needed by ftl_load_image that
 * supposes the filesystem in the image uses a BLOCK_SIZE blocksize).
 */
static int ftl_blocksize = CONFIG_BLK_DEV_FTL_BLOCK_SIZE;		/* blocksize of the FTL disks */

static int ftl_readpage(struct file *file, struct page *page)
{
	printk("ftl_readpage, page index:0x%lx\n ", page->index);

	return 0;
}


static const struct address_space_operations ftl_aops = {
	.readpage	=ftl_readpage,
//	.prepare_write	= ftl_prepare_write,
//	.commit_write	= ftl_commit_write,
//	.writepage	= ftl_writepage,
//	.set_page_dirty	= ftl_set_page_dirty,
//	.writepages	= ftl_writepages,
};

inline void wakeup_write_thd(FTL_PRIVATE* priv) {
	if (priv->thread_write_wait) {
		wake_up_process(priv->thread_write_wait);
		priv->thread_write_wait = NULL;
	}
}

inline void wakeup_read_thd(FTL_PRIVATE* priv) {
	if (priv->thread_read_wait) {
		wake_up_process(priv->thread_read_wait);
		priv->thread_read_wait = NULL;
	}
}


inline void wakeup_lock_thd(FTL_PRIVATE* priv) {
	if (priv->thread_wait_frh) {
		wake_up_process(priv->thread_wait_frh);
		priv->thread_wait_frh = NULL;
	}
}

inline struct storage_direct* copyFsgData(unsigned long arg) {
	struct storage_direct* result = NULL;
	struct storage_direct* sdArg = (struct storage_direct*)arg;
//	void *buf;
//	
//	buf = kmalloc(sdArg->count+sizeof(struct storage_direct), GFP_KERNEL);
//	if (!buf) return NULL;
//	result = (struct storage_direct*)(buf+sdArg->count);
//	result->buf = buf;
	g_storage_idx++;
	g_storage_idx %= MAX_PROCESS_NUM;
	result = &g_storage_d[g_storage_idx];

	memcpy(result->buf, sdArg->buf, sdArg->count);
	result->count = sdArg->count;
	result->pos = sdArg->pos;
	return result;
}

inline void deleteFsgData(struct storage_direct* sdArg) {
	kfree(sdArg->buf);
//	kfree(sdArg);
}

inline int fsg_read_sectors(struct storage_direct* sdArg) {
	unsigned int i, lba, sect_num, read_num, read_size;
	unsigned long src, dst;
	int err=0;
	struct timeval tv01, tv02;
	
	sect_num = sdArg->count>>9;
	lba = sdArg->pos >> 9;
	LOG_TMP("[fsg_readsects] begin sector: %u, size:%u, sect_num: %u. buf:0x%lx, dma:0x%lx \n", lba, sdArg->count, sect_num, sdArg->buf,sdArg->dma);
#if READ_USE_USB_BUF
	FTL_WriteClose_f();
#else
#if BUF_TRANSFER_BY_DMA
	//dst = (unsigned long)sdArg->dma;
	dst = sdArg->buf;
#else
	dst = sdArg->buf;
#endif
#endif
	sdArg->actual = 0;
	while (sect_num) {
#if TIME_CALC
		do_gettimeofday(&tv01);
		g_times[g_time_idx++] = 0x2001;
		g_times[g_time_idx++] = tv01.tv_sec * 1000000 + tv01.tv_usec;
#endif
#if READ_USE_USB_BUF
		gFTLSysInfo.userDataBufPtr = (unsigned int *)((unsigned long)sdArg->buf + sdArg->actual - (lba % FTL_PARTS_PER_VPP) * SECOTR_PART_SIZE);
		nand_dma_handle = (dma_addr_t)sdArg->dma + sdArg->actual - (lba % FTL_PARTS_PER_VPP) * SECOTR_PART_SIZE;
#endif
		read_num = FTL_ReadSector_f(lba, sect_num);
#if TIME_CALC
		do_gettimeofday(&tv02);
		g_times[g_time_idx++] = 0x2002;
		g_times[g_time_idx++] = tv02.tv_sec * 1000000 + tv02.tv_usec;
		sumusecFTLread += (tv02.tv_sec - tv01.tv_sec) * 1000000 + tv02.tv_usec - tv01.tv_usec;
		g_times[g_time_idx++] = 0x2009;
		g_times[g_time_idx++] = sumusecFTLread;
#endif
		read_size = read_num << 9;
#if !READ_USE_USB_BUF
#if BUF_TRANSFER_BY_DMA
		//src = (databuf_dma_handle + (lba % FTL_PARTS_PER_VPP) * SECOTR_PART_SIZE);
		//memcpy_by_dma(dst, src, read_size);
		src = (gpram_databuf + (lba % FTL_PARTS_PER_VPP) * SECOTR_PART_SIZE);
		memcpy(dst, src, read_size);
#else
		src = (gpram_databuf + (lba % FTL_PARTS_PER_VPP) * SECOTR_PART_SIZE);
		memcpy(dst, src, read_size);
#endif
		dst += read_size;
#endif
		sdArg->actual += read_size;
		lba += read_num;
		sect_num -= read_num;
	}
	pr_debug("[fsg_readsects] end err: %d, actualsize: %u \n", err, sdArg->actual);
#if READ_USE_USB_BUF
	gFTLSysInfo.userDataBufPtr = (UINT32 *)gpram_databuf;
	nand_dma_handle = databuf_dma_handle;
#endif

	return err;
}

inline int fsg_write_sectors(struct storage_direct* sdArg) {
	unsigned int i, lba, sect_num, write_num, start_idx;
	unsigned long copyed_size;
	unsigned long src, dst;
	unsigned int * tmp = (unsigned int *)sdArg->buf;
	int err=0, result;
	struct timeval tv01, tv02;

	sect_num = sdArg->count>>9;
	lba = sdArg->pos >> 9;
#if BUF_TRANSFER_BY_DMA
	//src = (unsigned long)sdArg->dma;
	src = (unsigned long)sdArg->buf;
#else
	src = (unsigned long)sdArg->buf;
#endif
	LOG_TMP("[fsg_writesects] begin sector:%u,size:%u,sect_num:%u,flashReqNum:%u, buf:0x%lx, dma:0x%lx\n", lba, sdArg->count, sect_num, g_ftl_data.flashReqNum, sdArg->buf,sdArg->dma);
	err = FTL_WriteOpen_f(lba, sect_num);
	pr_debug("[fsg_writesects] FTL_WriteOpen_f result: %d \n", ret);
#if TIME_CALC
	do_gettimeofday(&tv01);
	g_times[g_time_idx++] = 0x2021;
	g_times[g_time_idx++] = tv01.tv_sec * 1000000 + tv01.tv_usec;
#endif

	copyed_size = 0;
	while (sect_num) {
		start_idx = (lba % FTL_PARTS_PER_VPP);
		if (start_idx + sect_num < FTL_PARTS_PER_VPP)
			write_num = sect_num;
		else write_num = FTL_PARTS_PER_VPP - start_idx;
		if (unlikely((start_idx + sect_num < FTL_PARTS_PER_VPP) ||  //not enough for a vpp to write
			(start_idx > 0 && (gFTLSysInfo.currentOper.lastOP == 1 && 
			gFTLSysInfo.currentOper.lastLBA != 0xFFFFFFFE && 
			(gFTLSysInfo.currentOper.lastLBA % FTL_PARTS_PER_VPP) != (FTL_PARTS_PER_VPP-1) )))) { //already has a data in the buf
#if BUF_TRANSFER_BY_DMA
			//dst = (databuf_dma_handle + start_idx * SECOTR_PART_SIZE);
			//memcpy_by_dma(dst, src, copy_size);
			dst = (gpram_databuf + start_idx * SECOTR_PART_SIZE);
			memcpy(dst, src, write_num << 9);
#else
			dst = (gpram_databuf + start_idx * SECOTR_PART_SIZE);
			memcpy(dst, src, write_num << 9);
#endif
		} else {// directly use usb buf
			gFTLSysInfo.userDataBufPtr = (unsigned int *)((unsigned long)sdArg->buf + copyed_size - start_idx * SECOTR_PART_SIZE);
			nand_dma_handle = (dma_addr_t)sdArg->dma + copyed_size - start_idx * SECOTR_PART_SIZE;

//	} else {
//		result = FTL_WriteSector_f(lba + 31 - start_idx);
//		err |= result;
//		dst = databuf_dma_handle;
//		src += copy_size;
//		tmp += copy_size>>2;
//		start_idx = 0;
//		copy_size = sdArg->count - copy_size;
//		copy_size = sdArg->count;
		}
		result = FTL_WriteSector_f(lba+write_num-1);
		err |= result;
		copyed_size += write_num << 9;
		sect_num -= write_num;
		lba += write_num;
		gFTLSysInfo.userDataBufPtr = (UINT32 *)gpram_databuf;
		nand_dma_handle = databuf_dma_handle;
	}

#if TIME_CALC
		do_gettimeofday(&tv02);
		g_times[g_time_idx++] = 0x2022;
		g_times[g_time_idx++] = tv02.tv_sec * 1000000 + tv02.tv_usec;
		sumusecFTLwrite += (tv02.tv_sec - tv01.tv_sec) * 1000000 + tv02.tv_usec - tv01.tv_usec;
		g_times[g_time_idx++] = 0x2029;
		g_times[g_time_idx++] = sumusecFTLwrite;
#endif
	NAND_MSG("[fsg_writesects] end err: %d\n", err);
	return err;
}

//inline int fsg_write_sectors(struct storage_direct* sdArg) {
//	unsigned int i, lba, sect_num, start_idx, copy_size;
//	unsigned long src, dst;
//	unsigned int * tmp = (unsigned int *)sdArg->buf;
//	int err=0, result;
//	struct timeval tv01, tv02;
//
//	sect_num = sdArg->count>>9;
//	lba = sdArg->pos >> 9;
//	src = (unsigned long)sdArg->dma;
//	NAND_MSG("[fsg_writesects] begin sector:%u,size:%u,sect_num:%u,flashReqNum:%u\n", lba, sdArg->count, sect_num, g_ftl_data.flashReqNum);
//	err = FTL_WriteOpen_f(lba, sect_num);
//	pr_debug("[fsg_writesects] FTL_WriteOpen_f result: %d \n", ret);
//	start_idx = (lba % FTL_PARTS_PER_VPP);
//	dst = (databuf_dma_handle + start_idx * SECOTR_PART_SIZE);
//	if (start_idx + sect_num > 32) {
//		copy_size = (32 - start_idx)<<9;
//		memcpy_by_dma(dst, src, copy_size);
//#if TIME_CALC
//		do_gettimeofday(&tv01);
//		g_times[g_time_idx++] = 0x2021;
//		g_times[g_time_idx++] = tv01.tv_sec * 1000000 + tv01.tv_usec;
//#endif
//		result = FTL_WriteSector_f(lba + 31 - start_idx);
//		err |= result;
//#if TIME_CALC
//		do_gettimeofday(&tv02);
//		g_times[g_time_idx++] = 0x2022;
//		g_times[g_time_idx++] = tv02.tv_sec * 1000000 + tv02.tv_usec;
//		sumusecFTLwrite += (tv02.tv_sec - tv01.tv_sec) * 1000000 + tv02.tv_usec - tv01.tv_usec;
//		g_times[g_time_idx++] = 0x2029;
//		g_times[g_time_idx++] = sumusecFTLwrite;
//#endif
//		dst = databuf_dma_handle;
//		src += copy_size;
//		tmp += copy_size>>2;
//		start_idx = 0;
//		copy_size = sdArg->count - copy_size;
//	} else {
//		copy_size = sdArg->count;
//	}
//	memcpy_by_dma(dst, src, copy_size);
//	result = FTL_WriteSector_f(lba+sect_num-1);
//	err |= result;
////	{
////		int j;
////		unsigned int* data = (unsigned int*)(gpram_databuf + start_idx * SECOTR_PART_SIZE);
////		printk("######################################################\n");
////		printk("ORI DATA:");
////		for (j=0;j<128;j++) {
////			printk("%u:0x%x,", j, tmp[j]);
////		}
////		printk("\n");
////		printk("COPYED DATA:");
////		for (j=0;j<128;j++) {
////			printk("%u:0x%x,", j, data[j]);
////		}
////		printk("\n");
////	}
////
//	NAND_MSG("[fsg_writesects] end err: %d\n", err);
//	return err;
//}
//

inline int ftl_blkdev_readsects(struct bio_vec *vec, sector_t sector, unsigned char* pflag)
{
	sector_t index = sector;
	unsigned int sect_num = ((vec->bv_len+511) >> 9);
	unsigned int left_size = vec->bv_len;
	int err = 0;
	char* page_addr;
	char* dst;
	char* page_buf;
	struct timeval tv01, tv02;

	NAND_MSG("[ftldev_readsects] begin sector: %lu, size: %u, flag:%u \n", sector, left_size, *pflag);

	page_addr = kmap(vec->bv_page);
	dst = page_addr + vec->bv_offset;

//	nand_get_device(sep_mtd->priv, sep_mtd, FL_READING);
	do {
		int count;
		char *src;
		unsigned int read_num;

		pr_debug("[ftldev_readsects] FTL_ReadSector_f index: %lu ", index);
#if TIME_CALC
		do_gettimeofday(&tv01);
		g_times[g_time_idx++] = 0x2001;
		g_times[g_time_idx++] = tv01.tv_sec * 1000000 + tv01.tv_usec;
#endif
		read_num = FTL_ReadSector_f(index, sect_num);
#if TIME_CALC
		do_gettimeofday(&tv02);
		g_times[g_time_idx++] = 0x2002;
		g_times[g_time_idx++] = tv02.tv_sec * 1000000 + tv02.tv_usec;
		sumusecFTLread += (tv02.tv_sec - tv01.tv_sec) * 1000000 + tv02.tv_usec - tv01.tv_usec;
		g_times[g_time_idx++] = 0x2009;
		g_times[g_time_idx++] = sumusecFTLread;
#endif
		
		pr_debug(", read_num: %d \n", read_num);
//		if (!read_num) {
//			err = FLASH_TOO_MANY_ERROR;
//			goto out;
//		}
//		*pflag = 1;
		count = SECOTR_PART_SIZE * read_num;
		if (count > left_size) {
			count = left_size;
		}
		left_size -= count;

		src = ((char *)gpram_databuf) + (index % FTL_PARTS_PER_VPP) * SECOTR_PART_SIZE;
		pr_debug("[ftldev_readsects] index:%lu, src: 0x%lx, count:%d \n", index, src, count);
		if (index == sector) NAND_MSG("after [ReadSector]: sector:%lu, data:0x%lx,%lx,%lx,%lx \n", index, *(unsigned int*)src, *(unsigned int*)(src+4), *(unsigned int*)(src+8), *(unsigned int*)(src+12));
		memcpy(dst, src, count);
		dst += count;
		index += read_num;
		sect_num -= read_num;
	} while (left_size);
//	nand_release_device(sep_mtd);

 out:
	kunmap(vec->bv_page);
	pr_debug("[ftl_blkdev_readsects] end err: %d \n", err);
	return err;
}

inline int ftl_blkdev_writesects(struct bio_vec *vec, sector_t sector, unsigned short* pRestNum)
{
	sector_t index = sector;
	unsigned int size = vec->bv_len;
	int err = 0;
	char* page_addr;
	char* src;
	char* page_buf;
	struct timeval tv01, tv02;
	NAND_MSG("[ftldev_writesects] begin sector: %lu, size: %u, restSectors:%u \n", sector, size, *pRestNum);

	page_addr = kmap(vec->bv_page);
	src = page_addr + vec->bv_offset;

//	nand_get_device(sep_mtd->priv, sep_mtd, FL_WRITING);
	do {
		int count;
		char *dst;
		unsigned char result;

		dst = ((char *)gpram_databuf) + (index % FTL_PARTS_PER_VPP) * SECOTR_PART_SIZE;
		count = SECOTR_PART_SIZE;
		if (count > size) {
			memset(dst + size, 0xFF, count - size);
			count = size;
		}
		memcpy(dst, src, count);
		src += count;
		size -= count;
//		if (index == 0) {
//			int j;
//			unsigned int* data = (unsigned int*)gpram_databuf;
//			printk("LBA0 DATA:");
//			for (j=0;j<128;j++) {
//				printk("%u:0x%x,", j, data[j]);
//			}
//			printk("\n");
//		}

		pr_debug("[ftldev_writesects] FTL_WriteSector_f index: %lu ", index);
#if TIME_CALC
		do_gettimeofday(&tv01);
		g_times[g_time_idx++] = 0x2021;
		g_times[g_time_idx++] = tv01.tv_sec * 1000000 + tv01.tv_usec;
#endif
		if (index == sector) pr_debug("before [WriteSector]: sector:%lu, data:0x%lx,%lx,%lx,%lx \n", index, *(unsigned int*)dst, *(unsigned int*)(dst+4), *(unsigned int*)(dst+8), *(unsigned int*)(dst+12));
		result = FTL_WriteSector_f(index);
#if TIME_CALC
		do_gettimeofday(&tv02);
		g_times[g_time_idx++] = 0x2022;
		g_times[g_time_idx++] = tv02.tv_sec * 1000000 + tv02.tv_usec;
		sumusecFTLwrite += (tv02.tv_sec - tv01.tv_sec) * 1000000 + tv02.tv_usec - tv01.tv_usec;
		g_times[g_time_idx++] = 0x2029;
		g_times[g_time_idx++] = sumusecFTLwrite;
#endif
		pr_debug(", result: %d \n", result);
		(*pRestNum)--;
//		if (result) {
//			err = result;
//			goto out;
//		}
		index++;

	} while (size);
//	nand_release_device(sep_mtd);

 out:
	kunmap(vec->bv_page);
 	
	pr_debug("[ftldev_writesects] end err: %d \n", err);
	return err;
}

/*
 * Post finished request.
 */
static void ftl_end_request(struct request *req, int error)
{
	if (blk_end_request(req, error, blk_rq_bytes(req)))
		BUG();
}

inline int ftl_add_request(FTL_PRIVATE* priv, struct bio *bio)
{
	FTL_FLASH_REQ* flashReq = NULL;
	struct timeval tv01;
#if TIME_CALC
	do_gettimeofday(&tv01);
	g_times[g_time_idx++] = 0x1011;
	g_times[g_time_idx++] = tv01.tv_sec * 1000000 + tv01.tv_usec;
#endif
	pr_debug("[add_request] begin \n");
	flashReq = kmalloc(sizeof(FTL_FLASH_REQ), GFP_KERNEL);
	if (!flashReq) return -ENOMEM;
	flashReq->bio = bio;
	flashReq->sdArg = NULL;

	while (!spin_trylock_irq(priv->frh_lock)) {
		printk("[add_request] lock frh failed. \n");
		priv->thread_wait_frh = current;
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule();
	}
	//spin_lock_irq(priv->frh_lock);
	list_add_tail(&flashReq->list, &priv->flash_req_head);
	priv->flashReqNum++;
	spin_unlock_irq(priv->frh_lock);
	pr_debug("[add_request] after release lock \n");

	if (priv->needMoreReqs) {
		priv->needMoreReqs = FALSE;
		wake_up_process(priv->thread_req_proc);
	}
	
	return 0;
}

inline int ftl_add_ioctl_request(FTL_PRIVATE* priv, unsigned int rw, unsigned long arg)
{
	FTL_FLASH_REQ* flashReq = NULL;
	struct storage_direct* sdArg;
	struct timeval tv01;
#if TIME_CALC
	do_gettimeofday(&tv01);
	g_times[g_time_idx++] = 0x1011;
	g_times[g_time_idx++] = tv01.tv_sec * 1000000 + tv01.tv_usec;
#endif
	pr_debug("[add_ioctl_req] begin rw: 0x%lx \n", rw);
	flashReq = kmalloc(sizeof(FTL_FLASH_REQ), GFP_KERNEL);
	if (!flashReq) return -ENOMEM;
	flashReq->bio = NULL;
	if (rw == WRITE) {
		flashReq->sdArg = copyFsgData(arg);
		sdArg = (struct storage_direct*)arg;
		sdArg->actual = sdArg->count;
	} else {
		flashReq->sdArg = (struct storage_direct*)arg;
	}
	flashReq->sdArg->user_space = rw;

	while (!spin_trylock_irq(priv->frh_lock)) {
		printk("[add_ioctl_req] lock frh failed. \n");
		priv->thread_wait_frh = current;
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule();
	}
	//spin_lock_irq(priv->frh_lock);
	list_add_tail(&flashReq->list, &priv->flash_req_head);
	priv->flashReqNum++;
	spin_unlock_irq(priv->frh_lock);
	pr_debug("[add_ioctl_req] after release lock \n");

	if (priv->needMoreReqs) {
		priv->needMoreReqs = FALSE;
		wake_up_process(priv->thread_req_proc);
	}
	
	return 0;
}

inline void flush_bdev(void) {
//	FTL_PRIVATE* priv = &g_ftl_data;
//	if (priv->flashReqNum < THRESH_REQ_NUM) {
//		pr_debug("[flush_bdev] start flashReqNum:%u \n", priv->flashReqNum);
//		sync_blockdev(priv->bdev);
//	}
}

#if NO_SCHEDULER
	
#if USE_THREAD
static int do_ftl_request(struct bio *bio)
{
	struct block_device *bdev = bio->bi_bdev;
	sector_t sector = bio->bi_sector;
	unsigned short len = bio_sectors(bio);
	int rw = bio_data_dir(bio);
	struct bio_vec *bvec;
	int ret = 0, i;
	unsigned char flag = 0;

	pr_debug("[do_ftl_request] begin sector: %lu, len: %u \n", sector, len);

//	if (sector + len > get_capacity(bdev->bd_disk)) {
//		printk("[do_ftl_request] capacity overflowed \n");
//		goto fail;
//	}

	if (rw==READA)
		rw=READ;

	if (rw == WRITE) {
		ret = FTL_WriteOpen_f(sector, len);
		pr_debug("[do_ftl_request] FTL_WriteOpen_f result: %d \n", ret);
		//if (ret) goto fail;
	}

	bio_for_each_segment(bvec, bio, i) {
		if (rw == READ) {
			ret |= ftl_blkdev_readsects(bvec, sector, &flag);
		} else {
			ret |= ftl_blkdev_writesects(bvec, sector, (unsigned short *)&len);
		}
		sector += bvec->bv_len >> 9;
	}
//	if (ret)
//		goto fail;

	bio_endio(bio, 0);
	pr_debug("[do_ftl_request] end success \n");
	return 0;
fail:
	bio_io_error(bio);
	pr_debug("[do_ftl_request] end error \n");
	return 0;
} 

static int ftl_thread_req_proc(void *arg)
{
	struct timeval tv01, tv02;
	FTL_PRIVATE* priv = (FTL_PRIVATE *)arg;
	// we might get involved when memory gets low, so use PF_MEMALLOC
	current->flags |= PF_MEMALLOC;
	
	while (!kthread_should_stop()) {
		FTL_FLASH_REQ* flashReq;
		struct list_head *frh = &priv->flash_req_head;
		int res = 0;

		if (list_empty(frh)) {
			NAND_MSG("[thread_req_proc] before schedule when no reqs. \n");
			set_current_state(TASK_UNINTERRUPTIBLE);
			priv->needMoreReqs = TRUE;
			wakeup_read_thd(priv);
//			complete(&write_doing);
#if TIME_CALC
			do_gettimeofday(&tv01);
			g_times[g_time_idx++] = 0x1022;
			g_times[g_time_idx++] = tv01.tv_sec * 1000000 + tv01.tv_usec;
			sumusecReq += (tv01.tv_sec - tv02.tv_sec) * 1000000 + tv01.tv_usec - tv02.tv_usec;
			g_times[g_time_idx++] = 0x1029;
			g_times[g_time_idx++] = sumusecReq;
#endif
			schedule();
#if TIME_CALC
			do_gettimeofday(&tv02);
			g_times[g_time_idx++] = 0x1021;
			g_times[g_time_idx++] = tv02.tv_sec * 1000000 + tv02.tv_usec;
			sumusecWaitReq += (tv02.tv_sec - tv01.tv_sec) * 1000000 + tv02.tv_usec - tv01.tv_usec;
			g_times[g_time_idx++] = 0x102A;
			g_times[g_time_idx++] = sumusecWaitReq;
#endif
			continue;
		}

		pr_debug("[thread_req_proc] before get lock \n");
		while (!spin_trylock_irq(priv->frh_lock)) {
			printk("[thread_req_proc] lock frh failed. \n");
			priv->needMoreReqs = TRUE;
			set_current_state(TASK_UNINTERRUPTIBLE);
			schedule();
		}
//		spin_lock_irq(priv->frh_lock);
		flashReq = list_first_entry(frh, FTL_FLASH_REQ, list);
		list_del_init(&flashReq->list);
		priv->flashReqNum--;
		spin_unlock_irq(priv->frh_lock);
		pr_debug("[thread_req_proc] after release lock \n");
		wakeup_lock_thd(priv);
		if (priv->flashReqNum < LOW_PROCESS_NUM) {
			wakeup_write_thd(priv);
//			complete(&write_limited);
		}

		if (flashReq->sdArg){
			if (flashReq->sdArg->user_space == WRITE) 
				res = fsg_write_sectors(flashReq->sdArg);
			else res = fsg_read_sectors(flashReq->sdArg);
		} 
		else res = do_ftl_request(flashReq->bio);
		
		kfree(flashReq);
	}

	return 0;
}

static int ftl_make_request(struct request_queue *q, struct bio *bio)
{
	FTL_PRIVATE* priv = (FTL_PRIVATE *)q->queuedata;
	return ftl_add_request(priv, bio);
}

#else // !USE_THREAD
/*
 *  Basically, my strategy here is to set up a buffer-head which can't be
 *  deleted, and make that my FTLdisk.  If the request is outside of the
 *  allocated size, we must get rid of it...
 *
 * 19-JAN-1998  Richard Gooch <rgooch@atnf.csiro.au>  Added devfs support
 *
 */
static int ftl_make_request(struct request_queue *q, struct bio *bio)
{
	struct block_device *bdev = bio->bi_bdev;
	sector_t sector = bio->bi_sector;
	unsigned int len = bio_sectors(bio);
	int rw = bio_data_dir(bio);
	struct bio_vec *bvec;
	int ret = 0, i;
	unsigned char flag = 0;

	nand_get_device(sep_mtd->priv, sep_mtd, FL_READING);
	NAND_MSG("[ftl_make_request] begin sector: %lu, len: %u \n", sector, len);

	if (sector + len > get_capacity(bdev->bd_disk)) {
		printk("[ftl_make_request] capacity overflowed \n");
		goto fail;
	}

	if (rw==READA)
		rw=READ;

	if (rw == WRITE) {
//		nand_get_device(sep_mtd->priv, sep_mtd, FL_READING);
		ret = FTL_WriteOpen_f(sector, len);
//		nand_release_device(sep_mtd);
		pr_debug("[ftl_make_request] FTL_WriteOpen_f result: %d \n", ret);
		//if (ret) goto fail;
	}

	bio_for_each_segment(bvec, bio, i) {
		if (rw == READ) {
			ret |= ftl_blkdev_readsects(bvec, sector, &flag);
		} else {
			ret |= ftl_blkdev_writesects(bvec, sector, (unsigned short *)&len);
		}
		sector += bvec->bv_len >> 9;
	}
	if (ret)
		goto fail;

	bio_endio(bio, 0);
	nand_release_device(sep_mtd);
	pr_debug("[ftl_make_request] end success \n");
	return 0;
fail:
	bio_io_error(bio);
	nand_release_device(sep_mtd);
	pr_debug("[ftl_make_request] end error \n");
	return 0;
} 
#endif // USE_THREAD

#else // !NO_SCHEDULER

inline void flush_bdev(void) {
}

#if USE_THREAD
static int do_ftl_request(struct request *req)
{
	sector_t sector, total_sectors;
	int rw;
	unsigned short len;
	struct bio *bio;
	struct bio_vec *bvec;
	int ret = 0, res, i;
	unsigned char flag = 0;
	struct timeval tv01, tv02;

	rw = rq_data_dir(req);
	if (rw==READA) rw=READ;
//	total_sectors = get_capacity(req->rq_disk);
	NAND_MSG("[do_ftl_request] begin rw:%d \n", rw);

	__rq_for_each_bio(bio, req) {
		sector = bio->bi_sector;
		len = bio_sectors(bio);
		pr_debug("[do_ftl_request bio] bi_sector: 0x%lx, len:%u\n", sector, len);
//		if (sector + len > total_sectors) {
//			ret |= -EIO;
//			printk("[do_ftl_request] capacity overflowed \n");
//			continue;
//		}
		if (rw == READ) {
//			if (sector % 8 != 0) printk("[do_ftl_request] sector:0x%lx, len:%u in read \n", sector, len);
#if CALC_READ_IN_WRITE
			g_readreq_times++;
			printk("[Read Req]: read req times:%u, sector:%lu, len:%u \n", g_readreq_times, sector, len);
#endif
			flag = 0;
			bio_for_each_segment(bvec, bio, i) {
				ret |= ftl_blkdev_readsects(bvec, sector, &flag);
				sector += bvec->bv_len >> 9;
			}
		} else {
	//		nand_get_device(sep_mtd->priv, sep_mtd, FL_READING);
//			if (sector % 16 != 0) printk("[do_ftl_request] sector:0x%lx, len:%u in write \n", sector, len);
#if CALC_READ_IN_WRITE
			g_writereq_times++;
			printk("[Write Req]: write req times:%u, sector:%lu, len:%u \n", g_writereq_times, sector, len);
#endif
			pr_debug("before [WriteOpen]: sector:%lu, len:%u \n", sector, len);
#if TIME_CALC
			do_gettimeofday(&tv01);
			g_times[g_time_idx++] = 0x2011;
			g_times[g_time_idx++] = tv01.tv_sec * 1000000 + tv01.tv_usec;
#endif
			res = FTL_WriteOpen_f(sector);
#if TIME_CALC
			do_gettimeofday(&tv02);
			g_times[g_time_idx++] = 0x2012;
			g_times[g_time_idx++] = tv02.tv_sec * 1000000 + tv02.tv_usec;
			sumusecFTLopen += (tv02.tv_sec - tv01.tv_sec) * 1000000 + tv02.tv_usec - tv01.tv_usec;
			g_times[g_time_idx++] = 0x2019;
			g_times[g_time_idx++] = sumusecFTLopen;
#endif
	//		nand_release_device(sep_mtd);
//			ret |= res;
			pr_debug("[do_ftl_request] FTL_WriteOpen_f result: %d \n", res);
			//if (res) continue;

			bio_for_each_segment(bvec, bio, i) {
				ret |= ftl_blkdev_writesects(bvec, sector, &len);
				sector += bvec->bv_len >> 9;
			}
		}
	}

	pr_debug("[do_ftl_request] end  ret:%d\n", ret);
	return ret;
} 

static int ftl_thread_req_proc(void *arg)
{
	struct timeval tv01, tv02;
	FTL_PRIVATE* priv = (FTL_PRIVATE *)arg;
	struct request_queue *rq = priv->queue;
	// we might get involved when memory gets low, so use PF_MEMALLOC
	current->flags |= PF_MEMALLOC;
	
	spin_lock_irq(rq->queue_lock);
	while (!kthread_should_stop()) {
		struct request *req;
		int res = 0;

		req = elv_next_request(rq);

		if (!req) {
			set_current_state(TASK_UNINTERRUPTIBLE);
			spin_unlock_irq(rq->queue_lock);
			priv->needMoreReqs = TRUE;
#if TIME_CALC
			do_gettimeofday(&tv01);
			g_times[g_time_idx++] = 0x1022;
			g_times[g_time_idx++] = tv01.tv_sec * 1000000 + tv01.tv_usec;
			sumusecReq += (tv01.tv_sec - tv02.tv_sec) * 1000000 + tv01.tv_usec - tv02.tv_usec;
			g_times[g_time_idx++] = 0x1029;
			g_times[g_time_idx++] = sumusecReq;
#endif
			schedule();
#if TIME_CALC
			do_gettimeofday(&tv02);
			g_times[g_time_idx++] = 0x1021;
			g_times[g_time_idx++] = tv02.tv_sec * 1000000 + tv02.tv_usec;
			sumusecWaitReq += (tv02.tv_sec - tv01.tv_sec) * 1000000 + tv02.tv_usec - tv01.tv_usec;
			g_times[g_time_idx++] = 0x102A;
			g_times[g_time_idx++] = sumusecWaitReq;
#endif
			spin_lock_irq(rq->queue_lock);
			continue;
		}

		blkdev_dequeue_request(req);
		spin_unlock_irq(rq->queue_lock);

		res = do_ftl_request(req);

		spin_lock_irq(rq->queue_lock);

		ftl_end_request(req, res);
	}
	spin_unlock_irq(rq->queue_lock);

	return 0;
}

//for thread_req_proc (all requests processed by a thread)
static void ftl_request_proc(struct request_queue *rq)
{
	struct timeval tv01;
	FTL_PRIVATE* priv = (FTL_PRIVATE *)rq->queuedata;
#if TIME_CALC
	do_gettimeofday(&tv01);
	g_times[g_time_idx++] = 0x1011;
	g_times[g_time_idx++] = tv01.tv_sec * 1000000 + tv01.tv_usec;
#endif
	if (priv->needMoreReqs) {
		priv->needMoreReqs = FALSE;
		wake_up_process(priv->thread_req_proc);
	}
}
#else  // !USE_THREAD
/*
 *  Basically, my strategy here is to set up a buffer-head which can't be
 *  deleted, and make that my FTLdisk.  If the request is outside of the
 *  allocated size, we must get rid of it...
 *
 * 19-JAN-1998  Richard Gooch <rgooch@atnf.csiro.au>  Added devfs support
 *
 */
static void ftl_request_proc(struct request_queue *queue)
{
	sector_t sector, total_sectors;
	int rw;
	unsigned short len;
	struct request *req;
	struct bio *bio;
	struct bio_vec *bvec;
	int ret = 0, res, i;
	unsigned char flag = 0;
	struct tms tms;
	struct timeval tv01, tv02, tv03, tv04;

	nand_get_device(sep_mtd->priv, sep_mtd, FL_READING);
	NAND_MSG("[ftl_request_proc] begin \n");
	
#if TIME_CALC
////	do_sys_times(&tms);
//	g_times[g_time_idx++] = 0x1001;
////	g_times[g_time_idx++] = tms.tms_utime;
//	g_times[g_time_idx++] = current->utime;
//	g_times[g_time_idx++] = 0x1002;
////	g_times[g_time_idx++] = tms.tms_stime;
//	g_times[g_time_idx++] = current->stime;

	do_gettimeofday(&tv03);
	g_times[g_time_idx++] = 0x1021;
	g_times[g_time_idx++] = tv03.tv_sec * 1000000 + tv03.tv_usec;
#endif
	spin_lock_irq(queue->queue_lock);
	while ((req = elv_next_request(queue)) != NULL) {
		blkdev_dequeue_request(req);
		spin_unlock_irq(queue->queue_lock);
		rw = rq_data_dir(req);
		if (rw==READA) rw=READ;
//		total_sectors = get_capacity(req->rq_disk);
		NAND_MSG("[ftl_request_proc] begin rw:%d \n", rw);

		__rq_for_each_bio(bio, req) {
			sector = bio->bi_sector;
			len = bio_sectors(bio);
			pr_debug("[ftl_request_proc bio] bi_sector: 0x%lx, len:%u\n", sector, len);
//			if (sector + len > total_sectors) {
//				ret |= -EIO;
//				printk("[ftl_request_proc] capacity overflowed \n");
//				continue;
//			}
			if (rw == READ) {
#if CALC_READ_IN_WRITE
				g_readreq_times++;
				printk("[Read Req]: read req times:%u, sector:%lu, len:%u \n", g_readreq_times, sector, len);
#endif
				flag = 0;
				bio_for_each_segment(bvec, bio, i) {
					ret |= ftl_blkdev_readsects(bvec, sector, &flag);
					sector += bvec->bv_len >> 9;
				}
			} else {
		//		nand_get_device(sep_mtd->priv, sep_mtd, FL_READING);
#if CALC_READ_IN_WRITE
				g_writereq_times++;
				printk("[Write Req]: write req times:%u, sector:%lu, len:%u \n", g_writereq_times, sector, len);
#endif
#if TIME_CALC
				do_gettimeofday(&tv01);
				g_times[g_time_idx++] = 0x2011;
				g_times[g_time_idx++] = tv01.tv_sec * 1000000 + tv01.tv_usec;
#endif
				res = FTL_WriteOpen_f(sector);
#if TIME_CALC
				do_gettimeofday(&tv02);
				g_times[g_time_idx++] = 0x2012;
				g_times[g_time_idx++] = tv02.tv_sec * 1000000 + tv02.tv_usec;
				sumusecFTLopen += (tv02.tv_sec - tv01.tv_sec) * 1000000 + tv02.tv_usec - tv01.tv_usec;
				g_times[g_time_idx++] = 0x2019;
				g_times[g_time_idx++] = sumusecFTLopen;
#endif
		//		nand_release_device(sep_mtd);
//				ret |= res;
				pr_debug("[ftl_request_proc bio] FTL_WriteOpen_f result: %d \n", res);
				//if (res) continue;

				bio_for_each_segment(bvec, bio, i) {
					ret |= ftl_blkdev_writesects(bvec, sector, &len);
					sector += bvec->bv_len >> 9;
				}
			}
		}
		spin_lock_irq(queue->queue_lock);
		ftl_end_request(req, ret);
	}
	spin_unlock_irq(queue->queue_lock);

	nand_release_device(sep_mtd);

#if TIME_CALC
////	do_sys_times(&tms);
//	g_times[g_time_idx++] = 0x1003;
////	g_times[g_time_idx++] = tms.tms_utime;
//	g_times[g_time_idx++] = current->utime;
//	g_times[g_time_idx++] = 0x1004;
////	g_times[g_time_idx++] = tms.tms_stime;
//	g_times[g_time_idx++] = current->stime;

	do_gettimeofday(&tv04);
	g_times[g_time_idx++] = 0x1022;
	g_times[g_time_idx++] = tv04.tv_sec * 1000000 + tv04.tv_usec;
	sumusecReq += (tv04.tv_sec - tv03.tv_sec) * 1000000 + tv04.tv_usec - tv03.tv_usec;
	g_times[g_time_idx++] = 0x1029;
	g_times[g_time_idx++] = sumusecReq;
#endif
	pr_debug("[ftl_request_proc] end  ret:%d\n", ret);
} 
#endif // USE_THREAD

#endif // NO_SCHEDULER

static int ftl_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	FTL_INFO *p = bdev->bd_disk->private_data;

	geo->heads = p->heads;
	geo->sectors = p->sectors;
	geo->cylinders = p->cylinders;
	return 0;
}

static int ftl_release(struct gendisk * disk, fmode_t mode)
{
	pr_debug("[ftl_release] begin \n");
	//去除片选
	//FLASH_DisableChip_f(0);
//	//释放DMA
//	DMA_release();

//	bdev->bd_openers--;
	printk("[ftl_release] end \n");
	return 0;
}

#if TIME_CALC
void printtimes() {
	unsigned int i;
	unsigned int* data = g_times;
	printk("MAX TIME CNT %u, real time cnt: %u\n", TIMES_CNT, g_time_idx>>1);
	printk("NO_SCHEDULER:%d, USE_THREAD:%d\n", NO_SCHEDULER, USE_THREAD);
	printmodes();
	for (i = 0; i<((g_time_idx+1)>>1); i++) {
		printk("%x, %x, %u\n", data[0] >> 4, data[0] & 0xF, data[1]);
		data += 2;
	}
}
#endif

static int ftl_open(struct block_device *bdev, fmode_t mode)
{
	int err;
	struct address_space *mapping;
	unsigned bsize;
	
	pr_debug("[ftl_open] begin \n");
	g_ftl_data.bdev = bdev;
//	bdev->bd_openers++;
	
	pr_debug("[ftl_open] bd_inode:0x%lx, host:0x%lx, dentry:0x%lx, d_inode:0x%lx \n", bdev->bd_inode, bdev->bd_inode->i_mapping->host, bdev->bd_super ? bdev->bd_super->s_root:NULL, bdev->bd_super?bdev->bd_super->s_root->d_inode:NULL);
	//FOR FDISK、MOUNT and etc operation (address_space_operations will be used)
	bsize = bdev_logical_block_size(bdev);
	bdev->bd_block_size = bsize;
	bdev->bd_inode->i_blkbits = blksize_bits(bsize);
	pr_debug("[ftl_open] bd_inode.i_size:%lu, host.i_size:%lu \n", bdev->bd_inode->i_size, bdev->bd_inode->i_mapping->host->i_size);
//	bdev->bd_inode->i_size = get_capacity(bdev->bd_disk)<<9; 
//	bdev->bd_inode->i_mapping->host->i_size = get_capacity(bdev->bd_disk)<<9; 
	i_size_write(bdev->bd_inode, get_capacity(bdev->bd_disk)<<9);
	pr_debug("[ftl_open] bd_inode.i_size:%lu, host.i_size:%lu \n", bdev->bd_inode->i_size, bdev->bd_inode->i_mapping->host->i_size);
//	bdi_set_max_ratio(bdev->bd_inode_backing_dev_info, 20);

//	mapping = inode->i_mapping;
//	pr_debug("[ftl_open] begin  mapping->a_ops: 0x%x \n", (unsigned long)&mapping->a_ops);
//	mapping->a_ops = &ftl_aops;
	//使能片选
	//FLASH_SelectChip_f(0);
//	//初始化DMA
//	err = DMA_open();
//	if (err) return err;

//#if TIME_CALC
//	if ((mode & 0x1) == 0) {
//		g_time_idx = 0;
//	} else if ((mode & 0x80) != 0) {
//		printtimes();
//		g_time_idx = 0;
//	}
//#endif
	
	printk("[ftl_open] end, bdev:0x%lx, bd_dev:%lu, mode:0x%lx \n", (unsigned long)bdev, (unsigned long)bdev->bd_dev, (unsigned long)mode);
	return 0;
}

static int ftl_blk_ioctl(struct block_device *bdev, fmode_t mode,
			unsigned int cmd, unsigned long arg)
{
	int result = 0;
	struct storage_direct* sdArg;

	switch(cmd) {
	case IOCTL_STORAGE_DIRECT_READ:
#if USE_THREAD
		result = ftl_add_ioctl_request(&g_ftl_data, READ, arg);
		NAND_MSG("[ftl_ioctl] before read req schedule\n");
		g_ftl_data.thread_read_wait = current;
		while (g_ftl_data.thread_read_wait) {
			set_current_state(TASK_UNINTERRUPTIBLE);
			schedule();
		}
//		INIT_COMPLETION(write_doing); 
//		wait_for_completion(&write_doing);
		NAND_MSG("[ftl_ioctl] after read req schedule\n");
#else
		result = fsg_read_sectors((struct storage_direct*)arg);
#endif
		break;
	case IOCTL_STORAGE_DIRECT_WRITE:
#if USE_THREAD
		NAND_MSG("[ftl_ioctl] direct write begin\n");
		set_current_state(TASK_UNINTERRUPTIBLE);
		while (g_ftl_data.flashReqNum > MAX_PROCESS_NUM-1) {
			NAND_MSG("[ftl_ioctl] before write req schedule\n");
			g_ftl_data.thread_write_wait = current;
			schedule();
//			INIT_COMPLETION(write_limited); 
//			wait_for_completion(&write_limited);
			NAND_MSG("[ftl_ioctl] after write req schedule\n");
		}
		set_current_state(TASK_RUNNING);
		result = ftl_add_ioctl_request(&g_ftl_data, WRITE, arg);
#else
		sdArg = (struct storage_direct*)arg;
		result = fsg_write_sectors(sdArg);
		sdArg->actual = sdArg->count;
#endif
		break;
	case IOCTL_STORAGE_PING:
#if NO_IOCTL
		result = -EACCES;
#endif
		break;

#if TIME_CALC
	case _IO(0x12,221):
		g_time_idx = 0;
		break;
	case _IO(0x12,222):
		printtimes();
		break;
#endif
#ifdef _DEBUG
	case _IO(0x12,222):
		if (gLogfp) {
			if (g_buf_len) log_to_file();
			vfs_fsync(gLogfp, gLogfp->f_path.dentry, 1); //perform a fdatasync operation
		}
		break;
#endif

	default:
		printk("[ftl_ioctl] unknown cmd:0x%x\n", cmd);
		result = -ENOTTY;
		break;
	}

	return result;
}

static struct block_device_operations ftl_bd_op = {
	.owner =	THIS_MODULE,
	.open =		ftl_open,
	.release =	ftl_release,
	.ioctl = 	ftl_blk_ioctl,
	.getgeo =	ftl_getgeo,
};

/*
 * Before freeing the module, invalidate all of the protected buffers!
 */
static void __exit ftl_cleanup(void)
{
	printk("[ftl_cleanup] begin \n");

	/* Clean up the kernel thread */
#if USE_THREAD
	if (g_ftl_data.thread_req_proc) kthread_stop(g_ftl_data.thread_req_proc);
#endif
	if (g_ftl_data.disk) {
		//完成剩余的FTL写操作
		nand_get_device(sep_mtd->priv, sep_mtd, FL_WRITING);
		FTL_WriteClose_f();
		nand_release_device(sep_mtd);
		//NFC资源释放
		SEP_Nand_Release_f();
#if USE_THREAD
		if (g_storage_d[0].buf) {
			kfree((void *)g_storage_d[0].buf);
			g_storage_d[0].buf = NULL;
		}
#endif
		//释放dma使用的缓存
#if BUF_TRANSFER_BY_DMA
		if (gpram_databuf)  dma_free_coherent(NULL, GLOBAL_DATA_BUF_SIZE, (void *)gpram_databuf, databuf_dma_handle);
#else
		if (gpram_databuf) {
			kfree((void *)gpram_databuf);
		}
#endif
#if TIME_CALC
		if (g_times) kfree(g_times);
#endif
		del_gendisk(g_ftl_data.disk);
		put_disk(g_ftl_data.disk);
	}
	if (g_ftl_data.queue) blk_cleanup_queue(g_ftl_data.queue);
	
#ifdef _DEBUG
	if (gLogfp) {
		if (g_buf_len) log_to_file();
		vfs_fsync(gLogfp, gLogfp->f_path.dentry, 1); //perform a fdatasync operation
		filp_close(gLogfp, current->files);
	}
#endif
	//devfs_remove("ftlm");
	if (g_ftl_data.g_major) unregister_blkdev(g_ftl_data.g_major, "ftlmd");
	
	printk("[ftl_cleanup] end \n");
}

/*
 * This is the registration and initialization section of the FTL disk driver
 */
static int __init ftl_init(void)
{
	int err = -ENODEV;
	FTL_INFO *p=NULL;
	unsigned int i;

	printk("[ftl_init] begin \n");
#if !RUN_WITH_MTD
	complete_all(&nfc_doing);
#endif

	if (ftl_blocksize > SECOTR_PART_SIZE * FLASH_SECTOR_AMOUNT_PER_PP || ftl_blocksize < SECOTR_PART_SIZE ||
			(ftl_blocksize & (ftl_blocksize-1))) {
		printk("[ftl_init] wrong blocksize %d, reverting to defaults\n",
		       ftl_blocksize);
		ftl_blocksize = CONFIG_BLK_DEV_FTL_BLOCK_SIZE;
	}

	g_ftl_data.g_major = FTL_MAJOR;
	if (register_blkdev(g_ftl_data.g_major, "ftlmd")) {
		printk("[ftl_init] register_blkdev error \n");
		err = -EIO;
		return err;
	}

#ifdef _DEBUG
	new_log_file();
	if (!gLogfp) {
		printk("[ftl_init] gLogfp open error! \n");
		err = -EIO;
		goto out_dev;
	}
#endif

	//分配dma使用的缓存
#if BUF_TRANSFER_BY_DMA
	gpram_databuf = (unsigned long)dma_alloc_writecombine(NULL, GLOBAL_DATA_BUF_SIZE, &databuf_dma_handle, GFP_KERNEL);
#else
	gpram_databuf = (unsigned long)kmalloc(GLOBAL_DATA_BUF_SIZE, GFP_KERNEL);
#endif
	if (!gpram_databuf) {
		err = -EIO;
		goto out_dev;
	}
	nand_dma_handle = databuf_dma_handle;
	printk("[ftl_init] gpram_databuf: 0x%lx, gpram_databuf's dma_addr: 0x%lx, dma_addr calced: 0x%lx \n", gpram_databuf, (unsigned long)nand_dma_handle, (unsigned long)virt_to_dma(NULL, (void*)gpram_databuf));

#if USE_THREAD
	g_storage_d[0].buf = kmalloc(DIRECT_BUF_LEN*MAX_PROCESS_NUM, GFP_KERNEL);
	if (!g_storage_d[0].buf) {
		err = -EIO;
		goto out_dmabuf;
	}
	for (i = 1; i < MAX_PROCESS_NUM; i++) {
		g_storage_d[i].buf = g_storage_d[i-1].buf + DIRECT_BUF_LEN;
	}
#endif

#if TIME_CALC
	g_times = kmalloc(TIMES_CNT<<3, GFP_KERNEL);
	if (!g_times) {
		err = -EIO;
		goto out_dmabuf;
	}
#endif

	//NFC初始化
	err = SEP_Nand_Init_f();
	if (err) goto out_dmabuf;
	// Format the disk at the first device loading
	if (1 && (format && (format[0] == 'y' || format[0] == 'Y'))) FTL_LowFormat_f();
	if (test && (test[0] == 'd' || test[0] == 'D')) {
		DRIVER_test();
		goto out_nfc;
	}
	//FTL初始化
	nand_get_device(sep_mtd->priv, sep_mtd, FL_READING);
	err = -FTL_Init_f(0);
	nand_release_device(sep_mtd);
	if (err) {
		printk("[ftl_init] FTL_Init_f error:%d\n", -err);
		goto out_nfc;
	}

//	if (test && (test[0] == 'f' || test[0] == 'F')) main_test_FTL();
	if (test && (test[0] == 'i' || test[0] == 'I')) main_test_Driver_time();
	if (test && (test[0] == 't' || test[0] == 'T')) main_test_FTL_time();

	g_ftl_data.disk = alloc_disk(CONFIG_BLK_DEV_PARTITION_NUM);
	if (!g_ftl_data.disk) {
		printk("[ftl_init] alloc_disk error\n");
		goto out_nfc;
	}

	//devfs_mk_dir("ftlm");
	spin_lock_init(&g_ftl_data.queue_lock);
	spin_lock_init(&g_ftl_data.frh_lock);
	g_ftl_data.thread_write_wait = NULL;
	g_ftl_data.thread_read_wait = NULL;
	g_ftl_data.thread_wait_frh = NULL;
#if NO_SCHEDULER
	INIT_LIST_HEAD(&g_ftl_data.flash_req_head);
	g_ftl_data.flashReqNum = 0;
	g_ftl_data.queue = blk_alloc_queue(GFP_KERNEL);
	if (!g_ftl_data.queue)
		goto out_disk;

	blk_queue_make_request(g_ftl_data.queue, &ftl_make_request);
	blk_queue_ordered(g_ftl_data.queue, QUEUE_ORDERED_TAG, NULL);
#else
	g_ftl_data.queue = blk_init_queue(ftl_request_proc, &g_ftl_data.queue_lock);
	if (!g_ftl_data.queue)
		goto out_disk;

//	elevator_exit(g_ftl_data.queue->elevator);
//	err = elevator_init(g_ftl_data.queue, "noop");
//	if (err)
//		goto cleanup_queue;
#endif
	blk_queue_logical_block_size(g_ftl_data.queue, CONFIG_BLK_DEV_FTL_BLOCK_SIZE);
	blk_queue_max_sectors(g_ftl_data.queue, CONFIG_BLK_DEV_FTL_MAX_SEC);

#if USE_THREAD
	g_ftl_data.needMoreReqs = TRUE;
	g_ftl_data.queue->queuedata = &g_ftl_data;
	g_ftl_data.thread_req_proc = kthread_create(ftl_thread_req_proc, &g_ftl_data,
			"%sreq", "ftlmd");
	if (IS_ERR(g_ftl_data.thread_req_proc)) {
		err = PTR_ERR(g_ftl_data.thread_req_proc);
		goto cleanup_queue;
	}
#endif

	p = &ftl_info[0];
	p->unit = 0;
	p->cylinders = (gFTLSysInfo.realCapacity-1)/(255*63);
	p->heads = ((gFTLSysInfo.realCapacity-1) - p->cylinders*255*63)/63;
	p->sectors = gFTLSysInfo.realCapacity - p->cylinders*255*63 - p->heads*63;
	/* ftl_size is given in kB */
	ftl_size = gFTLSysInfo.realCapacity / 2; 
	g_ftl_data.disk->major = g_ftl_data.g_major;
	g_ftl_data.disk->first_minor = 0;
	g_ftl_data.disk->fops = &ftl_bd_op;
	g_ftl_data.disk->queue = g_ftl_data.queue;
	g_ftl_data.disk->private_data = p;
	g_ftl_data.disk->flags |= GENHD_FL_SUPPRESS_PARTITION_INFO;
	sprintf(g_ftl_data.disk->disk_name, "ndd%c", 'a');
	//sprintf(g_ftl_data.disk->devfs_name, "ftlm/%d", 0);
	set_capacity(g_ftl_data.disk, ftl_size * 2);
	add_disk(g_ftl_data.disk);
	
	/* ftl_size is given in kB */
	printk("FTLDISK driver initialized: %d FTL disks of %dK size %d blocksize\n",
		CONFIG_BLK_DEV_FTL_COUNT, ftl_size, ftl_blocksize);
	
	return 0;

cleanup_queue:
	blk_cleanup_queue(g_ftl_data.queue);
	g_ftl_data.queue = NULL;
out_disk:
	put_disk(g_ftl_data.disk);
//	blk_cleanup_queue(g_ftl_data.queue);
//	del_gendisk(g_ftl_data.disk);
out_nfc:
	SEP_Nand_Release_f();
out_dmabuf:
	if (g_storage_d[0].buf) {
		kfree((void *)g_storage_d[0].buf);
		g_storage_d[0].buf = NULL;
	}
	//释放dma使用的缓存
#if BUF_TRANSFER_BY_DMA
	if (gpram_databuf) dma_free_coherent(NULL, GLOBAL_DATA_BUF_SIZE, (void *)gpram_databuf, databuf_dma_handle);
#else
	if (gpram_databuf) {
		kfree((void *)gpram_databuf);
	}
#endif
	gpram_databuf = 0;
#if TIME_CALC
	if (g_times) kfree(g_times);
	g_times = 0;
#endif
out_dev:
	unregister_blkdev(g_ftl_data.g_major, "ftlmd");
	g_ftl_data.g_major = 0;
	
	return err;
}

module_init(ftl_init);
module_exit(ftl_cleanup);

/* options - nonmodular */
#ifndef MODULE
static int __init ftldisk_size(char *str)
{
	ftl_size = simple_strtol(str,NULL,0);
	return 1;
}
static int __init ftldisk_size2(char *str)	/* kludge */
{
	return ftldisk_size(str);
}
static int __init ftldisk_blocksize(char *str)
{
	ftl_blocksize = simple_strtol(str,NULL,0);
	return 1;
}
__setup("ftldisk=", ftldisk_size);
__setup("ftldisk_size=", ftldisk_size2);
__setup("ftldisk_blocksize=", ftldisk_blocksize);
#endif

/* options - modular */
module_param(ftl_size, int, 0);
MODULE_PARM_DESC(ftl_size, "Size of each FTL disk in kbytes.");
module_param(ftl_blocksize, int, 0);
MODULE_PARM_DESC(ftl_blocksize, "Blocksize of each FTL disk in bytes.");
module_param(format, charp, 0);
MODULE_PARM_DESC(format, "input 'y' to format the disk at the first device loading");
module_param(test, charp, 0);
MODULE_PARM_DESC(test, "input 'y' to test the disk at the first device loading");
MODULE_ALIAS_BLOCKDEV_MAJOR(FTL_MAJOR);

MODULE_LICENSE("GPL");
