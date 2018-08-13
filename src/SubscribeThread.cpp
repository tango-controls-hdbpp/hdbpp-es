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
SharedData::SharedData(HdbDevice *dev):Tango::LogAdapter(dev->_device)
{
	hdb_dev=dev;
	action=NOTHING;
	stop_it=false;
	initialized=false;
}
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
void SharedData::remove(string &signame, bool stop)
{
	//	Remove in signals list (vector)
	{
		if(!stop)
			veclock.readerIn();
		HdbSignal	*sig = get_signal(signame);
		int event_id = sig->event_id;
		int event_conf_id = sig->event_conf_id;
		Tango::AttributeProxy *attr = sig->attr;
		if(!stop)
			veclock.readerOut();
		if(stop)
		{
			try
			{
				if(event_id != ERR && attr)
				{
					DEBUG_STREAM <<"SharedData::"<< __func__<<": unsubscribing ARCHIVE_EVENT... "<< signame << endl;
					//unlocking, locked in SharedData::stop but possible deadlock if unsubscribing remote attribute with a faulty event connection
					sig->siglock->writerOut();
					attr->unsubscribe_event(event_id);
					sig->siglock->writerIn();
					DEBUG_STREAM <<"SharedData::"<< __func__<<": unsubscribed ARCHIVE_EVENT... "<< signame << endl;
				}
				if(event_conf_id != ERR && attr)
				{
					DEBUG_STREAM <<"SharedData::"<< __func__<<": unsubscribing ATTR_CONF_EVENT... "<< signame << endl;
					//unlocking, locked in SharedData::stop but possible deadlock if unsubscribing remote attribute with a faulty event connection
					sig->siglock->writerOut();
					attr->unsubscribe_event(event_conf_id);
					sig->siglock->writerIn();
					DEBUG_STREAM <<"SharedData::"<< __func__<<": unsubscribed ATTR_CONF_EVENT... "<< signame << endl;
				}
			}
			catch (Tango::DevFailed &e)
			{
				//	Do nothing
				//	Unregister failed means Register has also failed
				sig->siglock->writerIn();
				INFO_STREAM <<"SharedData::"<< __func__<<": Exception unsubscribing " << signame << " err=" << e.errors[0].desc << endl;
			}
		}

		if(!stop)
			veclock.writerIn();
		vector<HdbSignal>::iterator	pos = signals.begin();
		
		bool	found = false;
		for (unsigned int i=0 ; i<signals.size() && !found ; i++, pos++)
		{
			HdbSignal	*sig = &signals[i];
			if (sig->name==signame)
			{
				found = true;
				if(stop)
				{
					DEBUG_STREAM <<"SharedData::"<<__func__<< ": removing " << signame << endl;
					//sig->siglock->writerIn(); //: removed, already locked in SharedData::stop
					try
					{
						if(sig->event_id != ERR)
						{
							delete sig->archive_cb;
						}
						sig->event_id = ERR;
						if(sig->attr)
							delete sig->attr;
						sig->attr = NULL;
					}
					catch (Tango::DevFailed &e)
					{
						//	Do nothing
						//	Unregister failed means Register has also failed
						INFO_STREAM <<"SharedData::"<< __func__<<": Exception deleting " << signame << " err=" << e.errors[0].desc << endl;
					}
					//sig->siglock->writerOut();
					DEBUG_STREAM <<"SharedData::"<< __func__<<": stopped " << signame << endl;
				}
				if(!stop)
				{
					if(sig->running)
						hdb_dev->attr_AttributeStartedNumber_read--;
					if(sig->paused)
						hdb_dev->attr_AttributePausedNumber_read--;
					if(sig->stopped)
						hdb_dev->attr_AttributeStoppedNumber_read--;
					hdb_dev->attr_AttributeNumber_read--;
					delete sig->siglock;
					signals.erase(pos);
					DEBUG_STREAM <<"SharedData::"<< __func__<<": removed " << signame << endl;
				}
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
					DEBUG_STREAM <<"SharedData::"<<__func__<< ": removing " << signame << endl;
					if(stop)
					{
						sig->siglock->writerIn();
						try
						{
							if(sig->event_id != ERR)
							{
								delete sig->archive_cb;
							}
							sig->event_id = ERR;
							if(sig->attr)
								delete sig->attr;
							sig->attr = NULL;
						}
						catch (Tango::DevFailed &e)
						{
							//	Do nothing
							//	Unregister failed means Register has also failed
							INFO_STREAM <<"SharedData::"<< __func__<<": Exception unsubscribing " << signame << " err=" << e.errors[0].desc << endl;
						}
						sig->siglock->writerOut();
						DEBUG_STREAM <<"SharedData::"<< __func__<<": stopped " << signame << endl;
					}
					if(!stop)
					{
						if(sig->running)
							hdb_dev->attr_AttributeStartedNumber_read--;
						if(sig->paused)
							hdb_dev->attr_AttributePausedNumber_read--;
						if(sig->stopped)
							hdb_dev->attr_AttributeStoppedNumber_read--;
						hdb_dev->attr_AttributeNumber_read--;
						delete sig->siglock;
						signals.erase(pos);
						DEBUG_STREAM <<"SharedData::"<< __func__<<": removed " << signame << endl;
					}
					break;
				}
			}
		}
		if(!stop)
			veclock.writerOut();
		if (!found)
			Tango::Except::throw_exception(
						(const char *)"BadSignalName",
						"Signal " + signame + " NOT subscribed",
						(const char *)"SharedData::remove()");
	}
	//	then, update property
	if(!stop)
	{
		DEBUG_STREAM <<"SubscribeThread::"<< __func__<<": going to increase action... action="<<action<<"++" << endl;
		if(action <= UPDATE_PROP)
			action++;
		//put_signal_property();	//TODO: wakeup thread and let it do it? -> signal()
		signal();
	}
}
//=============================================================================
/**
 * Start saving on DB a signal.
 */
//=============================================================================
void SharedData::start(string &signame)
{
	ReaderLock lock(veclock);
	vector<string> contexts;	//TODO: not used in add(..., true)!!!
	Tango::DevULong ttl;	//TODO: not used in add(..., true)!!!
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			signals[i].siglock->writerIn();
			if(!signals[i].running)
			{
				if(signals[i].stopped)
				{
					hdb_dev->attr_AttributeStoppedNumber_read--;
					try
					{
						add(signame, contexts, ttl, NOTHING, true);
					}
					catch (Tango::DevFailed &e)
					{
						//Tango::Except::print_exception(e);
						INFO_STREAM << "SharedData::start: error adding  " << signame <<" err="<< e.errors[0].desc << endl;
						signals[i].status = e.errors[0].desc;
						/*signals[i].siglock->writerOut();
						return;*/
					}
				}
				signals[i].running=true;
				if(signals[i].paused)
					hdb_dev->attr_AttributePausedNumber_read--;
				hdb_dev->attr_AttributeStartedNumber_read++;
				signals[i].paused=false;
				signals[i].stopped=false;
			}
			signals[i].siglock->writerOut();
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
			signals[i].siglock->writerIn();
			if(!signals[i].running)
			{
				if(signals[i].stopped)
				{
					hdb_dev->attr_AttributeStoppedNumber_read--;
					try
					{
						add(signame, contexts, ttl, NOTHING, true);
					}
					catch (Tango::DevFailed &e)
					{
						//Tango::Except::print_exception(e);
						INFO_STREAM << "SharedData::start: error adding  " << signame << endl;
						signals[i].status = e.errors[0].desc;
						/*signals[i].siglock->writerOut();
						return;*/
					}
				}
				signals[i].running=true;
				if(signals[i].paused)
					hdb_dev->attr_AttributePausedNumber_read--;
				hdb_dev->attr_AttributeStartedNumber_read++;
				signals[i].paused=false;
				signals[i].stopped=false;
			}
			signals[i].siglock->writerOut();
			return;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal " + signame + " NOT subscribed",
				(const char *)"SharedData::start()");
}
//=============================================================================
/**
 * Pause saving on DB a signal.
 */
//=============================================================================
void SharedData::pause(string &signame)
{
	ReaderLock lock(veclock);
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			signals[i].siglock->writerIn();
			if(!signals[i].paused)
			{
				signals[i].paused=true;
				hdb_dev->attr_AttributePausedNumber_read++;
				if(signals[i].running)
					hdb_dev->attr_AttributeStartedNumber_read--;
				if(signals[i].stopped)
					hdb_dev->attr_AttributeStoppedNumber_read--;
				signals[i].running=false;
				signals[i].stopped=false;
			}
			signals[i].siglock->writerOut();
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
			signals[i].siglock->writerIn();
			if(!signals[i].paused)
			{
				signals[i].paused=true;
				hdb_dev->attr_AttributePausedNumber_read++;
				if(signals[i].running)
					hdb_dev->attr_AttributeStartedNumber_read--;
				if(signals[i].stopped)
					hdb_dev->attr_AttributeStoppedNumber_read--;
				signals[i].running=false;
				signals[i].stopped=false;
			}
			signals[i].siglock->writerOut();
			return;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal " + signame + " NOT subscribed",
				(const char *)"SharedData::pause()");
}
//=============================================================================
/**
 * Stop saving on DB a signal.
 */
//=============================================================================
void SharedData::stop(string &signame)
{
	ReaderLock lock(veclock);
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			signals[i].siglock->writerIn();
			if(!signals[i].stopped)
			{
				signals[i].stopped=true;
				hdb_dev->attr_AttributeStoppedNumber_read++;
				if(signals[i].running)
				{
					hdb_dev->attr_AttributeStartedNumber_read--;
					try
					{
						remove(signame, true);
					}
					catch (Tango::DevFailed &e)
					{
						//Tango::Except::print_exception(e);
						INFO_STREAM << "SharedData::stop: error removing  " << signame << endl;
					}
				}
				if(signals[i].paused)
				{
					hdb_dev->attr_AttributePausedNumber_read--;
					try
					{
						remove(signame, true);
					}
					catch (Tango::DevFailed &e)
					{
						//Tango::Except::print_exception(e);
						INFO_STREAM << "SharedData::stop: error removing  " << signame << endl;
					}
				}
				signals[i].running=false;
				signals[i].paused=false;
			}
			signals[i].siglock->writerOut();
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
			signals[i].siglock->writerIn();
			if(!signals[i].stopped)
			{
				signals[i].stopped=true;
				hdb_dev->attr_AttributeStoppedNumber_read++;
				if(signals[i].running)
				{
					hdb_dev->attr_AttributeStartedNumber_read--;
					try
					{
						remove(signame, true);
					}
					catch (Tango::DevFailed &e)
					{
						//Tango::Except::print_exception(e);
						INFO_STREAM << "SharedData::stop: error removing  " << signame << endl;
					}
				}
				if(signals[i].paused)
				{
					hdb_dev->attr_AttributePausedNumber_read--;
					try
					{
						remove(signame, true);
					}
					catch (Tango::DevFailed &e)
					{
						//Tango::Except::print_exception(e);
						INFO_STREAM << "SharedData::stop: error removing  " << signame << endl;
					}
				}
				signals[i].running=false;
				signals[i].paused=false;
			}
			signals[i].siglock->writerOut();
			return;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal " + signame + " NOT subscribed",
				(const char *)"SharedData::stop()");
}
//=============================================================================
/**
 * Start saving on DB all signals.
 */
//=============================================================================
void SharedData::start_all()
{
	ReaderLock lock(veclock);
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		signals[i].siglock->writerIn();
		signals[i].running=true;
		signals[i].paused=false;
		signals[i].stopped=false;
		signals[i].siglock->writerOut();
	}
	hdb_dev->attr_AttributeStoppedNumber_read=0;
	hdb_dev->attr_AttributePausedNumber_read=0;
	hdb_dev->attr_AttributeStartedNumber_read=signals.size();
}
//=============================================================================
/**
 * Pause saving on DB all signals.
 */
//=============================================================================
void SharedData::pause_all()
{
	ReaderLock lock(veclock);
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		signals[i].siglock->writerIn();
		signals[i].running=false;
		signals[i].paused=true;
		signals[i].stopped=false;
		signals[i].siglock->writerOut();
	}
	hdb_dev->attr_AttributeStoppedNumber_read=0;
	hdb_dev->attr_AttributePausedNumber_read=signals.size();
	hdb_dev->attr_AttributeStartedNumber_read=0;
}
//=============================================================================
/**
 * Stop saving on DB all signals.
 */
//=============================================================================
void SharedData::stop_all()
{
	ReaderLock lock(veclock);
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		signals[i].siglock->writerIn();
		signals[i].running=false;
		signals[i].paused=false;
		signals[i].stopped=true;
		signals[i].siglock->writerOut();
	}
	hdb_dev->attr_AttributeStoppedNumber_read=signals.size();
	hdb_dev->attr_AttributePausedNumber_read=0;
	hdb_dev->attr_AttributeStartedNumber_read=0;
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
			signals[i].siglock->readerIn();
			retval = signals[i].running;
			signals[i].siglock->readerOut();
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
			signals[i].siglock->readerIn();
			retval = signals[i].running;
			signals[i].siglock->readerOut();
			return retval;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal " + signame + " NOT subscribed",
				(const char *)"SharedData::is_running()");

	return true;
}
//=============================================================================
/**
 * Is a signal saved on DB?
 */
//=============================================================================
bool SharedData::is_paused(string &signame)
{
	bool retval=true;
	//to be locked if called outside lock in ArchiveCB::push_event
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			signals[i].siglock->readerIn();
			retval = signals[i].paused;
			signals[i].siglock->readerOut();
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
			signals[i].siglock->readerIn();
			retval = signals[i].paused;
			signals[i].siglock->readerOut();
			return retval;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal " + signame + " NOT subscribed",
				(const char *)"SharedData::is_paused()");

	return true;
}
//=============================================================================
/**
 * Is a signal not subscribed?
 */
//=============================================================================
bool SharedData::is_stopped(string &signame)
{
	bool retval=true;
	//to be locked if called outside lock in ArchiveCB::push_event
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			signals[i].siglock->readerIn();
			retval = signals[i].stopped;
			signals[i].siglock->readerOut();
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
			signals[i].siglock->readerIn();
			retval = signals[i].stopped;
			signals[i].siglock->readerOut();
			return retval;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal " + signame + " NOT subscribed",
				(const char *)"SharedData::is_stopped()");

	return true;
}
//=============================================================================
/**
 * Is a signal to be archived with current context?
 */
//=============================================================================
bool SharedData::is_current_context(string &signame, string context)
{
	bool retval=false;
	std::transform(context.begin(), context.end(), context.begin(), ::toupper);
	if(context == string(ALWAYS_CONTEXT))
	{
		retval = true;
		return retval;
	}
	//to be locked if called outside lock
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			signals[i].siglock->readerIn();
			vector<string>::iterator it = find(signals[i].contexts_upper.begin(), signals[i].contexts_upper.end(), context);
			if(it != signals[i].contexts_upper.end())
			{
				retval = true;
			}
			it = find(signals[i].contexts_upper.begin(), signals[i].contexts_upper.end(), ALWAYS_CONTEXT);
			if(it != signals[i].contexts_upper.end())
			{
				retval = true;
			}
			signals[i].siglock->readerOut();
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
			signals[i].siglock->readerIn();
			vector<string>::iterator it = find(signals[i].contexts_upper.begin(), signals[i].contexts_upper.end(), context);
			if(it != signals[i].contexts_upper.end())
			{
				retval = true;
			}
			it = find(signals[i].contexts_upper.begin(), signals[i].contexts_upper.end(), ALWAYS_CONTEXT);
			if(it != signals[i].contexts_upper.end())
			{
				retval = true;
			}
			signals[i].siglock->readerOut();
			return retval;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal " + signame + " NOT subscribed",
				(const char *)"SharedData::is_current_context()");

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
			signals[i].siglock->readerIn();
			retval = signals[i].first;
			signals[i].siglock->readerOut();
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
			signals[i].siglock->readerIn();
			retval = signals[i].first;
			signals[i].siglock->readerOut();
			return retval;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal " + signame + " NOT subscribed",
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
			signals[i].siglock->writerIn();
			signals[i].first = false;
			signals[i].siglock->writerOut();
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
			signals[i].siglock->writerIn();
			signals[i].first = false;
			signals[i].siglock->writerOut();
			return;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal " + signame + " NOT subscribed",
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
			signals[i].siglock->readerIn();
			retval = signals[i].first_err;
			signals[i].siglock->readerOut();
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
			signals[i].siglock->readerIn();
			retval = signals[i].first_err;
			signals[i].siglock->readerOut();
			return retval;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal " + signame + " NOT subscribed",
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
			signals[i].siglock->writerIn();
			signals[i].first_err = false;
			signals[i].siglock->writerOut();
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
			signals[i].siglock->writerIn();
			signals[i].first_err = false;
			signals[i].siglock->writerOut();
			return;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal " + signame + " NOT subscribed",
				(const char *)"SharedData::set_first()");

}
//=============================================================================
/**
 * Remove a signal in the list.
 */
//=============================================================================
void SharedData::unsubscribe_events()
{
	DEBUG_STREAM <<"SharedData::"<<__func__<< "    entering..."<< endl;
	veclock.readerIn();
	vector<HdbSignal>	local_signals(signals);
	veclock.readerOut();
	for (unsigned int i=0 ; i<local_signals.size() ; i++)
	{
		HdbSignal	*sig = &local_signals[i];
		if (local_signals[i].event_id != ERR && sig->attr)
		{
			DEBUG_STREAM <<"SharedData::"<<__func__<< "    unsubscribe " << sig->name << " id="<<omni_thread::self()->id()<< endl;
			try
			{
				sig->attr->unsubscribe_event(sig->event_id);
				sig->attr->unsubscribe_event(sig->event_conf_id);
				DEBUG_STREAM <<"SharedData::"<<__func__<< "    unsubscribed " << sig->name << endl;
			}
			catch (Tango::DevFailed &e)
			{
				//	Do nothing
				//	Unregister failed means Register has also failed
				INFO_STREAM <<"SharedData::"<<__func__<< "    ERROR unsubscribing " << sig->name << " err="<<e.errors[0].desc<< endl;
			}
		}
	}
	veclock.writerIn();
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		HdbSignal	*sig = &signals[i];
		sig->siglock->writerIn();
		if (signals[i].event_id != ERR && sig->attr)
		{
			delete sig->archive_cb;
			DEBUG_STREAM <<"SharedData::"<<__func__<< "    deleted cb " << sig->name << endl;
		}
		if(sig->attr)
		{
			delete sig->attr;
			DEBUG_STREAM <<"SharedData::"<<__func__<< "    deleted proxy " << sig->name << endl;
		}
		sig->siglock->writerOut();
		delete sig->siglock;
		DEBUG_STREAM <<"SharedData::"<<__func__<< "    deleted lock " << sig->name << endl;
	}
	DEBUG_STREAM <<"SharedData::"<<__func__<< "    ended loop, deleting vector" << endl;

	/*for (unsigned int j=0 ; j<signals.size() ; j++, pos++)
	{
		signals[j].event_id = ERR;
		signals[j].event_conf_id = ERR;
		signals[j].archive_cb = NULL;
		signals[j].attr = NULL;
	}*/
	signals.clear();
	veclock.writerOut();
	DEBUG_STREAM <<"SharedData::"<< __func__<< ": exiting..."<<endl;
}
//=============================================================================
/**
 * Add a new signal.
 */
//=============================================================================
void SharedData::add(string &signame, const vector<string> & contexts, Tango::DevULong ttl)
{
	add(signame, contexts, ttl, NOTHING, false);
}
//=============================================================================
/**
 * Add a new signal.
 */
//=============================================================================
void SharedData::add(string &signame, const vector<string> & contexts, Tango::DevULong ttl, int to_do, bool start)
{
	DEBUG_STREAM << "SharedData::"<<__func__<<": Adding " << signame << " to_do="<<to_do<<" start="<<(start ? "Y" : "N")<< endl;
	{
		veclock.readerIn();
		HdbSignal	*sig;
		//	Check if already subscribed
		bool	found = false;
		for (unsigned int i=0 ; i<signals.size() && !found ; i++)
		{
			sig = &signals[i];
			found = (sig->name==signame);
		}
		for (unsigned int i=0 ; i<signals.size() && !found ; i++)
		{
			sig = &signals[i];
#ifndef _MULTI_TANGO_HOST
			found = hdb_dev->compare_without_domain(sig->name,signame);
#else	
			found = !hdb_dev->compare_tango_names(sig->name,signame);
#endif
		}
		veclock.readerOut();
		//DEBUG_STREAM << "SharedData::"<<__func__<<": signame="<<signame<<" found="<<(found ? "Y" : "N") << " start="<<(start ? "Y" : "N")<< endl;
		if (found && !start)
			Tango::Except::throw_exception(
						(const char *)"BadSignalName",
						"Signal " + signame + " already subscribed",
						(const char *)"SharedData::add()");
		HdbSignal	*signal;
		if (!found && !start)
		{
			//	on name, split device name and attrib name
			string::size_type idx = signame.find_last_of("/");
			if (idx==string::npos)
			{
				Tango::Except::throw_exception(
							(const char *)"SyntaxError",
							"Syntax error in signal name " + signame,
							(const char *)"SharedData::add()");
			}
			signal = new HdbSignal();
			//	Build Hdb Signal object
			signal->name      = signame;
			signal->siglock = new(ReadersWritersLock);
			signal->devname = signal->name.substr(0, idx);
			signal->attname = signal->name.substr(idx+1);
			signal->status = "NOT connected";
			signal->attr = NULL;
			signal->running = false;
			signal->stopped = true;
			signal->paused = false;
			signal->contexts = contexts;
			signal->contexts_upper = contexts;
			signal->ttl = ttl;
			for(vector<string>::iterator it = signal->contexts_upper.begin(); it != signal->contexts_upper.end(); it++)
			    std::transform(it->begin(), it->end(), it->begin(), ::toupper);
			//DEBUG_STREAM << "SharedData::"<<__func__<<": signame="<<signame<<" created signal"<< endl;
		}
		else if(found && start)
		{
			signal = sig;
			signal->siglock->writerIn();
			signal->status = "NOT connected";
			signal->siglock->writerOut();
			//DEBUG_STREAM << "created proxy to " << signame << endl;
			//	create Attribute proxy
			signal->attr = new Tango::AttributeProxy(signal->name);	//TODO: OK out of siglock? accessed only inside the same thread?
			DEBUG_STREAM << "SharedData::"<<__func__<<": signame="<<signame<<" created proxy"<< endl;
		}
		signal->event_id = ERR;
		signal->event_conf_id = ERR;
		signal->evstate    = Tango::ALARM;
		signal->isZMQ    = false;
		signal->okev_counter = 0;
		signal->okev_counter_freq = 0;
		signal->nokev_counter = 0;
		signal->nokev_counter_freq = 0;
		signal->first = true;
		signal->first_err = true;
		signal->periodic_ev = -1;
		clock_gettime(CLOCK_MONOTONIC, &signal->last_ev);

		if(found && start)
		{
			try
			{
				Tango::AttributeInfo	info;
				if(signal->attr)
				{
					info = signal->attr->get_config();
					signal->data_type = info.data_type;
					signal->data_format = info.data_format;
					signal->write_type = info.writable;
					signal->max_dim_x = info.max_dim_x;
					signal->max_dim_y = info.max_dim_y;
				}
			}
			catch (Tango::DevFailed &e)
			{
				INFO_STREAM <<"SubscribeThread::"<<__func__<< " ERROR for " << signame << " in get_config err=" << e.errors[0].desc << endl;
			}
		}

		//DEBUG_STREAM <<"SubscribeThread::"<< __func__<< " created proxy to " << signame << endl;
		if (!found && !start)
		{
			veclock.writerIn();
			//	Add in vector
			signals.push_back(*signal);
			delete signal;
			hdb_dev->attr_AttributeNumber_read++;
			hdb_dev->attr_AttributeStoppedNumber_read++;
			veclock.writerOut();
			//DEBUG_STREAM << "SharedData::"<<__func__<<": signame="<<signame<<" push_back signal"<< endl;
		}
		else if(found && start)
		{

		}
		DEBUG_STREAM <<"SubscribeThread::"<< __func__<<": going to increase action... action="<<action<<" += " << to_do << endl;
		if(action <= UPDATE_PROP)
			action += to_do;
	}
	DEBUG_STREAM <<"SubscribeThread::"<< __func__<<": exiting... " << signame << endl;
	signal();
	//condition.signal();
}
//=============================================================================
/**
 * Update contexts for a signal.
 */
//=============================================================================
void SharedData::update(string &signame, const vector<string> & contexts)
{
	DEBUG_STREAM << "SharedData::"<<__func__<<": updating " << signame << " contexts.size=" << contexts.size()<< endl;

	veclock.readerIn();
	HdbSignal	*signal;
	//	Check if already subscribed
	bool	found = false;
	for (unsigned int i=0 ; i<signals.size() && !found ; i++)
	{
		signal = &signals[i];
		found = (signal->name==signame);
	}
	for (unsigned int i=0 ; i<signals.size() && !found ; i++)
	{
		signal = &signals[i];
#ifndef _MULTI_TANGO_HOST
		found = hdb_dev->compare_without_domain(signal->name,signame);
#else
		found = !hdb_dev->compare_tango_names(signal->name,signame);
#endif
	}
	veclock.readerOut();
	//DEBUG_STREAM << "SharedData::"<<__func__<<": signame="<<signame<<" found="<<(found ? "Y" : "N") << endl;
	if (!found)
		Tango::Except::throw_exception(
					(const char *)"BadSignalName",
					"Signal " + signame + " NOT found",
					(const char *)"SharedData::update()");

	if(found)
	{
		signal->siglock->writerIn();
		signal->contexts.clear();
		signal->contexts = contexts;
		signal->contexts_upper = contexts;
		for(vector<string>::iterator it = signal->contexts_upper.begin(); it != signal->contexts_upper.end(); it++)
		    std::transform(it->begin(), it->end(), it->begin(), ::toupper);
		signal->siglock->writerOut();
	}
	if(action <= UPDATE_PROP)
		action += UPDATE_PROP;
	DEBUG_STREAM <<"SubscribeThread::"<< __func__<<": exiting... " << signame << endl;
	this->signal();
}
//=============================================================================
/**
 * Update ttl for a signal.
 */
//=============================================================================
void SharedData::updatettl(string &signame, Tango::DevULong ttl)
{
	DEBUG_STREAM << "SharedData::"<<__func__<<": updating " << signame << " ttl=" << ttl<< endl;

	veclock.readerIn();
	HdbSignal	*signal;
	//	Check if already subscribed
	bool	found = false;
	for (unsigned int i=0 ; i<signals.size() && !found ; i++)
	{
		signal = &signals[i];
		found = (signal->name==signame);
	}
	for (unsigned int i=0 ; i<signals.size() && !found ; i++)
	{
		signal = &signals[i];
#ifndef _MULTI_TANGO_HOST
		found = hdb_dev->compare_without_domain(signal->name,signame);
#else
		found = !hdb_dev->compare_tango_names(signal->name,signame);
#endif
	}
	veclock.readerOut();
	//DEBUG_STREAM << "SharedData::"<<__func__<<": signame="<<signame<<" found="<<(found ? "Y" : "N") << endl;
	if (!found)
		Tango::Except::throw_exception(
					(const char *)"BadSignalName",
					"Signal " + signame + " NOT found",
					(const char *)"SharedData::update()");

	if(found)
	{
		signal->siglock->writerIn();
		signal->ttl=ttl;
		signal->siglock->writerOut();
	}
	if(action <= UPDATE_PROP)
		action += UPDATE_PROP;
	DEBUG_STREAM <<"SubscribeThread::"<< __func__<<": exiting... " << signame << endl;
	this->signal();
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
		DEBUG_STREAM << __func__<<": pthread_rwlock_trywrlock i="<<ii<<" name="<<sig2->name<<" just entered " << ret << endl;
		if(ret == 0) pthread_rwlock_unlock(&sig2->siglock);
	}*/
	//omni_mutex_lock sync(*this);
	veclock.readerIn();
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		HdbSignal	*sig = &signals[i];
		sig->siglock->writerIn();
		if (sig->event_id==ERR && !sig->stopped)
		{
			if(!sig->attr)
			{
				try
				{
					vector<string> contexts;	//TODO: not used in add(..., true)!!!
					Tango::DevULong ttl;	//TODO: not used in add(..., true)!!!
					add(sig->name, contexts, ttl, NOTHING, true);
				}
				catch (Tango::DevFailed &e)
				{
					//Tango::Except::print_exception(e);
					INFO_STREAM << "SharedData::subscribe_events: error adding  " << sig->name <<" err="<< e.errors[0].desc << endl;
					signals[i].status = e.errors[0].desc;
					signals[i].siglock->writerOut();
					continue;
				}
			}
			sig->archive_cb = new ArchiveCB(hdb_dev);
			Tango::AttributeInfo	info;
			try
			{
				sig->siglock->writerOut();
				sig->siglock->readerIn();
				info = sig->attr->get_config();
				sig->siglock->readerOut();
				sig->siglock->writerIn();
			}
			catch (Tango::DevFailed &e)
			{
				Tango::Except::print_exception(e);
				//sig->siglock->writerOut();
				sig->siglock->readerOut();
				sig->siglock->writerIn();
				sig->event_id = ERR;
				delete sig->archive_cb;
				sig->status = e.errors[0].desc;
				sig->siglock->writerOut();
				continue;
			}
			sig->first  = true;
			sig->data_type = info.data_type;
			sig->data_format = info.data_format;
			sig->write_type = info.writable;
			sig->max_dim_x = info.max_dim_x;
			sig->max_dim_y = info.max_dim_y;
			sig->first_err  = true;
			DEBUG_STREAM << "Subscribing for " << sig->name << " data_type=" << sig->data_type << " " << (sig->first ? "FIRST" : "NOT FIRST") << endl;
			sig->siglock->writerOut();
			int		event_id = ERR;
			int		event_conf_id = ERR;
			bool	isZMQ = true;
			bool	err = false;

			try
			{
				try
				{
				    event_id = sig->attr->subscribe_event(
				                                    Tango::ARCHIVE_EVENT,
				                                    sig->archive_cb,
				                                    /*stateless=*/false);
				}
				catch (Tango::DevFailed &e)
				{
				    INFO_STREAM <<__func__<< " sig->attr->subscribe_event EXCEPTION, try CHANGE_EVENT" << endl;
				    Tango::Except::print_exception(e);
				    event_id = sig->attr->subscribe_event(
				                                    Tango::CHANGE_EVENT,
				                                    sig->archive_cb,
				                                    /*stateless=*/false);
				    INFO_STREAM <<__func__<< " sig->attr->subscribe_event CHANGE_EVENT SUBSCRIBED" << endl;
				}
				event_conf_id = sig->attr->subscribe_event(
                                                Tango::ATTR_CONF_EVENT,
                                                sig->archive_cb,
                                                /*stateless=*/false);                
				/*sig->evstate  = Tango::ON;
				//sig->first  = false;	//first event already arrived at subscribe_event
				sig->status.clear();
				sig->status = "Subscribed";
				DEBUG_STREAM << sig->name <<  "  Subscribed" << endl;*/
				
				//	Check event source  ZMQ/Notifd ?
				Tango::ZmqEventConsumer	*consumer = 
						Tango::ApiUtil::instance()->get_zmq_event_consumer();
				isZMQ = (consumer->get_event_system_for_event_id(event_id) == Tango::ZMQ);
				
				DEBUG_STREAM << sig->name << "(id="<< event_id <<"):	Subscribed " << ((isZMQ)? "ZMQ Event" : "NOTIFD Event") << endl;
			}
			catch (Tango::DevFailed &e)
			{
				INFO_STREAM <<__func__<< " sig->attr->subscribe_event EXCEPTION:" << endl;
				err = true;
				Tango::Except::print_exception(e);
				sig->siglock->writerIn();
				sig->status = e.errors[0].desc;
				sig->event_id = ERR;
				delete sig->archive_cb;
				sig->siglock->writerOut();
			}
			if(!err)
			{
				sig->siglock->writerIn();
				sig->event_conf_id = event_conf_id;
				sig->event_id = event_id;
				sig->isZMQ = isZMQ;
				sig->siglock->writerOut();
			}
		}
		else
		{
			sig->siglock->writerOut();
		}
	}
	veclock.readerOut();
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
	ReaderLock lock(veclock);

	int	nb = 0;
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		signals[i].siglock->readerIn();
		if (signals[i].event_id == ERR && !signals[i].stopped)
		{
			nb++;
		}
		signals[i].siglock->readerOut();
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
	DEBUG_STREAM << "SharedData::"<<__func__<<": put_signal_property entering action=" << action << endl;
	//ReaderLock lock(veclock);
	if (action>NOTHING)
	{
		vector<string>	v;
		veclock.readerIn();
		for (unsigned int i=0 ; i<signals.size() ; i++)
		{
			string context;
			for(vector<string>::iterator it = signals[i].contexts.begin(); it != signals[i].contexts.end(); it++)
			{
				try
				{
					context += *it;
					if(it != signals[i].contexts.end() -1)
						context += "|";
				}
				catch(std::out_of_range &e)
				{

				}
			}
			stringstream conf_string;
			conf_string << signals[i].name << ";" << CONTEXT_KEY << "=" << context << ";" << TTL_KEY << "=" << signals[i].ttl;
			DEBUG_STREAM << "SharedData::"<<__func__<<": "<<i<<": " << conf_string.str() << endl;
			v.push_back(conf_string.str());
		}
		veclock.readerOut();
		hdb_dev->put_signal_property(v);
		if(action >= UPDATE_PROP)
			action--;
	}
	DEBUG_STREAM << "SharedData::"<<__func__<<": put_signal_property exiting action=" << action << endl;
}
//=============================================================================
/**
 *	Return the list of signals
 */
//=============================================================================
void SharedData::get_sig_list(vector<string> &list)
{
	ReaderLock lock(veclock);
	list.clear();
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		string	signame(signals[i].name);
		list.push_back(signame);
	}
	return;
}
//=============================================================================
/**
 *	Return the list of sources
 */
//=============================================================================
vector<bool>  SharedData::get_sig_source_list()
{
	ReaderLock lock(veclock);
	vector<bool>	list;
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		signals[i].siglock->readerIn();
		list.push_back(signals[i].isZMQ);
		signals[i].siglock->readerOut();
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
	ReaderLock lock(veclock);
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			signals[i].siglock->readerIn();
			retval = signals[i].isZMQ;
			signals[i].siglock->readerOut();
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
			signals[i].siglock->readerIn();
			retval = signals[i].isZMQ;
			signals[i].siglock->readerOut();
			return retval;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal " + signame + " NOT subscribed",
				(const char *)"SharedData::get_sig_source()");

	return true;
}
//=============================================================================
/**
 *	Return the list of signals on error
 */
//=============================================================================
void SharedData::get_sig_on_error_list(vector<string> &list)
{
	ReaderLock lock(veclock);
	list.clear();
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		signals[i].siglock->readerIn();
		if (signals[i].evstate==Tango::ALARM && signals[i].running)
		{
			string	signame(signals[i].name);
			list.push_back(signame);
		}
		signals[i].siglock->readerOut();
	}
	return;
}
//=============================================================================
/**
 *	Return the number of signals on error
 */
//=============================================================================
int  SharedData::get_sig_on_error_num()
{
	ReaderLock lock(veclock);
	int num=0;
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		signals[i].siglock->readerIn();
		if (signals[i].evstate==Tango::ALARM && signals[i].running)
		{
			num++;
		}
		signals[i].siglock->readerOut();
	}
	return num;
}
//=============================================================================
/**
 *	Return the list of signals not on error
 */
//=============================================================================
void SharedData::get_sig_not_on_error_list(vector<string> &list)
{
	ReaderLock lock(veclock);
	list.clear();
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		signals[i].siglock->readerIn();
		if (signals[i].evstate==Tango::ON || (signals[i].evstate==Tango::ALARM && !signals[i].running))
		{
			string	signame(signals[i].name);
			list.push_back(signame);
		}
		signals[i].siglock->readerOut();
	}
	return;
}
//=============================================================================
/**
 *	Return the number of signals not on error
 */
//=============================================================================
int  SharedData::get_sig_not_on_error_num()
{
	ReaderLock lock(veclock);
	int num=0;
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		signals[i].siglock->readerIn();
		if (signals[i].evstate==Tango::ON || (signals[i].evstate==Tango::ALARM && !signals[i].running))
		{
			num++;
		}
		signals[i].siglock->readerOut();
	}
	return num;
}
//=============================================================================
/**
 *	Return the list of signals started
 */
//=============================================================================
void SharedData::get_sig_started_list(vector<string> & list)
{
	ReaderLock lock(veclock);
	list.clear();
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		signals[i].siglock->readerIn();
		if (signals[i].running)
		{
			string	signame(signals[i].name);
			list.push_back(signame);
		}
		signals[i].siglock->readerOut();
	}
	return;
}
//=============================================================================
/**
 *	Return the number of signals started
 */
//=============================================================================
int  SharedData::get_sig_started_num()
{
	ReaderLock lock(veclock);
	int num=0;
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		signals[i].siglock->readerIn();
		if (signals[i].running)
		{
			num++;
		}
		signals[i].siglock->readerOut();
	}
	return num;
}
//=============================================================================
/**
 *	Return the list of signals not started
 */
//=============================================================================
void SharedData::get_sig_not_started_list(vector<string> &list)
{
	ReaderLock lock(veclock);
	list.clear();
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		signals[i].siglock->readerIn();
		if (!signals[i].running)
		{
			string	signame(signals[i].name);
			list.push_back(signame);
		}
		signals[i].siglock->readerOut();
	}
	return;
}
//=============================================================================
/**
 *	Return the list of errors
 */
//=============================================================================
bool SharedData::get_error_list(vector<string> &list)
{
	bool changed=false;
	ReaderLock lock(veclock);
	size_t old_size = list.size();
	size_t i;
	for (i=0 ; i<signals.size() && i < old_size ; i++)
	{
		signals[i].siglock->readerIn();
		string err;
		//if (signals[i].status != STATUS_SUBSCRIBED)
		if ((signals[i].evstate != Tango::ON) && (signals[i].running))
		{
			err = signals[i].status;
		}
		else
		{
			err = string("");
		}
		if(err != list[i])
		{
			list[i] = err;
			changed = true;
		}
		signals[i].siglock->readerOut();
	}
	if(signals.size() < old_size)
	{
		list.erase(list.begin()+i, list.begin()+old_size);
		changed = true;
	}
	else
	{
		for (size_t i=old_size ; i<signals.size() ; i++)
		{
			signals[i].siglock->readerIn();
			string err;
			//if (signals[i].status != STATUS_SUBSCRIBED)
			if ((signals[i].evstate != Tango::ON) && (signals[i].running))
			{
				err = signals[i].status;
			}
			else
			{
				err = string("");
			}
			list.push_back(err);
			signals[i].siglock->readerOut();
			changed = true;
		}
	}
	return changed;
}
//=============================================================================
/**
 *	Return the list of errors
 */
//=============================================================================
void SharedData::get_ev_counter_list(vector<uint32_t> &list)
{
	ReaderLock lock(veclock);
	list.clear();
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		signals[i].siglock->readerIn();
		list.push_back(signals[i].okev_counter + signals[i].nokev_counter);
		signals[i].siglock->readerOut();
	}
	return;
}
//=============================================================================
/**
 *	Return the number of signals not started
 */
//=============================================================================
int  SharedData::get_sig_not_started_num()
{
	ReaderLock lock(veclock);
	int num=0;
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		signals[i].siglock->readerIn();
		if (!signals[i].running)
		{
			num++;
		}
		signals[i].siglock->readerOut();
	}
	return num;
}
//=============================================================================
/**
 *	Return the complete, started and stopped lists of signals
 */
//=============================================================================
bool  SharedData::get_lists(vector<string> &s_list, vector<string> &s_start_list, vector<string> &s_pause_list, vector<string> &s_stop_list, vector<string> &s_context_list, Tango::DevULong *ttl_list)
{
	bool changed = false;
	ReaderLock lock(veclock);
	size_t old_s_list_size = s_list.size();
	size_t old_s_start_list_size = s_start_list.size();
	size_t old_s_pause_list_size = s_pause_list.size();
	size_t old_s_stop_list_size = s_stop_list.size();
	vector<string> tmp_start_list;
	vector<string> tmp_pause_list;
	vector<string> tmp_stop_list;
	//update list and context
	size_t i;
	for (i=0 ; i<signals.size() && i< old_s_list_size; i++)
	{
		string	signame(signals[i].name);
		if(signame != s_list[i])
		{
			s_list[i] = signame;
			changed = true;
		}
		signals[i].siglock->readerIn();
		string context;
		for(vector<string>::iterator it = signals[i].contexts.begin(); it != signals[i].contexts.end(); it++)
		{
			try
			{
				context += *it;
				if(it != signals[i].contexts.end() -1)
					context += "|";
			}
			catch(std::out_of_range &e)
			{

			}
		}
		if(ttl_list[i] != signals[i].ttl)
		{
			ttl_list[i] = signals[i].ttl;
			changed = true;
		}
		signals[i].siglock->readerOut();
		if(context != s_context_list[i])
		{
			s_context_list[i] = context;
			changed = true;
		}
	}
	if(signals.size() < old_s_list_size)
	{
		s_list.erase(s_list.begin()+i, s_list.begin()+old_s_list_size);
		s_context_list.erase(s_context_list.begin()+i, s_context_list.begin()+old_s_list_size);
		changed = true;
	}
	else
	{
		for (i=old_s_list_size ; i<signals.size() ; i++)
		{
			changed = true;
			string	signame(signals[i].name);
			s_list.push_back(signame);
			signals[i].siglock->readerIn();
			string context;
			for(vector<string>::iterator it = signals[i].contexts.begin(); it != signals[i].contexts.end(); it++)
			{
				try
				{
					context += *it;
					if(it != signals[i].contexts.end() -1)
						context += "|";
				}
				catch(std::out_of_range &e)
				{

				}
			}
			signals[i].siglock->readerOut();
			s_context_list.push_back(context);
		}
	}

	for (i=0 ; i<signals.size(); i++)
	{
		string	signame(signals[i].name);
		signals[i].siglock->readerIn();
		if (signals[i].running)
		{
			tmp_start_list.push_back(signame);
		}
		else if(signals[i].paused)
		{
			tmp_pause_list.push_back(signame);
		}
		else if(signals[i].stopped)
		{
			tmp_stop_list.push_back(signame);
		}
		signals[i].siglock->readerOut();
	}
	//update start list
	for (i=0 ; i<tmp_start_list.size() && i< old_s_start_list_size; i++)
	{
		string	signame(tmp_start_list[i]);
		if(signame != s_start_list[i])
		{
			s_start_list[i] = signame;
			changed = true;
		}
	}
	if(tmp_start_list.size() < old_s_start_list_size)
	{
		s_start_list.erase(s_start_list.begin()+i, s_start_list.begin()+old_s_start_list_size);
		changed = true;
	}
	else
	{
		for (size_t i=old_s_start_list_size ; i<tmp_start_list.size() ; i++)
		{
			changed = true;
			string	signame(tmp_start_list[i]);
			s_start_list.push_back(signame);
		}
	}
	//update pause list
	for (i=0 ; i<tmp_pause_list.size() && i< old_s_pause_list_size; i++)
	{
		string	signame(tmp_pause_list[i]);
		if(signame != s_pause_list[i])
		{
			s_pause_list[i] = signame;
			changed = true;
		}
	}
	if(tmp_pause_list.size() < old_s_pause_list_size)
	{
		s_pause_list.erase(s_pause_list.begin()+i, s_pause_list.begin()+old_s_pause_list_size);
		changed = true;
	}
	else
	{
		for (size_t i=old_s_pause_list_size ; i<tmp_pause_list.size() ; i++)
		{
			changed = true;
			string	signame(tmp_pause_list[i]);
			s_pause_list.push_back(signame);
		}
	}
	//update stop list
	for (i=0 ; i<tmp_stop_list.size() && i< old_s_stop_list_size; i++)
	{
		string	signame(tmp_stop_list[i]);
		if(signame != s_stop_list[i])
		{
			s_stop_list[i] = signame;
			changed = true;
		}
	}
	if(tmp_stop_list.size() < old_s_stop_list_size)
	{
		s_stop_list.erase(s_stop_list.begin()+i, s_stop_list.begin()+old_s_stop_list_size);
		changed = true;
	}
	else
	{
		for (size_t i=old_s_stop_list_size ; i<tmp_stop_list.size() ; i++)
		{
			changed = true;
			string	signame(tmp_stop_list[i]);
			s_stop_list.push_back(signame);
		}
	}
	return changed;
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
			signals[i].siglock->writerIn();
			signals[i].evstate = Tango::ON;
			signals[i].status = "Event received";
			signals[i].okev_counter++;
			signals[i].okev_counter_freq++;
			signals[i].first_err = true;
			gettimeofday(&signals[i].last_okev, NULL);
			clock_gettime(CLOCK_MONOTONIC, &signals[i].last_ev);
			signals[i].siglock->writerOut();
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
			signals[i].siglock->writerIn();
			signals[i].evstate = Tango::ON;
			signals[i].status = "Event received";
			signals[i].okev_counter++;
			signals[i].okev_counter_freq++;
			signals[i].first_err = true;
			gettimeofday(&signals[i].last_okev, NULL);
			clock_gettime(CLOCK_MONOTONIC, &signals[i].last_ev);
			signals[i].siglock->writerOut();
			return;
		}
	}
	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal " + signame + " NOT subscribed",
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
	ReaderLock lock(veclock);
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			signals[i].siglock->readerIn();
			retval = signals[i].okev_counter;
			signals[i].siglock->readerOut();
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
			signals[i].siglock->readerIn();
			retval = signals[i].okev_counter;
			signals[i].siglock->readerOut();
			return retval;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal " + signame + " NOT subscribed",
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
	ReaderLock lock(veclock);
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			signals[i].siglock->readerIn();
			retval = signals[i].okev_counter_freq;
			signals[i].siglock->readerOut();
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
			signals[i].siglock->readerIn();
			retval = signals[i].okev_counter_freq;
			signals[i].siglock->readerOut();
			return retval;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal " + signame + " NOT subscribed",
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
	ReaderLock lock(veclock);
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			signals[i].siglock->readerIn();
			retval = signals[i].last_okev;
			signals[i].siglock->readerOut();
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
			signals[i].siglock->readerIn();
			retval = signals[i].last_okev;
			signals[i].siglock->readerOut();
			return retval;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal " + signame + " NOT subscribed",
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
			signals[i].siglock->writerIn();
			signals[i].nokev_counter++;
			signals[i].nokev_counter_freq++;
			gettimeofday(&signals[i].last_nokev, NULL);
			clock_gettime(CLOCK_MONOTONIC, &signals[i].last_ev);
			signals[i].siglock->writerOut();
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
			signals[i].siglock->writerIn();
			signals[i].nokev_counter++;
			signals[i].nokev_counter_freq++;
			gettimeofday(&signals[i].last_nokev, NULL);
			clock_gettime(CLOCK_MONOTONIC, &signals[i].last_ev);
			signals[i].siglock->writerOut();
			return;
		}
	}
	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal " + signame + " NOT subscribed",
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
	ReaderLock lock(veclock);
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			signals[i].siglock->readerIn();
			retval = signals[i].nokev_counter;
			signals[i].siglock->readerOut();
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
			signals[i].siglock->readerIn();
			retval = signals[i].nokev_counter;
			signals[i].siglock->readerOut();
			return retval;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal " + signame + " NOT subscribed",
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
	ReaderLock lock(veclock);
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			signals[i].siglock->readerIn();
			retval = signals[i].nokev_counter_freq;
			signals[i].siglock->readerOut();
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
			signals[i].siglock->readerIn();
			retval = signals[i].nokev_counter_freq;
			signals[i].siglock->readerOut();
			return retval;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal " + signame + " NOT subscribed",
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
	ReaderLock lock(veclock);
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			signals[i].siglock->readerIn();
			retval = signals[i].last_nokev;
			signals[i].siglock->readerOut();
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
			signals[i].siglock->readerIn();
			retval = signals[i].last_nokev;
			signals[i].siglock->readerOut();
			return retval;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal " + signame + " NOT subscribed",
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
			signals[i].siglock->writerIn();
			signals[i].evstate = Tango::ALARM;
			signals[i].status = "Timeout on periodic event";
			signals[i].siglock->writerOut();
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
			signals[i].siglock->writerIn();
			signals[i].evstate = Tango::ALARM;
			signals[i].status = "Timeout on periodic event";
			signals[i].siglock->writerOut();
			return;
		}
	}
	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal " + signame + " NOT subscribed",
				(const char *)"SharedData::set_nok_periodic_event()");
}
//=============================================================================
/**
 *	Return the status of specified signal
 */
//=============================================================================
string  SharedData::get_sig_status(string &signame)
{
	string retval;
	ReaderLock lock(veclock);
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			signals[i].siglock->readerIn();
			retval = signals[i].status;
			signals[i].siglock->readerOut();
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
			signals[i].siglock->readerIn();
			retval = signals[i].status;
			signals[i].siglock->readerOut();
			return retval;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal " + signame + " NOT subscribed",
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
	ReaderLock lock(veclock);
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			signals[i].siglock->readerIn();
			retval = signals[i].evstate;
			signals[i].siglock->readerOut();
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
			signals[i].siglock->readerIn();
			retval = signals[i].evstate;
			signals[i].siglock->readerOut();
			return retval;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal " + signame + " NOT subscribed",
				(const char *)"SharedData::get_sig_state()");
	return Tango::ALARM;
}
//=============================================================================
/**
 *	Return the context of specified signal
 */
//=============================================================================
string  SharedData::get_sig_context(string &signame)
{
	string retval;
	ReaderLock lock(veclock);
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			signals[i].siglock->readerIn();
			for(vector<string>::iterator it = signals[i].contexts.begin(); it != signals[i].contexts.end(); it++)
			{
				try
				{
					retval += *it;
					if(it != signals[i].contexts.end() -1)
						retval += "|";
				}
				catch(std::out_of_range &e)
				{

				}
			}
			signals[i].siglock->readerOut();
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
			signals[i].siglock->readerIn();
			for(vector<string>::iterator it = signals[i].contexts.begin(); it != signals[i].contexts.end(); it++)
			{
				try
				{
					retval += *it;
					if(it != signals[i].contexts.end() -1)
						retval += "|";
				}
				catch(std::out_of_range &e)
				{

				}
			}
			signals[i].siglock->readerOut();
			return retval;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal " + signame + " NOT subscribed",
				(const char *)"SharedData::get_sig_context()");
	return "";
}
//=============================================================================
/**
 *	Return the ttl of specified signal
 */
//=============================================================================
Tango::DevULong  SharedData::get_sig_ttl(string &signame)
{
	Tango::DevULong retval;
	ReaderLock lock(veclock);
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		if (signals[i].name==signame)
		{
			signals[i].siglock->readerIn();
			retval = signals[i].ttl;
			signals[i].siglock->readerOut();
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
			signals[i].siglock->readerIn();
			retval = signals[i].ttl;
			signals[i].siglock->readerOut();
			return retval;
		}
	}

	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal " + signame + " NOT subscribed",
				(const char *)"SharedData::get_sig_ttl()");
	return DEFAULT_TTL;
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
			signals[i].siglock->writerIn();
			signals[i].periodic_ev = atoi(period.c_str());
			signals[i].siglock->writerOut();
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
			signals[i].siglock->writerIn();
			signals[i].periodic_ev = atoi(period.c_str());
			signals[i].siglock->writerOut();
			return;
		}
	}
	//	if not found
	Tango::Except::throw_exception(
				(const char *)"BadSignalName",
				"Signal " + signame + " NOT subscribed",
				(const char *)"SharedData::set_conf_periodic_event()");
}
//=============================================================================
/**
 *	Check Archive periodic event period
 */
//=============================================================================
int  SharedData::check_periodic_event_timeout(int delay_tolerance_ms)
{
	ReaderLock lock(veclock);
	timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	double min_time_to_timeout_ms = 10000;
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		signals[i].siglock->readerIn();
		if(!signals[i].running)
		{
			signals[i].siglock->readerOut();
			continue;
		}
		if(signals[i].evstate != Tango::ON)
		{
			signals[i].siglock->readerOut();
			continue;
		}
		if(signals[i].periodic_ev <= 0)
		{
			signals[i].siglock->readerOut();
			continue;
		}
		double diff_time_ms = (now.tv_sec - signals[i].last_ev.tv_sec) * 1000 + ((double)(now.tv_nsec - signals[i].last_ev.tv_nsec))/1000000;
		double time_to_timeout_ms = (double)(signals[i].periodic_ev + delay_tolerance_ms) - diff_time_ms;
		signals[i].siglock->readerOut();
		if(time_to_timeout_ms <= 0)
		{
			signals[i].siglock->writerIn();
			signals[i].evstate = Tango::ALARM;
			signals[i].status = "Timeout on periodic event";
			signals[i].siglock->writerOut();
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
	ReaderLock lock(veclock);
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		signals[i].siglock->writerIn();
		signals[i].nokev_counter=0;
		signals[i].okev_counter=0;
		signals[i].siglock->writerOut();
	}
}
//=============================================================================
/**
 *	Reset freq statistic counters
 */
//=============================================================================
void SharedData::reset_freq_statistics()
{
	ReaderLock lock(veclock);
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		signals[i].siglock->writerIn();
		signals[i].nokev_counter_freq=0;
		signals[i].okev_counter_freq=0;
		signals[i].siglock->writerOut();
	}
}
//=============================================================================
/**
 *	Return ALARM if at list one signal is not subscribed.
 */
//=============================================================================
Tango::DevState SharedData::state()
{
	ReaderLock lock(veclock);
	Tango::DevState	state = Tango::ON;
	for (unsigned int i=0 ; i<signals.size() ; i++)
	{
		signals[i].siglock->readerIn();
		if (signals[i].evstate==Tango::ALARM && signals[i].running)
		{
			state = Tango::ALARM;

		}
		signals[i].siglock->readerOut();
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
SubscribeThread::SubscribeThread(HdbDevice *dev):Tango::LogAdapter(dev->_device)
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
	INFO_STREAM << "SubscribeThread id="<<omni_thread::self()->id()<<endl;
	while(shared->get_if_stop()==false)
	{
		//	Try to subscribe
		DEBUG_STREAM << __func__<<": AWAKE"<<endl;
		updateProperty();
		shared->subscribe_events();
		int	nb_to_subscribe = shared->nb_sig_to_subscribe();
		//	And wait a bit before next time or
		//	wait a long time if all signals subscribed
		{
			omni_mutex_lock sync(*shared);
			//shared->lock();
			if (nb_to_subscribe==0 && shared->action == NOTHING)
			{
				DEBUG_STREAM << __func__<<": going to wait nb_to_subscribe=0"<<endl;
				//shared->condition.wait();
				shared->wait();
				//shared->wait(3*period*1000);
			}
			else if(shared->action == NOTHING)
			{
				DEBUG_STREAM << __func__<<": going to wait period="<<period<<"  nb_to_subscribe="<<nb_to_subscribe<<endl;
				//unsigned long s,n;
				//omni_thread::get_time(&s,&n,period,0);
				//shared->condition.timedwait(s,n);
				shared->wait(period*1000);
			}
			//shared->unlock();
		}
	}
	shared->unsubscribe_events();
	INFO_STREAM <<"SubscribeThread::"<< __func__<<": exiting..."<<endl;
	return NULL;
}
//=============================================================================
//=============================================================================



}	//	namespace
