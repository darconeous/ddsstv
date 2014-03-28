//
//  dddsp-deicmator.h
//  SSTV
//
//  Created by Robert Quattlebaum on 3/24/14.
//  Copyright (c) 2014 deepdarc. All rights reserved.
//

#ifndef SSTV_dddsp_decimator_h
#define SSTV_dddsp_decimator_h

#include <stdint.h>

struct dddsp_decimator_s {
	float offset, scale, error, nanvalue;
};

void dddsp_decimator_mulaw_init(struct dddsp_decimator_s* decimator, float min, float max);

void dddsp_decimator_uint8_init(struct dddsp_decimator_s* decimator, float min, float max);
void dddsp_decimator_int8_init(struct dddsp_decimator_s* decimator, float min, float max);
void dddsp_decimator_uint16_init(struct dddsp_decimator_s* decimator, float min, float max);
void dddsp_decimator_int16_init(struct dddsp_decimator_s* decimator, float min, float max);

uint8_t dddsp_decimator_mulaw_feed(struct dddsp_decimator_s* decimator, float input);

uint8_t dddsp_decimator_uint8_feed(struct dddsp_decimator_s* decimator, float input);
int8_t dddsp_decimator_int8_feed(struct dddsp_decimator_s* decimator, float input);

uint16_t dddsp_decimator_uint16_feed(struct dddsp_decimator_s* decimator, float input);
int16_t dddsp_decimator_int16_feed(struct dddsp_decimator_s* decimator, float input);

#endif
