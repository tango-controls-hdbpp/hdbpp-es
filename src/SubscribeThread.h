//=============================================================================
//
// file :        HdbEventHandler.h
//
// description : Include for the HDbDevice class.
//
// project :	Tango Device Server
//
// $Author: graziano $
//
// $Revision: 1.5 $
//
// $Log: SubscribeThread.h,v $
// Revision 1.5  2014-03-06 15:21:43  graziano
// StartArchivingAtStartup,
// start_all and stop_all,
// archiving of first event received at subscribe
//
// Revision 1.4  2013-09-24 08:42:21  graziano
// bug fixing
//
// Revision 1.3  2013-09-02 12:11:32  graziano
// cleaned
//
// Revision 1.2  2013-08-23 10:04:53  graziano
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

#ifndef _SUBSCRIBE_THREAD_H
#define _SUBSCRIBE_THREAD_H

#include <tango.h>
#include <eventconsumer.h>
#include <stdint.h>
#include <time.h>
#include <string>
#include <deque>
#include <condition_variable>
#include <thread>
#include <unordered_map>
#include <atomic>
#include "Consts.h"
#include <mutex>

/**
 * @author	$Author: graziano $
 * @version	$Revision: 1.5 $
 */

 //	constants definitions here.
 //-----------------------------------------------
//#define	ERR			-1
#define	NOTHING		0
#define	UPDATE_PROP	1

namespace HdbEventSubscriber_ns
{

class ArchiveCB;
class HdbDevice;
class HdbSignal;
class SubscribeThread;

//=========================================================
/**
 *	Shared data between DS and thread.
 */
//=========================================================
//class SharedData: public omni_mutex
class SharedData: public Tango::TangoMonitor, public Tango::LogAdapter
{
            friend class HdbSignal;
private:
        class PeriodicEventCheck
        {

            friend class HdbSignal;

            public:
            explicit PeriodicEventCheck(SharedData& vec): signals(vec)
                , abort(false)
            {
                period = std::chrono::milliseconds::max();
                periodic_check = std::make_unique<std::thread>(&PeriodicEventCheck::check_periodic_event_timeout, this);
            }

            ~PeriodicEventCheck()
            {
                abort_periodic_check();
            }
            
            auto abort_periodic_check() -> void
            {
                if(!abort)
                {
                abort = true;
                cv.notify_one();
                if(periodic_check)
                    periodic_check->join();
                }
                periods.clear();
            }

            void check_periodic_event_timeout();

            void notify()
            {
                cv.notify_one();
            };

            auto set_period(std::shared_ptr<HdbSignal> signal, std::chrono::milliseconds p) -> void
            {
                {
                    std::lock_guard<mutex> lk(m);
                    if(p > std::chrono::milliseconds::zero())
                    {
                        period = std::min(p, period);
                    }
                    periods.emplace(signal, p);
                }
                notify();
            };

            std::unique_ptr<std::thread> periodic_check;
            std::chrono::milliseconds period;
            SharedData& signals;
            std::unordered_map<std::shared_ptr<HdbSignal>, std::chrono::milliseconds> periods;

            std::mutex m;
            std::condition_variable cv;
            std::atomic_bool abort;
            
            auto check_periodic_event() -> bool;
            
            private:
            PeriodicEventCheck(const PeriodicEventCheck&) = delete;
            PeriodicEventCheck& operator=(PeriodicEventCheck const&) = delete;
        };

            // Init to 1 cause we make a division by this one
            const std::chrono::duration<double> stats_window = std::chrono::seconds(1);
            const std::chrono::milliseconds delay_periodic_event = std::chrono::milliseconds::zero();

            std::chrono::duration<double> min_process_time = std::chrono::duration<double>::max();
            std::chrono::duration<double> max_process_time = std::chrono::duration<double>::min();
            std::chrono::duration<double> min_store_time = std::chrono::duration<double>::max();
            std::chrono::duration<double> max_store_time = std::chrono::duration<double>::min();
            std::mutex timing_mutex;
            std::condition_variable timing_cv;
            std::atomic_bool timing_abort;
            std::unique_ptr<std::thread> timing_events;

            std::unique_ptr<PeriodicEventCheck> event_checker;

            std::atomic_ulong paused_number;
            std::atomic_ulong started_number;
            std::atomic_ulong stopped_number;

	/**
	 *	HdbDevice object
	 */
	HdbDevice	*hdb_dev;

	bool	stop_it;
        bool initialized;
        omni_mutex init_mutex;
        omni_condition init_condition;

        auto is_same_signal_name(const std::string& name1, const std::string& name2) -> bool;

        std::vector<std::shared_ptr<HdbSignal>> signals;
	ReadersWritersLock      veclock;
	
        void add(std::shared_ptr<HdbSignal> signal, int to_do, bool start);
	auto add(const string &signame, const vector<string>& contexts, bool start) -> std::shared_ptr<HdbSignal>;
        auto push_timing_events() -> void;
        auto reset_min_max() -> void;
        auto update_timing(std::chrono::duration<double> store_time, std::chrono::duration<double> process_time) -> void;
public:
	int		action;
	//omni_condition condition;


	/**
	 * Constructor
	 */
	//SharedData(HdbDevice *dev):condition(this){ hdb_dev=dev; action=NOTHING; stop_it=false; initialized=false;};
	SharedData(HdbDevice *dev, std::chrono::seconds window, std::chrono::milliseconds period);
	~SharedData();
	/**
	 * Add a new signal.
	 */
	void add(const string &signame, const vector<string> & contexts, unsigned int ttl);
	void add(const string &signame, const vector<string> & contexts, int data_type, int data_format, int write_type, int to_do, bool start);
	/**
	 * Remove a signal in the list.
	 */
	void remove(const string &signame);
	/**
	 * Update contexts for a signal.
	 */
	void update(const string &signame, const vector<string> & contexts);
	/**
	 * Update ttl for a signal.
	 */
	void updatettl(const string &signame, unsigned int ttl);
	/**
	 * Start saving on DB a signal.
	 */
	void start(const string &signame);
	/**
	 * Pause saving on DB a signal.
	 */
	void pause(const string &signame);
	/**
	 * Stop saving on DB a signal.
	 */
	void stop(const string &signame);
	/**
	 * Start saving on DB all signals.
	 */
	void start_all();
	/**
	 * Pause saving on DB all signals.
	 */
	void pause_all();
	/**
	 * Stop saving on DB all signals.
	 */
	void stop_all();
	/**
	 * Is a signal saved on DB?
	 */
	auto is_running(const string &signame) -> bool;
	/**
	 * Is a signal saved on DB?
	 */
	auto is_paused(const string &signame) -> bool;
	/**
	 * Is a signal not subscribed?
	 */
	auto is_stopped(const string &signame) -> bool;
	/**
	 * Is a signal to be archived with current context?
	 */
	auto is_current_context(const string &signame, string context) -> bool;
	/**
	 * Is a signal first event arrived?
	 */
	auto is_first(const string &signame) -> bool;
	/**
	 * Set a signal first event arrived
	 */
	void set_first(const string &signame);
	/**
	 * Is a signal first consecutive error event arrived?
	 */
	auto is_first_err(const string &signame) -> bool;
	/**
	 *	get signal by name.
	 */
	auto get_signal(const string &name) -> std::shared_ptr<HdbSignal>;
	/**
	 * Subscribe achive event for each signal
	 */
	void subscribe_events();
	/**
	 *	return number of signals to be subscribed
	 */
	auto nb_sig_to_subscribe() -> int;
	/**
	 *	build a list of signal to set HDB device property
	 */
	void put_signal_property();
	/**
	 *	Return the list of signals
	 */
	void get_sig_list(vector<string> &);
	/**
	 *	Return the list of sources
	 */
	auto get_sig_source_list() -> vector<bool>;
	/**
	 *	Return the source of specified signal
	 */
	auto get_sig_source(const string &signame) -> bool;
	/**
	 *	Return the list of signals on error
	 */
	void get_sig_on_error_list(vector<string> &);
	/**
	 *	Return the list of signals not on error
	 */
	void get_sig_not_on_error_list(vector<string> &);
	/**
	 *	Return the list of signals started
	 */
	void get_sig_started_list(vector<string> &);
	/**
	 *	Return the list of signals not started
	 */
	void get_sig_not_started_list(vector<string> &);
	/**
	 *	Return the list of errors
	 */
	auto get_error_list(vector<string> &) -> bool;
	/**
	 *	Return the list of event received
	 */
	void get_ev_counter_list(vector<uint32_t> &);
	/**
	 *	Return the number of signals on error
	 */
	auto  get_sig_on_error_num() -> int;
	/**
	 *	Return the number of signals not on error
	 */
	auto  get_sig_not_on_error_num() -> int;
	/**
	 *	Return the number of signals started
	 */
	auto  get_sig_started_num() -> int;
	/**
	 *	Return the number of signals not started
	 */
	auto  get_sig_not_started_num() -> int;
	/**
	 *	Return the complete, started and stopped lists of signals
	 */
	auto  get_lists(vector<string> &s_list, vector<string> &s_start_list, vector<string> &s_pause_list, vector<string> &s_stop_list, vector<string> &s_context_list, Tango::DevULong *ttl_list) -> bool;
	/**
	 *	Get the ok counter of event rx
	 */
	auto get_ok_event(const string &signame) -> uint32_t;
	/**
	 *	Get the ok counter of event rx for freq stats
	 */
	auto get_ok_event_freq(const string &signame) -> uint32_t;
	/**
	 *	Get last okev timestamp
	 */
	auto get_last_okev(const string &signame) -> std::chrono::time_point<std::chrono::system_clock>;
	/**
	 *	Get the error counter of event rx
	 */
	auto get_nok_event(const string &signame) -> uint32_t;
	/**
	 *	Get the error counter of event rx for freq stats
	 */
	auto get_nok_event_freq(const string &signame) -> uint32_t;
	/**
	 *	Get last nokev timestamp
	 */
	auto get_last_nokev(const string &signame) -> std::chrono::time_point<std::chrono::system_clock>;
	/**
	 *	Set state and status of timeout on periodic event
	 */
	void  set_nok_periodic_event(const string& signame);
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
        auto get_avg_store_time(const string& signame) -> std::chrono::duration<double>;
        /**
         *	Get min store time
         */
        auto get_min_store_time(const string& signame) -> std::chrono::duration<double>;
        /**
         *	Get max store time
         */
        auto get_max_store_time(const string& signame) -> std::chrono::duration<double>;
        /**
         *	Get avg process time
         */
        auto get_avg_process_time(const string& signame) -> std::chrono::duration<double>;
        /**
         *	Get min process time
         */
        auto get_min_process_time(const string& signame) -> std::chrono::duration<double>;
        /**
         *	Get max process time
         */
        auto get_max_process_time(const string& signame) -> std::chrono::duration<double>;
        /**
         *	Get last nokdb timestamp
         */
        auto get_last_nokdb(const string& signame) -> std::chrono::time_point<std::chrono::system_clock>;
        /**
	 *	Return the status of specified signal
	 */
	auto get_sig_status(const string &signame) -> string;
	/**
	 *	Return the state of specified signal
	 */
	auto get_sig_state(const string &signame) -> Tango::DevState;
	/**
	 *	Return the contexts of specified signal
	 */
	auto get_sig_context(const string &signame) -> string;
	/**
	 *	Return the ttl of specified signal
	 */
	auto get_sig_ttl(const string &signame) -> Tango::DevULong;
	/**
	 *	Set Archive periodic event period
	 */
	void  set_conf_periodic_event(const string &signame, const string &period);
	/**
	 *	Reset statistic counters
	 */
	void reset_statistics();
	/**
	 *	Return ALARM if at list one signal is not subscribed.
	 */
	auto state() -> Tango::DevState;
	
	auto is_initialized() -> bool;
	auto get_if_stop() -> bool;
	void stop_thread();
	void wait_initialized();

        auto get_record_freq() -> double;
        auto get_failure_freq() -> double;
        auto get_record_freq_list(std::vector<double>& ret) -> void;
        auto get_failure_freq_list(std::vector<double>& ret) -> void;
        auto get_event_number_list(std::vector<unsigned int>& ret) -> void;
        auto get_global_min_store_time() -> std::chrono::duration<double>;
        auto get_global_max_store_time() -> std::chrono::duration<double>;
        auto get_global_min_process_time() -> std::chrono::duration<double>;
        auto get_global_max_process_time() -> std::chrono::duration<double>;
        auto get_started_number() -> unsigned long;
        auto get_paused_number() -> unsigned long;
        auto get_stopped_number() -> unsigned long;
        auto size() -> size_t;
};








//=========================================================
/**
 *	Create a thread retry to subscribe event.
 */
//=========================================================
class SubscribeThread: public omni_thread, public Tango::LogAdapter
{
private:
	/**
	 *	Shared data
	 */
	std::shared_ptr<SharedData> shared;
	int			period;
	/**
	 *	HdbDevice object
	 */
	HdbDevice	*hdb_dev;


public:
	SubscribeThread(HdbDevice *dev);
	void updateProperty();
	/**
	 *	Execute the thread loop.
	 *	This thread is awaken when a command has been received 
	 *	and falled asleep when no command has been received from a long time.
	 */
	void *run_undetached(void *);
	void start() {start_undetached();}
};


}	// namespace_ns

#endif	// _SUBSCRIBE_THREAD_H
