/**
 * @file mbbridge.h
 *
 */

#ifndef MBBRIDGE_H
#define MBBRIDGE_H

//#include <time.h>

struct updatecycle {
	int	ident;
	int interval;	// seconds
	int *tagArray = NULL;
	int tagArraySize = 0;
	time_t nextUpdateTime;			// next update time 
};


#endif /* MBBRIDGE_H */
