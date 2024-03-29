/* linux/drivers/mtd/nand/s3c_nand.c
 *
 * Copyright (c) 2009 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>

#include <asm/io.h>

#include <plat/regs-nand.h>
#include <plat/nand.h>

#ifdef CONFIG_MTD_CMDLINE_PARTS
static const char *part_probes[] = { "cmdlinepart", NULL };
#endif

#ifdef CONFIG_FALINUX_ZEROBOOT
#include "nalcode_storage.h"
#endif

struct mtd_partition s3c_partition_info[] = {
	{
		.name		= "bootloader+config+logo",
		.offset		= 0,
		.size		= (4*SZ_1M),
		//.mask_flags	= MTD_CAP_NANDFLASH,	// read only
	},
	{
		.name		= "kernel+ramdisk",
		.offset		= MTDPART_OFS_APPEND,
		.size		= (13*SZ_1M),
	},
	{
		.name		= "root",
		.offset		= (17*SZ_1M),				// MTDPART_OFS_APPEND,
		.size		= MTDPART_SIZ_FULL,
	},
	{
		.name		= "system",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 0,
	},
	{
		.name		= "userdata",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 0,
	},
	{
		.name		= "cache",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 0,
	},
	{
		.name		= "snapshot",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 0,
	},
	{
		.name		= "app6",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 0,
	}
};

struct s3c_nand_mtd_info s3c_nand_mtd_part_info = {
	.chip_nr     = 1,
	.mtd_part_nr = 3,
	.partition   = s3c_partition_info,
};

static int __init setup_nand_part( char *s )
{
	size_t  size, offset;
	int     count;
	char   *ptr, *nptr;

	count  = 0;	
	offset = 0;
	ptr    = s;
	
	while(1)
	{
		switch( count )
		{
		// "bootloader+config+logo"
		case 0 : 
			size  = simple_strtoul( ptr, &nptr, 0 ); if (nptr) ptr = nptr + 1;
			size += simple_strtoul( ptr, &nptr, 0 ); if (nptr) ptr = nptr + 1;
			size += simple_strtoul( ptr, &nptr, 0 ); if (nptr) ptr = nptr + 1;		
			break;
		// "kernel+ramdisk"
		case 1 :
			size  = simple_strtoul( ptr, &nptr, 0 ); if (nptr) ptr = nptr + 1;
			size += simple_strtoul( ptr, &nptr, 0 ); if (nptr) ptr = nptr + 1;
			break;			
		default:
			size  = simple_strtoul( ptr, &nptr, 0 );
			break;
		}
		
		s3c_partition_info[count].offset = offset;
		
		if ( size == 0 )
			s3c_partition_info[count].size = MTDPART_SIZ_FULL;
		else
			s3c_partition_info[count].size = size * SZ_1M;
		
		offset += s3c_partition_info[count].size;
		count ++;
		
		if ( size == 0    ) break;
		if ( NULL == nptr ) break;
		if ( ',' != *nptr ) break;
		if ( count >= ARRAY_SIZE(s3c_partition_info) ) break;
			
		ptr = nptr + 1;
	} 
	
	// 파티션 개수
	s3c_nand_mtd_part_info.mtd_part_nr = count;

	return 1;
}
__setup( "nandparts=", setup_nand_part );



enum s3c_cpu_type {
	TYPE_S3C2450,	/* including s3c2416 */
	TYPE_S3C6400,
	TYPE_S3C6410,	/* including s3c6430/31 */
	TYPE_S5P6440,
	TYPE_S5PC100,
	TYPE_S5PC110,
	TYPE_S5PV210,
};

struct s3c_nand_info {
	/* mtd info */
	struct nand_hw_control		controller;
	struct s3c_nand_mtd_info	*mtds;
	struct s3c2410_platform_nand	*platform;

	/* device info */
	struct device			*device;
	struct resource			*area;
	struct clk			*clk;
	void __iomem			*regs;
	void __iomem			*sel_reg;
	int				sel_bit;
	int				mtd_count;

	enum s3c_cpu_type		cpu_type;
};
static struct s3c_nand_info s3c_nand;

struct mtd_info *s3c_mtd_list[4] = { NULL, NULL, NULL, NULL };
EXPORT_SYMBOL( s3c_mtd_list );

/* Nand flash definition values by jsgood */
#define S3C_NAND_TYPE_UNKNOWN	0x0
#define S3C_NAND_TYPE_SLC	0x1
#define S3C_NAND_TYPE_MLC	0x2

/*
 * Cached progamming disabled for now, Not sure if its worth the
 * trouble. The speed gain is not very impressive. (2.3->2.6Mib/s)
 *
 * if want to use cached program, define next
 * by jsgood (modified to keep prevent rule)
 */
#undef	CONFIG_MTD_NAND_S3C_CACHEDPROG

/* Nand flash global values by jsgood */
int cur_ecc_mode = 0;
int nand_type = S3C_NAND_TYPE_UNKNOWN;

#if defined(CONFIG_MTD_NAND_S3C_HWECC)
/* Nand flash oob definition for SLC 512b page size by jsgood */
static struct nand_ecclayout s3c_nand_oob_16 = {
	.eccbytes = 4,
	.eccpos = {1, 2, 3, 4},
	.oobfree = {
		{.offset = 6,
		 .length = 10}}
};

/* Nand flash oob definition for SLC 2k page size by jsgood */
static struct nand_ecclayout s3c_nand_oob_64 = {
	.eccbytes = 16,
	.eccpos = {40, 41, 42, 43, 44, 45, 46, 47,
		   48, 49, 50, 51, 52, 53, 54, 55},
	.oobfree = {
		{.offset = 2,
		 .length = 38}}
};

/* Nand flash oob definition for S5PV210 MCU SLC 2k page size by JaeKyong.OH  */
static struct nand_ecclayout s3c_nand_oob_64_for_ecc8 = {
	.eccbytes = 13*4,
	.eccpos = {         12, 13, 14, 15,
    	16, 17, 18, 19, 20, 21, 22, 23,
    	24, 25, 26, 27, 28, 29, 30, 31,
		32, 33, 34, 35, 36, 37, 38, 39,
		40, 41, 42, 43, 44, 45, 46, 47,
		48, 49, 50, 51, 52, 53, 54, 55,
		56, 57, 58, 59, 60, 61, 62, 63 },
	.oobfree = {
		{.offset = 2,
		 .length = 10}}
};

/* Nand flash oob definition for MLC 2k page size by jsgood */
static struct nand_ecclayout s3c_nand_oob_mlc_64 = {
	.eccbytes = 32,
	.eccpos = {
		   32, 33, 34, 35, 36, 37, 38, 39,
		   40, 41, 42, 43, 44, 45, 46, 47,
 		   48, 49, 50, 51, 52, 53, 54, 55,
   		   56, 57, 58, 59, 60, 61, 62, 63},
	.oobfree = {
		{.offset = 2,
		 .length = 28}}
};

/* Nand flash oob definition for 4Kb page size with 8_bit ECC */
static struct nand_ecclayout s3c_nand_oob_128 = {
        .eccbytes = 104,
        .eccpos = {
                   24, 25, 26, 27, 28, 29, 30, 31,
                   32, 33, 34, 35, 36, 37, 38, 39,
                   40, 41, 42, 43, 44, 45, 46, 47,
                   48, 49, 50, 51, 52, 53, 54, 55,
                   56, 57, 58, 59, 60, 61, 62, 63,
                   64, 65, 66, 67, 68, 69, 70, 71,
                   72, 73, 74, 75, 76, 77, 78, 79,
                   80, 81, 82, 83, 84, 85, 86, 87,
                   88, 89, 90, 91, 92, 93, 94, 95,
                   96, 97, 98, 99, 100, 101, 102, 103,
                   104, 105, 106, 107, 108, 109, 110, 111,
                   112, 113, 114, 115, 116, 117, 118, 119,
                   120, 121, 122, 123, 124, 125, 126, 127},
        .oobfree = {
                {.offset = 2,
                 .length = 22}}
};
#endif

#if defined(CONFIG_MTD_NAND_S3C_DEBUG)
/*
 * Function to print out oob buffer for debugging
 * Written by jsgood
 */
void print_oob(const char *header, struct mtd_info *mtd)
{
	int i;
	struct nand_chip *chip = mtd->priv;

	printk("%s:\t", header);

	for(i = 0; i < 64; i++)
		printk("%02x ", chip->oob_poi[i]);

	printk("\n");
}
EXPORT_SYMBOL(print_oob);
#endif


/*
 * Hardware specific access to control-lines function
 * Written by jsgood
 
 todo. nFCE 를 선택할 수 있어야 한다.
       각각의 함수를 만들면 될것 같다.
 */
static void s3c_nand_hwcontrol(struct mtd_info *mtd, int dat, unsigned int ctrl)
{
	unsigned int cur;
	void __iomem *regs = s3c_nand.regs;

	if (ctrl & NAND_CTRL_CHANGE) {
		if (ctrl & NAND_NCE) {
			if (dat != NAND_CMD_NONE) {
				cur = readl(regs + S3C_NFCONT);
				cur &= ~S3C_NFCONT_nFCE0;
				writel(cur, regs + S3C_NFCONT);
			}
		} else {
			cur = readl(regs + S3C_NFCONT);
			cur |= S3C_NFCONT_nFCE0;
			writel(cur, regs + S3C_NFCONT);
		}
	}

	if (dat != NAND_CMD_NONE) {
		
#ifdef CONFIG_FALINUX_ZEROBOOT
		goto safe_label_2;
safe_label_1:
		switch( dat )
		{
		case NAND_CMD_PAGEPROG:  
			if ( BUF_STORAGE_ACCESS == STORAGE_ACCESS_MARK_NALCODE )
			{
				ZEROBOOT_DBG( "  dirty NAND_CMD_PAGEPROG\n" ); 
				return;
			}
			break;			
		case NAND_CMD_CACHEDPROG :
			if ( BUF_STORAGE_ACCESS == STORAGE_ACCESS_MARK_NALCODE )
			{
				ZEROBOOT_DBG( "  dirty NAND_CMD_CACHEDPROG\n" ); 
				return;
			}
			break;			
	case NAND_CMD_ERASE1  :
		case NAND_CMD_ERASE2  :
			if ( BUF_STORAGE_ACCESS == STORAGE_ACCESS_MARK_NALCODE )
			{
				ZEROBOOT_DBG( "  dirty NAND_CMD_ERASE\n" ); 
				return;
			}
			break;
		}
		
		if (ctrl & NAND_CLE)
			writeb(dat, regs + S3C_NFCMMD);
		else if (ctrl & NAND_ALE)
			writeb(dat, regs + S3C_NFADDR);
		return;
		
safe_label_2 : goto safe_label_1;		

#else
		if (ctrl & NAND_CLE)
			writeb(dat, regs + S3C_NFCMMD);
		else if (ctrl & NAND_ALE)
			writeb(dat, regs + S3C_NFADDR);
#endif
	}
}

/*
 * Function for checking device ready pin
 * Written by jsgood
 */
static int s3c_nand_device_ready(struct mtd_info *mtd)
{
	void __iomem *regs = s3c_nand.regs;

	/* it's to check the RnB nand signal bit and
	 * return to device ready condition in nand_base.c
	 */
	return ((readl(regs + S3C_NFSTAT) & S3C_NFSTAT_READY));
}

/*
 * We don't use a bad block table
 */
static int s3c_nand_scan_bbt(struct mtd_info *mtdinfo)
{
	return 0;
}

#if defined(CONFIG_MTD_NAND_S3C_HWECC)
#if 0
/*
 * S3C Nand flash chip enable function
 * Written by jsgood
 */
static void s3c_nand_ce_on(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;

	chip->cmd_ctrl(mtd, 0x0, NAND_NCE | NAND_CTRL_CHANGE);
	nand_wait_ready(mtd);
}

/*
 * S3C Nand flash chip disable function
 * Written by jsgood
 */
static void s3c_nand_ce_off(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;

	chip->cmd_ctrl(mtd, NAND_CMD_NONE, NAND_CTRL_CHANGE);
	nand_wait_ready(mtd);
}
#endif

/*
 * Function for checking ECCEncDone in NFSTAT
 * Written by jsgood
 */
static void s3c_nand_wait_enc(void)
{
	void __iomem *regs = s3c_nand.regs;
	unsigned long timeo = jiffies;
	
    timeo += (10*HZ)/1000;    
	
	/* Apply this short delay always to ensure that we do wait tWB in
	 * any case on any machine. */
	
	while (time_before(jiffies, timeo)) {
		if (readl(regs + S3C_NFSTAT) & S3C_NFSTAT_ECCENCDONE)
				break;
		cond_resched();
	}
}

/*
 * Function for checking ECCDecDone in NFSTAT
 * Written by jsgood
 */
static void s3c_nand_wait_dec(void)
{
	void __iomem *regs = s3c_nand.regs;
    unsigned long timeo = jiffies;
 
    timeo += (10*HZ)/1000;    
 
    /* Apply this short delay always to ensure that we do wait tWB in
     * any case on any machine. */

    while (time_before(jiffies, timeo)) {
		if (readl(regs + S3C_NFSTAT) & S3C_NFSTAT_ECCDECDONE)
			break;
		cond_resched();
	}
}

/*
 * Function for checking ECCEncDone in NFCSTAT
 * Written by jsgood
 */
static void s3c_nand_wait_enc_8bit(void)
{
	void __iomem *regs = s3c_nand.regs;
	unsigned long timeo = jiffies;
	
	timeo += (10*HZ)/1000;    
	
	/* Apply this short delay always to ensure that we do wait tWB in
	 * any case on any machine. */
	
	while (time_before(jiffies, timeo)) {
		if (readl(regs + S3C_NFECCSTAT) & S3C_NFECSTAT_ECCENCDONE )
				break;
		cond_resched();
	}
}

/*
 * Function for checking ECCDecDone in NFCSTAT
 * Written by jsgood
 */
static void s3c_nand_wait_dec_8bit(void)
{
	void __iomem *regs = s3c_nand.regs;
	unsigned long timeo = jiffies;
 
	timeo += (10*HZ)/1000;    /* when Hz=200,  jiffies interval  1/200=5mS, waiting for 80mS  80/5 = 16 */
 
	/* Apply this short delay always to ensure that we do wait tWB in
	 * any case on any machine. */

	while (time_before(jiffies, timeo)) {
		if (readl(regs + S3C_NFECCSTAT) & S3C_NFECSTAT_ECCDECDONE )
			break;
		cond_resched();
	}
}


/*
 * Function for checking ECC Busy
 * Written by jsgood
 */
static void s3c_nand_wait_ecc_busy(void)
{
	void __iomem *regs = s3c_nand.regs;
	unsigned long timeo = jiffies;
 
	timeo += (10*HZ)/1000;    
	
	/* Apply this short delay always to ensure that we do wait tWB in
	 * any case on any machine. */
	
	while (time_before(jiffies, timeo)) {
		if (!(readl(regs + S3C_NFMECCERR0) & S3C_NFECCERR0_ECCBUSY))
			break;
		cond_resched();
	}
}

/*
 * This function is called before encoding ecc codes to ready ecc engine.
 * Written by jsgood
 */
static void s3c_nand_enable_hwecc(struct mtd_info *mtd, int mode)
{
	u_long nfcont;
	u_long nfconf;
	void __iomem *regs = s3c_nand.regs;

	cur_ecc_mode = mode;

	nfconf = readl(regs + S3C_NFCONF);

	if (s3c_nand.cpu_type == TYPE_S3C6400) {
		if (nand_type == S3C_NAND_TYPE_SLC)
			nfconf &= ~S3C_NFCONF_ECC_MLC;	/* SLC */
		else
			nfconf |= S3C_NFCONF_ECC_MLC;	/* MLC */
	} else {
		nfconf &= ~(0x3 << 23);

		if (nand_type == S3C_NAND_TYPE_SLC)
			nfconf |= S3C_NFCONF_ECC_1BIT;
		else
			nfconf |= S3C_NFCONF_ECC_4BIT;
	}

	writel(nfconf, regs + S3C_NFCONF);

	/* Init main ECC & unlock */
	nfcont  = readl(regs + S3C_NFCONT);
	nfcont |= S3C_NFCONT_INITMECC;		
	nfcont &= ~S3C_NFCONT_MECCLOCK;

	if (nand_type == S3C_NAND_TYPE_MLC) {
		if (mode == NAND_ECC_WRITE)
			nfcont |= S3C_NFCONT_ECC_ENC;
		else if (mode == NAND_ECC_READ)
			nfcont &= ~S3C_NFCONT_ECC_ENC;
	}

	writel(nfcont, regs + S3C_NFCONT);
}

/*
 * This function is called immediately after encoding ecc codes.
 * This function returns encoded ecc codes.
 * Written by jsgood
 */
static int s3c_nand_calculate_ecc(struct mtd_info *mtd, const u_char *dat, u_char *ecc_code)
{
	u_long nfcont, nfmecc0, nfmecc1;
	void __iomem *regs = s3c_nand.regs;

	/* Lock */
	nfcont = readl(regs + S3C_NFCONT);
	nfcont |= S3C_NFCONT_MECCLOCK;
	writel(nfcont, regs + S3C_NFCONT);

	if (nand_type == S3C_NAND_TYPE_SLC) {
		nfmecc0 = readl(regs + S3C_NFMECC0);

		ecc_code[0] = nfmecc0 & 0xff;
		ecc_code[1] = (nfmecc0 >> 8) & 0xff;
		ecc_code[2] = (nfmecc0 >> 16) & 0xff;
		ecc_code[3] = (nfmecc0 >> 24) & 0xff;
	} else {
		if (cur_ecc_mode == NAND_ECC_READ)
			s3c_nand_wait_dec();
		else {
			s3c_nand_wait_enc();
			
			nfmecc0 = readl(regs + S3C_NFMECC0);
			nfmecc1 = readl(regs + S3C_NFMECC1);

			ecc_code[0] = nfmecc0 & 0xff;
			ecc_code[1] = (nfmecc0 >> 8) & 0xff;
			ecc_code[2] = (nfmecc0 >> 16) & 0xff;
			ecc_code[3] = (nfmecc0 >> 24) & 0xff;			
			ecc_code[4] = nfmecc1 & 0xff;
			ecc_code[5] = (nfmecc1 >> 8) & 0xff;
			ecc_code[6] = (nfmecc1 >> 16) & 0xff;
			ecc_code[7] = (nfmecc1 >> 24) & 0xff;
		}
	}
	
	return 0;
}

/*
 * This function determines whether read data is good or not.
 * If SLC, must write ecc codes to controller before reading status bit.
 * If MLC, status bit is already set, so only reading is needed.
 * If status bit is good, return 0.
 * If correctable errors occured, do that.
 * If uncorrectable errors occured, return -1.
 * Written by jsgood
 */
static int s3c_nand_correct_data(struct mtd_info *mtd, u_char *dat, u_char *read_ecc, u_char *calc_ecc)
{
	int ret = -1;
	u_long nfestat0, nfestat1, nfmeccdata0, nfmeccdata1, nfmlcbitpt;
	u_char err_type;
	void __iomem *regs = s3c_nand.regs;

	if (!dat) {
		printk("No page data\n");
		return ret;
	}

	if (nand_type == S3C_NAND_TYPE_SLC) {
		/* SLC: Write ECC data to compare */
		nfmeccdata0 = (read_ecc[1] << 16) | read_ecc[0];
		nfmeccdata1 = (read_ecc[3] << 16) | read_ecc[2];
		writel(nfmeccdata0, regs + S3C_NFMECCDATA0);
		writel(nfmeccdata1, regs + S3C_NFMECCDATA1);

		/* Read ECC status */
		nfestat0 = readl(regs + S3C_NFMECCERR0);
		err_type = nfestat0 & 0x3;

		switch (err_type) {
		case 0: /* No error */
			ret = 0;
			break;

		case 1: /* 1 bit error (Correctable)
			   (nfestat0 >> 7) & 0x7ff	:error byte number
			   (nfestat0 >> 4) & 0x7	:error bit number */
#ifdef CONFIG_FALINUX_ZEROBOOT 
			if ( 0 == check_zeroboot_state_normal() )
#endif				
			printk("s3c-nand: 1 bit error detected at byte %ld, correcting from "
					"0x%02x ", (nfestat0 >> 7) & 0x7ff, dat[(nfestat0 >> 7) & 0x7ff]);
			dat[(nfestat0 >> 7) & 0x7ff] ^= (1 << ((nfestat0 >> 4) & 0x7));
#ifdef CONFIG_FALINUX_ZEROBOOT 
			if ( 0 == check_zeroboot_state_normal() )
#endif				
			printk("to 0x%02x...OK\n", dat[(nfestat0 >> 7) & 0x7ff]);
			ret = 1;
			break;

		case 2: /* Multiple error */
		case 3: /* ECC area error */
#ifdef CONFIG_FALINUX_ZEROBOOT 
			if ( 0 == check_zeroboot_state_normal() )
#endif				
			printk("s3c-nand: ECC uncorrectable error detected\n");
			ret = -1;
			break;
		}
	} else {
		/* MLC: */
		s3c_nand_wait_ecc_busy();
		
		nfestat0 = readl(regs + S3C_NFMECCERR0);
		nfestat1 = readl(regs + S3C_NFMECCERR1);
		nfmlcbitpt = readl(regs + S3C_NFMLCBITPT);

		err_type = (nfestat0 >> 26) & 0x7;

		/* No error, If free page (all 0xff) */
		if ((nfestat0 >> 29) & 0x1) {
			err_type = 0;
		} else {
			/* No error, If all 0xff from 17th byte in oob (in case of JFFS2 format) */
			if (dat) {
				if (dat[17] == 0xff && dat[26] == 0xff && dat[35] == 0xff && dat[44] == 0xff && dat[54] == 0xff)
					err_type = 0;
			}
		}

		switch (err_type) {
		case 5: /* Uncorrectable */
			printk("s3c-nand: ECC uncorrectable error detected\n");
			ret = -1;
			break;

		case 4: /* 4 bit error (Correctable) */
			dat[(nfestat1 >> 16) & 0x3ff] ^= ((nfmlcbitpt >> 24) & 0xff);

		case 3: /* 3 bit error (Correctable) */
			dat[nfestat1 & 0x3ff] ^= ((nfmlcbitpt >> 16) & 0xff);

		case 2: /* 2 bit error (Correctable) */
			dat[(nfestat0 >> 16) & 0x3ff] ^= ((nfmlcbitpt >> 8) & 0xff);

		case 1: /* 1 bit error (Correctable) */
			printk("s3c-nand: %d bit(s) error detected, corrected successfully\n", err_type);
			dat[nfestat0 & 0x3ff] ^= (nfmlcbitpt & 0xff);
			ret = err_type;
			break;

		case 0: /* No error */
			ret = 0;
			break;
		}
	}

	return ret;
}

static int s3c_nand_write_oob_1bit(struct mtd_info *mtd, struct nand_chip *chip,
			      int page)
{
	uint8_t *ecc_calc = chip->buffers->ecccalc;
	int status = 0;
	int eccbytes = chip->ecc.bytes;
	int secc_start = mtd->oobsize - eccbytes;
	int i;

	chip->cmdfunc(mtd, NAND_CMD_SEQIN, mtd->writesize, page);

	/* spare area */
	chip->ecc.hwctl(mtd, NAND_ECC_WRITE);
	chip->write_buf(mtd, chip->oob_poi, secc_start);
	chip->ecc.calculate(mtd, 0, &ecc_calc[chip->ecc.total]);

	for (i = 0; i < eccbytes; i++)
		chip->oob_poi[secc_start + i] = ecc_calc[chip->ecc.total + i];

	chip->write_buf(mtd, chip->oob_poi + secc_start, eccbytes);

	/* Send command to program the OOB data */
	chip->cmdfunc(mtd, NAND_CMD_PAGEPROG, -1, -1);

	status = chip->waitfunc(mtd, chip);

	return status & NAND_STATUS_FAIL ? -EIO : 0;
}

static int s3c_nand_read_oob_1bit(struct mtd_info *mtd, struct nand_chip *chip,
			     int page, int sndcmd)
{
	uint8_t *ecc_calc = chip->buffers->ecccalc;
	int eccbytes = chip->ecc.bytes;
	int secc_start = mtd->oobsize - eccbytes;
	
	if (sndcmd) {
		chip->cmdfunc(mtd, NAND_CMD_READOOB, 0, page);
		sndcmd = 0;
	}

	chip->ecc.hwctl(mtd, NAND_ECC_READ);
	chip->read_buf(mtd, chip->oob_poi, secc_start);
	chip->ecc.calculate(mtd, 0, &ecc_calc[chip->ecc.total]);
	chip->read_buf(mtd, chip->oob_poi + secc_start, eccbytes);

#if 0
	/* jffs2 special case */
	if (!(chip->oob_poi[2] == 0x85 && chip->oob_poi[3] == 0x19))
		chip->ecc.correct(mtd, chip->oob_poi, chip->oob_poi + secc_start, 0);
#endif

	return sndcmd;
}

static void s3c_nand_write_page_1bit(struct mtd_info *mtd, struct nand_chip *chip,
				  const uint8_t *buf)
{
	int i, eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	int secc_start = mtd->oobsize - eccbytes;
	uint8_t *ecc_calc = chip->buffers->ecccalc;
	const uint8_t *p = buf;
	
	uint32_t *eccpos = chip->ecc.layout->eccpos;

	/* main area */
	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {
		chip->ecc.hwctl(mtd, NAND_ECC_WRITE);
		chip->write_buf(mtd, p, eccsize);
		chip->ecc.calculate(mtd, p, &ecc_calc[i]);
	}

	for (i = 0; i < chip->ecc.total; i++)
		chip->oob_poi[eccpos[i]] = ecc_calc[i];

	/* spare area */
	chip->ecc.hwctl(mtd, NAND_ECC_WRITE);
	chip->write_buf(mtd, chip->oob_poi, secc_start);
	chip->ecc.calculate(mtd, p, &ecc_calc[chip->ecc.total]);

	for (i = 0; i < eccbytes; i++)
		chip->oob_poi[secc_start + i] = ecc_calc[chip->ecc.total + i];

	chip->write_buf(mtd, chip->oob_poi + secc_start, eccbytes);
}

static int s3c_nand_read_page_1bit(struct mtd_info *mtd, struct nand_chip *chip,
				uint8_t *buf, int sndcmd)
{
	int i, stat, eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	int secc_start = mtd->oobsize - eccbytes;
	int col = 0;
	uint8_t *p = buf;	
	uint32_t *mecc_pos = chip->ecc.layout->eccpos;
	uint8_t *ecc_calc = chip->buffers->ecccalc;

	col = mtd->writesize;
	chip->cmdfunc(mtd, NAND_CMD_RNDOUT, col, -1);

	/* spare area */
	chip->ecc.hwctl(mtd, NAND_ECC_READ);
	chip->read_buf(mtd, chip->oob_poi, secc_start);
	chip->ecc.calculate(mtd, p, &ecc_calc[chip->ecc.total]);
	chip->read_buf(mtd, chip->oob_poi + secc_start, eccbytes);

#if 0
	/* jffs2 special case */
	if (!(chip->oob_poi[2] == 0x85 && chip->oob_poi[3] == 0x19))
		chip->ecc.correct(mtd, chip->oob_poi, chip->oob_poi + secc_start, 0);
#endif

	col = 0;

	/* main area */
	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {
		chip->cmdfunc(mtd, NAND_CMD_RNDOUT, col, -1);
		chip->ecc.hwctl(mtd, NAND_ECC_READ);
		chip->read_buf(mtd, p, eccsize);
		chip->ecc.calculate(mtd, p, &ecc_calc[i]);

		stat = chip->ecc.correct(mtd, p, chip->oob_poi + mecc_pos[0] + ((chip->ecc.steps - eccsteps) * eccbytes), 0);
		if (stat == -1)
			mtd->ecc_stats.failed++;

		col = eccsize * (chip->ecc.steps + 1 - eccsteps);
	}
	
	return 0;
}


/* 
 * Hardware specific page read function for MLC.
 * Written by jsgood
 */
static int s3c_nand_read_page_4bit(struct mtd_info *mtd, struct nand_chip *chip,
				uint8_t *buf, int sndcmd)
{
	int i, stat, eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	int col = 0;
	uint8_t *p = buf;	
	uint32_t *mecc_pos = chip->ecc.layout->eccpos;

	/* Step1: read whole oob */
	col = mtd->writesize;
	chip->cmdfunc(mtd, NAND_CMD_RNDOUT, col, -1);
	chip->read_buf(mtd, chip->oob_poi, mtd->oobsize);

	col = 0;
	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {
		chip->cmdfunc(mtd, NAND_CMD_RNDOUT, col, -1);
		chip->ecc.hwctl(mtd, NAND_ECC_READ);
		chip->read_buf(mtd, p, eccsize);
		chip->write_buf(mtd, chip->oob_poi + mecc_pos[0] + ((chip->ecc.steps - eccsteps) * eccbytes), eccbytes);
		chip->ecc.calculate(mtd, 0, 0);
		stat = chip->ecc.correct(mtd, p, 0, 0);

		if (stat == -1)
			mtd->ecc_stats.failed++;

		col = eccsize * (chip->ecc.steps + 1 - eccsteps);
	}

	return 0;
}

/* 
 * Hardware specific page write function for MLC.
 * Written by jsgood
 */
static void s3c_nand_write_page_4bit(struct mtd_info *mtd, struct nand_chip *chip,
				  const uint8_t *buf)
{
	int i, eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	const uint8_t *p = buf;
	uint8_t *ecc_calc = chip->buffers->ecccalc;
	uint32_t *mecc_pos = chip->ecc.layout->eccpos;

	/* Step1: write main data and encode mecc */
	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {
		chip->ecc.hwctl(mtd, NAND_ECC_WRITE);
		chip->write_buf(mtd, p, eccsize);
		chip->ecc.calculate(mtd, p, &ecc_calc[i]);
	}

	/* Step2: save encoded mecc */
	for (i = 0; i < chip->ecc.total; i++)
		chip->oob_poi[mecc_pos[i]] = ecc_calc[i];

	chip->write_buf(mtd, chip->oob_poi, mtd->oobsize);
}


static void s3c_nand_wait_ecc_busy_8bit(void)
{
	void __iomem *regs = s3c_nand.regs;
	while (readl(regs + S3C_NF8ECCERR0) & S3C_NFECCERR0_ECCBUSY) {}
}

void s3c_nand_enable_hwecc_8bit(struct mtd_info *mtd, int mode)
{
	u_long nfcont, nfconf, nfreg;
	void __iomem *regs = s3c_nand.regs;

	cur_ecc_mode = mode;

	// hwecc 8-bit selection 
	{
		nfconf = readl(regs + S3C_NFCONF);
		nfconf &= ~(0x3 << 23);
		nfconf |= S3C_NFCONF_ECC_8BIT; 
		writel(nfconf, (regs + S3C_NFCONF));
	}

	// set 8/12/16bit ECC message length
	{
		nfreg = readl(regs + S3C_NFECCCONF);
		nfreg &= ~( (0x3ff<<16) | (0xf) );
		nfreg |=  ( (511 << 16) | (0x3) );
		writel(nfreg, (regs + S3C_NFECCCONF));
	}

	// enable
	{
		nfcont = readl(regs + S3C_NFCONT);	
		nfcont  &= ~(S3C_NFCONT_LOCKTGHT|S3C_NFCONT_LOCKSOFT);
		nfcont  |= 1;	// Enable
		writel(nfcont, (regs + S3C_NFCONT));
	}

	if (mode == NAND_ECC_WRITE)
	{
		// set 8/12/16bit Ecc direction to Encoding
		// Reset ECC value.
		nfreg = readl(regs + S3C_NFECCCONT);	
		nfreg |= (0x1 << 16);		// Encoding
		nfreg |= (0x1 << 2 );		// Reset ECC value.
		writel(nfreg, (regs + S3C_NFECCCONT));

		// Initialize & unlock
		nfreg  = readl(regs + S3C_NFCONT);	
		nfreg |=  S3C_NFCONT_INITECC;
		nfreg &= ~S3C_NFCONT_MECCLOCK;
		nfreg |=  S3C_NFCONT_ECC_ENC;	// Encoding
		writel(nfreg, (regs + S3C_NFCONT));
	}
	else
	{
		// set 8/12/16bit ECC direction
		// Reset ECC value.
		nfreg = readl(regs + S3C_NFECCCONT);	
		nfreg &= ~(0x1 << 16); 		// Decoding
		nfreg |=  (0x1 << 2 ); 		// Reset ECC value.
		writel(nfreg, (regs + S3C_NFECCCONT));

		// Initialize & unlock
		nfreg  = readl(regs + S3C_NFCONT);	
		nfreg |=  S3C_NFCONT_INITECC;
		nfreg &= ~S3C_NFCONT_MECCLOCK;
		nfreg &= ~S3C_NFCONT_ECC_ENC;	// Decoding
		writel(nfreg, (regs + S3C_NFCONT));
	}
}

int s3c_nand_calculate_ecc_8bit(struct mtd_info *mtd, const u_char *dat, u_char *ecc_code)
{
	u_long nfcont, nfm8ecc0, nfm8ecc1, nfm8ecc2, nfm8ecc3, nfreg;
	void __iomem *regs = s3c_nand.regs;

	/* Lock */
	nfcont = readl(regs + S3C_NFCONT);
	nfcont |= S3C_NFCONT_MECCLOCK;
	writel(nfcont, (regs + S3C_NFCONT));

	if (cur_ecc_mode == NAND_ECC_READ)
	{
		s3c_nand_wait_dec_8bit();
	}
	else 
	{
		s3c_nand_wait_enc_8bit();

		// clear 8/12/16bit ecc encode done
		{
			nfreg = readl(regs + S3C_NFECCSTAT);
			nfreg |= (1<<25);
			writel(nfreg, (regs + S3C_NFECCSTAT));
		}

		nfm8ecc0 = readl(regs + S3C_NFECCPRGECC0);
		nfm8ecc1 = readl(regs + S3C_NFECCPRGECC1);
		nfm8ecc2 = readl(regs + S3C_NFECCPRGECC2);
		nfm8ecc3 = readl(regs + S3C_NFECCPRGECC3);

		ecc_code[0] =  nfm8ecc0        & 0xff;
		ecc_code[1] = (nfm8ecc0 >> 8 ) & 0xff;
		ecc_code[2] = (nfm8ecc0 >> 16) & 0xff;
		ecc_code[3] = (nfm8ecc0 >> 24) & 0xff;
		ecc_code[4] =  nfm8ecc1        & 0xff;
		ecc_code[5] = (nfm8ecc1 >> 8)  & 0xff;
		ecc_code[6] = (nfm8ecc1 >> 16) & 0xff;
		ecc_code[7] = (nfm8ecc1 >> 24) & 0xff;
		ecc_code[8] =  nfm8ecc2        & 0xff;
		ecc_code[9] = (nfm8ecc2 >> 8)  & 0xff;
		ecc_code[10]= (nfm8ecc2 >> 16) & 0xff;
		ecc_code[11]= (nfm8ecc2 >> 24) & 0xff;
		ecc_code[12]=  nfm8ecc3        & 0xff;
	}

	return 0;
}

int s3c_nand_correct_data_8bit(struct mtd_info *mtd, u_char *dat, u_char *read_ecc, u_char *calc_ecc)
{
	int ret = -1;
	u_long nf8eccerr0, nf8eccerr1, nf8eccerr2, nfmlc8bitpt0, nfmlc8bitpt1;
	u_char err_type;
	void __iomem *regs = s3c_nand.regs;

	s3c_nand_wait_ecc_busy_8bit();

	nf8eccerr0 = readl(regs + S3C_NF8ECCERR0);
	nf8eccerr1 = readl(regs + S3C_NF8ECCERR1);
	nf8eccerr2 = readl(regs + S3C_NF8ECCERR2);
	nfmlc8bitpt0 = readl(regs + S3C_NFMLC8BITPT0);
	nfmlc8bitpt1 = readl(regs + S3C_NFMLC8BITPT1);

	err_type = (nf8eccerr0 >> 25) & 0xf;

	/* No error, If free page (all 0xff) */
        /* While testing, it was found that NFECCERR0[29] bit is set even if
         * the page contents were not zero. So this code is commented */
        /*if ((nf8eccerr0 >> 29) & 0x1)
 	         err_type = 0;*/

	switch (err_type) {
	case 9: /* Uncorrectable */
#ifdef CONFIG_FALINUX_ZEROBOOT 
			if ( 0 == check_zeroboot_state_normal() )
#endif				
		printk("s3c-nand: ECC uncorrectable error detected\n");
		ret = -1;
		break;

	case 8: /* 8 bit error (Correctable) */
		dat[(nf8eccerr2 >> 22) & 0x3ff] ^= ((nfmlc8bitpt1 >> 24) & 0xff);

	case 7: /* 7 bit error (Correctable) */
		dat[(nf8eccerr2 >> 11) & 0x3ff] ^= ((nfmlc8bitpt1 >> 16) & 0xff);

	case 6: /* 6 bit error (Correctable) */
		dat[nf8eccerr2 & 0x3ff] ^= ((nfmlc8bitpt1 >> 8) & 0xff);

	case 5: /* 5 bit error (Correctable) */
		dat[(nf8eccerr1 >> 22) & 0x3ff] ^= (nfmlc8bitpt1 & 0xff);

	case 4: /* 4 bit error (Correctable) */
		dat[(nf8eccerr1 >> 11) & 0x3ff] ^= ((nfmlc8bitpt0 >> 24) & 0xff);

	case 3: /* 3 bit error (Correctable) */
		dat[nf8eccerr1 & 0x3ff] ^= ((nfmlc8bitpt0 >> 16) & 0xff);

	case 2: /* 2 bit error (Correctable) */
		dat[(nf8eccerr0 >> 15) & 0x3ff] ^= ((nfmlc8bitpt0 >> 8) & 0xff);

	case 1: /* 1 bit error (Correctable) */
#ifdef CONFIG_FALINUX_ZEROBOOT 
			if ( 0 == check_zeroboot_state_normal() )
#endif				
		printk("s3c-nand: %d bit(s) error detected, corrected successfully\n", err_type);
		dat[nf8eccerr0 & 0x3ff] ^= (nfmlc8bitpt0 & 0xff);
		ret = err_type;
		break;

	case 0: /* No error */
		ret = 0;
		break;
	}

	return ret;
}

void s3c_nand_write_page_8bit(struct mtd_info *mtd, struct nand_chip *chip,
				  const uint8_t *buf)
{
	int i, eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	const uint8_t *p = buf;
	int badoffs = mtd->writesize == 512 ? NAND_SMALL_BADBLOCK_POS : NAND_LARGE_BADBLOCK_POS;
	uint8_t *ecc_calc = chip->buffers->ecccalc;
	uint32_t *mecc_pos = chip->ecc.layout->eccpos;

	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {
		chip->ecc.hwctl(mtd,NAND_ECC_WRITE);
		chip->write_buf(mtd, p, eccsize);
		chip->ecc.calculate(mtd, p, &ecc_calc[i]);
	}

	chip->oob_poi[badoffs] = 0xff;
	for (i = 0; i < chip->ecc.total; i++) {
		chip->oob_poi[mecc_pos[i]] = ecc_calc[i];
	}

	chip->write_buf(mtd, chip->oob_poi, mtd->oobsize);
}

int s3c_nand_read_page_8bit(struct mtd_info *mtd, struct nand_chip *chip,
				uint8_t *buf, int sndcmd)
{
	int i, stat, eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	int col = 0;
	uint8_t *p = buf;
	uint32_t *mecc_pos = chip->ecc.layout->eccpos;

	/* Step1: read whole oob */
	col = mtd->writesize;
	chip->cmdfunc(mtd, NAND_CMD_RNDOUT, col, -1);
	chip->read_buf(mtd, chip->oob_poi, mtd->oobsize);

	col = 0;
	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {
		chip->cmdfunc(mtd, NAND_CMD_RNDOUT, col, -1);
		s3c_nand_enable_hwecc_8bit(mtd, NAND_ECC_READ);
		chip->read_buf(mtd, p, eccsize);
		chip->write_buf(mtd, chip->oob_poi + mecc_pos[0] + ((chip->ecc.steps - eccsteps) * eccbytes), eccbytes);
		chip->ecc.calculate(mtd, 0, 0);
		stat = chip->ecc.correct(mtd, p, NULL, NULL);

		if (stat == -1)
			mtd->ecc_stats.failed++;

		col = eccsize * ((mtd->writesize / eccsize) + 1 - eccsteps);
	}

	return 0;
}


int s3c_nand_read_oob_8bit(struct mtd_info *mtd, struct nand_chip *chip, int page, int sndcmd)
{
        //int eccbytes = chip->ecc.bytes;
        //int secc_start = mtd->oobsize - eccbytes;

        if (sndcmd) {
                chip->cmdfunc(mtd, NAND_CMD_READOOB, 0, page);
                sndcmd = 0;
        }

        //chip->read_buf(mtd, chip->oob_poi, secc_start);
        chip->read_buf(mtd, chip->oob_poi, mtd->oobsize );
        return sndcmd;
}


int s3c_nand_write_oob_8bit(struct mtd_info *mtd, struct nand_chip *chip, int page)
{
        int status = 0;
        //int eccbytes = chip->ecc.bytes;
        //int secc_start = mtd->oobsize - eccbytes;

        chip->cmdfunc(mtd, NAND_CMD_SEQIN, mtd->writesize, page);
        
        //chip->write_buf(mtd, chip->oob_poi, secc_start);
        chip->write_buf(mtd, chip->oob_poi, mtd->oobsize);
        
        chip->cmdfunc(mtd, NAND_CMD_PAGEPROG, -1, -1);
        status = chip->waitfunc(mtd, chip);
        return status & NAND_STATUS_FAIL ? -EIO : 0;
}

#endif

/* s3c_nand_probe
 *
 * called by device layer when it finds a device matching
 * one our driver can handled. This code checks to see if
 * it can allocate all necessary resources then calls the
 * nand layer to look for devices
 */
static int s3c_nand_probe(struct platform_device *pdev, enum s3c_cpu_type cpu_type)
{
	struct s3c_nand_mtd_info *plat_info = &s3c_nand_mtd_part_info;
	struct mtd_partition *partition_info = (struct mtd_partition *)plat_info->partition;
	struct nand_chip *nand;
	struct resource *res;
	int err = 0;
	int ret = 0;
	int i, j, size, nand_page_size = 0;
	struct mtd_partition *partitions = NULL;
	int num_partitions = 0;

#if defined(CONFIG_MTD_NAND_S3C_HWECC)
	struct nand_flash_dev *type = NULL;
	u_char tmp;
#endif

	/* get the clock source and enable it */
#ifdef CONFIG_CPU_S3C64XX
	s3c_nand.clk = clk_get(&pdev->dev, "nand");
#else
	s3c_nand.clk = clk_get(&pdev->dev, "nand");
#endif	

	if (IS_ERR(s3c_nand.clk)) {
		dev_err(&pdev->dev, "failed to get clock\n");
		err = -ENOENT;
		goto exit_error;
	}
	clk_enable(s3c_nand.clk);

	/* allocate and map the resource */

	/* currently we assume we have the one resource */
	res  = pdev->resource;
	size = res->end - res->start + 1;

	s3c_nand.area = request_mem_region(res->start, size, pdev->name);

	if (s3c_nand.area == NULL) {
		dev_err(&pdev->dev, "cannot reserve register region\n");
		err = -ENOENT;
		goto exit_error;
	}

	s3c_nand.cpu_type   = cpu_type;
	s3c_nand.device     = &pdev->dev;
	s3c_nand.regs       = ioremap(res->start, size);

	if (s3c_nand.regs == NULL) {
		dev_err(&pdev->dev, "cannot reserve register region\n");
		err = -EIO;
		goto exit_error;
	}

	for (i = 0; i < plat_info->chip_nr + 1; i++) 
	{
		struct mtd_info *s3c_mtd; 
		
		/* allocate memory for MTD device structure and private data */
		s3c_mtd = kmalloc(sizeof(struct mtd_info) + sizeof(struct nand_chip), GFP_KERNEL);
    	
		if (!s3c_mtd) {
			printk("Unable to allocate NAND MTD dev structure.\n");
			return -ENOMEM;
		}

		s3c_mtd_list[i] = s3c_mtd;
    	
		/* Get pointer to private data */
		nand = (struct nand_chip *) (&s3c_mtd[1]);
    	
		/* Initialize structures */
		memset((char *) s3c_mtd, 0, sizeof(struct mtd_info));
		memset((char *) nand, 0, sizeof(struct nand_chip));
    	
		/* Link the private data with the MTD structure */
		s3c_mtd->priv = nand;

		nand->IO_ADDR_R		= (char *)(s3c_nand.regs + S3C_NFDATA);
		nand->IO_ADDR_W		= (char *)(s3c_nand.regs + S3C_NFDATA);
		nand->cmd_ctrl		= s3c_nand_hwcontrol;
		nand->dev_ready		= s3c_nand_device_ready;		
		nand->scan_bbt		= s3c_nand_scan_bbt;
		nand->options		= 0;
#if defined(CONFIG_MTD_NAND_S3C_CACHEDPROG)
		nand->options		|= NAND_CACHEPRG;
#endif

#if defined(CONFIG_MTD_NAND_S3C_HWECC)
		nand->ecc.mode		= NAND_ECC_HW;
		nand->ecc.hwctl		= s3c_nand_enable_hwecc;
		nand->ecc.calculate	= s3c_nand_calculate_ecc;
		nand->ecc.correct	= s3c_nand_correct_data;
		
		s3c_nand_hwcontrol(NULL, NAND_CMD_READID, NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
		s3c_nand_hwcontrol(NULL, 0x00, NAND_CTRL_CHANGE | NAND_NCE | NAND_ALE);
		s3c_nand_hwcontrol(NULL, 0x00, NAND_NCE | NAND_ALE);
		s3c_nand_hwcontrol(NULL, NAND_CMD_NONE, NAND_NCE | NAND_CTRL_CHANGE);
		s3c_nand_device_ready(0);

		tmp = readb(nand->IO_ADDR_R); /* Maf. ID */
		tmp = readb(nand->IO_ADDR_R); /* Device ID */

		for (j = 0; nand_flash_ids[j].name != NULL; j++) {
			if (tmp == nand_flash_ids[j].id) {
				type = &nand_flash_ids[j];
				break;
			}
		}

		if (!type) {
			printk("Unknown NAND Device.\n");
			goto exit_error;
		}
		
		nand->cellinfo = readb(nand->IO_ADDR_R);	/* the 3rd byte */
		tmp = readb(nand->IO_ADDR_R);			/* the 4th byte */
		if (!type->pagesize) {
			if (((nand->cellinfo >> 2) & 0x3) == 0) {
				nand_type = S3C_NAND_TYPE_SLC;				
				nand->ecc.size = 512;
				nand->ecc.bytes	= 4;

				if ((1024 << (tmp & 3)) == 4096) /* For 4KB Page 8_bit ECC */
				{
					nand_page_size = 4096;
					/* Page size is 4Kbytes */
					nand->ecc.read_page = s3c_nand_read_page_8bit;
					nand->ecc.write_page= s3c_nand_write_page_8bit;
					nand->ecc.read_oob  = s3c_nand_read_oob_8bit;
					nand->ecc.write_oob = s3c_nand_write_oob_8bit;
					nand->ecc.layout    = &s3c_nand_oob_128;
					nand->ecc.hwctl     = s3c_nand_enable_hwecc_8bit;
					nand->ecc.calculate = s3c_nand_calculate_ecc_8bit;
					nand->ecc.correct   = s3c_nand_correct_data_8bit;
					nand->ecc.size      = 512;
					nand->ecc.bytes     = 13;
					nand->options      |= NAND_NO_SUBPAGE_WRITE;
				} else {
					if ((1024 << (tmp & 0x3)) > 512) {
						nand_page_size = 2048;
						if ( i == 0 )
						{
							nand->ecc.read_page = s3c_nand_read_page_8bit;
							nand->ecc.write_page= s3c_nand_write_page_8bit;
							nand->ecc.read_oob  = s3c_nand_read_oob_8bit;
							nand->ecc.write_oob = s3c_nand_write_oob_8bit;
							nand->ecc.layout    = &s3c_nand_oob_64_for_ecc8;
							nand->ecc.hwctl     = s3c_nand_enable_hwecc_8bit;
							nand->ecc.calculate = s3c_nand_calculate_ecc_8bit;
							nand->ecc.correct   = s3c_nand_correct_data_8bit;
							nand->ecc.size      = 512;
							nand->ecc.bytes     = 13;
						}
						else					
						{
							nand->ecc.read_page  = s3c_nand_read_page_1bit;
							nand->ecc.write_page = s3c_nand_write_page_1bit;
							nand->ecc.read_oob   = s3c_nand_read_oob_1bit;
							nand->ecc.write_oob  = s3c_nand_write_oob_1bit;
							nand->ecc.layout     = &s3c_nand_oob_64;
						}
	
					} else {
						nand_page_size = 512;
						nand->ecc.layout     = &s3c_nand_oob_16;
					}
				}
			} else {
				nand_type = S3C_NAND_TYPE_MLC;
				nand->options |= NAND_NO_SUBPAGE_WRITE;	/* NOP = 1 if MLC */

	                        if ((1024 << (tmp & 3)) == 4096) {
        	                        /* Page size is 4Kbytes */
                	                nand->ecc.read_page = s3c_nand_read_page_8bit;
                        	        nand->ecc.write_page = s3c_nand_write_page_8bit;
                                	nand->ecc.read_oob = s3c_nand_read_oob_8bit;
		                        nand->ecc.write_oob = s3c_nand_write_oob_8bit;
	                                nand->ecc.layout = &s3c_nand_oob_128;
        	                        nand->ecc.hwctl = s3c_nand_enable_hwecc_8bit;
                	                nand->ecc.calculate = s3c_nand_calculate_ecc_8bit;
                        	        nand->ecc.correct = s3c_nand_correct_data_8bit;
	                                nand->ecc.size = 512;
        	                        nand->ecc.bytes = 13;
                	                nand->options |= NAND_NO_SUBPAGE_WRITE;
                        	} else {
	                                nand->ecc.read_page = s3c_nand_read_page_4bit;
        	                        nand->ecc.write_page = s3c_nand_write_page_4bit;
                	                nand->ecc.size = 512;
                        	        nand->ecc.bytes = 8;    /* really 7 bytes */
                                	nand->ecc.layout = &s3c_nand_oob_mlc_64;
	                        }
			}
		} else {
			nand_type = S3C_NAND_TYPE_SLC;
			nand->ecc.size = 512;
			nand->cellinfo = 0;
			nand->ecc.bytes = 4;
			nand->ecc.layout = &s3c_nand_oob_16;
		}

		//printk("S3C NAND Driver is using hardware ECC.\n");
#else
		nand->ecc.mode = NAND_ECC_SOFT;
		printk("S3C NAND Driver is using software ECC.\n");
#endif
		if (nand_scan(s3c_mtd, 1)) {
			ret = -ENXIO;
			goto exit_error;
		}

		s3c_mtd->name = dev_name(&pdev->dev);
		s3c_mtd->owner = THIS_MODULE;


		if ( nand_page_size == 2048 )
		{
			switch( i )
			{
			case 0 : 			
				printk("S3C NAND Driver is using hardware ECC-8\n");
				num_partitions = plat_info->mtd_part_nr;
				partitions     = partition_info;
				add_mtd_partitions(s3c_mtd, partitions, 2 ); 
				break;
			case 1 : 
				printk("S3C NAND Driver is using hardware ECC-1\n");
				add_mtd_partitions(s3c_mtd, &partitions[2], num_partitions-2); 
				break;
			}
		}
		else
		{
			printk("S3C NAND Driver is using hardware ECC-1\n");
			num_partitions = plat_info->mtd_part_nr;
			partitions     = partition_info;
			add_mtd_partitions(s3c_mtd, partitions, num_partitions);
			i++;	
		}
	}

	pr_debug("initialized ok\n");
	return 0;

exit_error:
	//kfree(s3c_mtd);

	return ret;
}

/* PM Support */
#if defined(CONFIG_PM)
static int s3c_nand_suspend(struct platform_device *dev, pm_message_t pm)
{
	clk_disable(s3c_nand.clk);
	return 0;
}

static int s3c_nand_resume(struct platform_device *dev)
{
	clk_enable(s3c_nand.clk);
	return 0;
}

#else
#define s3c_nand_suspend NULL
#define s3c_nand_resume NULL
#endif

/* device management functions */
static int s3c_nand_remove(struct platform_device *dev)
{
	platform_set_drvdata(dev, NULL);

	return 0;
}

/*
static int s5pc100_nand_probe(struct platform_device *dev)
{
        return s3c_nand_probe(dev, TYPE_S5PC100);
}

static int s5p6440_nand_probe(struct platform_device *dev)
{
        return s3c_nand_probe(dev, TYPE_S5P6440);
}

static int s5pc110_nand_probe(struct platform_device *dev)
{
        return s3c_nand_probe(dev, TYPE_S5PC110);
}

static struct platform_driver s5p6440_nand_driver = {
        .probe          = s5p6440_nand_probe,
        .remove         = s3c_nand_remove,
        .suspend        = s3c_nand_suspend,
        .resume         = s3c_nand_resume,
        .driver         = {
                .name   = "s3c-nand",
                .owner  = THIS_MODULE,
        },
};

static struct platform_driver s5pc110_nand_driver = {
        .probe          = s5pc110_nand_probe,
        .remove         = s3c_nand_remove,
        .suspend        = s3c_nand_suspend,
        .resume         = s3c_nand_resume,
        .driver         = {
                .name   = "s3c-nand",
                .owner  = THIS_MODULE,
        },
};
*/

#ifdef CONFIG_CPU_S3C64XX
static int s3c6410_nand_probe(struct platform_device *dev)
{
	return s3c_nand_probe(dev, TYPE_S3C6410);
}
static struct platform_driver s3c_nand_driver = {
	.probe		= s3c6410_nand_probe,
	.remove		= s3c_nand_remove,
	.suspend	= s3c_nand_suspend,
	.resume		= s3c_nand_resume,
	.driver		= {
		.name	= "s3c-nand",
		.owner	= THIS_MODULE,
	},
};
#endif	

#ifdef CONFIG_CPU_S5PV210
static int s5pv210_nand_probe(struct platform_device *dev)
{
        return s3c_nand_probe(dev, TYPE_S5PV210);
}
static struct platform_driver s3c_nand_driver = {
	.probe		= s5pv210_nand_probe,
	.remove		= s3c_nand_remove,
	.suspend	= s3c_nand_suspend,
	.resume		= s3c_nand_resume,
	.driver		= {
		.name	= "s3c-nand",
		.owner	= THIS_MODULE,
	},
};
#endif

static int __init s3c_nand_init(void)
{
	printk("S3C NAND Driver, (c) 2008 Samsung Electronics\n");
	return platform_driver_register(&s3c_nand_driver);
}

static void __exit s3c_nand_exit(void)
{
	platform_driver_unregister(&s3c_nand_driver);
}

module_init(s3c_nand_init);
module_exit(s3c_nand_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jinsung Yang <jsgood.yang@samsung.com>");
MODULE_DESCRIPTION("S3C MTD NAND driver");



