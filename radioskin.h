/*
 * radioskin.h - part of radio.c, a plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __RADIO_SKIN_H
#define __RADIO_SKIN_H


enum eRadioSkin
{
  eRadioSkinDummy,
  eRadioSkinClassicDefault,
  eRadioSkinSttngDefault,
  eRadioSkinEgalsTryDefault,
  eRadioSkinEgalsTryBlue,
  eRadioSkinEnigmaDefault,
  eRadioSkinEnigmaDarkBlue,
  eRadioSkinEnigmaWineRed,
  eRadioSkinEnigmaAppleGreen,
  eRadioSkinEnigmaWomensLike,
  eRadioSkinEnigmaYellowSun,
  eRadioSkinEnigmaBlack,
  eRadioSkinEnigmaBlue,
  eRadioSkinEnigmaBlue2,
  eRadioSkinEnigmaBlue3,
  eRadioSkinEnigmaCoolblue,
  eRadioSkinEnigmaGrey,
  eRadioSkinEnigmaMoBuntu,
  eRadioSkinEnigmabgw,
  eRadioSkinDeepBlueDefault,
  eRadioSkinSilverGreenDefault,
  eRadioSkinLightBlueDefault,
  eRadioSkinSoppalusikkaDefault,
  eRadioSkinSoppalusikkaMint,
  eRadioSkinSoppalusikkaOrange,
  eRadioSkinSoppalusikkaVanilla,
  eRadioSkinSoppalusikkaBlackberry,
  eRadioSkinSoppalusikkaCitron,
  eRadioSkinElchiDefault,
  eRadioSkinElchiMVBlack,
  eRadioSkinElchiMVWhite,
  eRadioSkinElchiMoose,
  eRadioSkinElchiPluginDefault,
  eRadioSkinEgalOrangeDefault,
  eRadioSkinMoronimoDefault,
  eRadioSkinDuotoneDefault,
  eRadioSkinEgalT2Default,
  eRadioSkinEgalT2BluYell,
  eRadioSkinEgalT2Purple,
  eRadioSkinMaxNumber
};

struct cRadioSkin
{
  const char   *name;            // Theme~Skin
  unsigned int clrTitleBack;     // Title-Background
  unsigned int clrTitleText;     // Title-Text
  unsigned int clrBack;          // Background
  unsigned int clrText;          // Text
};
extern const cRadioSkin radioSkin[eRadioSkinMaxNumber];

int theme_skin(void);


#endif //__RADIO_SKIN_H
