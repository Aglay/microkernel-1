#include "lib/bwio.h"
#include "kernel/helpers.h"
#include "config/ts7200.h"


void assert( int statement, char * err_msg ) {
	if( !statement ) wbprintf( COM2, "Assertion failed: %s\n\r", err_msg );
}

void panic( char * panic_reason ) {
	wbprintf( COM2, "Kernel panic: %s\n\r", panic_reason );
	for( ; ; );
}
