//
//  dddsp-filter.h
//  SSTV
//
//  Created by Robert Quattlebaum on 3/21/14.
//  Copyright (c) 2014 deepdarc. All rights reserved.
//

#ifndef SSTV_dddsp_filter_h
#define SSTV_dddsp_filter_h

#include <stdbool.h>
#include <stdint.h>
#include <math.h>

typedef enum {
	DDDSP_LOWPASS,
	DDDSP_HIGHPASS,
	DDDSP_BANDPASS,
	DDDSP_BANDSTOP
} dddsp_filter_type_t;

typedef enum {
	DDDSP_RECTANGULAR,
	DDDSP_HANNING,
	DDDSP_HAMMING,
	DDDSP_BLACKMAN
} dddsp_window_type_t;

#define DDDSP_FILTER_TYPE_IS_BAND(x)	((x)>=DDDSP_BANDPASS)

#define DDDSP_IIR_MAX_RIPPLE		(29)

int dddsp_calc_iir_chebyshev_float(float a[], float b[], int poles, float cutoff1, float cutoff2, float ripple, dddsp_filter_type_t type);
void dddsp_filter_iir_fwd_float(float out_samples[], const float in_samples[], size_t count, const float a[], const float b[], int poles);
void dddsp_filter_iir_rev_float(float out_samples[], const float in_samples[], size_t count, const float a[], const float b[], int poles);

int dddsp_calc_fir_window_float(float b[], int poles, float cutoff, dddsp_window_type_t window_type);


struct dddsp_iir_float_s;
typedef struct dddsp_iir_float_s *dddsp_iir_float_t;
void dddsp_iir_float_reset(dddsp_iir_float_t self);
dddsp_iir_float_t dddsp_iir_float_alloc(const float a[], const float b[], int poles, int delay);
dddsp_iir_float_t dddsp_iir_float_alloc_low_pass(float cutoff, float percent_ripple, int poles);
dddsp_iir_float_t dddsp_iir_float_alloc_high_pass(float cutoff, float percent_ripple, int poles);
dddsp_iir_float_t dddsp_iir_float_alloc_band_pass(float lower_cutoff, float upper_cutoff, float percent_ripple, int order);

dddsp_iir_float_t dddsp_iir_float_alloc_delay(int delay);

dddsp_iir_float_t dddsp_fir_float_alloc_low_pass(float cutoff, int poles);


float dddsp_iir_float_feed(dddsp_iir_float_t self, float sample);
int dddsp_iir_float_get_delay(dddsp_iir_float_t self);
void dddsp_iir_float_finalize(dddsp_iir_float_t self);
void dddsp_iir_float_block_fwd(dddsp_iir_float_t self,float out_samples[], const float in_samples[], size_t count);
void dddsp_iir_float_block_rev(dddsp_iir_float_t self,float out_samples[], const float in_samples[], size_t count);


#endif
