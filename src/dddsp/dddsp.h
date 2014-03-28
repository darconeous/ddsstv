//
//  dddsp.h
//  SSTV
//
//  Created by Robert Quattlebaum on 10/30/13.
//  Copyright (c) 2013 deepdarc. All rights reserved.
//

#ifndef SSTV_dddsp_h
#define SSTV_dddsp_h

#ifndef USEC_PER_MSEC
#define USEC_PER_MSEC		1000
#endif

#ifndef MSEC_PER_SEC
#define MSEC_PER_SEC		1000
#endif

#ifndef USEC_PER_SEC
#define USEC_PER_SEC		(MSEC_PER_SEC*USEC_PER_MSEC)
#endif

#include "dddsp-misc.h"
#include "dddsp-filter.h"
#include "dddsp-discriminator.h"
#include "dddsp-correlator.h"
#include "dddsp-resampler.h"
#include "dddsp-decimator.h"
#include "dddsp-modulator.h"

#endif
