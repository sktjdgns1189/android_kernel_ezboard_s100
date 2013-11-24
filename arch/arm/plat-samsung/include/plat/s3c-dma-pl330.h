/*
 * Copyright (C) 2010 Samsung Electronics Co. Ltd.
 *	Jaswinder Singh <jassi.brar@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef	__S3C_DMA_PL330_H_
#define	__S3C_DMA_PL330_H_

#define S3C2410_DMAF_AUTOSTART		(1 << 0)
#define S3C2410_DMAF_CIRCULAR		(1 << 1)

/*
 * PL330 can assign any channel to communicate with
 * any of the peripherals attched to the DMAC.
 * For the sake of consistency across client drivers,
 * We keep the channel names unchanged and only add
 * missing peripherals are added.
 * Order is not important since S3C PL330 API driver
 * use these just as IDs.
 */
enum dma_ch {
	DMACH_UART0_RX,
	DMACH_UART0_TX,
	DMACH_UART1_RX,
	DMACH_UART1_TX,
	DMACH_UART2_RX,
	DMACH_UART2_TX,
	DMACH_UART3_RX,
	DMACH_UART3_TX,
	DMACH_UART4_RX,
	DMACH_UART4_TX,
	DMACH_UART5_RX,
	DMACH_UART5_TX,
	DMACH_USI_RX,
	DMACH_USI_TX,
	DMACH_IRDA,
	DMACH_I2S0_RX,
	DMACH_I2S0_TX,
	DMACH_I2S0S_TX,
	DMACH_I2S1_RX,
	DMACH_I2S1_TX,
	DMACH_I2S2_RX,
	DMACH_I2S2_TX,
	DMACH_SPI0_RX,
	DMACH_SPI0_TX,
	DMACH_SPI1_RX,
	DMACH_SPI1_TX,
	DMACH_SPI2_RX,
	DMACH_SPI2_TX,
	DMACH_AC97_MICIN,
	DMACH_AC97_PCMIN,
	DMACH_AC97_PCMOUT,
	DMACH_EXTERNAL,
	DMACH_PWM,
	DMACH_SPDIF,
	DMACH_HSI_RX,
	DMACH_HSI_TX,
	DMACH_PCM0_TX,
	DMACH_PCM0_RX,
	DMACH_PCM1_TX,
	DMACH_PCM1_RX,
	DMACH_PCM2_TX,
	DMACH_PCM2_RX,
	DMACH_MSM_REQ3,
	DMACH_MSM_REQ2,
	DMACH_MSM_REQ1,
	DMACH_MSM_REQ0,
	DMACH_SLIMBUS0_RX,
	DMACH_SLIMBUS0_TX,
	DMACH_SLIMBUS0AUX_RX,
	DMACH_SLIMBUS0AUX_TX,
	DMACH_SLIMBUS1_RX,
	DMACH_SLIMBUS1_TX,
	DMACH_SLIMBUS2_RX,
	DMACH_SLIMBUS2_TX,
	DMACH_SLIMBUS3_RX,
	DMACH_SLIMBUS3_TX,
	DMACH_SLIMBUS4_RX,
	DMACH_SLIMBUS4_TX,
	DMACH_SLIMBUS5_RX,
	DMACH_SLIMBUS5_TX,
	DMACH_MTOM_0,
	DMACH_MTOM_1,
	DMACH_MTOM_2,
	DMACH_MTOM_3,
	DMACH_MTOM_4,
	DMACH_MTOM_5,
	DMACH_MTOM_6,
	DMACH_MTOM_7,
	/* END Marker, also used to denote a reserved channel */
	DMACH_MAX,
};

#define S3C_DMA_CHANNELS		(4)
#define DMACH_LOW_LEVEL			(1<<28)		/* use this to specifiy hardware ch no */



/* types */

enum s3c2410_dma_state {
	S3C2410_DMA_IDLE,
	S3C2410_DMA_RUNNING,
	S3C2410_DMA_PAUSED
};


/* dma buffer */

struct s3c2410_dma_buf;

/* s3c2410_dma_buf
 *
 * internally used buffer structure to describe a queued or running
 * buffer.
*/

struct s3c2410_dma_buf {
	struct s3c2410_dma_buf	*next;
	int			 magic;		/* magic */
	int			 size;		/* buffer size in bytes */
	dma_addr_t		 data;		/* start of DMA data */
	dma_addr_t		 ptr;		/* where the DMA got to [1] */
	void			*id;		/* client's id */
};

/* [1] is this updated for both recv/send modes? */

struct s3c2410_dma_stats {
	unsigned long		loads;
	unsigned long		timeout_longest;
	unsigned long		timeout_shortest;
	unsigned long		timeout_avg;
	unsigned long		timeout_failed;
};


/* enum s3c2410_dma_loadst
 *
 * This represents the state of the DMA engine, wrt to the loaded / running
 * transfers. Since we don't have any way of knowing exactly the state of
 * the DMA transfers, we need to know the state to make decisions on wether
 * we can
 *
 * S3C2410_DMA_NONE
 *
 * There are no buffers loaded (the channel should be inactive)
 *
 * S3C2410_DMA_1LOADED
 *
 * There is one buffer loaded, however it has not been confirmed to be
 * loaded by the DMA engine. This may be because the channel is not
 * yet running, or the DMA driver decided that it was too costly to
 * sit and wait for it to happen.
 *
 * S3C2410_DMA_1RUNNING
 *
 * The buffer has been confirmed running, and not finisged
 *
 * S3C2410_DMA_1LOADED_1RUNNING
 *
 * There is a buffer waiting to be loaded by the DMA engine, and one
 * currently running.
*/

enum s3c2410_dma_loadst {
	S3C2410_DMALOAD_NONE,
	S3C2410_DMALOAD_1LOADED,
	S3C2410_DMALOAD_1RUNNING,
	S3C2410_DMALOAD_1LOADED_1RUNNING,
};

struct s3c2410_dma_chan {
	/* channel state flags and information */
	unsigned char		 number;      /* number of this dma channel */
	unsigned char		 in_use;      /* channel allocated */
	unsigned char		 irq_claimed; /* irq claimed for channel */
	unsigned char		 irq_enabled; /* irq enabled for channel */
	unsigned char		 xfer_unit;   /* size of an transfer */

	/* channel state */

	enum s3c2410_dma_state	 state;
	enum s3c2410_dma_loadst	 load_state;
	struct s3c2410_dma_client *client;

	/* channel configuration */
	enum s3c2410_dmasrc	 source;
	enum dma_ch		 req_ch;
	unsigned long		 dev_addr;
	unsigned long		 load_timeout;
	unsigned int		 flags;		/* channel flags */

	struct s3c24xx_dma_map	*map;		/* channel hw maps */

	/* channel's hardware position and configuration */
	void __iomem		*regs;		/* channels registers */
	void __iomem		*addr_reg;	/* data address register */
	unsigned int		 irq;		/* channel irq */
	unsigned long		 dcon;		/* default value of DCON */

	/* driver handles */
	s3c2410_dma_cbfn_t	 callback_fn;	/* buffer done callback */
	s3c2410_dma_opfn_t	 op_fn;		/* channel op callback */

	/* stats gathering */
	struct s3c2410_dma_stats *stats;
	struct s3c2410_dma_stats  stats_store;

	/* buffer list and information */
	struct s3c2410_dma_buf	*curr;		/* current dma buffer */
	struct s3c2410_dma_buf	*next;		/* next buffer to load */
	struct s3c2410_dma_buf	*end;		/* end of queue */

	/* system device */
	struct sys_device	dev;
};

static inline bool s3c_dma_has_circular(void)
{
	return true;
}

static inline bool samsung_dma_is_dmadev(void)
{
	return true;
}


#include <plat/dma.h>

#endif	/* __S3C_DMA_PL330_H_ */
