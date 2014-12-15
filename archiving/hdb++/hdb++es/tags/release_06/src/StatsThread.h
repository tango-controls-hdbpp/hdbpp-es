//=============================================================================
//
// file :        StatsThread.h
//
// description : Include for the StatsThread.
//
// project :	Tango Device Server
//
// $Author: graziano $
//
// $Revision: 1.5 $
//
// $Log: StatsThread.h,v $
//
//
//
// copyleft :    European Synchrotron Radiation Facility
//               BP 220, Grenoble 38043
//               FRANCE
//
//=============================================================================

#ifndef _STATS_THREAD_H
#define _STATS_THREAD_H

#include <tango.h>
#include <eventconsumer.h>
#include <stdint.h>

/**
 * @author	$Author: graziano $
 * @version	$Revision: 1.5 $
 */

 //	constants definitions here.
 //-----------------------------------------------

namespace HdbEventSubscriber_ns
{

//=========================================================
/**
 *	Create a thread retry to subscribe event.
 */
//=========================================================
class StatsThread: public omni_thread
{
private:
	/**
	 *	HdbDevice object
	 */
	HdbDevice	*hdb_dev;


public:
	int			period;
	bool		abortflag;
	timeval		last_stat;
	StatsThread(HdbDevice *dev);
	/**
	 *	Execute the thread loop.
	 *	This thread is awaken when a command has been received 
	 *	and falled asleep when no command has been received from a long time.
	 */
	void *run_undetached(void *);
	void start() {start_undetached();}
	void abort_sleep(double time);
};


}	// namespace_ns

#endif	// _STATS_THREAD_H
