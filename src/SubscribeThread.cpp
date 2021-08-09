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


#include "SubscribeThread.h"
#include <HdbDevice.h>
#include <HdbEventSubscriber.h>
#include "Consts.h"

namespace HdbEventSubscriber_ns
{
    SharedData::SharedData(HdbDevice *dev): Tango::LogAdapter(dev->_device)
                                            , initialized(false)
                                            , init_condition(&init_mutex)
    {
        hdb_dev=dev;
        action=NOTHING;
        stop_it=false;
    }

    SharedData::~SharedData()
    {
        init_condition.broadcast();
    }

    //=============================================================================
    /**
     *	get signal by name.
     */
    //=============================================================================
    auto SharedData::get_signal(const string &signame) -> std::shared_ptr<HdbSignal>
    {
        //omni_mutex_lock sync(*this);
        for (auto &signal : signals)
        {
            if(is_same_signal_name(signal->name, signame))
                return signal;
        }

        Tango::Except::throw_exception(
                (const char *)"BadSignalName",
                "Signal " + signame + " NOT FOUND in signal list",
                (const char *)"SharedData::get_signal()");
    }

    //=============================================================================
    /**
     *	Check if two strings represents the same signal.
     */
    //=============================================================================
    auto SharedData::is_same_signal_name(const std::string& name1, const std::string& name2) -> bool
    {
        // Easy case
        if(name1 == name2)
            return true;

#ifndef _MULTI_TANGO_HOST
        if(HdbDevice::compare_without_domain(name1, name2))
#else		
        if(!hdb_dev->compare_tango_names(name1, name2))
#endif
            return true;
        
        return false;
    }
    
    //=============================================================================
    /**
     * Remove a signal in the list.
     */
    //=============================================================================
    void SharedData::remove(const string& signame, bool stop)
    {
        //	Remove in signals list (vector)
        {
            if(!stop)
                veclock.readerIn();
            auto sig = get_signal(signame);
            int event_id = sig->event_id;
            int event_conf_id = sig->event_conf_id;
            std::shared_ptr<Tango::AttributeProxy> attr = sig->attr;
            if(!stop)
                veclock.readerOut();
            if(stop)
            {
                try
                {
                    if(event_id != ERR && attr != nullptr)
                    {
                        DEBUG_STREAM <<"SharedData::"<< __func__<<": unsubscribing ARCHIVE_EVENT... "<< signame << endl;
                        //unlocking, locked in SharedData::stop but possible deadlock if unsubscribing remote attribute with a faulty event connection
                        sig->siglock->writerOut();
                        attr->unsubscribe_event(event_id);
                        sig->siglock->writerIn();
                        DEBUG_STREAM <<"SharedData::"<< __func__<<": unsubscribed ARCHIVE_EVENT... "<< signame << endl;
                    }
                    if(event_conf_id != ERR && attr != nullptr)
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
            auto pos = signals.begin();

            bool found = false;
            for(unsigned int i=0 ; i<signals.size() && !found ; i++, pos++)
            {
                std::shared_ptr<HdbSignal> sig = signals[i];
                if(is_same_signal_name(sig->name, signame))
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
                                sig->archive_cb.reset();
                            }
                            sig->event_id = ERR;
                            sig->attr.reset();
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
                        signals.erase(pos);
                        DEBUG_STREAM <<"SharedData::"<< __func__<<": removed " << signame << endl;
                    }
                    break;
                }
            }
            pos = signals.begin();
            
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
    void SharedData::start(const string& signame)
    {
        ReaderLock lock(veclock);
        vector<string> contexts;	//TODO: not used in add(..., true)!!!
        for (auto &signal : signals)
        {
            if(is_same_signal_name(signal->name, signame))
            {
                signal->siglock->writerIn();
                if(!signal->running)
                {
                    if(signal->stopped)
                    {
                        hdb_dev->attr_AttributeStoppedNumber_read--;
                        try
                        {
                            add(signame, contexts, NOTHING, true);
                        }
                        catch (Tango::DevFailed &e)
                        {
                            //Tango::Except::print_exception(e);
                            INFO_STREAM << "SharedData::start: error adding  " << signame <<" err="<< e.errors[0].desc << endl;
                            signal->status = e.errors[0].desc;
                            /*signal.siglock->writerOut();
                              return;*/
                        }
                    }
                    signal->running=true;
                    if(signal->paused)
                        hdb_dev->attr_AttributePausedNumber_read--;
                    hdb_dev->attr_AttributeStartedNumber_read++;
                    signal->paused=false;
                    signal->stopped=false;
                }
                signal->siglock->writerOut();
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
    void SharedData::pause(const string& signame)
    {
        ReaderLock lock(veclock);
        for (auto &signal : signals)
        {
            if(is_same_signal_name(signal->name, signame))
            {
                signal->siglock->writerIn();
                if(!signal->paused)
                {
                    signal->paused=true;
                    hdb_dev->attr_AttributePausedNumber_read++;
                    if(signal->running)
                        hdb_dev->attr_AttributeStartedNumber_read--;
                    if(signal->stopped)
                        hdb_dev->attr_AttributeStoppedNumber_read--;
                    signal->running=false;
                    signal->stopped=false;
                }
                signal->siglock->writerOut();
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
    void SharedData::stop(const string& signame)
    {
        ReaderLock lock(veclock);
        for (auto &signal : signals)
        {
            if(is_same_signal_name(signal->name, signame))
            {
                signal->siglock->writerIn();
                if(!signal->stopped)
                {
                    signal->stopped=true;
                    hdb_dev->attr_AttributeStoppedNumber_read++;
                    if(signal->running)
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
                    if(signal->paused)
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
                    signal->running=false;
                    signal->paused=false;
                }
                signal->siglock->writerOut();
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
        for (auto &signal : signals)
        {
            signal->siglock->writerIn();
            signal->running=true;
            signal->paused=false;
            signal->stopped=false;
            signal->siglock->writerOut();
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
        for (auto &signal : signals)
        {
            signal->siglock->writerIn();
            signal->running=false;
            signal->paused=true;
            signal->stopped=false;
            signal->siglock->writerOut();
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
        for (auto &signal : signals)
        {
            signal->siglock->writerIn();
            signal->running=false;
            signal->paused=false;
            signal->stopped=true;
            signal->siglock->writerOut();
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
    auto SharedData::is_running(const string& signame) -> bool
    {
        bool retval = true;
        //to be locked if called outside lock in ArchiveCB::push_event
        
        std::shared_ptr<HdbSignal> signal = get_signal(signame);

        signal->siglock->readerIn();
        retval = signal->running;
        signal->siglock->readerOut();
        return retval;
    }
    //=============================================================================
    /**
     * Is a signal saved on DB?
     */
    //=============================================================================
    auto SharedData::is_paused(const string& signame) -> bool
    {
        bool retval = true;
        //to be locked if called outside lock in ArchiveCB::push_event
        
        std::shared_ptr<HdbSignal> signal = get_signal(signame);

        signal->siglock->readerIn();
        retval = signal->paused;
        signal->siglock->readerOut();
        return retval;
    }
    //=============================================================================
    /**
     * Is a signal not subscribed?
     */
    //=============================================================================
    auto SharedData::is_stopped(const string& signame) -> bool
    {
        bool retval=true;
        //to be locked if called outside lock in ArchiveCB::push_event
        
        std::shared_ptr<HdbSignal> signal = get_signal(signame);

        signal->siglock->readerIn();
        retval = signal->stopped;
        signal->siglock->readerOut();
        return retval;
    }
    //=============================================================================
    /**
     * Is a signal to be archived with current context?
     */
    //=============================================================================
    auto SharedData::is_current_context(const string& signame, string context) -> bool
    {
        bool retval=false;
        std::transform(context.begin(), context.end(), context.begin(), ::toupper);
        if(context == string(ALWAYS_CONTEXT))
        {
            retval = true;
            return retval;
        }
        //to be locked if called outside lock
        
        std::shared_ptr<HdbSignal> signal = get_signal(signame);

        signal->siglock->readerIn();
        auto it = find(signal->contexts_upper.begin(), signal->contexts_upper.end(), context);
        if(it != signal->contexts_upper.end())
        {
            retval = true;
        }
        it = find(signal->contexts_upper.begin(), signal->contexts_upper.end(), ALWAYS_CONTEXT);
        if(it != signal->contexts_upper.end())
        {
            retval = true;
        }
        signal->siglock->readerOut();
        return retval;
    }
    //=============================================================================
    /**
     * Is a signal first event arrived?
     */
    //=============================================================================
    auto SharedData::is_first(const string& signame) -> bool
    {
        bool retval = false;

        std::shared_ptr<HdbSignal> signal = get_signal(signame);
        
        signal->siglock->readerIn();
        retval = signal->first;
        signal->siglock->readerOut();
        return retval;
    }
    //=============================================================================
    /**
     * Set a signal first event arrived
     */
    //=============================================================================
    void SharedData::set_first(const string& signame)
    {
        std::shared_ptr<HdbSignal> signal = get_signal(signame);
        
        signal->siglock->writerIn();
        signal->first = false;
        signal->siglock->writerOut();
    }
    //=============================================================================
    /**
     * Is a signal first consecutive error event arrived?
     */
    //=============================================================================
    auto SharedData::is_first_err(const string& signame) -> bool
    {
        bool retval = false;

        std::shared_ptr<HdbSignal> signal = get_signal(signame);

        signal->siglock->readerIn();
        retval = signal->first_err;
        signal->siglock->readerOut();
        return retval;
    }
    //=============================================================================
    /**
     * Set a signal first consecutive error event arrived
     */
    //=============================================================================
    void SharedData::set_first_err(const string& signame)
    {
        std::shared_ptr<HdbSignal> signal = get_signal(signame);

        signal->siglock->writerIn();
        signal->first_err = false;
        signal->siglock->writerOut();
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
        vector<std::shared_ptr<HdbSignal>>	local_signals(signals);
        veclock.readerOut();
        for (const auto &signal : local_signals)
        {
            if (signal->event_id != ERR && signal->attr != nullptr)
            {
                DEBUG_STREAM <<"SharedData::"<<__func__<< "    unsubscribe " << signal->name << " id="<<omni_thread::self()->id()<< endl;
                try
                {
                    signal->attr->unsubscribe_event(signal->event_id);
                    signal->attr->unsubscribe_event(signal->event_conf_id);
                    DEBUG_STREAM <<"SharedData::"<<__func__<< "    unsubscribed " << signal->name << endl;
                }
                catch (Tango::DevFailed &e)
                {
                    //	Do nothing
                    //	Unregister failed means Register has also failed
                    INFO_STREAM <<"SharedData::"<<__func__<< "    ERROR unsubscribing " << signal->name << " err="<<e.errors[0].desc<< endl;
                }
            }
        }
        veclock.writerIn();
        for (auto &signal : signals)
        {
            signal->siglock->writerIn();
            if (signal->event_id != ERR && signal->attr != nullptr)
            {
                signal->archive_cb.reset();
                DEBUG_STREAM <<"SharedData::"<<__func__<< "    deleted cb " << signal->name << endl;
            }
            if(signal->attr)
            {
                signal->attr.reset();
                DEBUG_STREAM <<"SharedData::"<<__func__<< "    deleted proxy " << signal->name << endl;
            }
            signal->siglock->writerOut();
            DEBUG_STREAM <<"SharedData::"<<__func__<< "    deleted lock " << signal->name << endl;
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
    void SharedData::add(const string& signame, const vector<string> & contexts)
    {
        add(signame, contexts, NOTHING, false);
    }
    //=============================================================================
    /**
     * Add a new signal.
     */
    //=============================================================================
    void SharedData::add(const string& signame, const vector<string> & contexts, int to_do, bool start)
    {
        DEBUG_STREAM << "SharedData::"<<__func__<<": Adding " << signame << " to_do="<<to_do<<" start="<<(start ? "Y" : "N")<< endl;
        {
            veclock.readerIn();

            //	Check if already subscribed
            bool found = false;
            std::shared_ptr<HdbSignal> signal;
            try
            {
                signal = get_signal(signame);
                found = true;
            }
            catch(Tango::DevFailed &e)
            {
            }
            veclock.readerOut();
            DEBUG_STREAM << "SharedData::"<<__func__<<": signame="<<signame<<" found="<<(found ? "Y" : "N") << " start="<<(start ? "Y" : "N")<< endl;
            if (found && !start)
                Tango::Except::throw_exception(
                        (const char *)"BadSignalName",
                        "Signal " + signame + " already subscribed",
                        (const char *)"SharedData::add()");
            if (!found)
            {
                // on name, split device name and attrib name
                string::size_type idx = signame.find_last_of('/');
                if (idx == string::npos)
                {
                    Tango::Except::throw_exception(
                            (const char *)"SyntaxError",
                            "Syntax error in signal name " + signame,
                            (const char *)"SharedData::add()");
                }
                // Build Hdb Signal object
                signal = std::make_shared<HdbSignal>();
                signal->name      = signame;
                signal->siglock = std::make_shared<ReadersWritersLock>();
                signal->devname = signal->name.substr(0, idx);
                signal->attname = signal->name.substr(idx+1);
                signal->status = "NOT connected";
                signal->attr = nullptr;
                signal->running = false;
                signal->stopped = true;
                signal->paused = false;
                signal->contexts = contexts;
                signal->contexts_upper = contexts;
                for(auto &it : signal->contexts_upper)
                    std::transform(it.begin(), it.end(), it.begin(), ::toupper);
                //DEBUG_STREAM << "SharedData::"<<__func__<<": signame="<<signame<<" created signal"<< endl;
            }
            if(start)
            {
                signal->siglock->writerIn();
                signal->status = "NOT connected";
                signal->siglock->writerOut();
                //DEBUG_STREAM << "created proxy to " << signame << endl;
                //	create Attribute proxy
                signal->attr = std::make_shared<Tango::AttributeProxy>(signal->name);	//TODO: OK out of siglock? accessed only inside the same thread?
                DEBUG_STREAM << "SharedData::"<<__func__<<": signame="<<signame<<" created proxy"<< endl;
            }
            signal->event_id = ERR;
            signal->event_conf_id = ERR;
            signal->evstate    = Tango::ALARM;
            signal->isZMQ    = false;
            signal->ok_events.counter = 0;
            signal->nok_events.counter = 0;
            signal->first_err = true;
            signal->periodic_ev = -1;
            signal->ttl = DEFAULT_TTL;
            clock_gettime(CLOCK_MONOTONIC, &signal->last_ev);

            if(found && start)
            {
                try
                {
                    if(signal->attr)
                    {
                        Tango::AttributeInfo info = signal->attr->get_config();
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
            if (!found)
            {
                veclock.writerIn();
                //	Add in vector
                signals.push_back(std::move(signal));
                hdb_dev->attr_AttributeNumber_read++;
                
                if(!start)
                    hdb_dev->attr_AttributeStoppedNumber_read++;
                
                veclock.writerOut();
                //DEBUG_STREAM << "SharedData::"<<__func__<<": signame="<<signame<<" push_back signal"<< endl;
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
    void SharedData::update(const string &signame, const vector<string> & contexts)
    {
        DEBUG_STREAM << "SharedData::"<<__func__<<": updating " << signame << " contexts.size=" << contexts.size()<< endl;

        veclock.readerIn();
        // Check if already subscribed
        std::shared_ptr<HdbSignal> signal;
        try
        {
            signal = get_signal(signame);
        }
        catch(Tango::DevFailed &e)
        {
            Tango::Except::throw_exception(
                    (const char *)"BadSignalName",
                    "Signal " + signame + " NOT found",
                    (const char *)"SharedData::update()");
        }
        veclock.readerOut();
        //DEBUG_STREAM << "SharedData::"<<__func__<<": signame="<<signame<<" found="<<(found ? "Y" : "N") << endl;

        signal->siglock->writerIn();
        signal->contexts.clear();
        signal->contexts = contexts;
        signal->contexts_upper = contexts;
        
        for(auto& context : signal->contexts_upper)
            std::transform(context.begin(), context.end(), context.begin(), ::toupper);
        
        signal->siglock->writerOut();
        
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
    void SharedData::updatettl(const string& signame, unsigned int ttl)
    {
        DEBUG_STREAM << "SharedData::"<<__func__<<": updating " << signame << " ttl=" << ttl<< endl;

        veclock.readerIn();
        // Check if already subscribed
        std::shared_ptr<HdbSignal> signal;
        try
        {
            signal = get_signal(signame);
        }
        catch(Tango::DevFailed &e)
        {
            Tango::Except::throw_exception(
                    (const char *)"BadSignalName",
                    "Signal " + signame + " NOT found",
                    (const char *)"SharedData::update()");
        }
        veclock.readerOut();
        //DEBUG_STREAM << "SharedData::"<<__func__<<": signame="<<signame<<" found="<<(found ? "Y" : "N") << endl;
        signal->siglock->writerIn();
        signal->ttl = ttl;
        signal->siglock->writerOut();

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
        for (const auto& sig : signals)
        {
            sig->siglock->writerIn();
            if (sig->event_id==ERR && !sig->stopped)
            {
                if(!sig->attr)
                {
                    try
                    {
                        vector<string> contexts;	//TODO: not used in add(..., true)!!!
                        add(sig->name, contexts, NOTHING, true);

                    }
                    catch (Tango::DevFailed &e)
                    {
                        //Tango::Except::print_exception(e);
                        INFO_STREAM << "SharedData::subscribe_events: error adding  " << sig->name <<" err="<< e.errors[0].desc << endl;
                        sig->status = e.errors[0].desc;
                        sig->siglock->writerOut();
                        continue;
                    }
                }
                sig->archive_cb = std::make_shared<ArchiveCB>(hdb_dev);
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
                    event_conf_id = sig->attr->subscribe_event(
                            Tango::ATTR_CONF_EVENT,
                            sig->archive_cb.get(),
                            /*stateless=*/false);                
                    try
                    {
                        event_id = sig->attr->subscribe_event(
                                Tango::ARCHIVE_EVENT,
                                sig->archive_cb.get(),
                                /*stateless=*/false);
                    }
                    catch (Tango::DevFailed &e)
                    {
                        if (hdb_dev->subscribe_change)
                        {
                            INFO_STREAM <<__func__<< " " << sig->name << "->subscribe_event EXCEPTION, try CHANGE_EVENT" << endl;
                            Tango::Except::print_exception(e);
                            event_id = sig->attr->subscribe_event(
                                    Tango::CHANGE_EVENT,
                                    sig->archive_cb.get(),
                                    /*stateless=*/false);
                            INFO_STREAM <<__func__<< " " << sig->name << "->subscribe_event CHANGE_EVENT SUBSCRIBED" << endl;
                        } 
                        else 
                        {
                            throw(e);
                        }
                    }

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
        
        init_condition.signal();
    }
    //=============================================================================
    //=============================================================================
    auto SharedData::is_initialized() -> bool
    {
        //omni_mutex_lock sync(*this);
        return initialized; 
    }
    //=============================================================================
    /**
     *	return number of signals to be subscribed
     */
    //=============================================================================
    auto SharedData::nb_sig_to_subscribe() -> int
    {
        ReaderLock lock(veclock);

        int nb = 0;
        for(const auto& signal : signals)
        {
            signal->siglock->readerIn();
            if (signal->event_id == ERR && !signal->stopped)
            {
                nb++;
            }
            signal->siglock->readerOut();
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
                for(auto it = signals[i]->contexts.begin(); it != signals[i]->contexts.end(); it++)
                {
                    try
                    {
                        context += *it;
                        if(it != signals[i]->contexts.end() -1)
                            context += "|";
                    }
                    catch(std::out_of_range &e)
                    {

                    }
                }
                stringstream conf_string;
                conf_string << signals[i]->name << ";" << CONTEXT_KEY << "=" << context << ";" << TTL_KEY << "=" << signals[i]->ttl;
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
        for(const auto& signal : signals)
        {
            string signame(signal->name);
            list.push_back(signame);
        }
    }
    //=============================================================================
    /**
     *	Return the list of sources
     */
    //=============================================================================
    auto SharedData::get_sig_source_list() -> vector<bool>
    {
        ReaderLock lock(veclock);
        vector<bool> list;
        for(const auto& signal : signals)
        {
            signal->siglock->readerIn();
            list.push_back(signal->isZMQ);
            signal->siglock->readerOut();
        }
        return list;
    }
    //=============================================================================
    /**
     *	Return the source of specified signal
     */
    //=============================================================================
    auto SharedData::get_sig_source(const string& signame) -> bool
    {
        bool retval = false;
        ReaderLock lock(veclock);

        std::shared_ptr<HdbSignal> signal = get_signal(signame);

        signal->siglock->readerIn();
        retval = signal->isZMQ;
        signal->siglock->readerOut();
        
        return retval;
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
        for(const auto& signal : signals)
        {
            signal->siglock->readerIn();
            if (signal->evstate==Tango::ALARM && signal->running)
            {
                string signame(signal->name);
                list.push_back(signame);
            }
            signal->siglock->readerOut();
        }
    }
    //=============================================================================
    /**
     *	Return the number of signals on error
     */
    //=============================================================================
    auto SharedData::get_sig_on_error_num() -> int
    {
        ReaderLock lock(veclock);
        int num=0;
        for(const auto& signal : signals)
        {
            signal->siglock->readerIn();
            if (signal->evstate==Tango::ALARM && signal->running)
            {
                num++;
            }
            signal->siglock->readerOut();
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
        for(const auto& signal : signals)
        {
            signal->siglock->readerIn();
            if (signal->evstate==Tango::ON || (signal->evstate==Tango::ALARM && !signal->running))
            {
                string signame(signal->name);
                list.push_back(signame);
            }
            signal->siglock->readerOut();
        }
    }
    //=============================================================================
    /**
     *	Return the number of signals not on error
     */
    //=============================================================================
    auto SharedData::get_sig_not_on_error_num() -> int
    {
        ReaderLock lock(veclock);
        int num=0;
        for(const auto& signal : signals)
        {
            signal->siglock->readerIn();
            if (signal->evstate==Tango::ON || (signal->evstate==Tango::ALARM && !signal->running))
            {
                num++;
            }
            signal->siglock->readerOut();
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
        for(const auto& signal : signals)
        {
            signal->siglock->readerIn();
            if (signal->running)
            {
                string signame(signal->name);
                list.push_back(signame);
            }
            signal->siglock->readerOut();
        }
    }
    //=============================================================================
    /**
     *	Return the number of signals started
     */
    //=============================================================================
    auto SharedData::get_sig_started_num() -> int
    {
        ReaderLock lock(veclock);
        int num=0;
        for(const auto& signal : signals)
        {
            signal->siglock->readerIn();
            if (signal->running)
            {
                num++;
            }
            signal->siglock->readerOut();
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
        for(const auto& signal : signals)
        {
            signal->siglock->readerIn();
            if (!signal->running)
            {
                string signame(signal->name);
                list.push_back(signame);
            }
            signal->siglock->readerOut();
        }
    }
    //=============================================================================
    /**
     *	Return the list of errors
     */
    //=============================================================================
    auto SharedData::get_error_list(vector<string> &list) -> bool
    {
        bool changed=false;
        ReaderLock lock(veclock);
        size_t old_size = list.size();
        size_t i = 0;
        for (i=0 ; i<signals.size() && i < old_size ; i++)
        {
            signals[i]->siglock->readerIn();
            string err;
            //if (signals[i].status != STATUS_SUBSCRIBED)
            if ((signals[i]->evstate != Tango::ON) && (signals[i]->running))
            {
                err = signals[i]->status;
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
            signals[i]->siglock->readerOut();
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
                signals[i]->siglock->readerIn();
                string err;
                //if (signals[i].status != STATUS_SUBSCRIBED)
                if ((signals[i]->evstate != Tango::ON) && (signals[i]->running))
                {
                    err = signals[i]->status;
                }
                else
                {
                    err = string("");
                }
                list.push_back(err);
                signals[i]->siglock->readerOut();
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
        for(const auto& signal : signals)
        {
            signal->siglock->readerIn();
            list.push_back(signal->ok_events.counter + signal->nok_events.counter);
            signal->siglock->readerOut();
        }
    }
    //=============================================================================
    /**
     *	Return the number of signals not started
     */
    //=============================================================================
    auto SharedData::get_sig_not_started_num() -> int
    {
        ReaderLock lock(veclock);
        int num=0;
        for(const auto& signal : signals)
        {
            signal->siglock->readerIn();
            if (!signal->running)
            {
                num++;
            }
            signal->siglock->readerOut();
        }
        return num;
    }
    //=============================================================================
    /**
     *	Return the complete, started and stopped lists of signals
     */
    //=============================================================================
    auto SharedData::get_lists(vector<string> &s_list, vector<string> &s_start_list, vector<string> &s_pause_list, vector<string> &s_stop_list, vector<string> &s_context_list, Tango::DevULong *ttl_list) -> bool
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
        size_t i = 0;
        for (i=0 ; i<signals.size() && i< old_s_list_size; i++)
        {
            string	signame(signals[i]->name);
            if(signame != s_list[i])
            {
                s_list[i] = signame;
                changed = true;
            }
            signals[i]->siglock->readerIn();
            string context;
            for(auto it = signals[i]->contexts.begin(); it != signals[i]->contexts.end(); it++)
            {
                try
                {
                    context += *it;
                    if(it != signals[i]->contexts.end() -1)
                        context += "|";
                }
                catch(std::out_of_range &e)
                {

                }
            }
            if(ttl_list[i] != signals[i]->ttl)
            {
                ttl_list[i] = signals[i]->ttl;
                changed = true;
            }
            signals[i]->siglock->readerOut();
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
                string	signame(signals[i]->name);
                s_list.push_back(signame);
                signals[i]->siglock->readerIn();
                string context;
                for(auto it = signals[i]->contexts.begin(); it != signals[i]->contexts.end(); it++)
                {
                    try
                    {
                        context += *it;
                        if(it != signals[i]->contexts.end() -1)
                            context += "|";
                    }
                    catch(std::out_of_range &e)
                    {

                    }
                }
                signals[i]->siglock->readerOut();
                s_context_list.push_back(context);
            }
        }

        for(const auto& signal : signals)
        {
            string signame(signal->name);
            signal->siglock->readerIn();
            if (signal->running)
            {
                tmp_start_list.push_back(signame);
            }
            else if(signal->paused)
            {
                tmp_pause_list.push_back(signame);
            }
            else if(signal->stopped)
            {
                tmp_stop_list.push_back(signame);
            }
            signal->siglock->readerOut();
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
    void SharedData::set_ok_event(const string& signame)
    {
        //not to be locked, called only inside lock in ArchiveCB::push_event
        auto signal = get_signal(signame);

        signal->siglock->writerIn();
        signal->evstate = Tango::ON;
        signal->status = "Event received";
        signal->ok_events.increment(hdb_dev->stats_window);
        signal->first_err = true;
        clock_gettime(CLOCK_MONOTONIC, &signal->last_ev);
        signal->siglock->writerOut();
    }
    //=============================================================================
    /**
     *	Get the ok counter of event rx
     */
    //=============================================================================
    auto SharedData::get_ok_event(const string& signame) -> uint32_t
    {
        uint32_t retval = 0;
        ReaderLock lock(veclock);
        
        auto signal = get_signal(signame);
        
        signal->siglock->readerIn();
        retval = signal->ok_events.counter;
        signal->siglock->readerOut();
        return retval;
    }
    //=============================================================================
    /**
     *	Get the ok counter of event rx for freq stats
     */
    //=============================================================================
    auto SharedData::get_ok_event_freq(const string &signame) -> uint32_t
    {
        uint32_t retval = 0;
        ReaderLock lock(veclock);
        
        auto signal = get_signal(signame);
        
        signal->siglock->readerIn();
        retval = signal->ok_events.get_freq(hdb_dev->stats_window);
        signal->siglock->readerOut();
        return retval;
    }
    //=============================================================================
    /**
     *	Get last okev timestamp
     */
    //=============================================================================
    auto SharedData::get_last_okev(const string &signame) -> timespec
    {
        timespec retval;
        ReaderLock lock(veclock);
        
        auto signal = get_signal(signame);
        
        signal->siglock->readerIn();
        retval = signal->ok_events.timestamps.back();
        signal->siglock->readerOut();
        return retval;
    }
    //=============================================================================
    /**
     *	Increment the error counter of event rx
     */
    //=============================================================================
    void SharedData::set_nok_event(const string& signame)
    {
        //not to be locked, called only inside lock in ArchiveCB::push_event
        
        auto signal = get_signal(signame);
        
        signal->siglock->writerIn();
        signal->nok_events.increment(hdb_dev->stats_window);
        clock_gettime(CLOCK_MONOTONIC, &signal->last_ev);
        signal->siglock->writerOut();
    }
    //=============================================================================
    /**
     *	Get the error counter of event rx
     */
    //=============================================================================
    auto SharedData::get_nok_event(const string& signame) -> uint32_t
    {
        uint32_t retval = 0;
        ReaderLock lock(veclock);
        
        auto signal = get_signal(signame);
        
        signal->siglock->readerIn();
        retval = signal->nok_events.counter;
        signal->siglock->readerOut();
        return retval;
    }
    //=============================================================================
    /**
     *	Get the error counter of event rx for freq stats
     */
    //=============================================================================
    auto SharedData::get_nok_event_freq(const string& signame) -> uint32_t
    {
        uint32_t retval = 0;
        ReaderLock lock(veclock);
        
        auto signal = get_signal(signame);
        
        signal->siglock->readerIn();
        retval = signal->nok_events.get_freq(hdb_dev->stats_window);
        signal->siglock->readerOut();
        return retval;
    }
    //=============================================================================
    /**
     *	Get last nokev timestamp
     */
    //=============================================================================
    auto SharedData::get_last_nokev(const string& signame) -> timespec
    {
        timespec retval;
        ReaderLock lock(veclock);
        
        auto signal = get_signal(signame);
        
        signal->siglock->readerIn();
        retval = signal->nok_events.timestamps.back();
        signal->siglock->readerOut();
        return retval;
    }
    //=============================================================================
    /**
     *	Set state and status of timeout on periodic event
     */
    //=============================================================================
    void SharedData::set_nok_periodic_event(const string& signame)
    {
        //not to be locked, called only inside lock in ArchiveCB::push_event
        
        auto signal = get_signal(signame);
        
        signal->siglock->writerIn();
        signal->evstate = Tango::ALARM;
        signal->status = "Timeout on periodic event";
        signal->siglock->writerOut();
    }
    //=============================================================================
    /**
     *	Return the status of specified signal
     */
    //=============================================================================
    auto SharedData::get_sig_status(const string &signame) -> string
    {
        string retval;
        ReaderLock lock(veclock);
        
        auto signal = get_signal(signame);
        
        signal->siglock->readerIn();
        retval = signal->status;
        signal->siglock->readerOut();
        return retval;
    }
    //=============================================================================
    /**
     *	Return the state of specified signal
     */
    //=============================================================================
    auto SharedData::get_sig_state(const string &signame) -> Tango::DevState
    {
        Tango::DevState retval = Tango::ON;
        ReaderLock lock(veclock);
        
        auto signal = get_signal(signame);
        
        signal->siglock->readerIn();
        retval = signal->evstate;
        signal->siglock->readerOut();
        return retval;
    }
    //=============================================================================
    /**
     *	Return the context of specified signal
     */
    //=============================================================================
    auto SharedData::get_sig_context(const string &signame) -> string
    {
        string retval;
        ReaderLock lock(veclock);
        
        auto signal = get_signal(signame);
        
        signal->siglock->readerIn();
        for(auto it = signal->contexts.cbegin(); it != signal->contexts.cend(); it++)
        {
            try
            {
                retval += *it;
                if(it != signal->contexts.end() - 1)
                    retval += "|";
            }
            catch(std::out_of_range &e)
            {

            }
        }
        signal->siglock->readerOut();
        return retval;
    }
    //=============================================================================
    /**
     *	Return the ttl of specified signal
     */
    //=============================================================================
    auto SharedData::get_sig_ttl(const string &signame) -> unsigned int
    {
        Tango::DevULong retval = 0;
        ReaderLock lock(veclock);
        
        auto signal = get_signal(signame);
        
        signal->siglock->readerIn();
        retval = signal->ttl;
        signal->siglock->readerOut();
        return retval;
    }
    //=============================================================================
    /**
     *	Set Archive periodic event period
     */
    //=============================================================================
    void SharedData::set_conf_periodic_event(const string &signame, const string &period)
    {
        //not to be locked, called only inside lock in ArchiveCB::push_event
        
        auto signal = get_signal(signame);
        
        signal->siglock->writerIn();
        signal->periodic_ev = atoi(period.c_str());
        signal->siglock->writerOut();
    }
    //=============================================================================
    /**
     *	Check Archive periodic event period
     */
    //=============================================================================
    auto SharedData::check_periodic_event_timeout(unsigned int delay_tolerance_ms) -> int
    {
        ReaderLock lock(veclock);
        timespec now{};
        clock_gettime(CLOCK_MONOTONIC, &now);
        double min_time_to_timeout_ms = ten_s_in_ms;
        for (auto &signal : signals)
        {
            signal->siglock->readerIn();
            if(!signal->running)
            {
                signal->siglock->readerOut();
                continue;
            }
            if(signal->evstate != Tango::ON)
            {
                signal->siglock->readerOut();
                continue;
            }
            if(signal->periodic_ev <= 0)
            {
                signal->siglock->readerOut();
                continue;
            }
            double diff_time_ms = (now.tv_sec - signal->last_ev.tv_sec) * s_to_ms_factor + ((double)(now.tv_nsec - signal->last_ev.tv_nsec))/ms_to_ns_factor;
            double time_to_timeout_ms = (double)(signal->periodic_ev + delay_tolerance_ms) - diff_time_ms;
            signal->siglock->readerOut();
            if(time_to_timeout_ms <= 0)
            {
                signal->siglock->writerIn();
                signal->evstate = Tango::ALARM;
                signal->status = "Timeout on periodic event";
                signal->siglock->writerOut();
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
        for (auto &signal : signals)
        {
            signal->siglock->writerIn();
            signal->nok_events.reset();
            signal->ok_events.reset();
            signal->siglock->writerOut();
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
        for (auto &signal : signals)
        {
            signal->siglock->writerIn();
            signal->nok_events.timestamps.clear();
            signal->ok_events.timestamps.clear();
            signal->siglock->writerOut();
        }
    }
    //=============================================================================
    /**
     *	Return ALARM if at list one signal is not subscribed.
     */
    //=============================================================================
    auto SharedData::state() -> Tango::DevState
    {
        ReaderLock lock(veclock);
        Tango::DevState	state = Tango::ON;
        for (const auto &sig : signals)
        {
            sig->siglock->readerIn();
            if (sig->evstate==Tango::ALARM && sig->running)
            {
                state = Tango::ALARM;

            }
            sig->siglock->readerOut();
            if(state == Tango::ALARM)
                break;
        }
        return state;
    }
    //=============================================================================
    //=============================================================================
    auto SharedData::get_if_stop() -> bool
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
    void SharedData::wait_initialized()
    {
        if(!is_initialized())
        {
            init_condition.wait();
        }
    }

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
    auto SubscribeThread::run_undetached(void * /*ptr*/) -> void *
    {
        INFO_STREAM << "SubscribeThread id="<<omni_thread::self()->id()<<endl;
        while(!shared->get_if_stop())
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
                    shared->wait(period * s_to_ms_factor);
                }
                //shared->unlock();
            }
        }
        shared->unsubscribe_events();
        INFO_STREAM <<"SubscribeThread::"<< __func__<<": exiting..."<<endl;
        return nullptr;
    }
    //=============================================================================
    //=============================================================================



}	//	namespace
