//=============================================================================
//
// file :        CheckPeriodicThread.h
//
// description : Include for the CheckPeriodicThread.
//
// project :	Tango Device Server
//
// $Author: graziano $
//
// $Revision: 1.5 $
//
// $Log: CheckPeriodicThread.h,v $
//
//
//
// copyleft :    European Synchrotron Radiation Facility
//               BP 220, Grenoble 38043
//               FRANCE
//
//=============================================================================

#ifndef _CHECK_PERIODIC_THREAD_H
#define _CHECK_PERIODIC_THREAD_H

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
 *	Create a thread retry to check periodic event.
 */
//=========================================================
class CheckPeriodicThread: public omni_thread
{
private:
	/**
	 *	HdbDevice object
	 */
	HdbDevice	*hdb_dev;


public:
	bool		abortflag;
	timespec	last_check;
	int			delay_tolerance_ms;
	CheckPeriodicThread(HdbDevice *dev);

	void *run_undetached(void *);
	void start() {start_undetached();}
	void abort_sleep(double time);
};


}	// namespace_ns

#endif	// _CHECK_PERIODIC_THREAD_H
