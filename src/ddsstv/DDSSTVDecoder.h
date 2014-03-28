//
//  DDSSTVDecoder.h
//  SSTV
//
//  Created by Robert Quattlebaum on 10/22/13.
//  Copyright (c) 2013 deepdarc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "ddsstv.h"
#import "DDSSTVDiscriminator.h"

@class DDSSTVDecoder;

@protocol DDSSTVDecoderDelegate <NSObject>
-(void)decoder:(DDSSTVDecoder*)decoder didFinishImage:(NSImage*)image;
@end

@interface DDSSTVDecoder : NSObject <DDSSTVDiscriminatorDelegate> {
	dispatch_queue_t decoder_queue;
	ddsstv_decoder_t ddsstv_decoder;
}

@property(assign) IBOutlet id<DDSSTVDecoderDelegate> delegate;

@property NSImage* image;
@property NSInteger sampleRate;

@property BOOL decodingImage;
@property BOOL asyncronous;
@property BOOL live;

@property double offset;
@property double scanlineLength;
@property double syncLength;
@property BOOL asynchronous;
@property NSInteger colorMode;
@property NSInteger visCode;

@property double autosyncOffset;
@property double frontPorchDuration;
@property double backPorchDuration;

@property BOOL autostartVIS;
@property BOOL autostartVSync;

-(IBAction)reset:(id)sender;
-(IBAction)start:(id)sender;
-(IBAction)stop:(id)sender;
-(IBAction)refreshImage:(id)sender;

-(bool)feedFrequencies:(const int16_t*)freqs count:(NSInteger)count;

@end
