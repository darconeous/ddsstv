//
//  ddsstv-decoder.h
//  SSTV
//
//  Created by Robert Quattlebaum on 3/27/14.
//  Copyright (c) 2014 deepdarc. All rights reserved.
//

#ifndef SSTV_ddsstv_decoder_h
#define SSTV_ddsstv_decoder_h

#include <stdint.h>
#include <stdbool.h>
#include <dddsp/dddsp.h>
#include "pt.h"
#include "ddsstv-mode.h"

#define DDSSTV_INTERNAL_SAMPLE_RATE     6800
#define DDSSTV_MAX_INGEST_SAMPLE_RATE   65530
#define DDSSTV_MIN_INGEST_SAMPLE_RATE   DDSSTV_INTERNAL_SAMPLE_RATE
#define DDSSTV_MAX_FREQ_BUFFER_SIZE     DDSSTV_INTERNAL_SAMPLE_RATE*60*5

#define LPM_TO_SCANLINE_DURATION(x)     (60.0*USEC_PER_SEC/(x))

typedef enum {
	kDDSSTV_STATUS_OK = 0,
	kDDSSTV_STATUS_ERROR = 1,
} ddsstv_status_t;

typedef enum {
	kDDSSTV_STARTED_BY_USER = 0,
	kDDSSTV_STARTED_BY_VIS = 1,
	kDDSSTV_STARTED_BY_VSYNC = 2,
	kDDSSTV_STARTED_BY_HSYNC = 3,
} ddsstv_started_by_t;

struct ddsstv_decoder_s;
typedef struct ddsstv_decoder_s *ddsstv_decoder_t;

struct ddsstv_decoder_s {
	struct ddsstv_mode_s mode;

	int16_t sync_freq;			//!^ In Hertz. Generally 1200
	int16_t max_freq;			//!^ In Hertz. Generally 2300

	int16_t *freq_buffer;		//!^ Samples are in hertz.
	size_t freq_buffer_size;

	// -- Header Decoding State --
	struct pt header_pt;
	int32_t header_best_score;
	int32_t header_location;	//!^ In samples.

	dddsp_correlator_t vis_correlator;
	dddsp_correlator_t vsync_correlator;
	dddsp_correlator_t hsync_correlator;

	int32_t hsync_len[5];
	int hsync_count;
	int32_t hsync_last;

	bool vis_parity_error;

	// Header Decoding Settings
	bool autostart_on_vis;
	bool autostart_on_vsync;
	bool autostart_on_hsync;

	// -- Image Decoding State --
	struct pt image_pt;

	ddsstv_started_by_t started_by;
	uint8_t *image_buffer;
	size_t image_buffer_size;

	bool is_decoding;
	bool has_image;
	bool last_image_was_complete;
	bool auto_started_decoding;

	int32_t header_offset;	//!^ In microseconds
	int16_t current_scanline;

	uint32_t scanline_duration_filter[3];
	uint32_t scanline_duration_count;
	dddsp_iir_float_t scanline_duration_filter2;

	int32_t current_scanline_start;		//!^ In microseconds.
	int32_t current_scanline_postsync;		//!^ In microseconds.
	int32_t current_scanline_stop;		//!^ In microseconds.
	int scanlines_since_last_hsync;
	int scanlines_in_sync;
	int scanline_mix_factor;

	int32_t last_good_scanline_stop;		//!^ In microseconds.
	int16_t last_good_scanline;

	// Image Decoding Settings
	bool continuous;
	bool asynchronous;
	bool clear_on_restart;
	bool preserve_luma_when_clipped;

	void (*image_finished_cb)(void* context, ddsstv_decoder_t decoder);
	void* context;
};

extern void ddsstv_decoder_init(ddsstv_decoder_t decoder, double ingest_sample_rate);
extern void ddsstv_decoder_set_image_finished_callback(ddsstv_decoder_t decoder, void* context,void (*image_finished_cb)(void* context, ddsstv_decoder_t decoder));

extern void ddsstv_decoder_reset(ddsstv_decoder_t decoder);
extern void ddsstv_decoder_start(ddsstv_decoder_t decoder);
extern void ddsstv_decoder_stop(ddsstv_decoder_t decoder);
extern void ddsstv_decoder_recalc_image(ddsstv_decoder_t decoder);
extern void ddsstv_decoder_set_mode(ddsstv_decoder_t decoder, const struct ddsstv_mode_s* mode);
extern bool ddsstv_decoder_ingest_freqs(ddsstv_decoder_t decoder, const int16_t* freqs, size_t count);

#ifdef __APPLE__
#include <CoreGraphics/CoreGraphics.h>
extern CGImageRef ddsstv_decoder_copy_image(ddsstv_decoder_t decoder);
#endif // defined(__APPLE__)


#endif
