//
//  dddsp-filter.c
//  SSTV
//
//  Created by Robert Quattlebaum on 3/27/14.
//  Copyright (c) 2014 deepdarc. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dddsp-filter.h"


static int
_chebyshev_calc(double a[3], double b[3], int poles, int p, double cutoff1, float cutoff2, double ripple, dddsp_filter_type_t type)
{
	const double theta_p = 1.0;
	double rp, ip;
	double x0, x1, x2;
	double     y1, y2;

	// Calculate the pole location on the unit circle.
	rp = -cos(M_PI/(poles*2.0) + (p-1.0)*M_PI/poles);
	ip = sin(M_PI/(poles*2.0) + (p-1.0)*M_PI/poles);

	if(ripple > 0.0001) {
		// Warp form a circle into an elipse.
		float es, vx, kx;

		es = (100.0/(100.0 - ripple));
		es = sqrt(es*es - 1.0);
		vx = (1.0/poles)*log10((1.0/es) + sqrt(1.0/(es*es)+1.0));
		kx = (1.0/poles)*log10((1.0/es) + sqrt(1.0/(es*es)-1.0));
		kx = pow(10, kx) + pow(10,-kx)/2.0;

		rp = rp*(pow(10, vx) - pow(10,-vx)/2.0)/kx;
		rp = rp*(pow(10, vx) + pow(10,-vx)/2.0)/kx;
	}


	{
		// S-domain to Z-domain transformation.
		double t, m, d;

		t = 2.0*tan(1.0/2.0);
		m = rp*rp + ip*ip;

		d = 4.0 - 4.0*rp*t + m*t*t;

		x0 = x2 = t*t/d;
		x1 = 2.0*x0;

		y1 = (8.0 - 2*m*t*t)/d;
		y2 = (-4.0 - 4.0*rp*t - m*t*t)/d;
	}

	{
		if(DDDSP_FILTER_TYPE_IS_BAND(type)) {
			// LP-to-BP or LP-to-BS transformation
			double mu_p1 = 2.0*M_PI*cutoff1;
			double mu_p2 = 2.0*M_PI*cutoff2;

			double alpha, k;

			alpha = cos((mu_p2 + mu_p1)/2.0)/cos((mu_p2 - mu_p1)/2.0);

			if(type == DDDSP_BANDPASS) {
				k = tan(theta_p/2.0)/tan((mu_p2 - mu_p1)/2.0);
			} else {
				k = tan(theta_p/2.0)*tan((mu_p2 - mu_p1)/2.0);
			}

		} else {
			// LP-to-LP or LP-to-HP transformation
			double mu_p = 2.0*M_PI*cutoff1;
			double d, alpha;
			const double y0 = -1.0;

			if(type == DDDSP_HIGHPASS) {
				alpha = -cos((theta_p + mu_p)/2.0)/cos((theta_p - mu_p)/2.0);
			} else {
				alpha = sin((theta_p - mu_p)/2.0)/sin((theta_p + mu_p)/2.0);
			}

			d = 1.0 + y1*alpha - y2*alpha*alpha;

			a[0] = (x0 - x1*alpha + x2*alpha*alpha)/d;
			a[1] = (x1 - 2.0*x0*alpha - 2.0*x2*alpha + x1*alpha*alpha)/d;
			a[2] = (x2 - x1*alpha + x0*alpha*alpha)/d;

			b[1] = (y1 - 2.0*y0*alpha - 2.0*y2*alpha + y1*alpha*alpha)/d;
			b[2] = (y2 - y1*alpha + y0*alpha*alpha)/d;

			if(type == DDDSP_HIGHPASS) {
				a[1] = -a[1];
				b[1] = -b[1];
			}
		}
	}

	return 0;
}

double
dddsp_calc_iir_gain_low_double(double a[], double b[], int poles)
{
	int i;
	double sa = 0, sb = 0;
	for(i=0;i<=poles;i++) {
		sa += *a++;
		sb += *b++;
	}
	double gain = sa/(1.0-sb);
//	fprintf(stderr,"low: sa=%f sb=%f gain=%f\n",sa,sb,gain);
	return gain;
}

double
dddsp_calc_iir_gain_high_double(double a[], double b[], int poles)
{
	int i;

	double sa = 0, sb = 0;
	for(i=0;i<=poles;i++) {
		sa += *a++ * (1.0-2.0*(i&1));
		sb += *b++ * (1.0-2.0*(i&1));
	}
	double gain = sa/(1.0-sb);
//	fprintf(stderr,"high: sa=%f sb=%f gain=%f\n",sa,sb,gain);
	return gain;
}


void
dddsp_adjust_iir_gain_double(double a[], int poles, double gain)
{
	int i;
	for(i=0;i<=poles;i++)
		*a++ *= gain;
}

//static void
//_bandpass_calc_double(double a[3], double b[3], double cutoff1, double cutoff2)
//{
//	double mu_p1 = 2.0*M_PI*cutoff1;
//	double mu_p2 = 2.0*M_PI*cutoff2;
//	double cos_mu = cos((mu_p2+mu_p1)/2.0);
//	double r = 1.0 - 3.0*(cutoff2-cutoff1);
//	double k = (1.0 - 2.0*r*cos_mu + r*r)/(2.0 - 2.0*cos_mu);
//	a[0] = 1.0 - k;
//	a[1] = 2.0*(k-r)*cos_mu;
//	a[2] = r*r - k;
//	b[1] = 2.0*r*cos_mu;
//	b[2] = -r*r;
//	b[0] = 0;
//}
//
//static void
//_notch_calc_double(double a[3], double b[3], double freq, double bandwidth)
//{
//	double cos_mu = cos(2.0*M_PI*freq);
//	double r = 1.0 - 3.0*bandwidth;
//	double k = (1.0 - 2.0*r*cos_mu + r*r)/(2.0 - 2.0*cos_mu);
//	a[0] = k;
//	a[1] = -2.0*k*cos_mu;
//	a[2] = k;
//	b[1] = 2.0*r*cos_mu;
//	b[2] = -r*r;
//	b[0] = 0;
//}
//
//static void
//_bandpass_calc_float(float a[3], float b[3], double cutoff1, double cutoff2)
//{
//	double mu_p1 = 2.0*M_PI*cutoff1;
//	double mu_p2 = 2.0*M_PI*cutoff2;
//	double cos_mu = cos((mu_p2+mu_p1)/2.0);
//	double r = 1.0 - 3.0*(cutoff2-cutoff1);
//	double k = (1.0 - 2.0*r*cos_mu + r*r)/(2.0 - 2.0*cos_mu);
//	a[0] = 1.0 - k;
//	a[1] = 2.0*(k-r)*cos_mu;
//	a[2] = r*r - k;
//	b[1] = 2.0*r*cos_mu;
//	b[2] = -r*r;
//	b[0] = 0;
//}
//
//static void
//_dump_iir_coeff_float(float a[], float b[], int poles) {
//	int i;
//	fprintf(stderr,"%d pole filter:\n",poles);
//	for(i=0;i<poles+1;i++) {
//		fprintf(stderr,"\ta[%d]=%f \tb[%d]=%f\n",i,a[i],i,b[i]);
//	}
//}
//
//static void
//_dump_iir_coeff_double(double a[], double b[], int poles) {
//	int i;
//	fprintf(stderr,"%d pole filter:\n",poles);
//	for(i=0;i<poles+1;i++) {
//		fprintf(stderr,"\ta[%d]=%f \tb[%d]=%f\n",i,a[i],i,b[i]);
//	}
//}

int
dddsp_calc_iir_chebyshev_float(float out_a[], float out_b[], int poles, float cutoff1, float cutoff2, float ripple, dddsp_filter_type_t type)
{
	int p,i;
	double a[poles+1], b[poles+1];

	memset(a,0,sizeof(double)*(poles+1));
	memset(b,0,sizeof(double)*(poles+1));
	a[0] = 1;
	b[0] = 1;

	// Calculate and combine all of the poles.
	if(type == DDDSP_BANDPASS) {
		double ta[poles+1+2], tb[poles+1+2];
		ta[0] = ta[1] = tb[0] = tb[1] = 0;

		for(p=1;p<=(poles+1)/4;p++) {
			double a_x[3], b_x[3], gain;

			_chebyshev_calc(a_x, b_x, (poles+1)/2, p, cutoff1, 0, ripple, DDDSP_HIGHPASS);
			b_x[0] = 0;
			gain = dddsp_calc_iir_gain_high_double(a_x, b_x, 2);
			dddsp_adjust_iir_gain_double(a_x, 2, 1.0/gain);

			memcpy(ta+2,a,sizeof(double)*(poles+1));
			memcpy(tb+2,b,sizeof(double)*(poles+1));

			for(i=0;i<poles+1;i++) {
				a[i] = a_x[0]*ta[i+2] + a_x[1]*ta[i+1] + a_x[2]*ta[i+0];
				b[i] =        tb[i+2] - b_x[1]*tb[i+1] - b_x[2]*tb[i+0];
			}

			_chebyshev_calc(a_x, b_x, (poles+1)/2, p, cutoff2, 0, ripple, DDDSP_LOWPASS);
			b_x[0] = 0;
			gain = dddsp_calc_iir_gain_low_double(a_x, b_x, 2);
			dddsp_adjust_iir_gain_double(a_x, 2, 1.0/gain);

			memcpy(ta+2,a,sizeof(double)*(poles+1));
			memcpy(tb+2,b,sizeof(double)*(poles+1));

			for(i=0;i<poles+1;i++) {
				a[i] = a_x[0]*ta[i+2] + a_x[1]*ta[i+1] + a_x[2]*ta[i+0];
				b[i] =        tb[i+2] - b_x[1]*tb[i+1] - b_x[2]*tb[i+0];
			}
		}
	} else {
		double ta[poles+1+2], tb[poles+1+2];
		ta[0] = ta[1] = tb[0] = tb[1] = 0;

		for(p=1;p<=poles/2;p++) {
			double a_x[3], b_x[3];

			_chebyshev_calc(a_x, b_x, poles, p, cutoff1, cutoff2, ripple, type);

			memcpy(ta+2,a,sizeof(double)*(poles+1));
			memcpy(tb+2,b,sizeof(double)*(poles+1));

			for(i=0;i<poles+1;i++) {
				a[i] = a_x[0]*ta[i+2] + a_x[1]*ta[i+1] + a_x[2]*ta[i+0];
				b[i] =        tb[i+2] - b_x[1]*tb[i+1] - b_x[2]*tb[i+0];
			}
		}
	}

	b[0] = 0.0;

	// Finish combining coefficients
	for(i=0;i<poles+1;i++) {
		b[i] = -b[i];
	}

//	_dump_iir_coeff_double(a, b, poles);

	// Normalize the gain on the coefficients.
	if(type==DDDSP_LOWPASS) {
		double gain;
		gain = dddsp_calc_iir_gain_low_double(a, b, poles);
//		fprintf(stderr,"lowpass filter gain: %e\n",gain);
		dddsp_adjust_iir_gain_double(a, poles, 1.0/gain);

//		gain = dddsp_calc_iir_gain_high_double(a, b, poles);
//		fprintf(stderr,"high-freq lowpass filter gain: %e\n",gain);
	} else if(type==DDDSP_HIGHPASS) {
		double gain;
		gain = dddsp_calc_iir_gain_high_double(a, b, poles);
//		fprintf(stderr,"highpass filter gain: %e\n",gain);
		dddsp_adjust_iir_gain_double(a, poles, 1.0/gain);

//		gain = dddsp_calc_iir_gain_low_double(a, b, poles);
//		fprintf(stderr,"low-freq highpass filter gain: %e\n",gain);
	}

	for(i=0;i<poles+1;i++) {
		out_a[i] = a[i];
		out_b[i] = b[i];
	}

//	_dump_iir_coeff_double(a, b, poles);
	return 0;
}

void
dddsp_filter_iir_fwd_float(
	float out_samples[],
	const float in_samples[],
	size_t count,
	const float a[],
	const float b[],
	int poles
) {
	if(count<poles)
		return;
	size_t step = 0;
	int i;
	for(;count;step++,count--,out_samples++,in_samples++) {
		*out_samples = 0;
		for(i=0;i<=poles;i++) {
			if(step<i)
				continue;
			*out_samples += a[i]*in_samples[-i];
			*out_samples += b[i]*out_samples[-i];
		}
	}
}

void
dddsp_filter_iir_rev_float(
	float out_samples[],
	const float in_samples[],
	size_t count,
	const float a[],
	const float b[],
	int poles
) {
	if(count<poles)
		return;
	size_t step = 0;
	int i;
	out_samples += count-1;
	in_samples += count-1;
	for(;count;step++,count--,out_samples--,in_samples--) {
		*out_samples = 0;
		for(i=0;i<=poles;i++) {
			if(step<i)
				continue;
			*out_samples += a[i]*in_samples[i];
			*out_samples += b[i]*out_samples[i];
		}
	}
}




struct dddsp_iir_float_s {
	int poles;
	int delay;
	float *a;
	float *b;
	float *x;
	float *y;
};

int
dddsp_iir_float_get_delay(dddsp_iir_float_t self)
{
	return self?self->delay:0;
}

void
dddsp_iir_float_reset(dddsp_iir_float_t self)
{
	memset(self->x,0,sizeof(float)*(self->poles+1));
	memset(self->y,0,sizeof(float)*(self->poles+1));
}

dddsp_iir_float_t
dddsp_iir_float_alloc(const float a[], const float b[], int poles, int delay)
{
	dddsp_iir_float_t self = NULL;

	if(poles<0)
		goto bail;

	self = calloc(sizeof(struct dddsp_iir_float_s),1);

	if (!self)
		goto bail;

	self->poles = poles;
	self->delay = delay;
	self->x = calloc(sizeof(float)*(poles+1),1);
	self->y = calloc(sizeof(float)*(poles+1),1);

	if (a != NULL) {
		self->a = calloc(sizeof(float)*(poles+1),1);
		memcpy(self->a, a, sizeof(float)*(poles+1));
	}
	if (b != NULL) {
		self->b = calloc(sizeof(float)*(poles+1),1);
		memcpy(self->b, b, sizeof(float)*(poles+1));
	}

bail:
	return self;
}

int
dddsp_calc_fir_window_float(float b[], int poles, float cutoff, dddsp_window_type_t window_type)
{
	int i;
	int taps = poles+1;
	float sum = 0;

	for (i = 0; i < taps; i++) {
		int n = (taps&1)?i-poles/2:i-taps/2;

		if (n != 0) {
			b[i] = sin(2.0 * M_PI * n * cutoff) / n;
		} else {
			b[i] = 2.0 * M_PI * cutoff;
		}

		switch (window_type) {
		case DDDSP_HANNING:
			// HaNNing window
			b[i] *= 0.5-0.5*cos((2*M_PI*i)/taps);
			break;
		case DDDSP_HAMMING:
			// Hamming window
			b[i] *= 0.54-0.46*cos((2*M_PI*i)/taps);
			break;
		case DDDSP_BLACKMAN:
			// Blackman window
			b[i] *= 0.42-0.5*cos((2*M_PI*i)/taps)+0.08*cos((4*M_PI*i)/taps);
			break;
		default:
		case DDDSP_RECTANGULAR:
			break;
		}

		sum += b[i];

		printf("[%02d] = %f\n",n,b[i]);
	}
	printf("SUM = %f\n",sum);

	for (i = 0; i < taps; i++) {
		b[i] /= sum;
	}

	return 0;
}

dddsp_iir_float_t dddsp_iir_float_alloc_delay(int delay)
{
	if (delay) {
		float a[delay+1];
		memset(a,0,sizeof(float)*(delay+1));
		a[delay] = 1.0;
		return dddsp_iir_float_alloc(a, NULL, delay, delay);
	}
	return NULL;
}


dddsp_iir_float_t
dddsp_fir_float_alloc_low_pass(float cutoff, int poles, dddsp_window_type_t window)
{
	float a[poles+1];
	dddsp_calc_fir_window_float(a, poles, cutoff, window);
	return dddsp_iir_float_alloc(a, NULL, poles, poles/2);
}

dddsp_iir_float_t
dddsp_fir_float_alloc_high_pass(float cutoff, int poles, dddsp_window_type_t window)
{
	float a[poles+1];
	dddsp_calc_fir_window_float(a, poles, cutoff, window);
	for (int i = 0; i < poles+1; i++) {
		a[i] = ((i==(poles+1)/2)?1.0:0.0) - a[i];
	}
	return dddsp_iir_float_alloc(a, NULL, poles, poles/2);
}

dddsp_iir_float_t
dddsp_fir_float_alloc_band_pass(float cutoffl, float cutoffh, int poles, dddsp_window_type_t window)
{
	float a[poles+1];
	float b[poles+1];
	dddsp_calc_fir_window_float(a, poles, cutoffl, window);
	dddsp_calc_fir_window_float(b, poles, cutoffh, window);
	for (int i = 0; i < poles+1; i++) {
		a[i] = b[i] - a[i];
	}
	return dddsp_iir_float_alloc(a, NULL, poles, poles/2);
}

dddsp_iir_float_t
dddsp_iir_float_alloc_low_pass(float cutoff, float ripple, int poles)
{
	float a[poles+1];
	float b[poles+1];
	dddsp_calc_iir_chebyshev_float(a, b, poles, cutoff, 0, ripple, DDDSP_LOWPASS);
	return dddsp_iir_float_alloc(a, b, poles, poles/2);
}

dddsp_iir_float_t
dddsp_iir_float_alloc_high_pass(float cutoff, float ripple, int poles)
{
	float a[poles+1];
	float b[poles+1];
	dddsp_calc_iir_chebyshev_float(a, b, poles, cutoff, 0, ripple, DDDSP_HIGHPASS);
	return dddsp_iir_float_alloc(a, b, poles, poles/2);
}

dddsp_iir_float_t
dddsp_iir_float_alloc_band_pass(float lower_cutoff, float upper_cutoff, float ripple, int order)
{
	int poles = order*2;
	float a[poles+1];
	float b[poles+1];

	// Calculate a low-pass filter
	dddsp_calc_iir_chebyshev_float(a, b, poles, lower_cutoff, upper_cutoff, ripple, DDDSP_BANDPASS);

//	_bandpass_calc_float(a, b, lower_cutoff, upper_cutoff);
//	poles = 2;

	return dddsp_iir_float_alloc(a, b, poles, poles/2);
}

void
dddsp_iir_float_finalize(dddsp_iir_float_t self)
{
	if(self) {
		free(self->a);
		free(self->b);
		free(self->x);
		free(self->y);
		free(self);
	}
}

float
dddsp_iir_float_feed(dddsp_iir_float_t self, float sample)
{
	if(self && isfinite(sample) && !isnan(sample)) {
		int i;
		memmove(self->x+1,self->x,sizeof(float)*(self->poles));
		memmove(self->y+1,self->y,sizeof(float)*(self->poles));
		self->x[0] = sample;
		self->y[0] = 0;
		for(i=0;i<=self->poles;i++)
			self->y[0] += self->x[i]*self->a[i];
		if (self->b) {
			for(i=1;i<=self->poles;i++)
				self->y[0] += self->y[i]*self->b[i];
		}
		sample = self->y[0];
		if(!isfinite(sample) || isnan(sample)) {
			self->y[0] = self->y[1];
		}
	}
	return sample;
}

void
dddsp_iir_float_block_fwd(dddsp_iir_float_t self,float out_samples[], const float in_samples[], size_t count)
{
	if(self) {
#if 1
		for(;count != 0;count--) {
			*out_samples++ = dddsp_iir_float_feed(self, *in_samples++);
		}
#else
		dddsp_filter_iir_fwd_float(out_samples, in_samples, count, self->a, self->b, self->poles);
#endif
	} else {
		memmove(out_samples,in_samples,sizeof(float)*count);
	}
}

void
dddsp_iir_float_block_rev(dddsp_iir_float_t self,float out_samples[], const float in_samples[], size_t count)
{
	if(self) {
#if 0
		out_samples += count-1;
		in_samples += count-1;
		for(;count != 0;count--) {
			*out_samples-- = dddsp_iir_float_feed(self, *in_samples--);
		}
#else
		dddsp_filter_iir_rev_float(out_samples, in_samples, count, self->a, self->b, self->poles);
#endif
	} else {
		memmove(out_samples,in_samples,sizeof(float)*count);
	}
}
