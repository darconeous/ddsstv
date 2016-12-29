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

#ifndef DDSSTV_USE_MMSSTV_PREFIX_TONES
#define DDSSTV_USE_MMSSTV_PREFIX_TONES 0
#endif

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
	ddsstv_mode_lookup_vis_code(&self->mode, kSSTVVISCodeMartin1);

	ddsstv_encoder_reset(self);
	self->modulator.multiplier = 8000;
	self->amplitude = 0.75;
#if DDSSTV_USE_MMSSTV_PREFIX_TONES
	self->use_mmsstv_prefix_tones = true;
#else
	self->use_mmsstv_prefix_tones = false;
#endif // DDSSTV_USE_MMSSTV_PREFIX_TONES
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

	if (self->use_mmsstv_prefix_tones) {
		// Extra MMSSTV tones
		const float freq_2300 = 2300;
		const float freq_1500 = 1500;
		dddsp_modulator_append_const_freq(&self->modulator, 100*0.001, freq_1900, self->amplitude);
		dddsp_modulator_append_const_freq(&self->modulator, 100*0.001, freq_1500, self->amplitude);
		dddsp_modulator_append_const_freq(&self->modulator, 100*0.001, freq_1900, self->amplitude);
		dddsp_modulator_append_const_freq(&self->modulator, 100*0.001, freq_1500, self->amplitude);
		dddsp_modulator_append_const_freq(&self->modulator, 100*0.001, freq_2300, self->amplitude);
		dddsp_modulator_append_const_freq(&self->modulator, 100*0.001, freq_1500, self->amplitude);
		dddsp_modulator_append_const_freq(&self->modulator, 100*0.001, freq_2300, self->amplitude);
		dddsp_modulator_append_const_freq(&self->modulator, 100*0.001, freq_1500, self->amplitude);
	}

	if ((self->mode.vis_code > 0) && (self->mode.vis_code < 127)) {
		dddsp_modulator_append_const_freq(&self->modulator, 300*0.001, freq_1900, self->amplitude);
		dddsp_modulator_append_const_freq(&self->modulator, 10*0.001, freq_1200, self->amplitude);
		dddsp_modulator_append_const_freq(&self->modulator, 300*0.001, freq_1900, self->amplitude);

		dddsp_modulator_append_const_freq(&self->modulator, 30*0.001, freq_1200, self->amplitude);

		for (i = 7; i ; i--, code >>= 1) {
			dddsp_modulator_append_const_freq(&self->modulator, 30*0.001, (code & 1)?freq_one:freq_zero, self->amplitude);
			parity += (code & 1);
		}

		dddsp_modulator_append_const_freq(&self->modulator, 30*0.001f, (parity & 1)?freq_one:freq_zero, self->amplitude);
		dddsp_modulator_append_const_freq(&self->modulator, 30*0.001f, freq_1200, self->amplitude);

	} else {
		// No VIS code. Just output a start pulse.
		dddsp_modulator_append_const_freq(&self->modulator, 0.25f, freq_1200, self->amplitude);
	}
}

static void
_ddsstv_encoder_append_header(ddsstv_encoder_t self)
{
    if ((self->mode.vis_code >= 0) && (self->mode.vis_code <= 127)) {
        _ddsstv_encoder_append_header_vis(self);
    } else {
        // No header supported.
    }
}

static float
_ddsstv_encoder_sample_to_freq(ddsstv_encoder_t self, uint8_t sample)
{
	return sample * (self->mode.max_freq - self->mode.zero_freq) / 255.0f + self->mode.zero_freq;
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
		&self->mode,
		DDSSTV_CHANNEL_Y,
		self->sample_buffer,
		self->sample_buffer_size
	);

	if (scanline_width > self->sample_buffer_size) {
		self->sample_buffer = realloc(self->sample_buffer, scanline_width);
		self->sample_buffer_size = scanline_width;

		assert(self->sample_buffer);

		scanline_width = (*self->pull_scanline_cb)(
			self->context,
			scanline,
			&self->mode,
			DDSSTV_CHANNEL_Y,
			self->sample_buffer,
			self->sample_buffer_size
		);
	}

	if (scanline_width == 0) {
		scanline_width = 1;
		self->sample_buffer[0] = 127;
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

static void
_ddsstv_encoder_append_scanline_rgb(ddsstv_encoder_t self, int scanline)
{
	const int channels = 3;
	const ddsstv_channel_t channel_lookup[] = {
		DDSSTV_CHANNEL_RED,
		DDSSTV_CHANNEL_GREEN,
		DDSSTV_CHANNEL_BLUE,
	};
	const float scanline_duration = (float)self->mode.scanline_duration/USEC_PER_SEC;
	const float sync_duration = (float)self->mode.sync_duration/USEC_PER_SEC;
	const float front_porch_duration = (float)self->mode.front_porch_duration/USEC_PER_SEC;
	const float back_porch_duration = (float)self->mode.back_porch_duration/USEC_PER_SEC;
	const float total_image_duration = scanline_duration - sync_duration - (front_porch_duration + back_porch_duration) * channels;
	const float image_duration = total_image_duration / channels;

	if (!self->mode.scotty_hack) {
		dddsp_modulator_append_const_freq(
			&self->modulator,
			sync_duration,
			self->mode.sync_freq,
			self->amplitude
		);
	}

	for (int i = 0; i < channels; i++) {
		ddsstv_channel_t channel = channel_lookup[self->mode.rev_channel_order[i]];
		if (self->mode.scotty_hack) {
			channel = channel_lookup[self->mode.rev_channel_order[(i+1)%3]];
		}
		size_t scanline_width = (*self->pull_scanline_cb)(
			self->context,
			scanline,
			&self->mode,
			channel,
			self->sample_buffer,
			self->sample_buffer_size
		);

		if (scanline_width > self->sample_buffer_size) {
			self->sample_buffer = realloc(self->sample_buffer, scanline_width);
			self->sample_buffer_size = scanline_width;

			scanline_width = (*self->pull_scanline_cb)(
				self->context,
				scanline,
				&self->mode,
				channel,
				self->sample_buffer,
				self->sample_buffer_size
			);

			assert(self->sample_buffer);
		}

		if (scanline_width == 0) {
			scanline_width = 1;
			self->sample_buffer[0] = 127;
		}

		if (self->mode.scotty_hack && i==2) {
			dddsp_modulator_append_const_freq(
				&self->modulator,
				sync_duration,
				self->mode.sync_freq,
				self->amplitude
			);
		}

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
}

static void
_ddsstv_encoder_append_scanline_ycbcr_422(ddsstv_encoder_t self, int scanline)
{
	const float scanline_duration = (float)self->mode.scanline_duration/USEC_PER_SEC;
	const float sync_duration = (float)self->mode.sync_duration/USEC_PER_SEC;
	const float front_porch_duration = (float)self->mode.front_porch_duration/USEC_PER_SEC;
	const float back_porch_duration = (float)self->mode.back_porch_duration/USEC_PER_SEC;
	const float total_image_duration = scanline_duration - (sync_duration + front_porch_duration + back_porch_duration) * 2;
	const float image_duration = total_image_duration / 2;


	size_t scanline_width = (*self->pull_scanline_cb)(
		self->context,
		scanline,
		&self->mode,
		DDSSTV_CHANNEL_Y,
		self->sample_buffer,
		self->sample_buffer_size
	);

	if (scanline_width > self->sample_buffer_size) {
		self->sample_buffer = realloc(self->sample_buffer, scanline_width);
		self->sample_buffer_size = scanline_width;

		scanline_width = (*self->pull_scanline_cb)(
			self->context,
			scanline,
			&self->mode,
			DDSSTV_CHANNEL_Y,
			self->sample_buffer,
			self->sample_buffer_size
		);
		assert(self->sample_buffer);
	}

	if (scanline_width == 0) {
		scanline_width = 1;
		self->sample_buffer[0] = 127;
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



	scanline_width = (*self->pull_scanline_cb)(
		self->context,
		scanline,
		&self->mode,
		DDSSTV_CHANNEL_Cr,
		self->sample_buffer,
		self->sample_buffer_size
	);
	if (scanline_width == 0) {
		scanline_width = 1;
		self->sample_buffer[0] = 127;
	}
	dddsp_modulator_append_const_freq(
		&self->modulator,
		sync_duration*0.5,
		self->mode.zero_freq,
		self->amplitude
	);
	dddsp_modulator_append_const_freq(
		&self->modulator,
		front_porch_duration*0.5,
		(self->mode.zero_freq+self->mode.max_freq)/2,
		self->amplitude
	);
	_ddsstv_encoder_append_samples(
		self,
		self->sample_buffer,
		scanline_width,
		image_duration*0.5
	);
	dddsp_modulator_append_const_freq(
		&self->modulator,
		back_porch_duration*0.5,
		(self->mode.zero_freq+self->mode.max_freq)/2,
		self->amplitude
	);





	scanline_width = (*self->pull_scanline_cb)(
		self->context,
		scanline,
		&self->mode,
		DDSSTV_CHANNEL_Cb,
		self->sample_buffer,
		self->sample_buffer_size
	);
	if (scanline_width == 0) {
		scanline_width = 1;
		self->sample_buffer[0] = 127;
	}
	dddsp_modulator_append_const_freq(
		&self->modulator,
		sync_duration*0.5,
		self->mode.max_freq,
		self->amplitude
	);
	dddsp_modulator_append_const_freq(
		&self->modulator,
		front_porch_duration*0.5,
		(self->mode.zero_freq+self->mode.max_freq)/2,
		self->amplitude
	);
	_ddsstv_encoder_append_samples(
		self,
		self->sample_buffer,
		scanline_width,
		image_duration*0.5
	);
	dddsp_modulator_append_const_freq(
		&self->modulator,
		back_porch_duration*0.5,
		(self->mode.zero_freq+self->mode.max_freq)/2,
		self->amplitude
	);
}

static void
_ddsstv_encoder_append_scanline_ycbcr_420(ddsstv_encoder_t self, int scanline)
{
	const float scanline_duration = (float)self->mode.scanline_duration/USEC_PER_SEC;
	const float sync_duration = (float)self->mode.sync_duration/USEC_PER_SEC;
	const float front_porch_duration = (float)self->mode.front_porch_duration/USEC_PER_SEC;
	const float back_porch_duration = (float)self->mode.back_porch_duration/USEC_PER_SEC;
	const float total_image_duration = scanline_duration - (sync_duration + front_porch_duration + back_porch_duration) * 1.5;
	const float image_duration = total_image_duration / 1.5;


	size_t scanline_width = (*self->pull_scanline_cb)(
		self->context,
		scanline,
		&self->mode,
		DDSSTV_CHANNEL_Y,
		self->sample_buffer,
		self->sample_buffer_size
	);

	if (scanline_width > self->sample_buffer_size) {
		self->sample_buffer = realloc(self->sample_buffer, scanline_width);
		self->sample_buffer_size = scanline_width;

		scanline_width = (*self->pull_scanline_cb)(
			self->context,
			scanline,
			&self->mode,
			DDSSTV_CHANNEL_Y,
			self->sample_buffer,
			self->sample_buffer_size
		);
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


	if (!(scanline&1)) {
		scanline_width = (*self->pull_scanline_cb)(
			self->context,
			scanline,
			&self->mode,
			DDSSTV_CHANNEL_Cr,
			self->sample_buffer,
			self->sample_buffer_size
		);
		dddsp_modulator_append_const_freq(
			&self->modulator,
			sync_duration*0.5,
			self->mode.zero_freq,
			self->amplitude
		);
		dddsp_modulator_append_const_freq(
			&self->modulator,
			front_porch_duration*0.5,
			(self->mode.zero_freq+self->mode.max_freq)/2,
			self->amplitude
		);
		_ddsstv_encoder_append_samples(
			self,
			self->sample_buffer,
			scanline_width,
			image_duration*0.5
		);
		dddsp_modulator_append_const_freq(
			&self->modulator,
			back_porch_duration*0.5,
			(self->mode.zero_freq+self->mode.max_freq)/2,
			self->amplitude
		);
	}




	if (scanline&1) {
		scanline_width = (*self->pull_scanline_cb)(
			self->context,
			scanline,
			&self->mode,
			DDSSTV_CHANNEL_Cb,
			self->sample_buffer,
			self->sample_buffer_size
		);
		dddsp_modulator_append_const_freq(
			&self->modulator,
			sync_duration*0.5,
			self->mode.max_freq,
			self->amplitude
		);
		dddsp_modulator_append_const_freq(
			&self->modulator,
			front_porch_duration*0.5,
			(self->mode.zero_freq+self->mode.max_freq)/2,
			self->amplitude
		);
		_ddsstv_encoder_append_samples(
			self,
			self->sample_buffer,
			scanline_width,
			image_duration*0.5
		);
		dddsp_modulator_append_const_freq(
			&self->modulator,
			back_porch_duration*0.5,
			(self->mode.zero_freq+self->mode.max_freq)/2,
			self->amplitude
		);
	}
}

bool
ddsstv_encoder_process(ddsstv_encoder_t self)
{
	if (self->scanline == -1) {
		dddsp_modulator_append_silence(&self->modulator, 0.25);

		_ddsstv_encoder_append_header(self);

		self->scanline++;

	} else if (self->scanline >= 0 && self->scanline < self->mode.height) {

		switch (self->mode.color_mode) {
        case kDDSSTV_COLOR_MODE_GRAYSCALE:
            _ddsstv_encoder_append_scanline_bw(self, self->scanline);
            break;

        case kDDSSTV_COLOR_MODE_RGB:
            _ddsstv_encoder_append_scanline_rgb(self, self->scanline);
            break;

        case kDDSSTV_COLOR_MODE_YCBCR_422_601:
        case kDDSSTV_COLOR_MODE_YCBCR_422_709:
            _ddsstv_encoder_append_scanline_ycbcr_422(self, self->scanline);
            break;

        case kDDSSTV_COLOR_MODE_YCBCR_420_601:
        case kDDSSTV_COLOR_MODE_YCBCR_420_709:
            _ddsstv_encoder_append_scanline_ycbcr_420(self, self->scanline);
            break;
        }

		self->scanline++;

	} else if (self->scanline == self->mode.height) {
		dddsp_modulator_append_const_freq(&self->modulator, 300*0.001, 1900, self->amplitude);
		dddsp_modulator_append_silence(&self->modulator, 1);

		self->scanline++;
	}

	return self->scanline < self->mode.height+1;
}
