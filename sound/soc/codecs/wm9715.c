/*
 * wm9715.c  --  ALSA Soc WM9715 codec support
 *
 * Copyright 2011 falinux.co.,ltd
 * Author: JaeKyong OH <freefrug@falinux.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  Features:-
 *
 *   o Support for AC97 Codec
 *   o Support for DAPM
 */

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/ac97_codec.h>
#include <sound/initval.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include "wm9715.h"

struct wm9715_priv {
	u32 pll_in; /* PLL input frequency */
};

static unsigned int ac97_read(struct snd_soc_codec *codec, unsigned int reg);
static int ac97_write(struct snd_soc_codec *codec, unsigned int reg, unsigned int val);

/*
 * wm9715 register cache
 * Reg 0x3c bit 15 is used by touch driver.
 */
static const u16 wm9715_reg[] = {
  /*+0/8    +2/a	+4/c	+6/e  */  
	0x6174, 0x8000, 0x8000, 0x8000,	// 0x00            00 : 6174 8000  8000 8000
	0x0f0f, 0xaaa0, 0xc008, 0x6808,	// 0x08            08 : 0f0f aaa0  c008 6808
	0xe808, 0xaaa0, 0xad00, 0x8000, // 0x10            10 : e808(0000) ad00 8000
	0xe808, 0x3000, 0x8000, 0x0000, // 0x18            18 : e808 3000  8000(0400)

  /*+0/8    +2/a	+4/c	+6/e  */  
	0x0000, 0x0000, 0x0000, 0x0000, // 0x20            20 : 0000 0000  0000(000f)
	0x0405, 0x0410, 0xbb80, 0xbb80, // 0x28            28 : 0405 0410  bb80 bb80
	0x0000, 0xbb80, 0x0000, 0x0000, // 0x30            30 : 0000 bb80  0000 0000
	0x0000, 0x2000, 0x0000, 0x0000, // 0x38            38 : 0000 2000  0000 0000

  /*+0/8    +2/a	+4/c	+6/e  */  
	0x0000, 0x0000, 0x0000, 0x0000, // 0x40            40 : 0000 0000  0000 0000 
	0x0000, 0x0000, 0xf83e, 0xffff, // 0x48            48 : 0000 0000  f83e ffff 
	0x0000, 0x0000, 0x0000, 0xf83e, // 0x50            50 : 0000 0000  0800 f83e 
	0x0008, 0x0000, 0x0000, 0x0000, // 0x58            58 :(000c 0040  0000 0400)
	
  /*+0/8    +2/a	+4/c	+6/e  */  
	0xb032, 0x3e00, 0x0000, 0x0000, // 0x60            60 : b032 3e00  0000 0000
	0x0000, 0x0000, 0x0000, 0x0000, // 0x68            68 : 0000 0000  0000 0000
	0x0000, 0x0000, 0x0000, 0x0006, // 0x70            70 : 0000 0000  0080 0006
	0x0001, 0x0000, 0x574d, 0x4c12, // 0x78            78 : 0001 0000  574d 4c12
	
	0x0000, 0x0000, 0x0000
};


/* virtual HP mixers regs */
#define HPL_MIXER	0x80
#define HPR_MIXER	0x82
#define MICB_MUX	0x82

static const char *wm9715_mic_mixer[]      = {"Mic 1", "diff 1-2", "Mic 2", "Stereo"};
static const char *wm9715_rec_mux[]        = {"Stereo", "Left", "Right", "Mute"};
static const char *wm9715_rec_src[]        = {"Mic", "none-1", "none-2", "Speaker-Mix", "Line", "Headphone-Mix", "Phone-Mix", "Phone-In" };
static const char *wm9715_rec_gain[]       = {"+1.5dB Steps", "+0.75dB Steps"};
static const char *wm9715_alc_select[]     = {"None", "Right", "Left", "Stereo"};
static const char *wm9715_mono_pga[]       = {"Vmid", "Zh", "Mono", "Inv", "Mono Vmid", "Inv Vmid"};
static const char *wm9715_spk_pga[]        = {"Vmid", "Zh", "Headphone", "Speaker", "Inv", "Headphone Vmid", "Speaker Vmid", "Inv Vmid"};
static const char *wm9715_hp_pga[]         = {"Vmid", "Zh", "Headphone", "Headphone Vmid"};
static const char *wm9715_out3_pga[]       = {"Right HP", "Vref", "Left+Right HP", "MonoOut"};
static const char *wm9715_out4_pga[]       = {"Vmid", "Zh", "Inv 2", "Inv 2 Vmid"};
static const char *wm9715_dac_inv[]        = {"Off", "Mono", "Speaker", "Left Headphone", "Right Headphone", "Headphone Mono", "NC", "Vmid"};
static const char *wm9715_bass[]           = {"Linear Control", "Adaptive Boost"};
static const char *wm9715_ng_type[]        = {"Constant Gain", "Mute"};
static const char *wm9715_mic_select[]     = {"Mic 1", "diff 1-2", "Mic 2", "Stereo"};
static const char *wm9715_micb_select[]    = {"MPB", "MPA"};
static const char *wm9715_lineout_select[] = {"from Speaker Mixer", "from HeadPhone Mixer"};
static const char *wm9715_mic_bias_select[]= {"3.3V", "none(3.3V)", "2.5V", "1.8V"};


//#define SOC_ENUM_DOUBLE(xreg, xshift_l, xshift_r, xmax, xtexts) 
//  {       .reg = xreg, .shift_l = xshift_l, .shift_r = xshift_r, 
//          .max = xmax, .texts = xtexts }
//#define SOC_ENUM_SINGLE(xreg, xshift, xmax, xtexts)

static const struct soc_enum wm9715_enum[] = {   
	SOC_ENUM_SINGLE(AC97_MIC            ,  5, 4,    wm9715_mic_mixer  ), /* record mic mixer           0 */
SOC_ENUM_SINGLE(AC97_REC_SEL            , 12, 4,    wm9715_rec_mux    ), /* record mux hp              1 */
	SOC_ENUM_SINGLE(AC97_VIDEO          ,  9, 4,    wm9715_rec_mux    ), /* record mux mono            2 */

SOC_ENUM_SINGLE(AC97_REC_SEL            ,  8, 8,    wm9715_rec_src    ), /* record mux left            3 */
SOC_ENUM_SINGLE(AC97_REC_SEL            ,  0, 8,    wm9715_rec_src    ), /* record mux right           4 */

	SOC_ENUM_DOUBLE(AC97_CD             , 14, 6, 2, wm9715_rec_gain   ), /* record step size           5 */
		
SOC_ENUM_SINGLE(AC97_WM9715_ALC_FUNC    , 14, 4,    wm9715_alc_select ), /* alc source select          6 */

	SOC_ENUM_SINGLE(AC97_REC_GAIN       , 14, 4,    wm9715_mono_pga   ), /* mono input select          7 */
	
	SOC_ENUM_SINGLE(AC97_REC_GAIN       , 11, 8,    wm9715_spk_pga    ), /* speaker left input select  8 */
	SOC_ENUM_SINGLE(AC97_REC_GAIN       ,  8, 8,    wm9715_spk_pga    ), /* speaker right input select 9 */
	SOC_ENUM_SINGLE(AC97_REC_GAIN       ,  6, 3,    wm9715_hp_pga     ), /* headphone left input      10 */
	SOC_ENUM_SINGLE(AC97_REC_GAIN       ,  4, 3,    wm9715_hp_pga     ), /* headphone right input     11 */
SOC_ENUM_SINGLE(AC97_WM9715_OUT3        ,  9, 4,    wm9715_out3_pga   ), /* out 3 source              12 */
	SOC_ENUM_SINGLE(AC97_REC_GAIN       ,  0, 4,    wm9715_out4_pga   ), /* out 4 source              13 */
	SOC_ENUM_SINGLE(AC97_REC_GAIN_MIC   , 13, 8,    wm9715_dac_inv    ), /* dac invert 1              14 */
	SOC_ENUM_SINGLE(AC97_REC_GAIN_MIC   , 10, 8,    wm9715_dac_inv    ), /* dac invert 2              15 */

SOC_ENUM_SINGLE(AC97_MASTER_TONE        , 15, 2,    wm9715_bass       ), /* bass control              16 */
SOC_ENUM_SINGLE(AC97_WM9715_ALC_FUNC    ,  5, 2,    wm9715_ng_type    ), /* noise gate type           17 */
	
	
SOC_ENUM_SINGLE(AC97_MIC                ,  5, 4,    wm9715_mic_select ), /* mic selection             18 */
	SOC_ENUM_SINGLE(MICB_MUX            ,  0, 2,    wm9715_micb_select), /* mic selection             19 */
	
SOC_ENUM_SINGLE(AC97_WM9715_OUT3        ,  8, 2,    wm9715_lineout_select ),/* LineOut selection      20 */
SOC_ENUM_SINGLE(AC97_WM9715_REG_5C      ,  5, 4,    wm9715_mic_bias_select),/* Mic Bias selection     21 */
	
};

static const DECLARE_TLV_DB_SCALE(out_tlv , -4650, 150, 0);
static const DECLARE_TLV_DB_SCALE(main_tlv, -3450, 150, 0);
static const DECLARE_TLV_DB_SCALE(misc_tlv, -1500, 600, 0);
static const DECLARE_TLV_DB_SCALE(rec_tlv ,     0,2250, 0);
//static unsigned int mic_tlv[] = {
//	TLV_DB_RANGE_HEAD(2),
//	0, 2, TLV_DB_SCALE_ITEM(1200, 600, 0),
//	3, 3, TLV_DB_SCALE_ITEM(3000, 0, 0),
//};

//#define SOC_SINGLE(xname, reg, shift, max, invert)
static const struct snd_kcontrol_new wm9715_snd_ac97_controls[] = {
SOC_DOUBLE_TLV("LineOut Playback Volume"             , AC97_WM9715_LINEOUT ,  8,  0, 63, 1, out_tlv ),
SOC_SINGLE    ("LineOut Playback Switch"             , AC97_WM9715_LINEOUT , 15,  1,  1             ),  
SOC_SINGLE    ("LineOut Playback ZC Switch"          , AC97_WM9715_LINEOUT ,  7,  1,  0             ),  
                                                                           
SOC_DOUBLE_TLV("Headphone Playback Volume"           , AC97_HEADPHONE      ,  8,  0, 63, 1, out_tlv ),
SOC_SINGLE    ("Headphone Playback Switch"           , AC97_HEADPHONE      , 15,  1,  1             ),  
SOC_SINGLE    ("Headphone Playback ZC Switch"        , AC97_HEADPHONE      ,  7,  1,  0             ),  

SOC_DOUBLE_TLV("PCM Playback Volume"                 , AC97_PCM            ,  8,  0, 31, 1, main_tlv),
SOC_SINGLE    ("PCM to HeadPhone Mixer Switch"       , AC97_PCM            , 15,  1,  1             ),  
SOC_SINGLE    ("PCM to Speaker Mixer Switch"         , AC97_PCM            , 14,  1,  1             ),  
SOC_SINGLE    ("PCM to Phone Mixer Switch"           , AC97_PCM            , 13,  1,  1             ),  

SOC_DOUBLE_TLV("Capture Volume"                      , AC97_REC_GAIN       ,  8,  0, 15, 0, rec_tlv ),
SOC_SINGLE    ("Capture Mute"                        , AC97_REC_GAIN       , 15,  1,  1             ),
SOC_SINGLE    ("Capture Boost (+20db) Switch"        , AC97_REC_SEL        , 14,  1,  0             ),
SOC_ENUM      ("Capture Path"                        , wm9715_enum[1]      ),
SOC_ENUM      ("Capture L-Source"                    , wm9715_enum[3]      ),
SOC_ENUM      ("Capture R-Source"                    , wm9715_enum[4]      ),

SOC_SINGLE_TLV("Mic 1 Volume"                        , AC97_MIC            ,  8, 31,  1,    main_tlv),
SOC_SINGLE_TLV("Mic 2 Volume"                        , AC97_MIC            ,  0, 31,  1,    main_tlv),
SOC_SINGLE    ("Mic Boost (+20dB) Switch"            , AC97_MIC            ,  7,  1,  0              ),
SOC_SINGLE    ("Mic 1 Mute"                          , AC97_MIC            , 14,  1,  0              ),
SOC_SINGLE    ("Mic 2 Mute"                          , AC97_MIC            , 13,  1,  0              ),
SOC_ENUM      ("Mic Bias"                            , wm9715_enum[21]     ),

SOC_DOUBLE_TLV("LineIn Volume"                       , AC97_LINE           ,  8,  0, 31, 1, main_tlv),
                                                     
SOC_SINGLE_TLV("Mono Playback Volume"                , AC97_MASTER_MONO    ,  0, 31,  1, out_tlv),
SOC_SINGLE    ("Mono Playback Switch"                , AC97_MASTER_MONO    , 15,  1,  1         ),
SOC_SINGLE    ("Mono Playback ZC Switch"             , AC97_MASTER_MONO    ,  7,  1,  0         ),
                                                                                      
SOC_SINGLE_TLV("Out3 Playback Volume"                , AC97_WM9715_OUT3    ,  0, 63,  1, out_tlv),
SOC_SINGLE    ("Out3 Playback Switch"                , AC97_WM9715_OUT3    , 15,  1,  1         ),
SOC_SINGLE    ("Out3 Playback ZC Switch"             , AC97_WM9715_OUT3    ,  7,  1,  0         ),
                                                                                     
SOC_SINGLE    ("ALC Target Volume"                   , AC97_WM9715_SIDETONE,  7,  7,  0),
SOC_SINGLE    ("ALC NG Threshold"                    , AC97_WM9715_ALC     , 12, 15,  0),
SOC_SINGLE    ("ALC Hold Time"                       , AC97_WM9715_ALC     ,  8, 15,  0),
SOC_SINGLE    ("ALC Decay Time"                      , AC97_WM9715_ALC     ,  4, 15,  0),
SOC_SINGLE    ("ALC Attack Time"                     , AC97_WM9715_ALC     ,  0, 15,  0),
SOC_ENUM      ("ALC Function"                        , wm9715_enum[6]                  ),
SOC_SINGLE    ("ALC Max Gain"                        , AC97_WM9715_ALC_FUNC, 11,  7,  0),
SOC_SINGLE    ("ALC ZC Timeout"                      , AC97_WM9715_ALC_FUNC,  9,  3,  0),
SOC_SINGLE    ("ALC ZC Switch"                       , AC97_WM9715_ALC_FUNC,  8,  1,  0),
SOC_SINGLE    ("ALC NG Switch"                       , AC97_WM9715_ALC_FUNC,  7,  1,  0),
SOC_ENUM      ("ALC NG Type"                         , wm9715_enum[17]                 ),
                                                                                                          
SOC_ENUM      ("Bass Control"                        , wm9715_enum[16]                ),
SOC_SINGLE    ("Bass Cut-off Switch"                 , AC97_MASTER_TONE    , 12, 1,  1),
SOC_SINGLE    ("Tone Cut-off Switch"                 , AC97_MASTER_TONE    ,  4, 1,  1),
SOC_SINGLE    ("Bass Volume"                         , AC97_MASTER_TONE    ,  8, 15, 1),
SOC_SINGLE    ("Tone Volume"                         , AC97_MASTER_TONE    ,  0, 15, 1),
SOC_SINGLE    ("PlayBack Attenuate (-6dB) Switch"    , AC97_MASTER_TONE    ,  6,  1, 0),
                                                     
SOC_SINGLE    ("3D Enable"                           , AC97_GENERAL_PURPOSE, 13,  1, 0),
SOC_SINGLE    ("3D Lower Cut-off Switch"             , AC97_3D_CONTROL     ,  5,  1, 0),
SOC_SINGLE    ("3D Upper Cut-off Switch"             , AC97_3D_CONTROL     ,  4,  1, 0),
SOC_SINGLE    ("3D Depth"                            , AC97_3D_CONTROL     ,  0, 15, 1),
              
SOC_SINGLE_TLV("Headphone Mixer Beep Playback Volume", AC97_PC_BEEP        , 12,  7, 1, misc_tlv),
SOC_SINGLE_TLV("Speaker Mixer Beep Playback Volume"  , AC97_PC_BEEP        ,  8,  7, 1, misc_tlv),
SOC_SINGLE_TLV("Mono Mixer Beep Playback Volume"     , AC97_PC_BEEP        ,  4,  7, 1, misc_tlv),
SOC_SINGLE    ("Headphone Mixer Beep Playback Switch", AC97_PC_BEEP        , 15,  1, 1          ),
SOC_SINGLE    ("Speaker Mixer Beep Playback Switch"  , AC97_PC_BEEP        , 11,  1, 1          ),
SOC_SINGLE    ("Mono Mixer Beep Playback Switch"     , AC97_PC_BEEP        ,  7,  1, 1          ),



//?? SOC_SINGLE_TLV("Speaker Mixer Aux Playback Volume", AC97_REC_SEL, 8, 7, 1, misc_tlv),
//?? SOC_SINGLE_TLV("Mono Mixer Aux Playback Volume", AC97_REC_SEL, 4, 7, 1,  misc_tlv),
//?? SOC_SINGLE("Aux Playback Headphone Volume", AC97_REC_SEL, 12, 7, 1),
//?? SOC_SINGLE("Aux Playback Master Volume", AC97_REC_SEL, 8, 7, 1),
//?? SOC_SINGLE_TLV("Mic 1 Preamp Volume", AC97_3D_CONTROL, 10, 3, 0, mic_tlv),
//?? SOC_SINGLE_TLV("Mic 2 Preamp Volume", AC97_3D_CONTROL, 12, 3, 0, mic_tlv),
//?? SOC_SINGLE("Mic Headphone Mixer Volume", AC97_LINE, 0, 7, 1),
//?? SOC_SINGLE("Capture Switch", AC97_CD, 15, 1, 1),
//?? SOC_ENUM("Capture Volume Steps", wm9715_enum[5]),
//?? SOC_DOUBLE("Capture Volume", AC97_CD, 8, 0, 31, 0),
//?? SOC_SINGLE("Capture ZC Switch", AC97_CD, 7, 1, 0),
//?? SOC_SINGLE_TLV("Capture to Headphone Volume", AC97_VIDEO, 11, 7, 1, misc_tlv),
//?? SOC_SINGLE("Capture to Mono Boost (+20dB) Switch", AC97_VIDEO, 8, 1, 0),
//?? SOC_SINGLE("Capture ADC Boost (+20dB) Switch", AC97_VIDEO, 6, 1, 0),
//?? SOC_SINGLE_TLV("Mono Capture Volume"    , AC97_MASTER_TONE, 8, 31, 1, main_tlv),
//?? SOC_SINGLE("Out4 Playback Switch", AC97_MASTER_MONO, 15, 1, 1),
//?? SOC_SINGLE("Out4 Playback ZC Switch", AC97_MASTER_MONO, 14, 1, 0),
//?? SOC_SINGLE_TLV("Out4 Playback Volume", AC97_MASTER_MONO, 8, 31, 1, out_tlv),
//?? SOC_SINGLE_TLV("Speaker Mixer Voice Playback Volume", AC97_PCM, 8, 7, 1, misc_tlv),
//?? SOC_SINGLE_TLV("Mono Mixer Voice Playback Volume", AC97_PCM, 4, 7, 1, misc_tlv),
};



//#define SOC_DAPM_SINGLE(xname, reg, shift, max, invert)

/* Headphone Mixers */
static const struct snd_kcontrol_new wm9715_hp_mixer_controls[] = {
SOC_DAPM_SINGLE("PCM Playback Switch"   , AC97_PCM  , 15, 1, 1),
SOC_DAPM_SINGLE("LineIn Switch"         , AC97_LINE , 15, 1, 1),
SOC_DAPM_SINGLE("Phone Switch"          , AC97_PHONE, 15, 1, 1),
};

/* Phone Mixers */
static const struct snd_kcontrol_new wm9715_phone_mixer_controls[] = {
SOC_DAPM_SINGLE("LineIn Switch"         , AC97_LINE , 13, 1, 1),
SOC_DAPM_SINGLE("PCM Playback Switch"   , AC97_PCM  , 13, 1, 1),
};

/* Speaker Mixers */
static const struct snd_kcontrol_new wm9715_speaker_mixer_controls[] = {
SOC_DAPM_SINGLE("LineIn Switch"         , AC97_LINE , 14, 1, 1),
SOC_DAPM_SINGLE("PCM Playback Switch"   , AC97_PCM  , 14, 1, 1),
SOC_DAPM_SINGLE("Phone Switch"          , AC97_PHONE, 14, 1, 1),
};

/* Capture source left */
static const struct snd_kcontrol_new wm9715_rec_srcl_mux_controls =
SOC_DAPM_ENUM("Route", wm9715_enum[3]);

/* Capture source right */
static const struct snd_kcontrol_new wm9715_rec_srcr_mux_controls =
SOC_DAPM_ENUM("Route", wm9715_enum[4]);

/* LineOut mux */
static const struct snd_kcontrol_new wm9715_lineout_mux_controls =
SOC_DAPM_ENUM("Route", wm9715_enum[20]);

/* capture mux */
static const struct snd_kcontrol_new wm9715_hp_rec_mux_controls =
SOC_DAPM_ENUM("Route", wm9715_enum[1]);

/* Out3 mux */
static const struct snd_kcontrol_new wm9715_out3_mux_controls =
SOC_DAPM_ENUM("Route", wm9715_enum[12]);

/* mic source */
static const struct snd_kcontrol_new wm9715_mic_sel_mux_controls =
SOC_DAPM_ENUM("Route", wm9715_enum[18]);

/* mic mux */
//?? static const struct snd_kcontrol_new wm9715_hp_mic_mux_controls =
//?? SOC_DAPM_ENUM("Route", wm9715_enum[0]);

/* mono mic mux */
//?? static const struct snd_kcontrol_new wm9715_mono_mic_mux_controls =
//?? SOC_DAPM_ENUM("Route", wm9715_enum[2]);

/* mono output mux */
//?? static const struct snd_kcontrol_new wm9715_mono_mux_controls =
//?? SOC_DAPM_ENUM("Route", wm9715_enum[7]);

/* speaker left output mux */
//?? static const struct snd_kcontrol_new wm9715_hp_spkl_mux_controls =
//?? SOC_DAPM_ENUM("Route", wm9715_enum[8]);

/* speaker right output mux */
//?? static const struct snd_kcontrol_new wm9715_hp_spkr_mux_controls =
//?? SOC_DAPM_ENUM("Route", wm9715_enum[9]);

/* headphone left output mux */
//?? static const struct snd_kcontrol_new wm9715_hpl_out_mux_controls =
//?? SOC_DAPM_ENUM("Route", wm9715_enum[10]);

/* headphone right output mux */
//?? static const struct snd_kcontrol_new wm9715_hpr_out_mux_controls =
//?? SOC_DAPM_ENUM("Route", wm9715_enum[11]);

/* Out4 mux */
//?? static const struct snd_kcontrol_new wm9715_out4_mux_controls =
//?? SOC_DAPM_ENUM("Route", wm9715_enum[13]);

/* DAC inv mux 1 */
//?? static const struct snd_kcontrol_new wm9715_dac_inv1_mux_controls =
//?? SOC_DAPM_ENUM("Route", wm9715_enum[14]);

/* DAC inv mux 2 */
//?? static const struct snd_kcontrol_new wm9715_dac_inv2_mux_controls =
//?? SOC_DAPM_ENUM("Route", wm9715_enum[15]);

/* mic source B virtual control */
//?? static const struct snd_kcontrol_new wm9715_micb_sel_mux_controls =
//?? SOC_DAPM_ENUM("Route", wm9715_enum[19]);


static const struct snd_soc_dapm_widget wm9715_dapm_widgets[] = {

SND_SOC_DAPM_MIXER("HeadPhone Mixer"      , SND_SOC_NOPM, 0, 0, &wm9715_hp_mixer_controls[0]     , ARRAY_SIZE(wm9715_hp_mixer_controls)     ),
SND_SOC_DAPM_MIXER("Phone Mixer"          , SND_SOC_NOPM, 0, 0, &wm9715_phone_mixer_controls[0]  , ARRAY_SIZE(wm9715_phone_mixer_controls)  ),
SND_SOC_DAPM_MIXER("Speaker Mixer"        , SND_SOC_NOPM, 0, 0, &wm9715_speaker_mixer_controls[0], ARRAY_SIZE(wm9715_speaker_mixer_controls)),
                                          
SND_SOC_DAPM_MUX  ("LineOut Source"       , SND_SOC_NOPM, 0, 0,	&wm9715_lineout_mux_controls  ),

SND_SOC_DAPM_MUX  ("Capture Headphone Mux", SND_SOC_NOPM, 0, 0, &wm9715_hp_rec_mux_controls   ),
SND_SOC_DAPM_MUX  ("Mic Source"           , SND_SOC_NOPM, 0, 0, &wm9715_mic_sel_mux_controls  ),
SND_SOC_DAPM_MUX  ("Out3 Mux"             , SND_SOC_NOPM, 0, 0, &wm9715_out3_mux_controls     ),

//SND_SOC_DAPM_MUX  ("Left Capture Source"  , SND_SOC_NOPM, 0, 0,	&wm9715_rec_srcl_mux_controls ),
//SND_SOC_DAPM_MUX  ("Right Capture Source" , SND_SOC_NOPM, 0, 0,	&wm9715_rec_srcr_mux_controls ),
//SND_SOC_DAPM_ADC  ("Left HiFi ADC"  , "Left HiFi Capture"  , SND_SOC_NOPM, 0, 0),
//SND_SOC_DAPM_ADC  ("Right HiFi ADC" , "Right HiFi Capture" , SND_SOC_NOPM, 0, 0),

//SND_SOC_DAPM_MIXER("AC97 Mixer"     , SND_SOC_NOPM, 0, 0, NULL, 0),
//SND_SOC_DAPM_MIXER("HP Mixer"       , SND_SOC_NOPM, 0, 0, NULL, 0),
//SND_SOC_DAPM_MIXER("Line Mixer"     , SND_SOC_NOPM, 0, 0, NULL, 0),
//SND_SOC_DAPM_MIXER("Capture Mixer"  , SND_SOC_NOPM, 0, 0, NULL, 0),

//SND_SOC_DAPM_OUTPUT("MONO"  ),
//SND_SOC_DAPM_OUTPUT("HPL"   ),
//SND_SOC_DAPM_OUTPUT("HPR"   ),
//SND_SOC_DAPM_OUTPUT("OUT3"  ),
//SND_SOC_DAPM_INPUT ("LINEL" ),
//SND_SOC_DAPM_INPUT ("LINER" ),
//SND_SOC_DAPM_INPUT ("MONOIN"),
//SND_SOC_DAPM_INPUT ("PCBEEP"),
//SND_SOC_DAPM_INPUT ("MIC1"  ),
//SND_SOC_DAPM_INPUT ("MIC2"  ),


//SND_SOC_DAPM_MIXER_E("Left HP Mixer", AC97_EXTENDED_MID, 3, 1,
//	&wm9715_hpl_mixer_controls[0], ARRAY_SIZE(wm9715_hpl_mixer_controls),
//	mixer_event, SND_SOC_DAPM_POST_REG),
//SND_SOC_DAPM_MIXER_E("Right HP Mixer", AC97_EXTENDED_MID, 2, 1,
//	&wm9715_hpr_mixer_controls[0], ARRAY_SIZE(wm9715_hpr_mixer_controls),
//	mixer_event, SND_SOC_DAPM_POST_REG),

//SND_SOC_DAPM_MUX("Sidetone Mux", SND_SOC_NOPM, 0, 0,	&wm9715_hp_mic_mux_controls),
//SND_SOC_DAPM_MUX("Capture Mono Mux", SND_SOC_NOPM, 0, 0, &wm9715_mono_mic_mux_controls),
//SND_SOC_DAPM_MUX("Mono Out Mux", SND_SOC_NOPM, 0, 0, &wm9715_mono_mux_controls),
//SND_SOC_DAPM_MUX("Left Speaker Out Mux", SND_SOC_NOPM, 0, 0,	&wm9715_hp_spkl_mux_controls),
//SND_SOC_DAPM_MUX("Right Speaker Out Mux", SND_SOC_NOPM, 0, 0,	&wm9715_hp_spkr_mux_controls),
//SND_SOC_DAPM_MUX("Left Headphone Out Mux", SND_SOC_NOPM, 0, 0,	&wm9715_hpl_out_mux_controls),
//SND_SOC_DAPM_MUX("Right Headphone Out Mux", SND_SOC_NOPM, 0, 0,	&wm9715_hpr_out_mux_controls),
//SND_SOC_DAPM_MUX("Out 4 Mux", SND_SOC_NOPM, 0, 0,	&wm9715_out4_mux_controls),
//SND_SOC_DAPM_MUX("DAC Inv Mux 1", SND_SOC_NOPM, 0, 0, &wm9715_dac_inv1_mux_controls),
//SND_SOC_DAPM_MUX("DAC Inv Mux 2", SND_SOC_NOPM, 0, 0, &wm9715_dac_inv2_mux_controls),
//SND_SOC_DAPM_MUX("Mic B Source", SND_SOC_NOPM, 0, 0, &wm9715_micb_sel_mux_controls),
//SND_SOC_DAPM_MICBIAS("Mic Bias", AC97_WM9715_REG_5C, 5, 1),

//SND_SOC_DAPM_DAC("Left DAC", "Left HiFi Playback", AC97_EXTENDED_MID, 7, 1),
//SND_SOC_DAPM_DAC("Right DAC", "Right HiFi Playback", AC97_EXTENDED_MID, 6, 1),
//SND_SOC_DAPM_DAC("Aux DAC", "Aux Playback", AC97_EXTENDED_MID, 11, 1),
//SND_SOC_DAPM_PGA("Left ADC", AC97_EXTENDED_MID, 5, 1, NULL, 0),
//SND_SOC_DAPM_PGA("Right ADC", AC97_EXTENDED_MID, 4, 1, NULL, 0),
//SND_SOC_DAPM_PGA("Left Headphone", AC97_EXTENDED_MSTATUS, 10, 1, NULL, 0),
//SND_SOC_DAPM_PGA("Right Headphone", AC97_EXTENDED_MSTATUS, 9, 1, NULL, 0),
//SND_SOC_DAPM_PGA("Left Speaker", AC97_EXTENDED_MSTATUS, 8, 1, NULL, 0),
//SND_SOC_DAPM_PGA("Right Speaker", AC97_EXTENDED_MSTATUS, 7, 1, NULL, 0),
//SND_SOC_DAPM_PGA("Out 3", AC97_EXTENDED_MSTATUS, 11, 1, NULL, 0),
//SND_SOC_DAPM_PGA("Out 4", AC97_EXTENDED_MSTATUS, 12, 1, NULL, 0),
//SND_SOC_DAPM_PGA("Mono Out", AC97_EXTENDED_MSTATUS, 13, 1, NULL, 0),
//SND_SOC_DAPM_PGA("Left Line In", AC97_EXTENDED_MSTATUS, 6, 1, NULL, 0),
//SND_SOC_DAPM_PGA("Right Line In", AC97_EXTENDED_MSTATUS, 5, 1, NULL, 0),
//SND_SOC_DAPM_PGA("Mono In", AC97_EXTENDED_MSTATUS, 4, 1, NULL, 0),
//SND_SOC_DAPM_PGA("Mic A PGA", AC97_EXTENDED_MSTATUS, 3, 1, NULL, 0),
//SND_SOC_DAPM_PGA("Mic B PGA", AC97_EXTENDED_MSTATUS, 2, 1, NULL, 0),
//SND_SOC_DAPM_PGA("Mic A Pre Amp", AC97_EXTENDED_MSTATUS, 1, 1, NULL, 0),
//SND_SOC_DAPM_PGA("Mic B Pre Amp", AC97_EXTENDED_MSTATUS, 0, 1, NULL, 0),

//SND_SOC_DAPM_OUTPUT("SPKL"),
//SND_SOC_DAPM_OUTPUT("SPKR"),
//SND_SOC_DAPM_OUTPUT("OUT4"),
//SND_SOC_DAPM_INPUT("MIC2A"),
//SND_SOC_DAPM_INPUT("MIC2B"),
//SND_SOC_DAPM_VMID("VMID"),
};

static const struct snd_soc_dapm_route audio_map[] = 
{
	//{"Left Headphone Out Mux", "Headphone", "Left HP Mixer"},	
};

static int wm9715_add_widgets(struct snd_soc_codec *codec)
{
	snd_soc_dapm_new_controls(codec, wm9715_dapm_widgets, ARRAY_SIZE(wm9715_dapm_widgets));

	//snd_soc_dapm_add_routes(codec, audio_map, ARRAY_SIZE(audio_map));
	return 0;
}

static unsigned int ac97_read(struct snd_soc_codec *codec,
	unsigned int reg)
{
	u16 *cache = codec->reg_cache;

	if (reg == AC97_RESET || reg == AC97_GPIO_STATUS ||
		reg == AC97_VENDOR_ID1 || reg == AC97_VENDOR_ID2 )
		return soc_ac97_ops.read(codec->ac97, reg);
	else {
		reg = reg >> 1;

		if (reg >= (ARRAY_SIZE(wm9715_reg)))
			return -EIO;

		return cache[reg];
	}
}

static int ac97_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int val)
{
	u16 *cache = codec->reg_cache;
	if (reg < 0x7c)
		soc_ac97_ops.write(codec->ac97, reg, val);
	reg = reg >> 1;
	if (reg < (ARRAY_SIZE(wm9715_reg)))
		cache[reg] = val;

	return 0;
}

/* PLL divisors */
struct _pll_div {
	u32 divsel:1;
	u32 divctl:1;
	u32 lf:1;
	u32 n:4;
	u32 k:24;
};

/* The size in bits of the PLL divide multiplied by 10
 * to allow rounding later */
#define FIXED_PLL_SIZE ((1 << 22) * 10)

static void pll_factors(struct _pll_div *pll_div, unsigned int source)
{
	u64 Kpart;
	unsigned int K, Ndiv, Nmod, target;

	/* The the PLL output is always 98.304MHz. */
	target = 98304000;

	/* If the input frequency is over 14.4MHz then scale it down. */
	if (source > 14400000) {
		source >>= 1;
		pll_div->divsel = 1;

		if (source > 14400000) {
			source >>= 1;
			pll_div->divctl = 1;
		} else
			pll_div->divctl = 0;

	} else {
		pll_div->divsel = 0;
		pll_div->divctl = 0;
	}

	/* Low frequency sources require an additional divide in the
	 * loop.
	 */
	if (source < 8192000) {
		pll_div->lf = 1;
		target >>= 2;
	} else
		pll_div->lf = 0;

	Ndiv = target / source;
	if ((Ndiv < 5) || (Ndiv > 12))
		printk(KERN_WARNING
			"wm9715 PLL N value %u out of recommended range!\n",
			Ndiv);

	pll_div->n = Ndiv;
	Nmod = target % source;
	Kpart = FIXED_PLL_SIZE * (long long)Nmod;

	do_div(Kpart, source);

	K = Kpart & 0xFFFFFFFF;

	/* Check if we need to round */
	if ((K % 10) >= 5)
		K += 5;

	/* Move down to proper range now rounding is done */
	K /= 10;

	pll_div->k = K;
}

/**
 * Please note that changing the PLL input frequency may require
 * resynchronisation with the AC97 controller.
 */
static int wm9715_set_pll(struct snd_soc_codec *codec,
	int pll_id, unsigned int freq_in, unsigned int freq_out)
{
	struct wm9715_priv *wm9715 = snd_soc_codec_get_drvdata(codec);
	u16 reg, reg2;
	struct _pll_div pll_div;

	/* turn PLL off ? */
	if (freq_in == 0) {
		/* disable PLL power and select ext source */
		reg = ac97_read(codec, AC97_HANDSET_RATE);
		ac97_write(codec, AC97_HANDSET_RATE, reg | 0x0080);
		//reg = ac97_read(codec, AC97_EXTENDED_MID);
		//ac97_write(codec, AC97_EXTENDED_MID, reg | 0x0200);
		wm9715->pll_in = 0;
		return 0;
	}

	pll_factors(&pll_div, freq_in);

	if (pll_div.k == 0) {
		reg = (pll_div.n << 12) | (pll_div.lf << 11) |
			(pll_div.divsel << 9) | (pll_div.divctl << 8);
		ac97_write(codec, AC97_LINE1_LEVEL, reg);
	} else {
		/* write the fractional k to the reg 0x46 pages */
		reg2 = (pll_div.n << 12) | (pll_div.lf << 11) | (1 << 10) |
			(pll_div.divsel << 9) | (pll_div.divctl << 8);

		/* K [21:20] */
		reg = reg2 | (0x5 << 4) | (pll_div.k >> 20);
		ac97_write(codec, AC97_LINE1_LEVEL, reg);

		/* K [19:16] */
		reg = reg2 | (0x4 << 4) | ((pll_div.k >> 16) & 0xf);
		ac97_write(codec, AC97_LINE1_LEVEL, reg);

		/* K [15:12] */
		reg = reg2 | (0x3 << 4) | ((pll_div.k >> 12) & 0xf);
		ac97_write(codec, AC97_LINE1_LEVEL, reg);

		/* K [11:8] */
		reg = reg2 | (0x2 << 4) | ((pll_div.k >> 8) & 0xf);
		ac97_write(codec, AC97_LINE1_LEVEL, reg);

		/* K [7:4] */
		reg = reg2 | (0x1 << 4) | ((pll_div.k >> 4) & 0xf);
		ac97_write(codec, AC97_LINE1_LEVEL, reg);

		reg = reg2 | (0x0 << 4) | (pll_div.k & 0xf); /* K [3:0] */
		ac97_write(codec, AC97_LINE1_LEVEL, reg);
	}

	/* turn PLL on and select as source */
	//reg = ac97_read(codec, AC97_EXTENDED_MID);
	//ac97_write(codec, AC97_EXTENDED_MID, reg & 0xfdff);
	reg = ac97_read(codec, AC97_HANDSET_RATE);
	ac97_write(codec, AC97_HANDSET_RATE, reg & 0xff7f);
	wm9715->pll_in = freq_in;

	/* wait 10ms AC97 link frames for the link to stabilise */
	schedule_timeout_interruptible(msecs_to_jiffies(10));
	return 0;
}

static int wm9715_set_dai_pll(struct snd_soc_dai *codec_dai, int pll_id,
		int source, unsigned int freq_in, unsigned int freq_out)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	return wm9715_set_pll(codec, pll_id, freq_in, freq_out);
}

/*
 * Tristate the PCM DAI lines, tristate can be disabled by calling
 * wm9715_set_dai_fmt()
 *
static int wm9715_set_dai_tristate(struct snd_soc_dai *codec_dai, int tristate )
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u16 reg = ac97_read(codec, AC97_CENTER_LFE_MASTER) & 0x9fff;

	if (tristate)
		ac97_write(codec, AC97_CENTER_LFE_MASTER, reg);

	return 0;
}
*/

/*
 * Configure wm9715 clock dividers.
 * Voice DAC needs 256 FS
 */
static int wm9715_set_dai_clkdiv(struct snd_soc_dai *codec_dai,
		int div_id, int div)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u16 reg;

	switch (div_id) {
	case WM9715_PCMCLK_DIV:
		reg = ac97_read(codec, AC97_HANDSET_RATE) & 0xf0ff;
		ac97_write(codec, AC97_HANDSET_RATE, reg | div);
		break;
	case WM9715_CLKA_MULT:
		reg = ac97_read(codec, AC97_HANDSET_RATE) & 0xfffd;
		ac97_write(codec, AC97_HANDSET_RATE, reg | div);
		break;
	case WM9715_CLKB_MULT:
		reg = ac97_read(codec, AC97_HANDSET_RATE) & 0xfffb;
		ac97_write(codec, AC97_HANDSET_RATE, reg | div);
		break;
	case WM9715_HIFI_DIV:
		reg = ac97_read(codec, AC97_HANDSET_RATE) & 0x8fff;
		ac97_write(codec, AC97_HANDSET_RATE, reg | div);
		break;
	case WM9715_PCMBCLK_DIV:
		reg = ac97_read(codec, AC97_CENTER_LFE_MASTER) & 0xf1ff;
		ac97_write(codec, AC97_CENTER_LFE_MASTER, reg | div);
		break;
	case WM9715_PCMCLK_PLL_DIV:
		reg = ac97_read(codec, AC97_LINE1_LEVEL) & 0xff80;
		ac97_write(codec, AC97_LINE1_LEVEL, reg | 0x60 | div);
		break;
	case WM9715_HIFI_PLL_DIV:
		reg = ac97_read(codec, AC97_LINE1_LEVEL) & 0xff80;
		ac97_write(codec, AC97_LINE1_LEVEL, reg | 0x70 | div);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/*
static int wm9715_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u16 gpio = ac97_read(codec, AC97_GPIO_CFG) & 0xffc5;
	u16 reg = 0x8000;

	// clock masters 
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		reg |= 0x4000;
		gpio |= 0x0010;
		break;
	case SND_SOC_DAIFMT_CBM_CFS:
		reg |= 0x6000;
		gpio |= 0x0018;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		reg |= 0x2000;
		gpio |= 0x001a;
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
		gpio |= 0x0012;
		break;
	}

	// clock inversion 
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_IB_IF:
		reg |= 0x00c0;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		reg |= 0x0080;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		reg |= 0x0040;
		break;
	}

	// DAI format 
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		reg |= 0x0002;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		reg |= 0x0001;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		reg |= 0x0003;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		reg |= 0x0043;
		break;
	}

	ac97_write(codec, AC97_GPIO_CFG, gpio);
	ac97_write(codec, AC97_CENTER_LFE_MASTER, reg);
	return 0;
}
*/

/*
static int wm9715_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 reg = ac97_read(codec, AC97_CENTER_LFE_MASTER) & 0xfff3;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		reg |= 0x0004;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		reg |= 0x0008;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		reg |= 0x000c;
		break;
	}

	// enable PCM interface in master mode
	ac97_write(codec, AC97_CENTER_LFE_MASTER, reg);
	return 0;
}
*/

static int ac97_hifi_prepare(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct snd_pcm_runtime *runtime = substream->runtime;
	int reg;
	u16 vra;

	vra = ac97_read(codec, AC97_EXTENDED_STATUS);
	ac97_write(codec, AC97_EXTENDED_STATUS, vra | 0x1);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		reg = AC97_PCM_FRONT_DAC_RATE;
	else
		reg = AC97_PCM_LR_ADC_RATE;

	// something todo

	return ac97_write(codec, reg, runtime->rate);
}

static int ac97_aux_prepare(struct snd_pcm_substream *substream,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct snd_pcm_runtime *runtime = substream->runtime;
	u16 vra, xsle;

	vra = ac97_read(codec, AC97_EXTENDED_STATUS);
	ac97_write(codec, AC97_EXTENDED_STATUS, vra | 0x1);
	xsle = ac97_read(codec, AC97_PCI_SID);
	ac97_write(codec, AC97_PCI_SID, xsle | 0x8000);

	if (substream->stream != SNDRV_PCM_STREAM_PLAYBACK)
		return -ENODEV;

	return ac97_write(codec, AC97_PCM_SURR_DAC_RATE, runtime->rate);
}

#define WM9715_RATES (SNDRV_PCM_RATE_8000  |	\
		      SNDRV_PCM_RATE_11025 |	\
		      SNDRV_PCM_RATE_22050 |	\
		      SNDRV_PCM_RATE_44100 |	\
		      SNDRV_PCM_RATE_48000)

#define WM9715_PCM_RATES (SNDRV_PCM_RATE_8000  |	\
			  SNDRV_PCM_RATE_11025 |	\
			  SNDRV_PCM_RATE_16000 |	\
			  SNDRV_PCM_RATE_22050 |	\
			  SNDRV_PCM_RATE_44100 |	\
			  SNDRV_PCM_RATE_48000)

#define WM9715_PCM_FORMATS \
	(SNDRV_PCM_FORMAT_S16_LE | SNDRV_PCM_FORMAT_S20_3LE | \
	 SNDRV_PCM_FORMAT_S24_LE)

static struct snd_soc_dai_ops wm9715_dai_ops_hifi = {
	.prepare	= ac97_hifi_prepare,
	.set_clkdiv	= wm9715_set_dai_clkdiv,
	.set_pll	= wm9715_set_dai_pll,
};

static struct snd_soc_dai_ops wm9715_dai_ops_aux = {
	.prepare	= ac97_aux_prepare,
	.set_clkdiv	= wm9715_set_dai_clkdiv,
	.set_pll	= wm9715_set_dai_pll,
};

struct snd_soc_dai wm9715_dai[] = {
	{
		.name = "AC97 HiFi",
		.ac97_control = 1,
		.playback = {
			.stream_name = "HiFi Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = WM9715_RATES,
			.formats = SND_SOC_STD_AC97_FMTS,},
		.capture = {
			.stream_name = "HiFi Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = WM9715_RATES,
			.formats = SND_SOC_STD_AC97_FMTS,},
		.ops = &wm9715_dai_ops_hifi,
	},
	{
	.name = "AC97 Aux",
	.playback = {
		.stream_name = "Aux Playback",
		.channels_min = 1,
		.channels_max = 1,
		.rates = WM9715_RATES,
		.formats = SND_SOC_STD_AC97_FMTS,},
	.ops = &wm9715_dai_ops_aux,
	},
};
EXPORT_SYMBOL_GPL(wm9715_dai);

int wm9715_reset(struct snd_soc_codec *codec, int try_warm)
{
	if (try_warm && soc_ac97_ops.warm_reset) {
		soc_ac97_ops.warm_reset(codec->ac97);
		if (ac97_read(codec, 0) == wm9715_reg[0])
			return 1;
	}

	soc_ac97_ops.reset(codec->ac97);
	if (soc_ac97_ops.warm_reset)
		soc_ac97_ops.warm_reset(codec->ac97);
	if (ac97_read(codec, 0) != wm9715_reg[0])
		return -EIO;
	return 0;
}
EXPORT_SYMBOL_GPL(wm9715_reset);

static int wm9715_set_bias_level(struct snd_soc_codec *codec,
				 enum snd_soc_bias_level level)
{
//	u16 reg;

	switch (level) {
	case SND_SOC_BIAS_ON:
		/* enable thermal shutdown */
		//reg = ac97_read(codec, AC97_EXTENDED_MID) & 0x1bff;
		//ac97_write(codec, AC97_EXTENDED_MID, reg);
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		/* enable master bias and vmid */
		//reg = ac97_read(codec, AC97_EXTENDED_MID) & 0x3bff;
		//ac97_write(codec, AC97_EXTENDED_MID, reg);
		ac97_write(codec, AC97_POWERDOWN, 0x0000);
		break;
	case SND_SOC_BIAS_OFF:
		/* disable everything including AC link */
		//ac97_write(codec, AC97_EXTENDED_MID, 0xffff);
		//ac97_write(codec, AC97_EXTENDED_MSTATUS, 0xffff);
		ac97_write(codec, AC97_POWERDOWN, 0xffff);
		break;
	}
	codec->bias_level = level;
	return 0;
}

static int wm9715_soc_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;
//	u16 reg;

	/* Disable everything except touchpanel - that will be handled
	 * by the touch driver and left disabled if touch is not in
	 * use. */
	//reg = ac97_read(codec, AC97_EXTENDED_MID);
	//ac97_write(codec, AC97_EXTENDED_MID, reg | 0x7fff);
	//ac97_write(codec, AC97_EXTENDED_MSTATUS, 0xffff);
	ac97_write(codec, AC97_POWERDOWN, 0x6f00);
	ac97_write(codec, AC97_POWERDOWN, 0xffff);

	return 0;
}

static int wm9715_soc_resume(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;
	struct wm9715_priv *wm9715 = snd_soc_codec_get_drvdata(codec);
	int i, ret;
	u16 *cache = codec->reg_cache;

	ret = wm9715_reset(codec, 1);
	if (ret < 0) {
		printk(KERN_ERR "could not reset AC97 codec\n");
		return ret;
	}

	wm9715_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	/* do we need to re-start the PLL ? */
	if (wm9715->pll_in)
		wm9715_set_pll(codec, 0, wm9715->pll_in, 0);

	/* only synchronise the codec if warm reset failed */
	if (ret == 0) {
		for (i = 2; i < ARRAY_SIZE(wm9715_reg) << 1; i += 2) {
			if (i == AC97_POWERDOWN  /* || i == AC97_EXTENDED_MID || i == AC97_EXTENDED_MSTATUS */
				 || i > 0x66)
				continue;
			soc_ac97_ops.write(codec->ac97, i, cache[i>>1]);
		}
	}

	return ret;
}

// add proc ================================================================

static struct snd_soc_codec  *fa_snd_codec = NULL;
static int    show_debug = 0;

int falinux_ac97_write( unsigned int reg, unsigned int val )
{
	if ( fa_snd_codec )
	{
		soc_ac97_ops.write(fa_snd_codec->ac97, reg, val );
		return 0;
		//return ac97_write( fa_snd_codec, reg, val);
	}
	
	return -1;
}
EXPORT_SYMBOL(falinux_ac97_write);

int falinux_ac97_read( int reg )
{
	if ( fa_snd_codec )
	{
		return soc_ac97_ops.read(fa_snd_codec->ac97, reg);
		//return ac97_read( fa_snd_codec, reg );
	}
	
	return -1;
}
EXPORT_SYMBOL(falinux_ac97_read);

//------------------------------------------------------------------------------
/** @brief   proc 쓰기를 지원한다.
    @remark
*///----------------------------------------------------------------------------
static int falinux_snd_proc_cmd( struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	char     cmd[256], *ptr;
	int      rtn, len = count;
	unsigned int reg, val;
	//u16     *wm_regs = wm9715_reg;

	if ( len > 256 ) len = 256;
	rtn = copy_from_user( cmd, buffer, len );
	cmd[len-1] = 0; // CR 제거

	if( strncmp( "regw=", cmd, 5 ) == 0 )
	{
		reg = simple_strtoul( &cmd[5], &ptr, 0 );
		if ( ptr )
		{
			ptr ++;
			val = simple_strtoul( ptr, NULL, 0 );
			
			falinux_ac97_write( reg, val );
			//wm_regs[reg] = val;
			if ( show_debug ) printk( "\nac97_write reg=0x%02x, val=0x%04x\n", reg, val );
		}
		return count;
	}
	
	if( strncmp( "regr=", cmd, 5 ) == 0 )
	{
		reg = simple_strtoul( &cmd[5], NULL, 0 );
		val = falinux_ac97_read( reg );
		
		if ( show_debug ) printk( "\nac97_read reg=0x%02x  val=0x%04x\n", reg, val );
		return count;
	}

	if( strncmp( "msgview=", cmd, 8 ) == 0 )
	{
		show_debug = cmd[8] & 0x1;
		return count;
	}
	else if( strncmp( "help", cmd, 4 ) == 0 )
	{
		printk( "  regw=0x00,0x0000\n"  \
		        "  regr=0x00\n"  \
		        "  msgview=0|1\n"  \
		        "\n" );
		return count;
	}
	
	return count;
}

static int falinux_snd_proc_read(char *buf, char **start, off_t fpos, int lenght, int *eof, void *data)
{
	char *p;
	int   reg, val[4];
	
	p = buf;
	p += sprintf(p, "\nfalinux-ac97 reg : val\n\n" );
	for ( reg=0; reg<0x80; reg+=8 )
	{
		val[0] = falinux_ac97_read( reg   );
		val[1] = falinux_ac97_read( reg+2 );		
		val[2] = falinux_ac97_read( reg+4 );		
		val[3] = falinux_ac97_read( reg+6 );
		p += sprintf(p, " %02x : %04x %04x  %04x %04x\n", reg, val[0], val[1], val[2], val[3] );
	}
	p += sprintf(p, "\n");
	*eof = 1;
	
	return p - buf;
}

//------------------------------------------------------------------------------
/** @brief   proc
    @remark
*///----------------------------------------------------------------------------
#include <linux/proc_fs.h>

static void falinux_snd_register_proc( void )
{
	struct proc_dir_entry *procdir;

	procdir = create_proc_entry("falinux-wm9715", S_IFREG | S_IRUGO, 0);
	procdir->read_proc  = falinux_snd_proc_read;
	procdir->write_proc = falinux_snd_proc_cmd;
}

#ifdef CONFIG_FALINUX_ZEROBOOT
static u16 zb_wm9715_reg[4*17];

void zb_ac97_codec_init( void )
{
	int  reg, count, ret;
	u16 *cache = (u16 *)fa_snd_codec->reg_cache;

	wm9715_reset(fa_snd_codec, 0);
	ret = wm9715_reset(fa_snd_codec, 1);
	if (ret < 0) 
	{
		printk( "Failed to reset WM9715: AC97 link error\n");
		return;
	}
	
	count = sizeof(wm9715_reg)/sizeof(u16);
	for (reg=0; reg<count; reg++)
	{
		if ( zb_wm9715_reg[reg] != cache[reg] )
		{	
			falinux_ac97_write( reg<<1, cache[reg] );
			//printk( "ac97 reg=%d 0x%04x\n", reg<<1, cache[reg] );
		}
	}
}
EXPORT_SYMBOL( zb_ac97_codec_init );
#endif

// end proc =================================================================


static int wm9715_soc_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec;
	int ret = 0, reg;

	socdev->card->codec = kzalloc(sizeof(struct snd_soc_codec),
				      GFP_KERNEL);
	if (socdev->card->codec == NULL)
		return -ENOMEM;
	codec = socdev->card->codec;
	mutex_init(&codec->mutex);

	codec->reg_cache = kmemdup(wm9715_reg, sizeof(wm9715_reg), GFP_KERNEL);
	if (codec->reg_cache == NULL) {
		ret = -ENOMEM;
		goto cache_err;
	}
	codec->reg_cache_size = sizeof(wm9715_reg);
	codec->reg_cache_step = 2;

	snd_soc_codec_set_drvdata(codec, kzalloc(sizeof(struct wm9715_priv),
						 GFP_KERNEL));
	if (snd_soc_codec_get_drvdata(codec) == NULL) {
		ret = -ENOMEM;
		goto priv_err;
	}

	codec->name = "wm9715";
	codec->owner = THIS_MODULE;
	codec->dai = wm9715_dai;
	codec->num_dai = ARRAY_SIZE(wm9715_dai);
	codec->write = ac97_write;
	codec->read = ac97_read;
	codec->set_bias_level = wm9715_set_bias_level;
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);

#ifdef CONFIG_FALINUX_ZEROBOOT
	memcpy( (void *)zb_wm9715_reg, (void *)codec->reg_cache, sizeof(wm9715_reg) );
#endif

	ret = snd_soc_new_ac97_codec(codec, &soc_ac97_ops, 0);
	if (ret < 0)
		goto codec_err;

	/* register pcms */
	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0)
		goto pcm_err;

	/* do a cold reset for the controller and then try
	 * a warm reset followed by an optional cold reset for codec */
	wm9715_reset(codec, 0);
	ret = wm9715_reset(codec, 1);
	if (ret < 0) {
		printk(KERN_ERR "Failed to reset wm9715: AC97 link error\n");
		goto reset_err;
	}

	wm9715_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	/* unmute the adc - move to kcontrol */
	//reg = ac97_read(codec, AC97_CD) & 0x7fff;
	//ac97_write(codec, AC97_CD, reg);

	// DAC path to Headphone mixer ON
	reg = ac97_read(codec, AC97_PCM) & 0x7fff;
	ac97_write(codec, AC97_PCM, reg);

	// Headphone Mute=off vol=75%
	//reg  = ac97_read(codec, AC97_HEADPHONE) & ( 0x7fff & 0xc0c0 );
	//reg |= 0x1010;
	reg = 0x0707;
	ac97_write(codec, AC97_HEADPHONE, reg);
	
	// LineOut Mute=off   vol=75%
	//reg = ac97_read(codec, AC97_WM9715_LINEOUT) & ( 0x7fff & 0xc0c0 );
	//reg |= 0x1010;
	reg = 0x0707;
	ac97_write(codec, AC97_WM9715_LINEOUT, reg);

	// mic-2 is slot-3
	reg = ac97_read(codec, AC97_WM9715_REG_5C) & 0xfffc;
	ac97_write(codec, AC97_CD, reg);

	snd_soc_add_controls(codec, wm9715_snd_ac97_controls,
				ARRAY_SIZE(wm9715_snd_ac97_controls));
	wm9715_add_widgets(codec);

	// for proc
	fa_snd_codec = codec;	
	falinux_snd_register_proc();

	return 0;

reset_err:
	snd_soc_free_pcms(socdev);
pcm_err:
	snd_soc_free_ac97_codec(codec);

codec_err:
	kfree(snd_soc_codec_get_drvdata(codec));

priv_err:
	kfree(codec->reg_cache);

cache_err:
	kfree(socdev->card->codec);
	socdev->card->codec = NULL;
	return ret;
}

static int wm9715_soc_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;

	if (codec == NULL)
		return 0;

	snd_soc_dapm_free(socdev);
	snd_soc_free_pcms(socdev);
	snd_soc_free_ac97_codec(codec);
	kfree(snd_soc_codec_get_drvdata(codec));
	kfree(codec->reg_cache);
	kfree(codec);
	
	fa_snd_codec = NULL;			
	return 0;
}

struct snd_soc_codec_device soc_codec_dev_wm9715 = {
	.probe = 	wm9715_soc_probe,
	.remove = 	wm9715_soc_remove,
	.suspend =	wm9715_soc_suspend,
	.resume = 	wm9715_soc_resume,
};
EXPORT_SYMBOL_GPL(soc_codec_dev_wm9715);

MODULE_DESCRIPTION("ASoC wm9715 driver");
MODULE_AUTHOR("JaeKyong OH");
MODULE_LICENSE("GPL");



/* 초기에 해야할 일 


   0x5C d[1..0] = 00b 		slot-3

*/



