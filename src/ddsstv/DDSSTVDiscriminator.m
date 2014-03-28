//
//  DDSSTVDiscriminator.m
//  SSTV
//
//  Created by Robert Quattlebaum on 11/18/13.
//  Copyright (c) 2013 deepdarc. All rights reserved.
//

#import "DDSSTVDiscriminator.h"


#define kNumberRecordBuffers	4

@implementation DDSSTVDiscriminator

static bool
discriminator_output_func(void* context, const int16_t* freqs, size_t count)
{
	DDSSTVDiscriminator* self = (__bridge DDSSTVDiscriminator*)context;
	return [self.delegate feedFrequencies:freqs count:count];
}

- (id)init
{
    self = [super init];
    if (self) {
		queue = dispatch_queue_create("discriminator_queue", 0);
        discriminator = calloc(1,sizeof(*discriminator));
		ddsstv_discriminator_init(discriminator, 8000, DDSSTV_INTERNAL_SAMPLE_RATE);
		discriminator->output_func_context = (__bridge void *)(self);
		discriminator->output_func = &discriminator_output_func;
    }
    return self;
}

static void MyInputBufferHandler(	void *                          inUserData,
									AudioQueueRef                   inAQ,
									AudioQueueBufferRef             inBuffer,
									const AudioTimeStamp *          inStartTime,
									UInt32							inNumPackets,
									const AudioStreamPacketDescription *inPacketDesc)
{
	DDSSTVDiscriminator* self = (__bridge DDSSTVDiscriminator*)inUserData;
	[self feedSamples:inBuffer->mAudioData count:inNumPackets];
	AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, NULL);
}

// ____________________________________________________________________________________
// Determine the size, in bytes, of a buffer necessary to represent the supplied number
// of seconds of audio data.
static int MyComputeRecordBufferSize(const AudioStreamBasicDescription *format, AudioQueueRef queue, float seconds)
{
	int packets, frames, bytes;

	frames = (int)ceil(seconds * format->mSampleRate);

	if (format->mBytesPerFrame > 0)
		bytes = frames * format->mBytesPerFrame;
	else {
		UInt32 maxPacketSize;
		if (format->mBytesPerPacket > 0)
			maxPacketSize = format->mBytesPerPacket;	// constant packet size
		else {
			UInt32 propertySize = sizeof(maxPacketSize);
			AudioQueueGetProperty(queue, kAudioConverterPropertyMaximumOutputPacketSize, &maxPacketSize,
				&propertySize);
		}
		if (format->mFramesPerPacket > 0)
			packets = frames / format->mFramesPerPacket;
		else
			packets = frames;	// worst-case scenario: 1 frame in a packet
		if (packets == 0)		// sanity check
			packets = 1;
		bytes = packets * maxPacketSize;
	}
	return bytes;
}

-(AudioQueueRef)audioQueue
{
	if(!audioQueue) {
		OSStatus status;
		AudioStreamBasicDescription recordFormat = {
			.mSampleRate = self.sampleRate,
			.mFormatID = kAudioFormatLinearPCM,
			.mFormatFlags = kAudioFormatFlagIsFloat,
			.mChannelsPerFrame = 1,
			.mBitsPerChannel = 32,
		};
		recordFormat.mBytesPerPacket = recordFormat.mBytesPerFrame =
				(recordFormat.mBitsPerChannel / 8) * recordFormat.mChannelsPerFrame;
		recordFormat.mFramesPerPacket = 1;
		status = AudioQueueNewInput(
			&recordFormat,
			MyInputBufferHandler,
			(__bridge void *)(self) /* userData */,
			NULL /* run loop */,
			NULL /* run loop mode */,
			0 /* flags */,
			&audioQueue
		);
		if(status) {
			NSLog(@"AudioQueueNewInput %d",status);
			return NULL;
		}

		// allocate and enqueue buffers
		int i,bufferByteSize = MyComputeRecordBufferSize(&recordFormat, audioQueue, 0.25);
		for (i = 0; i < kNumberRecordBuffers; ++i) {
			AudioQueueBufferRef buffer;
			AudioQueueAllocateBuffer(audioQueue, bufferByteSize, &buffer);
			AudioQueueEnqueueBuffer(audioQueue, buffer, 0, NULL);
		}
	}
	return audioQueue;
}

-(void)dealloc {
	if(audioQueue)
		CFRelease(audioQueue);
	ddsstv_discriminator_finalize(discriminator);
	free(discriminator);
}

-(BOOL)hiRes
{
	return false;
//	return ddsstv_discriminator_get_high_res(discriminator);
}

-(void)setHiRes:(BOOL)hiRes
{
//	dispatch_async(queue, ^{
//		ddsstv_discriminator_set_high_res(discriminator, hiRes);
//	});
}

-(double)sampleRate {
	return discriminator->resampler.ingest_sample_rate;
}

-(void)setSampleRate:(double)sampleRate {
	if(sampleRate != discriminator->resampler.ingest_sample_rate) {
		dispatch_async(queue, ^{
			ddsstv_discriminator_finalize(discriminator);
			ddsstv_discriminator_init(discriminator, sampleRate, DDSSTV_INTERNAL_SAMPLE_RATE);
			discriminator->output_func_context = (__bridge void *)(self);
			discriminator->output_func = &discriminator_output_func;
		});
	}
}

-(bool)feedSamples:(const float*)samples count:(NSInteger)count
{
	__block bool ret;
	dispatch_sync(queue, ^{
		ret = ddsstv_discriminator_ingest_samps(discriminator, samples, count);
	});
	return ret;
}

-(bool)feedSamples:(NSData*)samples
{
	return [self feedSamples:(const float*)[samples bytes] count:[samples length]/sizeof(float)];
}

@end
