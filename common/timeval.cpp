/* from the GNU libc manual */

#include "config.h"

#ifdef __linux__
#include <sys/time.h>
#endif

#include <time.h>

#include "common/timeval.h"

/* Subtract the `struct timeval' values X and Y,
   storing the result in RESULT.
   Return 1 if the difference is negative, otherwise 0.  */
     
int
timeval_subtract(struct timeval &result, const struct timeval &x, const struct timeval &y_)
{
	timeval y = y_;
	// Perform the carry for the later subtraction by updating Y
	if (x.tv_usec < y.tv_usec) 
	{
		int nsec = (y.tv_usec - x.tv_usec) / 1000000 + 1;
		y.tv_usec -= 1000000 * nsec;
		y.tv_sec += nsec;
	}
	if (x.tv_usec - y.tv_usec > 1000000) 
	{
		int nsec = (x.tv_usec - y.tv_usec) / 1000000;
		y.tv_usec += 1000000 * nsec;
		y.tv_sec -= nsec;
	}

	// Compute the time remaining to wait. tv_usec is certainly positive.
	result.tv_sec = x.tv_sec - y.tv_sec;
	result.tv_usec = x.tv_usec - y.tv_usec;

	// Return 1 if result is negative
	return x.tv_sec < y.tv_sec;
}
