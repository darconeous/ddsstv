//
//  dddsp.c
//  SSTV
//
//  Created by Robert Quattlebaum on 10/30/13.
//  Copyright (c) 2013 deepdarc. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include "dddsp.h"
#include "string.h"
#include "math.h"

int32_t
dddsp_median_int32(int32_t a, int32_t b, int32_t c)
{
	if(a<c) {
		if(b<a) {
			return a;
		} else if(c<b) {
			return c;
		}
	} else {
		if(a<b) {
			return a;
		} else if(b<c) {
			return c;
		}
	}
	return b;
}

float
dddsp_median_float(float a, float b, float c)
{
	if(a<c) {
		if(b<a) {
			return a;
		} else if(c<b) {
			return c;
		}
	} else {
		if(a<b) {
			return a;
		} else if(b<c) {
			return c;
		}
	}
	return b;
}

float
dddsp_median_float_feed(dddsp_median_float_t self, float v)
{
	self->count++;
	if (self->count >= 3) {
		self->count = 0;
	}
	self->samples[self->count] = v;
	return dddsp_median_float(self->samples[0],self->samples[1],self->samples[2]);
}


void
dddsp_calc_min_max(const float samples[], size_t count, float* min, float* max)
{
	double high = samples[0];
	double low = samples[0];
	size_t i;
	for(i=0;i<count;i++) {
		if(high<samples[i])
			high = samples[i];
		if(low>samples[i])
			low = samples[i];

	}
	if(min)
		*min = low;
	if(max)
		*max = high;
}

float
dddsp_calc_bias(const float samples[], size_t count)
{
	float min, max;
	dddsp_calc_min_max(samples, count, &min, &max);
	return (max+min)*0.5;
}

float
dddsp_calc_amplitude(const float samples[], size_t count)
{
	float min, max;
	dddsp_calc_min_max(samples, count, &min, &max);
	return (max-min)*0.5;
}

void dddsp_box_filter_float(float samples[], size_t count, int box_size)
{
	if(box_size>1) {
		size_t i;
		float avg = 0;
		for(i=0;i<box_size;i++) {
			avg += samples[i];
		}
		for(i=box_size;i<count-box_size;i++) {
			samples[i] = avg/box_size;
			avg -= samples[i];
			avg += samples[i+box_size];
		}
	}
}

void
dddsp_double_freq(float* samples, size_t count)
{
	float min, max;
	size_t i;

	dddsp_calc_min_max(samples, count, &min, &max);
	if(isnan(min)||!isfinite(min)||isnan(max)||!isfinite(max))
		min = max = 0;

	for(i=0;i<count;i++) {
		samples[i] -= (max+min)*0.5;
		samples[i] *= samples[i];
	}

	dddsp_calc_min_max(samples, count, &min, &max);
	for(i=0;i<count;i++) {
		samples[i] -= (max+min)*0.5;
		samples[i] /= (max-min)*0.5;
	}
}

void
dddsp_samps_to_freqs(float out_freq[], const float in_sample[], ssize_t skip, size_t count, int sampleRate)
{
	float last_tick=0;
	float tick=0;
	int back = 0;
	float bias = -dddsp_calc_bias(in_sample,count);
	float last_sample = 0;
	const float threshold = 0.0001;
	bool above_zero = ((*in_sample+bias)>0);
	int zeroCount = 0;

	//fprintf(stderr,"amplitude: %f\n",dddsp_calc_amplitude(in_sample,count));

	while(count--) {
		if(above_zero != ((*in_sample+bias)>0)) {
			int i;
			float intersection = 0;

			// Find the approximate location between the two samples
			// of the zero crossing.
			intersection = (last_sample)/(last_sample-in_sample[0]-bias);

			tick += intersection;

			// linearly interpolate.
#if 1
			for(i=0; back>2 && i<back; i++) {
				if(-skip<back)
					continue;
				if(last_tick) {
					out_freq[i-back] = (sampleRate*0.5)/((tick-last_tick)*i/back+last_tick);
				} else if(tick) {
					out_freq[i-back] = (sampleRate*0.5)/(tick);
				}
			}
#endif

			above_zero = !above_zero;
			last_tick = tick;
			back = 0;
			tick = 1.0 - intersection;
		} else {
			tick++;
		}

		if(skip<=0) {
			if(zeroCount>5)
				*out_freq++ = 0;
			else if(last_tick)
				*out_freq++ = (sampleRate*0.5)/(last_tick);
			else
				*out_freq++ = last_sample;
		}

		last_sample = *in_sample+bias;
#if 1
		if(last_sample>threshold) {
			zeroCount = 0;
		} else {
			zeroCount++;
			if(zeroCount>20)
				back = 0;
		}
#endif
		in_sample++;
		skip--;
		back++;

	}
}
