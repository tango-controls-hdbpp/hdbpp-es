//=============================================================================
//
// file :        PollerThread.h
//
// description : Include for the PollerThread.
//
// project :	Tango Device Server
//
// $Author: graziano $
//
// $Revision: 1.5 $
//
// $Log: PollerThread.h,v $
//
//
//
// copyleft :    European Synchrotron Radiation Facility
//               BP 220, Grenoble 38043
//               FRANCE
//
//=============================================================================

#ifndef _POLLER_THREAD_H
#define _POLLER_THREAD_H

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
class PollerThread: public omni_thread, public Tango::LogAdapter
{
private:
	/**
	 *	HdbDevice object
	 */
	HdbDevice	*hdb_dev;
	bool is_list_changed(vector<string> const & newlist, vector<string> &oldlist);


public:
	int			period;
	bool		abortflag;
	timeval		last_stat;
	PollerThread(HdbDevice *dev);
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

#endif	// _POLLER_THREAD_H
