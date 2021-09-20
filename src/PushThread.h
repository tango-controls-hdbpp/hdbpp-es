//=============================================================================
//
// file :        PushThread.h
//
// description : Include for the PushThread class.
//
// project :	Starter for Tango Administration
//
// $Author: graziano $
//
// $Revision: 1.7 $
//
// $Log: PushThread.h,v $
// Revision 1.7  2014-03-06 15:21:43  graziano
// StartArchivingAtStartup,
// start_all and stop_all,
// archiving of first event received at subscribe
//
// Revision 1.6  2014-02-20 14:59:47  graziano
// name and path fixing
// bug fixed in remove
//
// Revision 1.5  2013-09-24 08:42:21  graziano
// bug fixing
//
// Revision 1.4  2013-09-02 12:14:41  graziano
// libhdb refurbishing
//
// Revision 1.3  2013-08-23 10:04:53  graziano
// development
//
// Revision 1.2  2013-08-14 13:10:07  graziano
// development
//
// Revision 1.1  2013-07-17 13:37:43  graziano
// *** empty log message ***
//
//
//
// copyleft :    European Synchrotron Radiation Facility
//               BP 220, Grenoble 38043
//               FRANCE
//
//=============================================================================


#ifndef _PushThread_H
#define _PushThread_H

#include <memory>
#include <tango.h>
#include <cstdint>
#include <map>
#include <deque>
#include "hdb++/AbstractDB.h"
#include "HdbCmdData.h"
#include "AbortableThread.h"


namespace HdbEventSubscriber_ns
{


    
    class HdbDevice;

    //=========================================================
    /**
     *	Shared data between DS and thread.
     */
    //=========================================================
    class PushThread: public AbortableThread
    {
        private:
            /**
             *	HdbDevice object
             */
            HdbDevice	*hdb_dev;
            /**
             *	Manage data to write.
             */
            std::deque<std::unique_ptr<HdbCmdData>> events;
            /**
             *	Manage exceptions if any
             */
            vector<Tango::DevFailed> except;

            omni_mutex new_data_mutex;
            omni_condition new_data;

            std::unique_ptr<hdbpp::AbstractDB> mdb;

            size_t max_waiting;

            bool batch_insert = false;

        protected:

            void init_abort_loop() override;
            void run_thread_loop() override;
            void finalize_abort_loop() override;
            auto get_abort_loop_period() -> std::chrono::milliseconds override;
            
            void do_abort() override;

        public:
            /**
             *	Initialize the sub process parameters (name, domain, log_file).
             */
            PushThread(HdbDevice *dev, const string& ds_name, const vector<string>& configuration);

            void push_back_cmd(const std::unique_ptr<HdbCmdData> argin);
            //void push_back_cmd(Tango::EventData argin);
            //	void remove_cmd();
            auto nb_cmd_waiting() -> size_t;
            auto get_next_cmds() -> std::vector<std::unique_ptr<HdbCmdData>>;

            auto get_max_waiting() -> size_t;
            void get_sig_list_waiting(vector<string> &);

            void start_attr(HdbSignal& signal);
            void pause_attr(HdbSignal& signal);
            void stop_attr(HdbSignal& signal);
            void remove_attr(HdbSignal& signal);
            void add_attr(HdbSignal& signal, int data_type, int data_format, int write_type);
            void updatettl(HdbSignal& signal, unsigned int ttl);

    };


}// namespace

#endif	// _PushThread_H
