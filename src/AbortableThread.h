//=============================================================================
//
// file :        AbortableThread.h
//
// description : AbortableThread is an interface to factorize code
//               linked to aborting a thread.
//
// project :	Tango Device Server
//
// $Author: graziano $
//
// $Revision: 1.5 $
//
//
//
//
// copyleft :    European Synchrotron Radiation Facility
//               BP 220, Grenoble 38043
//               FRANCE
//
//=============================================================================

#ifndef _ABORTABLE_THREAD_H
#define _ABORTABLE_THREAD_H

#include <memory>
#include <atomic>
#include <tango.h>

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
     *	Thread with abort capability.
     *	Basically a loop will check on .
     */
    //=========================================================


    class AbortableThread: public omni_thread, public Tango::LogAdapter
    {
        private:

            std::atomic_bool abort_flag;
            std::chrono::duration<double> period = std::chrono::seconds(-1);

            omni_mutex abort_mutex;
            omni_condition abort_condition;

            auto timed_wait() -> int;

        protected:

            virtual void init_abort_loop() = 0;
            virtual void run_thread_loop() = 0;
            virtual void finalize_abort_loop() = 0;
            virtual auto get_abort_loop_period() -> std::chrono::milliseconds = 0;

            virtual void do_abort() {};

            auto get_period() const -> std::chrono::duration<double> {return period;}
            auto is_aborted() const -> bool {return abort_flag.load();}

        public:
            AbortableThread(Tango::DeviceImpl *dev);
            ~AbortableThread();

            //inherited from amni_thread
            auto run_undetached(void *) -> void* override;
            void start() {start_undetached();}

            void set_period(const std::chrono::duration<double> period_s) {period = period_s;}

            //abort logic
            void abort();
    };

    struct AbortableThreadDeleter {
        void operator()(AbortableThread* ptr) const {
        }
    };

}	// namespace_ns

#endif	// _ABORTABLE_THREAD_H
