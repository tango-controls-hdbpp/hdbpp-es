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

#include <tango.h>
#include <stdint.h>

#include "LibHdb++.h"


namespace HdbEventSubscriber_ns
{


typedef struct
{
	string	name;
	uint32_t nokdb_counter;
	uint32_t okdb_counter;
	Tango::DevState dbstate;
	double process_time_avg;
	double store_time_avg;
}
HdbStat;

	
//=========================================================
/**
 *	Shared data between DS and thread.
 */
//=========================================================
class PushThreadShared: public Tango::TangoMonitor
{
private:
	/**
	 *	Manage data to write.
	 */
	vector<HdbCmdData *>	events;
	/**
	 *	Manage exceptions if any
	 */
	vector<Tango::DevFailed>	except;
	 /**
	  *	Number maxi of command waiting since reset.
	  */
	int	max_waiting;
	bool	stop_it;

	string remove_domain(string str);
	bool compare_without_domain(string str1, string str2);

public:
	//PushThreadShared() { max_waiting=0; stop_it=false;};
	PushThreadShared(string host, string user, string password, string dbname, int port);
	~PushThreadShared();

	void push_back_cmd(HdbCmdData *argin);
	//void push_back_cmd(Tango::EventData argin);
	void remove_cmd();
	int nb_cmd_waiting();
	HdbCmdData *get_next_cmd();

	int get_max_waiting();
	vector<string> get_sig_list_waiting();
	void reset_statistics();
	void stop_thread();
	bool get_if_stop();

	void  remove(string &signame);
	/**
	 *	Return the list of signals on error
	 */
	vector<string>  get_sig_on_error_list();
	/**
	 *	Return the list of signals not on error
	 */
	vector<string>  get_sig_not_on_error_list();
	/**
	 *	Return the number of signals on error
	 */
	int  get_sig_on_error_num();
	/**
	 *	Return the number of signals not on error
	 */
	int  get_sig_not_on_error_num();
	/**
	 *	Return the db state of the signal
	 */
	Tango::DevState  get_sig_state(string signame);

	/**
	 *	Increment the error counter of db saving
	 */
	void  set_nok_db(string &signame);
	/**
	 *	Get the error counter of db saving
	 */
	uint32_t  get_nok_db(string &signame);
	/**
	 *	Get avg store time
	 */
	double  get_avg_store_time(string &signame);
	/**
	 *	Get avg process time
	 */
	double  get_avg_process_time(string &signame);
	/**
	 *	reset state
	 */
	void  set_ok_db(string &signame, double store_time, double process_time);

	void  start_attr(string &signame);
	void  stop_attr(string &signame);
	void  remove_attr(string &signame);
	void  start_all();
	void  stop_all();

	/**
	 *	Return ALARM if at list one signal is not saving in DB.
	 */
	Tango::DevState state();

	omni_mutex *sig_lock;
	vector<HdbStat>	signals;

	HdbClient *mdb;


};



//=========================================================
/**
 *	Create a thread to write data read from shared vector.
 */
//=========================================================
class PushThread: public omni_thread
{
	PushThreadShared	*shared;

public:
/**
 *	Initialize the sub process parameters (name, domain, log_file).
 */
	PushThread(PushThreadShared	*pts) \
			{ shared=pts;};
	
/**
 * Execute the fork of the sub process in a thread.
 */
	void *run_undetached(void *);
	void start() {start_undetached();}
};

}	//	namespace

#endif	// _PushThread_H
