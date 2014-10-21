//
//  ddsstv-mode.c
//  SSTV
//
//  Created by Robert Quattlebaum on 3/27/14.
//  Copyright (c) 2014 deepdarc. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dddsp/dddsp.h>
#include "ddsstv.h"

const bool
ddsstv_vis_code_is_supported(ddsstv_vis_code_t code)
{
	switch(code) {
	case kSSTVVISCodeScotty1:
	case kSSTVVISCodeScotty2:
	case kSSTVVISCodeScottyDX:
	case kSSTVVISCodeMartin1:
	case kSSTVVISCodeMartin2:
	case kSSTVVISCodeATV_24_Normal:
	case kSSTVVISCodeATV_90_Normal:
	case kSSTVVISCodeWRASSE_SC1_BW8:
	case kSSTVVISCodeWRASSE_SC1_BW16:
	case kSSTVVISCodeWRASSE_SC1_BW24:
//	case kSSTVVISCodeWRASSE_SC1_BW32:
	case kSSTVVISCodeWRASSE_SC1_48Q:
	case kSSTVVISCodeWRASSE_SC2_180:
	case kSSTVVISCodeRobot12c:
	case kSSTVVISCodeRobot24c:
	case kSSTVVISCodeRobot36c:
	case kSSTVVISCodeRobot72c:
	case kSSTVVISCodeBW8:
	case kSSTVVISCodeBW12:
	case kSSTVVISCodeBW24:
	case kSSTVVISCodeBW36:
	case kSSTVVISCodeClassic8:
	case kSSTVVISCodeWeatherFax120_IOC576:
		return true;
		break;
	}

	return false;
}

const char*
ddsstv_describe_vis_code(ddsstv_vis_code_t code)
{
	const char* ret = "Unknown";

	switch(code) {
	case kSSTVVISCodeWeatherFax120_IOC576:
		ret = "WeatherFAX-120-IOC576";
		break;
	case kSSTVVISCodeScotty1:
		ret = "Scotty-1-RGB";
		break;
	case kSSTVVISCodeScotty2:
		ret = "Scotty-2-RGB";
		break;
	case kSSTVVISCodeScottyDX:
		ret = "Scotty-DX-RGB";
		break;
	case kSSTVVISCodeMartin1:
		ret = "Martin-1-RGB";
		break;
	case kSSTVVISCodeMartin2:
		ret = "Martin-2-RGB";
		break;
	case kSSTVVISCodeATV_24_Normal:
		ret = "ATV-24N-RGB";
		break;
	case kSSTVVISCodeATV_90_Normal:
		ret = "ATV-90N-RGB";
		break;
	case kSSTVVISCodeWRASSE_SC1_BW8:
		ret = "SC1-8-BW";
		break;
	case kSSTVVISCodeWRASSE_SC1_BW16:
		ret = "SC1-16-BW";
		break;
	case kSSTVVISCodeWRASSE_SC1_BW24:
		ret = "SC1-24-BW";
		break;
	case kSSTVVISCodeWRASSE_SC1_BW32:
		ret = "SC1-32-BW";
		break;
	case kSSTVVISCodeWRASSE_SC1_48Q:
		ret = "SC1-48Q-RGB";
		break;
	case kSSTVVISCodeWRASSE_SC2_180:
		ret = "SC2-180-RGB";
		break;
	case kSSTVVISCodeRobot12c:
		ret = "Robot-12-YCC";
		break;
	case kSSTVVISCodeRobot24c:
		ret = "Robot-24-YCC";
		break;
	case kSSTVVISCodeRobot36c:
		ret = "Robot-36-YCC";
		break;
	case kSSTVVISCodeRobot72c:
		ret = "Robot-72-YCC";
		break;
	case kSSTVVISCodeBW8:
		ret = "Robot-8-BW";
		break;
	case kSSTVVISCodeBW12:
		ret = "Robot-12-BW";
		break;
	case kSSTVVISCodeBW24:
		ret = "Robot-24-BW";
		break;
	case kSSTVVISCodeBW36:
		ret = "Robot-36-BW";
		break;
	case kSSTVVISCodeClassic8:
		ret = "Classic-8-BW";
		break;
	case kSSTVVISCodeWWV:
		ret = "NIST WWV";
		break;
	case kSSTVVISCodeWWVH:
		ret = "NIST WWVH";
		break;
	default:
		{
			static char temp[63];
			char* type = "unk";
			char* color = "unk";
			if(0 == (code & 0xFFFFFF80)) {
				switch(code&kSSTVVISProp_Type_Mask) {
				case kSSTVVISProp_Type_Robot: type = "robot"; break;
				case kSSTVVISProp_Type_WraaseSC1: type = "sc1"; break;
				case kSSTVVISProp_Type_Martin: type = "martin"; break;
				case kSSTVVISProp_Type_Scotty: type = "scotty"; break;
				default: type = "unk";
				}
				switch(code&kSSTVVISProp_ColorMask) {
				case kSSTVVISProp_Color: color = "color"; break;
				case kSSTVVISProp_BW_R: color = "red"; break;
				case kSSTVVISProp_BW_G: color = "green"; break;
				case kSSTVVISProp_BW_B: color = "blue"; break;
				default: break;
				}
				snprintf(temp,sizeof(temp),"VIS %d (0x%02X %s-%s-%c%c)",code,code,
					type, color, (code&kSSTVVISProp_Horiz_320)?'H':'h', (code&kSSTVVISProp_Vert_240)?'V':'v'
				);
			} else if (code<0) {
				snprintf(temp,sizeof(temp),"UNK %d (0x%02X %s-%s-%c%c)",code,code,
					type, color, (code&kSSTVVISProp_Horiz_320)?'H':'h', (code&kSSTVVISProp_Vert_240)?'V':'v'
				);
			} else {
				snprintf(temp,sizeof(temp),"XVIS %d (0x%04X)",code,code);
			}

			ret = temp;
		}
	}
	return ret;
}

void
ddsstv_mode_lookup_vis_code(struct ddsstv_mode_s* mode, ddsstv_vis_code_t code)
{
	mode->vis_code = code;
	if(code == kSSTVVISCode_Unknown)
		return;
	memset(mode,0,sizeof(*mode));
	mode->vis_code = code;
	mode->width = (code&kSSTVVISProp_Horiz_320)?320:160;
	mode->height = (code&kSSTVVISProp_Vert_240)?240:120;
	mode->aspect_width = (code&kSSTVVISProp_Vert_240)?320:160;
	mode->autosync_tol = 100;
	mode->autosync_track_center = false;
	mode->channel_order[0] = 0;
	mode->channel_order[1] = 1;
	mode->channel_order[2] = 2;
	mode->channel_length[0] = 1;
	mode->channel_length[1] = 1;
	mode->channel_length[2] = 1;
#if 0
	mode->ycc_chroma = (240.0-16.0)/255.0;
	mode->ycc_contrast = (235.0-16.0)/255.0;
	mode->ycc_ycenter = ((235.0-16.0)/2.0+16.0)/255.0;
#else
	mode->ycc_ycenter = 0.5;
	mode->ycc_contrast = 1.0;
	mode->ycc_chroma = 1.0;
#endif
	mode->sync_freq = 1200;
	mode->zero_freq = 1500;
	mode->max_freq = 2300;

	if(code == kSSTVVISCodeWeatherFax120_IOC576) {
		mode->width = 1200;
		mode->height = 1200;
		mode->aspect_width = 1200;
		mode->autosync_tol = 100;
		mode->autosync_track_center = false;
		mode->channel_order[0] = 0;
		mode->channel_order[1] = 1;
		mode->channel_order[2] = 2;
		mode->channel_length[0] = 1;
		mode->channel_length[1] = 1;
		mode->channel_length[2] = 1;
		mode->zero_freq = 1500;
		mode->max_freq = 2300;
		mode->sync_freq = 1200;
		mode->scanline_duration = LPM_TO_SCANLINE_DURATION(120.0);

		mode->front_porch_duration = 0;
		mode->back_porch_duration = 0;
		mode->color_mode = kDDSSTV_COLOR_MODE_GRAYSCALE;
		// Don't let autosync take over...!
		mode->sync_duration = 0*USEC_PER_MSEC;
	} else
	if((code == kSSTVVISCodeWWV) || (code == kSSTVVISCodeWWVH)) {
		mode->sync_duration = 5*USEC_PER_MSEC; // overridden below
		mode->sync_freq = (code == kSSTVVISCodeWWV)?1000:1200;
		mode->zero_freq = 400;
		mode->max_freq = 1600;
		mode->scanline_duration = USEC_PER_SEC;
		mode->front_porch_duration = 25*USEC_PER_MSEC;
		mode->back_porch_duration = 5*USEC_PER_MSEC;
		mode->color_mode = kDDSSTV_COLOR_MODE_GRAYSCALE;
		mode->aspect_width = mode->width = 320;
		mode->height = 240; // four minutes

		// Don't let autosync take over...!
		mode->sync_duration = 0*USEC_PER_MSEC;

	} else if((code&kSSTVVISProp_ColorMask) == kSSTVVISProp_Color) {
		mode->front_porch_duration = 3*USEC_PER_MSEC;
		mode->color_mode = kDDSSTV_COLOR_MODE_RGB;

		switch(code&kSSTVVISProp_Type_Mask) {
		case kSSTVVISProp_Type_Scotty:
		case kSSTVVISProp_Type_Scotty+kSSTVVISProp_Type_WraaseSC1:
			mode->channel_order[0] = 0;
			mode->channel_order[1] = 1;
			mode->channel_order[2] = 2;
			mode->sync_duration = 9*USEC_PER_MSEC;
			mode->scanline_duration = 277.6920*USEC_PER_MSEC;
			mode->front_porch_duration = 1.5*USEC_PER_MSEC;
			mode->scotty_hack = true;
			if((code&kSSTVVISProp_Horiz_320)) {
				if((code&kSSTVVISProp_Type_Mask) == kSSTVVISProp_Type_Scotty+kSSTVVISProp_Type_WraaseSC1) {
					mode->scanline_duration = 1050.3*USEC_PER_MSEC;
					//mode->width *= 4;
				} else {
					mode->scanline_duration = 428.223*USEC_PER_MSEC;
					//mode->width *= 2;
				}
			}
			mode->start_offset += (mode->scanline_duration-mode->sync_duration)*2/3+mode->sync_duration;
			mode->aspect_width = mode->width = 320;
			mode->height = 256;
			//mode->autosync_offset = USEC_PER_MSEC*1.52;
			break;
		case kSSTVVISProp_Type_Martin:
			mode->channel_order[0] = 2;
			mode->channel_order[1] = 0;
			mode->channel_order[2] = 1;
			mode->scanline_duration = 226.7980*USEC_PER_MSEC;
			mode->front_porch_duration = 0.572/2.0*USEC_PER_MSEC;
			mode->back_porch_duration = 0.572/2.0*USEC_PER_MSEC;
			mode->sync_duration = 4.862*USEC_PER_MSEC;
			mode->autosync_track_center = true;

			// Compensate for the way porches are handled.
			mode->sync_duration += mode->front_porch_duration + mode->back_porch_duration;

			if((code&kSSTVVISProp_Horiz_320)) {
				mode->scanline_duration = 446.446*USEC_PER_MSEC;
			}
			mode->aspect_width = mode->width = 320;
			mode->height = 256;
			//mode->autosync_offset = USEC_PER_MSEC*2.446;

			break;
		case kSSTVVISProp_Type_Robot:
			mode->color_mode = (code&kSSTVVISProp_Horiz_320)?kDDSSTV_COLOR_MODE_YCBCR_422_601:kDDSSTV_COLOR_MODE_YCBCR_420_601;
			mode->channel_order[0] = 0;
			mode->channel_order[1] = 2;
			mode->channel_order[2] = 1;

			mode->front_porch_duration = 3*USEC_PER_MSEC;
			if(!(code&kSSTVVISProp_Vert_240)) {
				mode->sync_duration = 5*USEC_PER_MSEC;
			} else {
				mode->sync_duration = 9*USEC_PER_MSEC;
			}
			mode->autosync_offset = USEC_PER_MSEC*0.0;
			mode->front_porch_duration = 3*USEC_PER_MSEC;
			mode->back_porch_duration = 0*USEC_PER_MSEC;

		default:

			mode->width = (code&kSSTVVISProp_Vert_240)?320:160;
			mode->channel_length[0] = 2;
			mode->scanline_duration = 100*USEC_PER_MSEC;

			if((code&kSSTVVISProp_Horiz_320)) {
				mode->scanline_duration = mode->scanline_duration*2;
			}

			if((code&kSSTVVISProp_Vert_240))
				mode->scanline_duration = mode->scanline_duration*3/2;
			break;
		}
	} else {
		// Black-and-white
		mode->color_mode = kDDSSTV_COLOR_MODE_GRAYSCALE;
		mode->channel_length[0] = 1;
		mode->sync_duration = 5*USEC_PER_MSEC;
		mode->scanline_duration = 66.666667*USEC_PER_MSEC;
		mode->front_porch_duration = 1*USEC_PER_MSEC;

		switch(code&kSSTVVISProp_Type_Mask) {
		case kSSTVVISProp_Type_Robot:
			mode->sync_duration = 9*USEC_PER_MSEC;
			if(!(code&kSSTVVISProp_Vert_240)) {
				mode->sync_duration = 5*USEC_PER_MSEC;
			}
			mode->front_porch_duration = mode->sync_duration/3;
			//mode->autosync_offset = USEC_PER_MSEC*1.4;
			if (code==kSSTVVISCodeBW8) {
				//mode->scanline_duration = 66.897*USEC_PER_MSEC;
			}
			if (code==kSSTVVISCodeBW12) {
				//mode->start_offset = -36.4*USEC_PER_MSEC;
			}
		default:
			if((code&kSSTVVISProp_Horiz_320))
				mode->scanline_duration = mode->scanline_duration*3/2;

			if((code&kSSTVVISProp_Vert_240))
				mode->scanline_duration = mode->scanline_duration*3/2;
			break;
		}
		if (code==kSSTVVISCodeClassic8) {
			mode->scanline_duration = LPM_TO_SCANLINE_DURATION(900.0);
			mode->sync_duration = 5*USEC_PER_MSEC;
			mode->height = 128;
			mode->aspect_width = 128;
			mode->width = 128;
			mode->autosync_tol = 5;
			mode->autosync_offset = 0;
			//mode->autosync_track_center = false;
		}
		if (code==kSSTVVISCodeWRASSE_SC1_BW8) {
			mode->scanline_duration = LPM_TO_SCANLINE_DURATION(1000.0);
			mode->sync_duration = 5*USEC_PER_MSEC;
			mode->height = 120;
			mode->aspect_width = 120;
			mode->width = 120;
		}
	}

//	if(code==55) {
//		mode->scanline_duration = 710*USEC_PER_MSEC;
//		mode->sync_duration = (5.5225+0.5)*USEC_PER_MSEC;
//		mode->front_porch_duration = 0;
//		mode->back_porch_duration = 0;
//		mode->color_mode = kDDSSTV_COLOR_MODE_RGB;
//		mode->channel_order[0] = 0;
//		mode->channel_order[1] = 1;
//		mode->channel_order[2] = 2;
//		mode->width = 320;
//		mode->height = 256;
//		mode->aspect_width = 320;
//		mode->scotty_hack = false;
//	}
	if(code == kSSTVVISCodeWRASSE_SC1_BW32) {
		mode->scanline_duration = LPM_TO_SCANLINE_DURATION(500.0);
		mode->color_mode = kDDSSTV_COLOR_MODE_GRAYSCALE;
		mode->front_porch_duration = 0;
		mode->back_porch_duration = 0;
		mode->width = 240;
		mode->height = 256;
		mode->aspect_width = 240;
		mode->scotty_hack = false;
		mode->start_offset = 0;
	}
	if(code == kSSTVVISCodeWRASSE_SC1_48Q) {
		mode->scanline_duration = LPM_TO_SCANLINE_DURATION(900.0);
		mode->color_mode = kDDSSTV_COLOR_MODE_RGB;
		mode->front_porch_duration = 0;
		mode->back_porch_duration = 0;
		mode->width = 240;
		mode->height = 256;
		mode->aspect_width = 240;
		mode->scotty_hack = false;
		mode->start_offset = 0;
	}
	if(code == kSSTVVISCodeATV_24_Normal) {
		mode->scanline_duration = LPM_TO_SCANLINE_DURATION(960.0)*3;
		mode->sync_duration = 0;
		mode->front_porch_duration = 0;
		mode->back_porch_duration = 0;
		mode->color_mode = kDDSSTV_COLOR_MODE_RGB;
		mode->channel_order[0] = 0;
		mode->channel_order[1] = 1;
		mode->channel_order[2] = 2;
		mode->width = 128;
		mode->height = 128;
		mode->aspect_width = 128;
		mode->scotty_hack = false;
		mode->start_offset = 6222.5*USEC_PER_MSEC;
	}
	if(code == kSSTVVISCodeATV_90_Normal) {
		mode->scanline_duration = LPM_TO_SCANLINE_DURATION(480.0)*3;
		mode->sync_duration = 0;
		mode->front_porch_duration = 0;
		mode->back_porch_duration = 0;
		mode->color_mode = kDDSSTV_COLOR_MODE_RGB;
		mode->channel_order[0] = 0;
		mode->channel_order[1] = 1;
		mode->channel_order[2] = 2;
		mode->width = 256;
		mode->height = 240;
		mode->aspect_width = 256;
		mode->scotty_hack = false;
		mode->start_offset = 6222.5*USEC_PER_MSEC;
	}
	if(code == kSSTVVISCodeWRASSE_SC2_180) {
		mode->scanline_duration = LPM_TO_SCANLINE_DURATION(84.383);
		mode->sync_duration = (5.5225+0.5)*USEC_PER_MSEC;
		mode->front_porch_duration = 0;
		mode->back_porch_duration = 0;
		mode->color_mode = kDDSSTV_COLOR_MODE_RGB;
		mode->channel_order[0] = 0;
		mode->channel_order[1] = 1;
		mode->channel_order[2] = 2;
		mode->width = 320;
		mode->height = 256;
		mode->aspect_width = 320;
		mode->scotty_hack = false;
		mode->start_offset = 0;
	}
//	mode->autosync_tol = 40;

//	if(1) {
//		int32_t pixel_width = (mode->scanline_duration-mode->sync_duration-mode->front_porch_duration-mode->back_porch_duration)/mode->width;
//		mode->front_porch_duration += pixel_width/3;
//		mode->back_porch_duration -= pixel_width/3;
//		//mode->start_offset += pixel_width/3;
//	}


	//mode->width = mode->width*3/2;
	//mode->width = mode->width*2;
}

bool
ddsstv_mode_guess_vis_from_hsync(struct ddsstv_mode_s* mode, int32_t scanline_duration)
{
	ddsstv_vis_code_t code = -1;
	struct ddsstv_mode_s temp;
	uint32_t best_score = 0xFFFFFFFF;
	int32_t longest_duration = 0;
	int32_t shortest_duration = 0xFFFFFFFF;
	if(scanline_duration)
	for (ddsstv_vis_code_t i = 0; i<127 ; i++) {
		if(!ddsstv_vis_code_is_supported(i))
			continue;
		ddsstv_mode_lookup_vis_code(&temp, i);
		if(temp.scanline_duration>longest_duration)
			longest_duration = temp.scanline_duration;
		if(temp.scanline_duration<shortest_duration)
			shortest_duration = temp.scanline_duration;

		uint32_t score = abs(scanline_duration-temp.scanline_duration);

		// Mode blacklist
		switch(i) {
		case kSSTVVISCodeWRASSE_SC1_BW8:
		case kSSTVVISCodeRobot12c:
		case kSSTVVISCodeBW24:
		case kSSTVVISCodeBW36:
			score = 0xFFFFFFFF;
			break;
		}

		if(score<best_score) {
			code = i;
			best_score = score;
		}
	}
	//printf("best_score: %u, code: %d, scanline_duration:%d\n",best_score,code,scanline_duration);
	if(best_score<400) {
		ddsstv_mode_lookup_vis_code(mode, code);
	} else if ((mode->vis_code == kSSTVVISCode_Unknown)
		&& (scanline_duration>longest_duration)
		&& (scanline_duration<shortest_duration)
	) {
		mode->scanline_duration = scanline_duration;
		code = kSSTVVISCode_Unknown;
	} else {
		code = kSSTVVISCode_Unknown;
	}

	return code != kSSTVVISCode_Unknown;
}
