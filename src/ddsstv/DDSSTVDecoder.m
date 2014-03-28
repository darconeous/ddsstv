//
//  DDSSTVDecoder.m
//  SSTV
//
//  Created by Robert Quattlebaum on 10/22/13.
//  Copyright (c) 2013 deepdarc. All rights reserved.
//

#include <dddsp/dddsp.h>
#import "DDSSTVDecoder.h"
#import "DDSSTVEncoder.h"





@implementation DDSSTVDecoder

static void _image_finished_cb(void* context, ddsstv_decoder_t decoder)
{
	DDSSTVDecoder* self = (__bridge DDSSTVDecoder*)context;
	[self willChangeValueForKey:@"decodingImage"];
	[self didChangeValueForKey:@"decodingImage"];
	[self.delegate decoder:self didFinishImage:[self calcImage]];
}

-(void)setNilValueForKey:(NSString *)key
{
	SEL s = NSSelectorFromString([@"setNilValueFor" stringByAppendingString:[key capitalizedString]]);
	if ([self respondsToSelector:s])
		[self performSelector:s];
}

- (NSImage *)imageResize:(NSImage*)anImage newSize:(NSSize)newSize {
    NSImage *sourceImage = anImage;
    [sourceImage setScalesWhenResized:YES];

    // Report an error if the source isn't a valid image
    if (![sourceImage isValid])
    {
		NSLog(@"Invalid Image");
    } else
    {
		NSImage *smallImage = [[NSImage alloc] initWithSize: newSize];
		[smallImage lockFocus];
		[sourceImage setSize: newSize];
		[[NSGraphicsContext currentContext] setImageInterpolation:NSImageInterpolationHigh];
		[sourceImage drawAtPoint:NSZeroPoint fromRect:CGRectMake(0, 0, newSize.width, newSize.height) operation:NSCompositeCopy fraction:1.0];
		[smallImage unlockFocus];
		return smallImage;
    }
    return nil;
}

- (id)init
{
    self = [super init];
    if (self) {
		decoder_queue = dispatch_queue_create("decoder_queue", 0);
		ddsstv_decoder = calloc(1,sizeof(struct ddsstv_decoder_s));
		ddsstv_decoder_init(ddsstv_decoder, DDSSTV_INTERNAL_SAMPLE_RATE);
		//ddsstv_decoder->asynchronous = true;
		//ddsstv_decoder->autosync_vsync = true;
		ddsstv_mode_lookup_vis_code(&ddsstv_decoder->mode, 1);
		ddsstv_decoder_set_image_finished_callback(ddsstv_decoder, (__bridge void *)(self), &_image_finished_cb);
    }
    return self;
}

//-(NSInteger)sampleRate {
//	return ddsstv_decoder->ingest_sample_rate;
//}
//
//-(void)setSampleRate:(NSInteger)sampleRate {
//	if ((sampleRate>DDSSTV_MAX_INGEST_SAMPLE_RATE)
//		|| (sampleRate<DDSSTV_MIN_INGEST_SAMPLE_RATE)
//		|| (sampleRate&3)!=0
//	) {
//		@throw [NSException
//			exceptionWithName:NSInvalidArgumentException
//			reason:@"Unsupported ingest sample rate"
//			userInfo:nil
//		];
//	}
//
//	ddsstv_decoder->ingest_sample_rate = (uint16_t)sampleRate;
//}

-(BOOL)decodingImage {
	return ddsstv_decoder->is_decoding;
}

-(void)setDecodingImage:(BOOL)decoding {
	if(decoding != ddsstv_decoder->is_decoding) {
		if(decoding)
			ddsstv_decoder_start(ddsstv_decoder);
		else
			ddsstv_decoder_stop(ddsstv_decoder);
	}
}

-(double)autosyncOffset
{
	return ddsstv_decoder->mode.autosync_offset/1000.0;
}

-(void)setAutosyncOffset:(double)x
{
	dispatch_sync(decoder_queue, ^{
		ddsstv_decoder->mode.autosync_offset = x*1000.0;
		if(ddsstv_decoder->has_image) {
			ddsstv_decoder_recalc_image(ddsstv_decoder);
			[self refreshImage:self];
		}
	});
}

-(double)frontPorchDuration
{
	return ddsstv_decoder->mode.front_porch_duration/1000.0;
}

-(void)setFrontPorchDuration:(double)frontPorchDuration
{
	dispatch_sync(decoder_queue, ^{
		ddsstv_decoder->mode.front_porch_duration = frontPorchDuration*1000.0;
		if(ddsstv_decoder->has_image) {
			ddsstv_decoder_recalc_image(ddsstv_decoder);
			[self refreshImage:self];
		}
	});
}

-(double)backPorchDuration
{
	return ddsstv_decoder->mode.back_porch_duration/1000.0;
}

-(void)setBackPorchDuration:(double)backPorchDuration
{
	dispatch_sync(decoder_queue, ^{
		ddsstv_decoder->mode.back_porch_duration = backPorchDuration*1000.0;
		if(ddsstv_decoder->has_image) {
			ddsstv_decoder_recalc_image(ddsstv_decoder);
			[self refreshImage:self];
		}
	});
}

-(double)offset {
	if(!ddsstv_decoder)
		return 0;
	return ddsstv_decoder->mode.start_offset/1000.0;
}

-(void)setOffset:(double)offset {
	if(!ddsstv_decoder)
		return;
	if(offset<-40)
		offset = -40;
	dispatch_sync(decoder_queue, ^{
		ddsstv_decoder->mode.start_offset = offset*1000.0;
		if(ddsstv_decoder->has_image) {
			ddsstv_decoder_recalc_image(ddsstv_decoder);
			[self refreshImage:self];
		}
	});
}


-(BOOL)autostartVIS {
	if(!ddsstv_decoder)
		return NO;
	return ddsstv_decoder->autosync_vis;
}

-(void)setAutostartVIS:(BOOL)autostartVIS {
	if(!ddsstv_decoder)
		return;
//	dispatch_sync(decoder_queue, ^{
		ddsstv_decoder->autosync_vis = autostartVIS;
//	});
}

-(BOOL)autostartVSync {
	if(!ddsstv_decoder)
		return NO;
	return ddsstv_decoder->autosync_vis;
}

-(void)setAutostartVSync:(BOOL)autostartVSync {
	if(!ddsstv_decoder)
		return;
//	dispatch_sync(decoder_queue, ^{
		ddsstv_decoder->autosync_vsync = autostartVSync;
//	});
}



-(BOOL)asynchronous {
	if(!ddsstv_decoder)
		return NO;
	return ddsstv_decoder->asynchronous;
}

-(void)setAsynchronous:(BOOL)asynchronous {
	if(!ddsstv_decoder)
		return;
	[self willChangeValueForKey:@"scanlineLength"];
	[self willChangeValueForKey:@"syncLength"];
	dispatch_sync(decoder_queue, ^{
		ddsstv_decoder->asynchronous = asynchronous;
		if(ddsstv_decoder->has_image) {
			ddsstv_decoder_recalc_image(ddsstv_decoder);
			[self refreshImage:self];
		}
	});
	[self didChangeValueForKey:@"syncLength"];
	[self didChangeValueForKey:@"scanlineLength"];
}

-(double)scanlineLength {
	if(!ddsstv_decoder)
		return 0;
	return ddsstv_decoder->mode.scanline_duration/1000.0;
}

-(void)setScanlineLength:(double)scanlineLength {
	if(!ddsstv_decoder)
		return;
	if(scanlineLength<1)
		scanlineLength = 1;
	dispatch_sync(decoder_queue, ^{
		ddsstv_decoder->mode.scanline_duration = scanlineLength*1000.0;
		if(ddsstv_decoder->has_image) {
			ddsstv_decoder_recalc_image(ddsstv_decoder);
			[self refreshImage:self];
		}
	});
}

-(double)syncLength {
	if(!ddsstv_decoder)
		return 0;
	return ddsstv_decoder->mode.sync_duration/1000.0;
}

-(void)setNilValueForSyncLength {
	[self setSyncLength:0];
}

-(void)setSyncLength:(double)scanlineLength {
	if(!ddsstv_decoder)
		return;
	if(scanlineLength<0.0)
		scanlineLength = 0.0;
	dispatch_sync(decoder_queue, ^{
		ddsstv_decoder->mode.sync_duration = scanlineLength*1000.0;
		if(ddsstv_decoder->has_image) {
			ddsstv_decoder_recalc_image(ddsstv_decoder);
			[self refreshImage:self];
		}
	});
}

-(NSInteger)colorMode {
	if(!ddsstv_decoder)
		return 0;
	return ddsstv_decoder->mode.color_mode;
}

-(void)setColorMode:(NSInteger)colorMode {
	if(!ddsstv_decoder)
		return;
	dispatch_sync(decoder_queue, ^{
		ddsstv_decoder->mode.color_mode = (ddsstv_color_mode_t)colorMode;
		if(ddsstv_decoder->has_image) {
			ddsstv_decoder_recalc_image(ddsstv_decoder);
			[self refreshImage:self];
		}
	});
}

-(NSString*)visCodeDescription
{
	if(!ddsstv_decoder)
		return 0;
	return [NSString stringWithUTF8String:ddsstv_describe_vis_code(ddsstv_decoder->mode.vis_code)];
}

-(NSInteger)visCode {
	if(!ddsstv_decoder)
		return 0;
	return ddsstv_decoder->mode.vis_code;
}

-(void)willChangeMode
{
	[self willChangeValueForKey:@"visCode"];
	[self willChangeValueForKey:@"visCodeDescription"];
	[self willChangeValueForKey:@"syncLength"];
	[self willChangeValueForKey:@"scanlineLength"];
	[self willChangeValueForKey:@"colorMode"];
	[self willChangeValueForKey:@"offset"];
	[self willChangeValueForKey:@"autosyncOffset"];
	[self willChangeValueForKey:@"frontPorchDuration"];
	[self willChangeValueForKey:@"backPorchDuration"];
	[self willChangeValueForKey:@"decodingImage"];
}

-(void)didChangeMode
{
	[self didChangeValueForKey:@"decodingImage"];
	[self didChangeValueForKey:@"backPorchDuration"];
	[self didChangeValueForKey:@"frontPorchDuration"];
	[self didChangeValueForKey:@"offset"];
	[self didChangeValueForKey:@"colorMode"];
	[self didChangeValueForKey:@"scanlineLength"];
	[self didChangeValueForKey:@"syncLength"];
	[self didChangeValueForKey:@"autosyncOffset"];
	[self didChangeValueForKey:@"visCodeDescription"];
	[self didChangeValueForKey:@"visCode"];
}

-(void)setVisCode:(NSInteger)vis_code {
	if(!ddsstv_decoder)
		return;
	[self willChangeMode];
	dispatch_sync(decoder_queue, ^{
		struct ddsstv_mode_s mode = ddsstv_decoder->mode;
		ddsstv_mode_lookup_vis_code(&mode, (ddsstv_vis_code_t)vis_code);
		ddsstv_decoder_set_mode(ddsstv_decoder, &mode);
		ddsstv_decoder_recalc_image(ddsstv_decoder);
		[self refreshImage:self];
	});
	[self didChangeMode];
}

-(NSImage*)calcImage {
	CGImageRef image = ddsstv_decoder_copy_image(ddsstv_decoder);
	if(image) {
		NSImage *nsimage = [[NSImage alloc] initWithCGImage:image size:NSMakeSize(ddsstv_decoder->mode.aspect_width, ddsstv_decoder->mode.height)];
		CFRelease(image);
#if 1
		nsimage = [self imageResize:nsimage newSize:NSMakeSize(ddsstv_decoder->mode.aspect_width, ddsstv_decoder->mode.height)];
#else
		nsimage = [self imageResize:nsimage newSize:NSMakeSize(ddsstv_decoder->mode.width, ddsstv_decoder->mode.height*ddsstv_decoder->mode.width/ddsstv_decoder->mode.aspect_width)];
#endif
		return nsimage;
	}
	return nil;
}

-(bool)feedFrequencies:(const int16_t*)freqs count:(NSInteger)count
{
	__block bool ret = false;

	dispatch_sync(decoder_queue, ^{
		ret = ddsstv_decoder_ingest_freqs(ddsstv_decoder, freqs, count);
		dispatch_async(dispatch_get_main_queue(), ^{
			[self willChangeMode];
			[self didChangeMode];
		});
		[self refreshImage:self];
	});
	return ret;
}

-(IBAction)refreshImage:(id)sender
{
	dispatch_async(dispatch_get_main_queue(), ^{
		__block NSImage* image;
		dispatch_sync(decoder_queue, ^{
			image = [self calcImage];
		});
		self.image = image;
	});
}

-(IBAction)reset:(id)sender
{
	dispatch_sync(decoder_queue, ^{
		ddsstv_decoder_reset(ddsstv_decoder);
	});
}

-(IBAction)start:(id)sender
{
	dispatch_sync(decoder_queue, ^{
		ddsstv_decoder_start(ddsstv_decoder);
	});
}

-(IBAction)stop:(id)sender
{
	dispatch_sync(decoder_queue, ^{
		ddsstv_decoder_stop(ddsstv_decoder);
	});
}




@end





