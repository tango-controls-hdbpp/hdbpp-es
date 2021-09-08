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
#include <cstdint>
#include "AbortableThread.h"

/**
 * @author	$Author: graziano $
 * @version	$Revision: 1.5 $
 */

 //	constants definitions here.
 //-----------------------------------------------

namespace HdbEventSubscriber_ns
{

class HdbDevice;
//=========================================================
/**
 *	Create a thread retry to subscribe event.
 */
//=========================================================
class StatsThread: public AbortableThread
{
private:
	/**
	 *	HdbDevice object
	 */
	HdbDevice	*hdb_dev;

protected:

        void init_abort_loop() override;
        void run_thread_loop() override;
        void finalize_abort_loop() override;
        auto get_abort_loop_period() -> std::chrono::milliseconds override;

public:
	StatsThread(HdbDevice *dev);
};


}	// namespace_ns

#endif	// _STATS_THREAD_H
