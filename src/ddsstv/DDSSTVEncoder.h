//
//  DDSSTVEncoder.h
//  SSTV
//
//  Created by Robert Quattlebaum on 10/17/13.
//  Copyright (c) 2013 deepdarc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "dddsp.h"

enum {
	kSSTVPropertyColor	= 0,
	kSSTVPropertyBW_R	= 1,
	kSSTVPropertyBW_G	= 2,
	kSSTVPropertyBW_B	= 3,

	kSSTVPropertyHoriz_160	= (0<<2),
	kSSTVPropertyHoriz_320	= (1<<2),

	kSSTVPropertyVert_120	= (0<<3),
	kSSTVPropertyVert_240	= (1<<3),

	kSSTVPropertyType_Robot		= (0<<4),
	kSSTVPropertyType_WraaseSC1	= (1<<4),
	kSSTVPropertyType_Martin	= (2<<4),
	kSSTVPropertyType_Scotty	= (3<<4),


	kSSTVFormatScotty1 = 60,
	kSSTVFormatScotty2 = 56,
	kSSTVFormatScottyDX = 76,

	kSSTVFormatMartin1 = 44,
	kSSTVFormatMartin2 = 40,

	kSSTVFormatWRASSESC2_180 = 55,

	kSSTVFormatRobot12c = 0,
	kSSTVFormatRobot24c = 4,
	kSSTVFormatRobot36c = 8,
	kSSTVFormatRobot72c = 12,

	kSSTVFormatBW8 = 2,
	kSSTVFormatBW12 = 6,
	kSSTVFormatBW24 = 10,
	kSSTVFormatBW36 = 14,
};

@interface DDSSTVEncoder : NSObject {
	double leftover;
	double leftover_delta_theta;
	double last_freq;
	double last_mag;
	NSInteger _sampleSize;

	struct dddsp_modulator_s modulator;
	struct dddsp_decimator_s decimator;
}

@property BOOL useVideoRange;
@property NSImage* image;
@property double phase;
@property NSInteger sampleRate;
@property NSInteger sampleSize;
@property NSMutableData* data;

-(NSData*)WAVData;
-(void)appendTestPattern:(int)index encodedAs:(uint8_t)code;
-(void)appendImage:(NSImage*)image encodedAs:(uint8_t)code;
-(void)appendSamplesWithFrequency:(double)frequency forDuration:(double)duration;

@end
