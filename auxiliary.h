#pragma once

#include<ctype.h>
#include<stdarg.h>

#define print_error(format,...) fprintf(stderr,"\033[31m" format "\033[39m", __VA_ARGS__)

static const char * strupr(const char * c){
	char * s = (char*) c;

	while(*s){
		*s = toupper(*s);
		s++;
	}
	return c;
}

static const char * strlwr(const char * c){
	char * s = (char*) c;

	while(*s){
		*s = tolower(*s);
		s++;
	}
	return c;
}
