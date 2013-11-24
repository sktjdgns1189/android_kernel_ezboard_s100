/*
 * wm9715.h  --  WM9715 Soc Audio driver
 */

#ifndef _WM9715_H
#define _WM9715_H

#define AC97_WM9715_LINEOUT  	0x02
#define AC97_WM9715_AUXDAC  	0x12
#define AC97_WM9715_SIDETONE	0x14
#define AC97_WM9715_OUT3  		0x16
#define AC97_WM9715_REG_5C		0x5c
#define AC97_WM9715_ALC			0x60
#define AC97_WM9715_ALC_FUNC	0x62

/* clock inputs */
#define WM9715_CLKA_PIN			0
#define WM9715_CLKB_PIN			1

/* clock divider ID's */
#define WM9715_PCMCLK_DIV		0
#define WM9715_CLKA_MULT		1
#define WM9715_CLKB_MULT		2
#define WM9715_HIFI_DIV			3
#define WM9715_PCMBCLK_DIV		4
#define WM9715_PCMCLK_PLL_DIV           5
#define WM9715_HIFI_PLL_DIV             6

/* Calculate the appropriate bit mask for the external PCM clock divider */
#define WM9715_PCMDIV(x)	((x - 1) << 8)

/* Calculate the appropriate bit mask for the external HiFi clock divider */
#define WM9715_HIFIDIV(x)	((x - 1) << 12)

/* MCLK clock mulitipliers */
#define WM9715_CLKA_X1		(0 << 1)
#define WM9715_CLKA_X2		(1 << 1)
#define WM9715_CLKB_X1		(0 << 2)
#define WM9715_CLKB_X2		(1 << 2)

/* MCLK clock MUX */
#define WM9715_CLK_MUX_A		(0 << 0)
#define WM9715_CLK_MUX_B		(1 << 0)

/* Voice DAI BCLK divider */
#define WM9715_PCMBCLK_DIV_1	(0 << 9)
#define WM9715_PCMBCLK_DIV_2	(1 << 9)
#define WM9715_PCMBCLK_DIV_4	(2 << 9)
#define WM9715_PCMBCLK_DIV_8	(3 << 9)
#define WM9715_PCMBCLK_DIV_16	(4 << 9)

#define WM9715_DAI_AC97_HIFI	0
#define WM9715_DAI_AC97_AUX		1

extern struct snd_soc_codec_device soc_codec_dev_wm9715;
extern struct snd_soc_dai wm9715_dai[3];

int wm9715_reset(struct snd_soc_codec *codec,  int try_warm);

#endif
