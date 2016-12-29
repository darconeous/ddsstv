//
//  ddsstv-mode.h
//  SSTV
//
//  Created by Robert Quattlebaum on 3/27/14.
//  Copyright (c) 2014 deepdarc. All rights reserved.
//

#ifndef SSTV_ddsstv_mode_h
#define SSTV_ddsstv_mode_h

#include <stdint.h>
#include <stdbool.h>

typedef enum {
	DDSSTV_CHANNEL_Y,
	DDSSTV_CHANNEL_Cb,
	DDSSTV_CHANNEL_Cr,

	DDSSTV_CHANNEL_RED,
	DDSSTV_CHANNEL_GREEN,
	DDSSTV_CHANNEL_BLUE,
} ddsstv_channel_t ;

typedef enum {
	kDDSSTV_COLOR_MODE_GRAYSCALE,
	kDDSSTV_COLOR_MODE_RGB,
	kDDSSTV_COLOR_MODE_YCBCR_420_709,
	kDDSSTV_COLOR_MODE_YCBCR_422_709,
	kDDSSTV_COLOR_MODE_YCBCR_420_601,
	kDDSSTV_COLOR_MODE_YCBCR_422_601,
} ddsstv_color_mode_t;

#define DDSSTV_COLOR_MODE_IS_YCBCR(x)	(((x)>=kDDSSTV_COLOR_MODE_YCBCR_420_709) && ((x)<= kDDSSTV_COLOR_MODE_YCBCR_422_601))

#define DDSSTV_COLOR_MODE_IS_NTSC(x)	(((x)>=kDDSSTV_COLOR_MODE_YCBCR_420_601) && ((x)<= kDDSSTV_COLOR_MODE_YCBCR_422_601))

#define DDSSTV_COLOR_MODE_IS_SRGB(x)	(((x)>=kDDSSTV_COLOR_MODE_RGB) && ((x)<= kDDSSTV_COLOR_MODE_YCBCR_422_709))

#define DDSSTV_COLOR_MODE_IS_YCBCR_420(x)	((x)==kDDSSTV_COLOR_MODE_YCBCR_420_709 || (x)== kDDSSTV_COLOR_MODE_YCBCR_420_601)

#define DDSSTV_COLOR_MODE_IS_YCBCR_422(x)	((x)==kDDSSTV_COLOR_MODE_YCBCR_422_709 || (x)== kDDSSTV_COLOR_MODE_YCBCR_422_601)

typedef int32_t ddsstv_vis_code_t;

enum {
	kSSTVVISCode_Unknown = -1,


	kSSTVVISProp_Color	= 0,
	kSSTVVISProp_BW_R	= 1,
	kSSTVVISProp_BW_G	= 2,
	kSSTVVISProp_BW_B	= 3,
	kSSTVVISProp_ColorMask	= 3,

	kSSTVVISProp_Horiz_160	= (0<<2),
	kSSTVVISProp_Horiz_320	= (1<<2),

	kSSTVVISProp_Vert_120	= (0<<3),
	kSSTVVISProp_Vert_240	= (1<<3),

	kSSTVVISProp_Type_Robot		= (0<<4),
	kSSTVVISProp_Type_WraaseSC1	= (1<<4),
	kSSTVVISProp_Type_Martin	= (2<<4),
	kSSTVVISProp_Type_Scotty	= (3<<4),
	kSSTVVISProp_Type_Mask		= (7<<4),



	kSSTVVISCodeScotty1 = 60,
	kSSTVVISCodeScotty2 = 56,
	kSSTVVISCodeScottyDX = 76,

	kSSTVVISCodeMartin1 = 44,
	kSSTVVISCodeMartin2 = 40,

	kSSTVVISCodeATV_24_Normal = 64,
	kSSTVVISCodeATV_90_Normal = 68,

	kSSTVVISCodeWRASSE_SC1_BW8 = 18,
	kSSTVVISCodeWRASSE_SC1_BW16 = 22,
	kSSTVVISCodeWRASSE_SC1_BW24 = 26,
	kSSTVVISCodeWRASSE_SC1_BW32 = 30,

	kSSTVVISCodeWRASSE_SC1_48Q = 24,

	kSSTVVISCodeWRASSE_SC2_180 = 55,

	kSSTVVISCodeRobot12c = 0,
	kSSTVVISCodeRobot24c = 4,
	kSSTVVISCodeRobot36c = 8,
	kSSTVVISCodeRobot72c = 12,

	kSSTVVISCodeClassic8 = 1,

	kSSTVVISCodeBW8 = 2,
	kSSTVVISCodeBW12 = 6,
	kSSTVVISCodeBW24 = 10,
	kSSTVVISCodeBW36 = 14,



	kSSTVVISCodeWWV = -2,
	kSSTVVISCodeWWVH = -3,


	kSSTVVISCodeWeatherFax120_IOC576 = -501,
};

struct ddsstv_mode_s {
	ddsstv_vis_code_t vis_code;
	ddsstv_color_mode_t color_mode;
	int16_t aspect_width;		//!^ The apparent width of the image, compared to the height.
	int16_t width;				//!^ Quantized width of the image, in pixels.
	int16_t height;				//!^ Expected scanline count.
	int32_t start_offset;		//!^ In microseconds.
	int32_t scanline_duration;	//!^ In microseconds.
	int16_t sync_duration;		//!^ In microseconds.
	int16_t front_porch_duration;		//!^ In microseconds.
	int16_t back_porch_duration;		//!^ In microseconds.
	int8_t channel_order[3];	//!^ Order of channels (RGB vs GBR, etc)
	int8_t rev_channel_order[3];	//!^ Reverse Order of channels (RGB vs GBR, etc)
	int8_t channel_length[3];	//!^ Relative. ex: {1,1,1},{2,1,1},{1,2,1}
	float ycc_chroma;			//!^ 1.0 is full-range, <1.0 is desaturated
	float ycc_contrast;			//!^ 1.0 is full-range
	float ycc_ycenter;			//!^ 1.0 is full-range
	bool scotty_hack;

	int16_t autosync_tol;
	int32_t autosync_offset;	//!^ In microseconds.
	bool autosync_track_center;

	int32_t horizontal_offset;	//!^ In microseconds.

	int16_t sync_freq;			//!^ In Hertz. Generally 1200
	int16_t zero_freq;			//!^ In Hertz. Generally 1500
	int16_t max_freq;			//!^ In Hertz. Generally 2300
};

extern const bool ddsstv_vis_code_is_supported(ddsstv_vis_code_t code);
extern void ddsstv_mode_lookup_vis_code(struct ddsstv_mode_s* mode, ddsstv_vis_code_t code);
extern bool ddsstv_mode_guess_vis_from_hsync(struct ddsstv_mode_s* mode, int32_t scanline_duration);
extern const char* ddsstv_describe_vis_code(ddsstv_vis_code_t code);

#endif
