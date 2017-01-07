//
//  dddsp-misc.h
//  SSTV
//
//  Created by Robert Quattlebaum on 3/21/14.
//  Copyright (c) 2014 deepdarc. All rights reserved.
//

#ifndef SSTV_dddsp_misc_h
#define SSTV_dddsp_misc_h

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <math.h>

static inline float
dddsp_clampf(float value, float nanvalue, float min, float max) {
	value = (isnan(value)||!isfinite(value))?nanvalue:value;
	if (value>max) {
		value = max;
    }
	if (value<min) {
		value = min;
    }
	return value;
}

void dddsp_box_filter_float(float samples[], size_t count, int box_size);

void dddsp_calc_min_max(const float samples[], size_t count, float* min, float* max);

void dddsp_double_freq(float* samples, size_t count);

void dddsp_samps_to_freqs(float out_freq[], const float in_sample[], ssize_t skip, size_t count, int sampleRate);

int32_t dddsp_median_int32(int32_t a, int32_t b, int32_t c);
float dddsp_median_float(float a, float b, float c);

struct dddsp_median_float_s {
	float samples[3];
	uint8_t count;
};
typedef struct dddsp_median_float_s *dddsp_median_float_t;
float dddsp_median_float_feed(dddsp_median_float_t self, float v);


#endif
