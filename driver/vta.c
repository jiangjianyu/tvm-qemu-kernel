/*
Like every other hardware, we could interact with PCI on x86
using only IO instructions and memory operations.

But PCI is a complex communication protocol that the Linux kernel
implements beautifully for us, so let's use the kernel API.

This example relies on the QEMU "edu" educational device.
Grep QEMU source for the device description, and keep it open at all times!

-   edu device source and spec in QEMU tree:
	- https://github.com/qemu/qemu/blob/v2.7.0/hw/misc/edu.c
	- https://github.com/qemu/qemu/blob/v2.7.0/docs/specs/edu.txt
-   http://www.zarb.org/~trem/kernel/pci/pci-driver.c inb outb runnable example (no device)
-   LDD3 PCI chapter
-   another QEMU device + module, but using a custom QEMU device:
	- https://github.com/levex/kernel-qemu-pci/blob/31fc9355161b87cea8946b49857447ddd34c7aa6/module/levpci.c
	- https://github.com/levex/kernel-qemu-pci/blob/31fc9355161b87cea8946b49857447ddd34c7aa6/qemu/hw/char/lev-pci.c
-   https://is.muni.cz/el/1433/podzim2016/PB173/um/65218991/ course given by the creator of the edu device.
	In Czech, and only describes API
-   http://nairobi-embedded.org/linux_pci_device_driver.html

DMA:

- 	https://stackoverflow.com/questions/32592734/are-there-any-dma-driver-example-pcie-and-fpga/44716747#44716747
- 	https://stackoverflow.com/questions/17913679/how-to-instantiate-and-use-a-dma-driver-linux-module
*/

#include <linux/cdev.h> /* cdev_ */
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/pfn_t.h>
#include <linux/uaccess.h>

/* https://stackoverflow.com/questions/30190050/what-is-base-address-register-bar-in-pcie/44716618#44716618
 *
 * Each PCI device has 6 BAR IOs (base address register) as per the PCI spec.
 *
 * Each BAR corresponds to an address range that can be used to communicate with the PCI.
 *
 * Eech BAR is of one of the two types:
 *
 * - IORESOURCE_IO: must be accessed with inX and outX
 * - IORESOURCE_MEM: must be accessed with ioreadX and iowriteX
 *   	This is the saner method apparently, and what the edu device uses.
 *
 * The length of each region is defined BY THE HARDWARE, and communicated to software
 * via the configuration registers.
 *
 * The Linux kernel automatically parses the 64 bytes of standardized configuration registers for us.
 *
 * QEMU devices register those regions with:
 *
 *     memory_region_init_io(&edu->mmio, OBJECT(edu), &edu_mmio_ops, edu,
 *                     "edu-mmio", 1 << 20);
 *     pci_register_bar(pdev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &edu->mmio);
 **/

// Author of EDU: Ilies CHERGUI <ilies.chergui@gmail.com> 

#define BAR 0
#define CDEV_NAME "tvm-vta"
#define VTA_DEVICE_ID 0x11e9
#define IO_IRQ_ACK 0x64
#define IO_IRQ_STATUS 0x24
#define QEMU_VENDOR_ID 0x1234

static struct pci_device_id pci_ids[] = {
	{ PCI_DEVICE(QEMU_VENDOR_ID, VTA_DEVICE_ID), },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, pci_ids);

static int pci_irq;
static int major;
static struct pci_dev *pdev;
struct class* cdevice_class;
struct device* cdevice;
static void __iomem *mmio;

static ssize_t read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
	ssize_t ret;
	u32 kbuf;

	if (*off % 4 || len == 0) {
		ret = 0;
	} else {
		kbuf = ioread32(mmio + *off);
		if (copy_to_user(buf, (void *)&kbuf, 4)) {
			ret = -EFAULT;
		} else {
			ret = 4;
			(*off)++;
		}
	}
	return ret;
}

static ssize_t write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
	ssize_t ret;
	u32 kbuf;

	ret = len;
	if (!(*off % 4)) {
		if (copy_from_user((void *)&kbuf, buf, 4) || len != 4) {
			ret = -EFAULT;
		} else {
			iowrite32(kbuf, mmio + *off);
		}
	}
	return ret;
}

static loff_t llseek(struct file *filp, loff_t off, int whence)
{
	filp->f_pos = off;
	return off;
}

void mmap_open(struct vm_area_struct *vma)
{
    printk(KERN_DEBUG "Entering: mmap open %lx\n", (long)vma);
}

void mmap_close(struct vm_area_struct *vma)
{
    printk(KERN_DEBUG "Entering: mmap close %lx\n", (long)vma);
}

static int mmap_fault(struct vm_fault *vmf)
{
	// struct vm_area_struct *vma = vmf->vma;
	// pgprot_t prot = vma->vm_page_prot;
	// resource_size_t start = pci_resource_start(pdev, BAR);
	// printk(KERN_DEBUG "start at %lx %lx\n", mmio, start);
	// // resource_size_t cur = pfn_to_page(vmf->pgoff + start >> 12);
	// unsigned long pfn = __phys_to_pfn(virt_to_phys(mmio)) + vmf->pgoff;
	// unsigned long address = vmf->address + vmf->pgoff * 4096;
	// // if (vma->vm_flags & VM_MIXEDMAP)
	// // 	ret = vmf_insert_mixed_prot(vma, address,
	// // 					__pfn_to_pfn_t(pfn, PFN_DEV),
	// // 					prot);
	// // else
	// 	// ret = 
	// printk(KERN_DEBUG "end at %lx %lx\n", mmio, pfn);
	// if (vma->vm_flags & VM_MIXEDMAP)
	// 	vm_insert_mixed(vma, address,
	// 			__pfn_to_pfn_t(pfn, PFN_DEV));
	// else
	// 	vm_insert_pfn(vma, address, pfn);
	return 1;
}

struct vm_operations_struct mmap_vm_ops = {
	.open = mmap_open,
	.close = mmap_close,
	.fault = mmap_fault,
};

int vta_mmap(struct file *filp, struct vm_area_struct *vma)
{
    printk(KERN_DEBUG "Entering: vma %lx\n", (long)vma);
    vma->vm_ops = &mmap_vm_ops;
    vma->vm_flags |= VM_IO;
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	return io_remap_pfn_range(vma, vma->vm_start, 
		__phys_to_pfn(pci_resource_start(pdev, BAR)), 
		vma->vm_end - vma->vm_start, vma->vm_page_prot);
}

/* These fops are a bit daft since read and write interfaces don't map well to IO registers.
 *
 * One ioctl per register would likely be the saner option. But we are lazy.
 *
 * We use the fact that every IO is aligned to 4 bytes. Misaligned reads means EOF. */
static struct file_operations fops = {
	.owner   = THIS_MODULE,
	.mmap	 = vta_mmap
};

static irqreturn_t irq_handler(int irq, void *dev)
{
	int devi;
	irqreturn_t ret;
	u32 irq_status;

	devi = *(int *)dev;
	if (devi == major) {
		irq_status = ioread32(mmio + IO_IRQ_STATUS);
		pr_info("interrupt irq = %d dev = %d irq_status = %llx\n",
				irq, devi, (unsigned long long)irq_status);
		/* Must do this ACK, or else the interrupts just keeps firing. */
		iowrite32(irq_status, mmio + IO_IRQ_ACK);
		ret = IRQ_HANDLED;
	} else {
		ret = IRQ_NONE;
	}
	return ret;
}

/**
 * Called just after insmod if the hardware device is connected,
 * not called otherwise.
 *
 * 0: all good
 * 1: failed
 */
static int pci_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	u8 val;

	pr_info("pci_probe\n");
	major = register_chrdev(0, CDEV_NAME, &fops);
	pdev = dev;
	pci_read_config_byte(dev, PCI_INTERRUPT_LINE, &val);
	pr_info("irq pre %x\n", val);
	if (pci_enable_device(dev) < 0) {
		dev_err(&(pdev->dev), "pci_enable_device\n");
		goto error;
	}

    cdevice_class = class_create(THIS_MODULE, CDEV_NAME"-class");
    if (IS_ERR(cdevice_class))
    {
		printk(KERN_INFO "Class creation failed\n");
		return PTR_ERR(cdevice_class);
    }

	cdevice = device_create(cdevice_class, NULL, MKDEV(major, 0), NULL, CDEV_NAME"-0");
    if (IS_ERR(cdevice))
    {
    	printk(KERN_INFO "Device creation failed\n");
		class_destroy(cdevice_class);
    	goto error;
    }

	dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	pci_set_master(dev);
	if (pci_request_region(dev, BAR, "myregion0")) {
		dev_err(&(pdev->dev), "pci_request_region\n");
		goto error;
	}
	mmio = pci_iomap(pdev, BAR, pci_resource_len(pdev, BAR));

	/* IRQ setup. */
	pci_read_config_byte(dev, PCI_INTERRUPT_LINE, &val);
	pci_irq = val;
	if (request_irq(pci_irq, irq_handler, IRQF_SHARED, "pci_irq_handler0", &major) < 0) {
		dev_err(&(dev->dev), "request_irq\n");
		goto error;
	}

	return 0;
error:
	return 1;
}

static void pci_remove(struct pci_dev *dev)
{
	pr_info("pci_remove\n");
	free_irq(pci_irq, &major);
	pci_release_region(dev, BAR);
	unregister_chrdev(major, CDEV_NAME);
	device_destroy(cdevice_class, MKDEV(major, 0));
	class_destroy(cdevice_class);
}

static struct pci_driver pci_driver = {
	.name     = "vta_pci",
	.id_table = pci_ids,
	.probe    = pci_probe,
	.remove   = pci_remove,
};

static int myinit(void)
{
	if (pci_register_driver(&pci_driver) < 0) {
		return 1;
	}
	return 0;
}

static void myexit(void)
{
	pci_unregister_driver(&pci_driver);
}

int init_module (void) {
	return myinit();
}

void cleanup_module (void) {
	myexit();
}

MODULE_DESCRIPTION("Linux driver for VTA");
MODULE_VERSION("1.0");
MODULE_AUTHOR("Jianyu Jiang <jianyu@connect.hku.hk>");
MODULE_LICENSE("GPL");
