/*
 * ezbd_wm9715.c  --  SoC audio for ezbd
 *
 * Copyright 2011 falinux.co.,ltd
 * Author: JaeKyong OH <freefrug@falinux.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/device.h>
#include <sound/soc.h>

#include "../codecs/wm9715.h"
#include "s3c-dma.h"
#include "s3c-ac97.h"

static struct snd_soc_card ezbd;
/*
 Playback (HeadPhone):-
	$ amixer sset 'Headphone' unmute
	$ amixer sset 'Right Headphone Out Mux' 'Headphone'
	$ amixer sset 'Left Headphone Out Mux' 'Headphone'
	$ amixer sset 'Right HP Mixer PCM' unmute
	$ amixer sset 'Left HP Mixer PCM' unmute

 Capture (LineIn):-
	$ amixer sset 'Right Capture Source' 'Line'
	$ amixer sset 'Left Capture Source' 'Line'
*/

static struct snd_soc_dai_link ezbd_dai = {
	.name = "AC97",
	.stream_name = "AC97 HiFi",
	.cpu_dai = &s3c_ac97_dai[S3C_AC97_DAI_PCM],
	.codec_dai = &wm9715_dai[WM9715_DAI_AC97_HIFI],
};

static struct snd_soc_card ezbd = {
	.name = "ezbd",
	.platform = &s3c24xx_soc_platform,
	.dai_link = &ezbd_dai,
	.num_links = 1,
};

static struct snd_soc_device ezbd_snd_ac97_devdata = {
	.card = &ezbd,
	.codec_dev = &soc_codec_dev_wm9715,
};


int ezb_load_ac97 = 1;
EXPORT_SYMBOLE(ezb_load_ac97);

int ezb_option_ac97( char *s )
{
	ezb_load_ac97 = simple_strtoul( s, NULL, 0 );
	return 1;	
}
__setup("ezb_ac97=", ezb_option_ac97 );


static struct platform_device *ezbd_snd_ac97_device;

static int __init ezbd_init(void)
{
	int ret;

	if ( 0 == ezb_load_ac97 ) return -ENODEV;

	ezbd_snd_ac97_device = platform_device_alloc("soc-audio", -1);
	if (!ezbd_snd_ac97_device)
		return -ENOMEM;

	platform_set_drvdata(ezbd_snd_ac97_device,
			     &ezbd_snd_ac97_devdata);
	ezbd_snd_ac97_devdata.dev = &ezbd_snd_ac97_device->dev;

	ret = platform_device_add(ezbd_snd_ac97_device);
	if (ret)
		platform_device_put(ezbd_snd_ac97_device);

	return ret;
}

static void __exit ezbd_exit(void)
{
	platform_device_unregister(ezbd_snd_ac97_device);
}

module_init(ezbd_init);
module_exit(ezbd_exit);

/* Module information */
MODULE_AUTHOR("JaeKyong OH <freefrug@falinux.com>");
MODULE_DESCRIPTION("ALSA SoC EZ-Board WM9715");
MODULE_LICENSE("GPL");
