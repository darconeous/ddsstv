#ifndef __g711h__
#define __g711h__

extern unsigned char linear2alaw(int pcm_val);	/* 2's complement (16-bit range) */
extern int alaw2linear(unsigned char a_val);
extern unsigned char linear2ulaw(int pcm_val);	/* 2's complement (16-bit range) */
extern int ulaw2linear(unsigned char u_val);
extern unsigned char alaw2ulaw(unsigned char aval);
extern unsigned char ulaw2alaw(unsigned char uval);

#endif
