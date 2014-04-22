static const char *RcsId = "$Header: /home/cvsadm/cvsroot/fermi/servers/hdb++/hdb++es/src/SubscribeThread.cpp,v 1.6 2014-03-06 15:21:43 graziano Exp $";
//+=============================================================================
//
// file :         HdbEventHandler.cpp
//
// description :  C++ source for thread management
// project :      TANGO Device Server
//
// $Author: graziano $
//
// $Revision: 1.6 $
//
// $Log: SubscribeThread.cpp,v $
// Revision 1.6  2014-03-06 15:21:43  graziano
// StartArchivingAtStartup,
// start_all and stop_all,
// archiving of first event received at subscribe
//
// Revision 1.5  2014-02-20 14:59:02  graziano
// name and path fixing
// removed start acquisition from add
//
// Revision 1.4  2013-09-24 08:42:21  graziano
// bug fixing
//
// Revision 1.3  2013-09-02 12:13:22  graziano
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
// copyleft :     European Synchrotron Radiation Facility
//                BP 220, Grenoble 38043
//                FRANCE
//
//-=============================================================================


#include <HdbDevice.h>


namespace HdbEventSubscriber_ns
{

//=============================================================================
/**
 *	get signal by name.
 */
//=============================================================================
HdbSignal *SharedData::get_signal(string signame)
{
	//omni_mutex_lock sync(*this);
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		HdbSignal	*sig = &signals[i];
		if (sig->name==signame)
			return sig;
	}
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		HdbSignal	*sig = &signals[i];
		if (compare_without_domain(sig->name,signame))
			return sig;
	}
	return NULL;
}
//=============================================================================
/**
 * Remove a signal in the list.
 */
//=============================================================================
void SharedData::remove(string &signame)
{
	//	Remove in signals list (vector)
	{
		omni_mutex_lock sync(*this);
		vector<HdbSignal>::iterator	pos = signals.begin();
		
		
		bool	found = false;
		for (unsigned int i=0 ; i<signals.size() && !found ; i++, pos++)
		{
			HdbSignal	*sig = &signals[i];
			if (sig->name==signame)
			{
				found = true;
				cout <<__func__<< "removing " << signame << endl;
				try
				{
					if(sig->event_id != ERR)
					{
						sig->attr->unsubscribe_event(sig->event_id);
						delete sig->archive_cb;
					}
					delete sig->attr;
				}
				catch (Tango::DevFailed &e)
				{
					//	Do nothing
					//	Unregister failed means Register has also failed
				}
				cout << __func__<<": unsubscribed " << signame << endl;
				signals.erase(pos);
				break;
			}
		}
		pos = signals.begin();
		if (!found)
		{
			for (unsigned int i=0 ; i<signals.size() && !found ; i++, pos++)
			{
				HdbSignal	*sig = &signals[i];
				if (compare_without_domain(sig->name,signame))
				{
					found = true;
					cout <<__func__<< "removing " << signame << endl;
					try
					{
						if(sig->event_id != ERR)
						{
							sig->attr->unsubscribe_event(sig->event_id);
							delete sig->archive_cb;
						}
						delete sig->attr;
					}
					catch (Tango::DevFailed &e)
					{
						//	Do nothing
						//	Unregister failed means Register has also failed
					}
					cout << __func__<<": unsubscribed " << signame << endl;
					signals.erase(pos);
					break;
				}
			}
		}
		if (!found)
			Tango::Except::throw_exception(
						(const char *)"BadSignalName",
						"Signal NOT subscribed",
						(const char *)"SharedData::remove()");
	}
	//	then, update property
	action = UPDATE_PROP;
	put_signal_property();
}
//=============================================================================
/**
 * Start saving on DB a signal.
 */
//=============================================================================
void SharedData::start(string &signame)
{
	omni_mutex_lock sync(*this);
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			signals[i].running=true;
			return;
		}
	}
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (compare_without_domain(signals[i].name,signame))
		{
			signals[i].running=true;
			return;
		}
	}
	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal NOT subscribed",
				(const char *)"SharedData::start()");
}
//=============================================================================
/**
 * Stop saving on DB a signal.
 */
//=============================================================================
void SharedData::stop(string &signame)
{
	omni_mutex_lock sync(*this);
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			signals[i].running=false;
			return;
		}
	}
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (compare_without_domain(signals[i].name,signame))
		{
			signals[i].running=false;
			return;
		}
	}
	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal NOT subscribed",
				(const char *)"SharedData::stop()");
}
//=============================================================================
/**
 * Start saving on DB all signals.
 */
//=============================================================================
void SharedData::start_all()
{
	omni_mutex_lock sync(*this);
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		signals[i].running=true;

	}
}
//=============================================================================
/**
 * Stop saving on DB all signals.
 */
//=============================================================================
void SharedData::stop_all()
{
	omni_mutex_lock sync(*this);
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		signals[i].running=false;
	}
}
//=============================================================================
/**
 * Is a signal saved on DB?
 */
//=============================================================================
bool SharedData::is_running(string &signame)
{
	//to be locked if called outside lock in ArchiveCB::push_event
	for (unsigned int i=0 ; i<signals.size() ; i++)
		if (signals[i].name==signame)
			return signals[i].running;

	for (unsigned int i=0 ; i<signals.size() ; i++)
		if (compare_without_domain(signals[i].name,signame))
			return signals[i].running;

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal NOT subscribed",
				(const char *)"SharedData::is_running()");

	return true;
}
//=============================================================================
/**
 * Is a signal first event arrived?
 */
//=============================================================================
bool SharedData::is_first(string &signame)
{
	//not to be locked, called only inside lock in ArchiveCB::push_event
	for (unsigned int i=0 ; i<signals.size() ; i++)
		if (signals[i].name==signame)
			return signals[i].first;

	for (unsigned int i=0 ; i<signals.size() ; i++)
		if (compare_without_domain(signals[i].name,signame))
			return signals[i].first;

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal NOT subscribed",
				(const char *)"SharedData::is_first()");

	return true;
}
//=============================================================================
/**
 * Set a signal first event arrived
 */
//=============================================================================
void SharedData::set_first(string &signame)
{
	//not to be locked, called only inside lock in ArchiveCB::push_event
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			signals[i].first = false;
			return;
		}
	}

	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (compare_without_domain(signals[i].name,signame))
		{
			signals[i].first = false;
			return;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal NOT subscribed",
				(const char *)"SharedData::set_first()");

}
//=============================================================================
/**
 * Is a signal first consecutive error event arrived?
 */
//=============================================================================
bool SharedData::is_first_err(string &signame)
{
	//not to be locked, called only inside lock in ArchiveCB::push_event
	for (unsigned int i=0 ; i<signals.size() ; i++)
		if (signals[i].name==signame)
			return signals[i].first_err;

	for (unsigned int i=0 ; i<signals.size() ; i++)
		if (compare_without_domain(signals[i].name,signame))
			return signals[i].first_err;

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal NOT subscribed",
				(const char *)"SharedData::is_first()");

	return true;
}
//=============================================================================
/**
 * Set a signal first consecutive error event arrived
 */
//=============================================================================
void SharedData::set_first_err(string &signame)
{
	//not to be locked, called only inside lock in ArchiveCB::push_event
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			signals[i].first_err = false;
			return;
		}
	}

	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (compare_without_domain(signals[i].name,signame))
		{
			signals[i].first_err = false;
			return;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal NOT subscribed",
				(const char *)"SharedData::set_first()");

}
//=============================================================================
/**
 * Remove a signal in the list.
 */
//=============================================================================
void SharedData::unsubscribe_events()
{
	omni_mutex_lock sync(*this);
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		HdbSignal	*sig = &signals[i];
		if (signals[i].event_id != ERR)
		{
			cout << "    unsubscribe " << sig->name << endl;
			try
			{
				sig->attr->unsubscribe_event(sig->event_id);
				delete sig->archive_cb;
			}
			catch (Tango::DevFailed &e)
			{
				//	Do nothing
				//	Unregister failed means Register has also failed
			}
		}
		delete sig->attr;
	}
	cout << __func__<< ": exiting..."<<endl;
}
//=============================================================================
/**
 * Add a new signal.
 */
//=============================================================================
void SharedData::add(string &signame)
{
	add(signame, NOTHING);
}
//=============================================================================
/**
 * Add a new signal.
 */
//=============================================================================
void SharedData::add(string &signame, int to_do)
{
	cout << "Adding " << signame << endl;
	{
		omni_mutex_lock sync(*this);
		
		//	Check if already subscribed
		bool	found = false;
		for (unsigned int i=0 ; i<signals.size() && !found ; i++)
		{
			HdbSignal	*sig = &signals[i];
			found = (sig->name==signame);
		}
		for (unsigned int i=0 ; i<signals.size() && !found ; i++)
		{
			HdbSignal	*sig = &signals[i];
			found = compare_without_domain(sig->name,signame);
		}
		if (found)		
			Tango::Except::throw_exception(
						(const char *)"BadSignalName",
						"Signal already subscribed",
						(const char *)"SharedData::add()");

		//	Build Hdb Signal object
		HdbSignal	signal;
		signal.name      = signame;

		//	on name, split device name and attrib name
		string::size_type idx = signal.name.find_last_of("/");
		if (idx==string::npos)
			Tango::Except::throw_exception(
						(const char *)"SyntaxError",
						"Syntax error in signal name",
						(const char *)"SharedData::add()");
		signal.devname = signal.name.substr(0, idx);
		signal.attname = signal.name.substr(idx+1);

		//cout << "created proxy to " << signame << endl;
		//	create Attribute proxy
		signal.attr = new Tango::AttributeProxy(signal.name);
		signal.event_id = ERR;
		signal.evstate    = Tango::ALARM;
		signal.isZMQ    = false;
		signal.okev_counter = 0;
		signal.nokev_counter = 0;
		signal.running = false;
		signal.first = true;
		signal.first_err = true;

		try
		{
			Tango::AttributeInfo	info = signal.attr->get_config();
			signal.data_type = info.data_type;
			signal.data_format = info.data_format;
			signal.write_type = info.writable;
			signal.max_dim_x = info.max_dim_x;
			signal.max_dim_y = info.max_dim_y;
		}
		catch (Tango::DevFailed &e)
		{

		}

		cout << "created proxy to " << signame << endl;

		//	Add in vector
		signals.push_back(signal);

		action = to_do;
	}
	cout << __func__<<": exiting... " << signame << endl;
	signal();
	//condition.signal();
}
//=============================================================================
/**
 * Subscribe archive event for each signal
 */
//=============================================================================
void SharedData::subscribe_events()
{
	//omni_mutex_lock sync(*this);
	this->lock();
	vector<HdbSignal>	local_signals(signals);
	this->unlock();
	//now using unlocked local copy since subscribe_event call callback that needs to lock signals
	for (unsigned int i=0 ; i<local_signals.size() ; i++)
	{
		HdbSignal	*sig = &local_signals[i];
		if (sig->event_id==ERR)
		{
			sig->archive_cb = new ArchiveCB(hdb_dev);
			try
			{
				cout << "Subscribing for " << sig->name << endl;
				Tango::AttributeInfo	info = sig->attr->get_config();
				sig->data_type = info.data_type;
				sig->data_format = info.data_format;
				sig->write_type = info.writable;
				sig->max_dim_x = info.max_dim_x;
				sig->max_dim_y = info.max_dim_y;
				sig->event_id = sig->attr->subscribe_event(
												Tango::ARCHIVE_EVENT,
												sig->archive_cb,
												/*stateless=*/false);
				sig->evstate  = Tango::ON;
				sig->first  = true;
				sig->first_err  = true;
				sig->status.clear();
				sig->status = "Subscribed";
				cout << sig->name <<  "  Subscribed" << endl;
				
				//	Check event source  ZMQ/Notifd ?
				Tango::ZmqEventConsumer	*consumer = 
						Tango::ApiUtil::instance()->get_zmq_event_consumer();
				sig->isZMQ = (consumer->get_event_system_for_event_id(sig->event_id) == Tango::ZMQ);
				
				cout << sig->name << "(id="<< sig->event_id <<"):	" << ((sig->isZMQ)? "ZMQ Event" : "NOTIFD Event") << endl;
			}
			catch (Tango::DevFailed &e)
			{
				Tango::Except::print_exception(e);
				sig->status.clear();
				sig->status = e.errors[0].desc;
				sig->event_id = ERR;
				delete sig->archive_cb;
			}
		}
	}
	this->lock();
	for (unsigned int i=0 ; i<local_signals.size() ; i++)
	{
		for (unsigned int j=0 ; j<signals.size() ; j++)
		{
			//if this signal just subscribed:
			if (signals[j].name==local_signals[i].name && signals[j].event_id==ERR && local_signals[i].event_id!=ERR)
			{
				signals[j].archive_cb = local_signals[i].archive_cb;
				signals[j].data_type = local_signals[i].data_type;
				signals[j].data_format = local_signals[i].data_format;
				signals[j].write_type = local_signals[i].write_type;
				signals[j].max_dim_x = local_signals[i].max_dim_x;
				signals[j].max_dim_y = local_signals[i].max_dim_y;
				signals[j].event_id = local_signals[i].event_id;
				signals[j].evstate  = local_signals[i].evstate;
				signals[j].first  = local_signals[i].first;
				signals[j].first_err  = local_signals[i].first_err;
				signals[j].status.clear();
				signals[j].status = local_signals[i].status;
				signals[j].isZMQ = local_signals[i].isZMQ;
			}
		}
	}
	this->unlock();
	initialized = true;
}
//=============================================================================
//=============================================================================
bool SharedData::is_initialized()
{
	omni_mutex_lock sync(*this);
	return initialized; 
}
//=============================================================================
/**
 *	return number of signals to be subscribed
 */
//=============================================================================
int SharedData::nb_sig_to_subscribe()
{
	omni_mutex_lock sync(*this);

	int	nb = 0;
	for (unsigned int i=0 ; i<signals.size() ; i++)
		if (signals[i].event_id == ERR)
			nb++;
	return nb;
}
//=============================================================================
/**
 *	build a list of signal to set HDB device property
 */
//=============================================================================
void SharedData::put_signal_property()
{
	omni_mutex_lock sync(*this);

	if (action==UPDATE_PROP)
	{
		vector<string>	v;
		for (unsigned int i=0 ; i<signals.size() ; i++)
			v.push_back(signals[i].name);
//			v.push_back(signals[i].name + ",	" + signals[i].taco_type);

		hdb_dev->put_signal_property(v);
		action = NOTHING;
	}
}
//=============================================================================
/**
 *	Return the list of signals
 */
//=============================================================================
vector<string>  SharedData::get_sig_list()
{
	omni_mutex_lock sync(*this);
	vector<string>	list;
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		string	signame(signals[i].name);
		list.push_back(signame);
	}
	return list;
}
//=============================================================================
/**
 *	Return the list of sources
 */
//=============================================================================
vector<bool>  SharedData::get_sig_source_list()
{
	omni_mutex_lock sync(*this);
	vector<bool>	list;
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		list.push_back(signals[i].isZMQ);
	}
	return list;
}
//=============================================================================
/**
 *	Return the source of specified signal
 */
//=============================================================================
bool  SharedData::get_sig_source(string &signame)
{
	omni_mutex_lock sync(*this);
	for (unsigned int i=0 ; i<signals.size() ; i++)
		if (signals[i].name==signame)
			return signals[i].isZMQ;

	for (unsigned int i=0 ; i<signals.size() ; i++)
		if (compare_without_domain(signals[i].name,signame))
			return signals[i].isZMQ;

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal NOT subscribed",
				(const char *)"SharedData::get_sig_source()");

	return true;
}
//=============================================================================
/**
 *	Return the list of signals on error
 */
//=============================================================================
vector<string>  SharedData::get_sig_on_error_list()
{
	omni_mutex_lock sync(*this);
	vector<string>	list;
	for (unsigned int i=0 ; i<signals.size() ; i++)
		if (signals[i].evstate==Tango::ALARM)
		{
			string	signame(signals[i].name);
			list.push_back(signame);
		}
	return list;
}
//=============================================================================
/**
 *	Return the number of signals on error
 */
//=============================================================================
int  SharedData::get_sig_on_error_num()
{
	omni_mutex_lock sync(*this);
	int num=0;
	for (unsigned int i=0 ; i<signals.size() ; i++)
		if (signals[i].evstate==Tango::ALARM)
		{
			num++;
		}
	return num;
}
//=============================================================================
/**
 *	Return the list of signals not on error
 */
//=============================================================================
vector<string>  SharedData::get_sig_not_on_error_list()
{
	omni_mutex_lock sync(*this);
	vector<string>	list;
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].evstate==Tango::ON)
		{
			string	signame(signals[i].name);
			list.push_back(signame);
		}
	}
	return list;
}
//=============================================================================
/**
 *	Return the number of signals not on error
 */
//=============================================================================
int  SharedData::get_sig_not_on_error_num()
{
	omni_mutex_lock sync(*this);
	int num=0;
	for (unsigned int i=0 ; i<signals.size() ; i++)
		if (signals[i].evstate==Tango::ON)
		{
			num++;
		}
	return num;
}


//=============================================================================
/**
 *	Increment the ok counter of event rx
 */
//=============================================================================
void  SharedData::set_ok_event(string &signame)
{
	//not to be locked, called only inside lock in ArchiveCB::push_event
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			signals[i].okev_counter++;
			signals[i].first_err = true;
			return;
		}
	}
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (compare_without_domain(signals[i].name,signame))
		{
			signals[i].okev_counter++;
			signals[i].first_err = true;
			return;
		}
	}
	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal NOT subscribed",
				(const char *)"SharedData::set_ok_event()");
}
//=============================================================================
/**
 *	Get the ok counter of event rx
 */
//=============================================================================
uint32_t  SharedData::get_ok_event(string &signame)
{
	omni_mutex_lock sync(*this);
	for (unsigned int i=0 ; i<signals.size() ; i++)
		if (signals[i].name==signame)
			return signals[i].okev_counter;

	for (unsigned int i=0 ; i<signals.size() ; i++)
		if (compare_without_domain(signals[i].name,signame))
			return signals[i].okev_counter;

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal NOT subscribed",
				(const char *)"SharedData::get_ok_event()");

	return 0;
}
//=============================================================================
/**
 *	Increment the error counter of event rx
 */
//=============================================================================
void  SharedData::set_nok_event(string &signame)
{
	//not to be locked, called only inside lock in ArchiveCB::push_event
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			signals[i].nokev_counter++;
			return;
		}
	}
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (compare_without_domain(signals[i].name,signame))
		{
			signals[i].nokev_counter++;
			return;
		}
	}
	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal NOT subscribed",
				(const char *)"SharedData::set_nok_event()");
}
//=============================================================================
/**
 *	Get the error counter of event rx
 */
//=============================================================================
uint32_t  SharedData::get_nok_event(string &signame)
{
	omni_mutex_lock sync(*this);
	for (unsigned int i=0 ; i<signals.size() ; i++)
		if (signals[i].name==signame)
			return signals[i].nokev_counter;

	for (unsigned int i=0 ; i<signals.size() ; i++)
		if (compare_without_domain(signals[i].name,signame))
			return signals[i].nokev_counter;

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal NOT subscribed",
				(const char *)"SharedData::get_nok_event()");

	return 0;
}

//=============================================================================
/**
 *	Return the status of specified signal
 */
//=============================================================================
string  SharedData::get_sig_status(string &signame)
{
	omni_mutex_lock sync(*this);
	for (unsigned int i=0 ; i<signals.size() ; i++)
		if (signals[i].name==signame)
			return signals[i].status;

	for (unsigned int i=0 ; i<signals.size() ; i++)
		if (compare_without_domain(signals[i].name,signame))
			return signals[i].status;

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal NOT subscribed",
				(const char *)"SharedData::get_sig_status()");
	return "";
}
//=============================================================================
/**
 *	Reset statistic counters
 */
//=============================================================================
void SharedData::reset_statistics()
{
	omni_mutex_lock sync(*this);
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		signals[i].nokev_counter=0;
		signals[i].okev_counter=0;
	}

}
//=============================================================================
/**
 *	Return ALARM if at list one signal is not subscribed.
 */
//=============================================================================
Tango::DevState SharedData::state()
{
	omni_mutex_lock sync(*this);
	Tango::DevState	state = Tango::ON;
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].evstate==Tango::ALARM)
		{
			state = Tango::ALARM;
			break;
		}
	}
	return state;
}
//=============================================================================
//=============================================================================
bool SharedData::get_if_stop()
{
	omni_mutex_lock sync(*this);
	return stop_it;
}
//=============================================================================
//=============================================================================
void SharedData::stop_thread()
{
	omni_mutex_lock sync(*this);
	stop_it = true;
	signal();
	//condition.signal();
}
//=============================================================================
//=============================================================================

string SharedData::remove_domain(string str)
{
	string::size_type	end1 = str.find(".");
	if (end1 == string::npos)
	{
		return str;
	}
	else
	{
		string::size_type	start = str.find("tango://");
		if (start == string::npos)
		{
			start = 0;
		}
		else
		{
			start = 8;	//tango:// len
		}
		string::size_type	end2 = str.find(":", start);
		if(end1 > end2)	//'.' not in the tango host part
			return str;
		string th = str.substr(0, end1);
		th += str.substr(end2, str.size()-end2);
		return th;
	}
}
//=============================================================================
//=============================================================================
bool SharedData::compare_without_domain(string str1, string str2)
{
	string str1_nd = remove_domain(str1);
	string str2_nd = remove_domain(str2);
	return (str1_nd==str2_nd);
}
//=============================================================================
//=============================================================================







//=============================================================================
//=============================================================================
SubscribeThread::SubscribeThread(HdbDevice *dev)
{
	hdb_dev = dev;
	period  = dev->period;
	shared  = dev->shared;
}
//=============================================================================
//=============================================================================
void SubscribeThread::updateProperty()
{
	shared->put_signal_property();
}
//=============================================================================
//=============================================================================
void *SubscribeThread::run_undetached(void *ptr)
{
	while(shared->get_if_stop()==false)
	{
		//	Try to subscribe
		shared->subscribe_events();
		updateProperty();
		int	nb_to_subscribe = shared->nb_sig_to_subscribe();
		//	And wait a bit before next time or
		//	wait a long time if all signals subscribed
		{
			omni_mutex_lock sync(*shared);
			//shared->lock();
			if (nb_to_subscribe==0)
			{
				cout << __func__<<": going to wait nb_to_subscribe=0"<<period<<endl;
				//shared->condition.wait();
				shared->wait();
			}
			else
			{
				cout << __func__<<": going to wait period="<<period<<endl;
				//unsigned long s,n;
				//omni_thread::get_time(&s,&n,period,0);
				//shared->condition.timedwait(s,n);
				shared->wait(period*1000);
			}
			//shared->unlock();
		}
	}
	shared->unsubscribe_events();
	cout <<"SubscribeThread::"<< __func__<<": exiting..."<<endl;
	return NULL;
}
//=============================================================================
//=============================================================================




}	//	namespace
