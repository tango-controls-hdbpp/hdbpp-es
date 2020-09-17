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
// $Log: HdbDevice.h,v $
// Revision 1.5  2014-03-06 15:21:43  graziano
// StartArchivingAtStartup,
// start_all and stop_all,
// archiving of first event received at subscribe
//
// Revision 1.4  2013-09-02 12:19:11  graziano
// cleaned
//
// Revision 1.3  2013-08-26 13:25:44  graziano
// added fix_tango_host
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

#ifndef _HDBDEVICE_H
#define _HDBDEVICE_H

//#define MAX_ATTRIBUTES		10000
#define CONTEXT_KEY		"strategy"
#define TTL_KEY			"ttl"
#define DEFAULT_TTL		0	//0 -> infinite, >0 -> time to live in hours
#define ALWAYS_CONTEXT	"ALWAYS"
#define ALWAYS_CONTEXT_DESC	"Always stored"

#include <tango.h>
#include "Consts.h"

/**
 * @author	$Author: graziano $
 * @version	$Revision: 1.5 $
 */

 //	constants definitions here.
 //-----------------------------------------------
#ifndef	TIME_VAR
#ifndef WIN32

#	define	DECLARE_TIME_VAR	struct timeval
#	define	GET_TIME(t)	gettimeofday(&t, NULL);
#	define	ELAPSED(before, after)	\
		1000.0*(after.tv_sec-before.tv_sec) + \
		((double)after.tv_usec-before.tv_usec) / 1000

#else

#	define	DECLARE_TIME_VAR	struct _timeb
#	define	GET_TIME(t)	_ftime(&t);
#	define	ELAPSED(before, after)	\
		1000*(after.time - before.time) + (after.millitm - before.millitm)

#endif	/*	WIN32		*/
#endif	/*	TIME_VAR	*/

#define STATUS_SUBSCRIBED	string("Subscribed")
#define STATUS_DB_ERROR		string("Storing Error")


namespace HdbEventSubscriber_ns
{

class PollerThread;
class StatsThread;
class CheckPeriodicThread;
class PushThread;
class SubscribeThread;
class SharedData;

//==========================================================
/**
 * Class Description:
 * This class manage a HdbAccess class to store value in TACO history database
 */
//==========================================================
class HdbDevice: public Tango::LogAdapter
{
public:
	//	Data members here
	//-----------------------------------------
        std::string				status;
	std::unique_ptr<SubscribeThread, std::function<void(SubscribeThread*)>> thread;
	std::unique_ptr<PushThread, std::function<void(PushThread*)>> push_thread;
	std::unique_ptr<StatsThread, std::function<void(StatsThread*)>> stats_thread;
	std::unique_ptr<CheckPeriodicThread, std::function<void(CheckPeriodicThread*)>> check_periodic_thread;
	std::unique_ptr<PollerThread, std::function<void(PollerThread*)>> poller_thread;
	int					period;
	int					poller_period;
	int					stats_window;
	int					check_periodic_delay;
	bool				subscribe_change;
	/**
	 *	Shared data
	 */
	std::shared_ptr<SharedData> shared;
	Tango::DeviceImpl *_device;
        std::map<std::string, std::string> domain_map;

	Tango::DevDouble	AttributeRecordFreq;
	Tango::DevDouble	AttributeFailureFreq;
	Tango::DevDouble	AttributeRecordFreqList[MAX_ATTRIBUTES];
	Tango::DevDouble	AttributeFailureFreqList[MAX_ATTRIBUTES];
	Tango::DevLong		AttributeEventNumberList[MAX_ATTRIBUTES];
	Tango::DevLong		AttributePendingNumber;
	Tango::DevLong		AttributeMaxPendingNumber;

	Tango::DevLong	attr_AttributeOkNumber_read;
	Tango::DevLong	attr_AttributeNokNumber_read;
	Tango::DevLong	attr_AttributeNumber_read;
	Tango::DevLong	attr_AttributeStartedNumber_read;
	Tango::DevLong	attr_AttributePausedNumber_read;
	Tango::DevLong	attr_AttributeStoppedNumber_read;

	Tango::DevDouble	attr_AttributeMaxStoreTime_read;
	Tango::DevDouble	attr_AttributeMinStoreTime_read;
	Tango::DevDouble	attr_AttributeMaxProcessingTime_read;
	Tango::DevDouble	attr_AttributeMinProcessingTime_read;

	Tango::DevString	attr_AttributeList_read[MAX_ATTRIBUTES];
	Tango::DevString	attr_AttributeOkList_read[MAX_ATTRIBUTES];
	Tango::DevString	attr_AttributeNokList_read[MAX_ATTRIBUTES];
	Tango::DevString	attr_AttributePendingList_read[MAX_ATTRIBUTES];
	Tango::DevString	attr_AttributeStartedList_read[MAX_ATTRIBUTES];
	Tango::DevString	attr_AttributePausedList_read[MAX_ATTRIBUTES];
	Tango::DevString	attr_AttributeStoppedList_read[MAX_ATTRIBUTES];
	Tango::DevString	attr_AttributeErrorList_read[MAX_ATTRIBUTES];
	Tango::DevString	attr_AttributeContextList_read[MAX_ATTRIBUTES];

	Tango::DevULong		attr_AttributeTTLList_read[MAX_ATTRIBUTES];

	vector<string> attribute_list_str;
	size_t attribute_list_str_size;
	vector<string> attribute_ok_list_str;
	size_t attribute_ok_list_str_size;
	vector<string> attribute_nok_list_str;
	size_t attribute_nok_list_str_size;
	vector<string> attribute_pending_list_str;
	size_t attribute_pending_list_str_size;
	vector<string> attribute_started_list_str;
	size_t attribute_started_list_str_size;
	vector<string> attribute_paused_list_str;
	size_t attribute_paused_list_str_size;
	vector<string> attribute_stopped_list_str;
	size_t attribute_stopped_list_str_size;
	vector<string> attribute_error_list_str;
	size_t attribute_error_list_str_size;
	vector<string> attribute_context_list_str;
	size_t attribute_context_list_str_size;

	map<string,string> contexts_map;
	map<string,string> contexts_map_upper;
	string defaultStrategy;

#ifdef _USE_FERMI_DB_RW
private:
	string host_rw;
	long port_rw;
public:
#endif
	/**
	 * Constructs a newly allocated Command object.
	 *
	 *	@param devname 	Device Name
	 *	@param p	 	Period to retry subscribe event
	 *	@param pp	 	Poller thread Period
	 *	@param s	 	Period to compute statistics
	 *	@param c	 	Delay before timeout on periodic events
	 *	@param ch	 	Subscribe to change event if archive event is not used
	 */
	HdbDevice(int p, int pp, int s, int c, bool ch, Tango::DeviceImpl *device);
	~HdbDevice();
	/**
	 * initialize object
	 */
	void initialize();

	/**
	 * Add a new signal.
	 */
	void add(const string &signame, vector<string>& contexts, int data_type, int data_format, int write_type);
	/**
	 * AddRemove a signal in the list.
	 */
	void remove(const string &signame);
	/**
	 * Update contexts for a signal.
	 */
	void update(const string &signame, vector<string>& contexts);
	/**
	 * Update ttl for a signal.
	 */
	void updatettl(const string &signame, long ttl);
	/**
	 *	Update SignalList property
	 */
	void put_signal_property(vector<string> &prop);

	/**
	 *	Return the list of signals
	 */
	void get_sig_list(vector<string> &);
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
	 *	Return the list of signals not_started
	 */
	void get_sig_not_started_list(vector<string> &);
	/**
	 *	Return the list errors
	 */
	bool get_error_list(vector<string> &);
	/**
	 *	Populate the list of event received numbers
	 */
	void  get_event_number_list();
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
	 *	Return the status of specified signal
	 */
	auto get_sig_status(const string &signame) -> string;
	/**
	 *	Return ALARM if at list one signal is not subscribed.
	 */
	virtual Tango::DevState subcribing_state();
	/**
	 *	Manage attribute received an error event
	 */
	void error_attribute(Tango::EventData *data);
	/**
	 *	Calculate statistics of the HDB storage time/attribute
	 */
	void storage_time(Tango::EventData *data, double elapsed);
	/**
	 *	Returns how many signals are waiting to be stored
	 */
	 auto get_max_waiting() const -> int;
	/**
	 *	Returns how many signals are waiting to be stored
	 */
	 auto nb_cmd_waiting() const -> int;
	/**
	 *	Returns the list of signals waiting to be stored
	 */
	void get_sig_list_waiting(vector<string> &) const;
	/**
	 *	Reset statistic counters
	 */
	 void reset_statistics();
	/**
	 *	Reset statistic freq counters
	 */
	 void reset_freq_statistics();
	/**
	 *	Return the complete, started  and stopped lists of signals
	 */
	bool  get_lists(vector<string> &_list, vector<string> &_start_list, vector<string> &_pause_list, vector<string> &_stop_list, vector<string> &_context_list, Tango::DevULong *ttl_list);
	/**
	 *	Check if fqdn, otherwise fix it
	 */
	void fix_tango_host(const string &attr, string& fixed);
	/**
	 *	Check if full domain name, otherwise fix it
	 */
	void add_domain(const string &attr, string& with_domain);
#ifndef _MULTI_TANGO_HOST
	/**
	 *	Compare without domain
	 */
	static auto compare_without_domain(const string &str1, const string &str2) -> bool;
#else
	/**
	 *	compare 2 tango names considering fqdn, domain, multi tango host
	 *	returns 0 if equal
	 */
	static auto compare_tango_names(const string& str1, const string& str2) -> int;
#endif
	/**
	 *	explode a string in multiple strings using separator
	 */
	static void string_explode(const string &str, const string &separator, vector<string>& results);

        template<typename T>
        void push_events(const std::string& att_name, T* data, bool sleep = false);
        template<typename T>
        void push_events(const std::string& att_name, T* data, long size, bool sleep = false);

protected :	
	/**
	 * Read signal list in database as property.
	 *
	 */
	void get_hdb_signal_list(vector<string> &);
	/**
	 * Build signal vector
	 *
	 *	@param list 	signal names vector
	 */
	void build_signal_vector(const vector<string> &, const string &);
	/**
	 *	Store double (vector) in HDB
	 */
	void store_double(string &name, vector<double> values, int time);
	/**
	 *	Store long (vector) in HDB
	 */
	void store_long(string &name, vector<long int> values, int time);
	/**
	 *	Store string (vector) in HDB
	 */
	void store_string(string &name, vector<string> values, int time);

private:	
	/**
	 *	Returns the tango host and signal name (tango host has been added since tango 7.1.1)
	 */
	static void get_tango_host_and_signal_name(const string &signame, string& tango_host, string& name);
	/**
	 *	Remove domain
	 */
	static auto remove_domain(const string &str) -> string;
};



//==========================================================
/**
 * Class Description:
 * This class manage archive events
 */
//==========================================================
class ArchiveCB : public Tango::CallBack, public Tango::LogAdapter
{
public:
	HdbDevice	*hdb_dev;
//	HdbSignal	*signal;
	ArchiveCB(HdbDevice	*dev);// { hdb_dev=dev; };

	virtual void push_event(Tango::EventData *data);
	virtual void push_event(Tango::AttrConfEventData* data);
};

template<typename T>
void HdbDevice::push_events(const std::string& attr_name, T* data, bool sleep)
{
    try
    {
        _device->push_change_event(attr_name, data);
        _device->push_archive_event(attr_name, data);
    }
    catch(Tango::DevFailed &e){}

    // TODO is this needed ?
    if(sleep)
        usleep(1 * s_to_ms_factor);
}

template<typename T>
void HdbDevice::push_events(const std::string& attr_name, T* data, long size, bool sleep)
{
    try
    {
        _device->push_change_event(attr_name, data, size);
        _device->push_archive_event(attr_name, data, size);
    }
    catch(Tango::DevFailed &e){}

    // TODO is this needed ?
    if(sleep)
        usleep(1 * ms_to_us_factor);
}
}	// namespace_ns

#endif	// _HDBDEVICE_H
