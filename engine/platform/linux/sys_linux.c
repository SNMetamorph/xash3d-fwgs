/*
sys_linux.c - Linux system utils
Copyright (C) 2018 a1batross

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include <time.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>
#include "platform/platform.h"

static double frametime;
static pthread_mutex_t tmutex;
static qboolean flag;
static pthread_t timer;
static pthread_cond_t tsignal;

static void *Sys_TimerThread( void *v )
{
	struct timespec time, time1, time2, time3 = { 0,0 };
	time.tv_sec = 0;
	time.tv_nsec = frametime * 1e9;
	while (1)
	{

		long long tdiff;
		nanosleep(&time, &time1);
		//printf("%ld\n", time1.tv_nsec );
		time1.tv_sec = 0;
		time1.tv_nsec = 0;
		clock_gettime(CLOCK_MONOTONIC, &time2);
		tdiff = time2.tv_nsec - time3.tv_nsec - frametime * 1e9;
		if (tdiff < 0)
			tdiff = 0;
		if (tdiff > frametime / 2.0 * 1e9)
			tdiff = 0;

		pthread_mutex_lock(&tmutex);

		time.tv_nsec = 1e9 * frametime - tdiff;
		if (flag == 0)
			pthread_cond_signal(&tsignal);
		flag = 1;
		pthread_mutex_unlock(&tmutex);
		time3 = time2;
	}
}

void Platform_Delay( double time )
{
	if (flag)
	{
		flag = 0;
		return;
	}

	pthread_mutex_lock(&tmutex);
	frametime = time;
	while (!flag)
		pthread_cond_wait(&tsignal, &tmutex);
	flag = 0;
	pthread_mutex_unlock(&tmutex);
}

void Platform_TimerInit( void )
{
	flag = 1;
	tmutex = PTHREAD_MUTEX_INITIALIZER;
	tsignal = PTHREAD_COND_INITIALIZER;
	frametime = 1.0 / host_maxfps->value;
	pthread_create(&timer, NULL, Sys_TimerThread, NULL);
}

qboolean Sys_DebuggerPresent( void )
{
	char buf[4096];
	ssize_t num_read;
	int status_fd;

	status_fd = open( "/proc/self/status", O_RDONLY );
	if ( status_fd == -1 )
		return 0;

	num_read = read( status_fd, buf, sizeof( buf ) );
	close( status_fd );

	if ( num_read > 0 )
	{
		static const char TracerPid[] = "TracerPid:";
		const byte *tracer_pid;

		buf[num_read] = 0;
		tracer_pid    = (const byte*)Q_strstr( buf, TracerPid );
		if( !tracer_pid )
			return false;
		//printf( "%s\n", tracer_pid );
		while( *tracer_pid < '0' || *tracer_pid > '9'  )
			if( *tracer_pid++ == '\n' )
				return false;
		//printf( "%s\n", tracer_pid );
		return !!Q_atoi( (const char*)tracer_pid );
	}

	return false;
}
