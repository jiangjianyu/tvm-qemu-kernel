#include <linux/init.h> 
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/kernel.h> 
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#define MAX_BUF_SIZE 256

#define IOCTL_TVM_VTA_CMD_NEW_PAGE    1
#define IOCTL_TVM_VTA_CMD_FREE_PAGE   2
#define IOCTL_TVM_VTA_CMD_EXEC        3

// 4 * 4M
#define TOTAL_PAGES_LOG 15
#define TOTAL_PAGES (1 << TOTAL_PAGES_LOG)
#define PER_ALLOC_PAGES_LOG 4

#ifndef VM_RESERVED
#define  VM_RESERVED   (VM_DONTEXPAND | VM_DONTDUMP)
#endif

static struct page** pages_free = NULL;
static char* this_mem = NULL;

struct mmap_info {
	char *data;
	int reference;
};

void mmap_open(struct vm_area_struct *vma)
{
    int i;
    printk(KERN_DEBUG "Entering: mmap open %lx\n", (long)vma);
    // for (i = 0;i < TOTAL_PAGES;i++) {
    //     get_page(pages_free[i]);
    // }
}

void mmap_close(struct vm_area_struct *vma)
{
    int i;
    printk(KERN_DEBUG "Entering: mmap close %lx\n", (long)vma);
    // for (i = 0;i < TOTAL_PAGES;i++) {
    //     put_page(pages_free[i]);
    // }
}

static int mmap_fault(struct vm_fault *vmf)
{
    int idx = vmf->pgoff;
	vmf->page = pages_free[idx];
    get_page(vmf->page);
    // printk(KERN_DEBUG "Entering: mmap fault %lx:%lx\n", (long)vmf, vmf->pgoff);
	return 0;
}

struct vm_operations_struct mmap_vm_ops = {
	.open = mmap_open,
	.close = mmap_close,
	.fault = mmap_fault,
};

int cdev_mmap(struct file *filp, struct vm_area_struct *vma)
{
    printk(KERN_DEBUG "Entering: vma %lx\n", (long)vma);
    vma->vm_ops = &mmap_vm_ops;
    vma->vm_flags |= VM_RESERVED;
    mmap_open(vma);

    return 0;
}

struct cdev_data {
    struct cdev cdev;
};

static int cdev_major = 0;
static struct class *mycdev_class = NULL;
static struct cdev_data mycdev_data;
static unsigned char *user_data;

static int cdev_open (struct inode *inode, struct file *file) {
    printk(KERN_DEBUG "Entering: %s\n", __func__);
    return 0;
}

static int cdev_release (struct inode *inode, struct file *file) {
    printk(KERN_DEBUG "Entering: %s\n", __func__);
    return 0;
}

static void calculate(void) {
    long sum = 0;
    int i = 0;
    for (i = 0;i < 4096 * TOTAL_PAGES;i++) {
        sum += this_mem[i];
    }
    printk(KERN_DEBUG "sum: %ld\n", sum);
}

static long cdev_ioctl (struct file *file, unsigned int cmd, unsigned long arg) {
    printk(KERN_DEBUG "Entering: %s\n", __func__);
    switch (cmd) {
        case IOCTL_TVM_VTA_CMD_EXEC: calculate();   break;
        default:                                    break;
    }
    return 0;
}

static ssize_t cdev_read (struct file *file, char __user *buf, size_t count, loff_t *offset) {
    size_t udatalen;

    printk(KERN_DEBUG "Entering: %s\n", __func__);
    udatalen = strlen(user_data);
    printk(KERN_DEBUG "user data len: %zu\n", udatalen);
    if (count > udatalen)
        count = udatalen;

    if (copy_to_user(buf, user_data, count) != 0) {
        printk(KERN_ERR "Copy data to user failed\n");
        return -EFAULT;
    }

    return count;
}

static ssize_t cdev_write (struct file *file, const char __user *buf, size_t count, loff_t *offset) {
    size_t udatalen = MAX_BUF_SIZE;
    size_t nbr_chars = 0;

    printk(KERN_DEBUG "Entering: %s\n", __func__);
    if (count < udatalen)
        udatalen = count;

    nbr_chars = copy_from_user(user_data, buf, udatalen);
    if (nbr_chars == 0) {
        printk(KERN_DEBUG "Copied %zu bytes from the user\n", udatalen);
        printk (KERN_DEBUG "Receive data from user: %s", user_data);
    } else {
        printk(KERN_ERR "Copy data from user failed\n");
        return -EFAULT;
    }

    return count;
}

static const struct file_operations cdev_fops = {
    .owner    = THIS_MODULE,
    .open     = cdev_open,
    .release  = cdev_release,
    .unlocked_ioctl = cdev_ioctl,
    .read    = cdev_read,
    .write   = cdev_write,
    .mmap    = cdev_mmap
};

#define DEV_NAME "tvm-vta"

int init_module ( void ) {
    int err;
    struct device *dev_ret;
    dev_t dev;
    struct page* pages;
    int i, j;

    printk(KERN_DEBUG "Entering: %s\n", __func__);

    err = alloc_chrdev_region(&dev, 0, 1, DEV_NAME);
    if ( err < 0 ) {
        printk(KERN_ERR "Allocate a range of char device numbers failed.\n");
        return err;
    }

    cdev_major = MAJOR(dev);
    printk(KERN_DEBUG "device major number is: %d\n", cdev_major);
    if (IS_ERR(mycdev_class = class_create(THIS_MODULE, DEV_NAME))) {
        unregister_chrdev_region(MKDEV(cdev_major, 0), 1);
        return PTR_ERR(mycdev_class);
    }
    
    if (IS_ERR(dev_ret = device_create(mycdev_class, NULL, MKDEV(cdev_major, 0), NULL, DEV_NAME"-0"))) {
        class_destroy(mycdev_class);
        unregister_chrdev_region(MKDEV(cdev_major, 0), 1);
        return PTR_ERR(dev_ret);
    }
    
    cdev_init(&mycdev_data.cdev, &cdev_fops);
    mycdev_data.cdev.owner = THIS_MODULE;
 
    err = cdev_add(&mycdev_data.cdev, MKDEV(cdev_major, 0), 1);
    if ( err < 0 ) {
        printk (KERN_ERR "Unable to add a char device\n");
        device_destroy(mycdev_class, MKDEV(cdev_major, 0));
        class_unregister(mycdev_class);
        class_destroy(mycdev_class);
        unregister_chrdev_region(MKDEV(cdev_major, 0), 1);        
        return err;
    }

    user_data = (unsigned char*) kzalloc(MAX_BUF_SIZE, GFP_KERNEL);
    if (user_data == NULL) {
        printk (KERN_ERR "Allocation memory for data buffer failed\n");
        device_destroy(mycdev_class, MKDEV(cdev_major, 0));
        class_unregister(mycdev_class);
        class_destroy(mycdev_class);
        unregister_chrdev_region(MKDEV(cdev_major, 0), 1);
        return -ENOMEM;
    }

    pages_free = kmalloc(sizeof(struct page*) * TOTAL_PAGES, GFP_KERNEL);

    for (i = 0;i < TOTAL_PAGES;) {
        pages = alloc_pages(__GFP_ZERO, PER_ALLOC_PAGES_LOG);
        for (j = 0;j < (1 << PER_ALLOC_PAGES_LOG);j++) {
            pages_free[i + j] = pages + j;
            get_page(pages_free[i + j]);
        }
        i += (1 << PER_ALLOC_PAGES_LOG);
    }

    this_mem = vmap(pages_free, TOTAL_PAGES, VM_RESERVED, PAGE_KERNEL);
    return 0;
}

void cleanup_module ( void ) {
    int i, j;
    printk(KERN_DEBUG "Entering: %s\n", __func__);
    device_destroy(mycdev_class, MKDEV(cdev_major, 0));
    class_unregister(mycdev_class);
    class_destroy(mycdev_class);
    unregister_chrdev_region(MKDEV(cdev_major, 0), 1);
    if (user_data != NULL)
        kfree(user_data);
    vunmap (this_mem);
    for (i = 0;i < TOTAL_PAGES;) {
        for (j = 0;j < (1 << PER_ALLOC_PAGES_LOG);j++)
            put_page(pages_free[i + j]);
        // __free_pages(pages_free[i], PER_ALLOC_PAGES_LOG);
        i += (1 << PER_ALLOC_PAGES_LOG);
    }
}

MODULE_DESCRIPTION("A simple Linux char driver");
MODULE_VERSION("1.0");
MODULE_AUTHOR("Ilies CHERGUI <ilies.chergui@gmail.com>");
MODULE_LICENSE("GPL");

