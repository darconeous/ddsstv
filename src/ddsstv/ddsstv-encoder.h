//
//  ddsstv-encoder.h
//  DDSSTV
//
//  Created by Robert Quattlebaum on 10/20/14.
//  Copyright (c) 2014 deepdarc. All rights reserved.
//

#ifndef __DDSSTV__ddsstv_encoder__
#define __DDSSTV__ddsstv_encoder__

#include <stdint.h>
#include <stdbool.h>
#include <dddsp/dddsp.h>
#include "pt.h"
#include "ddsstv-mode.h"

struct ddsstv_encoder_s;
typedef struct ddsstv_encoder_s *ddsstv_encoder_t;


typedef size_t (*dddsp_encoder_pull_scanline_func)(
	void* context,
	int line,
	const struct ddsstv_mode_s* mode,
	ddsstv_channel_t channel,
	uint8_t* samples,
	size_t max_samples
);

struct ddsstv_encoder_s {
	void* context;

	dddsp_encoder_pull_scanline_func pull_scanline_cb;

	struct dddsp_modulator_s modulator;
	struct ddsstv_mode_s mode;

	float amplitude;
	int scanline;

	uint8_t* sample_buffer;
	size_t sample_buffer_size;
};

extern ddsstv_encoder_t ddsstv_encoder_init(ddsstv_encoder_t encoder);
extern void ddsstv_encoder_finalize(ddsstv_encoder_t encoder);

extern void ddsstv_encoder_reset(ddsstv_encoder_t encoder);
extern bool ddsstv_encoder_process(ddsstv_encoder_t encoder);

//extern void ddsstv_decoder_set_image_finished_callback(ddsstv_decoder_t decoder, void* context,void (*image_finished_cb)(void* context, ddsstv_decoder_t decoder));

#endif /* defined(__DDSSTV__ddsstv_encoder__) */
