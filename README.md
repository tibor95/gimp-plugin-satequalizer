# gimp-plugin-satequalizer
SATURATION EQUALIZER - gimp plugin
(current stable version 0.9.0, development version 0.9.2)


 The purpose is to let user to modify saturation based on current saturation intensity. Standard saturation (single slider/amount saturation) 
usually leads to oversaturation and/or undersaturation of some areas. To address this shortcoming the Saturation Equalizer provides 6-band equalizer,
where you can modify saturation based on current amount of saturation (of individual pixel).
 Additional features are:
 - Brightness modification via levels like tool
 - Temperature adjustment
 - Per-Corner brightness adjustment
 - Virtual color-based mask - lets you select color the will be affected by plugin. The slider controls how similar colors will be included into
  virtual mask. You can f.e. protect skin from saturating effect of the plugin
 - Auto color alighment - shifts RGB channels relatively, to make average of RGB values the same. Usefull for making snow white...
 - Five internal color modes / color spaces available: 2x own, HSV, HSL and YUL
 
 Internal details:
 - Plugin internally works in floats, so there are no data loss during processing.
 - It is multithreaded.


INSTALATION on Linux and perhaps other unix-like OS:

Plugin is distributed as C code, so for single user installation  use (recommended to keep your system partition clean):
  gimptool-2.0 --install saturate.c
 and for system-wide installation (as root):
  gimptool-2.0 --install-admin saturate.c
 (you might need to install gimptool-2.0 package - search your distribution repositories for it)

if compilation fails with: ....undefined reference to symbol 'pow@@GLIBC_2.x.x'...., run
  export LDFLAGS="$LDFLAGS -lm"
and repeat the compilation command

INSTALATION on windows - I can not provide binaries/packages for windows, check http://registry.gimp.org/node/25300, or just google for it.


USAGE:
 Plugin is to be found as FILTERS->ENHANCE->Saturation Equalizer.
 Functionality should be obvious, also you can read mouse-over tips...
 


BUGS:
So far I dont thing there is any big problem here.



CHANGELOG:
v. 0.9.2 - inline function changed to static inline to satisfy compiler
         - removed deprecated threading functions
v. 0.9.1 - button "Export to layer" added
		 - few under the hood changes
v. 0.9.0 - no significant changes, this is just "stable" version
v. 0.8.6 - masks eliminated, replaced by "Per-Corner Brightness" (still development version)
v. 0.8.5 - changes in "masks"
v. 0.8.4 - small changes to tips
         - "production" version
v. 0.8.3 - mainly GUI-related changes (tips, layout ...)
v. 0.8.2 - Color balance added 
v. 0.8.1 - zoomable preview
         - selective saturation (per color) made (from skin protection color)
v. 0.8.  - no significant change (this is to be "production" release)
v. 0.7.7 - skin tone protection now works with other colorspaces
         - statistics info reworked
         - mouse-over tips added (to some widgets)
v. 0.7.6.- Skin tone protection added
v. 0.7.4 - small changes to GUI and internal algorihm changes (not much visible to user)
v. 0.7.3 - color model reworked (4 new added)
		 - removed hue protect checkbutton (it would be too complex as there are 5 different colorization models. Also some of them (HSV/HSL) has in-built limits to saturation)
		 - reworked temperature modification
v. 0.7.2 - icons are now compiled into binary, so no external .png s are required anymore
v. 0.7.1 - reworked color temperature modification (also hidden in expandable box)
	     - buttons "color protect" and "apply temperature changes" are now exclusive (can be both on in the same time)
v. 0.7   - stable version, no functional changes to 0.6.9
v. 0.6.9 - small tweaks to masks
v. 0.6.8 - small changes related to masks (one mask added)
v. 0.6.7 - three types of filters added, plus related changes in algorithm
v. 0.6.6.- coupling of saturation sliders with setable maximal difference
		 - changes in GUI
v. 0.6.5 - multithreading added, 3 threads by default
		 - temperature modification changed to two spinboxes for finer tuning of temeprature
		 - checkbox - protect color (hue) added, it will reduce saturation if one of RGB components should get out of range
v. 0.6.3 - bugfix to 0.6.2 (typo) (I'm sorry for two bugs in line)
v. 0.6.2 - bugfix to 0.6.1 (problem with alpha channel on images with 3 channels)
v. 0.6.1 - support for alpha channel layer added
v. 0.6 -0.5
- reset button added (adding was easy, make it work was harder :) )
- content of brightness formula is hide-able and hiden from start (in order to reduce height of plugin window)
v. 0.5 - 0.4
- GUI reworked (shrinked)
v. 0.4 -0.3:
- added combo to pick brightness algorithms
- enhancing internals calculation of saturation
v. 0.3-0.2:
- sliders in brightness section replaced for spinbuttons
- explanation labels added to brightness and temperature sections
v. 0.1-0.2:
- One band added (first one - for lowest saturation)
- fixed orientation of equalizer bands



CONTACT:
Any feedback welcomed: tiborb95 at gmail dot com

LICENSE:
GPL v.3


May 06, 2016
