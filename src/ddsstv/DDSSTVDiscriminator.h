//
//  DDSSTVDiscriminator.h
//  SSTV
//
//  Created by Robert Quattlebaum on 11/18/13.
//  Copyright (c) 2013 deepdarc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "ddsstv.h"
#import <AudioToolbox/AudioToolbox.h>

@class DDSSTVDiscriminator;

@protocol DDSSTVDiscriminatorDelegate <NSObject>
-(bool)feedFrequencies:(const int16_t*)frequencies count:(NSInteger)count;
@end

@interface DDSSTVDiscriminator : NSObject {
	dispatch_queue_t queue;
	ddsstv_discriminator_t discriminator;
	AudioQueueRef audioQueue;
}

@property(assign) IBOutlet id<DDSSTVDiscriminatorDelegate> delegate;

@property double sampleRate;
@property BOOL hiRes;

-(AudioQueueRef)audioQueue;

-(bool)feedSamples:(const float*)samples count:(NSInteger)count;
-(bool)feedSamples:(NSData*)samples;

@end
