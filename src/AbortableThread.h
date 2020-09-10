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
// $Log: CheckPeriodicThread.h,v $
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

    bool abort_flag = false;
    double period = -1;

    omni_mutex abort_mutex;
    omni_condition abort_condition;

    auto timed_wait() -> int;

protected:

    virtual void init_abort_loop() = 0;
    virtual void run_thread_loop() = 0;
    virtual void finalize_abort_loop() = 0;
    virtual auto get_abort_loop_period_ms() -> unsigned int = 0;

    auto get_period() const -> double {return period;}

public:
    AbortableThread(Tango::DeviceImpl *dev);

    //inherited from amni_thread
    auto run_undetached(void *) -> void*;
    void start() {start_undetached();}

    void set_period(double period_s) {period = period_s;}

    //abort logic
    void abort();
};


}	// namespace_ns

#endif	// _ABORTABLE_THREAD_H
