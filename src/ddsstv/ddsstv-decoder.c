//
//  ddsstv.c
//  SSTV
//
//  Created by Robert Quattlebaum on 10/27/13.
//  Copyright (c) 2013 deepdarc. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dddsp/dddsp.h>
#include "ddsstv.h"

#define UNDEFINED_SCORE			(DDDSP_UNCORRELATED)
#define BEST_SCORE			     (565556)

#define AUTO_ADJUST_SYNC_FREQ 1


void
ddsstv_decoder_init(ddsstv_decoder_t decoder, double ingest_sample_rate)
{
	memset(decoder,0,sizeof(*decoder));
	decoder->sync_freq = 1200;
	decoder->max_freq = 2300;
	decoder->asynchronous = true;
	ddsstv_mode_lookup_vis_code(&decoder->mode, 2);
	decoder->clear_on_restart = true;

	decoder->is_decoding = false;
	decoder->autosync_vis = true;
	decoder->autosync_vsync = true;
	decoder->autosync_hsync = true;
	decoder->header_best_score = UNDEFINED_SCORE;
//	decoder->preserve_luma_when_clipped = true;

#if 0
	decoder->scanline_duration_filter2 = dddsp_iir_float_alloc_low_pass(
		1.0/40.0, 0.0, 2
	);
	decoder->scanline_mix_factor = 2;
#else
	decoder->scanline_duration_filter2 = dddsp_iir_float_alloc_low_pass(
		1.0/30.0, 0.0, 2
	);
	decoder->scanline_mix_factor = 1;
#endif

	ddsstv_decoder_reset(decoder);
}

void
_ddsstv_did_finish_image(ddsstv_decoder_t decoder)
{
	if(decoder->image_finished_cb) {
		(*decoder->image_finished_cb)(decoder->context, decoder);
	}
	decoder->hsync_last = -DDSSTV_INTERNAL_SAMPLE_RATE*5;
	decoder->hsync_count = 0;
}

void
ddsstv_decoder_set_image_finished_callback(ddsstv_decoder_t decoder, void* context,void (*image_finished_cb)(void* context, ddsstv_decoder_t decoder))
{
	decoder->image_finished_cb = image_finished_cb;
	decoder->context = context;
}

int32_t
ddsstv_usec_to_index(int32_t usec)
{
	if(usec>0)
		return (int32_t)((int64_t)usec*DDSSTV_INTERNAL_SAMPLE_RATE/USEC_PER_SEC);
	return 0;
}

int32_t
ddsstv_index_to_usec(int32_t index)
{
	return (int32_t)((int64_t)index*USEC_PER_SEC/DDSSTV_INTERNAL_SAMPLE_RATE)
		+USEC_PER_SEC/DDSSTV_INTERNAL_SAMPLE_RATE/2
	;
}




void
ddsstv_decoder_truncate_to(ddsstv_decoder_t decoder, int32_t usec)
{
//	fprintf(stderr,"Truncating-upto freq buffer\n");

	size_t index = ddsstv_usec_to_index(usec);

	if(index>decoder->freq_buffer_size)
		index = decoder->freq_buffer_size;

	memmove(decoder->freq_buffer, decoder->freq_buffer+index, (decoder->freq_buffer_size-index)*sizeof(*decoder->freq_buffer));

	decoder->freq_buffer_size -= index;
	decoder->header_location -= index;
	decoder->hsync_last -= index;
	decoder->current_scanline_start -= usec;
	decoder->current_scanline_stop -= usec;
	decoder->current_scanline_postsync -= usec;
	if(decoder->current_scanline_start<0) decoder->current_scanline_start = 0;
	if(decoder->current_scanline_stop<0) decoder->current_scanline_stop = 0;
	if(decoder->current_scanline_postsync<0) decoder->current_scanline_postsync = 0;
}

void
ddsstv_decoder_truncate_into(ddsstv_decoder_t decoder, int32_t usec)
{
	ssize_t index = decoder->freq_buffer_size-ddsstv_usec_to_index(usec);
	if(index>0) {
//		fprintf(stderr,"Truncating-into freq buffer\n");

		memmove(decoder->freq_buffer, decoder->freq_buffer+index, (decoder->freq_buffer_size-index)*sizeof(*decoder->freq_buffer));

		decoder->freq_buffer_size -= index;
		decoder->header_location -= index;
		decoder->current_scanline_start -= usec;
		decoder->current_scanline_stop -= usec;
		decoder->current_scanline_postsync -= usec;
		if(decoder->current_scanline_start<0) decoder->current_scanline_start = 0;
		if(decoder->current_scanline_stop<0) decoder->current_scanline_stop = 0;
		if(decoder->current_scanline_postsync<0) decoder->current_scanline_postsync = 0;
	}
}


void
ddsstv_decoder_reset(ddsstv_decoder_t decoder)
{
	decoder->is_decoding = false;
	decoder->auto_started_decoding = false;

	decoder->freq_buffer_size = 0;
	decoder->header_best_score = UNDEFINED_SCORE;
	PT_INIT(&decoder->header_pt);
	PT_INIT(&decoder->image_pt);
}

void
ddsstv_decoder_start(ddsstv_decoder_t decoder)
{
	if(!decoder->is_decoding) {
		decoder->is_decoding = true;
		decoder->auto_started_decoding = false;
		decoder->header_offset = 0;
		decoder->header_best_score = UNDEFINED_SCORE;
		ddsstv_decoder_truncate_to(
			decoder,
			ddsstv_index_to_usec(decoder->header_location)
		);
		PT_INIT(&decoder->image_pt);
	}
	decoder->started_by = kDDSSTV_STARTED_BY_USER;
	decoder->auto_started_decoding = false;
}

void
ddsstv_decoder_stop(ddsstv_decoder_t decoder)
{
	decoder->is_decoding = false;
	decoder->auto_started_decoding = false;
	decoder->header_best_score = 0;
	decoder->has_image = false;
	PT_INIT(&decoder->image_pt);
}

void
ddsstv_decoder_recalc_image(ddsstv_decoder_t decoder)
{
	void (*image_finished_cb)(void* context, ddsstv_decoder_t decoder);
	ddsstv_started_by_t started_by = decoder->started_by;
	image_finished_cb = decoder->image_finished_cb;
	if(!decoder->is_decoding)
		decoder->image_finished_cb = NULL;
	PT_INIT(&decoder->image_pt);
	decoder->is_decoding = true;
	ddsstv_decoder_ingest_freqs(decoder, NULL, 0);
	decoder->image_finished_cb = image_finished_cb;
	decoder->started_by = started_by;
}

void
ddsstv_decoder_set_mode(ddsstv_decoder_t decoder, const struct ddsstv_mode_s* mode)
{
	decoder->mode = *mode;
	if(decoder->is_decoding)
		ddsstv_decoder_recalc_image(decoder);
}

int32_t
ddsstv_seek_x_freq_after(ddsstv_decoder_t decoder, int16_t freq, int32_t after, int16_t window, bool higher)
{
	size_t totalSamples = decoder->freq_buffer_size;
	const int16_t *freqPtr = decoder->freq_buffer;
	int32_t sum_acc = 0;
	int32_t sum_div = 0;
	uint32_t i;
	const int offset = -window/2;
	int32_t overshoot = 0;

	after = ddsstv_usec_to_index(after);

	if(after+totalSamples+window>totalSamples)
		totalSamples -= window*2;

	if(after+offset<0)
		after = -offset;

	if(after>decoder->freq_buffer_size)
		return (int32_t)decoder->freq_buffer_size;

	after++;
	freqPtr += after;

	for(i=0;i<window;i++) {
		if((freqPtr[i]>900) && (freqPtr[i]<2600)) {
			sum_acc += freqPtr[i];
			sum_div++;
		}
	}

	for(;after<totalSamples;after++,freqPtr++) {
		if((freqPtr[0]>900) && (freqPtr[0]<2600)) {
			sum_acc -= freqPtr[0];
			sum_div--;
		}
		if((freqPtr[window]>900) && (freqPtr[window]<2600)) {
			sum_acc += freqPtr[window];
			sum_div++;
		}
		if(sum_div<(window/4)+1)
			continue;
		if(higher) {
			if((sum_acc/sum_div)>freq) {
				break;
			}
		} else {
			if((sum_acc/sum_div)<freq) {
				break;
			}
		}
		overshoot = (sum_acc/sum_div);
	}

	if(after>totalSamples)
		after = (int32_t)totalSamples;

	if(after<offset)
		after = offset+1;

	after = ddsstv_index_to_usec(after);

	if(sum_div && ((sum_acc/sum_div)-overshoot))
		after += (int32_t)((float)(freq - overshoot)/((sum_acc/sum_div)-overshoot)*USEC_PER_SEC/DDSSTV_INTERNAL_SAMPLE_RATE);

	after += ddsstv_index_to_usec(window)/2;

	return after;
}

int32_t
ddsstv_seek_higher_freq_after(ddsstv_decoder_t decoder, int16_t freq, int32_t after, int16_t window)
{
	return ddsstv_seek_x_freq_after(decoder, freq, after, window, true);
}

int32_t
ddsstv_seek_lower_freq_after(ddsstv_decoder_t decoder, int16_t freq, int32_t after, int16_t window)
{
	return ddsstv_seek_x_freq_after(decoder, freq, after, window, false);
}

bool
_ddsstv_handle_vis(ddsstv_decoder_t decoder) {
	const int16_t sync_freq = decoder->sync_freq;
	const int16_t max_freq = decoder->max_freq;
//	const int16_t zero_freq = sync_freq+(max_freq-sync_freq)*3/11;
	const int16_t mid_freq = sync_freq+(max_freq-sync_freq)*7/11;
	int i;
	ddsstv_vis_code_t code = 0;
	int firstBit = 640*DDSSTV_INTERNAL_SAMPLE_RATE/USEC_PER_MSEC+decoder->header_location;
	int bitIncrement = 30*DDSSTV_INTERNAL_SAMPLE_RATE/USEC_PER_MSEC;
	int32_t calc_sync_freq = sync_freq;
	uint8_t set_bit_count=0;
	uint8_t worst_bit = 0, current_bit = 0;
	float worst_deviation = 0;
	int score = decoder->header_best_score;

	decoder->mode.sync_freq = sync_freq;
	decoder->mode.max_freq = max_freq;

#if AUTO_ADJUST_SYNC_FREQ
	int32_t calc_mid_freq;
	calc_sync_freq = dddsp_correlator_get_box_avg(decoder->vis_correlator,7);
	calc_mid_freq = (dddsp_correlator_get_box_avg(decoder->vis_correlator,1)+dddsp_correlator_get_box_avg(decoder->vis_correlator,5))/2;

	if(abs((calc_mid_freq-calc_sync_freq)-(mid_freq-sync_freq))>=30) {
//		fprintf(stderr,"Freq Adjust too large: %d (would have been %d)\n",abs((calc_mid_freq-calc_sync_freq)-(mid_freq-sync_freq)),calc_mid_freq - mid_freq);
		calc_sync_freq = sync_freq;
	} else
	{
		calc_sync_freq = calc_mid_freq - 700;
//		fprintf(stderr,"Freq Adjust: %d\n",calc_sync_freq-sync_freq);
	}
#else
	calc_sync_freq = sync_freq;
#endif
	for(i=firstBit;i<firstBit+bitIncrement*8;i+=bitIncrement,current_bit++) {
		float sum = 0;
		float deviation = 0;
		int j;
		for(j=0;j<bitIncrement;j++) {
			float value = decoder->freq_buffer[j+i];
			if(value>sync_freq+100)
				value = sync_freq+100;
			if(value<sync_freq-100)
				value = sync_freq-100;
			sum += value;
		}
		sum/=j;

		deviation=fabsf(fabsf(sum-calc_sync_freq)-100.0);
		if(deviation>worst_deviation) {
			worst_deviation = deviation;
			worst_bit = current_bit;
		}

		code = (code>>1);
		code |= (sum<calc_sync_freq)?0x80:0;
		if(sum<calc_sync_freq)set_bit_count++;
	}
	code &= 0x7F;
	if((set_bit_count&1)!=0) {
//						uint8_t bad_code = code;
		if(worst_bit!=7)
			code ^= (1<<worst_bit);
//						fprintf(stderr,"Q:%d VIS Parity Error! %d (%s), corrected to %d (%s), worst bit was %d\n",score,
//							bad_code,ddsstv_describe_vis_code(bad_code),
//							code,ddsstv_describe_vis_code(code),
//							worst_bit);
		score = score*4/5;
		decoder->vis_parity_error = true;
	} else if (code!=127){
		decoder->vis_parity_error = false;
//						fprintf(stderr,"Q:%d VIS Parity Good! %d (%s)\n",
//							score,code,ddsstv_describe_vis_code(code));

	}
	if(!ddsstv_vis_code_is_supported(code)) {
		score = score*3/5;
		decoder->mode.autosync_tol = 50;
#if AUTO_ADJUST_SYNC_FREQ
		decoder->mode.sync_freq = calc_sync_freq;
		decoder->mode.max_freq = decoder->mode.sync_freq + (2300-1200);
#endif
		decoder->mode.vis_code = kSSTVVISCode_Unknown;
	} else {
		//score = score*5/4;
		ddsstv_mode_lookup_vis_code(&decoder->mode, code);
#if AUTO_ADJUST_SYNC_FREQ
		decoder->mode.sync_freq = calc_sync_freq;
		decoder->mode.max_freq = decoder->mode.sync_freq + (2300-1200);
#endif
	}
	decoder->header_best_score = score;
	decoder->header_offset = 910*USEC_PER_MSEC;

	decoder->header_offset = ddsstv_seek_lower_freq_after(
		decoder,
		(mid_freq+sync_freq)/2,
		600*USEC_PER_MSEC,
		12
	) + 300*USEC_PER_MSEC;

	printf("AUTODETECTED VIS!!! header_offset=%d mode=%s\n",decoder->header_offset, ddsstv_describe_vis_code(decoder->mode.vis_code));
	printf(" *** OR mode=%s\n", ddsstv_describe_vis_code(code));

	return true;
}

bool
_ddsstv_handle_vsync(ddsstv_decoder_t decoder) {
	const int16_t sync_freq = decoder->mode.sync_freq;
	const int16_t max_freq = decoder->mode.max_freq;
	int score = decoder->header_best_score;

	decoder->vis_parity_error = false;
	score/=2;

	if(decoder->mode.vis_code != kSSTVVISCode_Unknown) {
		ddsstv_mode_lookup_vis_code(&decoder->mode, decoder->mode.vis_code);
	}

#if AUTO_ADJUST_SYNC_FREQ
	decoder->mode.sync_freq = sync_freq;
	decoder->mode.max_freq = max_freq;
#endif

	decoder->header_best_score = score;
	decoder->header_offset = 910000-6000;
	return true;
}

bool
_ddsstv_handle_hsync_score(ddsstv_decoder_t decoder, int32_t hsync_score) {
	// hsync detect.
	if(hsync_score>0) {
//		printf("HSYNC: header_location:%d hsync_score:%d\n",decoder->header_location,hsync_score);
		if(decoder->header_location - decoder->hsync_last > DDSSTV_INTERNAL_SAMPLE_RATE*2) {
			decoder->hsync_count = 0;
//			printf("HSYNC: Reset\n");
		} else if(decoder->header_location - decoder->hsync_last > DDSSTV_INTERNAL_SAMPLE_RATE/100) {
			if(decoder->hsync_count == 3) {
				decoder->hsync_len[0] = decoder->hsync_len[1];
				decoder->hsync_len[1] = decoder->hsync_len[2];
				decoder->hsync_count = 2;
			}
			decoder->hsync_len[decoder->hsync_count] = decoder->header_location - decoder->hsync_last;
			decoder->hsync_count++;
//			printf("HSYNC: Detected %d\n",decoder->header_location - decoder->hsync_last);
		} else if(decoder->hsync_count) {
			decoder->hsync_len[decoder->hsync_count-1] += decoder->header_location;
			decoder->hsync_len[decoder->hsync_count-1] -= decoder->hsync_last;
//			printf("HSYNC: Extended %d\n",decoder->hsync_len[decoder->hsync_count-1]);
		}
		decoder->hsync_last = decoder->header_location;
	}
	return true;
}

int32_t
_ddsstv_hsync_duration(ddsstv_decoder_t decoder) {
	int32_t ret = 0;

	if (decoder->hsync_count==3
		&& (abs(decoder->hsync_len[0]-decoder->hsync_len[1])<DDSSTV_INTERNAL_SAMPLE_RATE/100)
		&& (abs(decoder->hsync_len[1]-decoder->hsync_len[2])<DDSSTV_INTERNAL_SAMPLE_RATE/100)
		&& (abs(decoder->hsync_len[1]-decoder->hsync_len[0])<DDSSTV_INTERNAL_SAMPLE_RATE/100)
	) {
		ret = dddsp_median_int32(decoder->hsync_len[0], decoder->hsync_len[1], decoder->hsync_len[2]);
	}

	return ret;
}

bool
_ddsstv_check_hsync(ddsstv_decoder_t decoder) {
	int32_t median = _ddsstv_hsync_duration(decoder);
	ddsstv_vis_code_t prev_mode = decoder->mode.vis_code;

	if(ddsstv_mode_guess_vis_from_hsync(&decoder->mode,median*USEC_PER_SEC/DDSSTV_INTERNAL_SAMPLE_RATE)) {
		if(!decoder->is_decoding
			|| ((decoder->started_by>kDDSSTV_STARTED_BY_VIS) && (prev_mode != decoder->mode.vis_code)))
		{
			printf("AUTODETECTED HSYNC!!! hsync_len=%d mode=%s\n",median, ddsstv_describe_vis_code(decoder->mode.vis_code));
			// Don't lose any images we might have in progress...
			if(decoder->is_decoding
				&& (decoder->scanlines_in_sync>decoder->mode.height/8)
			) {
				decoder->scanlines_in_sync = 0;
				_ddsstv_did_finish_image(decoder);
			}

			decoder->header_offset = 0;
			decoder->started_by = kDDSSTV_STARTED_BY_HSYNC;
			decoder->auto_started_decoding = true;
			decoder->is_decoding = true;
			decoder->header_best_score = 0;
			PT_INIT(&decoder->image_pt);
			if(decoder->last_image_was_complete) {
				ddsstv_decoder_truncate_to(
					decoder,
					ddsstv_index_to_usec(decoder->header_location)
				);
			} else if(decoder->header_location>ddsstv_usec_to_index(decoder->mode.scanline_duration*decoder->mode.height)) {
				ddsstv_decoder_truncate_to(
					decoder,
					ddsstv_index_to_usec(decoder->header_location) - decoder->mode.scanline_duration*decoder->mode.height/2
				);
			}
			return true;
		}
	}
	return false;
}

PT_THREAD(header_decoder_protothread(ddsstv_decoder_t decoder))
{
	const int16_t sync_freq = decoder->sync_freq;
	const int16_t max_freq = decoder->max_freq;
	const int16_t zero_freq = sync_freq+(max_freq-sync_freq)*3/11;
	const int16_t mid_freq = sync_freq+(max_freq-sync_freq)*7/11;
	int32_t offset = DDSSTV_INTERNAL_SAMPLE_RATE*910/1000;
	bool autosync_vsync = decoder->autosync_vsync;

	if (decoder->current_scanline>(decoder->mode.height-8))
		autosync_vsync = false;

	PT_BEGIN(&decoder->header_pt);

	{
		int thresh = 75;
		thresh += 15;
		struct dddsp_correlator_box_s vis_boxes[] = {
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*000/1000, .expect = mid_freq, .threshold = thresh},
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE* 30/1000, .expect = mid_freq, .threshold = thresh},
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*270/1000, .expect = mid_freq, .threshold = thresh},
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*300/1000, .expect = sync_freq, .threshold = thresh*3},
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*310/1000, .expect = mid_freq, .threshold = thresh},
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*340/1000, .expect = mid_freq, .threshold = thresh},
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*580/1000, .expect = mid_freq, .threshold = thresh},
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*610/1000, .expect = sync_freq, .threshold = thresh},

			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*640/1000, .expect = sync_freq-100, .expect2 = sync_freq+100, .threshold = thresh, .type = DDDSP_BOX_DUAL },
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*670/1000, .expect = sync_freq-100, .expect2 = sync_freq+100, .threshold = thresh, .type = DDDSP_BOX_DUAL },
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*700/1000, .expect = sync_freq-100, .expect2 = sync_freq+100, .threshold = thresh, .type = DDDSP_BOX_DUAL },
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*730/1000, .expect = sync_freq-100, .expect2 = sync_freq+100, .threshold = thresh, .type = DDDSP_BOX_DUAL },
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*760/1000, .expect = sync_freq-100, .expect2 = sync_freq+100, .threshold = thresh, .type = DDDSP_BOX_DUAL },
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*790/1000, .expect = sync_freq-100, .expect2 = sync_freq+100, .threshold = thresh, .type = DDDSP_BOX_DUAL },
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*820/1000, .expect = sync_freq-100, .expect2 = sync_freq+100, .threshold = thresh, .type = DDDSP_BOX_DUAL },
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*850/1000, .expect = sync_freq-100, .expect2 = sync_freq+100, .threshold = thresh, .type = DDDSP_BOX_DUAL },

			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*880/1000, .expect = sync_freq, .threshold = thresh },
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*910/1000 },
		};
#if 0
		struct dddsp_correlator_box_s vis16_boxes[] = {
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*000/1000, .expect = mid_freq, .threshold = thresh},
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE* 30/1000, .expect = mid_freq, .threshold = thresh},
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*270/1000, .expect = mid_freq, .threshold = thresh},
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*300/1000, .expect = sync_freq, .threshold = thresh*3},
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*310/1000, .expect = mid_freq, .threshold = thresh},
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*340/1000, .expect = mid_freq, .threshold = thresh},
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*580/1000, .expect = mid_freq, .threshold = thresh},
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*610/1000, .expect = sync_freq, .threshold = thresh},

			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*640/1000, .expect = sync_freq-100, .threshold = thresh },
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*670/1000, .expect = sync_freq-100, .threshold = thresh },
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*700/1000, .expect = sync_freq+100, .threshold = thresh },
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*730/1000, .expect = sync_freq+100, .threshold = thresh },
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*760/1000, .expect = sync_freq+100, .threshold = thresh },
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*790/1000, .expect = sync_freq-100, .threshold = thresh },
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*820/1000, .expect = sync_freq+100, .threshold = thresh },
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*850/1000, .expect = sync_freq+100, .threshold = thresh },

/* high bits here */
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*640/1000, .expect = sync_freq-100, .expect2 = sync_freq+100, .threshold = thresh, .type = DDDSP_BOX_DUAL },
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*670/1000, .expect = sync_freq-100, .expect2 = sync_freq+100, .threshold = thresh, .type = DDDSP_BOX_DUAL },
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*700/1000, .expect = sync_freq-100, .expect2 = sync_freq+100, .threshold = thresh, .type = DDDSP_BOX_DUAL },
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*730/1000, .expect = sync_freq-100, .expect2 = sync_freq+100, .threshold = thresh, .type = DDDSP_BOX_DUAL },
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*760/1000, .expect = sync_freq-100, .expect2 = sync_freq+100, .threshold = thresh, .type = DDDSP_BOX_DUAL },
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*790/1000, .expect = sync_freq-100, .expect2 = sync_freq+100, .threshold = thresh, .type = DDDSP_BOX_DUAL },
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*820/1000, .expect = sync_freq-100, .expect2 = sync_freq+100, .threshold = thresh, .type = DDDSP_BOX_DUAL },
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*850/1000, .expect = sync_freq-100, .expect2 = sync_freq+100, .threshold = thresh, .type = DDDSP_BOX_DUAL },

			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*880/1000, .expect = sync_freq, .threshold = thresh },
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*910/1000 },
		};
#endif
		thresh = 62;
		struct dddsp_correlator_box_s vsync_boxes[] = {
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*000/1000, .expect = sync_freq, .threshold = thresh },
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE* 15/1000, .expect = sync_freq, .threshold = thresh },
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE* 30/1000, .expect = zero_freq, .expect2 = max_freq, .threshold = thresh, .type = DDDSP_BOX_BETWEEN },
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE* 33/1000, .expect = zero_freq, .expect2 = max_freq, .threshold = thresh, .type = DDDSP_BOX_BETWEEN },
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE* 36/1000 },
		};
		struct dddsp_correlator_box_s hsync_boxes[] = {
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE*000/1000, .expect = sync_freq, .threshold = thresh },
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE* 01/1000, .expect = zero_freq, .expect2 = max_freq, .threshold = thresh, .type = DDDSP_BOX_BETWEEN },
			{ .offset = DDSSTV_INTERNAL_SAMPLE_RATE* 02/1000 },
		};

		dddsp_correlator_finalize(decoder->vis_correlator);
		dddsp_correlator_finalize(decoder->vsync_correlator);
		dddsp_correlator_finalize(decoder->hsync_correlator);

		decoder->vis_correlator = dddsp_correlator_alloc(vis_boxes, sizeof(vis_boxes)/sizeof(*vis_boxes));
		decoder->vsync_correlator = dddsp_correlator_alloc(vsync_boxes, sizeof(vsync_boxes)/sizeof(*vsync_boxes));
		decoder->hsync_correlator = dddsp_correlator_alloc(hsync_boxes, sizeof(hsync_boxes)/sizeof(*hsync_boxes));

	}
	decoder->header_location = -offset;
	decoder->hsync_last = -DDSSTV_INTERNAL_SAMPLE_RATE*5;
	decoder->hsync_count = 0;
	decoder->header_best_score = UNDEFINED_SCORE;
	while(1) {
		if(!decoder->autosync_vis && !autosync_vsync && !decoder->autosync_hsync) {
			PT_WAIT_UNTIL(&decoder->header_pt, decoder->autosync_vis || autosync_vsync);
			PT_RESTART(&decoder->header_pt);
		}
		PT_WAIT_UNTIL(&decoder->header_pt, decoder->freq_buffer_size-decoder->header_location>DDSSTV_INTERNAL_SAMPLE_RATE*2);

		decoder->header_location++;

		int32_t vis_score = DDDSP_UNCORRELATED;
		int32_t vsync_score = DDDSP_UNCORRELATED;
		int32_t hsync_score = DDDSP_UNCORRELATED;

		if(decoder->header_location+offset>=0) {
			int32_t freq = decoder->freq_buffer[decoder->header_location+offset];
			if((freq<900) || (freq>2500)) {
				freq = DDDSP_UNCORRELATED;
			}

			vis_score = dddsp_correlator_feed(
				decoder->vis_correlator,
				freq
			);

			vsync_score = dddsp_correlator_feed(
				decoder->vsync_correlator,
				freq
			) / 4 + 1;

			hsync_score = dddsp_correlator_feed(
				decoder->hsync_correlator,
				freq
			);
		}

		_ddsstv_handle_hsync_score(decoder, hsync_score);

		if(!(!decoder->is_decoding || (decoder->current_scanline<16) || (decoder->scanlines_since_last_hsync>12))) {
			autosync_vsync = false;
		}

		if(!autosync_vsync) {
			vsync_score = DDDSP_UNCORRELATED;
		}

		if(!decoder->autosync_vis) {
			vis_score = DDDSP_UNCORRELATED;
		}

		int32_t score = vis_score;

		if (vsync_score > score) {
			score = vsync_score;
		}

		if((score>0) && (!decoder->is_decoding || (score > decoder->header_best_score))) {
#if 0
			printf("vis_score:%f(%d) vsync_score:%f(%d) header_best_score:%f(%d)\n",
				vis_score>0?log10(vis_score):0.0, vis_score,
				vsync_score>0?log10(vsync_score):0.0,vsync_score,
				decoder->header_best_score>0?log10(decoder->header_best_score):0.0,decoder->header_best_score
			);
#endif

			// Don't lose any images we might have in progress...
			if(decoder->is_decoding
				&& (decoder->scanlines_in_sync>decoder->mode.height/8)
			) {
				decoder->scanlines_in_sync = 0;
				_ddsstv_did_finish_image(decoder);
			}

			decoder->header_best_score = score;

			if(vis_score > vsync_score) {
				_ddsstv_handle_vis(decoder);
				decoder->started_by = kDDSSTV_STARTED_BY_VIS;
			} else {
				_ddsstv_handle_vsync(decoder);
				decoder->started_by = kDDSSTV_STARTED_BY_VSYNC;
			}

			PT_INIT(&decoder->image_pt);
			decoder->auto_started_decoding = true;
			decoder->is_decoding = true;
			ddsstv_decoder_truncate_to(
				decoder,
				ddsstv_index_to_usec(decoder->header_location)
			);
		} else if (decoder->autosync_hsync && (!decoder->is_decoding || (decoder->started_by>kDDSSTV_STARTED_BY_VIS))) {
			_ddsstv_check_hsync(decoder);
		}
	}
	PT_END(&decoder->header_pt);
}

int32_t _filter_scanline_duration(ddsstv_decoder_t decoder, int32_t calc_scanline_duration)
{
#if 1
	uint32_t tmp1;
	tmp1 =
		dddsp_median_int32(
			decoder->scanline_duration_filter[0],
			decoder->scanline_duration_filter[1],
			decoder->scanline_duration_filter[2]
		);
	uint32_t tmp2;
	decoder->scanline_duration_filter[decoder->scanline_duration_count++%3] = calc_scanline_duration;
	tmp2 =
		dddsp_median_int32(
			decoder->scanline_duration_filter[0],
			decoder->scanline_duration_filter[1],
			decoder->scanline_duration_filter[2]
		);
	calc_scanline_duration = tmp2;

	calc_scanline_duration = (tmp1+calc_scanline_duration)/2;
#endif
	decoder->scanlines_in_sync++;
	return dddsp_iir_float_feed(
		decoder->scanline_duration_filter2,
		calc_scanline_duration
	);
}

void _seek_next_scanline(ddsstv_decoder_t decoder, int32_t *scanline_start, int32_t *scanline_postsync, int32_t *scanline_stop)
{
	const int16_t sync_freq = decoder->mode.sync_freq;
	const int16_t max_freq = decoder->mode.max_freq;
	const int16_t zero_freq = sync_freq+(max_freq-sync_freq)*3/11;
	int box_size = ddsstv_usec_to_index(decoder->mode.front_porch_duration*3/4);
	int threshold = (sync_freq*3+zero_freq)/4;

/* A scanline is made of the following parts:
 *
 *  0. [PREV LINE] Back porch of `zero_freq` (optional)
 *  1. Sync pulse of `sync_freq`
 *  2. Front porch of `zero_freq` (optional)
 *  3. Scanline data between `zero_freq` and `max_freq`
 *  4. Back porch of `zero_freq` (optional)
 *  5. [NEXT LINE] ... Sync pulse of `sync_freq`
 *
 * To describe the current scanline, we use the following
 * values:
 *
 *  * `scanline_start` = transition between #0 and #1
 *  * `sync_duration` = duration of #1
 *  * `scanline_postsync` = transition between #1 and #2
 *  * `scanline_stop` = transition between #4 and #5
*/

	if(box_size<12)
		box_size = 12;

	*scanline_postsync = ddsstv_seek_higher_freq_after(
		decoder,
		threshold,
		*scanline_start + decoder->mode.sync_duration/2,
		box_size
	);

#if 0
	// Next three lines puts the middle of the sync
	// for the next line in *scanline_stop.
	*scanline_stop = ddsstv_seek_lower_freq_after(
		decoder,
		threshold,
		*scanline_postsync + box_size,
		box_size
	);
	*scanline_stop += ddsstv_seek_higher_freq_after(
		decoder,
		threshold,
		*scanline_stop + box_size,
		box_size
	);
	*scanline_stop /= 2;
	// Move *scanline_stop to before the sync.
	*scanline_stop -= decoder->mode.sync_duration/2;
#else
	*scanline_stop = ddsstv_seek_lower_freq_after(
		decoder,
		threshold,
		*scanline_postsync + box_size,
		box_size
	);
	*scanline_stop = ddsstv_seek_higher_freq_after(
		decoder,
		threshold,
		*scanline_stop + box_size,
		box_size
	);
	*scanline_stop -= decoder->mode.sync_duration;
#endif


	*scanline_stop -= decoder->mode.autosync_offset;

	*scanline_postsync = *scanline_start + decoder->mode.sync_duration;
}


PT_THREAD(image_decoder_protothread(ddsstv_decoder_t decoder))
{
	const int16_t sync_freq = decoder->mode.sync_freq;
	const int16_t max_freq = decoder->mode.max_freq;
	const int16_t zero_freq = sync_freq+(max_freq-sync_freq)*3/11;
	const int16_t mid_freq = sync_freq+(max_freq-sync_freq)*7/11;

	PT_BEGIN(&decoder->image_pt);

	decoder->current_scanline = 0;

	PT_WAIT_UNTIL(&decoder->image_pt, decoder->is_decoding);

	// Reset all image-related variables.
	decoder->current_scanline = 0;
	decoder->scanlines_in_sync = 0;
	decoder->has_image = true;
	decoder->last_image_was_complete = false;
//	decoder->scanline_duration_count = 1;
//	decoder->scanline_duration_sum = decoder->mode.scanline_duration*decoder->scanline_duration_count;
	decoder->current_scanline_start = decoder->current_scanline_stop = decoder->mode.start_offset + decoder->header_offset;
	decoder->scanline_duration_filter[0] =
	decoder->scanline_duration_filter[1] =
	decoder->scanline_duration_filter[2] = decoder->mode.scanline_duration;
	decoder->scanlines_since_last_hsync = 0;

	PT_YIELD(&decoder->image_pt);

	fprintf(stderr,"Starting \"%s\" decode (started by %d, score %d)\n", ddsstv_describe_vis_code(decoder->mode.vis_code), decoder->started_by, decoder->header_best_score);

	dddsp_iir_float_reset(decoder->scanline_duration_filter2);

	for(int i=0;i<200;i++) {
		dddsp_iir_float_feed(decoder->scanline_duration_filter2, decoder->mode.scanline_duration);
	}

	// (Re)alloc the image buffer
	if(!decoder->image_buffer) {
		decoder->image_buffer_size = decoder->mode.width*decoder->mode.height*sizeof(uint8_t)*3;
		if(decoder->image_buffer_size < 320*256*sizeof(uint8_t)*3)
			decoder->image_buffer_size = 320*256*sizeof(uint8_t)*3;
		decoder->image_buffer = calloc(
			1,
			decoder->image_buffer_size
		);
	} else if(decoder->image_buffer_size < decoder->mode.width*decoder->mode.height*sizeof(uint8_t)*3){
		decoder->image_buffer_size = decoder->mode.width*decoder->mode.height*sizeof(uint8_t)*3;
		decoder->image_buffer = realloc(
			decoder->image_buffer,
			decoder->image_buffer_size
		);
	}

	if(decoder->clear_on_restart)
		memset(decoder->image_buffer,0x7F,decoder->image_buffer_size);

#define DD_BUFFER_TO_SAMPLE(decoder, sample_index) \
	PT_WAIT_UNTIL(&(decoder)->image_pt, (decoder)->freq_buffer_size>=(sample_index))

#define DD_BUFFER_TO_TIME(decoder, time) DD_BUFFER_TO_SAMPLE(decoder, ((uint64_t)time)*DDSSTV_INTERNAL_SAMPLE_RATE/USEC_PER_SEC)


	// Start building the image.
	for(;
		decoder->is_decoding && decoder->current_scanline<decoder->mode.height;
		decoder->current_scanline++
	) {
		decoder->current_scanline_start = decoder->current_scanline_stop;
		decoder->current_scanline_postsync = decoder->current_scanline_start + decoder->mode.sync_duration;
		decoder->current_scanline_stop = decoder->current_scanline_start + decoder->mode.scanline_duration;
		if(decoder->asynchronous && decoder->mode.sync_duration>48) {
			DD_BUFFER_TO_TIME(
				decoder,
				decoder->current_scanline_stop
					+ decoder->mode.scanline_duration*MIN(
						MIN(MAX(3,decoder->scanlines_since_last_hsync),15),
						decoder->mode.height-decoder->current_scanline
					)
				);

			uint32_t calculated_scanline_duration = 0;
			int64_t scan_track_tol;

			_seek_next_scanline(
				decoder,
				&decoder->current_scanline_start,
				&decoder->current_scanline_postsync,
				&decoder->current_scanline_stop
			);

			if(decoder->current_scanline>2) {
				scan_track_tol = decoder->scanlines_since_last_hsync;
				if(scan_track_tol>10)
					scan_track_tol = 10;
				scan_track_tol = decoder->mode.autosync_tol*(6-scan_track_tol)/8;
				scan_track_tol += decoder->mode.autosync_tol;
			} else {
				scan_track_tol = decoder->mode.autosync_tol;
			}
			calculated_scanline_duration = decoder->current_scanline_stop-decoder->current_scanline_start;
//			printf("%d: len: %ld expect: %ld upper: %ld lower: %ld tol:%ld track-tol:%d\n",(int)decoder->current_scanline, decoder->current_scanline_stop-decoder->current_scanline_start, decoder->mode.scanline_duration, (int64_t)decoder->mode.scanline_duration*(scan_track_tol-1)/scan_track_tol, (int64_t)decoder->mode.scanline_duration*(scan_track_tol+1)/scan_track_tol, (int64_t)decoder->mode.scanline_duration*(scan_track_tol+1)/scan_track_tol-(int64_t)decoder->mode.scanline_duration*(scan_track_tol-1)/scan_track_tol, scan_track_tol);

#define EQUAL_WITHIN_TOLERANCE(lhs, rhs, tol)	(\
	((lhs)<(int64_t)(rhs)*((tol)-1)/(tol)) \
	|| ((lhs)>(int64_t)(rhs)*((tol)+1)/(tol)) \
	)
			if(EQUAL_WITHIN_TOLERANCE(calculated_scanline_duration,decoder->mode.scanline_duration,scan_track_tol))
			{
				uint32_t next_calculated_scanline_duration;
				int32_t next_start = decoder->current_scanline_stop;
				int32_t next_post_sync, next_stop;

				_seek_next_scanline(
					decoder,
					&next_start,
					&next_post_sync,
					&next_stop
				);

				next_calculated_scanline_duration = next_stop - next_start;
//				printf("\tnext?: len: %ld\n", next_stop-next_start, decoder->mode.scanline_duration);

				if(EQUAL_WITHIN_TOLERANCE(next_calculated_scanline_duration,decoder->mode.scanline_duration,scan_track_tol))
				{
					printf("%d: lost sync, \n",(int)decoder->current_scanline);


					if(decoder->mode.vis_code == kSSTVVISCode_Unknown
						&& (abs(next_calculated_scanline_duration-calculated_scanline_duration)<400*USEC_PER_MSEC)
						&& ((decoder->scanlines_since_last_hsync >= 32) || (decoder->current_scanline<32))
					) {
						if(ddsstv_mode_guess_vis_from_hsync(&decoder->mode,next_calculated_scanline_duration)) {
							printf("HIT: %d %s\n", decoder->mode.vis_code, ddsstv_describe_vis_code(decoder->mode.vis_code));
//							decoder->mode.vis_code = kSSTVVISCode_Unknown;
							decoder->started_by = kDDSSTV_STARTED_BY_HSYNC;
							if ((decoder->current_scanline > 56)) {
								_ddsstv_did_finish_image(decoder);
							}
							PT_RESTART(&decoder->image_pt);
						}
					}
					if(decoder->auto_started_decoding) {
						// If this wasn't a manually started image,
						// then investigate if we should give up.
						if((decoder->scanlines_since_last_hsync >= 40)) {
							printf("**** %d %d %d\n",abs(next_calculated_scanline_duration-calculated_scanline_duration),next_calculated_scanline_duration,calculated_scanline_duration);
							if((abs(next_calculated_scanline_duration-calculated_scanline_duration)<400*USEC_PER_MSEC) && ddsstv_mode_guess_vis_from_hsync(&decoder->mode,next_calculated_scanline_duration)
							) {
								printf("HIT2: %d %s\n", decoder->mode.vis_code, ddsstv_describe_vis_code(decoder->mode.vis_code));
	//							decoder->mode.vis_code = kSSTVVISCode_Unknown;
								decoder->started_by = kDDSSTV_STARTED_BY_HSYNC;
								if ((decoder->current_scanline > 56)) {
									_ddsstv_did_finish_image(decoder);
									ddsstv_decoder_truncate_to(decoder, decoder->current_scanline_start);
									decoder->header_offset = 0;
								}

								PT_RESTART(&decoder->image_pt);
							} else if ((decoder->current_scanline <= 56)) {
								printf("No sync detected by line 56, aborting image.\n");
								decoder->is_decoding = false;
								decoder->auto_started_decoding = false;
								decoder->header_best_score = 0;
								decoder->has_image = false;
								PT_RESTART(&decoder->image_pt);
							}
						}
					}


					decoder->current_scanline_stop = decoder->current_scanline_start + decoder->mode.scanline_duration;
					decoder->current_scanline_postsync = decoder->current_scanline_start + decoder->mode.sync_duration;
					decoder->scanlines_since_last_hsync++;
					if(decoder->header_best_score != UNDEFINED_SCORE)
						decoder->header_best_score *= 0.95;
				} else {
					printf("%d: partial lost sync, \n",(int)decoder->current_scanline);

					// Update scanline statistics
					if(++decoder->scanlines_since_last_hsync>6)
						decoder->scanlines_since_last_hsync = 6;

					decoder->mode.scanline_duration = next_stop-next_start;

					// Filter and update scanline duration
					next_calculated_scanline_duration = _filter_scanline_duration(decoder,next_calculated_scanline_duration);
					decoder->mode.scanline_duration = _filter_scanline_duration(decoder,decoder->mode.scanline_duration);

					decoder->current_scanline_start = (int32_t)(next_start - next_calculated_scanline_duration*lroundf((float)(next_start-decoder->current_scanline_start)/next_calculated_scanline_duration));
					decoder->current_scanline_postsync = decoder->current_scanline_start + decoder->mode.sync_duration;
					decoder->current_scanline_stop = decoder->current_scanline_start + next_calculated_scanline_duration;
				}
			} else {
				// Update scanline statistics
				decoder->scanlines_since_last_hsync = 0;
				if(decoder->scanline_duration_count>16) {
					if(decoder->header_best_score > -100)
						decoder->header_best_score = -100;
					else if (decoder->header_best_score>BEST_SCORE)
						decoder->header_best_score *= 1.005;
				}

				// Quick Sync recovery
				if(decoder->current_scanline-decoder->last_good_scanline>3) {
					int32_t tmp = (decoder->current_scanline_stop-decoder->last_good_scanline_stop)
					             /(decoder->current_scanline-decoder->last_good_scanline);
					if(EQUAL_WITHIN_TOLERANCE(tmp,decoder->mode.scanline_duration,scan_track_tol/5+1)) {
						printf("QuickSync!\n");
						for (int i=0;i<(decoder->current_scanline-decoder->last_good_scanline);i++) {
							_filter_scanline_duration(decoder, tmp);
						}
					}
				}
				decoder->last_good_scanline_stop = decoder->current_scanline_stop;
				decoder->last_good_scanline = decoder->current_scanline;

				// Filter and update scanline duration
				decoder->mode.scanline_duration = _filter_scanline_duration(decoder, decoder->current_scanline_stop-decoder->current_scanline_start);

				if(decoder->current_scanline>8) {
					decoder->current_scanline_stop += (decoder->current_scanline_start + decoder->mode.scanline_duration)*decoder->scanline_mix_factor;
					decoder->current_scanline_stop /= decoder->scanline_mix_factor+1;
				}
			}
		} else {
			decoder->scanlines_in_sync++;
			DD_BUFFER_TO_TIME(decoder, decoder->current_scanline_stop+ decoder->mode.scanline_duration);
		}

		{	// Draw the scanline.
			int i;
			uint8_t* scanline_start = decoder->image_buffer+decoder->current_scanline*decoder->mode.width*3;
			float c_start[3], c_stop[3];
			int16_t c_line[3][decoder->mode.width];

			if(decoder->mode.color_mode == kDDSSTV_COLOR_MODE_RGB) {
				/* RGB Color */
				float channel_width = (decoder->current_scanline_stop-decoder->current_scanline_postsync)/3.000;
				c_start[0] = decoder->current_scanline_postsync;
				c_stop[0] = c_start[0] + channel_width;
				c_start[1] = c_stop[0];
				c_stop[1] = c_start[1] + channel_width;
				c_start[2] = c_stop[1];
				c_stop[2] = c_start[2] + channel_width;

				// Scotty modes are weird.
				if(decoder->mode.scotty_hack) {
					c_start[1] -= (decoder->current_scanline_stop-decoder->current_scanline_start);
					c_stop[1] -= (decoder->current_scanline_stop-decoder->current_scanline_start);
					c_start[2] -= (decoder->current_scanline_stop-decoder->current_scanline_start);
					c_stop[2] -= (decoder->current_scanline_stop-decoder->current_scanline_start);
				}
			} else if(DDSSTV_COLOR_MODE_IS_YCBCR_422(decoder->mode.color_mode)) {
				/* YCbCr 4:2:2 Color */
				c_start[0] = decoder->current_scanline_postsync;
				c_stop[0] = (decoder->current_scanline_start+decoder->current_scanline_stop)/2.0;
				c_start[1] = c_stop[0] + (c_start[0]-decoder->current_scanline_start)/2.0;
				c_stop[1] = c_start[1] + (c_stop[0]-c_start[0])/2.0;
				c_start[2] = c_stop[1] + (c_start[0]-decoder->current_scanline_start)/2.0;
				c_stop[2] = decoder->current_scanline_stop;

				// Compensate for the later porch calculations
				c_start[1] -= decoder->mode.front_porch_duration/2.0;
				c_stop[1] += decoder->mode.back_porch_duration/2.0;
				c_start[2] -= decoder->mode.front_porch_duration/2.0;
				c_stop[2] += decoder->mode.back_porch_duration/2.0;
			} else if(DDSSTV_COLOR_MODE_IS_YCBCR_420(decoder->mode.color_mode)) {
				/* YCbCr 4:2:0 Color */
				c_start[0] = decoder->current_scanline_postsync;
				c_stop[0] = decoder->current_scanline_start+(decoder->current_scanline_stop-decoder->current_scanline_start)*2.0/3.0;

				c_start[1] = c_stop[0] +(c_start[0]-decoder->current_scanline_start)/2.0;
				c_stop[1] = decoder->current_scanline_stop;
				c_start[2] = c_stop[0] +(c_start[0]-decoder->current_scanline_start)/2.0;
				c_stop[2] = decoder->current_scanline_stop;

				if(decoder->current_scanline&1) {
					c_start[1] -= decoder->mode.scanline_duration;
					c_stop[1] -= decoder->mode.scanline_duration;
				} else if(decoder->current_scanline+1<decoder->mode.height) {
					c_start[2] += decoder->mode.scanline_duration;
					c_stop[2] += decoder->mode.scanline_duration;
				}

				// Compensate for the later porch calculations
				c_start[1] -= decoder->mode.front_porch_duration/2.0;
				c_stop[1] += decoder->mode.back_porch_duration/2.0;
				c_start[2] -= decoder->mode.front_porch_duration/2.0;
				c_stop[2] += decoder->mode.back_porch_duration/2.0;
			} else {
				/* Vanilla Grayscale */
				c_start[2] = c_start[1] = c_start[0] = decoder->current_scanline_postsync;
				c_stop[2] = c_stop[1] = c_stop[0] = decoder->current_scanline_stop;
			}

			// Remove the porch.
			c_start[0] += decoder->mode.front_porch_duration;
			c_stop[0] -= decoder->mode.back_porch_duration;
			c_start[1] += decoder->mode.front_porch_duration;
			c_stop[1] -= decoder->mode.back_porch_duration;
			c_start[2] += decoder->mode.front_porch_duration;
			c_stop[2] -= decoder->mode.back_porch_duration;

			for(i=0;i<3;i++) {
				float start_offset = (float)((double)c_start[decoder->mode.channel_order[i]]*DDSSTV_INTERNAL_SAMPLE_RATE/USEC_PER_SEC);
				float stop_offset = (float)((double)c_stop[decoder->mode.channel_order[i]]*DDSSTV_INTERNAL_SAMPLE_RATE/USEC_PER_SEC);

				if(start_offset>=decoder->freq_buffer_size) start_offset = decoder->freq_buffer_size-1;
				if(stop_offset>=decoder->freq_buffer_size) stop_offset = decoder->freq_buffer_size-1;

				if (stop_offset>start_offset) {
					dddsp_resample_int16(
						c_line[i],
						decoder->mode.width,
						decoder->freq_buffer,
						start_offset,
						stop_offset
					);
				}
			}

			int16_t neutral_freq[3] = { mid_freq, mid_freq, mid_freq };
			for(i=0; i < decoder->mode.width; i++) {
				int16_t c_freq[3] = {
					c_line[0][i],
					c_line[1][i],
					c_line[2][i]
				};

				// If the value is way out of range then make it something neutral.
				if((c_freq[0]>max_freq*1.2) || (c_freq[0]<zero_freq/1.2)) c_freq[0] = neutral_freq[0];
				if((c_freq[1]>max_freq*1.2) || (c_freq[1]<zero_freq/1.2)) c_freq[1] = neutral_freq[1];
				if((c_freq[2]>max_freq*1.2) || (c_freq[2]<zero_freq/1.2)) c_freq[2] = neutral_freq[2];

				c_freq[0] -= zero_freq;
				c_freq[1] -= zero_freq;
				c_freq[2] -= zero_freq;

				// If we need to, do a Y'CbCr conversion.
				if (DDSSTV_COLOR_MODE_IS_YCBCR(decoder->mode.color_mode)){

					if (0) {
						// Full-range to Video-range conversion
						c_freq[1] -= mid_freq-zero_freq;
						c_freq[2] -= mid_freq-zero_freq;
						c_freq[0] = c_freq[0]*255.0/240.0 + 16*(max_freq-zero_freq)/255.0;
						c_freq[1] = c_freq[1]*255.0/240.0 + 128*(max_freq-zero_freq)/255.0;
						c_freq[2] = c_freq[2]*255.0/240.0 + 128*(max_freq-zero_freq)/255.0;
					}

					c_freq[0] -= 16*(max_freq-zero_freq)/255.0;
					c_freq[1] -= 128*(max_freq-zero_freq)/255.0;
					c_freq[2] -= 128*(max_freq-zero_freq)/255.0;

					if (DDSSTV_COLOR_MODE_IS_NTSC(decoder->mode.color_mode)) {
						// Rec.601 YCbCr to sRGB conversion
						// TODO: Should also do sRGB conversion...?
						float r, g, b;
						r = 1.164*c_freq[0] + 1.596*c_freq[2];
						g = 1.164*c_freq[0] - 0.392*c_freq[1] - 0.813*c_freq[2];
						b = 1.164*c_freq[0] + 2.017*c_freq[1];

						c_freq[0] = r;
						c_freq[1] = g;
						c_freq[2] = b;
					} else {
						// Rec.709 YCbCr to sRGB conversion
						float r, g, b;
						r = 1.164*c_freq[0] + 1.793*c_freq[2];
						g = 1.164*c_freq[0] - 0.213*c_freq[1] - 0.533*c_freq[2];
						b = 1.164*c_freq[0] + 2.112*c_freq[1];

						c_freq[0] = r;
						c_freq[1] = g;
						c_freq[2] = b;
					}

					int y = c_freq[1]*0.614+c_freq[0]*0.183+c_freq[2]*0.062;

					// Clamp.
					if(c_freq[0]>(max_freq-zero_freq)) c_freq[0] = (max_freq-zero_freq);
					if(c_freq[0]<0) c_freq[0] = 0;
					if(c_freq[1]>(max_freq-zero_freq)) c_freq[1] = (max_freq-zero_freq);
					if(c_freq[1]<0) c_freq[1] = 0;
					if(c_freq[2]>(max_freq-zero_freq)) c_freq[2] = (max_freq-zero_freq);
					if(c_freq[2]<0) c_freq[2] = 0;

					if(decoder->preserve_luma_when_clipped) {
						float adj = ((float)(c_freq[1]*0.614+c_freq[0]*0.183+c_freq[2]*0.062))/y;
						c_freq[0] /= adj;
						c_freq[1] /= adj;
						c_freq[2] /= adj;
					}
				}

				// Scale to fit in 8 bits.
				c_freq[0] = (c_freq[0])*255/(max_freq-zero_freq);
				c_freq[1] = (c_freq[1])*255/(max_freq-zero_freq);
				c_freq[2] = (c_freq[2])*255/(max_freq-zero_freq);

				// Clamp.
				if(c_freq[0]>255) c_freq[0] = 255;
				if(c_freq[0]<0) c_freq[0] = 0;
				if(c_freq[1]>255) c_freq[1] = 255;
				if(c_freq[1]<0) c_freq[1] = 0;
				if(c_freq[2]>255) c_freq[2] = 255;
				if(c_freq[2]<0) c_freq[2] = 0;

				*scanline_start++ = c_freq[0];
				*scanline_start++ = c_freq[1];
				*scanline_start++ = c_freq[2];
			}
		}
	}

	fprintf(stderr,"Image decode finished! %s %d\n",ddsstv_describe_vis_code(decoder->mode.vis_code), decoder->scanlines_since_last_hsync);

	decoder->last_image_was_complete = (decoder->current_scanline==decoder->mode.height)
		&& (decoder->scanlines_since_last_hsync<decoder->mode.height/8)
		&& (decoder->scanlines_in_sync>decoder->mode.height*1/3);

	if(decoder->last_image_was_complete) {
		decoder->header_location = ddsstv_usec_to_index(decoder->current_scanline_stop);
	}

	decoder->is_decoding = false;
	decoder->auto_started_decoding = false;
	decoder->scanlines_in_sync = 0;
	_ddsstv_did_finish_image(decoder);
	decoder->current_scanline = 0;

	PT_END(&decoder->image_pt);
}


bool
ddsstv_decoder_ingest_freqs(ddsstv_decoder_t decoder, const int16_t* freqs, size_t count)
{
	bool ret = false;

	if(decoder->continuous && !decoder->is_decoding) {
		decoder->is_decoding = true;
		decoder->auto_started_decoding = true;

		decoder->header_best_score = UNDEFINED_SCORE;
		PT_INIT(&decoder->image_pt);
	}

//	fprintf(stderr,"Feeding %d freqs %d\n",(int)count, count?freqs[0]:0);

	if(count) {
		decoder->freq_buffer = realloc(
			decoder->freq_buffer,
			(decoder->freq_buffer_size+count)*sizeof(int16_t)
		);
		if(!decoder->freq_buffer) {
			fprintf(stderr,"Malloc failure!\n");
			return false;
		}
		memcpy(
			decoder->freq_buffer+decoder->freq_buffer_size,
			freqs,
			count*sizeof(int16_t)
		);
		decoder->freq_buffer_size += count;
	}

	if(decoder->freq_buffer_size) {
		PT_SCHEDULE(header_decoder_protothread(decoder));
		bool was_decoding = decoder->is_decoding;

		ret = !PT_SCHEDULE(image_decoder_protothread(decoder));

		if(ret || (was_decoding && !decoder->is_decoding && decoder->header_best_score!=0))
			ret = true;

		if(decoder->is_decoding || ret) {
		} else if(count && (decoder->freq_buffer_size>DDSSTV_INTERNAL_SAMPLE_RATE*4)) {
			if(!decoder->has_image
				|| (decoder->freq_buffer_size>DDSSTV_MAX_FREQ_BUFFER_SIZE-DDSSTV_INTERNAL_SAMPLE_RATE)) {
				decoder->has_image = false;
				ddsstv_decoder_truncate_into(decoder,USEC_PER_MSEC*2);
			}
		}
	}

	return ret;
}

#ifdef __APPLE__
CGImageRef
ddsstv_decoder_copy_image(ddsstv_decoder_t decoder)
{
	CGImageRef image = NULL;
	CGColorSpaceRef colorSpace = NULL;
	CGBitmapInfo bitmapInfo = 0;
	int channels = 0;
	CGDataProviderRef dataProvider = NULL;
	CFDataRef data = NULL;
	if(!decoder->image_buffer)
		goto bail;

#if 0
	data = CFDataCreateWithBytesNoCopy(NULL, decoder->image_buffer, decoder->image_buffer_size, kCFAllocatorNull);
#else
	data = CFDataCreate(NULL, decoder->image_buffer, decoder->image_buffer_size);
#endif

	if(!data)
		goto bail;

	dataProvider = CGDataProviderCreateWithCFData(data);

	if(!dataProvider)
		goto bail;

	switch (decoder->mode.color_mode) {
#if DDSSTV_SINGLE_CHANNEL_GRAYSCALE
	case kDDSSTV_COLOR_MODE_GRAYSCALE:
		colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceGenericGrayGamma2_2);
		channels = 1;
		break;
#endif

	case kDDSSTV_COLOR_MODE_YCBCR_420_601:
	case kDDSSTV_COLOR_MODE_YCBCR_422_601:
		// Robot uses Rec.601 primaries and transfer function.
		colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
		channels = 3;
		break;

	default:	// Default to sRGB colorspace.
	case kDDSSTV_COLOR_MODE_YCBCR_420_709:
	case kDDSSTV_COLOR_MODE_YCBCR_422_709:
	case kDDSSTV_COLOR_MODE_RGB:
		colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
		channels = 3;
		break;
	}

	if(!colorSpace)
		goto bail;

	image = CGImageCreate(
		decoder->mode.width,
		decoder->mode.height,
		8,
		8*channels,
		decoder->mode.width*channels,
		colorSpace,
		bitmapInfo,
		dataProvider,
		NULL,
		true,
		kCGRenderingIntentDefault
	);

bail:
	if(data)
		CFRelease(data);
	if(colorSpace)
		CFRelease(colorSpace);
	if(dataProvider)
		CFRelease(dataProvider);
	return image;
}
#endif // defined(__APPLE__)
