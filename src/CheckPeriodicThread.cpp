static const char *RcsId = "$Header: /home/cvsadm/cvsroot/fermi/servers/hdb++/hdb++es/src/CheckPeriodicThread.cpp,v 1.6 2014-03-06 15:21:43 graziano Exp $";
//+=============================================================================
//
// file :         CheckPeriodicThread.cpp
//
// description :  C++ source for thread management
// project :      TANGO Device Server
//
// $Author: graziano $
//
// $Revision: 1.6 $
//
// $Log: CheckPeriodicThread.cpp,v $
//
//
//
// copyleft :     European Synchrotron Radiation Facility
//                BP 220, Grenoble 38043
//                FRANCE
//
//-=============================================================================


#include "CheckPeriodicThread.h"
#include "HdbDevice.h"
#include "SubscribeThread.h"

namespace HdbEventSubscriber_ns
{

const unsigned int default_period = 1000;

//=============================================================================
//=============================================================================
CheckPeriodicThread::CheckPeriodicThread(HdbDevice *dev): AbortableThread(dev->_device)
                                                          , last_check()
                                                          , delay_tolerance_ms(0)
{
	hdb_dev = dev;
	last_check.tv_sec = 0;
	last_check.tv_nsec = 0;
	clock_gettime(CLOCK_MONOTONIC, &last_check);
}

//=============================================================================
//=============================================================================
auto CheckPeriodicThread::init_abort_loop() -> void
{
    INFO_STREAM << "CheckPeriodicThread delay_tolerance_ms="<<delay_tolerance_ms<<" id="<<omni_thread::self()->id()<<endl;
}

//=============================================================================
//=============================================================================
auto CheckPeriodicThread::get_abort_loop_period_ms() -> unsigned int
{
    unsigned int min_time_to_timeout_ms = 0;
    try
    {
        min_time_to_timeout_ms = hdb_dev->shared->check_periodic_event_timeout(delay_tolerance_ms);
    }catch(Tango::DevFailed &e)
    {
    }

    //sleep 1 second more than the first expiration time calculated
    return default_period + min_time_to_timeout_ms;
}

//=============================================================================
//=============================================================================
auto CheckPeriodicThread::finalize_abort_loop() -> void
{
    INFO_STREAM <<"CheckPeriodicThread::"<< __func__<<": exiting..."<<endl;
}

//=============================================================================
//=============================================================================
auto CheckPeriodicThread::run_thread_loop() -> void
{
}

}	//	namespace
