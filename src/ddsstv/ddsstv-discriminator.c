//
//  ddsstv-discriminator.c
//  SSTV
//
//  Created by Robert Quattlebaum on 11/20/13.
//  Copyright (c) 2013 deepdarc. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dddsp/dddsp.h>
#include "ddsstv-discriminator.h"

static bool _dddsp_resampler_output_func(void* context, const float* samples, size_t count);

void
ddsstv_discriminator_init(ddsstv_discriminator_t discriminator, double ingest_sample_rate, double output_sample_rate)
{
	memset(discriminator,0,sizeof(*discriminator));

	dddsp_resampler_init(&discriminator->resampler, ingest_sample_rate, output_sample_rate);
	discriminator->resampler.output_func = &_dddsp_resampler_output_func;
	discriminator->resampler.output_func_context = discriminator;

	// TODO: REMOVE ME
	discriminator->pass_thru = true;

	discriminator->ingest_processing_int16 = calloc(1,sizeof(*discriminator->ingest_processing_int16)*output_sample_rate);

	bool high_quality = true;
	bool aggressive_filtering = false;

	discriminator->freq_disc = dddsp_discriminator_alloc(
		discriminator->resampler.output_sample_rate,
		1700.0/discriminator->resampler.output_sample_rate,
		(high_quality&&aggressive_filtering)?700.0/discriminator->resampler.output_sample_rate:0,
		high_quality
	);

	if (high_quality) {
		discriminator->freq_disc_sync = dddsp_discriminator_alloc(
			discriminator->resampler.output_sample_rate,
			1200.0/discriminator->resampler.output_sample_rate,
			(aggressive_filtering?240.0:300.0)/discriminator->resampler.output_sample_rate,
			high_quality
		);
    }
}

void
ddsstv_discriminator_finalize(ddsstv_discriminator_t discriminator)
{
	dddsp_resampler_finalize(&discriminator->resampler);
	free(discriminator->ingest_processing_int16);
	memset(discriminator,0,sizeof(*discriminator));
}

bool
_dddsp_resampler_output_func(void* context, const float* samples, size_t count)
{
	ddsstv_discriminator_t discriminator = context;
	bool ret = false;
	ssize_t freq_count = 0;
	int16_t* freq_buffer = discriminator->ingest_processing_int16;

	if (!count) {
		ret = discriminator->output_func(
			discriminator->output_func_context,
			NULL,
			0
		);
	}

	while (count--) {
		float v = *samples++;

		if (!discriminator->pass_thru) {
			if (discriminator->freq_disc_sync) {
				float low_v = dddsp_discriminator_feed(discriminator->freq_disc_sync, v);
				v = dddsp_discriminator_feed(discriminator->freq_disc, v);

				if (!isfinite(v) || (low_v<1350.0 && v<1400.0)) {
					v = low_v;
				}
			} else {
				v = dddsp_discriminator_feed(discriminator->freq_disc, v);
			}
		}

		// Quantize
		freq_buffer[freq_count++] = (int16_t)(v+0.5f);

		if (freq_count >= discriminator->resampler.output_sample_rate) {
			ret |= discriminator->output_func(
				discriminator->output_func_context,
				freq_buffer,freq_count
			);
			freq_count = 0;
		}
	}
	if(freq_count) {
		ret |= discriminator->output_func(
			discriminator->output_func_context,
			freq_buffer,freq_count
		);
	}
	return ret;
}


bool
ddsstv_discriminator_ingest_samps(ddsstv_discriminator_t discriminator, const float* samps, size_t count)
{
	return dddsp_resampler_ingest_samps(&discriminator->resampler, samps, count);
}

//bool
//ddsstv_discriminator_get_high_res(ddsstv_discriminator_t discriminator)
//{
//	return discriminator->hires_filter != NULL;
//}
//
//void
//ddsstv_discriminator_set_high_res(ddsstv_discriminator_t discriminator, bool hi_res)
//{
//	float filter_top = 2350.0;
//	float filter_bottom = 1050.0;
//
//	if(hi_res==ddsstv_discriminator_get_high_res(discriminator))
//		return;
//
//	if(hi_res) {
//		discriminator->hires_filter = dddsp_iir_float_alloc_band_pass(
//			filter_bottom/discriminator->ingest_sample_rate*2,
//			filter_top/discriminator->ingest_sample_rate*2,
//			0.1,
//			4
//		);
//	} else {
//		dddsp_iir_float_finalize(discriminator->hires_filter);
//		discriminator->hires_filter = NULL;
//	}
//}
