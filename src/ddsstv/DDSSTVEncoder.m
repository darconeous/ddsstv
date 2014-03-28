//
//  DDSSTVEncoder.m
//  SSTV
//
//  Created by Robert Quattlebaum on 10/17/13.
//  Copyright (c) 2013 deepdarc. All rights reserved.
//

#include <dddsp/dddsp.h>
#import "DDSSTVEncoder.h"
#import "ddsstv.h"
#import <math.h>

@implementation DDSSTVEncoder

static bool _dddsp_modulator_output_func(void* context, const float* samples, size_t count)
{
	DDSSTVEncoder* self = (__bridge DDSSTVEncoder*)context;

	if(self.sampleSize == sizeof(*samples)) {
		if (nil == self.data) {
			self.data = [NSMutableData
				dataWithBytes:(void*)samples
				length:sizeof(*samples)*count
			];
		} else {
			[self.data appendBytes:samples length:count*sizeof(*samples)];
		}
	} else {
		NSMutableData* mutable_segment = [NSMutableData dataWithLength:self.sampleSize*count];
		uintptr_t index = 0;
		for(index = 0; index<count; index++) {
			if(self.sampleSize == 1) {
				((uint8_t*)[mutable_segment mutableBytes])[index] = dddsp_decimator_mulaw_feed(&self->decimator, samples[index]);
			} else if(self.sampleSize == 2) {
				((int16_t*)[mutable_segment mutableBytes])[index] = dddsp_decimator_int16_feed(&self->decimator, samples[index]);
			}
		}
		if(self.data)
			[self.data appendData:mutable_segment];
		else
			self.data = mutable_segment;
	}

	return true;
}

-(NSInteger)sampleSize {
	return _sampleSize;
}

-(void)setSampleSize:(NSInteger)sampleSize {
	_sampleSize = sampleSize;
	switch(sampleSize) {
	case 1:
		dddsp_decimator_mulaw_init(&decimator, -1.0, 1.0);
		break;
	case 2:
		dddsp_decimator_int16_init(&decimator, -1.0, 1.0);
		break;
	}
}

-(id)init
{
	if((self = [super init])) {
		self.sampleRate = 44100;
		self.sampleSize = 2;
		dddsp_modulator_init(&modulator);
		modulator.output_func_context =  (__bridge void *)(self);
		modulator.output_func = &_dddsp_modulator_output_func;
	}

	return self;
}

- (NSImage *)imageResize:(NSImage*)anImage newSize:(NSSize)newSize {
    NSImage *sourceImage = anImage;
	bool needsMore = false;

    // Report an error if the source isn't a valid image
    if (![sourceImage isValid])
    {
    NSLog(@"Invalid Image");
    } else
    {
    NSImage *smallImage;

#if 1
	if(newSize.height<anImage.size.height*0.75) {
		needsMore = true;
		smallImage = [[NSImage alloc] initWithSize: NSMakeSize(newSize.width, anImage.size.height*0.75)];
	} else
#endif
	{
		smallImage = [[NSImage alloc] initWithSize: newSize];
	}
    [smallImage lockFocus];
//	[[NSGraphicsContext currentContext] setImageInterpolation:NSImageInterpolationMedium];
	[[NSGraphicsContext currentContext] setImageInterpolation:NSImageInterpolationHigh];
    [[NSGraphicsContext currentContext] setShouldAntialias:YES];
	[sourceImage setSize:smallImage.size];
    [sourceImage setScalesWhenResized:YES];
    [sourceImage drawAtPoint:NSZeroPoint fromRect:CGRectMake(0, 0, smallImage.size.width, smallImage.size.height) operation:NSCompositeCopy fraction:1.0];
    [smallImage unlockFocus];
	if(needsMore) {
		smallImage = [self imageResize:smallImage newSize:newSize];
	}
    return smallImage;
    }
    return nil;
}

-(NSData*)WAVData {
/*
The canonical WAVE format starts with the RIFF header:

0         4   ChunkID          Contains the letters "RIFF" in ASCII form
                               (0x52494646 big-endian form).
4         4   ChunkSize        36 + SubChunk2Size, or more precisely:
                               4 + (8 + SubChunk1Size) + (8 + SubChunk2Size)
                               This is the size of the rest of the chunk
                               following this number.  This is the size of the
                               entire file in bytes minus 8 bytes for the
                               two fields not included in this count:
                               ChunkID and ChunkSize.
8         4   Format           Contains the letters "WAVE"
                               (0x57415645 big-endian form).

The "WAVE" format consists of two subchunks: "fmt " and "data":
The "fmt " subchunk describes the sound data's format:

12        4   Subchunk1ID      Contains the letters "fmt "
                               (0x666d7420 big-endian form).
16        4   Subchunk1Size    16 for PCM.  This is the size of the
                               rest of the Subchunk which follows this number.
20        2   AudioFormat      PCM = 1 (i.e. Linear quantization)
                               Values other than 1 indicate some
                               form of compression.
22        2   NumChannels      Mono = 1, Stereo = 2, etc.
24        4   SampleRate       8000, 44100, etc.
28        4   ByteRate         == SampleRate * NumChannels * BitsPerSample/8
32        2   BlockAlign       == NumChannels * BitsPerSample/8
                               The number of bytes for one sample including
                               all channels. I wonder what happens when
                               this number isn't an integer?
34        2   BitsPerSample    8 bits = 8, 16 bits = 16, etc.
          2   ExtraParamSize   if PCM, then doesn't exist
          X   ExtraParams      space for extra parameters

The "data" subchunk contains the size of the data and the actual sound:

36        4   Subchunk2ID      Contains the letters "data"
                               (0x64617461 big-endian form).
40        4   Subchunk2Size    == NumSamples * NumChannels * BitsPerSample/8
                               This is the number of bytes in the data.
                               You can also think of this as the size
                               of the read of the subchunk following this
                               number.
44        *   Data             The actual sound data.*/
	uint16_t format = 1;
	if(self.sampleSize==4) {
		format = 3;
	}
	if(self.sampleSize==1) {
		format = 7;
	}
	struct wave_header_s {
		char chunkid[4];
		uint32_t chunksize;
		char format[4];

		char subchunkid1[4];
		uint32_t subchunkid1size;
		uint16_t audioformat;
		uint16_t channelcount;
		uint32_t samplerate;
		uint32_t byterate;
		uint16_t blockalign;
		uint16_t bitspersample;
		char subchunkid2[4];
		uint32_t subchunkid2size;
	} header = {
		"RIFF",
		(uint32_t)[self.data length]+36,
		"WAVE",
		"fmt ",
		16,
		format,
		1,
		(uint32_t)self.sampleRate,
		(uint32_t)(self.sampleRate*self.sampleSize),
		1,
		self.sampleSize*8,
		"data",
		(uint32_t)[self.data length]
	};

	NSMutableData* dataret = [NSMutableData dataWithBytes:&header length:sizeof(header)];
	[dataret appendData:self.data];
	return dataret;
}

-(void)appendSilenceForDuration:(double)duration
{
	dddsp_modulator_append_silence(
		&modulator,
		duration*self.sampleRate
	);
}

//double slew_window(double x) {
//	x = sin(x*M_PI/2.0);
//
//	return x;
//}
//
-(void)appendSamplesWithFrequency:(double)frequency forDuration:(double)duration
{
	dddsp_modulator_append_const_freq(
		&modulator,
		duration*self.sampleRate,
		frequency/self.sampleRate,
		0.25
	);
}

-(void)appendVISHeader:(uint8_t)code {

//	[self appendSamplesWithFrequency:1900 forDuration:100*0.001];
//	[self appendSamplesWithFrequency:1500 forDuration:100*0.001];
//	[self appendSamplesWithFrequency:1900 forDuration:100*0.001];
//	[self appendSamplesWithFrequency:1500 forDuration:100*0.001];
//	[self appendSamplesWithFrequency:2300 forDuration:100*0.001];
//	[self appendSamplesWithFrequency:1500 forDuration:100*0.001];
//	[self appendSamplesWithFrequency:2300 forDuration:100*0.001];
//	[self appendSamplesWithFrequency:1500 forDuration:100*0.001];


	[self appendSamplesWithFrequency:1900 forDuration:300*0.001];
	[self appendSamplesWithFrequency:1200 forDuration:10*0.001];
	[self appendSamplesWithFrequency:1900 forDuration:300*0.001];
	[self appendSamplesWithFrequency:1200 forDuration:30*0.001];
	int i,parity=0;
	for(i=7;i;i--,code>>=1) {
		[self appendSamplesWithFrequency:(code&1)?1100:1300 forDuration:30*0.001];
		parity+=(code&1);
	}
	[self appendSamplesWithFrequency:(parity&1)?1100:1300 forDuration:30*0.001];
	[self appendSamplesWithFrequency:1200 forDuration:30*0.001];
}

-(double)valueToFrequency:(double)luma
{
	return luma*(2300-1500)+1500;
}


-(void)appendSamples:(NSArray*)samples forDuration:(double)duration
{
	double sample_time = duration/[samples count];
	for(NSNumber* valueObj in samples) {
		[self appendSamplesWithFrequency:[self valueToFrequency:[valueObj doubleValue]] forDuration:sample_time];
	}
}

-(void)appendScottyImage:(NSImage*)image lineWidth:(double)lineWidth {
	[self appendSamplesWithFrequency:1200 forDuration:9*0.001];

	lineWidth -= (9.0+4.5)*0.001;
	lineWidth /= 3.0;

	NSSize imageSize = NSMakeSize(image.size.width, 256);
	image = [self imageResize:image newSize:imageSize];
	NSBitmapImageRep* imageRep = [[NSBitmapImageRep alloc] initWithData:[image TIFFRepresentation]];
	int line=0;

	for(line=0;line<imageRep.size.height;line++) {
		int col = 0;
		NSMutableArray *colColors = [NSMutableArray array];
		for(col=0; col<imageRep.size.width; col++) {
			double x = col*2;
			double y = line*2;
			NSColor* color = [imageRep colorAtX:x y:y];
			@try {
				[color redComponent];
			} @catch(...) {
				color = [color colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
			}
			[colColors addObject:color];
		}

		// Separator Pulse
		[self appendSamplesWithFrequency:1500 forDuration:1.5*0.001];

		// GREEN
		[self appendSamples:[colColors valueForKey:@"greenComponent"] forDuration:lineWidth];

		// Separator Pulse
		[self appendSamplesWithFrequency:1500 forDuration:1.5*0.001];

		// BLUE
		[self appendSamples:[colColors valueForKey:@"blueComponent"] forDuration:lineWidth];

		// Sync Pulse
		[self appendSamplesWithFrequency:1200 forDuration:9.0*0.001];

		// Sync Porch
		[self appendSamplesWithFrequency:1500 forDuration:1.5*0.001];

		// RED
		[self appendSamples:[colColors valueForKey:@"redComponent"] forDuration:lineWidth];
	}
}

-(void)appendMartinImage:(NSImage*)image lineWidth:(double)lineWidth {
	NSSize imageSize = NSMakeSize(image.size.width, 256);
	image = [self imageResize:image newSize:imageSize];
	NSBitmapImageRep* imageRep = [[NSBitmapImageRep alloc] initWithData:[image TIFFRepresentation]];
	int line=0;

	for(line=0;line<imageRep.size.height;line++) {
		int col = 0;
		NSMutableArray *colColors = [NSMutableArray array];
		for(col=0; col<imageRep.size.width; col++) {
			double x = col*2;
			double y = line*2;
			NSColor* color = [imageRep colorAtX:x y:y];
			@try {
				[color redComponent];
			} @catch(...) {
				color = [color colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
			}
			[colColors addObject:color];
		}

		// Sync Pulse
		[self appendSamplesWithFrequency:1200 forDuration:4.862*0.001];

		// Sync Porch
		[self appendSamplesWithFrequency:1500 forDuration:0.572*0.001];

		// GREEN
		[self appendSamples:[colColors valueForKey:@"greenComponent"] forDuration:lineWidth];

		// Separator Pulse
		[self appendSamplesWithFrequency:1500 forDuration:0.572*0.001];

		// BLUE
		[self appendSamples:[colColors valueForKey:@"blueComponent"] forDuration:lineWidth];

		// Separator Pulse
		[self appendSamplesWithFrequency:1500 forDuration:0.572*0.001];

		// RED
		[self appendSamples:[colColors valueForKey:@"redComponent"] forDuration:lineWidth];

		// Separator Pulse
		[self appendSamplesWithFrequency:1500 forDuration:0.572*0.001];
	}
}

-(void)appendWRASSEImage:(NSImage*)image lineWidth:(double)lineWidth {
	NSSize imageSize = NSMakeSize(image.size.width, 256);
	image = [self imageResize:image newSize:imageSize];
	NSBitmapImageRep* imageRep = [[NSBitmapImageRep alloc] initWithData:[image TIFFRepresentation]];
	int line=0;

	for(line=0;line<imageRep.size.height;line++) {
		int col = 0;
		NSMutableArray *colColors = [NSMutableArray array];
		for(col=0; col<imageRep.size.width; col++) {
			double x = col*2;
			double y = line*2;
			NSColor* color = [imageRep colorAtX:x y:y];
			@try {
				[color redComponent];
			} @catch(...) {
				color = [color colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
			}
			[colColors addObject:color];
		}
		// Sync Pulse
		[self appendSamplesWithFrequency:1200 forDuration:5.5225*0.001];

		// Sync Porch
		[self appendSamplesWithFrequency:1500 forDuration:0.5*0.001];

		// RED
		[self appendSamples:[colColors valueForKey:@"redComponent"] forDuration:lineWidth];
		// GREEN
		[self appendSamples:[colColors valueForKey:@"greenComponent"] forDuration:lineWidth];
		// BLUE
		[self appendSamples:[colColors valueForKey:@"blueComponent"] forDuration:lineWidth];
	}
}

-(void)appendRobotLine:(NSInteger)line
	fromSamples:(const uint8_t*)samples
	sampleCount:(NSInteger)sampleCount
	lineWidth:(double)lineWidth
	syncWidth:(double)syncWidth
	subsampleChroma:(bool)subsampleChroma
{
	const float Yporch = 1500;
	const float Cporch = 1900;
	const double syncPorch = 3.0/1000.0;
	const double porchAlignment = 0.0;
	const NSInteger imageWidth = sampleCount;
	int i;

	lineWidth -= syncWidth + syncPorch;

	// Sync Pulse
	[self appendSamplesWithFrequency:1200 forDuration:syncWidth];

	// Porch
	[self appendSamplesWithFrequency:Yporch forDuration:syncPorch*(1.0-porchAlignment)];

	// Y
	for(i=0; i<sampleCount*3; i+=3) {
		float value = [self valueToFrequency:samples[i]/255.0];
		[self
			appendSamplesWithFrequency:value
			forDuration:lineWidth/imageWidth
		];
	}

	// Porch
	[self appendSamplesWithFrequency:Yporch forDuration:syncPorch*(porchAlignment)];

	if(!subsampleChroma || !(line&1)) {
		// R-Y
		[self appendSamplesWithFrequency:1500 forDuration:(syncWidth)*0.5];
		[self appendSamplesWithFrequency:Cporch forDuration:syncPorch*(1.0-porchAlignment)*0.5];

		for(i=2; i<sampleCount*3; i+=3) {
			float value = [self valueToFrequency:samples[i]/255.0];
			[self
				appendSamplesWithFrequency:value
				forDuration:lineWidth/2.0/imageWidth
			];
		}
		[self appendSamplesWithFrequency:Cporch forDuration:syncPorch*(porchAlignment)*0.5];
	}

	if(!subsampleChroma || (line&1)) {
		// B-Y
		[self appendSamplesWithFrequency:2300 forDuration:(syncWidth)*0.5];
		[self appendSamplesWithFrequency:Cporch forDuration:syncPorch*(1.0-porchAlignment)*0.5];
		for(i=1; i<sampleCount*3; i+=3) {
			float value = [self valueToFrequency:samples[i]/255.0];
			[self
				appendSamplesWithFrequency:value
				forDuration:lineWidth/2.0/imageWidth
			];
		}
		[self appendSamplesWithFrequency:Cporch forDuration:syncPorch*(porchAlignment)*0.5];
	}
}

-(void)appendRobotTestPattern:(int)index
	lineWidth:(double)lineWidth
	syncWidth:(double)syncWidth
	rows:(NSInteger)rows
	subsampleChroma:(bool)subsampleChroma
{
	int line;
	int i;

	const uint8_t blackBar[3] = { 16, 128, 128 };
	uint8_t colorBars100[] = {
		235, 128, 128,
		210, 64, 146,
		170, 166, 16,
		145, 54, 34,
		106, 202, 222,
		81,	90, 240,
		41,	240, 110,
	};
/*
                        601 75% color bars
    white  yellow cyan  green  magenta  red  blue  black
Y    180    162    131   112      84     65    35    16
Cb   128     44    156    72     184    100   212   128
Cr   128    142     44    58     198    212   114   128

                         709 75% color bars
    white  yellow cyan  green  magenta  red  blue  black
Y    180    168    145   133      63     51    28    16
Cb   128     44    147    63     193    109   212   128
Cr   128    136     44    52     204    212   120   128
*/
	uint8_t colorBars75[] = {
		181, 128, 128,
		162, 44, 142,
		131, 156, 44,
		112, 72, 58,
		84, 184, 198,
		65, 100, 212,
		35, 212, 114,
	};
	uint8_t colorBarsRev75[] = {
		35, 212, 114,
		16, 128, 128,
		84, 184, 198,
		16, 128, 128,
		131, 156, 44,
		16, 128, 128,
		181, 128, 128,
	};
	uint8_t lineTest640[640*3];
	uint8_t plugeBars[(7*3)*3];
	uint8_t scaleY[(240-16+32)*3];
	uint8_t scaleCb[(240-16+32)*3];
	uint8_t scaleCr[(240-16+32)*3];

	uint8_t gammaOdd[(3)*3] = {
		16, 128, 128,
		(uint8_t)(16+pow(0.5,1.0/2.2)*(235-16)), 128, 128,
		235, 128, 128,
	};
	uint8_t gammaEven[(3)*3] = {
		235, 128, 128,
		(uint8_t)(16+pow(0.5,1.0/2.2)*(235-16)), 128, 128,
		16, 128, 128,
	};

	for(i=0;i<sizeof(scaleY)/3;i++) {
		if(i<16 || i>sizeof(scaleY)/3-16) {
			scaleY[i*3+0] = 128;
		} else {
			scaleY[i*3+0] = i;
		}

		scaleY[i*3+1] = 128;
		scaleY[i*3+2] = 128;
	}
	for(i=0;i<sizeof(scaleCb)/3;i++) {
		if(i<16 || i>sizeof(scaleCb)/3-16) {
			scaleCb[i*3+0] = 128;
			scaleCb[i*3+1] = 128;
			scaleCb[i*3+2] = 128;
		} else {
			scaleCb[i*3+0] = 128;
			scaleCb[i*3+1] = i;
			scaleCb[i*3+2] = 128;
		}
	}
	for(i=0;i<sizeof(scaleCb)/3;i++) {
		if(i<16 || i>sizeof(scaleCb)/3-16) {
			scaleCr[i*3+0] = 128;
			scaleCr[i*3+1] = 128;
			scaleCr[i*3+2] = 128;
		} else {
			scaleCr[i*3+0] = 128;
			scaleCr[i*3+1] = 128;
			scaleCr[i*3+2] = i;
		}
	}

	// Calculate the line test
	for(i=0;i<sizeof(lineTest640)/3;i++) {
		int mask = 1;


		lineTest640[i*3+0] = (i&mask)?16:235;
		lineTest640[i*3+1] = 128;
		lineTest640[i*3+2] = 128;
	}

	// Calculate the pluge bars
	for(i=0;i<sizeof(plugeBars)/3;i++) {
		if(i/(3)>=6) {
			plugeBars[i*3+0] = 16;
			plugeBars[i*3+1] = 128;
			plugeBars[i*3+2] = 128;
		} else if(i/(3)>=5) {
			switch(i%3) {
			case 0: plugeBars[i*3+0] = 7; break;
			case 1: plugeBars[i*3+0] = 16; break;
			case 2: plugeBars[i*3+0] = 25; break;
			}
			plugeBars[i*3+1] = 128;
			plugeBars[i*3+2] = 128;
		} else if(i/(4)>=3) {
			plugeBars[i*3+0] = 16;
			plugeBars[i*3+1] = 128;
			plugeBars[i*3+2] = 128;
		} else if(i/(4)>=2) {
			// "Q"*0.2
			plugeBars[i*3+0] = 16;
			plugeBars[i*3+1] = 171;
			plugeBars[i*3+2] = 148;
		} else if(i/(4)>=1) {
			// "WHITE"
			plugeBars[i*3+0] = 235;
			plugeBars[i*3+1] = 128;
			plugeBars[i*3+2] = 128;
		} else {
			// "-I"*0.2
			plugeBars[i*3+0] = 16;
			plugeBars[i*3+1] = 156;
			plugeBars[i*3+2] = 97;
		}
	}

#define DO_LINE_COUNT(x,c) 				\
	[self \
		appendRobotLine:line \
		fromSamples:x \
		sampleCount:c \
		lineWidth:lineWidth \
		syncWidth:syncWidth \
		subsampleChroma:subsampleChroma \
	]

#define DO_LINE(x) 	DO_LINE_COUNT(x,sizeof(x)/3)


	if(index == 0) {
		for(line=0;line<rows;line++) {
			if(line < ((int)(rows*0.65)&~1)) {
				DO_LINE(colorBars75);
			} else if(line < ((int)(rows*0.75)&~1)) {
				DO_LINE(colorBarsRev75);
			} else {
				DO_LINE(plugeBars);
			}
		}
	} else if(index == 1) {
		for(line=0;line<rows;line++) {
			if(line < ((int)(rows*(8.0/240.0))&~1)) {
				DO_LINE(blackBar);
			} else if(line < ((int)(rows*((8.0+16.0*1)/240.0))&~1)) {
				DO_LINE_COUNT(lineTest640, 640/8);
			} else if(line < ((int)(rows*((8.0+16.0*2)/240.0))&~1)) {
				DO_LINE_COUNT(lineTest640, 640/4);
			} else if(line < ((int)(rows*((8.0+16.0*3)/240.0))&~1)) {
				DO_LINE_COUNT(lineTest640, 640/2);
			} else if(line < ((int)(rows*((8.0+16.0*4)/240.0))&~1)) {
				DO_LINE_COUNT(lineTest640, 640/1);
			} else if(line < ((int)(rows*((0.0+16.0*5)/240.0))&~1)) {
				DO_LINE(blackBar);
			} else if(line < ((int)(rows*((0.0+16.0*6)/240.0))&~1)) {
				DO_LINE_COUNT(lineTest640, 512/8);
			} else if(line < ((int)(rows*((0.0+16.0*7)/240.0))&~1)) {
				DO_LINE_COUNT(lineTest640, 512/4);
			} else if(line < ((int)(rows*((0.0+16.0*8)/240.0))&~1)) {
				DO_LINE_COUNT(lineTest640, 512/2);
			} else if(line < ((int)(rows*((0.0+16.0*9)/240.0))&~1)) {
				DO_LINE_COUNT(lineTest640, 512/1);
			} else {
				DO_LINE(colorBars75);
			}
		}
	}

}

-(void)appendRobotImage:(NSImage*)image
	lineWidth:(double)lineWidth
	syncWidth:(double)syncWidth
	rows:(NSInteger)rows
	subsampleChroma:(bool)subsampleChroma
{
	NSSize imageSize = NSMakeSize(image.size.width, rows);
	image = [self imageResize:image newSize:imageSize];
	NSBitmapImageRep* imageRep = [[NSBitmapImageRep alloc] initWithData:[image TIFFRepresentation]];
	int line=0;
	const double yccChroma = (240.0-16.0)/255.0;
	const double yccChromaCenter = 0.5;
	const double yccLumaCenter = ((235.0-16.0)/2.0+16.0)/255.0;
	const double yccContrast = (235.0-16.0)/255.0;
	NSMutableArray *colColors = NULL;
	NSMutableArray *colColors2 = NULL;

	uint8_t *lineBuffer = malloc(image.size.width*3);

	for(line=0;line<imageRep.size.height;line++) {
		int col = 0;
		if(!subsampleChroma) {
			colColors = [NSMutableArray array];
			colColors2 = [NSMutableArray array];
			for(col=0; col<imageRep.size.width; col++) {
				double x = col*2;
				double y = line*2;
				NSColor* color = [imageRep colorAtX:x y:y];
				@try {
					[color redComponent];
				} @catch(...) {
					color = [color colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
				}
				[colColors addObject:color];
			}
		} else {
			if(!(line&1)) {
				colColors = [NSMutableArray array];
				colColors2 = [NSMutableArray array];
				for(col=0; col<imageRep.size.width; col++) {
					double x = col*2.0;
					double y = line*2.0;
					NSColor* color = [imageRep colorAtX:x y:y];
					@try {
						[color redComponent];
					} @catch(...) {
						color = [color colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
					}
					[colColors addObject:color];
				}
				for(col=0; col<imageRep.size.width; col++) {
					double x = col*2.0;
					double y = (line+1.0)*2.0;
					NSColor* color = [imageRep colorAtX:x y:y];
					@try {
						[color redComponent];
					} @catch(...) {
						color = [color colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
					}
					[colColors2 addObject:color];
				}
			} else {
				NSMutableArray *tmp = colColors2;
				colColors2 = colColors;
				colColors = tmp;
			}
		}

		for(col=0; col<imageRep.size.width; col++) {
			NSColor* pixel = [colColors objectAtIndex:col];
			double Y = 0, Cb = 0, Cr = 0;
			double red = [pixel redComponent];
			double green = [pixel greenComponent];
			double blue = [pixel blueComponent];
			Y = red*0.299 + green*0.587 + blue*0.114;
			Y -= 0.5;
			Y *= yccContrast;
			Y += yccLumaCenter;

			lineBuffer[col*3+0] = (uint8_t)(Y*255.0 + 0.5);

			if(subsampleChroma) {
				pixel = [colColors2 objectAtIndex:col];
				double gamma = 2.2;
				red = pow(red,gamma);
				green = pow(green,gamma);
				blue = pow(blue,gamma);
				red = (red+pow([pixel redComponent],gamma))*0.5;
				green = (green+pow([pixel greenComponent],gamma))*0.5;
				blue = (blue+pow([pixel blueComponent],gamma))*0.5;
				gamma = 1.0/gamma;
				red = pow(red,gamma);
				green = pow(green,gamma);
				blue = pow(blue,gamma);
			}

			Y = red*0.299 + green*0.587 + blue*0.114;

			Cb = blue - Y;
			Cb *= 0.5;
			Cb *= yccChroma;
			Cb += yccChromaCenter;

			Cr = red - Y;
			Cr *= 0.5;
			Cr *= yccChroma;
			Cr += yccChromaCenter;

			lineBuffer[col*3+1] = (uint8_t)(Cb*255.0 + 0.5);
			lineBuffer[col*3+2] = (uint8_t)(Cr*255.0 + 0.5);
		}

		[self
			appendRobotLine:line
			fromSamples:lineBuffer
			sampleCount:imageRep.size.width
			lineWidth:lineWidth
			syncWidth:syncWidth
			subsampleChroma:subsampleChroma
		];
	}

	free(lineBuffer);
}


-(void)appendBWImage:(NSImage*)image imageSize:(NSSize)imageSize lineWidth:(double)lineWidth syncWidth:(double)syncWidth porchWidth:(double)porchWidth{
#if 1
	imageSize = NSMakeSize(image.size.width, imageSize.height);
#endif
	image = [self imageResize:image newSize:imageSize];
	NSBitmapImageRep* imageRep = [[NSBitmapImageRep alloc] initWithData:[image TIFFRepresentation]];
	int line=0;

	double syncPorch = porchWidth;
	lineWidth -= syncWidth + syncPorch;

//	[self appendSamplesWithFrequency:2300 forDuration:1.0*0.001];
//	[self appendSamplesWithFrequency:1500 forDuration:1.0*0.001];
//
//	[self appendSamplesWithFrequency:1200 forDuration:syncWidth];
//	[self appendSamplesWithFrequency:1500 forDuration:lineWidth];
//	[self appendSamplesWithFrequency:1200 forDuration:syncWidth];
//	[self appendSamplesWithFrequency:1500 forDuration:lineWidth];

	for(line=0;line<imageRep.size.height;line++) {
		int col = 0;
		NSMutableArray *colColors = [NSMutableArray array];
		for(col=0; col<imageRep.size.width; col++) {
			double x = col*2;
			double y = line*2;
			NSColor* color = [imageRep colorAtX:x y:y];
			@try {
				[color redComponent];
			} @catch(...) {
				color = [color colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
			}
			[colColors addObject:color];
		}
		// Sync Pulse
		[self appendSamplesWithFrequency:1200 forDuration:syncWidth];

		// Porch
		[self appendSamplesWithFrequency:1500 forDuration:syncPorch];

		// Y
		for(NSColor* pixel in colColors) {
			double value = 0;
			@try {
				double red = [pixel redComponent];
				double green = [pixel greenComponent];
				double blue = [pixel blueComponent];
				value = red*0.299 + green*0.587 + blue*0.114;
			} @catch(...) {
				value = [pixel brightnessComponent];
			}
			[self appendSamplesWithFrequency:[self valueToFrequency:value] forDuration:lineWidth/imageRep.size.width];
		}
	}

//	[self appendSamplesWithFrequency:1200 forDuration:syncWidth];
//	[self appendSamplesWithFrequency:1500 forDuration:lineWidth];
}

//enum {
//	kSSTVFormatScotty1 = 60,
//	kSSTVFormatScotty2 = 56,
//	kSSTVFormatScottyDX = 76,
//
//	kSSTVFormatMartin1 = 44,
//	kSSTVFormatMartin2 = 40,
//
//	kSSTVFormatWRASSESC2_180 = 55,
//
//	kSSTVFormatRobot72c = 12,
//	kSSTVFormatRobot32c = 8,
//
//	kSSTVFormatBW8 = 2,
//};

-(void)appendTestPattern:(int)index encodedAs:(uint8_t)code
{
	[self appendSilenceForDuration:1.0];
	[self appendVISHeader:code];
	switch(code) {
		case kSSTVFormatRobot12c:
			[self
				appendRobotTestPattern:index
				lineWidth:66.6667*0.001
				syncWidth:5.0*0.001
				rows:120
				subsampleChroma:YES
			];
			break;
		case kSSTVFormatRobot24c:
			[self
				appendRobotTestPattern:index
				lineWidth:100.0*0.001
				syncWidth:5.0*0.001
				rows:120
				subsampleChroma:NO
			];
			break;
		case kSSTVFormatRobot36c:
			[self
				appendRobotTestPattern:index
				lineWidth:100.0*0.001
				syncWidth:9.0*0.001
				rows:240
				subsampleChroma:YES
			];
			break;
		case kSSTVFormatRobot72c:
			[self
				appendRobotTestPattern:index
				lineWidth:150.0*0.001
				syncWidth:9.0*0.001
				rows:240
				subsampleChroma:NO
			];
			break;


#if 0
		case kSSTVFormatScotty1: [self appendScottyImage:image lineWidth:428.223*0.001]; break;
		case kSSTVFormatScotty2: [self appendScottyImage:image lineWidth:277.6920*0.001]; break;
		case kSSTVFormatScottyDX: [self appendScottyImage:image lineWidth:1050.3*0.001]; break;

		case kSSTVFormatMartin1: [self appendMartinImage:image lineWidth:0.4576*0.001*320]; break;
		case kSSTVFormatMartin2: [self appendMartinImage:image lineWidth:0.2288*0.001*320]; break;

		case kSSTVFormatWRASSESC2_180: [self appendWRASSEImage:image lineWidth:0.7344*0.001*320]; break;

		case kSSTVFormatRobot12c: [self appendRobotImage:image lineWidth:66.6667*0.001 syncWidth:5.0*0.001 rows:120 subsampleChroma:YES]; break;
		case kSSTVFormatRobot24c: [self appendRobotImage:image lineWidth:100.0*0.001 syncWidth:5.0*0.001 rows:120 subsampleChroma:NO]; break;

		case kSSTVFormatRobot36c: [self appendRobotImage:image lineWidth:100.0*0.001 syncWidth:9.0*0.001 rows:240 subsampleChroma:YES]; break;
		case kSSTVFormatRobot72c: [self appendRobotImage:image lineWidth:150.0*0.001 syncWidth:9.0*0.001 rows:240 subsampleChroma:NO]; break;

		case kSSTVVISCodeWRASSE_SC1_BW8: [self appendBWImage:image imageSize:NSMakeSize(120,120) lineWidth:60.0/1000.0 syncWidth:5.0*0.001 porchWidth:1.0*0.001]; break;
		case kSSTVVISCodeClassic8: [self appendBWImage:image imageSize:NSMakeSize(120,120) lineWidth:60.0/900.0 syncWidth:5.0*0.001 porchWidth:1.0*0.001]; break;

		case kSSTVFormatBW8: [self appendBWImage:image imageSize:NSMakeSize(160,120) lineWidth:(66.666666)*0.001 syncWidth:5.0*0.001 porchWidth:(5.0/3.0)*0.001]; break;
		case kSSTVFormatBW12: [self appendBWImage:image imageSize:NSMakeSize(160,120) lineWidth:100.0*0.001 syncWidth:5.0*0.001 porchWidth:5.0/3.0*0.001]; break;
		case kSSTVFormatBW24: [self appendBWImage:image imageSize:NSMakeSize(320,240) lineWidth:100.0*0.001 syncWidth:9.0*0.001 porchWidth:9.0/3.0*0.001]; break;
		case kSSTVFormatBW36: [self appendBWImage:image imageSize:NSMakeSize(320,240) lineWidth:150.0*0.001 syncWidth:9.0*0.001 porchWidth:9.0/3.0*0.001]; break;
#endif
		default:
			break;
	}
	//[self appendSamplesWithFrequency:1900 forDuration:1.0*0.005];

	[self appendSilenceForDuration:2.5];
}

-(void)appendImage:(NSImage*)image encodedAs:(uint8_t)code
{
	[self appendSilenceForDuration:1.0];
	[self appendVISHeader:code];
	switch(code) {
		case kSSTVFormatScotty1: [self appendScottyImage:image lineWidth:428.223*0.001]; break;
		case kSSTVFormatScotty2: [self appendScottyImage:image lineWidth:277.6920*0.001]; break;
		case kSSTVFormatScottyDX: [self appendScottyImage:image lineWidth:1050.3*0.001]; break;

		case kSSTVFormatMartin1: [self appendMartinImage:image lineWidth:0.4576*0.001*320]; break;
		case kSSTVFormatMartin2: [self appendMartinImage:image lineWidth:0.2288*0.001*320]; break;

		case kSSTVFormatWRASSESC2_180: [self appendWRASSEImage:image lineWidth:0.7344*0.001*320]; break;

		case kSSTVFormatRobot12c: [self appendRobotImage:image lineWidth:66.6667*0.001 syncWidth:5.0*0.001 rows:120 subsampleChroma:YES]; break;
		case kSSTVFormatRobot24c: [self appendRobotImage:image lineWidth:100.0*0.001 syncWidth:5.0*0.001 rows:120 subsampleChroma:NO]; break;

		case kSSTVFormatRobot36c: [self appendRobotImage:image lineWidth:100.0*0.001 syncWidth:9.0*0.001 rows:240 subsampleChroma:YES]; break;
		case kSSTVFormatRobot72c: [self appendRobotImage:image lineWidth:150.0*0.001 syncWidth:9.0*0.001 rows:240 subsampleChroma:NO]; break;

		case kSSTVVISCodeWRASSE_SC1_BW8: [self appendBWImage:image imageSize:NSMakeSize(120,120) lineWidth:60.0/1000.0 syncWidth:5.0*0.001 porchWidth:1.0*0.001]; break;
		case kSSTVVISCodeClassic8: [self appendBWImage:image imageSize:NSMakeSize(120,120) lineWidth:60.0/900.0 syncWidth:5.0*0.001 porchWidth:1.0*0.001]; break;

		case kSSTVFormatBW8: [self appendBWImage:image imageSize:NSMakeSize(160,120) lineWidth:(66.666666)*0.001 syncWidth:5.0*0.001 porchWidth:(5.0/3.0)*0.001]; break;
		case kSSTVFormatBW12: [self appendBWImage:image imageSize:NSMakeSize(160,120) lineWidth:100.0*0.001 syncWidth:5.0*0.001 porchWidth:5.0/3.0*0.001]; break;
		case kSSTVFormatBW24: [self appendBWImage:image imageSize:NSMakeSize(320,240) lineWidth:100.0*0.001 syncWidth:9.0*0.001 porchWidth:9.0/3.0*0.001]; break;
		case kSSTVFormatBW36: [self appendBWImage:image imageSize:NSMakeSize(320,240) lineWidth:150.0*0.001 syncWidth:9.0*0.001 porchWidth:9.0/3.0*0.001]; break;
		default:
			break;
	}
	//[self appendSamplesWithFrequency:1900 forDuration:1.0*0.005];

	[self appendSilenceForDuration:2.5];
}

@end
