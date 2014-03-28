//
//  dddsp-discriminator.h
//  SSTV
//
//  Created by Robert Quattlebaum on 3/21/14.
//  Copyright (c) 2014 deepdarc. All rights reserved.
//

#ifndef SSTV_dddsp_discriminator_h
#define SSTV_dddsp_discriminator_h

#include <stdbool.h>
#include <stdint.h>
#include <math.h>

struct dddsp_discriminator_s;
typedef struct dddsp_discriminator_s *dddsp_discriminator_t;
dddsp_discriminator_t dddsp_discriminator_alloc(float sample_rate, float carrier, float max_deviation, bool high_quality);
void dddsp_discriminator_finalize(dddsp_discriminator_t self);
float dddsp_discriminator_feed(dddsp_discriminator_t self, float sample);


#endif
