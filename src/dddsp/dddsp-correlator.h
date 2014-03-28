//
//  dddsp-correlator.h
//  SSTV
//
//  Created by Robert Quattlebaum on 3/21/14.
//  Copyright (c) 2014 deepdarc. All rights reserved.
//

#ifndef SSTV_dddsp_correlator_h
#define SSTV_dddsp_correlator_h

#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#define DDDSP_UNCORRELATED		(-2147483647)

typedef enum {
	DDDSP_BOX_DEFAULT = 0,
	DDDSP_BOX_GREATER_THAN,
	DDDSP_BOX_LESS_THAN,
	DDDSP_BOX_DUAL,
	DDDSP_BOX_BETWEEN,
	DDDSP_BOX_SKIP,
} dddsp_correlator_box_type_t;

struct dddsp_correlator_box_s {
	int offset;
	int width;			//!^ May be zero
	int32_t expect;
	int32_t expect2;
	int32_t threshold;
	dddsp_correlator_box_type_t type;

	// Read-only after this point
	int32_t sum;
	int count;
};

struct dddsp_correlator_s {
	int box_count;
	struct dddsp_correlator_box_s* box;

	// Internal after this point
	int32_t *buffer;
	int buffer_size;
	int64_t next_sample;
};

struct dddsp_correlator_s;
typedef struct dddsp_correlator_s *dddsp_correlator_t;

dddsp_correlator_t dddsp_correlator_alloc(const struct dddsp_correlator_box_s* boxes, int box_count);
void dddsp_correlator_finalize(dddsp_correlator_t self);
void dddsp_correlator_reset(dddsp_correlator_t self);
int32_t dddsp_correlator_feed(dddsp_correlator_t self, int32_t sample);
int32_t dddsp_correlator_get_box_avg(dddsp_correlator_t self, int box_index);

#endif
