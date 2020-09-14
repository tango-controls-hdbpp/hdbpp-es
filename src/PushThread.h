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


    typedef struct
    {
        std::string	name;
        uint32_t nokdb_counter;
        uint32_t nokdb_counter_freq;
        uint32_t okdb_counter;
        Tango::DevState dbstate;
        std::string dberror;
        double process_time_avg;
        double process_time_min;
        double process_time_max;
        double store_time_avg;
        double store_time_min;
        double store_time_max;
        timeval last_nokdb;
    }
    HdbStat;

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
            std::deque<std::shared_ptr<HdbCmdData>> events;
            /**
             *	Manage exceptions if any
             */
            vector<Tango::DevFailed>	except;

            omni_mutex sig_lock;
            omni_mutex new_data_mutex;
            omni_condition new_data;
            std::map<const std::string, HdbStat> signals;

            std::unique_ptr<hdbpp::AbstractDB> mdb;

            size_t max_waiting;

            HdbStat NO_SIGNAL;

            auto get_signal(const std::string& signame) -> HdbStat&;

            bool batch_insert = false;

        protected:

            void init_abort_loop() override;
            void run_thread_loop() override;
            void finalize_abort_loop() override;
            auto get_abort_loop_period_ms() -> unsigned int override;
            
            void do_abort() override;

        public:
            /**
             *	Initialize the sub process parameters (name, domain, log_file).
             */
            PushThread(HdbDevice *dev, const string& ds_name, const vector<string>& configuration);

            void push_back_cmd(const std::shared_ptr<HdbCmdData>& argin);
            //void push_back_cmd(Tango::EventData argin);
            //	void remove_cmd();
            auto nb_cmd_waiting() -> size_t;
            auto get_next_cmds() -> std::vector<std::shared_ptr<HdbCmdData>>;

            auto get_max_waiting() -> size_t;
            void get_sig_list_waiting(vector<string> &);
            void reset_statistics();
            void reset_freq_statistics();

            void remove(const string& signame);
            /**
             *	Return the list of signals on error
             */
            auto get_sig_on_error_list() -> vector<string>;
            /**
             *	Return the list of signals not on error
             */
            auto get_sig_not_on_error_list() -> vector<string>;
            /**
             *	Return the number of signals on error
             */
            auto get_sig_on_error_num() -> int;
            /**
             *	Return the number of signals not on error
             */
            auto get_sig_not_on_error_num() -> int;
            /**
             *	Return the db state of the signal
             */
            auto get_sig_state(const string& signame) -> Tango::DevState;
            /**
             *	Return the db error status of the signal
             */
            auto get_sig_status(const string& signame) -> std::string;
            /**
             *	Increment the error counter of db saving
             */
            void set_nok_db(const string& signame, const string& error);
            /**
             *	Get the error counter of db saving
             */
            auto get_nok_db(const string& signame) -> uint32_t;
            /**
             *	Get the error counter of db saving for freq stats
             */
            auto get_nok_db_freq(const string& signame) -> uint32_t;
            /**
             *	Get avg store time
             */
            auto get_avg_store_time(const string& signame) -> double;
            /**
             *	Get min store time
             */
            auto get_min_store_time(const string& signame) -> double;
            /**
             *	Get max store time
             */
            auto get_max_store_time(const string& signame) -> double;
            /**
             *	Get avg process time
             */
            auto get_avg_process_time(const string& signame) -> double;
            /**
             *	Get min process time
             */
            auto get_min_process_time(const string& signame) -> double;
            /**
             *	Get max process time
             */
            auto get_max_process_time(const string& signame) -> double;
            /**
             *	Get last nokdb timestamp
             */
            auto get_last_nokdb(const string& signame) -> timeval;
            /**
             *	reset state
             */
            void set_ok_db(const string& signame, double store_time, double process_time);

            void  start_attr(const string& signame);
            void  pause_attr(const string& signame);
            void  stop_attr(const string& signame);
            void  remove_attr(const string& signame);
            void  add_attr(const string& signame, int data_type, int data_format, int write_type);
            void  start_all();
            void  pause_all();
            void  stop_all();
            void  updatettl(const string &signame, unsigned int ttl);

            /**
             *	Return ALARM if at list one signal is not saving in DB.
             */
            auto state() -> Tango::DevState;

    };


}// namespace

#endif	// _PushThread_H
