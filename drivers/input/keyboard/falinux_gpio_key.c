/*0
 * Driver for keys on GPIO lines capable of generating interrupts.
 *
 * Copyright 2009 David You frog@falinux.com 
 * Base : gpio_keys.c 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <asm/io.h>
#include <asm/delay.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <mach/map.h>
#include <mach/gpio.h>
#include <mach/regs-gpio.h>

#include <plat/gpio-cfg.h>


#define	KEY_PRESS			1
#define	KEY_RELEASE			0

struct input_dev *falinux_gpio_key_input = NULL;

#define FALINUX_GPIO_KEYCODE_MAX             3

/*
	Android Keymap ( /system/usr/keylayout/qwerty.kl )
		
	key 399   GRAVE
	key 2     1
	key 3     2
	key 4     3
	key 5     4
	key 6     5
	key 7     6
	key 8     7
	key 9     8
	key 10    9
	key 11    0
	key 158   BACK              WAKE_DROPPED
	key 230   SOFT_RIGHT        WAKE
	key 60    SOFT_RIGHT        WAKE
	key 107   ENDCALL           WAKE_DROPPED
	key 62    ENDCALL           WAKE_DROPPED
	key 229   MENU              WAKE_DROPPED
	key 139   MENU              WAKE_DROPPED
	key 59    MENU              WAKE_DROPPED
	key 127   SEARCH            WAKE_DROPPED
	key 217   SEARCH            WAKE_DROPPED
	key 228   POUND
	key 227   STAR
	key 231   CALL              WAKE_DROPPED
	key 61    CALL              WAKE_DROPPED
	key 232   DPAD_CENTER       WAKE_DROPPED
	key 108   DPAD_DOWN         WAKE_DROPPED
	key 103   DPAD_UP           WAKE_DROPPED
	key 102   HOME              WAKE
	key 105   DPAD_LEFT         WAKE_DROPPED
	key 106   DPAD_RIGHT        WAKE_DROPPED
	key 115   VOLUME_UP         WAKE
	key 114   VOLUME_DOWN       WAKE
	key 116   POWER             WAKE
	key 212   CAMERA
	
	key 16    Q
	key 17    W
	key 18    E
	key 19    R
	key 20    T
	key 21    Y
	key 22    U
	key 23    I
	key 24    O
	key 25    P
	key 26    LEFT_BRACKET
	key 27    RIGHT_BRACKET
	key 43    BACKSLASH
	
	key 30    A
	key 31    S
	key 32    D
	key 33    F
	key 34    G
	key 35    H
	key 36    J
	key 37    K
	key 38    L
	key 39    SEMICOLON
	key 40    APOSTROPHE
	key 14    DEL
	        
	key 44    Z
	key 45    X
	key 46    C
	key 47    V
	key 48    B
	key 49    N
	key 50    M
	key 51    COMMA
	key 52    PERIOD
	key 53    SLASH
	key 28    ENTER
	        
	key 56    ALT_LEFT
	key 100   ALT_RIGHT
	key 42    SHIFT_LEFT
	key 54    SHIFT_RIGHT
	key 15    TAB
	key 57    SPACE
	key 150   EXPLORER
	key 155   ENVELOPE        
	
	key 12    MINUS
	key 13    EQUALS
	key 215   AT
	
	# On an AT keyboard: ESC, F10
	key 1     BACK              WAKE_DROPPED
	key 68    MENU              WAKE_DROPPED
*/
static const unsigned short falinux_gpio_keycode[FALINUX_GPIO_KEYCODE_MAX] = {
	102,		// KEY_HOME,
	KEY_MENU,
	KEY_BACK,
};

static int falinux_gpio_number_on_board[FALINUX_GPIO_KEYCODE_MAX] = {
	S5PV210_GPE1(0),
	S5PV210_GPE1(1),
	S5PV210_GPE1(2),
}; 

static int *falinux_gpio_number = falinux_gpio_number_on_board; 

static int falinux_gpio_old_state[FALINUX_GPIO_KEYCODE_MAX] = {
	0,
	0,
	0,
};	
	
static struct delayed_work 	time_work;

static void falinux_scan_timer_handler(struct work_struct *work);

static void falinux_scan_timer( void )
{
	INIT_DELAYED_WORK(&time_work, falinux_scan_timer_handler);
	schedule_delayed_work(&time_work, ((HZ)/50) );
}

static void falinux_scan_sync( void )
{
	int lp;
	int state;

	for( lp = 0; lp < FALINUX_GPIO_KEYCODE_MAX; lp++ )
	{
		if(falinux_gpio_number[lp] < 0 )	break;
		state = gpio_get_value( falinux_gpio_number[lp] ) ? KEY_RELEASE : KEY_PRESS;
		falinux_gpio_old_state[lp] = state; 
	}
           	
}

static void falinux_scan_timer_handler(struct work_struct *work)
{
	int lp;
	int state;

	for( lp = 0; lp < FALINUX_GPIO_KEYCODE_MAX; lp++ )
	{
		if(falinux_gpio_number[lp] < 0 )	break;
		state = gpio_get_value( falinux_gpio_number[lp] ) ? KEY_RELEASE : KEY_PRESS;
		if( falinux_gpio_old_state[lp] != state )
		{
			input_report_key(falinux_gpio_key_input, falinux_gpio_keycode[lp], state);
			input_sync(falinux_gpio_key_input);
		}	
	}	
	
	falinux_scan_sync();
	falinux_scan_timer();
	
}

static void	gpio_hw_init(void)
{
	s3c_gpio_cfgpin (S5PV210_GPE1(0), S3C_GPIO_INPUT);
	s3c_gpio_setpull(S5PV210_GPE1(0), S3C_GPIO_PULL_UP);

	s3c_gpio_cfgpin (S5PV210_GPE1(1), S3C_GPIO_INPUT);
	s3c_gpio_setpull(S5PV210_GPE1(1), S3C_GPIO_PULL_UP);

	s3c_gpio_cfgpin (S5PV210_GPE1(2), S3C_GPIO_INPUT);
	s3c_gpio_setpull(S5PV210_GPE1(2), S3C_GPIO_PULL_UP);
}

static int	falinux_gpio_key_open(struct input_dev *dev)
{
	falinux_scan_sync();
	falinux_scan_timer();
	return	0;
}

static void	falinux_gpio_key_close(struct input_dev *dev)
{
	
}

//------------------------------------------------------------------------------
/** @brief   proc 쓰기를 지원한다.
    @remark
*///----------------------------------------------------------------------------
static int falinux_gpio_key_proc_cmd( struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	char    cmd[256];
	int     len = count;

	if ( len > 256 ) len = 256;
	copy_from_user( cmd, buffer, len );
	cmd[len-1] = 0; // CR 제거

	if ( strcmp( "back", cmd ) == 0 )
	{
		printk( "falinux gpio key [KEY_BACK]\n" );
		input_event(falinux_gpio_key_input, EV_KEY, KEY_BACK, KEY_PRESS);
		input_sync(falinux_gpio_key_input);
	}
	else if( strcmp( "backup", cmd ) == 0 )
	{
		printk( "falinux gpio key [KEY_BACK]\n" );
		input_event(falinux_gpio_key_input, EV_KEY, KEY_BACK, KEY_RELEASE);
		input_sync(falinux_gpio_key_input);

	}
	else if ( strcmp( "menu", cmd ) == 0 )
	{
		printk( "falinux gpio key [KEY_MENU]\n" );
		input_event(falinux_gpio_key_input, EV_KEY, KEY_MENU, KEY_PRESS);
		input_sync(falinux_gpio_key_input);
	}
	else if ( strcmp( "menuup", cmd ) == 0 )
	{
		printk( "falinux gpio key [KEY_MENU]\n" );
		input_event(falinux_gpio_key_input, EV_KEY, KEY_MENU, KEY_RELEASE);
		input_sync(falinux_gpio_key_input);
	}
	else if ( strcmp( "home", cmd ) == 0 )
	{
		printk( "falinux gpio key [KEY_HOME]\n" );
		input_event(falinux_gpio_key_input, EV_KEY, KEY_HOME, KEY_PRESS);
		input_sync(falinux_gpio_key_input);
	}
	else if ( strcmp( "homeup", cmd ) == 0 )
	{
		printk( "falinux gpio key [KEY_HOME]\n" );
		input_event(falinux_gpio_key_input, EV_KEY, KEY_HOME, KEY_RELEASE);
		input_sync(falinux_gpio_key_input);
	}

	return -1;
}

//------------------------------------------------------------------------------
/** @brief   proc, ez virtual key
    @remark
*///----------------------------------------------------------------------------
static void falinux_gpio_key_register_proc( void )
{
	struct proc_dir_entry *procdir;

	procdir = create_proc_entry("fkey", S_IFREG | S_IRUGO, 0);
	//procdir->read_proc  =;
	procdir->write_proc = falinux_gpio_key_proc_cmd;
}

static int __devinit falinux_gpio_keys_probe(struct platform_device *pdev)
{
	int error;
	int key_code;
	int lp;
	
	falinux_gpio_key_input = input_allocate_device();
	
	if (!falinux_gpio_key_input) return -ENOMEM;

	falinux_gpio_key_input->name 		= pdev->name;
	falinux_gpio_key_input->phys 		= "falinux_gpio-keys/input0";
	falinux_gpio_key_input->dev.parent 	= &pdev->dev;

	falinux_gpio_key_input->id.bustype 	= BUS_HOST;
	falinux_gpio_key_input->id.vendor 	= 0x0001;
	falinux_gpio_key_input->id.product 	= 0x0001;
	falinux_gpio_key_input->id.version 	= 0x0100;
	
	falinux_gpio_key_input->evbit[0] 	= BIT_MASK(EV_KEY) | BIT_MASK(EV_SYN);
	
	falinux_gpio_key_input->keycode 	= &falinux_gpio_keycode;
	falinux_gpio_key_input->keycodesize = sizeof(unsigned short);
	falinux_gpio_key_input->keycodemax 	= ARRAY_SIZE(falinux_gpio_keycode);
	
	bitmap_fill(falinux_gpio_key_input->keybit, KEY_MAX);

    falinux_gpio_key_input->open 	= falinux_gpio_key_open;
    falinux_gpio_key_input->close	= falinux_gpio_key_close;
	
	
	error = input_register_device(falinux_gpio_key_input);
	if( error ) {
		printk( "Oh My God falinux gpio key register fail\n" );
		return error;
	}
	
	printk( "Ok! falinux gpio key register\n" );

	falinux_gpio_key_register_proc();    // EZ Virtual key

	return 0;
}

static int __devexit falinux_gpio_keys_remove(struct platform_device *pdev)
{
	input_unregister_device( falinux_gpio_key_input );
	return 0;
}


#ifdef CONFIG_PM
static int falinux_gpio_keys_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int falinux_gpio_keys_resume(struct platform_device *pdev)
{
	return 0;
}
#else
#define falinux_gpio_keys_suspend	NULL
#define falinux_gpio_keys_resume	NULL
#endif

static struct platform_driver falinux_gpio_keys_device_driver = {
	.probe		= falinux_gpio_keys_probe,
	.remove		= __devexit_p(falinux_gpio_keys_remove),
	.suspend	= falinux_gpio_keys_suspend,
	.resume		= falinux_gpio_keys_resume,
	.driver		= {
		.name	= "falinux-gpio-keys",
		.owner	= THIS_MODULE,
	}
};


static int __init falinux_gpio_keys_init(void)
{
	gpio_hw_init();	
	return platform_driver_register(&falinux_gpio_keys_device_driver);
}

static void __exit falinux_gpio_keys_exit(void)
{
	platform_driver_unregister(&falinux_gpio_keys_device_driver);
}



module_init(falinux_gpio_keys_init);
module_exit(falinux_gpio_keys_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("David You<frog@falinux.com>");
MODULE_DESCRIPTION("Keyboard driver for GPIOs");
MODULE_ALIAS("platform:gpio-keys");
