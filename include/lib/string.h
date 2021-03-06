#ifndef __STRING_H__
#define __STRING_H__

void strcpy( const char * , char * );

int strcmp( const char *s1, const char *s2 );

char *substr( char *dest, const char *src, int initial_pos, int size ); 

int strlen( const char *str ); 

int append_char( char *str, char c, int num ); 

#endif
