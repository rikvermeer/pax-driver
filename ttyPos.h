#ifndef _TTY_POS_H_
#define _TTY_POS_H_

#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/kref.h>
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,18))
#include <linux/uaccess.h>
#endif
#include <linux/usb.h>
#include <linux/mutex.h>
#include <linux/time.h>
#include <linux/sched.h>

#include <linux/err.h>
#include <linux/kthread.h>

#define USB_VENDOR_ID		0x1234
#define USB_PRODUCT_ID		0x0101

#define USB_NEW_VENDOR_ID		0x2FB8
#define PROLIN_PRODUCT_ID		0x1101
#define PROLIN_DOUBLE_PRODUCT_ID		0x110B
#define MONITOR_PRODUCT_ID		0x0101

static struct usb_device_id pos_usb_table[] = {
	{ USB_DEVICE(USB_VENDOR_ID, USB_PRODUCT_ID) },
	{ USB_DEVICE(USB_NEW_VENDOR_ID, PROLIN_PRODUCT_ID) },
	{ USB_DEVICE(USB_NEW_VENDOR_ID, MONITOR_PRODUCT_ID) },
	{ USB_DEVICE(USB_NEW_VENDOR_ID, PROLIN_DOUBLE_PRODUCT_ID) 	},
	{},
};

MODULE_DEVICE_TABLE(usb,pos_usb_table);

//#define POS_TTY_MAJOR		192	/* experimental range */
#define POS_TTY_MINORS		20	/* only have 20 devices */

#define THREAD_INIT		0x00
#define THREAD_STOPPED		0x01
#define THREAD_RUNNING		0x82
//#define THREAD_CLOSE		0x83
//#define THREAD_QUERY_WAIT	0x84
//#define THREAD_WAIT			0x85

#define THREAD_IS_RUNNING(ThreadState)	(ThreadState & 0x80)

#define WRITE_COMMAND		0	/* write to usb device command */
#define READ_COMMAND		1	/* read from to usb device command */
#define STATUS_COMMAND		2	/* get device buffer status command */
#define RESET_COMMAND       3
#define MAXDATA_COMMAND     4

#if 0
#define POOL_SIZE		10241
#define MAX_DATA        508
#else
#define POOL_SIZE		(120*1024+1)
#define MAX_DATA        65532
#endif

#define MAX_TRANSFER_SIZE 512

typedef struct _POOL {
	volatile unsigned int ReadPos;
	volatile unsigned int WritePos;
	unsigned char Buffer[POOL_SIZE];
} __attribute__((packed)) POOL, *PPOOL;

typedef struct {
	unsigned char SeqNo;
	unsigned char ReqType;		/* 0:OUT, 1:IN, 2:STAT, 3:RESET */
	unsigned short Len;
	unsigned char Data[MAX_DATA];
} __attribute__((packed)) ST_BULK_IO;

typedef struct {
	volatile unsigned int TxBufSize;
	volatile unsigned int RxBufSize;
	volatile unsigned int TxLeft;
	volatile unsigned int RxLeft;
} __attribute__((packed)) ST_BIO_STATE;

struct tty_pos {
	struct tty_struct *tty;
	unsigned char devIndex;
    atomic_t rc_busy;
    atomic_t discon;
    atomic_t openCnt;

	/* for tiocmget and tiocmset functions */
	int msr;		/* MSR shadow */
	int mcr;		/* MCR shadow */

	struct usb_device *udev;
	struct usb_interface *interface;
	struct urb *urb;
	wait_queue_head_t urb_wait;
	atomic_t urb_done;
	int timeout_jiffies;
	atomic_t urb_busy;

	wait_queue_head_t write_wait;
	atomic_t write_flag;

	__u8 bulk_in_epAddr;
	__u8 bulk_out_epAddr;

	struct kref kref;
	struct mutex io_mutex;		/* synchronize I/O with disconnect */
	volatile u8 ThreadState;

	unsigned char SeqCount;
	ST_BULK_IO *BioPack;		/* for IO access */
	ST_BIO_STATE BioDevState;

	volatile POOL TxPool;
    unsigned short maxdata;
};

#define to_pos_dev(d)		container_of(d, struct tty_pos, kref)

#define INIT_POOL_BUFFER(pool)	pool.ReadPos = pool.WritePos = 0

#define IS_POOL_EMPTY(pool)	(pool.ReadPos == pool.WritePos)

#define GET_USING_POOL(pool) \
	((pool.WritePos + POOL_SIZE - pool.ReadPos) % POOL_SIZE)

#define GET_SPACE_POOL(pool)	(POOL_SIZE - 1 - GET_USING_POOL(pool))

#if 0
#define ERR(stuff...)
#define INFO(stuff...)
#else
#define ERR(stuff...)		printk(KERN_ERR "ttyPos: " stuff)
#define INFO(stuff...)		printk(KERN_ALERT "ttyPos: " stuff)
#endif

#endif
