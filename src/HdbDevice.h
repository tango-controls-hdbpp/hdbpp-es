//=============================================================================
//
// file :        HdbDevice.h
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
#define ALWAYS_CONTEXT_DESC	"Always stored"
#define ALWAYS_CONTEXT	"ALWAYS"
#define DEFAULT_TTL		0	//0 -> infinite, >0 -> time to live in hours
#define CONTEXT_KEY		"strategy"
#define TTL_KEY			"ttl"

#include <tango.h>
#include "Consts.h"
#include <mutex>
#include <shared_mutex>

/**
 * @author	$Author: graziano $
 * @version	$Revision: 1.5 $
 */

 //	constants definitions here.
 //-----------------------------------------------
#define STATUS_SUBSCRIBED	string("Subscribed")
#define STATUS_DB_ERROR		string("Storing Error")


namespace HdbEventSubscriber_ns
{

class PollerThread;
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
    private:
        std::string current_context;
        std::chrono::time_point<std::chrono::system_clock> last_stat;
        std::mutex attributes_mutex;
	
#ifdef _USE_FERMI_DB_RW
	string host_rw;
	long port_rw;
#endif

    public:
	//	Data members here
	//-----------------------------------------
	std::unique_ptr<SubscribeThread, std::function<void(SubscribeThread*)>> thread;
	std::unique_ptr<PushThread, std::function<void(PushThread*)>> push_thread;
	std::unique_ptr<PollerThread, std::function<void(PollerThread*)>> poller_thread;
	int					period;
	int					poller_period;
	int					stats_window;
	int					check_periodic_delay;
	bool				subscribe_change;
	bool				list_from_file;
	string				list_file_error;
	string				list_filename;
	/**
	 *	Shared data
	 */
	std::shared_ptr<SharedData> shared;
	Tango::DeviceImpl *_device;
        std::map<std::string, std::string> domain_map;

	Tango::DevULong	attr_AttributeOkNumber_read;
	Tango::DevULong	attr_AttributeNokNumber_read;
	Tango::DevULong	attr_AttributeNumber_read;
	Tango::DevULong	attr_AttributeStartedNumber_read;
	Tango::DevULong	attr_AttributePausedNumber_read;
	Tango::DevULong	attr_AttributeStoppedNumber_read;

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
	HdbDevice(int p, int pp, int s, int c, bool ch, const string &fn, Tango::DeviceImpl *device);
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
	auto get_event_number_list(std::vector<unsigned int>& ret) -> void;
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
	/**
	 *	Set current context and start the attributes that should start.
	 */
	void set_context_and_start_attributes(const string& context);
	/**
	 *	Start an attribute.
	 */
	void start_attribute(const string& attribute);
	/**
	 *	Stop an attribute.
	 */
	void stop_attribute(const string& attribute);
	/**
	 *	Pause an attribute.
	 */
	void pause_attribute(const string& attribute);

        auto get_last_stat() -> std::chrono::time_point<std::chrono::system_clock>
        {
            return last_stat;
        };
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

        /**
         * To be called after an attribute has been removed to update the various attributes.
         * Ex: decrease number of attribute, remove it from the stats and reindex lists.
         */
        auto remove_attribute(size_t idx, bool running, bool paused, bool stopped) -> void;
        
        auto get_record_freq() -> double;
        auto get_failure_freq() -> double;
        auto get_record_freq_list(std::vector<double>& ret) -> void;
        auto get_failure_freq_list(std::vector<double>& ret) -> void;

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
    using namespace std::chrono_literals;
    if(sleep)
        usleep(std::chrono::microseconds(1ms).count());
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
    using namespace std::chrono_literals;
    if(sleep)
        usleep(std::chrono::microseconds(1ms).count());
}
}	// namespace_ns

#endif	// _HDBDEVICE_H
