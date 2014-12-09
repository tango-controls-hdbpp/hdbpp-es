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
#ifndef _MULTI_TANGO_HOST
		if (hdb_dev->compare_without_domain(sig->name,signame))
#else		
		if (!hdb_dev->compare_tango_names(sig->name,signame))
#endif
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
#ifdef _RWLOCK
		veclock.readerIn();
#else
		omni_mutex_lock sync(*this);
#endif
		HdbSignal	*sig = get_signal(signame);
		int event_id = sig->event_id;
		int event_conf_id = sig->event_conf_id;
		Tango::AttributeProxy *attr = sig->attr;
#ifdef _RWLOCK
		veclock.readerOut();
#endif
		try
		{
			if(event_id != ERR)
			{

				cout <<"SharedData::"<< __func__<<": unsubscribing... "<< signame << endl;
				attr->unsubscribe_event(event_id);
				attr->unsubscribe_event(event_conf_id);
				cout <<"SharedData::"<< __func__<<": unsubscribed... "<< signame << endl;
			}
		}
		catch (Tango::DevFailed &e)
		{
			//	Do nothing
			//	Unregister failed means Register has also failed
			cout <<"SharedData::"<< __func__<<": Exception unsubscribing " << signame << " err=" << e.errors[0].desc << endl;
		}

#ifdef _RWLOCK
		veclock.writerIn();
#endif
		vector<HdbSignal>::iterator	pos = signals.begin();
		
		bool	found = false;
		for (unsigned int i=0 ; i<signals.size() && !found ; i++, pos++)
		{
			HdbSignal	*sig = &signals[i];
			if (sig->name==signame)
			{
				found = true;
				cout <<"SharedData::"<<__func__<< ": removing " << signame << endl;
#ifdef _RWLOCK
				sig->siglock->writerIn();
#endif
				try
				{
					if(sig->event_id != ERR)
					{
						delete sig->archive_cb;
					}
					delete sig->attr;
				}
				catch (Tango::DevFailed &e)
				{
					//	Do nothing
					//	Unregister failed means Register has also failed
					cout <<"SharedData::"<< __func__<<": Exception unsubscribing " << signame << " err=" << e.errors[0].desc << endl;
				}
#ifdef _RWLOCK
				sig->siglock->writerOut();
				delete sig->siglock;
#endif
				cout <<"SharedData::"<< __func__<<": removed " << signame << endl;
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
#ifndef _MULTI_TANGO_HOST
				if (hdb_dev->compare_without_domain(sig->name,signame))
#else					
				if (!hdb_dev->compare_tango_names(sig->name,signame))
#endif
				{
					found = true;
					cout <<"SharedData::"<<__func__<< ": removing " << signame << endl;
#ifdef _RWLOCK
					sig->siglock->writerIn();
#endif
					try
					{
						if(sig->event_id != ERR)
						{
							delete sig->archive_cb;
						}
						delete sig->attr;
					}
					catch (Tango::DevFailed &e)
					{
						//	Do nothing
						//	Unregister failed means Register has also failed
						cout <<"SharedData::"<< __func__<<": Exception unsubscribing " << signame << " err=" << e.errors[0].desc << endl;
					}
#ifdef _RWLOCK
					sig->siglock->writerOut();
#endif
					cout <<"SharedData::"<< __func__<<": removed " << signame << endl;
					signals.erase(pos);
					break;
				}
			}
		}
#ifdef _RWLOCK
		veclock.writerOut();
#endif
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
#ifdef _RWLOCK
	ReaderLock lock(veclock);
#else
	omni_mutex_lock sync(*this);
#endif
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
#ifdef _RWLOCK
			signals[i].siglock->writerIn();
#endif
			signals[i].running=true;
#ifdef _RWLOCK
			signals[i].siglock->writerOut();
#endif
			return;
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
#ifdef _RWLOCK
			signals[i].siglock->writerIn();
#endif
			signals[i].running=true;
#ifdef _RWLOCK
			signals[i].siglock->writerOut();
#endif
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
#ifdef _RWLOCK
	ReaderLock lock(veclock);
#else
	omni_mutex_lock sync(*this);
#endif
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
#ifdef _RWLOCK
			signals[i].siglock->writerIn();
#endif
			signals[i].running=false;
#ifdef _RWLOCK
			signals[i].siglock->writerOut();
#endif
			return;
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
#ifdef _RWLOCK
			signals[i].siglock->writerIn();
#endif
			signals[i].running=false;
#ifdef _RWLOCK
			signals[i].siglock->writerOut();
#endif
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
#ifdef _RWLOCK
	ReaderLock lock(veclock);
#else
	omni_mutex_lock sync(*this);
#endif
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
#ifdef _RWLOCK
		signals[i].siglock->writerIn();
#endif
		signals[i].running=true;
#ifdef _RWLOCK
		signals[i].siglock->writerOut();
#endif
	}
}
//=============================================================================
/**
 * Stop saving on DB all signals.
 */
//=============================================================================
void SharedData::stop_all()
{
#ifdef _RWLOCK
	ReaderLock lock(veclock);
#else
	omni_mutex_lock sync(*this);
#endif
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
#ifdef _RWLOCK
		signals[i].siglock->writerIn();
#endif
		signals[i].running=false;
#ifdef _RWLOCK
		signals[i].siglock->writerOut();
#endif
	}
}
//=============================================================================
/**
 * Is a signal saved on DB?
 */
//=============================================================================
bool SharedData::is_running(string &signame)
{
	bool retval=true;
	//to be locked if called outside lock in ArchiveCB::push_event
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
#ifdef _RWLOCK
			signals[i].siglock->readerIn();
#endif
			retval = signals[i].running;
#ifdef _RWLOCK
			signals[i].siglock->readerOut();
#endif
			return retval;
		}
	}

	for (unsigned int i=0 ; i<signals.size(); i++)
	{
#ifndef _MULTI_TANGO_HOST
		if (hdb_dev->compare_without_domain(signals[i].name,signame))
#else	
		if (!hdb_dev->compare_tango_names(signals[i].name,signame))
#endif
		{
#ifdef _RWLOCK
			signals[i].siglock->readerIn();
#endif
			retval = signals[i].running;
#ifdef _RWLOCK
			signals[i].siglock->readerOut();
#endif
			return retval;
		}
	}

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
	bool retval;
	//not to be locked, called only inside lock in ArchiveCB::push_event
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
#ifdef _RWLOCK
			signals[i].siglock->readerIn();
#endif
			retval = signals[i].first;
#ifdef _RWLOCK
			signals[i].siglock->readerOut();
#endif
			return retval;
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
#ifdef _RWLOCK
			signals[i].siglock->readerIn();
#endif
			retval = signals[i].first;
#ifdef _RWLOCK
			signals[i].siglock->readerOut();
#endif
			return retval;
		}
	}

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
#ifdef _RWLOCK
			signals[i].siglock->writerIn();
#endif
			signals[i].first = false;
#ifdef _RWLOCK
			signals[i].siglock->writerOut();
#endif
			return;
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
#ifdef _RWLOCK
			signals[i].siglock->writerIn();
#endif
			signals[i].first = false;
#ifdef _RWLOCK
			signals[i].siglock->writerOut();
#endif
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
	bool retval;
	//not to be locked, called only inside lock in ArchiveCB::push_event
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
#ifdef _RWLOCK
			signals[i].siglock->readerIn();
#endif
			retval = signals[i].first_err;
#ifdef _RWLOCK
			signals[i].siglock->readerOut();
#endif
			return retval;
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
#ifdef _RWLOCK
			signals[i].siglock->readerIn();
#endif
			retval = signals[i].first_err;
#ifdef _RWLOCK
			signals[i].siglock->readerOut();
#endif
			return retval;
		}
	}

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
#ifdef _RWLOCK
			signals[i].siglock->writerIn();
#endif
			signals[i].first_err = false;
#ifdef _RWLOCK
			signals[i].siglock->writerOut();
#endif
			return;
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
#ifdef _RWLOCK
			signals[i].siglock->writerIn();
#endif
			signals[i].first_err = false;
#ifdef _RWLOCK
			signals[i].siglock->writerOut();
#endif
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
	cout <<"SharedData::"<<__func__<< "    entering..."<< endl;
#ifndef _RWLOCK
	this->lock();
	vector<HdbSignal>	local_signals(signals);
	this->unlock();
	for (unsigned int i=0 ; i<local_signals.size() ; i++)
	{
		HdbSignal	*sig = &local_signals[i];
		if (signals[i].event_id != ERR)
		{
			cout <<"SharedData::"<<__func__<< "    unsubscribe " << sig->name << " id="<<omni_thread::self()->id()<< endl;
			try
			{
				sig->attr->unsubscribe_event(sig->event_id);
				sig->attr->unsubscribe_event(sig->event_conf_id);
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
#else
	veclock.readerIn();
	vector<HdbSignal>	local_signals(signals);
	veclock.readerOut();
	for (unsigned int i=0 ; i<local_signals.size() ; i++)
	{
		HdbSignal	*sig = &local_signals[i];
		if (signals[i].event_id != ERR)
		{
			cout <<"SharedData::"<<__func__<< "    unsubscribe " << sig->name << " id="<<omni_thread::self()->id()<< endl;
			try
			{
				sig->attr->unsubscribe_event(sig->event_id);
				sig->attr->unsubscribe_event(sig->event_conf_id);
				cout <<"SharedData::"<<__func__<< "    unsubscribed " << sig->name << endl;
			}
			catch (Tango::DevFailed &e)
			{
				//	Do nothing
				//	Unregister failed means Register has also failed
			}
		}
	}
	veclock.writerIn();
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		HdbSignal	*sig = &signals[i];
		sig->siglock->writerIn();
		if (signals[i].event_id != ERR)
		{
			delete sig->archive_cb;
			cout <<"SharedData::"<<__func__<< "    deleted cb " << sig->name << endl;
		}
		delete sig->attr;
		cout <<"SharedData::"<<__func__<< "    deleted proxy " << sig->name << endl;
		sig->siglock->writerOut();
		delete sig->siglock;
		cout <<"SharedData::"<<__func__<< "    deleted lock " << sig->name << endl;
	}
	cout <<"SharedData::"<<__func__<< "    ended loop, deleting vector" << endl;
#endif
#ifdef _RWLOCK

#else
	this->lock();
#endif
	/*for (unsigned int j=0 ; j<signals.size() ; j++, pos++)
	{
		signals[j].event_id = ERR;
		signals[j].event_conf_id = ERR;
		signals[j].archive_cb = NULL;
		signals[j].attr = NULL;
	}*/
	signals.clear();
#ifdef _RWLOCK
	veclock.writerOut();
#else
	this->unlock();
#endif
	cout <<"SharedData::"<< __func__<< ": exiting..."<<endl;
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
#ifdef _RWLOCK
		veclock.readerIn();
#else
		omni_mutex_lock sync(*this);
#endif
		
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
#ifndef _MULTI_TANGO_HOST
			found = hdb_dev->compare_without_domain(sig->name,signame);
#else	
			found = !hdb_dev->compare_tango_names(sig->name,signame);
#endif
		}
#ifdef _RWLOCK
		veclock.readerOut();
#endif
		if (found)
			Tango::Except::throw_exception(
						(const char *)"BadSignalName",
						"Signal already subscribed",
						(const char *)"SharedData::add()");

		//	Build Hdb Signal object
		HdbSignal	signal;
		signal.name      = signame;
#ifdef _RWLOCK
		signal.siglock = new(ReadersWritersLock);
#endif
		signal.status = "Syntax error in signal name";
		//	on name, split device name and attrib name
		string::size_type idx = signal.name.find_last_of("/");
		if (idx==string::npos)
			Tango::Except::throw_exception(
						(const char *)"SyntaxError",
						"Syntax error in signal name",
						(const char *)"SharedData::add()");
		signal.devname = signal.name.substr(0, idx);
		signal.attname = signal.name.substr(idx+1);
		signal.status = "NOT connected";
		//cout << "created proxy to " << signame << endl;
		//	create Attribute proxy
		signal.attr = new Tango::AttributeProxy(signal.name);
		signal.event_id = ERR;
		signal.event_conf_id = ERR;
		signal.evstate    = Tango::ALARM;
		signal.isZMQ    = false;
		signal.okev_counter = 0;
		signal.okev_counter_freq = 0;
		signal.nokev_counter = 0;
		signal.nokev_counter_freq = 0;
		signal.running = false;
		signal.first = true;
		signal.first_err = true;
		signal.periodic_ev = -1;
		clock_gettime(CLOCK_MONOTONIC, &signal.last_ev);

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
			cout <<"SubscribeThread::"<<__func__<< " ERROR for " << signame << " in get_config err=" << e.errors[0].desc << endl;
		}

		cout <<"SubscribeThread::"<< __func__<< " created proxy to " << signame << endl;
#ifdef _RWLOCK
		veclock.writerIn();
#endif
		//	Add in vector
		signals.push_back(signal);
#ifdef _RWLOCK
		veclock.writerOut();
#endif

		action = to_do;
	}
	cout <<"SubscribeThread::"<< __func__<<": exiting... " << signame << endl;
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
	/*for (unsigned int ii=0 ; ii<signals.size() ; ii++)
	{
		HdbSignal	*sig2 = &signals[ii];
		int ret = pthread_rwlock_trywrlock(&sig2->siglock);
		cout << __func__<<": pthread_rwlock_trywrlock i="<<ii<<" name="<<sig2->name<<" just entered " << ret << endl;
		if(ret == 0) pthread_rwlock_unlock(&sig2->siglock);
	}*/
	//omni_mutex_lock sync(*this);
#ifndef _RWLOCK
	this->lock();
	vector<HdbSignal>	local_signals(signals);
	this->unlock();

	//now using unlocked local copy since subscribe_event call callback that needs to lock signals
	for (unsigned int i=0 ; i<local_signals.size() ; i++)
	{
		HdbSignal	*sig = &local_signals[i];
#else
	veclock.readerIn();
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		HdbSignal	*sig = &signals[i];
		sig->siglock->writerIn();
#endif
		if (sig->event_id==ERR)
		{
			sig->archive_cb = new ArchiveCB(hdb_dev);
			Tango::AttributeInfo	info;
			try
			{
				info = sig->attr->get_config();
			}
			catch (Tango::DevFailed &e)
			{
				Tango::Except::print_exception(e);
				sig->status.clear();
				sig->status = e.errors[0].desc;
				sig->event_id = ERR;
				delete sig->archive_cb;
#ifdef _RWLOCK
				sig->siglock->writerOut();
#endif
				continue;
			}
			sig->first  = true;
			sig->data_type = info.data_type;
			sig->data_format = info.data_format;
			sig->write_type = info.writable;
			sig->max_dim_x = info.max_dim_x;
			sig->max_dim_y = info.max_dim_y;
			sig->first_err  = true;
			cout << "Subscribing for " << sig->name << " data_type=" << sig->data_type << " " << (sig->first ? "FIRST" : "NOT FIRST") << endl;
#ifdef _RWLOCK
			sig->siglock->writerOut();
#endif
			int		event_id = ERR;
			int		event_conf_id = ERR;
			bool	isZMQ = true;
			bool	err = false;

			try
			{
				event_conf_id = sig->attr->subscribe_event(
												Tango::ATTR_CONF_EVENT,
												sig->archive_cb,
												/*stateless=*/false);
				event_id = sig->attr->subscribe_event(
												Tango::ARCHIVE_EVENT,
												sig->archive_cb,
												/*stateless=*/false);
				/*sig->evstate  = Tango::ON;
				//sig->first  = false;	//first event already arrived at subscribe_event
				sig->status.clear();
				sig->status = "Subscribed";
				cout << sig->name <<  "  Subscribed" << endl;*/
				
				//	Check event source  ZMQ/Notifd ?
				Tango::ZmqEventConsumer	*consumer = 
						Tango::ApiUtil::instance()->get_zmq_event_consumer();
				isZMQ = (consumer->get_event_system_for_event_id(event_id) == Tango::ZMQ);
				
				cout << sig->name << "(id="<< event_id <<"):	Subscribed " << ((isZMQ)? "ZMQ Event" : "NOTIFD Event") << endl;
			}
			catch (Tango::DevFailed &e)
			{
				err = true;
				Tango::Except::print_exception(e);
#ifdef _RWLOCK
				sig->siglock->writerIn();
#endif
				sig->status.clear();
				sig->status = e.errors[0].desc;
				sig->event_id = ERR;
				delete sig->archive_cb;
#ifdef _RWLOCK
				sig->siglock->writerOut();
#endif
			}
			if(!err)
			{
#ifdef _RWLOCK
				sig->siglock->writerIn();
#endif
				sig->event_conf_id = event_conf_id;
				sig->event_id = event_id;
				sig->isZMQ = isZMQ;
#ifdef _RWLOCK
				sig->siglock->writerOut();
#endif
			}
		}
		else
		{
#ifdef _RWLOCK
			sig->siglock->writerOut();
#endif
		}
	}
#ifdef _RWLOCK
	veclock.readerOut();
#endif
#ifndef _RWLOCK
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
				signals[j].event_conf_id = local_signals[i].event_conf_id;
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
#endif
	initialized = true;
}
//=============================================================================
//=============================================================================
bool SharedData::is_initialized()
{
	//omni_mutex_lock sync(*this);
	return initialized; 
}
//=============================================================================
/**
 *	return number of signals to be subscribed
 */
//=============================================================================
int SharedData::nb_sig_to_subscribe()
{
#ifdef _RWLOCK
	ReaderLock lock(veclock);
#else
	omni_mutex_lock sync(*this);
#endif

	int	nb = 0;
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
#ifdef _RWLOCK
		signals[i].siglock->readerIn();
#endif
		if (signals[i].event_id == ERR)
		{
			nb++;
		}
#ifdef _RWLOCK
		signals[i].siglock->readerOut();
#endif
	}
	return nb;
}
//=============================================================================
/**
 *	build a list of signal to set HDB device property
 */
//=============================================================================
void SharedData::put_signal_property()
{
#ifdef _RWLOCK
	ReaderLock lock(veclock);
#else
	omni_mutex_lock sync(*this);
#endif

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
#ifdef _RWLOCK
	ReaderLock lock(veclock);
#else
	omni_mutex_lock sync(*this);
#endif
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
#ifdef _RWLOCK
	ReaderLock lock(veclock);
#else
	omni_mutex_lock sync(*this);
#endif
	vector<bool>	list;
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
#ifdef _RWLOCK
		signals[i].siglock->readerIn();
#endif
		list.push_back(signals[i].isZMQ);
#ifdef _RWLOCK
		signals[i].siglock->readerOut();
#endif
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
	bool retval;
#ifdef _RWLOCK
	ReaderLock lock(veclock);
#else
	omni_mutex_lock sync(*this);
#endif
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
#ifdef _RWLOCK
			signals[i].siglock->readerIn();
#endif
			retval = signals[i].isZMQ;
#ifdef _RWLOCK
			signals[i].siglock->readerOut();
#endif
			return retval;
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
#ifdef _RWLOCK
			signals[i].siglock->readerIn();
#endif
			retval = signals[i].isZMQ;
#ifdef _RWLOCK
			signals[i].siglock->readerOut();
#endif
			return retval;
		}
	}

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
#ifdef _RWLOCK
	ReaderLock lock(veclock);
#else
	omni_mutex_lock sync(*this);
#endif
	vector<string>	list;
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
#ifdef _RWLOCK
		signals[i].siglock->readerIn();
#endif
		if (signals[i].evstate==Tango::ALARM && signals[i].running)
		{
			string	signame(signals[i].name);
			list.push_back(signame);
		}
#ifdef _RWLOCK
		signals[i].siglock->readerOut();
#endif
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
#ifdef _RWLOCK
	ReaderLock lock(veclock);
#else
	omni_mutex_lock sync(*this);
#endif
	int num=0;
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
#ifdef _RWLOCK
		signals[i].siglock->readerIn();
#endif
		if (signals[i].evstate==Tango::ALARM && signals[i].running)
		{
			num++;
		}
#ifdef _RWLOCK
		signals[i].siglock->readerOut();
#endif
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
#ifdef _RWLOCK
	ReaderLock lock(veclock);
#else
	omni_mutex_lock sync(*this);
#endif
	vector<string>	list;
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
#ifdef _RWLOCK
		signals[i].siglock->readerIn();
#endif
		if (signals[i].evstate==Tango::ON || (signals[i].evstate==Tango::ALARM && !signals[i].running))
		{
			string	signame(signals[i].name);
			list.push_back(signame);
		}
#ifdef _RWLOCK
		signals[i].siglock->readerOut();
#endif
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
#ifdef _RWLOCK
	ReaderLock lock(veclock);
#else
	omni_mutex_lock sync(*this);
#endif
	int num=0;
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
#ifdef _RWLOCK
		signals[i].siglock->readerIn();
#endif
		if (signals[i].evstate==Tango::ON || (signals[i].evstate==Tango::ALARM && !signals[i].running))
		{
			num++;
		}
#ifdef _RWLOCK
		signals[i].siglock->readerOut();
#endif
	}
	return num;
}
//=============================================================================
/**
 *	Return the list of signals started
 */
//=============================================================================
vector<string>  SharedData::get_sig_started_list()
{
#ifdef _RWLOCK
	ReaderLock lock(veclock);
#else
	omni_mutex_lock sync(*this);
#endif
	vector<string>	list;
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
#ifdef _RWLOCK
		signals[i].siglock->readerIn();
#endif
		if (signals[i].running)
		{
			string	signame(signals[i].name);
			list.push_back(signame);
		}
#ifdef _RWLOCK
		signals[i].siglock->readerOut();
#endif
	}
	return list;
}
//=============================================================================
/**
 *	Return the number of signals started
 */
//=============================================================================
int  SharedData::get_sig_started_num()
{
#ifdef _RWLOCK
	ReaderLock lock(veclock);
#else
	omni_mutex_lock sync(*this);
#endif
	int num=0;
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
#ifdef _RWLOCK
		signals[i].siglock->readerIn();
#endif
		if (signals[i].running)
		{
			num++;
		}
#ifdef _RWLOCK
		signals[i].siglock->readerOut();
#endif
	}
	return num;
}
//=============================================================================
/**
 *	Return the list of signals not started
 */
//=============================================================================
vector<string>  SharedData::get_sig_not_started_list()
{
#ifdef _RWLOCK
	ReaderLock lock(veclock);
#else
	omni_mutex_lock sync(*this);
#endif
	vector<string>	list;
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
#ifdef _RWLOCK
		signals[i].siglock->readerIn();
#endif
		if (!signals[i].running)
		{
			string	signame(signals[i].name);
			list.push_back(signame);
		}
#ifdef _RWLOCK
		signals[i].siglock->readerOut();
#endif
	}
	return list;
}
//=============================================================================
/**
 *	Return the list of errors
 */
//=============================================================================
vector<string>  SharedData::get_error_list()
{
#ifdef _RWLOCK
	ReaderLock lock(veclock);
#else
	omni_mutex_lock sync(*this);
#endif
	vector<string>	list;
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
#ifdef _RWLOCK
		signals[i].siglock->readerIn();
#endif
		//if (signals[i].status != STATUS_SUBSCRIBED)
		if (signals[i].evstate != Tango::ON)
		{
			list.push_back(signals[i].status);
		}
		else
		{
			list.push_back(string(""));
		}
#ifdef _RWLOCK
		signals[i].siglock->readerOut();
#endif
	}
	return list;
}
//=============================================================================
/**
 *	Return the list of errors
 */
//=============================================================================
vector<uint32_t>  SharedData::get_ev_counter_list()
{
#ifdef _RWLOCK
	ReaderLock lock(veclock);
#else
	omni_mutex_lock sync(*this);
#endif
	vector<uint32_t>	list;
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
#ifdef _RWLOCK
		signals[i].siglock->readerIn();
#endif
		list.push_back(signals[i].okev_counter + signals[i].nokev_counter);
#ifdef _RWLOCK
		signals[i].siglock->readerOut();
#endif
	}
	return list;
}
//=============================================================================
/**
 *	Return the number of signals not started
 */
//=============================================================================
int  SharedData::get_sig_not_started_num()
{
#ifdef _RWLOCK
	ReaderLock lock(veclock);
#else
	omni_mutex_lock sync(*this);
#endif
	int num=0;
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
#ifdef _RWLOCK
		signals[i].siglock->readerIn();
#endif
		if (!signals[i].running)
		{
			num++;
		}
#ifdef _RWLOCK
		signals[i].siglock->readerOut();
#endif
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
#ifdef _RWLOCK
			signals[i].siglock->writerIn();
#endif
			signals[i].evstate = Tango::ON;
			signals[i].status = "Event received";
			signals[i].okev_counter++;
			signals[i].okev_counter_freq++;
			signals[i].first_err = true;
			gettimeofday(&signals[i].last_okev, NULL);
			clock_gettime(CLOCK_MONOTONIC, &signals[i].last_ev);
#ifdef _RWLOCK
			signals[i].siglock->writerOut();
#endif
			return;
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
#ifdef _RWLOCK
			signals[i].siglock->writerIn();
#endif
			signals[i].evstate = Tango::ON;
			signals[i].status = "Event received";
			signals[i].okev_counter++;
			signals[i].okev_counter_freq++;
			signals[i].first_err = true;
			gettimeofday(&signals[i].last_okev, NULL);
			clock_gettime(CLOCK_MONOTONIC, &signals[i].last_ev);
#ifdef _RWLOCK
			signals[i].siglock->writerOut();
#endif
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
	uint32_t retval;
#ifdef _RWLOCK
	ReaderLock lock(veclock);
#else
	omni_mutex_lock sync(*this);
#endif
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
#ifdef _RWLOCK
			signals[i].siglock->readerIn();
#endif
			retval = signals[i].okev_counter;
#ifdef _RWLOCK
			signals[i].siglock->readerOut();
#endif
			return retval;
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
#ifdef _RWLOCK
			signals[i].siglock->readerIn();
#endif
			retval = signals[i].okev_counter;
#ifdef _RWLOCK
			signals[i].siglock->readerOut();
#endif
			return retval;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal NOT subscribed",
				(const char *)"SharedData::get_ok_event()");

	return 0;
}
//=============================================================================
/**
 *	Get the ok counter of event rx for freq stats
 */
//=============================================================================
uint32_t  SharedData::get_ok_event_freq(string &signame)
{
	uint32_t retval;
#ifdef _RWLOCK
	ReaderLock lock(veclock);
#else
	omni_mutex_lock sync(*this);
#endif
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
#ifdef _RWLOCK
			signals[i].siglock->readerIn();
#endif
			retval = signals[i].okev_counter_freq;
#ifdef _RWLOCK
			signals[i].siglock->readerOut();
#endif
			return retval;
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
#ifdef _RWLOCK
			signals[i].siglock->readerIn();
#endif
			retval = signals[i].okev_counter_freq;
#ifdef _RWLOCK
			signals[i].siglock->readerOut();
#endif
			return retval;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal NOT subscribed",
				(const char *)"SharedData::get_ok_event()");

	return 0;
}
//=============================================================================
/**
 *	Get last okev timestamp
 */
//=============================================================================
timeval  SharedData::get_last_okev(string &signame)
{
	timeval retval;
#ifdef _RWLOCK
	ReaderLock lock(veclock);
#else
	omni_mutex_lock sync(*this);
#endif
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
#ifdef _RWLOCK
			signals[i].siglock->readerIn();
#endif
			retval = signals[i].last_okev;
#ifdef _RWLOCK
			signals[i].siglock->readerOut();
#endif
			return retval;
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
#ifdef _RWLOCK
			signals[i].siglock->readerIn();
#endif
			retval = signals[i].last_okev;
#ifdef _RWLOCK
			signals[i].siglock->readerOut();
#endif
			return retval;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal NOT subscribed",
				(const char *)"SharedData::get_last_okev()");
	timeval ret;
	ret.tv_sec=0;
	ret.tv_usec=0;
	return ret;
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
#ifdef _RWLOCK
			signals[i].siglock->writerIn();
#endif
			signals[i].nokev_counter++;
			signals[i].nokev_counter_freq++;
			gettimeofday(&signals[i].last_nokev, NULL);
			clock_gettime(CLOCK_MONOTONIC, &signals[i].last_ev);
#ifdef _RWLOCK
			signals[i].siglock->writerOut();
#endif
			return;
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
#ifdef _RWLOCK
			signals[i].siglock->writerIn();
#endif
			signals[i].nokev_counter++;
			signals[i].nokev_counter_freq++;
			gettimeofday(&signals[i].last_nokev, NULL);
			clock_gettime(CLOCK_MONOTONIC, &signals[i].last_ev);
#ifdef _RWLOCK
			signals[i].siglock->writerOut();
#endif
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
	uint32_t retval;
#ifdef _RWLOCK
	ReaderLock lock(veclock);
#else
	omni_mutex_lock sync(*this);
#endif
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
#ifdef _RWLOCK
			signals[i].siglock->readerIn();
#endif
			retval = signals[i].nokev_counter;
#ifdef _RWLOCK
			signals[i].siglock->readerOut();
#endif
			return retval;
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
#ifdef _RWLOCK
			signals[i].siglock->readerIn();
#endif
			retval = signals[i].nokev_counter;
#ifdef _RWLOCK
			signals[i].siglock->readerOut();
#endif
			return retval;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal NOT subscribed",
				(const char *)"SharedData::get_nok_event()");

	return 0;
}
//=============================================================================
/**
 *	Get the error counter of event rx for freq stats
 */
//=============================================================================
uint32_t  SharedData::get_nok_event_freq(string &signame)
{
	uint32_t retval;
#ifdef _RWLOCK
	ReaderLock lock(veclock);
#else
	omni_mutex_lock sync(*this);
#endif
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
#ifdef _RWLOCK
			signals[i].siglock->readerIn();
#endif
			retval = signals[i].nokev_counter_freq;
#ifdef _RWLOCK
			signals[i].siglock->readerOut();
#endif
			return retval;
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
#ifdef _RWLOCK
			signals[i].siglock->readerIn();
#endif
			retval = signals[i].nokev_counter_freq;
#ifdef _RWLOCK
			signals[i].siglock->readerOut();
#endif
			return retval;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal NOT subscribed",
				(const char *)"SharedData::get_nok_event()");

	return 0;
}
//=============================================================================
/**
 *	Get last nokev timestamp
 */
//=============================================================================
timeval  SharedData::get_last_nokev(string &signame)
{
	timeval retval;
#ifdef _RWLOCK
	ReaderLock lock(veclock);
#else
	omni_mutex_lock sync(*this);
#endif
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
#ifdef _RWLOCK
			signals[i].siglock->readerIn();
#endif
			retval = signals[i].last_nokev;
#ifdef _RWLOCK
			signals[i].siglock->readerOut();
#endif
			return retval;
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
#ifdef _RWLOCK
			signals[i].siglock->readerIn();
#endif
			retval = signals[i].last_nokev;
#ifdef _RWLOCK
			signals[i].siglock->readerOut();
#endif
			return retval;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal NOT subscribed",
				(const char *)"SharedData::get_last_nokev()");
	timeval ret;
	ret.tv_sec=0;
	ret.tv_usec=0;
	return ret;
}
//=============================================================================
/**
 *	Set state and status of timeout on periodic event
 */
//=============================================================================
void  SharedData::set_nok_periodic_event(string &signame)
{
	//not to be locked, called only inside lock in ArchiveCB::push_event
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
#ifdef _RWLOCK
			signals[i].siglock->writerIn();
#endif
			signals[i].evstate = Tango::ALARM;
			signals[i].status = "Timeout on periodic event";
#ifdef _RWLOCK
			signals[i].siglock->writerOut();
#endif
			return;
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
#ifdef _RWLOCK
			signals[i].siglock->writerIn();
#endif
			signals[i].evstate = Tango::ALARM;
			signals[i].status = "Timeout on periodic event";
#ifdef _RWLOCK
			signals[i].siglock->writerOut();
#endif
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
 *	Return the status of specified signal
 */
//=============================================================================
string  SharedData::get_sig_status(string &signame)
{
	string retval;
#ifdef _RWLOCK
	ReaderLock lock(veclock);
#else
	omni_mutex_lock sync(*this);
#endif
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
#ifdef _RWLOCK
			signals[i].siglock->readerIn();
#endif
			retval = signals[i].status;
#ifdef _RWLOCK
			signals[i].siglock->readerOut();
#endif
			return retval;
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
#ifdef _RWLOCK
			signals[i].siglock->readerIn();
#endif
			retval = signals[i].status;
#ifdef _RWLOCK
			signals[i].siglock->readerOut();
#endif
			return retval;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal NOT subscribed",
				(const char *)"SharedData::get_sig_status()");
	return "";
}
//=============================================================================
/**
 *	Return the state of specified signal
 */
//=============================================================================
Tango::DevState  SharedData::get_sig_state(string &signame)
{
	Tango::DevState retval;
#ifdef _RWLOCK
	ReaderLock lock(veclock);
#else
	omni_mutex_lock sync(*this);
#endif
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
#ifdef _RWLOCK
			signals[i].siglock->readerIn();
#endif
			retval = signals[i].evstate;
#ifdef _RWLOCK
			signals[i].siglock->readerOut();
#endif
			return retval;
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
#ifdef _RWLOCK
			signals[i].siglock->readerIn();
#endif
			retval = signals[i].evstate;
#ifdef _RWLOCK
			signals[i].siglock->readerOut();
#endif
			return retval;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal NOT subscribed",
				(const char *)"SharedData::get_sig_state()");
	return Tango::ALARM;
}
//=============================================================================
/**
 *	Set Archive periodic event period
 */
//=============================================================================
void SharedData::set_conf_periodic_event(string &signame, string period)
{
	//not to be locked, called only inside lock in ArchiveCB::push_event
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
#ifdef _RWLOCK
			signals[i].siglock->writerIn();
#endif
			signals[i].periodic_ev = atoi(period.c_str());
#ifdef _RWLOCK
			signals[i].siglock->writerOut();
#endif
			return;
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
#ifdef _RWLOCK
			signals[i].siglock->writerIn();
#endif
			signals[i].periodic_ev = atoi(period.c_str());
#ifdef _RWLOCK
			signals[i].siglock->writerOut();
#endif
			return;
		}
	}
	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal NOT subscribed",
				(const char *)"SharedData::set_conf_periodic_event()");
}
//=============================================================================
/**
 *	Check Archive periodic event period
 */
//=============================================================================
int  SharedData::check_periodic_event_timeout(int delay_tolerance_ms)
{
#ifdef _RWLOCK
	ReaderLock lock(veclock);
#else
	omni_mutex_lock sync(*this);
#endif
	timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	double min_time_to_timeout_ms = 10000;
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
#ifdef _RWLOCK
		signals[i].siglock->readerIn();
#endif
		if(!signals[i].running)
		{
#ifdef _RWLOCK
			signals[i].siglock->readerOut();
#endif
			continue;
		}
		if(signals[i].evstate != Tango::ON)
		{
#ifdef _RWLOCK
			signals[i].siglock->readerOut();
#endif
			continue;
		}
		if(signals[i].periodic_ev <= 0)
		{
#ifdef _RWLOCK
			signals[i].siglock->readerOut();
#endif
			continue;
		}
		double diff_time_ms = (now.tv_sec - signals[i].last_ev.tv_sec) * 1000 + ((double)(now.tv_nsec - signals[i].last_ev.tv_nsec))/1000000;
		double time_to_timeout_ms = (double)(signals[i].periodic_ev + delay_tolerance_ms) - diff_time_ms;
#ifdef _RWLOCK
		signals[i].siglock->readerOut();
#endif
		if(time_to_timeout_ms <= 0)
		{
#ifdef _RWLOCK
			signals[i].siglock->writerIn();
#endif
			signals[i].evstate = Tango::ALARM;
			signals[i].status = "Timeout on periodic event";
#ifdef _RWLOCK
			signals[i].siglock->writerOut();
#endif
		}
		else if(time_to_timeout_ms < min_time_to_timeout_ms || min_time_to_timeout_ms == 0)
			min_time_to_timeout_ms = time_to_timeout_ms;
	}
	return min_time_to_timeout_ms;
}
//=============================================================================
/**
 *	Reset statistic counters
 */
//=============================================================================
void SharedData::reset_statistics()
{
#ifdef _RWLOCK
	ReaderLock lock(veclock);
#else
	omni_mutex_lock sync(*this);
#endif
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
#ifdef _RWLOCK
		signals[i].siglock->writerIn();
#endif
		signals[i].nokev_counter=0;
		signals[i].okev_counter=0;
#ifdef _RWLOCK
		signals[i].siglock->writerOut();
#endif
	}
}
//=============================================================================
/**
 *	Reset freq statistic counters
 */
//=============================================================================
void SharedData::reset_freq_statistics()
{
#ifdef _RWLOCK
	ReaderLock lock(veclock);
#else
	omni_mutex_lock sync(*this);
#endif
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
#ifdef _RWLOCK
		signals[i].siglock->writerIn();
#endif
		signals[i].nokev_counter_freq=0;
		signals[i].okev_counter_freq=0;
#ifdef _RWLOCK
		signals[i].siglock->writerOut();
#endif
	}
}
//=============================================================================
/**
 *	Return ALARM if at list one signal is not subscribed.
 */
//=============================================================================
Tango::DevState SharedData::state()
{
#ifdef _RWLOCK
	ReaderLock lock(veclock);
#else
	omni_mutex_lock sync(*this);
#endif
	Tango::DevState	state = Tango::ON;
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
#ifdef _RWLOCK
		signals[i].siglock->readerIn();
#endif
		if (signals[i].evstate==Tango::ALARM && signals[i].running)
		{
			state = Tango::ALARM;

		}
#ifdef _RWLOCK
		signals[i].siglock->readerOut();
#endif
		if(state == Tango::ALARM)
			break;
	}
	return state;
}
//=============================================================================
//=============================================================================
bool SharedData::get_if_stop()
{
	//omni_mutex_lock sync(*this);
	return stop_it;
}
//=============================================================================
//=============================================================================
void SharedData::stop_thread()
{
	//omni_mutex_lock sync(*this);
	stop_it = true;
	signal();
	//condition.signal();
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
	cout << "SubscribeThread id="<<omni_thread::self()->id()<<endl;
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
				//cout << __func__<<": going to wait nb_to_subscribe=0"<<endl;
				//shared->condition.wait();
				shared->wait();
			}
			else
			{
				//cout << __func__<<": going to wait period="<<period<<endl;
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
