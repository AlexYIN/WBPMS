#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/mutex.h>
//内核版本的问题
#include <linux/vermagic.h>


#define USB_SKEL_VENDOR_ID	0x1241
#define USB_SKEL_PRODUCT_ID	0xb001

/* Get a minor range for your devices from the usb maintainer */
#define USB_SKEL_MINOR_BASE	192

struct usb_blood_private 
{
	struct usb_interface *interface;
	struct usb_device *device;
	int data_length;//数据数目


	/* synchronize I/O with disconnect ,用于同步读写*/
	struct mutex			io_mutex;
	/*保证我们在读或者写的过程中，不可以进行其他的读或者写*/
	/*为了等待读的完成。读完成后得回来，在原来的.read中把数据copy到用户空间，
	写不需要回来，因为写完就可以了，不需要回到原来的write操作中去。*/
	/* to wait for an ongoing read */
	struct completion		read_in_completion;	/* to wait for an ongoing read */
	
};

static struct usb_driver usb_blood_driver;














static int blood_usb_open(struct inode *inode, struct file *file)
{
	struct usb_blood_private *blood_dev;
	struct usb_interface *interface;
	int subminor;
	int retval = 0;

	//获得次设备号
	subminor = iminor (inode);
	//通过接口，获得我们的自定义结构体！！，既然我们可以使用全局变量usb_blood_driver
	//为什么不直接使用 全局变量，自定义结构体呢？？
	interface = usb_find_interface(&usb_blood_driver, subminor);
	if (!interface) {
		err("%s - error, can't find device for minor %d",
		     __func__, subminor);
		retval = -ENODEV;
		goto exit;
	}

	//从usb_interface 那得到自定义结构体，他们俩之间的关联是在.probe中连系起来的
	blood_dev = usb_get_intfdata(interface);
	if (!blood_dev) {
		retval = -ENODEV;
		goto exit;
	}
	
	file->private_data = blood_dev;
exit:
	return retval;

}

static void blood_usb_read_callback(struct urb *urb)
{
	struct usb_blood_private *blood_dev;

	blood_dev = urb->context;


	/* sync/async unlink faults aren't errors */
	if (urb->status) 
	{
		if (!(urb->status == -ENOENT ||urb->status == -ECONNRESET ||urb->status == -ESHUTDOWN))
		err("%s - nonzero write bulk status received: %d",__func__, urb->status);

	}



	complete(&blood_dev->read_in_completion);
	
}


static ssize_t blood_usb_read(struct file *file, char *buffer, size_t count,loff_t *ppos)
{

	struct urb *urb = NULL;
	__u8 *transfer_buffer= NULL;
	int retval = 0;
	//把自定义结构体保存下来
	struct usb_blood_private *blood_dev  = (struct usb_blood_private *)(file->private_data);

	
	/**/

	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb) {
		retval = -ENOMEM;

	}

	
	//这里用系统API申请我们要传输的数据缓冲区	
	transfer_buffer = usb_buffer_alloc(blood_dev->device, count, GFP_KERNEL,&urb->transfer_dma);

	usb_fill_int_urb (urb,//要提交的urb
				blood_dev->device,//对应的usb设备
				usb_rcvintpipe (blood_dev->device,1),//这个地方使用的1是端点号，不是端点地址0x81
				transfer_buffer,//缓冲区
				count,//缓冲区大小
				blood_usb_read_callback,//读回掉函数
				blood_dev,//传递的自定义结构体参数,在
				8);//最后一个参数是lsusb -v命令中,查看端点描述符得出的

	urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
	/*提交urb之前加锁*/			
	mutex_lock(&blood_dev->io_mutex);
	/* send the data out the bulk port */
	retval = usb_submit_urb(urb, GFP_KERNEL);


	/*
	 * release our reference to this urb, the USB core will eventually free
	 * it entirely
	 */
	//为什么这里就释放了，不是在完成回掉函数中释放么！！在urb回掉函数中提交另外一个urb很正常，
	usb_free_urb(urb);


	wait_for_completion(&blood_dev->read_in_completion);
	copy_to_user(buffer,transfer_buffer,count);


	//写完成，解锁，使得后续的 读/写 可以进行
	mutex_unlock(&blood_dev->io_mutex);




}






static void blood_usb_write_callback(struct urb *urb)
{
	/*如果传输完成了，就解锁*/
	struct usb_blood_private *blood_dev;

	//在填充urb的时候放上的
	blood_dev = urb->context;

	/* sync/async unlink faults aren't errors */
	if (urb->status) 
	{
		if (!(urb->status == -ENOENT ||urb->status == -ECONNRESET || urb->status == -ESHUTDOWN))
			
		err("%s - nonzero write bulk status received: %d",__func__, urb->status);
	
		
	}

	/* free up our allocated buffer */
	usb_buffer_free(urb->dev, urb->transfer_buffer_length,
			urb->transfer_buffer, urb->transfer_dma);

	//写完成，解锁，使得后续的 读/写 可以进行
	mutex_unlock(&blood_dev->io_mutex);



}


static ssize_t blood_usb_write(struct file *file, const char *user_buffer,
			  size_t count, loff_t *ppos)
{
	/*创建控制urb，提交，要对设备加锁，保证在写完成之前，读不可以进行。
	
	这里可以看出，对设备加锁，不光是为了让多个进程，同时访问设备，也可以保证
	同一进程，对设备的访问次序：先写，写完

	在回掉函数中，释放urb，同时，要释放
*/
	//把自定义结构体保存下来
	struct urb *urb = NULL;
	__u8 *setup_packet = NULL;
	__u8 *transfer_buffer= NULL;
	int retval = 0;
	struct usb_blood_private *blood_dev  = (struct usb_blood_private *)(file->private_data);



	
	/* create a urb, and a buffer for it, and copy the data to the urb */
	//第一个参数，如果我们要创建等时传输，那么是等式传输的数据包的数量，否则，置为0
	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb) {
		retval = -ENOMEM;

	}

	/*填充urb的时候要使用usb_device,这个地方是通过file->private_data传递过来的，
	这个参数是自定义结构体指针，他是在 .probe函数中 关联到interface,然后在open里面赋值给file->private_data*/

	//这里用系统API申请我们要传输的数据缓冲区	
	transfer_buffer = usb_buffer_alloc(blood_dev->device, count, GFP_KERNEL,&urb->transfer_dma);
	
	if (copy_from_user(transfer_buffer, user_buffer, count)) {
		retval = -EFAULT;

	}



	setup_packet = kmalloc (8,GFP_KERNEL);
	/*这个地方有个大小端问题，还没弄明白*/
	setup_packet[0] = 0x42;
	setup_packet[1] = 0x00;
	setup_packet[2] = 0x00;
	setup_packet[3] = 0x00;
	setup_packet[4] = 0x00;
	setup_packet[5] = 0x00;
	setup_packet[6] = 0x00;
	setup_packet[7] = 0x01;


	usb_fill_control_urb (urb,//要提交的urb
				blood_dev->device,//要发送的目标设备
				//unsigned int usb_sndctrlpipe(struct usb_device *dev, unsigned int endpoint)
				usb_sndctrlpipe(blood_dev->device,0),//（必须）用函数生成的特定端点信息
				//8个字节的setup_packet
				setup_packet,
				transfer_buffer,//传输的，特定数据的缓冲区（这个是用API申请的）
				1,//传输的，特定数据的缓冲区的长度
				blood_usb_write_callback,//回掉函数
				//用于参数传递，我们这里把我们的自定义结构体传进去，然后在urb完成回调函数中就可以使用了。
				blood_dev);
		


	urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
					
	/*提交urb之前加锁*/			
	mutex_lock(&blood_dev->io_mutex);
	/* send the data out the bulk port */
	retval = usb_submit_urb(urb, GFP_KERNEL);


	/*
	 * release our reference to this urb, the USB core will eventually free
	 * it entirely
	 */
	//为什么这里就释放了，不是在完成回掉函数中释放么！！在urb回掉函数中提交另外一个urb很正常，
	usb_free_urb(urb);

	return count;



}







static const struct file_operations blood_fops =
{
	.owner	=	THIS_MODULE,
	.open		=	blood_usb_open,	
	//.read		=	blood_usb_read,
	//.write	=	blood_usb_write,

};






static struct usb_class_driver blood_usb_class =
{
	.name = "myblood",
	.fops = &blood_fops,
	.minor_base = USB_SKEL_MINOR_BASE,



};

/*加载驱动后，如果匹配上设备，那么会调用这个函数来完成设备的初始化和
注册对本设备的使用的接口
*/
static int usb_blood_probe(struct usb_interface *interface,const struct usb_device_id *id)
{
	
	struct usb_blood_private *blood_dev;
	int retval = -ENOMEM;


	printk(KERN_INFO"usb-kernel- 匹配\n");

	//创建自定义结构体
	blood_dev  = kzalloc (sizeof(struct usb_blood_private),GFP_KERNEL);
	if (!blood_dev) {
		err("Out of memory");
		goto error;
	}
	//初始化自定义结构体

	mutex_init(&blood_dev->io_mutex);//初始化I/O同步信号量
	init_completion(&blood_dev->read_in_completion);//读操作中使用，用于读Urb完成后，回到原来的read函数，进行后续操作。

	//关联自定义结构体和usb_interface
	blood_dev->interface = interface;
	usb_set_intfdata(interface, blood_dev);

	/*我们要获取usb_device，然后在后面的read,write函数中，填充urb的时候要用*/
	blood_dev->device = usb_get_dev(interface_to_usbdev(interface));


	blood_dev->data_length = 0;//默认里面有0条数据


	//我们注册一个 用户应用程序访问本设备的接口，第二个参数
	retval = usb_register_dev(interface, &blood_usb_class);
	if (retval) {
		/* something prevented us from registering this driver */
		err("Not able to get a minor for this device.");
		usb_set_intfdata(interface, NULL);
		goto error;
	}
	return 0;
error:
	
	return retval;

}
static void usb_blood_disconnect(struct usb_interface *interface)
{
	struct usb_blood_private *blood_dev;
	int minor = interface->minor;

	blood_dev = usb_get_intfdata(interface);
	usb_set_intfdata(interface, NULL);

	//切断接口和自定义结构体之间的关系
	/* give back our minor */
	usb_deregister_dev(interface, &blood_usb_class);
	/* prevent more I/O from starting */
	blood_dev->interface = NULL;


	



}







/*以上是设备使用，之下是设备的管理*/


static struct usb_device_id usb_blood_table[] = {
	{ USB_DEVICE(USB_SKEL_VENDOR_ID, USB_SKEL_PRODUCT_ID) },
	{ }	
};
MODULE_DEVICE_TABLE(usb, usb_blood_table);


static struct usb_driver usb_blood_driver = {
	.name 	=	"skeleton",
	.probe 	=	usb_blood_probe,
	.disconnect 	=	usb_blood_disconnect,
	.id_table 	=	usb_blood_table,
	//.supports_autosuspend = 1,
};

static int __init usb_blood_init(void)
{
	int result;

	printk(KERN_INFO"usb-kernel- init\n");
	/* register this driver with the USB subsystem */
	result = usb_register(&usb_blood_driver);
	if (result)
		err("usb_register failed. Error number %d", result);

	return result;
}

static void __exit usb_blood_exit(void)
{
	/* deregister this driver with the USB subsystem */
	usb_deregister(&usb_blood_driver);
}

module_init(usb_blood_init);
module_exit(usb_blood_exit);

MODULE_LICENSE("GPL");

