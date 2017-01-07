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

#include "config.h"

#include <stdio.h>

#include <SDL/SDL.h>
#include <SDL_image.h>

#include <getopt.h>

#include "ddsstv/ddsstv.h"

// Ignores return value from function 's'
#define IGNORE_RETURN_VALUE(s)  do { if (s){} } while (0)

// SDL for some reason redefines main. Undo that.
#undef main

#define EXIT_QUIT                       65535

int sRet = 0;
static sig_t sPreviousHandlerForSIGINT;

static void signal_SIGINT(int sig)
{
    static const char message[] = "\nCaught SIGINT!\n";

    sRet = EXIT_QUIT;

    // Can't use syslog() because it isn't async signal safe.
    // So we write to stderr
    IGNORE_RETURN_VALUE(write(STDERR_FILENO, message, sizeof(message)-1));

    // Restore the previous handler so that if we end up getting
    // this signal again we peform the system default action.
    signal(SIGINT, sPreviousHandlerForSIGINT);
    sPreviousHandlerForSIGINT = NULL;

    (void)sig;
}


struct encoder_state_s {
	struct ddsstv_encoder_s encoder;
	struct dddsp_decimator_s decimator;

	uint8_t* samples;
	uint32_t sample_count;
	uint32_t sample_max;

	uint32_t sample_playback;

	bool use_mu_law;
};

// Test scanline callback
static size_t
colorbar_scanline_callback(
	void* context,
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


static size_t
sdl_image_scanline_callback(
	void* context,
	int line,
	const struct ddsstv_mode_s* mode,
	ddsstv_channel_t channel,
	uint8_t* samples,
	size_t max_samples
) {
	SDL_Surface *surface = context;
	const int bpp = surface->format->BytesPerPixel;
	size_t ret = surface->w;
	int redShift, greenShift, blueShift;
	float y_conv[] = {16, 65.481, 128.553, 24.966};
	float cb_conv[] = {128, -37.797, -74.203, 112};
	float cr_conv[] = {128, 112, -93.786, -18.214};

	// This just does stupid nearest-neighbor for the vertical axis.
	// Would be better to interpolate.
	line = line * surface->h / mode->height;

	if (line > surface->h) {
		return 0;
	}

	redShift = surface->format->Rshift/8;
	greenShift = surface->format->Gshift/8;
	blueShift = surface->format->Bshift/8;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	redShift = bpp - redShift;
	greenShift = bpp - greenShift;
	blueShift = bpp - blueShift;
#endif

	if (ret <= max_samples) {
		SDL_LockSurface(surface);
		int i;
		uint8_t* p = (uint8_t*)surface->pixels + line * surface->pitch;

		if (bpp>1) {
			if (channel >= DDSSTV_CHANNEL_RED && channel <= DDSSTV_CHANNEL_BLUE) {
				switch (channel) {
				default:
				case DDSSTV_CHANNEL_GREEN: p += greenShift; break;
				case DDSSTV_CHANNEL_RED: p += redShift; break;
				case DDSSTV_CHANNEL_BLUE: p += blueShift; break;
				}
				for (i=0;i<ret;i++,p += bpp) {
					*samples++ = *p;
				}
			} else {
				const float *conv;
				switch (channel) {
				case DDSSTV_CHANNEL_Y: conv = y_conv; break;
				case DDSSTV_CHANNEL_Cb: conv = cb_conv; break;
				case DDSSTV_CHANNEL_Cr: conv = cr_conv; break;
				default: ret = 0; break;
				}
				for (i=0;i<ret;i++,p += bpp) {
					*samples++ = (uint8_t) (conv[0]
					           + (float)conv[1]*((float)p[redShift]/255.0)
							   + (float)conv[2]*((float)p[greenShift]/255.0)
							   + (float)conv[3]*((float)p[blueShift]/255.0));
				}

			}
		} else {
			// Palette

			switch (channel) {
			case DDSSTV_CHANNEL_Y:
			case DDSSTV_CHANNEL_GREEN:
				for (i=0;i<ret;i++,p+=bpp) {
					*samples++ = surface->format->palette->colors[*p].g;
				}
				break;
			case DDSSTV_CHANNEL_BLUE:
				for (i=0;i<ret;i++,p+=bpp) {
					*samples++ = surface->format->palette->colors[*p].b;
				}
				break;
			case DDSSTV_CHANNEL_RED:
				for (i=0;i<ret;i++,p+=bpp) {
					*samples++ = surface->format->palette->colors[*p].r;
				}
				break;
			default: ret = 0; break;
			}
		}

		SDL_UnlockSurface(surface);
	}
	return ret;
}

void
modulator_output_callback(void* context, const float* samples, size_t count)
{
	struct encoder_state_s *state = context;

	while (count--) {
		if (state->sample_count >= state->sample_max) {
			return;
		}
		if (state->use_mu_law) {
			state->samples[state->sample_count++] = dddsp_decimator_mulaw_feed(&state->decimator, *samples++ * 0.5);
		} else {
			state->samples[state->sample_count++] = dddsp_decimator_int8_feed(&state->decimator, *samples++ * 0.5);
		}
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
		fprintf(stderr, "fed %d thru %d\n",state->sample_playback, state->sample_playback+len);
		memcpy(stream, &state->samples[state->sample_playback], len);
		state->sample_playback += len;
	}
}

static void print_version(void)
{
    printf("ddsstv-encoder " PACKAGE_VERSION "(" __TIME__ " " __DATE__ ")\n");
    printf("Copyright (c) 2016 Robert Quattlebaum, All Rights Reserved\n");
}


static void print_help(void)
{
    print_version();
    const char* help =
    "\n"
    "Syntax:\n"
    "\n"
    "    ddsstv-encoder [options] <image-file>\n"
    "\n"
    "Options:\n"
    "\n"
    "    -l/--list-vis-codes .......... List supported VIS codes.\n"
    "    -c/--vis-code[=code] ......... What VIS code to use for encoding image.\n"
    "    -o/--output[=filename.wav] ... Output as a WAV file instead of speakers\n"
    "    -h/-?/--help ................. Print out usage information and exit.\n"
    "\n";

    printf("%s", help);
}

static void
print_vis_codes(void)
{
	int i;
	for (i = 0; i < 255; i++) {
		struct ddsstv_mode_s mode;
		ddsstv_mode_lookup_vis_code(&mode, i);
		if (ddsstv_vis_code_is_supported(i)) {
			printf("\t%d\t%s\t%dx%d\t%.2fms/l\n", i, ddsstv_describe_vis_code(i), mode.width, mode.height, (double)mode.scanline_duration/USEC_PER_MSEC);
		}
	}

	printf("\t%d\t%s\n", kSSTVVISCodeWeatherFax120_IOC576, ddsstv_describe_vis_code(kSSTVVISCodeWeatherFax120_IOC576));
}

static int
write_wav(struct encoder_state_s *state, const char* output_filename)
{
/*
The canonical WAVE format starts with the RIFF header:

0         4   ChunkID          Contains the letters "RIFF" in ASCII form
                               (0x52494646 big-endian form).
4         4   ChunkSize        36 + SubChunk2Size, or more precisely:
                               4 + (8 + SubChunk1Size) + (8 + SubChunk2Size)
                               This is the size of the rest of the chunk
                               following this number.  This is the size of the
                               entire file in bytes minus 8 bytes for the
                               two fields not included in this count:
                               ChunkID and ChunkSize.
8         4   Format           Contains the letters "WAVE"
                               (0x57415645 big-endian form).

The "WAVE" format consists of two subchunks: "fmt " and "data":
The "fmt " subchunk describes the sound data's format:

12        4   Subchunk1ID      Contains the letters "fmt "
                               (0x666d7420 big-endian form).
16        4   Subchunk1Size    16 for PCM.  This is the size of the
                               rest of the Subchunk which follows this number.
20        2   AudioFormat      PCM = 1 (i.e. Linear quantization)
                               Values other than 1 indicate some
                               form of compression.
22        2   NumChannels      Mono = 1, Stereo = 2, etc.
24        4   SampleRate       8000, 44100, etc.
28        4   ByteRate         == SampleRate * NumChannels * BitsPerSample/8
32        2   BlockAlign       == NumChannels * BitsPerSample/8
                               The number of bytes for one sample including
                               all channels. I wonder what happens when
                               this number isn't an integer?
34        2   BitsPerSample    8 bits = 8, 16 bits = 16, etc.
          2   ExtraParamSize   if PCM, then doesn't exist
          X   ExtraParams      space for extra parameters

The "data" subchunk contains the size of the data and the actual sound:

36        4   Subchunk2ID      Contains the letters "data"
                               (0x64617461 big-endian form).
40        4   Subchunk2Size    == NumSamples * NumChannels * BitsPerSample/8
                               This is the number of bytes in the data.
                               You can also think of this as the size
                               of the read of the subchunk following this
                               number.
44        *   Data             The actual sound data.*/
	int ret = EXIT_SUCCESS;
	uint16_t format = state->use_mu_law?7:1;
	int sample_size = 1;
	struct wave_header_s {
		char chunkid[4];
		uint32_t chunksize;
		char format[4];

		char subchunkid1[4];
		uint32_t subchunkid1size;
		uint16_t audioformat;
		uint16_t channelcount;
		uint32_t samplerate;
		uint32_t byterate;
		uint16_t blockalign;
		uint16_t bitspersample;
		char subchunkid2[4];
		uint32_t subchunkid2size;
	} header = {
		"RIFF",
		(uint32_t)(state->sample_count*sample_size)+36,
		"WAVE",
		"fmt ",
		16,
		format,
		1,
		(uint32_t)state->encoder.modulator.multiplier,
		(uint32_t)(state->encoder.modulator.multiplier*sample_size),
		1,
		sample_size*8,
		"data",
		(uint32_t)state->sample_count*sample_size
	};
	int i;
	FILE* file = fopen(output_filename, "w");

	if (!file) {
		perror("fopen");
		exit(EXIT_FAILURE);
	}

	fwrite(&header, sizeof(header), 1, file);
	for(i = 0; i<state->sample_count; i++) {
		uint8_t sample = state->samples[i] + (state->use_mu_law?0:127);
		fwrite(&sample, 1, 1, file);
	}
	fclose(file);

	return ret;
}


int
main(int argc, char * argv[])
{
	struct encoder_state_s state = { };
	SDL_AudioSpec audio_spec;
	const char* input_filename = NULL;
	const char* output_filename = NULL;
	ddsstv_vis_code_t vis_code = kSSTVVISCodeBW8;

    static struct option options[] = {
        { "output",     required_argument, NULL,   'o'           },
        { "version",    no_argument,       NULL,   'V'           },
        { "help",       no_argument,       NULL,   'h'           },
        { "list-vis-codes",no_argument,    NULL,   'l'           },
        { "vis-code",   no_argument,       NULL,   'c'           },
        { NULL,         0,                 NULL,   0             },
    };

    if (argc < 2)
    {
        print_help();
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        int c = getopt_long(argc, argv, "i:c:o:lvVh?", options, NULL);
        if (c == -1)
        {
            break;
        }
        else
        {
            switch (c)
            {
            case 'o':
				output_filename = optarg;
				state.use_mu_law = true;
                break;

			case 'c':
				vis_code = strtol(optarg, NULL, 0);
				break;

            case 'V':
                print_version();
                exit(EXIT_SUCCESS);
                break;

            case 'l':
                print_vis_codes();
                exit(EXIT_SUCCESS);
                break;

            case 'h':
            case '?':
                print_help();
                exit(EXIT_SUCCESS);
                break;
            }
        }
    }

    argc -= optind;
    argv += optind;

    if (argc == 1)
    {
		input_filename = argv[0];
    } else if (argc > 1) {
		fprintf(stderr, "error: Unexpected argument: %s\n", argv[1]);
		exit(EXIT_FAILURE);
	}

	setenv("SDL_VIDEODRIVER", "dummy", 1);

	if ( SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO) < 0 ) {
		fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	ddsstv_encoder_init(&state.encoder);

	state.encoder.modulator.multiplier = 8000;
	//state.encoder.use_mmsstv_prefix_tones = true;

	if (state.use_mu_law) {
		dddsp_decimator_mulaw_init(&state.decimator, -1, 1);
	} else {
		dddsp_decimator_int8_init(&state.decimator, -1, 1);
	}

	state.sample_max = state.encoder.modulator.multiplier*120; // 120 seconds of samples.
	state.sample_count = 0;
	state.samples = malloc(state.sample_max);
	state.sample_playback = 0;

	if (input_filename) {
		//SDL_Surface *surface = SDL_LoadBMP(input_filename);
		SDL_Surface *surface = IMG_Load(input_filename);
		if (!surface) {
			fprintf(stderr, "Couldn't open image file \"%s\": %s\n", input_filename, SDL_GetError());
			exit(EXIT_FAILURE);
		}
		state.encoder.pull_scanline_cb = &sdl_image_scanline_callback;
		state.encoder.context = surface;
	} else {
		state.encoder.pull_scanline_cb = &colorbar_scanline_callback;
	}

	state.encoder.modulator.output_func_context = &state;
	state.encoder.modulator.output_func = &modulator_output_callback;

	ddsstv_mode_lookup_vis_code(&state.encoder.mode, vis_code);

    sPreviousHandlerForSIGINT = signal(SIGINT, &signal_SIGINT);

	while(ddsstv_encoder_process(&state.encoder)) { }

	if (output_filename) {
		write_wav(&state, output_filename);

	} else {
		// ------ Now play back the audio -----

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
			if (sRet) {
				break;
			}

			sleep(1);
		}

		if (!sRet) {
			sleep(1);
		}

		SDL_PauseAudio(1);
	}

	if (sRet == EXIT_QUIT) {
		sRet = EXIT_SUCCESS;
	}

	return sRet;
}
