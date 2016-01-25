//
//  DDAppDelegate.m
//  SSTV
//
//  Created by Robert Quattlebaum on 10/17/13.
//  Copyright (c) 2013 deepdarc. All rights reserved.
//

#import "DDAppDelegate.h"
#import <CoreAudio/CoreAudio.h>
#import <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>
#include "ddsstv.h"




@implementation DDAppDelegate

-(void)decoder:(DDSSTVDecoder *)decoder didFinishImage:(NSImage *)image
{
//	return;
	if(image) {
		static int counter;
		time_t timestamp = time(NULL);
		NSString *filename = nil;
		NSData* data = nil;
		filename = [NSString stringWithFormat:@"/Users/darco/Desktop/HamRadio/SSTV/inbound/ddsstv-14230-%ld-%d",timestamp,counter++];
//		filename = [NSString stringWithFormat:@"/Users/darco/Desktop/HamRadio/SSTV/inbound/ddsstv-dino-%d",counter++];
		NSLog(@"Writing %@",filename);
		NSBitmapImageRep* image_rep = [NSBitmapImageRep imageRepWithData:[image TIFFRepresentation]];

		if(image_rep) {
			data = [image_rep
				representationUsingType:NSJPEGFileType
				properties:[NSDictionary dictionaryWithObjectsAndKeys:
					nil
				]
			];
		}
		if(data) {
			filename = [filename stringByAppendingString:@".jpeg"];
		} else {
			NSLog(@"Can't save image as a JPEG! %@",filename);
			data = [image TIFFRepresentation];
			filename = [filename stringByAppendingString:@".tiff"];
		}
		BOOL success = [data writeToFile:filename atomically:NO];
		if(!success) {
			NSLog(@"Failed to write out image %@!",filename);
		} else {
			//NSLog(@"saved %@!",filename);
		}
	}
}

-(void)testNewDiscriminator
{
	double sampleRate = 16000;
	double phase = 0;

	dddsp_discriminator_t disc[2];
	disc[0] = dddsp_discriminator_alloc(
		sampleRate,
		1400.0/sampleRate,
		1400.0/2.0/sampleRate,
		true
	);
	disc[1] = dddsp_discriminator_alloc(
		sampleRate,
		2000.0/sampleRate,
		2000.0/2.0/sampleRate,
		true
	);

	for(int i=100;i<4000;i+=100) {
		double delta_theta = (i*M_PI*2.0)/sampleRate;
		double f = 0;
		int j = 0;

		for(j=-500;j<500;j++) {
			double t[2];
			t[0] = dddsp_discriminator_feed(disc[0], sin(phase));
			t[1] = dddsp_discriminator_feed(disc[1], sin(phase));

//			if(t[0]+t[1]>1400.0+2000.0)
//				t[0] = t[1];

			phase += delta_theta;

			if (j >= 0)
				f += t[0];
		}
		printf(" ******************** FREQ: %d - %f = %f\n",i,f/j,(double)i-f/j);
	}

	exit(0);

}

-(void)testNewFilter
{
	double sampleRate = 16000;
	double phase = 0;

	dddsp_iir_float_t filter;
	filter = dddsp_fir_float_alloc_low_pass(4000/sampleRate, 20);
//	filter = dddsp_iir_float_alloc_low_pass(2000/sampleRate, DDDSP_IIR_MAX_RIPPLE, 8);
//	filter = dddsp_iir_float_alloc_low_pass(2000/sampleRate, 0, 8);

	for(int i=100;i<8000;i+=10) {
		double delta_theta = (i*M_PI*2.0)/sampleRate;
		double f_min = 1000000;
		double f_max = -1000000;
		int j = 0;

		for(j=-500;j<500;j++) {
			double t;
			t = dddsp_iir_float_feed(filter, sin(phase));

			phase += delta_theta;

			if (j >= 0) {
				if (t < f_min)
					f_min = t;
				if (t > f_max)
					f_max = t;
			}
		}
		//printf(" ******************** ATTEN: %d = %fdB\t(%f, %f)\n",i,log10((f_max-f_min)/2)*10, f_min, f_max);
		printf("%d, \t%f, %f\n", i, log10((f_max-f_min)/2)*10, (f_max-f_min)/2);
	}

	exit(0);

}

-(void)testNewCorrelator
{
	int threshold = 2;
	struct dddsp_correlator_box_s boxes[] = {
		{ .offset = 0, .expect = 4, .threshold = threshold},
		{ .offset = 4, .expect = 9, .threshold = threshold},
		{ .offset = 5, .expect = 4, .threshold = threshold},
		{ .offset = 8, .expect = 9, .threshold = threshold},
		{ .offset = 9, .expect = 10, .expect2 = 15, .threshold = threshold, .type = DDDSP_BOX_DUAL },
		{ .offset = 10, .expect = 10, .expect2 = 15, .threshold = threshold, .type = DDDSP_BOX_DUAL },
		{ .offset = 11, .expect = 10, .expect2 = 15, .threshold = threshold, .type = DDDSP_BOX_DUAL },
		{ .offset = 12, .expect = 10, .expect2 = 15, .threshold = threshold, .type = DDDSP_BOX_DUAL },
		{ .offset = 13, .width = 1, .expect = 10, .expect2 = 15, .threshold = threshold, .type = DDDSP_BOX_DUAL },
	};
	int32_t values[] = {
/*
		1,
		2,
		3,
		4,
		5,
		5,

*/
		7,3,7,9,5,4,5,5,

		4,4,4,4,9,4,4,4,4,9,10,10,10,10,10,
/*
		4,4,4,4,9,4,4,4,4,9,15,10,15,10,15,

		7,3,7,9,5,4,4,4,

		4,4,4,4,9,4,4,4,4,9,15,10,15,10,15,
		4,4,4,4,9,4,4,4,4,9,15,10,15,10,15,
*/

		7,
		3,
		7,
		9,
		5,
		4,
		4,
		4,
		4,
		4,
	};
	int i;
	dddsp_correlator_t correlator = dddsp_correlator_alloc(boxes, sizeof(boxes)/sizeof(*boxes));
	if(correlator == NULL) {
		abort();
	}
	for(i=0;i<sizeof(values)/sizeof(*values);i++) {
		int32_t input = values[i];
		int32_t correlation = dddsp_correlator_feed(correlator, input);
		if(correlation==DDDSP_UNCORRELATED) {
			printf("%d (%d): UNCORRELATED\n",i,input);
		} else {
			printf("%d (%d): %d\n",i,input,correlation);
		}
	}

	exit(0);

}

-(NSData*)generateSamples
{
	DDSSTVEncoder* encoder = [[DDSSTVEncoder alloc] init];

	NSImage* image = nil;
//	image = [[NSImage alloc] initWithContentsOfFile:@"/Users/darco/Desktop/HamRadio/SSTV/sstv-test.jpg"];
//	image = [[NSImage alloc] initWithContentsOfFile:@"/Users/darco/Desktop/HamRadio/SSTV/O-RLY.jpg"];
//	image = [[NSImage alloc] initWithContentsOfFile:@"/Users/darco/Desktop/HamRadio/SSTV/Unknown.jpeg"];
//	image = [[NSImage alloc] initWithContentsOfFile:@"/Users/darco/Desktop/HamRadio/SSTV/SMPTE_Color_Bars.svg.png"];
//	image = [[NSImage alloc] initWithContentsOfFile:@"/Users/darco/Desktop/HamRadio/SSTV/tv_digital_art_test_pattern_1280x1024_68386.jpg"];
//	image = [[NSImage alloc] initWithContentsOfFile:@"/Users/darco/Desktop/HamRadio/SSTV/test-pattern.jpg"];
//	image = [[NSImage alloc] initWithContentsOfFile:@"/Users/darco/Desktop/HamRadio/SSTV/SSTV-N6DRC-CQ.jpg"];
//	image = [[NSImage alloc] initWithContentsOfFile:@"/Users/darco/Desktop/HamRadio/SSTV/QRCode-2.png"];
//	image = [[NSImage alloc] initWithContentsOfFile:@"/Users/darco/Desktop/HamRadio/SSTV/test-patterns/Zone720-hardedge-A.png"];
//	image = [[NSImage alloc] initWithContentsOfFile:@"/Users/darco/Desktop/HamRadio/SSTV/test-patterns/star-chart-bars-full-600dpi.png"];

//	if(!image)
//		return nil;

	encoder.sampleSize = 4;
//	encoder.useVideoRange = 1;

//	encoder.sampleRate = 6800;
//	encoder.sampleRate = 96000;
	encoder.sampleRate = 8000;
//	encoder.sampleRate = 44100;
//	encoder.sampleSize = 1;


//	[encoder appendImage:image encodedAs:kSSTVFormatBW36];
//	[encoder appendImage:image encodedAs:kSSTVFormatMartin1];
//	[encoder appendImage:image encodedAs:kSSTVFormatScottyDX];
//	[encoder appendImage:image encodedAs:kSSTVFormatBW12];
//	[encoder appendImage:image encodedAs:kSSTVFormatRobot36c];
//	[encoder appendImage:image encodedAs:kSSTVFormatRobot24c];
//	[encoder appendImage:image encodedAs:kSSTVFormatWRASSESC2_180];
//	[encoder appendImage:image encodedAs:kSSTVFormatRobot36c];
//	[encoder appendImage:image encodedAs:kSSTVFormatMartin1];
//	[encoder appendImage:image encodedAs:kSSTVFormatMartin2];
//	[encoder appendImage:image encodedAs:kSSTVFormatBW8];
//	[encoder appendImage:image encodedAs:kSSTVFormatBW12];
//	[encoder appendImage:image encodedAs:kSSTVFormatBW24];
//	[encoder appendImage:image encodedAs:kSSTVFormatBW36];
//	[encoder appendImage:image encodedAs:kSSTVFormatRobot12c];
//	[encoder appendImage:image encodedAs:kSSTVFormatRobot24c];
//	[encoder appendImage:image encodedAs:kSSTVFormatRobot36c];
//	[encoder appendImage:image encodedAs:kSSTVFormatRobot72c];
//	[encoder appendImage:image encodedAs:kSSTVVISCodeWRASSE_SC1_BW8];
//	[encoder appendImage:image encodedAs:kSSTVFormatMartin2];
//	[encoder appendImage:image encodedAs:kSSTVFormatScotty2];

	[encoder appendTestPattern:0 encodedAs:kSSTVFormatRobot12c];

//	[[encoder WAVData] writeToFile:@"/Users/darco/Desktop/HamRadio/SSTV/NewTestPatterns/test.wav" atomically:NO];
//	return nil;
	self.discriminator.sampleRate = encoder.sampleRate;

	return encoder.data;
}

-(void)feedData:(NSData*)data
{
	if (!work_queue) {
		work_queue = dispatch_queue_create("work_queue", 0);
	}
	dispatch_async(work_queue, ^{
		NSInteger i, x = 0;
		NSInteger increment = 10000 * sizeof(float);
		for(i = 0; i < [data length]; i += increment, x++) {
			NSRange range = { i, increment };

			if (range.length + range.location > [data length]) {
				range.length = [data length]-range.location;
			}

			NSData* subset = [data subdataWithRange:range];

			[self.discriminator feedSamples:subset];

			if(self.decoder.decodingImage) {
				usleep(5000);
			} else {
				usleep(1000);
			}
		}

		[self.discriminator feedSamples:NULL count:0];
		[self.decoder refreshImage:self];
		if(self.decoder.decodingImage) {
			[self decoder:self.decoder didFinishImage:self.decoder.image];
		}
		printf("FINISHED!\n");
	});
}

-(void)feedDataFromFile:(NSString*)filename
{
	if (!work_queue) {
		work_queue = dispatch_queue_create("work_queue", 0);
	}
	dispatch_async(work_queue, ^{
		float samples[10000];
		FILE* file = fopen([filename UTF8String], "r");
		if(!file) {
			fprintf(stderr,"Unable to open %s for read\n",[filename UTF8String]);
			return;
		}
		memset(samples,0,sizeof(samples));
//		[self.discriminator feedSamples:samples count:10000];
//		[self.discriminator feedSamples:samples count:10000];

		while(file && !ferror(file) && !feof(file)) {
			size_t count;
			count = fread(samples, sizeof(float), 10000, file);
			[self.discriminator feedSamples:samples count:count];
			if(self.decoder.decodingImage)
				usleep(5000);
			else
				usleep(1000);
		}

		memset(samples,0,sizeof(samples));
//		[self.discriminator feedSamples:samples count:10000];
//		[self.discriminator feedSamples:samples count:10000];
		[self.discriminator feedSamples:NULL count:0];
		[self.decoder refreshImage:self];
		if(self.decoder.decodingImage) {
			[self decoder:self.decoder didFinishImage:self.decoder.image];
		}
		printf("FINISHED!\n");
		fclose(file);
	});
}


- (void)startListening:(id)sender
{
	OSStatus status = -1;
	@try {
		status = AudioQueueStart([self.discriminator audioQueue], NULL);
	}
	@catch (id exception) {
		NSLog(@"Caught exception: %@",exception);
	}

	if(status) {
		NSLog(@"AudioQueueStart %d",status);
	}
}

-(void)applicationDidBecomeActive:(NSNotification *)notification
{
	[self.toolwindow orderWindow:NSWindowBelow relativeTo:self.toolwindow.windowNumber];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
//	[self testNewDiscriminator];
//	[self testNewCorrelator];
//	[self testNewFilter];
	self.decoder.autostartVSync = YES;
#if 0
//	self.discriminator.sampleRate = 44100.0;
	[self startListening:self];
#elif 0
//	self.discriminator.sampleRate = 44100.0;
//	self.decoder.asynchronous = NO;
//	self.decoder.autostartHSync = NO;
//	[self feedDataFromFile:@"/Users/darco/Desktop/HamRadio/SSTV/sstv-8BdDV6EYuW4.raw"];
//	[self feedDataFromFile:@"/Users/darco/Desktop/HamRadio/SSTV/arrisat-sstv.raw"];
//	[self feedDataFromFile:@"/Users/darco/Desktop/HamRadio/SSTV/sstv3_filtered.raw"];
//	[self feedDataFromFile:@"/Users/darco/Desktop/HamRadio/SSTV/classic-sstv.raw"];
//	[self feedDataFromFile:@"/Users/darco/Desktop/HamRadio/SSTV/sstv-robot-12.raw"];
//	[self feedDataFromFile:@"/Users/darco/Desktop/HamRadio/SSTV/test-sstv.raw"];
//	[self feedDataFromFile:@"/Users/darco/Desktop/HamRadio/SSTV/test-sstv-2.raw"];
//	[self feedDataFromFile:@"/Users/darco/Desktop/HamRadio/SSTV/test-sstv-3.raw"];
//	[self feedDataFromFile:@"/Users/darco/Desktop/HamRadio/SSTV/test-sstv-4.raw"];
//	[self feedDataFromFile:@"/Users/darco/Desktop/HamRadio/SSTV/test-sstv-5.raw"];
//	[self feedDataFromFile:@"/Users/darco/Desktop/HamRadio/SSTV/iss.raw"];
//	[self feedDataFromFile:@"/Users/darco/Desktop/HamRadio/SSTV/portal-sstv.raw"]; // 5587.4 150.132
//	[self feedDataFromFile:@"/Users/darco/Desktop/HamRadio/SSTV/overnight-sstv.raw"];
//	[self feedDataFromFile:@"/Users/darco/Desktop/HamRadio/SSTV/overnight-2.raw"];
//	[self feedDataFromFile:@"/Users/darco/Desktop/HamRadio/SSTV/overnight-3.raw"];
//	[self feedDataFromFile:@"/Users/darco/Desktop/HamRadio/SSTV/overnight-4.raw"];
//	[self feedDataFromFile:@"/Users/darco/Desktop/HamRadio/SSTV/unittest-1.raw"];
#else
	NSData* data = nil;

//	self.decoder.asynchronous = NO;
	[self.decoder start:self];
//	self.decoder.visCode = 12;
//	self.decoder.autostartVSync = false;
//	self.decoder.offset = 760.7;
	data = [self generateSamples]; // offset 2085.3

	if(data) {
		[self feedData:data];
	} else {
		[self startListening:self];
	}
#endif
}

@end
