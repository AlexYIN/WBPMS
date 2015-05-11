#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = KBUILD_MODNAME,
 .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
 .exit = cleanup_module,
#endif
 .arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xad12b0d5, "module_layout" },
	{ 0xfac3858a, "kmalloc_caches" },
	{ 0x163bfbfd, "dev_set_drvdata" },
	{ 0x105e2727, "__tracepoint_kmalloc" },
	{ 0x274e9e24, "usb_deregister_dev" },
	{ 0xffc7c184, "__init_waitqueue_head" },
	{ 0x759bb4fc, "usb_deregister" },
	{ 0x20f1ef60, "__mutex_init" },
	{ 0xb72397d5, "printk" },
	{ 0xb4390f9a, "mcount" },
	{ 0x7fac94da, "usb_register_dev" },
	{ 0xdeabe977, "kmem_cache_alloc" },
	{ 0x1143feb9, "usb_get_dev" },
	{ 0x7e8075e2, "usb_find_interface" },
	{ 0xdf64fc23, "usb_register_driver" },
	{ 0x4854eee, "dev_get_drvdata" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

MODULE_ALIAS("usb:v1241pB001d*dc*dsc*dp*ic*isc*ip*");

MODULE_INFO(srcversion, "3C5051E618AF090A1D36870");
