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

//"Extracting the fundamental frequency from a time-domain signal" is
//basically a long-winded way of saying "demodulating the baseband
//signal from FM-encoded signal". Oh wait, that's almost just as long
//winded. Oh well.
//
//In an FM radio, the contraption that accomplishes this task is
//referred to as a "discriminator". So the task here is to implement a
//software-based frequency discriminator. There are a few different ways
//to accomplish this.
//
//The easiest is to count zero crossings. It is conceptually
//straightforward and it is relatively immune to amplitude noise.
//However, good results require a relatively high-sample rate, in order
//to get good accuracy on the exact location of each zero crossing
//(which may occur between samples when sampled at lower sample rates).
//It is also not continuous: you have no information about the signal
//between the zero crossings---so you have to guess. I ended up linearly
//interpolating in my implementation with acceptable results, but some
//sort of IIR or FIR filter would be more appropriate for certain
//domains.
//
//As an aside, if you can guarantee that *only* one frequency will be
//present (with *no* noise) and you are sampling at a relatively high
//sample rate, then you can *double* the resolution of the zero-crossing
//detector by multiplying the signal with itself. This will effectively
//double the frequency of the signal. However, my own experimentation
//has shown this technique to be useless when there is noise present.
//
//In hardware, some discriminators use phase-locked-loop (PLL) to
//discipline an oscillator to match the frequency detected. The
//adjustments used to adjust the oscillator is the demodulated baseband
//signal. It is possible to simulate this with software. I have not
//tried doing this.
//
//A somewhat heavy-handed approach to implementing a discriminator is to
//do an FFT on the signal and then use a heuristic to determine the
//fundamental frequency from the FFT output. While somewhat
//straightforward, this was something I was trying to avoid. It just
//seemed like there should be a better solution.
//
//The method I'm using is somewhat different. The basic idea is this:
//
//1.  Break the signal into quadrature (I and Q) signals
//2.  Use I and Q to calculate the instantaneous angle via a simple arc
//    tangent.
//3.  Use the difference between the last two decoded angles to
//    calculate angular velocity
//4.  Convert the angular velocity into a frequency with some simple
//    linear math.
//
//\#1 is a lot more simple than you might otherwise imagine. It is
//easiest to describe it with code. For each `sample` that is input,
//into the discriminator, you do the following:
//
//// Constants const float sample\_rate;
//
//// Persistent state float prev*i = 0; float prev*q = 0; float
//last\_angle = 0; int phase = 0;
//
//// Per-sample state float v*i = 0; float v*q = 0; float sample; // Our
//input, a sample float ret; // Our output, a frequency
//
//switch(phase) { case 0: v*i = sample; v*q = 0; phase++; break; case 1:
//v*i = 0; v*q = sample; phase++; break; case 2: v*i = -sample; v*q = 0;
//phase++; break; case 3: v*i = 0; v*q = -sample; phase = 0; break; }
//
//We now have our I and Q signals, but there is a great deal of
//high-frequency noise on them, making them useless without additional
//filtering. While a FIR filter (via a convolution kernel) would be the
//preferable filter, I found a simple 2-pole IIR filter to be adequate
//for SSTV demodulation and is significantly faster than a larger (but
//more optimal) FIR filter. I pre-calculated the coefficients for a a
//two-pole low-pass IIR filter with a cutoff frequency of 0.25 times the
//sample rate. Applying the filter is then fairly easy:
//
//v*i = iir*float*feed(filter*i, v*i); v*q = iir*float*feed(filter*q,
//v*q);
//
//Once you have your filtered I and Q signals, things get fairly
//straightforward:
//
//ret = -self->last*angle; self->last*angle = atan2f(v*q,v*i); ret +=
//self->last\_angle;
//
//"But hey!", you might exclaim, "How is this 'fast' if we are relying
//on that slow `atan2f()` call?"
//
//This is where the magic comes in. Have a look at this beauty, which
//completely replaces the code above:
//
//ret = (v*q\*prev*i - v*i\*prev*q)/(v*i\*v*i + v*q\*v*q); prev*i = v*i;
//prev*q = v*q;
//
//What we've done is collapse steps #2 and #3 into a single operation
//that involves only basic math. Now, if you were wondering, this code
//is indeed only an approximation---but it is a pretty damn good one
//based on my experience.
//
//You can then convert the angular velocity to a frequency with some
//simple math:
//
//ret = (1.0f-ret/M*PI)+sample*rate;
//
//You could also extract the instantaneous amplitude with the help of
//everyone's favorite religious zealot, Pythagorus: `sort(v_i*v_i +
//v_q*v_q)`. This is, however, not terribly useful for SSTV
//demodulation.
//
//In conclusion: This algorithm is fantastic. It is fast: needs no trig
//functions and it is a perfect candidate for SIMD operations. It is
//super easy to use: one sample goes in, one frequency comes out. It is
//quite low-latency as well, with the latency limited primarily by the I
//and Q filters.
//
//More detailed derivation of this algorithm can be found here:
//
//http://www.agurk.dk/bjarke/projects/dsp/dsp%20fm%20demodulator%20for%20sstv.htm

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
//	self->filter = dddsp_iir_float_alloc_low_pass(2.0*max_deviation, DDDSP_IIR_MAX_RIPPLE, 2);
#if 0
	self->filter_i = dddsp_iir_float_alloc_low_pass(2.0*max_deviation, DDDSP_IIR_MAX_RIPPLE, 4);
	self->filter_q = dddsp_iir_float_alloc_low_pass(2.0*max_deviation, DDDSP_IIR_MAX_RIPPLE, 4);
#else
	self->filter_i = dddsp_fir_float_alloc_low_pass(2.0*max_deviation, 23);
	self->filter_q = dddsp_fir_float_alloc_low_pass(2.0*max_deviation, 23);
#endif
bail:
	return self;
}

void
dddsp_discriminator_finalize(dddsp_discriminator_t self)
{
	dddsp_iir_float_finalize(self->filter_i);
	dddsp_iir_float_finalize(self->filter_q);
	dddsp_iir_float_finalize(self->filter);
	free(self);
}

int
dddsp_discriminator_get_delay(dddsp_discriminator_t self)
{
	return dddsp_iir_float_get_delay(self->filter)
		+ (
			dddsp_iir_float_get_delay(self->filter_i)
			+ dddsp_iir_float_get_delay(self->filter_q)
		) / 2;
}

float
dddsp_discriminator_feed(dddsp_discriminator_t self, float sample)
{
	// Algorithm based on:
	// http://www.agurk.dk/bjarke/projects/dsp/dsp%20fm%20demodulator%20for%20sstv.htm
	//
	float ret;
	float v_i = 0;
	float v_q = 0;

	if(isnan(sample) || !isfinite(sample))
		sample = 0.0;

	if(self->carrier >= 0.249999) {
		self->carrier = 0.25;
		switch(self->current_step) {
		case 0:
			v_i = sample;
			v_q = 0;
			self->current_step++;
			break;
		case 1:
			v_i = 0;
			v_q = sample;
			self->current_step++;
			break;
		case 2:
			v_i = -sample;
			v_q = 0;
			self->current_step++;
			break;
		case 3:
			v_i = 0;
			v_q = -sample;
			self->current_step = 0;
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
