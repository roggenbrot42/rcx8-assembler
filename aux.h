#pragma once

#include<ctype.h>


const char * strupr(const char * c){
	char * s = (char*) c;

	while(*s){
		*s = toupper(*s);
		s++;
	}
	return c;
}

const char * strlwr(const char * c){
	char * s = (char*) c;

	while(*s){
		*s = tolower(*s);
		s++;
	}
	return c;
}