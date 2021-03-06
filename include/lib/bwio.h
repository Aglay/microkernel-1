#ifndef __BWIO_H__
#define __BWIO_H__

#define DBG_FATAL	 	0	// panic and bwassert only
#define DBG_USR		 	1	// user tasks 
#define DBG_SYS		 	2	// sytem/user tasks
#define DBG_REQ		 	3	// kernel requests + sytem/user tasks
#define DBG_KERN 		4	// all

#define COM1	0
#define COM2	1
#define COM3	2

#define ON		1
#define	OFF		0

int bwsetenabled( int channel, int state );

int bwsetfifo( int channel, int state );

int bwsetspeed( int channel, int speed );

int bwputc( int channel, char c );

int bwgetc( int channel );

int bwputx( int channel, char c );

int bwputstr( int channel, char *str );

int bwputr( int channel, unsigned int reg );

void bwputw( int channel, int n, char fc, char *bf );

void bwprintf( int channel, char *format, ... );

void bwdebug( int threshhold_lvl, int debug_area, char *message, ... );

void bwassert( int condition, char * message, ... );

void bwpanic( char * message, ... );


void todo_debug( int value, int channel ); 

#endif
