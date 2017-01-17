//
//  dddsp-correlator.c
//  SSTV
//
//  Created by Robert Quattlebaum on 3/27/14.
//  Copyright (c) 2014 deepdarc. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dddsp-correlator.h"
#include "dddsp-filter.h"

#define DDDSP_CORRELATOR_DEBUG 0
#if DDDSP_CORRELATOR_DEBUG
#define CORR_DPRINTF	printf
#else
#define CORR_DPRINTF(...)	do { } while(0)
#endif




dddsp_correlator_t
dddsp_correlator_alloc(const struct dddsp_correlator_box_s* boxes, int box_count)
{
	dddsp_correlator_t self = NULL;

	if(box_count<=0)
		goto bail;

	int min_s = boxes[0].offset, max_s = boxes[0].offset+boxes[0].width;

	for(int i=0;i<box_count;i++) {
		int width = boxes[i].width;
		int offset = boxes[i].offset;
		if(!width && (i+1)<box_count)
			width = boxes[i+1].offset-offset;
		if(!offset && (i-1)>=0)
			offset = boxes[i-1].offset+width;
		if (min_s > offset)
			min_s = offset;
		if (max_s < offset+width)
			max_s = offset+width;
	}

	if (max_s - min_s <= 0)
		goto bail;

	self = calloc(sizeof(*self),1);

	if (!self)
		goto bail;

	self->box_count = box_count;
	self->box = calloc(sizeof(*self->box),self->box_count);
	memcpy(self->box, boxes, sizeof(*self->box)*self->box_count);

	for(int i=0;i<box_count;i++) {
		if(!self->box[i].width && (i+1)<box_count) {
			self->box[i].width = self->box[i+1].offset-self->box[i].offset;
		}
		if(!self->box[i].offset && (i-1)>=0) {
			self->box[i].offset = self->box[i-1].offset+self->box[i].width;
		}
		self->box[i].offset -= max_s;
		self->box[i].sum = 0;
		self->box[i].count = 0;
	}

	while(self->box_count && !self->box[self->box_count-1].width)
		self->box_count--;

	self->buffer_size = (max_s - min_s)+1;
	self->buffer = calloc(sizeof(*self->buffer),self->buffer_size);

	dddsp_correlator_reset(self);

bail:
	return self;
}

void
dddsp_correlator_reset(dddsp_correlator_t self)
{
	for(int i=0;i<self->box_count;i++) {
		self->box[i].sum = 0;
		self->box[i].count = 0;
	}
	memset(self->buffer,0,sizeof(self->buffer_size)*sizeof(*self->buffer));
	self->next_sample = 0;
	printf("CORRELATOR RESET\n");
}

void
dddsp_correlator_finalize(dddsp_correlator_t self)
{
	if(self) {
		free(self->box);
		free(self->buffer);
		free(self);
	}
}

int32_t
dddsp_correlator_get_box_avg(dddsp_correlator_t self, int box_index)
{
	int32_t ret = DDDSP_UNCORRELATED;

	if(self && box_index<self->box_count) {
		ret = self->box[box_index].sum/self->box[box_index].count;
	}

	return ret;
}

int32_t
dddsp_correlator_feed(dddsp_correlator_t self, int32_t sample)
{
	if(self) {
		int i = self->next_sample++ % self->buffer_size;

		// Add sample to ring buffer
		self->buffer[i] = sample;

		CORR_DPRINTF("[%d]: input: %d\n",i,sample);

#define SAMPLE_OFFSET(x)	((self->next_sample+(x)+self->buffer_size-1)%self->buffer_size)

		// Update boxes.
		for(i=0;i<self->box_count;i++) {
			if(self->next_sample + self->box[i].offset + self->box[i].width > 0)
			{
				int buffer_index = SAMPLE_OFFSET(self->box[i].offset + self->box[i].width);
				int32_t freq = self->buffer[buffer_index];
				if (freq!=DDDSP_UNCORRELATED) {
					self->box[i].sum += freq;
					self->box[i].count++;
					CORR_DPRINTF("\tAdding    %4d from sample %d to   box %d (count      is %d, sum:%d)\n",freq,buffer_index,i,self->box[i].count,self->box[i].sum);
				} else {
					CORR_DPRINTF("\tWTF ADD sample %d,box %d\n",SAMPLE_OFFSET(self->box[i].offset + self->box[i].width),i);
				}
			} else {
				CORR_DPRINTF("\tSkipping adding  of sample %d to   box %d (count remains %d, sum:%d)\n",SAMPLE_OFFSET(self->box[i].offset + self->box[i].width),i,self->box[i].count,self->box[i].sum);
			}
			if(self->next_sample + self->box[i].offset > 0)
			{
				int buffer_index = SAMPLE_OFFSET(self->box[i].offset);
				int32_t freq = self->buffer[buffer_index];
				if (freq!=DDDSP_UNCORRELATED) {
					self->box[i].sum -= freq;
					self->box[i].count--;
					CORR_DPRINTF("\tRemoving  %4d from sample %d from box %d (count      is %d, sum:%d)\n",freq,buffer_index,i,self->box[i].count,self->box[i].sum);

					if(self->box[i].count < 0) {
						self->box[i].sum = 0;
						self->box[i].count = 0;
					}
				} else {
					CORR_DPRINTF("\tWTF REMOVE sample %d,box %d\n",SAMPLE_OFFSET(self->box[i].offset),i);
				}
			} else {
				CORR_DPRINTF("\tSkipping removal of sample %d from box %d (count remains %d, sum:%d)\n",SAMPLE_OFFSET(self->box[i].offset),i,self->box[i].count,self->box[i].sum);
			}
		}

#if DDDSP_CORRELATOR_DEBUG
		// Debug Boxes.
		for(i=0;i<self->box_count;i++) {
			CORR_DPRINTF("\tbox[%d] count:%d sum:%d avg:%d\n",i,self->box[i].count,self->box[i].sum,self->box[i].count?self->box[i].sum/self->box[i].count:0);
		}
#endif

		// Calculate score.
		sample = DDDSP_UNCORRELATED;
		int score = 0;
		int total_samples = 0, total_uncorrelated = 0;

		for(i=0; i<self->box_count && (self->box[i].count>=(self->box[i].width*3/4+1));i++) {
			int32_t freq = (self->box[i].sum/self->box[i].count);
			total_samples += self->box[i].width;
			total_uncorrelated += self->box[i].width-self->box[i].count;
			switch(self->box[i].type) {
			case DDDSP_BOX_DEFAULT:
			default:
				freq -= self->box[i].expect;
				if (0 < freq)
					freq = -freq;
				freq += self->box[i].threshold;
				break;
			case DDDSP_BOX_LESS_THAN:
				freq -= self->box[i].expect;
				freq = -freq;
				if (0 >= freq)
					freq = 100;
				freq += self->box[i].threshold;
				break;
			case DDDSP_BOX_GREATER_THAN:
				freq -= self->box[i].expect;
				if (0 >= freq)
					freq = 100;
				freq += self->box[i].threshold;
				break;
			case DDDSP_BOX_SKIP:
				freq = 0;
				break;
			case DDDSP_BOX_BETWEEN:
				if((freq>self->box[i].expect-self->box[i].threshold)
				 && (freq<self->box[i].expect2+self->box[i].threshold)
				) {
					freq = 100;
					break;
				}
			case DDDSP_BOX_DUAL:
				{
					int32_t freq2 = freq;

					freq -= self->box[i].expect;
					if (0 < freq)
						freq = -freq;

					freq2 -= self->box[i].expect2;
					if (0 < freq2)
						freq2 = -freq2;

					if (freq2 > freq)
						freq = freq2;
					freq += self->box[i].threshold;
				}
				break;
			}
			if (freq<0)
				freq *= 4;
			score += freq * self->box[i].width;
		}

		CORR_DPRINTF("\t%d (%d)\n",i, score);

		if (i == self->box_count && (total_uncorrelated<=total_samples/10))
			sample = score;
	} else {
		sample = DDDSP_UNCORRELATED;
	}
	return sample;
}
