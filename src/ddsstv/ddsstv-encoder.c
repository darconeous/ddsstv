//
//  ddsstv-encoder.c
//  DDSSTV
//
//  Created by Robert Quattlebaum on 10/20/14.
//  Copyright (c) 2014 deepdarc. All rights reserved.
//

#include "assert-macros.h"
#include "ddsstv-encoder.h"
#include <string.h>
#include <stdlib.h>

void
ddsstv_encoder_reset(ddsstv_encoder_t self)
{
	self->scanline = -1;
}


ddsstv_encoder_t
ddsstv_encoder_init(ddsstv_encoder_t self)
{
	memset(self, 0, sizeof(*self));
	dddsp_modulator_init(&self->modulator);
	ddsstv_mode_lookup_vis_code(&self->mode, kSSTVVISCodeRobot36c);

	ddsstv_encoder_reset(self);
	self->amplitude = 0.75;
	return self;
}

void
ddsstv_encoder_finalize(ddsstv_encoder_t self)
{
	dddsp_modulator_finalize(&self->modulator);

	free(self->sample_buffer);

	self->sample_buffer = NULL;
}

static void
_ddsstv_encoder_append_header_vis(ddsstv_encoder_t self)
{
	const float freq_1900 = 1900;
	const float freq_1200 = 1200;
	const float freq_one = 1100;
	const float freq_zero = 1300;

	uint8_t code = self->mode.vis_code;
	int i, parity=0;

	dddsp_modulator_append_const_freq(&self->modulator, 300*0.001, freq_1900, self->amplitude);
	dddsp_modulator_append_const_freq(&self->modulator, 300*0.001, freq_1200, self->amplitude);
	dddsp_modulator_append_const_freq(&self->modulator, 300*0.001, freq_1900, self->amplitude);
	dddsp_modulator_append_const_freq(&self->modulator, 300*0.001, freq_1200, self->amplitude);

	for(i = 7; i ; i--, code >>= 1) {
		dddsp_modulator_append_const_freq(&self->modulator, 30*0.001, (code & 1)?freq_one:freq_zero, self->amplitude);
		parity += (code & 1);
	}

	dddsp_modulator_append_const_freq(&self->modulator, 30*0.001f, (parity & 1)?freq_one:freq_zero, self->amplitude);
	dddsp_modulator_append_const_freq(&self->modulator, 30*0.001f, freq_1200, self->amplitude);
}

static void
_ddsstv_encoder_append_header(ddsstv_encoder_t self)
{
	_ddsstv_encoder_append_header_vis(self);
}

static float
_ddsstv_encoder_sample_to_freq(ddsstv_encoder_t self, uint8_t sample)
{
	return sample * (self->mode.zero_freq - self->mode.max_freq) / 255.0f + self->mode.zero_freq;
}

static void
_ddsstv_encoder_append_samples(ddsstv_encoder_t self, const uint8_t *samples, size_t count, float total_duration)
{
	const float pixel_duration = total_duration / count;

	for ( ; count > 0; samples++, count--) {
		dddsp_modulator_append_const_freq(
			&self->modulator,
			pixel_duration,
			_ddsstv_encoder_sample_to_freq(self, samples[0]),
			self->amplitude
		);
	}
}

static void
_ddsstv_encoder_append_scanline_bw(ddsstv_encoder_t self, int scanline)
{
	const float scanline_duration = (float)self->mode.scanline_duration/USEC_PER_SEC;
	const float sync_duration = (float)self->mode.sync_duration/USEC_PER_SEC;
	const float front_porch_duration = (float)self->mode.front_porch_duration/USEC_PER_SEC;
	const float back_porch_duration = (float)self->mode.back_porch_duration/USEC_PER_SEC;
	const float image_duration = scanline_duration - sync_duration - front_porch_duration - back_porch_duration;

	size_t scanline_width = (*self->pull_scanline_cb)(
		self->context,
		scanline,
		DDSSTV_CHANNEL_Y,
		self->sample_buffer,
		self->sample_buffer_size
	);

	if (scanline_width > self->sample_buffer_size) {
		self->sample_buffer = realloc(self->sample_buffer, scanline_width);
		self->sample_buffer_size = scanline_width;

		assert(self->sample_buffer);
	}

	dddsp_modulator_append_const_freq(
		&self->modulator,
		sync_duration,
		self->mode.sync_freq,
		self->amplitude
	);

	dddsp_modulator_append_const_freq(
		&self->modulator,
		front_porch_duration,
		self->mode.zero_freq,
		self->amplitude
	);

	_ddsstv_encoder_append_samples(
		self,
		self->sample_buffer,
		scanline_width,
		image_duration
	);

	dddsp_modulator_append_const_freq(
		&self->modulator,
		back_porch_duration,
		self->mode.zero_freq,
		self->amplitude
	);
}

bool
ddsstv_encoder_process(ddsstv_encoder_t self)
{
	if (self->scanline == -1) {
		dddsp_modulator_append_silence(&self->modulator, 1);
		_ddsstv_encoder_append_header(self);

		self->scanline++;
	} else if (self->scanline >= 0 && self->scanline < self->mode.height) {
		_ddsstv_encoder_append_scanline_bw(self, self->scanline);

		self->scanline++;
	} else if (self->scanline == self->mode.height) {
		dddsp_modulator_append_silence(&self->modulator, 1);

		self->scanline++;
	}

	return self->scanline < self->mode.height;
}
