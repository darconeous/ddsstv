//
//  dddsp-discriminator.c
//  SSTV
//
//  Created by Robert Quattlebaum on 3/27/14.
//  Copyright (c) 2014 deepdarc. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include "dddsp-discriminator.h"
#include "dddsp-filter.h"

struct dddsp_discriminator_s {
	bool high_quality;
	float sample_rate;

	float carrier;

	union {
		float theta;
		unsigned int current_step;
	};

	union {
		float last_angle;
		float v_i;
	};
	float v_q;

	dddsp_iir_float_t filter;
	dddsp_iir_float_t filter_i;
	dddsp_iir_float_t filter_q;
};

dddsp_discriminator_t
dddsp_discriminator_alloc(float sample_rate, float carrier, float max_deviation, bool high_quality)
{
	dddsp_discriminator_t self = NULL;

	if(carrier>0.250001)
		goto bail;

	self = calloc(sizeof(struct dddsp_discriminator_s),1);

	if (!self)
		goto bail;

	if((max_deviation <= 0) || (max_deviation > carrier*0.5))
		max_deviation = carrier*0.5;

	self->sample_rate = sample_rate;
	self->high_quality = high_quality;
	self->carrier = carrier;
	self->filter = dddsp_iir_float_alloc_low_pass(max_deviation, DDDSP_IIR_MAX_RIPPLE, 2);
	self->filter_i = dddsp_iir_float_alloc_low_pass(2.0*max_deviation, DDDSP_IIR_MAX_RIPPLE, 2);
	self->filter_q = dddsp_iir_float_alloc_low_pass(2.0*max_deviation, DDDSP_IIR_MAX_RIPPLE, 2);
bail:
	return self;
}

void
dddsp_discriminator_finalize(dddsp_discriminator_t self)
{
	dddsp_iir_float_finalize(self->filter_i);
	dddsp_iir_float_finalize(self->filter_q);
	free(self);
}

float
dddsp_discriminator_feed(dddsp_discriminator_t self, float sample)
{
	float ret;
	float v_i = 0;
	float v_q = 0;

	if(isnan(sample) || !isfinite(sample))
		sample = 0.0;

	if(self->carrier >= 0.249999) {
		self->carrier = 0.25;
		switch((self->current_step++)%4) {
		case 0:
			v_i = sample;
			v_q = 0;
			break;
		case 1:
			v_i = 0;
			v_q = sample;
			break;
		case 2:
			v_i = -sample;
			v_q = 0;
			break;
		case 3:
			v_i = 0;
			v_q = -sample;
			break;
		}
	} else {
		self->theta += self->carrier*2.0*M_PI;
		if (self->theta>M_PI)
			self->theta -= 2.0*M_PI;
		v_i = sample*cosf(self->theta);
		v_q = sample*sinf(self->theta);
	}

	if(isnan(v_i) || !isfinite(v_i) || isnan(v_q) || !isfinite(v_q))
		return NAN;

	v_i = dddsp_iir_float_feed(self->filter_i, v_i);
	v_q = dddsp_iir_float_feed(self->filter_q, v_q);

	if(self->high_quality) {
		ret = -self->last_angle;
		self->last_angle = atan2f(v_q,v_i);
		ret += self->last_angle;
		if (ret>M_PI)
			ret -= 2.0*M_PI;
		if (ret<-M_PI)
			ret += 2.0*M_PI;
	} else {
		ret = (v_q*self->v_i - v_i*self->v_q)/(v_i*v_i + v_q*v_q);
		self->v_i = v_i;
		self->v_q = v_q;
	}

	if(isnan(ret) || !isfinite(ret))
		return NAN;

	ret /= (-2.0*M_PI) * self->carrier;
	ret += 1.0f;
	ret *= self->sample_rate * self->carrier;

	ret = dddsp_iir_float_feed(self->filter, ret);

	return ret;
}
