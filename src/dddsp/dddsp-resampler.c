//
//  dddsp-resampler.c
//  SSTV
//
//  Created by Robert Quattlebaum on 3/21/14.
//  Copyright (c) 2014 deepdarc. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dddsp-resampler.h"

#ifndef DDDSP_USE_FASTEST_MINIFY
#define DDDSP_USE_FASTEST_MINIFY 0
#endif
#ifndef DDDSP_USE_FASTER_MINIFY
#define DDDSP_USE_FASTER_MINIFY 0
#endif



void
dddsp_resample_int16(
	int16_t out_samples[],
	size_t out_count,
	const int16_t in_samples[],
	float in_start,
	float in_stop
) {
	size_t in_count = (in_stop-in_start);
#if 0
	in_start = floor(in_start);
	in_stop = floor(in_stop);
#endif
	in_samples += (int)floor(in_start);
	in_stop -= (int)floor(in_start);
	in_start -= (int)floor(in_start);
	size_t i;
	if(out_count < in_count) {
#if DDDSP_USE_FASTEST_MINIFY
		for(i=0;i<out_count;i++) {
			out_samples[i] = in_samples[(int)(i*(in_stop-in_start)+(int)(in_start*out_count))/out_count];
		}
#else
		const size_t out_overflow = in_count;
		const size_t out_increment = out_count;
		size_t j = 0;
		int32_t out_bucket = (*in_samples)*(1.0+(floor(in_start)-in_start));
		ssize_t out_error_bucket = out_increment*(1.0+(floor(in_start)-in_start));
		for(i=0;i<in_count;i++) {
			out_error_bucket += out_increment;
			j++;

			if(out_error_bucket >= out_overflow) {
				int32_t v = *in_samples++;

				out_error_bucket -= out_overflow;

#if DDDSP_USE_FASTER_MINIFY
				out_bucket += v;
				*out_samples++ = out_bucket/j;
				j = 0;
				out_bucket = 0;
#else
				out_bucket += v*(out_increment-out_error_bucket)/out_increment;
				*out_samples++ = out_bucket*out_increment/out_overflow;
				j = 0;
				out_bucket = v*(int32_t)out_error_bucket/(int32_t)out_increment;
#endif
			} else {
				out_bucket += *in_samples++;
			}
		}
		if(j)
			*out_samples/=j;
#endif
	} else {
		// Getting Bigger
		for(i=0;i<out_count;i++) {
			out_samples[i] = in_samples[(int)(i*(in_stop-in_start)+(int)(in_start*out_count))/out_count];
		}
	}
}



void
dddsp_resample_float(
	float out_samples[],
	size_t out_count,
	const float in_samples[],
	float in_start,
	float in_stop
) {
	size_t in_count = (in_stop-in_start);
	in_samples += (int)floor(in_start);
	in_stop -= (int)floor(in_start);
	in_start -= (int)floor(in_start);
	size_t i;
	if(out_count < in_count) {
#if DDDSP_USE_FASTEST_MINIFY
		for(i=0;i<out_count;i++) {
			out_samples[i] = in_samples[(int)(i*(in_stop-in_start)+(int)(in_start*out_count))/out_count];
		}
#else
		const size_t out_overflow = in_count;
		const size_t out_increment = out_count;
		double out_bucket = (*in_samples)*(1.0+(floor(in_start)-in_start));
		ssize_t out_error_bucket = out_increment*(1.0+(floor(in_start)-in_start));
		size_t j = 0;
		for(i=0;i<in_count;i++) {
			out_error_bucket += out_increment;
			j++;

			if(out_error_bucket >= out_overflow) {
				float v = *in_samples++;

				out_error_bucket -= out_overflow;

#if DDDSP_USE_FASTER_MINIFY
				out_bucket += v;
				*out_samples++ = out_bucket/j;
				j = 0;
				out_bucket = 0;
#else
				out_bucket += v*(out_increment-out_error_bucket)/out_increment;
				*out_samples++ = out_bucket*out_increment/out_overflow;
				j = 0;
				out_bucket = v*out_error_bucket/out_increment;
#endif
			} else {
				out_bucket += *in_samples++;
			}
		}
#endif
	} else {
		// Getting Bigger
		for(i=0;i<out_count;i++) {
			out_samples[i] = in_samples[(int)(i*(in_stop-in_start)+(int)(in_start*out_count))/out_count];
		}
	}
}



void
dddsp_resampler_init(dddsp_resampler_t resampler, double ingest_sample_rate, double output_sample_rate)
{
	memset(resampler,0,sizeof(*resampler));

	resampler->ingest_sample_rate = ingest_sample_rate;
	resampler->output_sample_rate = output_sample_rate;

	resampler->intermediate_factor = 6*output_sample_rate/ingest_sample_rate;
	if(resampler->intermediate_factor<1)
		resampler->intermediate_factor = 1;
	resampler->intermediate_sample_rate = ingest_sample_rate*resampler->intermediate_factor;

	resampler->ingest_processing = calloc(1,sizeof(*resampler->ingest_processing)*(size_t)ceil(resampler->ingest_sample_rate)*2);

	float freq = 0.5/resampler->intermediate_factor * (output_sample_rate/ingest_sample_rate);
	resampler->filter = dddsp_iir_float_alloc_low_pass(
		freq,
		1.0,
		6
	);
}

void
dddsp_resampler_finalize(dddsp_resampler_t resampler)
{
	free(resampler->ingest_processing);
	memset(resampler,0,sizeof(*resampler));
}


bool
dddsp_resampler_ingest_samps(dddsp_resampler_t resampler, const float* samps, size_t count)
{
	bool ret = false;

	if(fabs(resampler->ingest_sample_rate-resampler->output_sample_rate)<0.001) {
		return resampler->output_func(
			resampler->output_func_context,samps,count
		);
	}


	ssize_t freq_count = 0;
	float* freq_buffer = resampler->ingest_processing;
	if(!count) {
		ret = resampler->output_func(
			resampler->output_func_context,
			NULL,
			0
		);
	}
	while(count--) {
		const double out_overflow = resampler->intermediate_sample_rate;
		const double out_increment = resampler->output_sample_rate;
		float v = *samps++;
		int i;

		for (i=0;i<resampler->intermediate_factor;i++) {
			if (i!=0) {
				v = 0;
			}
			v = dddsp_iir_float_feed(resampler->filter, v*resampler->intermediate_factor);
			resampler->out_error_bucket += out_increment;
			if(resampler->out_error_bucket >= out_overflow) {
				resampler->out_error_bucket -= out_overflow;
				resampler->out_bucket += v*(out_increment-resampler->out_error_bucket)/out_increment;

				freq_buffer[freq_count++] = resampler->out_bucket*out_increment/out_overflow;

				resampler->out_bucket = v*resampler->out_error_bucket/out_increment;

				if(freq_count >= resampler->output_sample_rate) {
					ret |= resampler->output_func(
						resampler->output_func_context,
						freq_buffer,freq_count
					);
					freq_count = 0;
				}
			} else {
				resampler->out_bucket += v;
			}
		}

	}
	if(freq_count) {
		ret |= resampler->output_func(
			resampler->output_func_context,
			freq_buffer,freq_count
		);
	}
	return ret;
}

bool
dddsp_resampler_ingest_samps_uint16(dddsp_resampler_t resampler, const uint16_t* samps, size_t count)
{
	return false;
}
