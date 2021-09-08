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
                                                              , abort_flag(false)
                                                              , abort_condition(&abort_mutex)
    {
    }

    //=============================================================================
    //=============================================================================
    AbortableThread::~AbortableThread() = default;
    /*
    {
       // join(nullptr);
    }
    */

    //=============================================================================
    //=============================================================================
    auto AbortableThread::run_undetached(void* /*unused*/) -> void *
    {
        init_abort_loop();

        bool aborted = abort_flag.load();

        while(!aborted)
        {

            run_thread_loop();

            // Temporisation
            aborted = timed_wait() != 0;
        }

        finalize_abort_loop();
        return nullptr;
    }

    //=============================================================================
    //=============================================================================
    auto AbortableThread::timed_wait() -> int
    {
        using namespace std::chrono_literals;
        omni_mutex_lock sync(abort_mutex);
        if(!abort_flag.load())
        {
            std::chrono::duration<double> time(0.);

            if(period > 0s)
            {
                time = period;
            }
            else
            {
                time = get_abort_loop_period();
            }

            // if timeout < 0 do not wait.
            if(time > 0s)
            {
                unsigned long abs_sec = 0;
                unsigned long abs_nsec = 0;
                
                unsigned long rel_sec = 0;
                unsigned long rel_nsec = 0;
                
                // Compute rel_sec and rel_nsec from the time in sec.
                auto seconds = std::chrono::duration_cast<std::chrono::seconds>(time);

                rel_sec = seconds.count();
                rel_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>((time - seconds)).count();

                omni_thread::get_time(&abs_sec, &abs_nsec, rel_sec, rel_nsec);
                return abort_condition.timedwait(abs_sec, abs_nsec);
            }
            // if not waiting return the abort_flag in case it was aborted
            return 0;
        }
        return 1;
    }

    //=============================================================================
    //=============================================================================
    void AbortableThread::abort()
    {
        abort_flag.store(true);

        abort_condition.signal();

        //in case extra steps are needed on abort
        do_abort();
    }
}//	namespace
