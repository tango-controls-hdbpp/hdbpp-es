//+=============================================================================
//
// file :         AbortableThread.cpp
//
// description :  A thread that run code in a loop till it is aborted
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


#include <AbortableThread.h>
#include <cmath>
#include "Consts.h"

namespace HdbEventSubscriber_ns
{

//=============================================================================
//=============================================================================
AbortableThread::AbortableThread(Tango::DeviceImpl *dev): Tango::LogAdapter(dev)
                                                          , abort_condition(&abort_mutex)
{
}

//=============================================================================
//=============================================================================
auto AbortableThread::run_undetached(void * /*ptr*/) -> void *
{
    init_abort_loop();
    
    while(!abort_flag)
    {
        // Temporisation
        bool aborted = false;
        aborted = timed_wait() != 0;

        // End the loop if aborted
        if(aborted)
            break;

        run_thread_loop();
    }
    
    finalize_abort_loop();
    return nullptr;
}

//=============================================================================
//=============================================================================
auto AbortableThread::timed_wait() -> int
{
    unsigned long abs_sec = 0;
    unsigned long abs_nsec = 0;

    unsigned long rel_sec = 0;
    unsigned long rel_nsec = 0;

    double time = 0.;
    
    if(period > 0)
    {
        time = period;
    }
    else
    {
        time = 0. + get_abort_loop_period_ms();
    }

    // Compute rel_sec and rel_nsec from the time in sec.
    double int_time = 0;
    double dec_time = std::modf(time, &int_time);
    
    rel_sec = static_cast<unsigned long>(int_time);
    rel_nsec = static_cast<unsigned long>(dec_time * s_to_ns_factor);

    omni_thread::get_time(&abs_sec, &abs_nsec, rel_sec, rel_nsec);
    return abort_condition.timedwait(abs_sec, abs_nsec);
}

//=============================================================================
//=============================================================================
void AbortableThread::abort()
{
    omni_mutex_lock lock(abort_mutex);
    
    abort_flag = true;
    
    abort_condition.signal();
}
}//	namespace
