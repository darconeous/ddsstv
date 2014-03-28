//
//  dddsp-resampler.h
//  SSTV
//
//  Created by Robert Quattlebaum on 3/21/14.
//  Copyright (c) 2014 deepdarc. All rights reserved.
//

#ifndef SSTV_dddsp_resampler_h
#define SSTV_dddsp_resampler_h

#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include "dddsp-filter.h"

void dddsp_resample_int16(int16_t out_samples[], size_t out_count, const int16_t in_samples[], float in_start, float in_stop);

void dddsp_resample_float(float out_samples[], size_t out_count, const float in_samples[], float in_start, float in_stop);

typedef bool (*dddsp_resampler_output_func)(void* context, const float* samples, size_t count);
struct dddsp_resampler_s {
	double output_sample_rate;
	double ingest_sample_rate;

	int intermediate_factor;
	double intermediate_sample_rate;
	float *ingest_processing;
	double out_bucket;
	double out_error_bucket;
	dddsp_iir_float_t filter;

	dddsp_resampler_output_func output_func;
	void* output_func_context;
};
typedef struct dddsp_resampler_s *dddsp_resampler_t;

extern void dddsp_resampler_init(dddsp_resampler_t resampler, double ingest_sample_rate, double output_sample_rate);
extern void dddsp_resampler_finalize(dddsp_resampler_t resampler);
extern bool dddsp_resampler_ingest_samps(dddsp_resampler_t resampler, const float* samps, size_t count);
extern bool dddsp_resampler_ingest_samps_uint16(dddsp_resampler_t resampler, const uint16_t* samps, size_t count);

#endif
