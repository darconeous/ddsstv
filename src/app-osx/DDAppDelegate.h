//
//  DDAppDelegate.h
//  SSTV
//
//  Created by Robert Quattlebaum on 10/17/13.
//  Copyright (c) 2013 deepdarc. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "DDSSTVEncoder.h"
#import "DDSSTVDecoder.h"
#import "DDSSTVDiscriminator.h"
#import <CoreAudio/CoreAudio.h>
#import <AudioUnit/AudioUnit.h>
#import <AudioToolbox/AudioToolbox.h>
#import "ddsstv.h"

@interface DDAppDelegate : NSObject <NSApplicationDelegate,DDSSTVDecoderDelegate>
{
	dispatch_queue_t work_queue;
}

@property IBOutlet DDSSTVDecoder* decoder;
@property IBOutlet DDSSTVDiscriminator* discriminator;

@property (assign) IBOutlet NSImageView *imageView;

@property (assign) IBOutlet NSWindow *window;

@property (assign) IBOutlet NSWindow *toolwindow;

@end
