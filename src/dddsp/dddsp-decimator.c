//
//  dddsp-decimator.c
//  SSTV
//
//  Created by Robert Quattlebaum on 3/27/14.
//  Copyright (c) 2014 deepdarc. All rights reserved.
//

#include <stdio.h>
#include "dddsp-misc.h"
#include "dddsp-decimator.h"
#include "g711.h"

void
dddsp_decimator_mulaw_init(struct dddsp_decimator_s* decimator, float min, float max)
{
	decimator->offset = -(max+min)*0.5f;
	decimator->scale = (max-min)/65535.0f;
}


uint8_t
dddsp_decimator_mulaw_feed(struct dddsp_decimator_s* decimator, float input)
{
	uint8_t ret;

	input = (input + decimator->offset) / decimator->scale;
	input = dddsp_clampf(input, decimator->nanvalue, -32768.0f, 32767.0f);
	ret = linear2ulaw(input + 0.5f);

	// Noise shaping only works for linear decimation.
	//decimator->error = input - (float)ulaw2linear(ret);
	return ret;
}


void
dddsp_decimator_uint8_init(struct dddsp_decimator_s* decimator, float min, float max)
{
	decimator->offset = -min;
	decimator->scale = (max-min)/255.0f;
	decimator->nanvalue = 127.0f;
}

uint8_t
dddsp_decimator_uint8_feed(struct dddsp_decimator_s* decimator, float input)
{
	uint8_t ret;

	input = (input + decimator->offset) / decimator->scale;
	input = dddsp_clampf(input, decimator->nanvalue, 0.0f, 255.0f);
	ret = (decimator->error + input + 0.5f);
	decimator->error = input - (float)ret;

	return ret;
}

void
dddsp_decimator_int8_init(struct dddsp_decimator_s* decimator, float min, float max)
{
	decimator->offset = -(max+min)*0.5f;
	decimator->scale = (max-min)/255.0f;
	decimator->nanvalue = 0;
}

int8_t dddsp_decimator_int8_feed(struct dddsp_decimator_s* decimator, float input)
{
	int8_t ret;

	input = (input + decimator->offset) / decimator->scale;
	input = dddsp_clampf(input, decimator->nanvalue, -128.0f, 127.0f);
	ret = (decimator->error + input + 0.5f);
	decimator->error = input - (float)ret;

	return ret;
}

void
dddsp_decimator_uint16_init(struct dddsp_decimator_s* decimator, float min, float max)
{
	decimator->offset = -min;
	decimator->scale = (max-min)/65535.0f;
}

uint16_t
dddsp_decimator_uint16_feed(struct dddsp_decimator_s* decimator, float input)
{
	uint16_t ret;

	input = (input + decimator->offset) / decimator->scale;
	input = dddsp_clampf(input, decimator->nanvalue, 0.0f, 65535.0f);
	ret = (decimator->error + input + 0.5f);
	decimator->error = input - (float)ret;

	return ret;
}

void
dddsp_decimator_int16_init(struct dddsp_decimator_s* decimator, float min, float max)
{
	decimator->offset = -(max+min)*0.5f;
	decimator->scale = (max-min)/65535.0f;
}

int16_t
dddsp_decimator_int16_feed(struct dddsp_decimator_s* decimator, float input)
{
	int16_t ret;

	input = (input + decimator->offset) / decimator->scale;
	input = dddsp_clampf(input, decimator->nanvalue, -32768.0f, 32767.0f);
	ret = (decimator->error + input + 0.5f);
	decimator->error = input - (float)ret;

	return ret;
}
