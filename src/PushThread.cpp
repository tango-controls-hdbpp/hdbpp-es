static const char *RcsId = "$Header: /home/cvsadm/cvsroot/fermi/servers/hdb++/hdb++es/src/PushThread.cpp,v 1.7 2014-03-06 15:21:43 graziano Exp $";
//+=============================================================================
//
// file :         HdbEventHandler.cpp
//
// description :  C++ source for the HdbDevice
// project :      TANGO Device Server
//
// $Author: graziano $
//
// $Revision: 1.7 $
//
// $Log: PushThread.cpp,v $
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
// Revision 1.4  2013-09-02 12:15:34  graziano
// libhdb refurbishing, cleanings
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
//-=============================================================================

#include <PushThread.h>
#include <HdbDevice.h>

namespace HdbEventSubscriber_ns
{
//=============================================================================
//=============================================================================
PushThreadShared::PushThreadShared(HdbDevice *dev, vector<string> configuration):Tango::LogAdapter(dev->_device)
{
	max_waiting=0; stop_it=false;

	try
	{
		mdb = new HdbClient(configuration);
	}
	catch (Tango::DevFailed &err)
	{
		FATAL_STREAM << __func__ << ": error connecting DB: " << err.errors[0].desc << endl;
		exit(-1);
	}
	hdb_dev = dev;
	sig_lock = new omni_mutex();
}
//=============================================================================
//=============================================================================
PushThreadShared::~PushThreadShared()
{
	delete mdb;
	delete sig_lock;
}
//=============================================================================
//=============================================================================
void PushThreadShared::push_back_cmd(HdbCmdData *argin)
{

	omni_mutex_lock sync(*this);
	//	Add data at end of vector

	events.push_back(argin);
	size_t events_size = events.size();

	//	Check if nb waiting more the stored one.
	if (events_size>(unsigned )max_waiting)
		max_waiting = events_size;

	hdb_dev->AttributePendingNumber = events_size;
	hdb_dev->AttributeMaxPendingNumber = max_waiting;
#if 0	//TODO: sometimes deadlock: Not able to acquire serialization (dev, class or process) monitor
	try
	{
		(hdb_dev->_device)->push_change_event("AttributePendingNumber",&hdb_dev->AttributePendingNumber);
		(hdb_dev->_device)->push_archive_event("AttributePendingNumber",&hdb_dev->AttributePendingNumber);
		(hdb_dev->_device)->push_change_event("AttributeMaxPendingNumber",&hdb_dev->AttributeMaxPendingNumber);
		(hdb_dev->_device)->push_archive_event("AttributeMaxPendingNumber",&hdb_dev->AttributeMaxPendingNumber);
	}
	catch(Tango::DevFailed &e)
	{
		INFO_STREAM <<"PushThreadShared::"<< __func__<<": error pushing events="<<e.errors[0].desc<<endl;
	}
#endif
	//	And awake thread
	signal();
}
//=============================================================================
//=============================================================================
/*void PushThreadShared::remove_cmd()
{
	omni_mutex_lock sync(*this);
	//	Remove first element of vector
	events.erase(events.begin());
}*/
//=============================================================================
//=============================================================================
int PushThreadShared::nb_cmd_waiting()
{
	omni_mutex_lock sync(*this);
	return events.size();
}
//=============================================================================
//=============================================================================
int PushThreadShared::get_max_waiting()
{
	omni_mutex_lock sync(*this);
	int	tmp_max_waiting = max_waiting;
//	max_waiting = events.size();
	return tmp_max_waiting;
}
//=============================================================================
//=============================================================================
void PushThreadShared::get_sig_list_waiting(vector<string> & list)
{
	omni_mutex_lock sync(*this);
	list.clear();
	for (unsigned int i=0 ; i<events.size() ; i++)
	{
		HdbCmdData *ev = events[i];
		string	signame;
		if(ev->op_code == DB_INSERT)
			signame = string(ev->ev_data->attr_name);
		else if(ev->op_code == DB_INSERT_PARAM)
			signame = string(ev->ev_data_param->attr_name);
		else
			signame = string(ev->attr_name);
		list.push_back(signame);
	}
	return;
}
//=============================================================================
//=============================================================================
void PushThreadShared::reset_statistics()
{
	sig_lock->lock();
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		signals[i].nokdb_counter = 0;
		signals[i].okdb_counter = 0;
		signals[i].store_time_avg = 0;
		signals[i].store_time_min = -1;
		signals[i].store_time_max = -1;
		signals[i].process_time_avg = 0;
		signals[i].process_time_min = -1;
		signals[i].process_time_max = -1;
	}
	hdb_dev->attr_AttributeMinStoreTime_read = -1;
	hdb_dev->attr_AttributeMaxStoreTime_read = -1;
	hdb_dev->attr_AttributeMinProcessingTime_read = -1;
	hdb_dev->attr_AttributeMaxProcessingTime_read = -1;
	max_waiting = 0;
	sig_lock->unlock();
}
//=============================================================================
//=============================================================================
void PushThreadShared::reset_freq_statistics()
{
	sig_lock->lock();
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		signals[i].nokdb_counter_freq = 0;
	}
	sig_lock->unlock();
}
//=============================================================================
//=============================================================================
HdbCmdData *PushThreadShared::get_next_cmd()
{
	omni_mutex_lock sync(*this);
	size_t events_size = events.size();
	hdb_dev->AttributePendingNumber = events_size;
#if 0	//TODO: disabled because of problems with: Not able to acquire serialization (dev, class or process) monitor
	try
	{
		(hdb_dev->_device)->push_change_event("AttributePendingNumber",&hdb_dev->AttributePendingNumber);
		(hdb_dev->_device)->push_archive_event("AttributePendingNumber",&hdb_dev->AttributePendingNumber);
	}
	catch(Tango::DevFailed &e)
	{

	}
#endif
	if(events_size>0)
	{
		HdbCmdData *cmd = events[0];
		events.erase(events.begin());
		return cmd;
	}
	else
	{
		return NULL;
	}
}
//=============================================================================
//=============================================================================
void PushThreadShared::stop_thread()
{
	omni_mutex_lock sync(*this);
	stop_it = true;
	signal();
}
//=============================================================================
//=============================================================================
bool PushThreadShared::get_if_stop()
{
	omni_mutex_lock sync(*this);
	return stop_it;
}
//=============================================================================
/**
 *
 */
//=============================================================================
void  PushThreadShared::remove(string &signame)
{
	sig_lock->lock();
	unsigned int i;
	vector<HdbStat>::iterator pos = signals.begin();
	for (i=0 ; i<signals.size() ; i++, pos++)
	{
		if (signals[i].name==signame)
		{
			signals.erase(pos);
			sig_lock->unlock();
			return;
		}
	}
	pos = signals.begin();
	for (i=0 ; i<signals.size() ; i++, pos++)
	{
#ifndef _MULTI_TANGO_HOST
		if (hdb_dev->compare_without_domain(signals[i].name,signame))
#else
		if (!hdb_dev->compare_tango_names(signals[i].name,signame))
#endif
		{
			signals.erase(pos);
			sig_lock->unlock();
			return;
		}
	}
	sig_lock->unlock();
}

//=============================================================================
/**
 *	Return the list of signals on error
 */
//=============================================================================
vector<string>  PushThreadShared::get_sig_on_error_list()
{
	sig_lock->lock();
	vector<string>	list;
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].dbstate==Tango::ALARM)
		{
			string	signame(signals[i].name);
			list.push_back(signame);
		}
	}

	sig_lock->unlock();
	return list;
}
//=============================================================================
/**
 *	Return the number of signals on error
 */
//=============================================================================
int  PushThreadShared::get_sig_on_error_num()
{
	sig_lock->lock();
	int num=0;
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].dbstate==Tango::ALARM)
		{
			num++;
		}
	}
	sig_lock->unlock();
	return num;
}
//=============================================================================
/**
 *	Return the list of signals not on error
 */
//=============================================================================
vector<string>  PushThreadShared::get_sig_not_on_error_list()
{
	sig_lock->lock();
	vector<string>	list;
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].dbstate==Tango::ON)
		{
			string	signame(signals[i].name);
			list.push_back(signame);
		}
	}
	sig_lock->unlock();
	return list;
}
//=============================================================================
/**
 *	Return the number of signals not on error
 */
//=============================================================================
int  PushThreadShared::get_sig_not_on_error_num()
{
	sig_lock->lock();
	int num=0;
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].dbstate==Tango::ON)
		{
			num++;
		}
	}
	sig_lock->unlock();
	return num;
}
//=============================================================================
/**
 *	Return the db state of the signal
 */
//=============================================================================
Tango::DevState  PushThreadShared::get_sig_state(string signame)
{
	sig_lock->lock();
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			Tango::DevState ret = signals[i].dbstate;
			sig_lock->unlock();
			return ret;
		}
	}
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
#ifndef _MULTI_TANGO_HOST
		if (hdb_dev->compare_without_domain(signals[i].name,signame))
#else
		if (!hdb_dev->compare_tango_names(signals[i].name,signame))
#endif
		{
			Tango::DevState ret = signals[i].dbstate;
			sig_lock->unlock();
			return ret;
		}
	}

	sig_lock->unlock();
	return Tango::ON;
}
//=============================================================================
/**
 *	Return the db error status of the signal
 */
//=============================================================================
string  PushThreadShared::get_sig_status(string signame)
{
	sig_lock->lock();
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			string ret = signals[i].dberror;
			sig_lock->unlock();
			return ret;
		}
	}
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
#ifndef _MULTI_TANGO_HOST
		if (hdb_dev->compare_without_domain(signals[i].name,signame))
#else
		if (!hdb_dev->compare_tango_names(signals[i].name,signame))
#endif
		{
			string ret = signals[i].dberror;
			sig_lock->unlock();
			return ret;
		}
	}

	sig_lock->unlock();
	return STATUS_DB_ERROR;
}
//=============================================================================
/**
 *	Increment the error counter of db saving
 */
//=============================================================================
void  PushThreadShared::set_nok_db(string &signame, string error)
{
	sig_lock->lock();
	unsigned int i;
	for (i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			signals[i].nokdb_counter++;
			signals[i].nokdb_counter_freq++;
			signals[i].dbstate = Tango::ALARM;
			signals[i].dberror = STATUS_DB_ERROR;
			if(error.length() > 0)
				signals[i].dberror += ": " + error;
			gettimeofday(&signals[i].last_nokdb, NULL);
			sig_lock->unlock();
			return;
		}
	}
	for (i=0 ; i<signals.size() ; i++)
	{
#ifndef _MULTI_TANGO_HOST
		if (hdb_dev->compare_without_domain(signals[i].name,signame))
#else
		if (!hdb_dev->compare_tango_names(signals[i].name,signame))
#endif
		{
			signals[i].nokdb_counter++;
			signals[i].nokdb_counter_freq++;
			signals[i].dbstate = Tango::ALARM;
			signals[i].dberror = STATUS_DB_ERROR;
			if(error.length() > 0)
				signals[i].dberror += ": " + error;
			gettimeofday(&signals[i].last_nokdb, NULL);
			sig_lock->unlock();
			return;
		}
	}
	if(i == signals.size())
	{
		HdbStat sig;
		sig.name = signame;
		sig.nokdb_counter = 1;
		sig.nokdb_counter_freq = 1;
		sig.okdb_counter = 0;
		sig.store_time_avg = 0.0;
		sig.store_time_min = -1;
		sig.store_time_max = -1;
		sig.process_time_avg = 0.0;
		sig.process_time_min = -1;
		sig.process_time_max = -1;
		sig.dbstate = Tango::ALARM;
		sig.dberror = STATUS_DB_ERROR;
		if(error.length() > 0)
			sig.dberror += ": " + error;
		gettimeofday(&sig.last_nokdb, NULL);
		signals.push_back(sig);
	}
	sig_lock->unlock();
}
//=============================================================================
/**
 *	Get the error counter of db saving
 */
//=============================================================================
uint32_t  PushThreadShared::get_nok_db(string &signame)
{
	sig_lock->lock();
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			uint32_t ret = signals[i].nokdb_counter;
			sig_lock->unlock();
			return ret;
		}
	}
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
#ifndef _MULTI_TANGO_HOST
		if (hdb_dev->compare_without_domain(signals[i].name,signame))
#else
		if (!hdb_dev->compare_tango_names(signals[i].name,signame))
#endif
		{
			uint32_t ret = signals[i].nokdb_counter;
			sig_lock->unlock();
			return ret;
		}
	}
	sig_lock->unlock();
	return 0;
	//	if not found
	/*Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal NOT subscribed",
				(const char *)"SharedData::get_nok_db()");*/
}
//=============================================================================
/**
 *	Get the error counter of db saving for freq stats
 */
//=============================================================================
uint32_t  PushThreadShared::get_nok_db_freq(string &signame)
{
	sig_lock->lock();
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			uint32_t ret = signals[i].nokdb_counter_freq;
			sig_lock->unlock();
			return ret;
		}
	}
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
#ifndef _MULTI_TANGO_HOST
		if (hdb_dev->compare_without_domain(signals[i].name,signame))
#else
		if (!hdb_dev->compare_tango_names(signals[i].name,signame))
#endif
		{
			uint32_t ret = signals[i].nokdb_counter_freq;
			sig_lock->unlock();
			return ret;
		}
	}
	sig_lock->unlock();
	return 0;
	//	if not found
	/*Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal NOT subscribed",
				(const char *)"SharedData::get_nok_db()");*/
}
//=============================================================================
/**
 *	Get avg store time
 */
//=============================================================================
double  PushThreadShared::get_avg_store_time(string &signame)
{
	//omni_mutex_lock sync(*this);
	sig_lock->lock();
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			double ret = signals[i].store_time_avg;
			sig_lock->unlock();
			return ret;
		}
	}
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
#ifndef _MULTI_TANGO_HOST
		if (hdb_dev->compare_without_domain(signals[i].name,signame))
#else
		if (!hdb_dev->compare_tango_names(signals[i].name,signame))
#endif
		{
			double ret = signals[i].store_time_avg;
			sig_lock->unlock();
			return ret;
		}
	}
	sig_lock->unlock();

	return -1;
}
//=============================================================================
/**
 *	Get min store time
 */
//=============================================================================
double  PushThreadShared::get_min_store_time(string &signame)
{
	//omni_mutex_lock sync(*this);
	sig_lock->lock();
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			double ret = signals[i].store_time_min;
			sig_lock->unlock();
			return ret;
		}
	}
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
#ifndef _MULTI_TANGO_HOST
		if (hdb_dev->compare_without_domain(signals[i].name,signame))
#else
		if (!hdb_dev->compare_tango_names(signals[i].name,signame))
#endif
		{
			double ret = signals[i].store_time_min;
			sig_lock->unlock();
			return ret;
		}
	}
	sig_lock->unlock();

	return -1;
}
//=============================================================================
/**
 *	Get max store time
 */
//=============================================================================
double  PushThreadShared::get_max_store_time(string &signame)
{
	//omni_mutex_lock sync(*this);
	sig_lock->lock();
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			double ret = signals[i].store_time_max;
			sig_lock->unlock();
			return ret;
		}
	}
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
#ifndef _MULTI_TANGO_HOST
		if (hdb_dev->compare_without_domain(signals[i].name,signame))
#else
		if (!hdb_dev->compare_tango_names(signals[i].name,signame))
#endif
		{
			double ret = signals[i].store_time_max;
			sig_lock->unlock();
			return ret;
		}
	}
	sig_lock->unlock();

	return -1;
}
//=============================================================================
/**
 *	Get avg process time
 */
//=============================================================================
double  PushThreadShared::get_avg_process_time(string &signame)
{
	//omni_mutex_lock sync(*this);
	sig_lock->lock();
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			double ret = signals[i].process_time_avg;
			sig_lock->unlock();
			return ret;
		}
	}
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
#ifndef _MULTI_TANGO_HOST
		if (hdb_dev->compare_without_domain(signals[i].name,signame))
#else
		if (!hdb_dev->compare_tango_names(signals[i].name,signame))
#endif
		{
			double ret = signals[i].process_time_avg;
			sig_lock->unlock();
			return ret;
		}
	}
	sig_lock->unlock();

	return -1;
}
//=============================================================================
/**
 *	Get min process time
 */
//=============================================================================
double  PushThreadShared::get_min_process_time(string &signame)
{
	//omni_mutex_lock sync(*this);
	sig_lock->lock();
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			double ret = signals[i].process_time_min;
			sig_lock->unlock();
			return ret;
		}
	}
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
#ifndef _MULTI_TANGO_HOST
		if (hdb_dev->compare_without_domain(signals[i].name,signame))
#else
		if (!hdb_dev->compare_tango_names(signals[i].name,signame))
#endif
		{
			double ret = signals[i].process_time_min;
			sig_lock->unlock();
			return ret;
		}
	}
	sig_lock->unlock();

	return -1;
}
//=============================================================================
/**
 *	Get max process time
 */
//=============================================================================
double  PushThreadShared::get_max_process_time(string &signame)
{
	//omni_mutex_lock sync(*this);
	sig_lock->lock();
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			double ret = signals[i].process_time_max;
			sig_lock->unlock();
			return ret;
		}
	}
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
#ifndef _MULTI_TANGO_HOST
		if (hdb_dev->compare_without_domain(signals[i].name,signame))
#else
		if (!hdb_dev->compare_tango_names(signals[i].name,signame))
#endif
		{
			double ret = signals[i].process_time_max;
			sig_lock->unlock();
			return ret;
		}
	}
	sig_lock->unlock();

	return -1;
}
//=============================================================================
/**
 *	Get last nokdb timestamp
 */
//=============================================================================
timeval  PushThreadShared::get_last_nokdb(string &signame)
{
	sig_lock->lock();
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			timeval ret = signals[i].last_nokdb;
			sig_lock->unlock();
			return ret;
		}
	}
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
#ifndef _MULTI_TANGO_HOST
		if (hdb_dev->compare_without_domain(signals[i].name,signame))
#else
		if (!hdb_dev->compare_tango_names(signals[i].name,signame))
#endif
		{
			timeval ret = signals[i].last_nokdb;
			sig_lock->unlock();
			return ret;
		}
	}
	sig_lock->unlock();
	timeval ret;
	ret.tv_sec=0;
	ret.tv_usec=0;
	return ret;
}
//=============================================================================
/**
 *	reset state
 */
//=============================================================================
void  PushThreadShared::set_ok_db(string &signame, double store_time, double process_time)
{
	sig_lock->lock();
	unsigned int i;
	for (i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			signals[i].dbstate = Tango::ON;
			signals[i].dberror = "";
			signals[i].store_time_avg = ((signals[i].store_time_avg * signals[i].okdb_counter) + store_time)/(signals[i].okdb_counter+1);
			//signal store min
			if(signals[i].store_time_min == -1)
				signals[i].store_time_min = store_time;
			else if(store_time < signals[i].store_time_min)
				signals[i].store_time_min = store_time;
			//global store min
			if(hdb_dev->attr_AttributeMinStoreTime_read == -1)
				hdb_dev->attr_AttributeMinStoreTime_read = store_time;
			else if(store_time < hdb_dev->attr_AttributeMinStoreTime_read)
				hdb_dev->attr_AttributeMinStoreTime_read = store_time;
			//signal store max
			if(signals[i].store_time_max == -1)
				signals[i].store_time_max = store_time;
			else if(store_time > signals[i].store_time_max)
				signals[i].store_time_max = store_time;
			//global store max
			if(hdb_dev->attr_AttributeMaxStoreTime_read == -1)
				hdb_dev->attr_AttributeMaxStoreTime_read = store_time;
			else if(store_time > hdb_dev->attr_AttributeMaxStoreTime_read)
				hdb_dev->attr_AttributeMaxStoreTime_read = store_time;

			signals[i].process_time_avg = ((signals[i].process_time_avg * signals[i].okdb_counter) + process_time)/(signals[i].okdb_counter+1);
			//signal process min
			if(signals[i].process_time_min == -1)
				signals[i].process_time_min = process_time;
			else if(process_time < signals[i].process_time_min)
				signals[i].process_time_min = process_time;
			//global process min
			if(hdb_dev->attr_AttributeMinProcessingTime_read == -1)
				hdb_dev->attr_AttributeMinProcessingTime_read = process_time;
			else if(process_time < hdb_dev->attr_AttributeMinProcessingTime_read)
				hdb_dev->attr_AttributeMinProcessingTime_read = process_time;
			//signal process max
			if(signals[i].process_time_max == -1)
				signals[i].process_time_max = process_time;
			else if(process_time > signals[i].process_time_max)
				signals[i].process_time_max = process_time;
			//global process max
			if(hdb_dev->attr_AttributeMaxProcessingTime_read == -1)
				hdb_dev->attr_AttributeMaxProcessingTime_read = process_time;
			else if(process_time > hdb_dev->attr_AttributeMaxProcessingTime_read)
				hdb_dev->attr_AttributeMaxProcessingTime_read = process_time;
			signals[i].okdb_counter++;
			sig_lock->unlock();
			return;
		}
	}
	for (i=0 ; i<signals.size() ; i++)
	{
#ifndef _MULTI_TANGO_HOST
		if (hdb_dev->compare_without_domain(signals[i].name,signame))
#else
		if (!hdb_dev->compare_tango_names(signals[i].name,signame))
#endif
		{
			signals[i].dbstate = Tango::ON;
			signals[i].dberror = "";
			signals[i].store_time_avg = ((signals[i].store_time_avg * signals[i].okdb_counter) + store_time)/(signals[i].okdb_counter+1);
			//signal store min
			if(signals[i].store_time_min == -1)
				signals[i].store_time_min = store_time;
			else if(store_time < signals[i].store_time_min)
				signals[i].store_time_min = store_time;
			//global store min
			if(hdb_dev->attr_AttributeMinStoreTime_read == -1)
				hdb_dev->attr_AttributeMinStoreTime_read = store_time;
			else if(store_time < hdb_dev->attr_AttributeMinStoreTime_read)
				hdb_dev->attr_AttributeMinStoreTime_read = store_time;
			//signal store max
			if(signals[i].store_time_max == -1)
				signals[i].store_time_max = store_time;
			else if(store_time > signals[i].store_time_max)
				signals[i].store_time_max = store_time;
			//global store max
			if(hdb_dev->attr_AttributeMaxStoreTime_read == -1)
				hdb_dev->attr_AttributeMaxStoreTime_read = store_time;
			else if(store_time > hdb_dev->attr_AttributeMaxStoreTime_read)
				hdb_dev->attr_AttributeMaxStoreTime_read = store_time;
			signals[i].process_time_avg = ((signals[i].process_time_avg * signals[i].okdb_counter) + process_time)/(signals[i].okdb_counter+1);
			//signal process min
			if(signals[i].process_time_min == -1)
				signals[i].process_time_min = process_time;
			else if(process_time < signals[i].process_time_min)
				signals[i].process_time_min = process_time;
			//global process min
			if(hdb_dev->attr_AttributeMinProcessingTime_read == -1)
				hdb_dev->attr_AttributeMinProcessingTime_read = process_time;
			else if(process_time < hdb_dev->attr_AttributeMinProcessingTime_read)
				hdb_dev->attr_AttributeMinProcessingTime_read = process_time;
			//signal process max
			if(signals[i].process_time_max == -1)
				signals[i].process_time_max = process_time;
			else if(process_time > signals[i].process_time_max)
				signals[i].process_time_max = process_time;
			//global process max
			if(hdb_dev->attr_AttributeMaxProcessingTime_read == -1)
				hdb_dev->attr_AttributeMaxProcessingTime_read = process_time;
			else if(process_time > hdb_dev->attr_AttributeMaxProcessingTime_read)
				hdb_dev->attr_AttributeMaxProcessingTime_read = process_time;
			signals[i].okdb_counter++;
			sig_lock->unlock();
			return;
		}
	}
	if(i == signals.size())
	{
		HdbStat sig;
		sig.name = signame;
		sig.nokdb_counter = 0;
		sig.nokdb_counter_freq = 0;
		sig.okdb_counter = 1;
		sig.store_time_avg = store_time;
		sig.store_time_min = store_time;
		sig.store_time_max = store_time;
		sig.process_time_avg = process_time;
		sig.process_time_min = process_time;
		sig.process_time_max = process_time;
		sig.dbstate = Tango::ON;
		sig.dberror = "";
		signals.push_back(sig);
		//global store min
		if(hdb_dev->attr_AttributeMinStoreTime_read == -1)
			hdb_dev->attr_AttributeMinStoreTime_read = store_time;
		else if(store_time < hdb_dev->attr_AttributeMinStoreTime_read)
			hdb_dev->attr_AttributeMinStoreTime_read = store_time;
		//global store max
		if(hdb_dev->attr_AttributeMaxStoreTime_read == -1)
			hdb_dev->attr_AttributeMaxStoreTime_read = store_time;
		else if(store_time > hdb_dev->attr_AttributeMaxStoreTime_read)
			hdb_dev->attr_AttributeMaxStoreTime_read = store_time;
		//global process min
		if(hdb_dev->attr_AttributeMinProcessingTime_read == -1)
			hdb_dev->attr_AttributeMinProcessingTime_read = process_time;
		else if(process_time < hdb_dev->attr_AttributeMinProcessingTime_read)
			hdb_dev->attr_AttributeMinProcessingTime_read = process_time;
		//global process max
		if(hdb_dev->attr_AttributeMaxProcessingTime_read == -1)
			hdb_dev->attr_AttributeMaxProcessingTime_read = process_time;
		else if(process_time > hdb_dev->attr_AttributeMaxProcessingTime_read)
			hdb_dev->attr_AttributeMaxProcessingTime_read = process_time;
	}
	sig_lock->unlock();
}

void  PushThreadShared::start_attr(string &signame)
{
	//------Configure DB------------------------------------------------
	HdbCmdData *cmd = new HdbCmdData(DB_START, signame);
	push_back_cmd(cmd);
}

void  PushThreadShared::pause_attr(string &signame)
{
	//------Configure DB------------------------------------------------
	HdbCmdData *cmd = new HdbCmdData(DB_PAUSE, signame);
	push_back_cmd(cmd);
}

void  PushThreadShared::stop_attr(string &signame)
{
	//------Configure DB------------------------------------------------
	HdbCmdData *cmd = new HdbCmdData(DB_STOP, signame);
	push_back_cmd(cmd);
}

void  PushThreadShared::remove_attr(string &signame)
{
	//------Configure DB------------------------------------------------
	HdbCmdData *cmd = new HdbCmdData(DB_REMOVE, signame);
	push_back_cmd(cmd);
}

void  PushThreadShared::updatettl(string &signame, Tango::DevULong ttl)
{
	//------Configure DB------------------------------------------------
	HdbCmdData *cmd = new HdbCmdData(DB_UPDATETTL, ttl, signame);
	push_back_cmd(cmd);
}

void  PushThreadShared::start_all()
{
	sig_lock->lock();
	unsigned int i;
	for (i=0 ; i<signals.size() ; i++)
	{
		start_attr(signals[i].name);
	}
	sig_lock->unlock();
}

void  PushThreadShared::pause_all()
{
	sig_lock->lock();
	unsigned int i;
	for (i=0 ; i<signals.size() ; i++)
	{
		pause_attr(signals[i].name);
	}
	sig_lock->unlock();
}

void  PushThreadShared::stop_all()
{
	sig_lock->lock();
	unsigned int i;
	for (i=0 ; i<signals.size() ; i++)
	{
		stop_attr(signals[i].name);
	}
	sig_lock->unlock();
}

//=============================================================================
/**
 *	Return ALARM if at list one signal is not subscribed.
 */
//=============================================================================
Tango::DevState PushThreadShared::state()
{
	sig_lock->lock();
	Tango::DevState	state = Tango::ON;
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].dbstate==Tango::ALARM)
		{
			state = Tango::ALARM;
			break;
		}
	}
	sig_lock->unlock();
	return state;
}


//=============================================================================
//=============================================================================
PushThread::PushThread(PushThreadShared	*pts, HdbDevice *dev) : Tango::LogAdapter(dev->_device)
{
	shared=pts;
};

//=============================================================================
/**
 * Execute the thread infinite loop.
 */
//=============================================================================
void *PushThread::run_undetached(void *ptr)
{
	while(shared->get_if_stop()==false)
	{
		//	Check if command ready
		HdbCmdData	*cmd;
		while ((cmd=shared->get_next_cmd())!=NULL)
		{
			switch(cmd->op_code)
			{
				case DB_INSERT:
				{
					timeval now;
					gettimeofday(&now, NULL);
					double	dstart = now.tv_sec + (double)now.tv_usec/1.0e6;
					try
					{
						shared->mdb->insert_Attr(cmd->ev_data, cmd->ev_data_type);

						gettimeofday(&now, NULL);
						double  dnow = now.tv_sec + (double)now.tv_usec/1.0e6;
						double  rcv_time = cmd->ev_data->get_date().tv_sec + (double)cmd->ev_data->get_date().tv_usec/1.0e6;
						shared->set_ok_db(cmd->ev_data->attr_name, dnow-dstart, dnow-rcv_time);
					}
					catch(Tango::DevFailed  &e)
					{
						shared->set_nok_db(cmd->ev_data->attr_name, string(e.errors[0].desc));
						Tango::Except::print_exception(e);
					}
					break;
				}
				case DB_INSERT_PARAM:
				{
					try
					{
						//	Send it to DB
						shared->mdb->insert_param_Attr(cmd->ev_data_param, cmd->ev_data_type);
					}
					catch(Tango::DevFailed  &e)
					{
						ERROR_STREAM << "PushThread::run_undetached: An error was detected when inserting attribute parameter for: "
									 << cmd->ev_data->attr_name << endl;

						Tango::Except::print_exception(e);
					}
					break;
				}
				case DB_START:
				case DB_STOP:
				case DB_PAUSE:
				case DB_REMOVE:
				{
					try
					{
						//	Send it to DB
						shared->mdb->event_Attr(cmd->attr_name, cmd->op_code);
					}
					catch(Tango::DevFailed  &e)
					{
						ERROR_STREAM << "PushThread::run_undetached: An was error detected when removing attribute: "
									 << cmd->ev_data->attr_name << endl;

						Tango::Except::print_exception(e);
					}
					break;
				}
				case DB_UPDATETTL:
				{
					try
					{
						//	Send it to DB
						shared->mdb->updateTTL_Attr(cmd->attr_name, cmd->ttl);
					}
					catch(Tango::DevFailed  &e)
					{
						ERROR_STREAM << "PushThread::run_undetached: An was error detected when updating the TTL on attribute: "
									 << cmd->ev_data->attr_name << endl;

						Tango::Except::print_exception(e);
					}
					break;
				}
			}
			delete cmd;

		}
		
		//	Wait until next command.
		if(shared->get_if_stop()==false)
		{
			omni_mutex_lock sync(*shared);
			//shared->wait();
			//DEBUG_STREAM <<"PushThread::"<< __func__<<": before shared->wait(2*1000)..."<<endl;
			shared->wait(2*1000);
		}
	}
	cout <<"PushThread::"<< __func__<<": exiting..."<<endl;
	return NULL;
}



//=============================================================================
//=============================================================================
}	//	namespace
