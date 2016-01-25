/*!	@file ddsstv-encoder.c
**	@author Robert Quattlebaum <darco@deepdarc.com>
**	@brief MIT License
**
**	Copyright (C) 2011-2013 Robert Quattlebaum
**
**	Permission is hereby granted, free of charge, to any person
**	obtaining a copy of this software and associated
**	documentation files (the "Software"), to deal in the
**	Software without restriction, including without limitation
**	the rights to use, copy, modify, merge, publish, distribute,
**	sublicense, and/or sell copies of the Software, and to
**	permit persons to whom the Software is furnished to do so,
**	subject to the following conditions:
**
**	The above copyright notice and this permission notice shall
**	be included in all copies or substantial portions of the
**	Software.
**
**	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
**	KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
**	WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
**	PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
**	OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
**	OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
**	OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
**	SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <stdio.h>
#include <SDL/SDL.h>
#include "ddsstv.h"

struct encoder_state_s {
	struct ddsstv_encoder_s encoder;
	struct dddsp_decimator_s decimator;

	uint8_t* samples;
	uint32_t sample_count;
	uint32_t sample_max;

	uint32_t sample_playback;
};

static size_t
colorbar_scanline_callback(
	uint8_t* context,
	int line,
	const struct ddsstv_mode_s* mode,
	ddsstv_channel_t channel,
	uint8_t* samples,
	size_t max_samples
) {
	if (max_samples < 32) {
		return 32;
	}

	if ((line*100/mode->height) < 25) {
		int i;
		if (channel == DDSSTV_CHANNEL_Cb || channel == DDSSTV_CHANNEL_Cr) {
			samples[0] = 127;
			return 1;
		}
		for (i=0x0;i<0xF;i++) {
			samples[i] = i*0x10 + i;
		}
		return 0xF;
	} else {
		switch (channel) {
		case DDSSTV_CHANNEL_Y:
			samples[0] = 16;
			samples[1] = 35;
			samples[2] = 65;
			samples[3] = 84;

			samples[4] = 112;
			samples[5] = 131;
			samples[6] = 162;
			samples[7] = 180;
			break;

		case DDSSTV_CHANNEL_Cb:
			samples[0] = 128;
			samples[1] = 212;
			samples[2] = 109;
			samples[3] = 193;

			samples[4] = 63;
			samples[5] = 147;
			samples[6] = 44;
			samples[7] = 128;
			break;

		case DDSSTV_CHANNEL_Cr:
			samples[0] = 128;
			samples[1] = 120;
			samples[2] = 212;
			samples[3] = 204;

			samples[4] = 52;
			samples[5] = 44;
			samples[6] = 136;
			samples[7] = 128;
			break;

		case DDSSTV_CHANNEL_RED:
			samples[0] = 0x00;
			samples[1] = 0x00;
			samples[2] = 0x00;
			samples[3] = 0x00;

			samples[4] = 0xff*3/4;
			samples[5] = 0xff*3/4;
			samples[6] = 0xff*3/4;
			samples[7] = 0xff*3/4;
			break;

		case DDSSTV_CHANNEL_GREEN:
			samples[0] = 0x00;
			samples[1] = 0x00;
			samples[2] = 0xff*3/4;
			samples[3] = 0xff*3/4;

			samples[4] = 0x00;
			samples[5] = 0x00;
			samples[6] = 0xff*3/4;
			samples[7] = 0xff*3/4;
			break;

		case DDSSTV_CHANNEL_BLUE:
			samples[0] = 0x00;
			samples[1] = 0xff*3/4;
			samples[2] = 0x00;
			samples[3] = 0xff*3/4;

			samples[4] = 0x00;
			samples[5] = 0xff*3/4;
			samples[6] = 0x00;
			samples[7] = 0xff*3/4;
			break;
		}

		return 8;
	}
	return 1;
}

void
modulator_output_callback(void* context, const float* samples, size_t count)
{
	struct encoder_state_s *state = context;

	while (count--) {
		if (state->sample_count >= state->sample_max) {
			return;
		}
		state->samples[state->sample_count++] = dddsp_decimator_int8_feed(&state->decimator, *samples++);
	}
}

void
sdl_audio_callback(void *context, Uint8 *stream, int len)
{
	struct encoder_state_s *state = context;

	if (state->sample_playback + len > state->sample_count) {
		len = state->sample_count - state->sample_playback;
	}

	if (len) {
		printf("fed %d thru %d\n",state->sample_playback, state->sample_playback+len);
		memcpy(stream, &state->samples[state->sample_playback], len);
		state->sample_playback += len;
	}
}



#undef main

int
main(int argc, char * argv[])
{
	struct encoder_state_s state;
	SDL_AudioSpec audio_spec;

	if ( SDL_Init(SDL_INIT_AUDIO) < 0 ) {
		fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	ddsstv_encoder_init(&state.encoder);
	dddsp_decimator_int8_init(&state.decimator, -1, 1);

	state.sample_max = state.encoder.modulator.multiplier*120; // 120 seconds of samples.
	state.sample_count = 0;
	state.samples = malloc(state.sample_max);
	state.sample_playback = 0;

	state.encoder.pull_scanline_cb = &colorbar_scanline_callback;
	state.encoder.modulator.output_func_context = &state;
	state.encoder.modulator.output_func = &modulator_output_callback;

//	ddsstv_mode_lookup_vis_code(&state.encoder.mode, kSSTVVISCodeBW8);
//	ddsstv_mode_lookup_vis_code(&state.encoder.mode, kSSTVVISCodeScotty1);
//	ddsstv_mode_lookup_vis_code(&state.encoder.mode, kSSTVVISCodeRobot24c);
	ddsstv_mode_lookup_vis_code(&state.encoder.mode, kSSTVVISCodeRobot12c);

	while(ddsstv_encoder_process(&state.encoder)) {
		printf("%d\n",state.encoder.scanline);
	}

	audio_spec.freq = state.encoder.modulator.multiplier;
	audio_spec.channels = 1;
	audio_spec.format = AUDIO_S8;
	audio_spec.silence = 0;
	audio_spec.samples = 4096;
	audio_spec.size = audio_spec.samples;
	audio_spec.userdata = &state;
	audio_spec.callback = &sdl_audio_callback;

	if (SDL_OpenAudio(&audio_spec, NULL) < 0) {
		fprintf(stderr, "Couldn't open audio device: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	SDL_PauseAudio(0);

	while(state.sample_count > state.sample_playback) {

		sleep(1);
	}
	sleep(1);

	SDL_PauseAudio(1);


	return 0;
}
