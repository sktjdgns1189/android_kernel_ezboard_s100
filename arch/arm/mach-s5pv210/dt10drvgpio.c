/****************************************************************************/
/* Copyright (C) 2009-2011, Heartland.Data.co.  All Rights Reserved.		*/
/*																			*/
/* Title	: DT10 Driver	[All]											*/
/* Author	: HLDC															*/
/****************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/cdev.h>
#include <asm/io.h>

#define LICENSE "GPL v2"

//#include "dt10_tpdrv.h"
#define		DT_VARIABLE_BIT			0x02				/* Memory Dump MASK */
#define		DT_EVTTRG_BIT			0x08				/* EventTrigger */
#define		DT_VARIABLE_FAST_BIT	0x01				/* Memory Dump MASK(Fast Mode) */
#define		DT_EVTTRG_FAST_BIT		0x02				/* EventTrigger(Fast Mode) */

#define	MAX_TP_DATA					256

typedef struct {
	unsigned int	addr;				/* Address */
	unsigned short	data;				/* Data */
	int				size;				/* MemoryDump Size */
	unsigned char	p[MAX_TP_DATA+1];	/* MemoryDump Array */
	int				fast_mode;			/* 0: Normal Mode, 1:Fast Mode, 2:Fast Mode EventTrigger */
} dt10_tp;


static int driver_major_no = 0;		/* メジャー番号を設定する */
static struct cdev char_dev;


#define PHYS_GPH0				0xE0200C00

#define REG_GPH0CON 			*(volatile u32 *)(gpio_port + 0x00)   // R/W Port Group GPH0 Configuration Register 0x00000000
#define REG_GPH0DAT 			*(volatile u32 *)(gpio_port + 0x04)   // R/W Port Group GPH0 Data Register 0x00
#define REG_GPH0PUD 			*(volatile u32 *)(gpio_port + 0x08)   // R/W Port Group GPH0 Pull-up/down Register 0x5555
#define REG_GPH0DRV 			*(volatile u32 *)(gpio_port + 0x0C)   // R/W Port Group GPH0 Drive Strength Control Register 0x0000

#define  GPIO_CON_ANDMASK(x)	(~( (0xf)<<((x)*4) ))
#define  GPIO_CON_INPUT(x)		( ( (0x0)<<((x)*4) ))
#define  GPIO_CON_OUTPUT(x)		( ( (0x1)<<((x)*4) ))
#define  GPIO_DIR_OUTPUT(r,x)	{ u32 v; v = (r) & GPIO_CON_ANDMASK(x); (r) = v | GPIO_CON_OUTPUT(x); }
#define  GPIO_DIR_INPUT(r,x)	{ u32 v; v = (r) & GPIO_CON_ANDMASK(x); (r) = v | GPIO_CON_INPUT(x) ; }
#define  GPIO_OUTPUT(r,x,b)		{ u32 v; v = (r) & (~(1<<(x))); (r) = v | (((b)&0x1)<<(x)); }
#define  GPIO_INPUT(r,x)		( ((r) & (1<<(x))) ? 1:0 )
#define  GPIO_PULLUP(r,x)		{ unsigned int v; v = (r) & ~(3<<(x)); (r) = v | (2<<(x)); }
#define  GPIO_PULLDN(r,x)		{ unsigned int v; v = (r) & ~(3<<(x)); (r) = v | (1<<(x)); }

#define	CS_BIT					(1<<4)
#define	CLK_BIT					(1<<5)
#define	DATA_BIT				(0xf<<0)
#define GPIO_DATA				REG_GPH0DAT

#define	CLK_CS_HI				GPIO_DATA |= (CS_BIT | CLK_BIT)
#define	CLK_HI					GPIO_DATA |=  CLK_BIT
#define	CLK_LOW					GPIO_DATA &= ~CLK_BIT				
#define	CS_HI					GPIO_DATA |=  CS_BIT
#define	CS_LOW					GPIO_DATA &= ~CS_BIT
#define DATA_OUT(x) 			{ u32 bits; bits = GPIO_DATA & ~DATA_BIT; bits |= (x)&DATA_BIT; GPIO_DATA = bits; } 			
                           
static void *gpio_port = NULL;

static int dt10_gpio_init( void )
{ 
	printk( "..init DT10 gpio driver\n" ); 
	
	gpio_port = ioremap( PHYS_GPH0, 0x100 );
	
	GPIO_DIR_OUTPUT( REG_GPH0CON, 0 );
	GPIO_DIR_OUTPUT( REG_GPH0CON, 1 );
	GPIO_DIR_OUTPUT( REG_GPH0CON, 2 );
	GPIO_DIR_OUTPUT( REG_GPH0CON, 3 );
	GPIO_DIR_OUTPUT( REG_GPH0CON, 4 );
	GPIO_DIR_OUTPUT( REG_GPH0CON, 5 );
	
	GPIO_PULLUP( REG_GPH0PUD, 0 );
	GPIO_PULLUP( REG_GPH0PUD, 1 );
	GPIO_PULLUP( REG_GPH0PUD, 2 );
	GPIO_PULLUP( REG_GPH0PUD, 3 );
	GPIO_PULLUP( REG_GPH0PUD, 4 );
	GPIO_PULLUP( REG_GPH0PUD, 5 );
	
	CLK_CS_HI;
		
	return 0;
}


/*===============================================================================
	関数名	：	dt10drv_open
	処理概要：	GPIOの初期設定を行う	
===============================================================================*/
static int dt10drv_open(struct inode *inode, struct file *filp)
{	
#if 0
	unsigned int bit ;

	/* Port A */
	bit = inl(GPIO_PADDR);
	bit |= CLK_BIT | CS_BIT;
	outl(bit, GPIO_PADDR);

	/* Port B */
	bit = inl(GPIO_PBDDR);
	bit |= DATA_BIT;
	outl(bit, GPIO_PBDDR);

	CLK_CS_HI;

	pr_debug("dt10drv: open\n");
#endif
	return 0;
}

/*===============================================================================
	関数名	：	gpio_busout
	処理概要：	dt10drv_write()関数実行時のポートHi／Lo出力部	
===============================================================================*/
static void gpio_busout( unsigned int addr, unsigned int data )
{
	CS_LOW;

	DATA_OUT(data >> 12);		// bit15-12	
	CLK_LOW;

	DATA_OUT(data >> 8);		// bit11-8	
	CLK_HI;

	DATA_OUT(data >> 4);		// bit7-4	
	CLK_LOW;

	DATA_OUT(data);				// bit3-0	
	CLK_HI;

	if( addr >= 0x00010000 )
	{
		DATA_OUT(addr >> 20);		// bit23-20	
		CLK_LOW;
		DATA_OUT(addr >> 16);		// bit19-16	
		CLK_HI;
	}

	if( addr >= 0x00000100 )
	{
		DATA_OUT(addr >> 12);		// bit15-12	
		CLK_LOW;
		DATA_OUT(addr >> 8);		// bit11-8	
		CLK_HI;
	}

	DATA_OUT(addr >> 4);			// bit7-4	
	CLK_LOW;

	DATA_OUT(addr);					// bit3-0	
	CLK_HI;

	CS_HI;

}

/*===============================================================================
	関数名	：	gpio_memoryout
	処理概要：	dt10drv_write()関数実行時のポートHi／Lo出力部	
===============================================================================*/
static void gpio_memoryout(unsigned char data)
{
	DATA_OUT(data >> 4 );			// bit7-4
	CLK_LOW;

	DATA_OUT(data);				// bit3-0
	CLK_HI;
}

/*===============================================================================
	関数名	：	gpio_busoutfast
	処理概要：	dt10drv_write()関数実行時のポートHi／Lo出力部	
===============================================================================*/
static void gpio_busoutfast(unsigned int data, unsigned int bit)
{
	CS_LOW;
	if( bit >= 9 ){
		DATA_OUT(data >> 8);
		CLK_LOW;
		DATA_OUT(data >> 4);
		CLK_HI;
		DATA_OUT(data);
		CLK_LOW;
	}else if( bit < 9 && bit >= 5 ){
		DATA_OUT(data >> 4);
		CLK_LOW;
		DATA_OUT(data);
		CLK_HI;
	}else{
		DATA_OUT(data);
		CLK_LOW;
	}

	CS_HI;
	CLK_HI;
}

/*===============================================================================
	関数名	：	gpio_eventout
	処理概要：	dt10drv_write()関数実行時のポートHi／Lo出力部	
===============================================================================*/
static void gpio_eventout(unsigned int addr, unsigned int data)
{
	CS_LOW;
	DATA_OUT(addr);
	CLK_LOW;
	CS_HI;
	CLK_HI;
	CS_LOW;
	gpio_memoryout((data));
	gpio_memoryout((data >> 8));
	gpio_memoryout(0x00);
	gpio_memoryout(0x00);
	CS_HI;
}

/*===============================================================================
	関数名	：	dt10drv_write
	処理概要：	Write命令時に実行する	
===============================================================================*/
static int dt10drv_write(struct file *filp, char *buff, size_t count, loff_t *pos)
{
	dt10_tp *tp;
	int	i;
	
	tp = (dt10_tp *)buff ;
	switch (tp->fast_mode) {
	case 0:					/* Normal Mode */
		gpio_busout(tp->addr, tp->data);
		break;
	case 1:					/* Fast Mode */
		gpio_busoutfast(tp->addr, tp->data);
		break;
	case 2:					/* Fast Mode Event Trigger */
		gpio_eventout(tp->addr, tp->data);
		break;
	default:
		break;
	}
	if( tp->size != 0 ){
		CS_LOW;
		for (i = 0; i < tp->size; i++) {
			gpio_memoryout(tp->p[i]);
		}
		CS_HI;
	}
	return count ;

}

/*===============================================================================
	関数名	：	dt10drv_release
	処理概要：	デバイスドライバのクローズ時に実行	
===============================================================================*/
static int dt10drv_release(struct inode *inode, struct file *filp)
{
	pr_debug("dt10drv: release\n");
	return 0;
}


/*===============================================================================
	関数名	：	_TP_BusOut
	処理概要：	テストポイント出力信号を発信させる
	引数	：	DT_UINT32 addr		It is used for the judgment of the InsertPoint. 
			 	DT_UINT32 dat		It is used for the judgment of the InsertPoint.
	戻り値	：	void
===============================================================================*/
void _TP_BusOut( u32 addr, u32 dat )
{
	dt10_tp	tp;	

	tp.addr		 = addr;
	tp.data		 = dat;
	tp.size		 = 0;
	tp.fast_mode = 0;

	dt10drv_write( NULL, (char *)&tp, sizeof(tp), NULL );
	
/*
	int	ret;
	dt10_tp	tp;	

	if (fd < 0) {
		_TP_BusOutOpen();
	}

	tp.addr		= addr;
	tp.data		= dat;
	tp.size		= 0;
	tp.fast_mode = 0;
	ret = write( fd, &tp, sizeof(tp) );
	if( ret < 0 )
	{
		perror("write");
	}
*/	
}
EXPORT_SYMBOL( _TP_BusOut );
/*===============================================================================
	関数名	：	_TP_MemoryOutput
	処理概要：	メモリダンプ用テストポイント関数
	引数	：	DT_UINT32 addr		It is used for the judgment of the InsertPoint. 
			 	DT_UINT32 dat		It is used for the judgment of the InsertPoint.
			 	void *value			Address of Dump-StartPoint 
			 	DT_UINT32 size		Size of DumpArea
	戻り値	：	void
===============================================================================*/
void _TP_MemoryOutput( u32 addr, u32 dat, void *value, u32 size)
{
	dt10_tp	tp;	
	int	    i;
	unsigned char *p;

	tp.addr		 = addr | DT_VARIABLE_BIT;
	tp.data		 = dat;
	tp.fast_mode = 0;
	
	if (size >= MAX_TP_DATA) size = MAX_TP_DATA;
	tp.p[0] = size;
	if (value) {
		p = (unsigned char *)value;
		for (i = 0; i < size; i++) {
			tp.p[1+i] = p[i];		// Memory Dump
		}
	}
	tp.size = size + 1;
	
	dt10drv_write( NULL, (char *)&tp, sizeof(tp), NULL );

/*
	dt10_tp	tp;	
	int	i;
	int	ret;
	unsigned char *p;

	if (fd < 0) {
		_TP_BusOutOpen();
	}

	tp.addr		= addr | DT_VARIABLE_BIT ;			// Memory Dump Start
	tp.data		= dat ;
	tp.fast_mode = 0;

	if (size >= MAX_TP_DATA) size = MAX_TP_DATA;
	tp.p[0] = size;
	if (value) {
		p = (unsigned char *)value;
		for (i = 0; i < size; i++) {
			tp.p[1+i] = p[i];		// Memory Dump
		}
	}
	tp.size = size + 1;
	ret = write( fd, &tp, sizeof(tp) );
	if( ret < 0 )
	{
		perror("write");
	}
*/	
}
EXPORT_SYMBOL( _TP_MemoryOutput );

/*===============================================================================
	関数名	：	_TP_EventTrigger
	処理概要：	イベントトリガー出力
	引数	：	DT_UINT32 addr		It is used for the judgment of the InsertSourceFile. 
			 	DT_UINT32 dat		EventData.
	戻り値	：	void
===============================================================================*/
void _TP_EventTrigger( u32 addr, u32 dat)
{
	dt10_tp	tp;	

	tp.addr		 = addr | DT_EVTTRG_BIT ;
	tp.data		 = ((unsigned short)(dat & 0x0FFF)) << 4 ;
	tp.size		 = 0;
	tp.fast_mode = 0;
	
	dt10drv_write( NULL, (char *)&tp, sizeof(tp), NULL );

/*
	dt10_tp	tp;	
	int	ret;

	if (fd < 0) {
		_TP_BusOutOpen();
	}

	tp.addr		= addr | DT_EVTTRG_BIT ;
	tp.data		= ((unsigned short)(dat & 0x0FFF)) << 4 ;
	tp.size		= 0;
	tp.fast_mode = 0;
	ret = write( fd, &tp, sizeof(tp) );
	if( ret < 0 )
	{
		perror("write");
	}
*/
}
EXPORT_SYMBOL( _TP_EventTrigger );


#if 0


/*===============================================================================
	関数名	：	file_operations driver_fops
	処理概要：	ファイル操作定義構造体	
===============================================================================*/
static struct file_operations driver_fops = {
	.write = dt10drv_write,
	.open = dt10drv_open,
	.release = dt10drv_release,
};

/*===============================================================================
	関数名	：	init_module
	処理概要：	デバイスドライバのインストール時に実行
===============================================================================*/
int init_module(void)
{
	int ret;
	dev_t dev = MKDEV(driver_major_no, 0);		
	pr_debug("dt10drv: init_module\n");

	cdev_init(&char_dev, &driver_fops);
	char_dev.owner = THIS_MODULE;
	ret = cdev_add(&char_dev, dev, 1);
	if (ret < 0) {
		pr_debug("dt10drv: Major no. cannot be assigned.\n");
		return ret;
	}
	if (driver_major_no == 0) {
		driver_major_no = ret;
		printk( "dt10drv: Major no. is assigned to %d.\n", ret);
	}
	return 0;
}

/*===============================================================================
	関数名	：	cleanup_module
	処理概要：	デバイスドライバのアンインストール時に実行
===============================================================================*/
void cleanup_module(void)
{
	pr_debug("dt10drv: cleanup_module\n");
	cdev_del(&char_dev);
}


MODULE_LICENSE(LICENSE);

#endif


module_init( dt10_gpio_init );


