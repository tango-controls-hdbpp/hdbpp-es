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
#include "Consts.h"

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
class SubscribeThread;
class HdbSignal;

//=========================================================
/**
 *	Shared data between DS and thread.
 */
//=========================================================
//class SharedData: public omni_mutex
class SharedData: public Tango::TangoMonitor, public Tango::LogAdapter
{
private:
	/**
	 *	HdbDevice object
	 */
	HdbDevice	*hdb_dev;

	bool	stop_it;
        bool initialized;
        omni_mutex init_mutex;
        omni_condition init_condition;

        auto is_same_signal_name(const std::string& name1, const std::string& name2) -> bool;

        vector<std::shared_ptr<HdbSignal>>	signals;
	ReadersWritersLock      veclock;
public:
	int		action;
	//omni_condition condition;


	/**
	 * Constructor
	 */
	//SharedData(HdbDevice *dev):condition(this){ hdb_dev=dev; action=NOTHING; stop_it=false; initialized=false;};
	SharedData(HdbDevice *dev);
	~SharedData();
	/**
	 * Add a new signal.
	 */
	void add(const string &signame, const vector<string> & contexts);
	void add(const string &signame, const vector<string> & contexts, int to_do, bool start);
	void add(std::shared_ptr<HdbSignal> signal, const vector<string> & contexts, int to_do, bool start);
	/**
	 * Remove a signal in the list.
	 */
	void remove(const string &signame, bool stop);
	/**
         * Unsubscribe events for this signal
         */
        void unsubscribe_events(std::shared_ptr<HdbSignal>);
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
	 * Unsubscribe achive event for each signal
         * and clear the signals list
	 */
	void clear_signals();
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
	auto get_last_okev(const string &signame) -> timespec;
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
	auto get_last_nokev(const string &signame) -> timespec;
	/**
	 *	Set state and status of timeout on periodic event
	 */
	void  set_nok_periodic_event(const string& signame);
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
	 *	Check Archive periodic event period
	 */
	auto check_periodic_event_timeout(unsigned int delay_tolerance_ms) -> int;
	/**
	 *	Reset statistic counters
	 */
	void reset_statistics();
	/**
	 *	Reset freq statistic counters
	 */
	void reset_freq_statistics();
	/**
	 *	Return ALARM if at list one signal is not subscribed.
	 */
	auto state() -> Tango::DevState;
	
	auto is_initialized() -> bool;
	auto get_if_stop() -> bool;
	void stop_thread();
	void wait_initialized();
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
