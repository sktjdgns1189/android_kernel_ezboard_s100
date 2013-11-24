/* linux/arch/arm/mach-s5pv210/mach-ezs5pv210.c
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
#include <linux/init.h>
#include <linux/serial_core.h>
#include <linux/gpio.h>
#include <linux/gpio_event.h>
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/max8998.h>
#include <linux/i2c/ak8973.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/usb/ch9.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/clk.h>
#include <linux/input/cypress-touchkey.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/skbuff.h>
#include <linux/console.h>

#include <net/ax88796.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/system.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#include <mach/gpio-herring.h>
#include <mach/ts-s3c.h>
#include <mach/adc.h>
#include <mach/param.h>
#include <mach/system.h>

#include <linux/usb/gadget.h>
#include <linux/fsa9480.h>
#include <linux/pn544.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/wlan_plat.h>
#include <linux/mfd/wm8994/wm8994_pdata.h>

#ifdef CONFIG_ANDROID_PMEM
  #include <linux/android_pmem.h>
  #include <plat/media.h>
  #include <mach/media.h>
#endif

#ifdef CONFIG_S5PV210_POWER_DOMAIN
  #include <mach/power-domain.h>
#endif

#include <mach/cpu-freq-v210.h>
#include <media/s5ka3dfx_platform.h>
#include <media/s5k4ecgx.h>

#include <plat/regs-serial.h>
#include <plat/s5pv210.h>
#include <plat/devs.h>
#include <plat/cpu.h>
#include <plat/fb.h>
#include <plat/mfc.h>
#include <plat/iic.h>
#include <plat/pm.h>

#include <plat/sdhci.h>
#include <plat/fimc.h>
#include <plat/jpeg.h>
#include <plat/clock.h>
#include <plat/regs-otg.h>
#include <linux/gp2a.h>

#include <../../../drivers/video/samsung/s3cfb.h>

#ifdef CONFIG_FALINUX_ZEROBOOT
static int zbopt_usb_late = 0;
#endif

/*----------------------------------------------------------------------------
    EM-S5PV210 모듈을 사용한다면 아래의 주소와 IRQ 를 준수한다.
*/
#define BASE_ADDR_AX88796B_0        (S5PV210_PA_CS1 + 0x000)
#define BASE_ADDR_AX88796B_1        (S5PV210_PA_CS1 + 0x100)
#define IRQ_AX88796B_0              IRQ_EINT10
#define IRQ_AX88796B_1              IRQ_EINT11
#define GPIO_FOR_IRQ_AX88796B_0     (S5PV210_GPH1(2))          // EINT10
#define GPIO_FOR_IRQ_AX88796B_1     (S5PV210_GPH1(3))          // EINT11

#define BASE_ADDR_UART8250_0		(S5PV210_PA_CS4 + 0x000)	// 4-port UART Chip#0
#define BASE_ADDR_UART8250_1		(S5PV210_PA_CS4 + 0x400)	// 4-port UART Chip#1
#define BASE_ADDR_UART8250_2		(S5PV210_PA_CS4 + 0x800)	// 4-port UART Chip#2
#define BASE_ADDR_UART8250_3		(S5PV210_PA_CS4 + 0xC00)	// 4-port UART Chip#3
#define IRQ_UART8250_0_1			IRQ_EINT6
#define IRQ_UART8250_2_3			IRQ_EINT7
#define GPIO_FOR_IRQ_UART8250_0_1	(S5PV210_GPH0(6))			// 8-port share IRQ		EINT6
#define GPIO_FOR_IRQ_UART8250_2_3	(S5PV210_GPH0(7))			// 8-port share IRQ		EINT7


/*----------------------------------------------------------------------------
    GPIO  in/out 을 정의하고 출력일 경우 초기값을 정의한다.
*/
#define S3C_GPIO_SETPIN_ZERO         0
#define S3C_GPIO_SETPIN_ONE          1
#define S3C_GPIO_SETPIN_NONE	     2
struct  gpio_init_data {	u32 num;  u32 cfg;	u32 val; u32 pud; u32 drv; };
static struct gpio_init_data ezs5pv210_init_gpios[] = {
	//									S3C_GPIO_INPUT				S3C_GPIO_SETPIN_ZERO			S3C_GPIO_PULL_NONE        	S3C_GPIO_DRVSTR_1X
	//							 		S3C_GPIO_OUTPUT				S3C_GPIO_SETPIN_ONE 			S3C_GPIO_PULL_DOWN			S3C_GPIO_DRVSTR_2X
	//							       	S3C_GPIO_SFN(2)		  		S3C_GPIO_SETPIN_NONE			S3C_GPIO_PULL_UP			S3C_GPIO_DRVSTR_4X

	// UART0
	{	.num =	S5PV210_GPA0(0),.cfg =	S3C_GPIO_SFN(2),	.val =	S3C_GPIO_SETPIN_NONE,	.pud =	S3C_GPIO_PULL_UP,	.drv =	S3C_GPIO_DRVSTR_4X, }, 	// rxd.0 
	{	.num =	S5PV210_GPA0(1),.cfg =	S3C_GPIO_SFN(2),	.val =	S3C_GPIO_SETPIN_NONE,	.pud =	S3C_GPIO_PULL_UP,	.drv =	S3C_GPIO_DRVSTR_4X, },  // txd.0 
	{	.num =	S5PV210_GPA0(2),.cfg =	S3C_GPIO_SFN(2),	.val =	S3C_GPIO_SETPIN_NONE,	.pud =	S3C_GPIO_PULL_UP,	.drv =	S3C_GPIO_DRVSTR_4X, },  // cts.0 
	{	.num =	S5PV210_GPA0(3),.cfg =	S3C_GPIO_SFN(2),	.val =	S3C_GPIO_SETPIN_NONE,	.pud =	S3C_GPIO_PULL_UP,	.drv =	S3C_GPIO_DRVSTR_4X, },  // rts.0 
	// UART1
	{	.num =	S5PV210_GPA0(4),.cfg =	S3C_GPIO_SFN(2),	.val =	S3C_GPIO_SETPIN_NONE,	.pud =	S3C_GPIO_PULL_UP,	.drv =	S3C_GPIO_DRVSTR_4X, },  // rxd.1 
	{	.num =	S5PV210_GPA0(5),.cfg =	S3C_GPIO_SFN(2),	.val =	S3C_GPIO_SETPIN_NONE,	.pud =	S3C_GPIO_PULL_UP,	.drv =	S3C_GPIO_DRVSTR_4X, },  // txd.1 
	{	.num =	S5PV210_GPA0(6),.cfg =	S3C_GPIO_SFN(2),	.val =	S3C_GPIO_SETPIN_NONE,	.pud =	S3C_GPIO_PULL_UP,	.drv =	S3C_GPIO_DRVSTR_4X, },  // cts.1 
	{	.num =	S5PV210_GPA0(7),.cfg =	S3C_GPIO_SFN(2),	.val =	S3C_GPIO_SETPIN_NONE,	.pud =	S3C_GPIO_PULL_UP,	.drv =	S3C_GPIO_DRVSTR_4X, },  // rts.1 
	// UART3
	{	.num =	S5PV210_GPA1(2),.cfg =	S3C_GPIO_SFN(2),	.val =	S3C_GPIO_SETPIN_NONE,	.pud =	S3C_GPIO_PULL_UP,	.drv =	S3C_GPIO_DRVSTR_4X, },  // rxd.3 
	{	.num =	S5PV210_GPA1(3),.cfg =	S3C_GPIO_SFN(2),	.val =	S3C_GPIO_SETPIN_NONE,	.pud =	S3C_GPIO_PULL_UP,	.drv =	S3C_GPIO_DRVSTR_4X, },  // txd.3 
};


//static void __init ezs5pv210_gpio_set( void )
//{
//    // UART.0
//    s3c_gpio_cfgpin( S5PV210_GPA0(0), S3C_GPIO_SFN(2)); // rxd.0
//    s3c_gpio_cfgpin( S5PV210_GPA0(1), S3C_GPIO_SFN(2)); // txd.0
//    s3c_gpio_cfgpin( S5PV210_GPA0(2), S3C_GPIO_SFN(2)); // cts.0
//    s3c_gpio_cfgpin( S5PV210_GPA0(3), S3C_GPIO_SFN(2)); // rts.0
//
//    // UART.1
//    s3c_gpio_cfgpin( S5PV210_GPA0(4), S3C_GPIO_SFN(2)); // rxd.1
//    s3c_gpio_cfgpin( S5PV210_GPA0(5), S3C_GPIO_SFN(2)); // txd.1
//    s3c_gpio_cfgpin( S5PV210_GPA0(6), S3C_GPIO_SFN(2)); // cts.1
//    s3c_gpio_cfgpin( S5PV210_GPA0(7), S3C_GPIO_SFN(2)); // rts.1
//
//    // UART.3
//    s3c_gpio_cfgpin( S5PV210_GPA1(2), S3C_GPIO_SFN(2)); // rxd.3
//    s3c_gpio_cfgpin( S5PV210_GPA1(3), S3C_GPIO_SFN(2)); // txd.3
//
//	// 사운드 ON
//	s3c_gpio_cfgpin (S5PV210_GPE0(6), S3C_GPIO_OUTPUT);
//	s3c_gpio_setpull(S5PV210_GPE0(6), S3C_GPIO_PULL_NONE);
//	gpio_set_value  (S5PV210_GPE0(6), 1);
//	
//	// USB 전원 제어
//	s3c_gpio_cfgpin (S5PV210_GPE0(3), S3C_GPIO_OUTPUT);
//	s3c_gpio_setpull(S5PV210_GPE0(3), S3C_GPIO_PULL_NONE);
//	gpio_set_value  (S5PV210_GPE0(3), 1);
//
//	s3c_gpio_cfgpin (S5PV210_GPE0(4), S3C_GPIO_OUTPUT);
//	s3c_gpio_setpull(S5PV210_GPE0(4), S3C_GPIO_PULL_NONE);
//	gpio_set_value  (S5PV210_GPE0(4), 1);
//
//	s3c_gpio_cfgpin (S5PV210_GPE0(5), S3C_GPIO_OUTPUT);
//	s3c_gpio_setpull(S5PV210_GPE0(5), S3C_GPIO_PULL_NONE);
//	gpio_set_value  (S5PV210_GPE0(5), 1);
//}

/* Following are default values for UCON, ULCON and UFCON UART registers */
#define S5PV210_UCON_DEFAULT	(S3C2410_UCON_TXILEVEL |	\
				 S3C2410_UCON_RXILEVEL |	\
				 S3C2410_UCON_TXIRQMODE |	\
				 S3C2410_UCON_RXIRQMODE |	\
				 S3C2410_UCON_RXFIFO_TOI |	\
				 S3C2443_UCON_RXERR_IRQEN)

#define S5PV210_ULCON_DEFAULT	S3C2410_LCON_CS8

#define S5PV210_UFCON_DEFAULT	(S3C2410_UFCON_FIFOMODE |	\
				 S5PV210_UFCON_TXTRIG4 |	\
				 S5PV210_UFCON_RXTRIG4)

static struct s3c2410_uartcfg ezs5pv210_uartcfgs[] __initdata = {
	[0] = {
		.hwport		= 0,
		.flags		= 0,
		.ucon		= S5PV210_UCON_DEFAULT,
		.ulcon		= S5PV210_ULCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
	},
	[1] = {
		.hwport		= 1,
		.flags		= 0,
		.ucon		= S5PV210_UCON_DEFAULT,
		.ulcon		= S5PV210_ULCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
	},
	[2] = {
		.hwport		= 2,
		.flags		= 0,
		.ucon		= S5PV210_UCON_DEFAULT,
		.ulcon		= S5PV210_ULCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
	},
	[3] = {
		.hwport		= 3,
		.flags		= 0,
		.ucon		= S5PV210_UCON_DEFAULT,
		.ulcon		= S5PV210_ULCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
	},
};


#if defined(CONFIG_TOUCHSCREEN_S3C)
static struct s3c_ts_mach_info s3c_ts_platform __initdata = {
    .delay                  = 10000,
    .presc                  = 49,
    .oversampling_shift     = 2,
    .resol_bit              = 12,
    .s3c_adc_con            = ADC_TYPE_2,
};


/* Touch srcreen */
static struct resource s3c_ts_resource[] = {
    [0] = {
        .start = S3C_PA_ADC,
        .end   = S3C_PA_ADC + SZ_4K - 1,
        .flags = IORESOURCE_MEM,
    },
#ifdef CONFIG_TOUCHSCREEN_ADC1
    [1] = {
        .start = IRQ_PENDN1,
        .end   = IRQ_PENDN1,
        .flags = IORESOURCE_IRQ,
    },
    [2] = {
        .start = IRQ_ADC1,
        .end   = IRQ_ADC1,
        .flags = IORESOURCE_IRQ,
    }
#else
    [1] = {
        .start = IRQ_PENDN,
        .end   = IRQ_PENDN,
        .flags = IORESOURCE_IRQ,
    },
    [2] = {
        .start = IRQ_ADC,
        .end   = IRQ_ADC,
        .flags = IORESOURCE_IRQ,
    }
#endif
};

struct platform_device s3c_device_ts = {
    .name         = "s3c-ts",
    .id       = -1,
    .num_resources    = ARRAY_SIZE(s3c_ts_resource),
    .resource     = s3c_ts_resource,
};

void __init s3c_ts_set_platdata(struct s3c_ts_mach_info *pd)
{
    struct s3c_ts_mach_info *npd;

    npd = kmalloc(sizeof(*npd), GFP_KERNEL);
    if (npd) {
        memcpy(npd, pd, sizeof(*npd));
        s3c_device_ts.dev.platform_data = npd;
    } else {
        pr_err("no memory for Touchscreen platform data\n");
    }
}
#endif

#ifdef CONFIG_S5P_ADC
static struct s3c_adc_mach_info s3c_adc_platform __initdata = {
    /* s5pc110 support 12-bit resolution */
    .delay  = 10000,
    .presc  = 49,
    .resolution = 12,
};
#endif

#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC0	( 6144 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC1	( 9900 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC2	( 6144 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC0		(36864 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC1		(36864 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMD 	( CONFIG_FB_S3C_X_VRES * \
												  CONFIG_FB_S3C_Y_VRES * 4 * \
					     						  CONFIG_FB_S3C_NR_BUFFERS )
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_JPEG 	(8192 * SZ_1K)

static struct s5p_media_device ezs5pv210_media_devs[] = {
	[0] = {
		.id = S5P_MDEV_MFC,
		.name = "mfc",
		.bank = 0,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC0,
		.paddr = 0,
	},
	[1] = {
		.id = S5P_MDEV_MFC_B, // S5P_MDEV_MFC,
		.name = "mfc_b",
		.bank = 0,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC1,
		.paddr = 0,
	},
	[2] = {
		.id = S5P_MDEV_FIMD,
		.name = "fimd",
		.bank = 0,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMD,
		.paddr = 0,
	},
#ifdef CONFIG_VIDEO_JPEG_V2
	[3] = {
		.id = S5P_MDEV_JPEG,
		.name = "jpeg",
		.bank = 0,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_JPEG,
		.paddr = 0,
	},
#endif

#ifdef CONFIG_VIDEO_FIMC
	[4] = {
		.id = S5P_MDEV_FIMC0,
		.name = "fimc0",
		.bank = 0,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC0,
		.paddr = 0,
	},
	[5] = {
		.id = S5P_MDEV_FIMC1,
		.name = "fimc1",
		.bank = 0,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC1,
		.paddr = 0,
	},
	[6] = {
		.id = S5P_MDEV_FIMC2,
		.name = "fimc2",
		.bank = 0,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC2,
		.paddr = 0,
	},
#endif
};

#ifdef CONFIG_CPU_FREQ
static struct s5pv210_cpufreq_voltage smdkc110_cpufreq_volt[] = {
	{
		.freq	= 1000000,
		.varm	= 1275000,
		.vint	= 1100000,
	}, {
		.freq	=  800000,
		.varm	= 1200000,
		.vint	= 1100000,
	}, {
		.freq	=  400000,
		.varm	= 1050000,
		.vint	= 1100000,
	}, {
		.freq	=  200000,
		.varm	=  950000,
		.vint	= 1100000,
	}, {
		.freq	=  100000,
		.varm	=  950000,
		.vint	= 1000000,
	},
};

static struct s5pv210_cpufreq_data smdkc110_cpufreq_plat = {
	.volt	= smdkc110_cpufreq_volt,
	.size	= ARRAY_SIZE(smdkc110_cpufreq_volt),
};
#endif

/*
static unsigned int lcd_type;
module_param_named(lcd_type, lcd_type, uint, 0444);
MODULE_PARM_DESC(lcd_type, "LCD type: default= sony, 1= hydis, 2= hitachi");
*/
static void panel_cfg_gpio(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF0(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF0(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF1(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF1(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF2(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF2(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 4; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF3(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF3(i), S3C_GPIO_PULL_NONE);
	}

	/* mDNIe SEL: why we shall write 0x2 ? */
#ifdef CONFIG_FB_S3C_MDNIE
	writel(0x1, S5P_MDNIE_SEL);
#else
	writel(0x2, S5P_MDNIE_SEL);
#endif
}


void lcd_cfg_gpio_early_suspend(void)
{
//
}
EXPORT_SYMBOL(lcd_cfg_gpio_early_suspend);

void lcd_cfg_gpio_late_resume(void)
{
//
}
EXPORT_SYMBOL(lcd_cfg_gpio_late_resume);

static int panel_reset_lcd(struct platform_device *pdev)
{
	return 0;
}

static int panel_backlight_on(struct platform_device *pdev)
{
	return 0;
}


/* Interface setting */
static struct s3c_platform_fimc fimc_plat_lsi = {
	.srclk_name	= "mout_mpll",
	.clk_name	= "sclk_fimc",
	.lclk_name	= "sclk_fimc_lclk",
	.clk_rate	= 166750000,
//	.default_cam	= CAMERA_PAR_A,
//	.camera		= {
//		&s5k4ecgx,
//		&s5ka3dfx,
//	},
	.hw_ver		= 0x43,
};

#ifdef CONFIG_VIDEO_JPEG_V2
static struct s3c_platform_jpeg jpeg_plat __initdata = {
	.max_main_width		= 800,
	.max_main_height	= 480,
	.max_thumb_width	= 320,
	.max_thumb_height	= 240,
};
#endif


#ifdef CONFIG_AX88796B 
static struct ax_plat_data ax88796_platdata = {
    .flags      = AXFLG_MAC_FROMDEV,
    .wordlength = 2,
    .dcr_val    = 0x48,
    .rcr_val    = 0x40,
};

static struct resource ax88796_resources[] = {
    [0] = {
        .start  = BASE_ADDR_AX88796B_0,
        .end    = BASE_ADDR_AX88796B_0  + 0xff,
        .flags  = IORESOURCE_MEM,
    },
    [1] = {
        .start = IRQ_AX88796B_0,
        .end   = IRQ_AX88796B_0,
        .flags = IORESOURCE_IRQ,
    },
/*
    [2] = {
        .start  = BASE_ADDR_AX88796B_1,
        .end    = BASE_ADDR_AX88796B_1  + 0xff,
        .flags  = IORESOURCE_MEM,
    },
    [3] = {
        .start = IRQ_AX88796B_1,
        .end   = IRQ_AX88796B_1,
        .flags = IORESOURCE_IRQ,
    },
*/
};

struct platform_device device_ax88796 = {
    .name           = "ax88796b",
    .id             = -1,
    .num_resources  = ARRAY_SIZE(ax88796_resources),
    .resource       = ax88796_resources,
    .dev        = {
        .platform_data = &ax88796_platdata
    }
};
#endif

#ifdef CONFIG_ANDROID_PMEM
static struct android_pmem_platform_data pmem_pdata = {
	.name = "pmem",
	.no_allocator = 1,
	.cached = 1,
	.start = 0,
	.size = 0,
};

static struct android_pmem_platform_data pmem_gpu1_pdata = {
	.name = "pmem_gpu1",
	.no_allocator = 1,
	.cached = 1,
	.buffered = 1,
	.start = 0,
	.size = 0,
};

static struct android_pmem_platform_data pmem_adsp_pdata = {
	.name = "pmem_adsp",
	.no_allocator = 1,
	.cached = 1,
	.buffered = 1,
	.start = 0,
	.size = 0,
};

static struct platform_device pmem_device = {
	.name = "android_pmem",
	.id = 0,
	.dev = { .platform_data = &pmem_pdata },
};

static struct platform_device pmem_gpu1_device = {
	.name = "android_pmem",
	.id = 1,
	.dev = { .platform_data = &pmem_gpu1_pdata },
};

static struct platform_device pmem_adsp_device = {
	.name = "android_pmem",
	.id = 2,
	.dev = { .platform_data = &pmem_adsp_pdata },
};

static void __init android_pmem_set_platdata(void)
{
	pmem_pdata.start = (u32)s5p_get_media_memory_bank(S5P_MDEV_PMEM, 0);
	pmem_pdata.size = (u32)s5p_get_media_memsize_bank(S5P_MDEV_PMEM, 0);

	pmem_gpu1_pdata.start =
		(u32)s5p_get_media_memory_bank(S5P_MDEV_PMEM_GPU1, 0);
	pmem_gpu1_pdata.size =
		(u32)s5p_get_media_memsize_bank(S5P_MDEV_PMEM_GPU1, 0);

	pmem_adsp_pdata.start =
		(u32)s5p_get_media_memory_bank(S5P_MDEV_PMEM_ADSP, 0);
	pmem_adsp_pdata.size =
		(u32)s5p_get_media_memsize_bank(S5P_MDEV_PMEM_ADSP, 0);
}
#endif


void s3c_config_gpio_table(void)
{
	u32 i, gpio;

	for (i = 0; i < ARRAY_SIZE(ezs5pv210_init_gpios); i++) 
	{
		gpio = ezs5pv210_init_gpios[i].num;
		if (system_rev <= 0x07 && gpio == S5PV210_GPJ3(3)) 
		{
			s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
			gpio_set_value(gpio, S3C_GPIO_SETPIN_ONE);
		} 
		else if (gpio <= S5PV210_MP05(7)) 
		{
			s3c_gpio_cfgpin(gpio, ezs5pv210_init_gpios[i].cfg);
			s3c_gpio_setpull(gpio, ezs5pv210_init_gpios[i].pud);

			if (ezs5pv210_init_gpios[i].val != S3C_GPIO_SETPIN_NONE)
				gpio_set_value(gpio, ezs5pv210_init_gpios[i].val);

			s3c_gpio_set_drvstrength(gpio, ezs5pv210_init_gpios[i].drv);
		}
	}
}

void s3c_config_sleep_gpio_table( int array_size, unsigned int (*gpio_table)[3] )
{
	u32 i, gpio;

	for (i = 0; i < array_size; i++) {
		gpio = gpio_table[i][0];
		s3c_gpio_slp_cfgpin(gpio, gpio_table[i][1]);
		s3c_gpio_slp_setpull_updown(gpio, gpio_table[i][2]);
	}
}

#define S5PV210_PS_HOLD_CONTROL_REG (S3C_VA_SYS+0xE81C)

/* I2C0 */
static struct i2c_board_info i2c_devs0[] __initdata = {
	{
		I2C_BOARD_INFO("ds1307", 0x68),
	},
};



static struct platform_device watchdog_device = {
	.name = "watchdog",
	.id = -1,
};

static struct platform_device falinux_gpio_keys_device = {
    .name   = "falinux-gpio-keys",
    .id     = -1,
};

static struct platform_device *ezs5pv210_devices[] __initdata = {
	&watchdog_device,
	&falinux_gpio_keys_device,

#ifdef CONFIG_RTC_DRV_S3C
	&s5p_device_rtc,
#endif
#ifdef CONFIG_S3C_DEV_WDT
	&s3c_device_wdt,
#endif

#ifdef CONFIG_FB_S3C
	&s3c_device_fb,
#endif

#ifdef CONFIG_VIDEO_MFC50
	&s3c_device_mfc,
#endif

#ifdef CONFIG_VIDEO_FIMC
//	&s3c_device_fimc0,
	&s3c_device_fimc1,
//	&s3c_device_fimc2,
#endif

#ifdef CONFIG_VIDEO_JPEG_V2
	&s3c_device_jpeg,
#endif

	&s3c_device_g3d,
	&s3c_device_lcd,

#ifdef CONFIG_USB_GADGET
	&s3c_device_usbgadget,
#endif
#ifdef CONFIG_USB_ANDROID
	&s3c_device_android_usb,
#ifdef CONFIG_USB_ANDROID_MASS_STORAGE
	&s3c_device_usb_mass_storage,
#endif
#ifdef CONFIG_USB_ANDROID_RNDIS
	&s3c_device_rndis,
#endif
#endif

#ifdef CONFIG_TOUCHSCREEN_S3C
    &s3c_device_ts,
#endif
#ifdef CONFIG_S5P_ADC
    &s3c_device_adc,
#endif

#ifndef CONFIG_FALINUX_ZEROBOOT
#ifdef CONFIG_USB
    &s3c_device_usb_ehci,
    &s3c_device_usb_ohci,
#endif
#endif

#ifdef CONFIG_S3C_DEV_HSMMC
	&s3c_device_hsmmc0,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC1
	&s3c_device_hsmmc1,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC2
	&s3c_device_hsmmc2,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC3
	&s3c_device_hsmmc3,
#endif

#ifdef CONFIG_S5PV210_POWER_DOMAIN
	&s5pv210_pd_audio,
	&s5pv210_pd_cam,
	&s5pv210_pd_tv,
	&s5pv210_pd_lcd,
	&s5pv210_pd_mfc,
#endif

#ifdef CONFIG_ANDROID_PMEM
	&pmem_device,
	&pmem_gpu1_device,
	&pmem_adsp_device,
#endif

	&s3c_device_i2c0,
#if defined(CONFIG_S3C_DEV_I2C1)
	&s3c_device_i2c1,
#endif

#if defined(CONFIG_S3C_DEV_I2C2)
	&s3c_device_i2c2,
#endif

#ifdef CONFIG_HAVE_PWM
	&s3c_device_timer[0],
	&s3c_device_timer[1],
	&s3c_device_timer[2],
	&s3c_device_timer[3],
#endif


#ifdef CONFIG_AX88796B
	&device_ax88796,
#endif

#ifdef CONFIG_MTD_NAND	// @FG
	&s3c_device_nand,
#endif

#ifdef CONFIG_SOUND
	&s5pv210_device_ac97,
#endif
};



unsigned int HWREV;
EXPORT_SYMBOL(HWREV);


static void __init ezs5pv210_map_io(void)
{
	s5p_init_io(NULL, 0, S5P_VA_CHIPID);

	s3c24xx_init_clocks(24000000);

	s5pv210_gpiolib_init();

	s3c24xx_init_uarts(ezs5pv210_uartcfgs, ARRAY_SIZE(ezs5pv210_uartcfgs));

	s5p_reserve_bootmem(ezs5pv210_media_devs, ARRAY_SIZE(ezs5pv210_media_devs));
}

unsigned int pm_debug_scratchpad = 0;

static void __init ezs5pv210_fixup(struct machine_desc *desc,
		struct tag *tags, char **cmdline,
		struct meminfo *mi)
{
#ifdef CONFIG_PHYS_DRAM_256M_AT_0x20000000
	mi->bank[0].start = 0x20000000;
	mi->bank[0].size  = 256 * SZ_1M;
	mi->bank[0].node  = 0;
	mi->nr_banks      = 1;
#endif	
#ifdef CONFIG_PHYS_DRAM_512M_AT_0x20000000
	mi->bank[0].start = 0x20000000;
	mi->bank[0].size  = 512 * SZ_1M;
	mi->bank[0].node  = 0;
	mi->nr_banks      = 1;
#endif
#ifdef CONFIG_PHYS_DRAM_512M_AT_0x30000000
	mi->bank[0].start = 0x30000000;
	mi->bank[0].size  = 256 * SZ_1M;
	mi->bank[0].node  = 0;
	mi->bank[1].start = 0x40000000;
	mi->bank[1].size  = 256 * SZ_1M;
	mi->bank[1].node  = 1;
	mi->nr_banks      = 2;
#endif
#ifdef CONFIG_PHYS_DRAM_1G_AT_0x20000000
	mi->bank[0].start = 0x20000000;
	mi->bank[0].size  = 512 * SZ_1M;
	mi->bank[0].node  = 0;
	mi->bank[1].start = 0x40000000;
	mi->bank[1].size  = 512 * SZ_1M;
	mi->bank[1].node  = 1;
	mi->nr_banks      = 2;
#endif
}


/* this function are used to detect s5pc110 chip version temporally */

static void ezs5pv210_init_gpio(void)
{
	s3c_config_gpio_table();
	//s3c_config_sleep_gpio_table(ARRAY_SIZE(ezs5pv210_sleep_gpio_table),
	//		ezs5pv210_sleep_gpio_table);
}

static void __init fsa9480_gpio_init(void)
{
	s3c_gpio_cfgpin(GPIO_USB_SEL, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_USB_SEL, S3C_GPIO_PULL_NONE);

	s3c_gpio_cfgpin(GPIO_UART_SEL, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_UART_SEL, S3C_GPIO_PULL_NONE);

}


static void __init sound_init(void)
{
	u32 reg;

	reg = __raw_readl(S5P_OTHERS);
	reg &= ~(0x3 << 8);
	reg |= 3 << 8;
	__raw_writel(reg, S5P_OTHERS);

	reg = __raw_readl(S5P_CLK_OUT);
	reg &= ~(0x1f << 12);
	reg |= 19 << 12;
	__raw_writel(reg, S5P_CLK_OUT);

	reg = __raw_readl(S5P_CLK_OUT);
	reg &= ~0x1;
	reg |= 0x1;
	__raw_writel(reg, S5P_CLK_OUT);

	gpio_request(GPIO_MICBIAS_EN, "micbias_enable");
}


static void __init ezs5pv210_eint_set( void )
{
    u32  gpio, irq;
	//u32 _num, _cfg, _val, _pud,  _drv;

#if 1
#ifdef CONFIG_AX88796B
    gpio = GPIO_FOR_IRQ_AX88796B_0;
    irq  = IRQ_AX88796B_0;
        s3c_gpio_cfgpin     ( gpio, EINT_MODE            );
        s3c_gpio_setpull    ( gpio, S3C_GPIO_PULL_UP     );
        set_irq_type        ( irq , IRQF_TRIGGER_FALLING );

    gpio = GPIO_FOR_IRQ_AX88796B_1;
    irq  = IRQ_AX88796B_1;
        s3c_gpio_cfgpin     ( gpio, EINT_MODE            );
        s3c_gpio_setpull    ( gpio, S3C_GPIO_PULL_UP     );
        set_irq_type        ( irq , IRQF_TRIGGER_FALLING );
#endif
#endif

#if 0
#ifdef CONFIG_SERIAL_8250
    gpio = GPIO_FOR_IRQ_UART8250_0_1;
    irq  = IRQ_UART8250_0_1;
        s3c_gpio_cfgpin     ( gpio, EINT_MODE            );
        s3c_gpio_setpull    ( gpio, S3C_GPIO_PULL_DOWN   );
        set_irq_type        ( irq , IRQF_TRIGGER_HIGH    );

    gpio = GPIO_FOR_IRQ_UART8250_2_3;
    irq  = IRQ_UART8250_2_3;
        s3c_gpio_cfgpin     ( gpio, EINT_MODE            );
        s3c_gpio_setpull    ( gpio, S3C_GPIO_PULL_DOWN   );
        set_irq_type        ( irq , IRQF_TRIGGER_HIGH    );
#endif
#endif

}



static void __init ezs5pv210_machine_init(void)
{
	printk("\r\nezs5pv210_machine_init\r\n");
	
	ezs5pv210_init_gpio();
	ezs5pv210_eint_set();
	

#ifdef CONFIG_TOUCHSCREEN_S3C
    s3c_ts_set_platdata(&s3c_ts_platform);
#endif

#ifdef CONFIG_S5P_ADC
    s3c_adc_set_platdata(&s3c_adc_platform);
#endif

#ifdef CONFIG_ANDROID_PMEM
	android_pmem_set_platdata();
#endif

#if defined(CONFIG_PM)
	s3c_pm_init();
#endif

#ifdef CONFIG_FB_S3C
    s3cfb_set_platdata(NULL);
#endif

	/* i2c */
	s3c_i2c0_set_platdata(NULL);
	i2c_register_board_info(0, i2c_devs0, ARRAY_SIZE(i2c_devs0));

#ifdef CONFIG_S3C_DEV_I2C1
	s3c_i2c1_set_platdata(NULL);
#endif

#ifdef CONFIG_S3C_DEV_I2C2
	s3c_i2c2_set_platdata(NULL);
#endif

#ifdef CONFIG_VIDEO_FIMC
	/* fimc */
//	s3c_fimc0_set_platdata(&fimc_plat_lsi);
	s3c_fimc1_set_platdata(&fimc_plat_lsi);
//	s3c_fimc2_set_platdata(&fimc_plat_lsi);
#endif

#ifdef CONFIG_VIDEO_JPEG_V2
	s3c_jpeg_set_platdata(&jpeg_plat);
#endif

#ifdef CONFIG_VIDEO_MFC50
	/* mfc */
	s3c_mfc_set_platdata(NULL);
#endif

#ifdef CONFIG_S3C_DEV_HSMMC
	s5pv210_default_sdhci0();
#endif
#ifdef CONFIG_S3C_DEV_HSMMC1
	s5pv210_default_sdhci1();
#endif
#ifdef CONFIG_S3C_DEV_HSMMC2
	s5pv210_default_sdhci2();
#endif
#ifdef CONFIG_S3C_DEV_HSMMC3
	s5pv210_default_sdhci3();
#endif
#ifdef CONFIG_S5PV210_SETUP_SDHCI
	s3c_sdhci_set_platdata();
#endif

#ifdef CONFIG_SOUND
	//GPIO E0_6 HIGH
	s3c_gpio_cfgpin (S5PV210_GPE0(6), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S5PV210_GPE0(6), S3C_GPIO_PULL_UP);
	gpio_set_value(S5PV210_GPE0(6), 1);
#endif

#ifdef CONFIG_CPU_FREQ
	s5pv210_cpufreq_set_platdata(&smdkc110_cpufreq_plat);
#endif

	platform_add_devices(ezs5pv210_devices, ARRAY_SIZE(ezs5pv210_devices));
	
#ifdef CONFIG_FALINUX_ZEROBOOT
	if ( 0 == zbopt_usb_late ) 
	{
		platform_device_register( &s3c_device_usb_ehci );
		platform_device_register( &s3c_device_usb_ohci );
	}
#endif

	/* write something into the INFORM6 register that we can use to
	 * differentiate an unclear reboot from a clean reboot (which
	 * writes a small integer code to INFORM6).
	 */
	__raw_writel(0xee, S5P_INFORM6);
}


#ifdef CONFIG_USB_SUPPORT
/* Initializes OTG Phy. */
void otg_phy_init(void)
{
	/* USB PHY0 Enable */
	writel(readl(S5P_USB_PHY_CONTROL) | (0x1<<0),
			S5P_USB_PHY_CONTROL);
	writel((readl(S3C_USBOTG_PHYPWR) & ~(0x3<<3) & ~(0x1<<0)) | (0x1<<5),
			S3C_USBOTG_PHYPWR);
	writel((readl(S3C_USBOTG_PHYCLK) & ~(0x5<<2)) | (0x3<<0),
			S3C_USBOTG_PHYCLK);
	writel((readl(S3C_USBOTG_RSTCON) & ~(0x3<<1)) | (0x1<<0),
			S3C_USBOTG_RSTCON);
	msleep(1);
	writel(readl(S3C_USBOTG_RSTCON) & ~(0x7<<0),
			S3C_USBOTG_RSTCON);
	msleep(1);

	/* rising/falling time */
	writel(readl(S3C_USBOTG_PHYTUNE) | (0x1<<20),
			S3C_USBOTG_PHYTUNE);

	/* set DC level as 0xf (24%) */
	writel(readl(S3C_USBOTG_PHYTUNE) | 0xf, S3C_USBOTG_PHYTUNE);
}
EXPORT_SYMBOL(otg_phy_init);

/* USB Control request data struct must be located here for DMA transfer */
struct usb_ctrlrequest usb_ctrl __attribute__((aligned(64)));

/* OTG PHY Power Off */
void otg_phy_off(void)
{
	writel(readl(S3C_USBOTG_PHYPWR) | (0x3<<3),
			S3C_USBOTG_PHYPWR);
	writel(readl(S5P_USB_PHY_CONTROL) & ~(1<<0),
			S5P_USB_PHY_CONTROL);
}
EXPORT_SYMBOL(otg_phy_off);

void usb_host_phy_init(void)
{
	struct clk *otg_clk;

	otg_clk = clk_get(NULL, "otg");
	clk_enable(otg_clk);

	if (readl(S5P_USB_PHY_CONTROL) & (0x1<<1))
		return;

	__raw_writel(__raw_readl(S5P_USB_PHY_CONTROL) | (0x1<<1),
			S5P_USB_PHY_CONTROL);
	__raw_writel((__raw_readl(S3C_USBOTG_PHYPWR)
			& ~(0x1<<7) & ~(0x1<<6)) | (0x1<<8) | (0x1<<5),
			S3C_USBOTG_PHYPWR);
	__raw_writel((__raw_readl(S3C_USBOTG_PHYCLK) & ~(0x1<<7)) | (0x3<<0),
			S3C_USBOTG_PHYCLK);
	__raw_writel((__raw_readl(S3C_USBOTG_RSTCON)) | (0x1<<4) | (0x1<<3),
			S3C_USBOTG_RSTCON);
	__raw_writel(__raw_readl(S3C_USBOTG_RSTCON) & ~(0x1<<4) & ~(0x1<<3),
			S3C_USBOTG_RSTCON);
}
EXPORT_SYMBOL(usb_host_phy_init);

void usb_host_phy_off(void)
{
	__raw_writel(__raw_readl(S3C_USBOTG_PHYPWR) | (0x1<<7)|(0x1<<6),
			S3C_USBOTG_PHYPWR);
	__raw_writel(__raw_readl(S5P_USB_PHY_CONTROL) & ~(1<<1),
			S5P_USB_PHY_CONTROL);
}
EXPORT_SYMBOL(usb_host_phy_off);
#endif


MACHINE_START(EZS5PV210, "EZS5PV210")
	.phys_io		= S3C_PA_UART & 0xfff00000,
	.io_pg_offst	= (((u32)S3C_VA_UART) >> 18) & 0xfffc,
	.boot_params	= S5P_PA_SDRAM + 0x100,
	.fixup			= ezs5pv210_fixup,
	.init_irq		= s5pv210_init_irq,
	.map_io			= ezs5pv210_map_io,
	.init_machine	= ezs5pv210_machine_init,
#if	defined(CONFIG_S5P_HIGH_RES_TIMERS)
	.timer		= &s5p_systimer,
#else
	.timer		= &s3c24xx_timer,
#endif
MACHINE_END


#ifdef CONFIG_FALINUX_ZEROBOOT

//// plat-s5p/devs.c 에 첫번째것 정의됨
//struct platform_device s3c_device_g3d_zb = {
//	.name	= "pvrsrvkm",
//	.id		= 1,
//};
//static int g3d_zb_load = 0;

#include <asm/uaccess.h>
#include <linux/proc_fs.h>
static int zeroboot_option_proc_write( struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	char	cmd[128];
	int		rtn, len = count;

	if ( len > 128 ) len = 128;
	rtn = copy_from_user( cmd, buffer, len );
	cmd[len-1] = 0;	// CR 제거
	
	if ( 0 == strncmp( "usb", cmd, 3 ) )
	{
		if ( 1 == zbopt_usb_late )
		{
			zbopt_usb_late ++;
			platform_device_register( &s3c_device_usb_ehci );
			platform_device_register( &s3c_device_usb_ohci );
		}
	}
	//else if ( 0 == strncmp( "pvr", cmd, 3 ) )
	//{
	//	if ( 0 == g3d_zb_load )
	//	{
	//		g3d_zb_load ++;
	//		platform_device_register( &s3c_device_g3d_zb );
	//	}
	//}	
	return count;
}

//void zeroboot_boot_call( void )
//{
//		if ( 0 == g3d_zb_load )
//		{
//			g3d_zb_load ++;
//			platform_device_register( &s3c_device_g3d_zb );
//		}
//}
//EXPORT_SYMBOL(zeroboot_boot_call); 

int __init setup_zbusb( char *s )
{
	zbopt_usb_late  = simple_strtoul( s, NULL, 0 );
	zbopt_usb_late &= 0x1;
	return 0;	
}
__setup( "zb_usb=", setup_zbusb );


int zeroboot_option_init( void )
{
	struct proc_dir_entry *procdir;
 
	procdir = create_proc_entry( "zb_active_late", S_IFREG | S_IRUGO, 0);
	procdir->write_proc = zeroboot_option_proc_write;
	return 0;
}
module_init(zeroboot_option_init);

#endif  // CONFIG_FALINUX_ZEROBOOT

