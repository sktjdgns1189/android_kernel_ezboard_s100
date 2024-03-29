/* linux/arch/arm/mach-s5pv210/cpu.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/sysdev.h>
#include <linux/platform_device.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <asm/proc-fns.h>
#include <mach/map.h>
#include <mach/regs-clock.h>

#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/clock.h>
#include <plat/s5pv210.h>
#include <plat/iic-core.h>
#include <plat/sdhci.h>

/* Initial IO mappings */

static struct map_desc s5pv210_iodesc[] __initdata = {
	{
		.virtual	= (unsigned long)S5P_VA_SYSTIMER,
		.pfn		= __phys_to_pfn(S5PV210_PA_SYSTIMER),
		.length		= SZ_1M,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)VA_VIC2,
		.pfn		= __phys_to_pfn(S5PV210_PA_VIC2),
		.length		= SZ_16K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)VA_VIC3,
		.pfn		= __phys_to_pfn(S5PV210_PA_VIC3),
		.length		= SZ_16K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_SROMC,
		.pfn		= __phys_to_pfn(S5PV210_PA_SROMC),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S3C_VA_WATCHDOG,
		.pfn		= __phys_to_pfn(S5P_PA_WDT),
		.length 	= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S3C_VA_OTG,
		.pfn		= __phys_to_pfn(S5PV210_PA_OTG),
		.length		= SZ_1M,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S3C_VA_OTGSFR,
		.pfn		= __phys_to_pfn(S5PV210_PA_OTGSFR),
		.length		= SZ_1M,
		.type		= MT_DEVICE,
	},
#if defined(CONFIG_HRT_RTC)
	{
		.virtual	= (unsigned long)S5P_VA_RTC,
		.pfn		= __phys_to_pfn(S5PV210_PA_RTC),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	},
#endif
	{
		.virtual	= (unsigned long)S5P_VA_DMC0,
		.pfn		= __phys_to_pfn(S5P_PA_DMC0),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_DMC1,
		.pfn		= __phys_to_pfn(S5P_PA_DMC1),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_AUDSS,
		.pfn		= __phys_to_pfn(S5PV210_PA_AUDSS),
		.length		= SZ_1M,
		.type		= MT_DEVICE,
	},
#ifdef CONFIG_FALINUX_ZEROBOOT
    #ifdef CONFIG_FALINUX_ZEROBOOT_NAL
    {
        .virtual    = (unsigned long)0xFFFF8000,
        .pfn        = __phys_to_pfn(PHYS_OFFSET+0x88000),
        .length     = 0x8000,
        .type       = MT_HIGH_VECTORS,
    },
//	{    // specially mapping bl1 irom 96K
//        .virtual    = (unsigned long)0xD0020000,
//        .pfn        = __phys_to_pfn(0xD0020000),
//        .length     = 0x00020000,
//        .type       = MT_HIGH_VECTORS,
//    },
    #endif
#endif
};

static void s5pv210_idle(void)
{
	if (!need_resched())
		cpu_do_idle();

	local_irq_enable();
}

/* s5pv210_map_io
 *
 * register the standard cpu IO areas
*/

void __init s5pv210_map_io(void)
{
#ifdef CONFIG_S3C_DEV_ADC
	s3c_device_adc.name	= "s3c64xx-adc";
#endif

	iotable_init(s5pv210_iodesc, ARRAY_SIZE(s5pv210_iodesc));

	/* initialise device information early */
	s5pv210_default_sdhci0();
	s5pv210_default_sdhci1();
	s5pv210_default_sdhci2();

	/* the i2c devices are directly compatible with s3c2440 */
	s3c_i2c0_setname("s3c2440-i2c");
	s3c_i2c1_setname("s3c2440-i2c");
	s3c_i2c2_setname("s3c2440-i2c");
}

void __init s5pv210_init_clocks(int xtal)
{
	printk(KERN_DEBUG "%s: initializing clocks\n", __func__);

	s3c24xx_register_baseclocks(xtal);
	s5p_register_clocks(xtal);
	s5pv210_register_clocks();
	s5pv210_setup_clocks();
}

void __init s5pv210_init_irq(void)
{
	u32 vic[4];	/* S5PV210 supports 4 VIC */

	/* All the VICs are fully populated. */
	vic[0] = ~0;
	vic[1] = ~0;
	vic[2] = ~0;
	vic[3] = ~0;

	s5p_init_irq(vic, ARRAY_SIZE(vic));
}

struct sysdev_class s5pv210_sysclass = {
	.name	= "s5pv210-core",
};

static struct sys_device s5pv210_sysdev = {
	.cls	= &s5pv210_sysclass,
};

static int __init s5pv210_core_init(void)
{
	return sysdev_class_register(&s5pv210_sysclass);
}

core_initcall(s5pv210_core_init);

int __init s5pv210_init(void)
{
	printk(KERN_INFO "S5PV210: Initializing architecture\n");

	/* set idle function */
	pm_idle = s5pv210_idle;

	return sysdev_register(&s5pv210_sysdev);
}
