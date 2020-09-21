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

#include "AbortableThread.h"

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

class HdbDevice;

class CheckPeriodicThread: public AbortableThread
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
        auto get_abort_loop_period_ms() -> unsigned int override;

public:
	unsigned int			delay_tolerance_ms;
	CheckPeriodicThread(HdbDevice *dev);
};


}	// namespace_ns

#endif	// _CHECK_PERIODIC_THREAD_H
