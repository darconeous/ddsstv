//
//  dddsp-modulator.c
//  SSTV
//
//  Created by Robert Quattlebaum on 3/25/14.
//  Copyright (c) 2014 deepdarc. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "dddsp-modulator.h"

void
dddsp_modulator_init(struct dddsp_modulator_s* self)
{
	memset(self,0,sizeof(*self));
	self->multiplier = 1.0;
}

void
dddsp_modulator_finalize(struct dddsp_modulator_s* self)
{
	free(self->buffer);
}

void
dddsp_modulator_append_silence(
	struct dddsp_modulator_s* self,
	float duration
)
{
	return dddsp_modulator_append_const_freq(
		self,
		duration,
		0.0f,
		0.0f
	);
}

void
dddsp_modulator_append_const_freq(
	struct dddsp_modulator_s* self,
	float duration,
	float freq,
	float amplitude
)
{
	if (duration == 0.0) {
		goto bail;
	}

	if (NULL == self->buffer) {
		self->buffer_index = 0;
		if (!self->buffer_size) {
			self->buffer_size = 128;
		}
		self->buffer = malloc(self->buffer_size*sizeof(float));
		if (NULL == self->buffer) {
			goto bail;
		}
	}

	duration *= self->multiplier;
	duration += self->leftover;
	intptr_t samples = (intptr_t)floor(duration);
	float delta_theta = freq*M_PI*2.0/self->multiplier;
	intptr_t index;

	self->buffer_index = 0;

	if (amplitude == 0.0) {
		delta_theta = 0;
		self->phase = 0;
	}

	self->phase -= delta_theta * self->leftover;

	for (index = 0; index<samples; index++) {
		float value = 0;

		if (!self->pass_thru) {
			if (amplitude != 0.0) {
				self->phase += delta_theta;

				if (self->phase > M_PI*2.0) {
					self->phase -= M_PI*2.0;
				}

				value = sin(self->phase) * amplitude;
			}
		} else {
			value = freq;
		}

		self->buffer[self->buffer_index++] = value;

		if(self->buffer_index == self->buffer_size) {
			if (self->output_func) {
				(*self->output_func)(
					self->output_func_context,
					self->buffer,
					self->buffer_index
				);
			}
			self->buffer_index = 0;
		}
	}

	self->leftover = duration - (double)samples;
	self->phase += delta_theta * self->leftover;

	if(self->buffer_index && self->output_func) {
		(*self->output_func)(
			self->output_func_context,
			self->buffer,
			self->buffer_index
		);
	}

bail:
	return;
}
