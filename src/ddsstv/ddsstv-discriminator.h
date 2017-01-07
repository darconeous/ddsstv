//
//  ddsstv-discriminator.h
//  SSTV
//
//  Created by Robert Quattlebaum on 3/27/14.
//  Copyright (c) 2014 deepdarc. All rights reserved.
//

#ifndef SSTV_ddsstv_discriminator_h
#define SSTV_ddsstv_discriminator_h

#include <stdbool.h>
#include <stdint.h>
#include <dddsp/dddsp.h>

typedef bool (*ddsstv_discriminator_output_func)(void* context, const int16_t* freqs, size_t count);

struct ddsstv_discriminator_s {
	struct dddsp_resampler_s resampler;
	int16_t *ingest_processing_int16;

	dddsp_discriminator_t freq_disc;
	dddsp_discriminator_t freq_disc_sync;
	dddsp_iir_float_t filter;
	struct dddsp_median_float_s median_filter;

	ddsstv_discriminator_output_func output_func;
	void* output_func_context;

	bool pass_thru;
};
typedef struct ddsstv_discriminator_s *ddsstv_discriminator_t;

extern void ddsstv_discriminator_init(
	ddsstv_discriminator_t discriminator,
	double ingest_sample_rate,
	double output_sample_rate
);

extern void ddsstv_discriminator_finalize(ddsstv_discriminator_t discriminator);

extern bool ddsstv_discriminator_ingest_samps(
	ddsstv_discriminator_t discriminator,
	const float* samps,
	size_t count
);

#endif
