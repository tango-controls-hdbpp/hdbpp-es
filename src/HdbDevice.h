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


#include <tango.h>
#include <SubscribeThread.h>
#include <PushThread.h>
#include <StatsThread.h>
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
#define STATUS_DB_ERROR		string("DB Error")


namespace HdbEventSubscriber_ns
{

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
	string				status;
	SubscribeThread		*thread;
	PushThread			*push_thread;
	StatsThread			*stats_thread;
	int					period;
	int					stats_window;
	/**
	 *	Shared data
	 */
	SharedData			*shared;
	PushThreadShared	*push_shared;
	Tango::DeviceImpl 	*_device;
	bool startArchivingAtStartup;
	map<string, string> domain_map;

	Tango::DevDouble	AttributeRecordFreq;
	Tango::DevDouble	AttributeFailureFreq;
	Tango::DevDouble	AttributeRecordFreqList[10000];
	Tango::DevDouble	AttributeFailureFreqList[10000];
	Tango::DevLong		AttributeEventNumberList[10000];

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
	 */
	HdbDevice(int p, int s, Tango::DeviceImpl *device);
	~HdbDevice();
	/**
	 * initialize object
	 */
	void initialize();

	/**
	 * Add a new signal.
	 */
	void add(string &signame);
	/**
	 * AddRemove a signal in the list.
	 */
	void remove(string &signame);

	/**
	 *	Update SignalList property
	 */
	void put_signal_property(vector<string> &prop);

	/**
	 *	Return the list of signals
	 */
	vector<string> get_sig_list();
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
	 *	Return the list of signals not_started
	 */
	vector<string>  get_sig_not_started_list();
	/**
	 *	Return the list errors
	 */
	vector<string>  get_error_list();
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
	string  get_sig_status(string &signame);
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
	 int get_max_waiting();
	/**
	 *	Returns how many signals are waiting to be stored
	 */
	 int nb_cmd_waiting();
	/**
	 *	Returns the list of signals waiting to be stored
	 */
	 vector<string> get_sig_list_waiting();
	/**
	 *	Reset statistic counters
	 */
	 void reset_statistics();
	/**
	 *	Reset statistic freq counters
	 */
	 void reset_freq_statistics();
	/**
	 *	Returns the signal name (tango host has been added sinse tango 7.1.1)
	 */
	string	get_only_signal_name(string signame);
	/**
	 *	Returns the tango host (tango host has been added sinse tango 7.1.1)
	 */
	string	get_only_tango_host(string signame);
	/**
	 *	Check if fqdn, otherwise fix it
	 */
	void fix_tango_host(string &attr);
	/**
	 *	Check if full domain name, otherwise fix it
	 */
	void add_domain(string &attr);
	/**
	 *	Remove domain
	 */
	string remove_domain(string str);
#ifndef _MULTI_TANGO_HOST
	/**
	 *	Compare without domain
	 */
	bool compare_without_domain(string str1, string str2);
#else
	/**
	 *	compare 2 tango names considering fqdn, domain, multi tango host
	 *	returns 0 if equal
	 */
	int compare_tango_names(string str1, string str2);
	/**
	 *	explode a string in multiple strings using separator
	 */
	void string_explode(string str, string separator, vector<string>* results);
#endif

protected :	
	/**
	 * Read signal list in database as property.
	 *
	 */
	vector<string> get_hdb_signal_list();
	/**
	 * Build signal vector
	 *
	 *	@param list 	signal names vector
	 */
	void build_signal_vector(vector<string>);
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
	
};



//==========================================================
/**
 * Class Description:
 * This class manage archive events
 */
//==========================================================
class ArchiveCB : public Tango::CallBack
{
public:
	HdbDevice	*hdb_dev;
//	HdbSignal	*signal;
	ArchiveCB(HdbDevice	*dev) { hdb_dev=dev; };

	virtual void push_event(Tango::EventData *data);
	virtual void push_event(Tango::AttrConfEventData* data) {};
};


}	// namespace_ns

#endif	// _HDBDEVICE_H
