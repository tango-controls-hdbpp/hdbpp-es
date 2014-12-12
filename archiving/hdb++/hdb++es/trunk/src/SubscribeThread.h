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


/**
 * @author	$Author: graziano $
 * @version	$Revision: 1.5 $
 */

 //	constants definitions here.
 //-----------------------------------------------
#define	ERR			-1
#define	NOTHING		0
#define	UPDATE_PROP	1

namespace HdbEventSubscriber_ns
{

class ArchiveCB;

typedef struct 
{
	string	name;
	string	devname;
	string	attname;
	string	status;
	int		data_type;
	Tango::AttrDataFormat		data_format;
	int		write_type;
	int max_dim_x;
	int max_dim_y;
	Tango::AttributeProxy	*attr;
	Tango::DevState			evstate;
	bool 	first;
	bool 	first_err;
	ArchiveCB	*archive_cb;
	int		event_id;
	int		event_conf_id;
	bool	isZMQ;
	uint32_t okev_counter;
	uint32_t okev_counter_freq;
	timeval last_okev;
	uint32_t nokev_counter;
	uint32_t nokev_counter_freq;
	timeval last_nokev;
	timespec last_ev;
	int periodic_ev;
	bool running;
	ReadersWritersLock *siglock;
}
HdbSignal;

class HdbDevice;
class SubscribeThread;


//=========================================================
/**
 *	Shared data between DS and thread.
 */
//=========================================================
//class SharedData: public omni_mutex
class SharedData: public Tango::TangoMonitor
{
private:
	/**
	 *	HdbDevice object
	 */
	HdbDevice	*hdb_dev;

	int		action;
	bool	stop_it;
	bool	initialized;

public:
	//omni_condition condition;
	vector<HdbSignal>	signals;
	ReadersWritersLock      veclock;


	/**
	 * Constructor
	 */
	//SharedData(HdbDevice *dev):condition(this){ hdb_dev=dev; action=NOTHING; stop_it=false; initialized=false;};
	SharedData(HdbDevice *dev){ hdb_dev=dev; action=NOTHING; stop_it=false; initialized=false;};
	/**
	 * Add a new signal.
	 */
	void add(string &signame);
	void add(string &signame, int to_do);
	/**
	 * Remove a signal in the list.
	 */
	void remove(string &signame);
	/**
	 * Start saving on DB a signal.
	 */
	void start(string &signame);
	/**
	 * Stop saving on DB a signal.
	 */
	void stop(string &signame);
	/**
	 * Start saving on DB all signals.
	 */
	void start_all();
	/**
	 * Stop saving on DB all signals.
	 */
	void stop_all();
	/**
	 * Is a signal saved on DB?
	 */
	bool is_running(string &signame);
	/**
	 * Is a signal first event arrived?
	 */
	bool is_first(string &signame);
	/**
	 * Set a signal first event arrived
	 */
	void set_first(string &signame);
	/**
	 * Is a signal first consecutive error event arrived?
	 */
	bool is_first_err(string &signame);
	/**
	 * Set a signal first consecutive error event arrived
	 */
	void set_first_err(string &signame);
	/**
	 *	get signal by name.
	 */
	HdbSignal *get_signal(string name);
	/**
	 * Subscribe achive event for each signal
	 */
	void subscribe_events();
	/**
	 * Unsubscribe achive event for each signal
	 */
	void unsubscribe_events();
	/**
	 *	return number of signals to be subscribed
	 */
	int nb_sig_to_subscribe();
	/**
	 *	build a list of signal to set HDB device property
	 */
	void put_signal_property();
	/**
	 *	Return the list of signals
	 */
	vector<string> get_sig_list();
	/**
	 *	Return the list of sources
	 */
	vector<bool> get_sig_source_list();
	/**
	 *	Return the source of specified signal
	 */
	bool  get_sig_source(string &signame);
	/**
	 *	Return the list of signals on error
	 */
	vector<string>  get_sig_on_error_list();
	/**
	 *	Return the list of signals not on error
	 */
	vector<string>  get_sig_not_on_error_list();
	/**
	 *	Return the list of signals started
	 */
	vector<string>  get_sig_started_list();
	/**
	 *	Return the list of signals not started
	 */
	vector<string>  get_sig_not_started_list();
	/**
	 *	Return the list of errors
	 */
	vector<string>  get_error_list();
	/**
	 *	Return the list of event received
	 */
	vector<uint32_t>  get_ev_counter_list();
	/**
	 *	Return the number of signals on error
	 */
	int  get_sig_on_error_num();
	/**
	 *	Return the number of signals not on error
	 */
	int  get_sig_not_on_error_num();
	/**
	 *	Return the number of signals started
	 */
	int  get_sig_started_num();
	/**
	 *	Return the number of signals not started
	 */
	int  get_sig_not_started_num();
	/**
	 *	Increment the ok counter of event rx
	 */
	void  set_ok_event(string &signame);
	/**
	 *	Get the ok counter of event rx
	 */
	uint32_t  get_ok_event(string &signame);
	/**
	 *	Get the ok counter of event rx for freq stats
	 */
	uint32_t  get_ok_event_freq(string &signame);
	/**
	 *	Get last okev timestamp
	 */
	timeval  get_last_okev(string &signame);
	/**
	 *	Increment the error counter of event rx
	 */
	void  set_nok_event(string &signame);
	/**
	 *	Get the error counter of event rx
	 */
	uint32_t  get_nok_event(string &signame);
	/**
	 *	Get the error counter of event rx for freq stats
	 */
	uint32_t  get_nok_event_freq(string &signame);
	/**
	 *	Get last nokev timestamp
	 */
	timeval  get_last_nokev(string &signame);
	/**
	 *	Set state and status of timeout on periodic event
	 */
	void  set_nok_periodic_event(string &signame);
	/**
	 *	Return the status of specified signal
	 */
	string  get_sig_status(string &signame);
	/**
	 *	Return the state of specified signal
	 */
	Tango::DevState  get_sig_state(string &signame);
	/**
	 *	Set Archive periodic event period
	 */
	void  set_conf_periodic_event(string &signame, string period);
	/**
	 *	Check Archive periodic event period
	 */
	int  check_periodic_event_timeout(int delay_tolerance_ms);
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
	Tango::DevState state();
	
	bool is_initialized();
	bool get_if_stop();
	void stop_thread();
	
};








//=========================================================
/**
 *	Create a thread retry to subscribe event.
 */
//=========================================================
class SubscribeThread: public omni_thread
{
private:
	/**
	 *	Shared data
	 */
	SharedData	*shared;
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
