//
//  dddsp-modulator.h
//  SSTV
//
//  Created by Robert Quattlebaum on 3/24/14.
//  Copyright (c) 2014 deepdarc. All rights reserved.
//

#ifndef SSTV_dddsp_modulator_h
#define SSTV_dddsp_modulator_h

#include <stdbool.h>

typedef void (*dddsp_modulator_output_func)(void* context, const float* samples, size_t count);

struct dddsp_modulator_s {
	float phase;
	float leftover;

	float *buffer;
	size_t buffer_index;
	size_t buffer_size;

	float multiplier; //!< Typically samples per second

	dddsp_modulator_output_func output_func;
	void* output_func_context;
};

void dddsp_modulator_init(struct dddsp_modulator_s* modulator);
void dddsp_modulator_finalize(struct dddsp_modulator_s* modulator);

void dddsp_modulator_append_silence(
	struct dddsp_modulator_s* modulator,
	float duration
);

void dddsp_modulator_append_const_freq(
	struct dddsp_modulator_s* modulator,
	float duration,		//!< In Samples
	float freq,			//!< In cycles-per-sample
	float amplitude
);

#endif
