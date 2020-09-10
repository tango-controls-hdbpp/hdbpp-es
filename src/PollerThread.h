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
#include "AbortableThread.h"
#include "Consts.h"

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
    class PollerThread: public AbortableThread
    {
        private:

            /**
             *	HdbDevice object
             */
            HdbDevice	*hdb_dev;
            static auto is_list_changed(const vector<string> & newlist, vector<string> &oldlist) -> bool;
            static void update_array(Tango::DevString (&out)[MAX_ATTRIBUTES], size_t& out_size, const vector<string>& in);

        protected:

            void init_abort_loop() override;
            void run_thread_loop() override;
            void finalize_abort_loop() override;
            auto get_abort_loop_period_ms() -> unsigned int override;

        public:
            timeval		last_stat;
            PollerThread(HdbDevice *dev);
    };


}	// namespace_ns

#endif	// _POLLER_THREAD_H
