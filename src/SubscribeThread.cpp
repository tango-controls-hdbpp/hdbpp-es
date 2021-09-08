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
#include "HdbSignal.h"
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
     * get signal by name.
     */
    //=============================================================================
    auto SharedData::get_signal(const string &signame) -> std::shared_ptr<HdbSignal>
    {
        ReaderLock lock(veclock);
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
     * Check if two strings represents the same signal.
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
        // Remove in signals list (vector)
        auto sig = get_signal(signame);

        if(stop)
        {
            unsubscribe_events(sig);
        }
        else
        {
            WriterLock lock(veclock);
            auto pos = signals.begin();

            for(size_t i = 0; i < signals.size(); ++i, pos++)
            {
                std::shared_ptr<HdbSignal> sig = signals[i];
                if(is_same_signal_name(sig->name, signame))
                {
                    if(sig->is_running())
                        hdb_dev->attr_AttributeStartedNumber_read--;
                    if(sig->is_paused())
                        hdb_dev->attr_AttributePausedNumber_read--;
                    if(sig->is_stopped())
                        hdb_dev->attr_AttributeStoppedNumber_read--;
                    hdb_dev->attr_AttributeNumber_read--;
                    signals.erase(pos);
                    DEBUG_STREAM <<"SharedData::"<< __func__<<": removed " << signame << endl;
                    break;
                }
            }
            pos = signals.begin();

            // then, update property
            {
                omni_mutex_lock sync(*this);
                DEBUG_STREAM <<"SubscribeThread::"<< __func__<<": going to increase action... action="<<action<<"++" << endl;
                if(action <= UPDATE_PROP)
                    action++;
            }
            //put_signal_property(); //TODO: wakeup thread and let it do it? -> signal()
            signal();
        }
    }
    //=============================================================================
    /**
     * Unsubscribe events for a signal
     */
    //=============================================================================
    void SharedData::unsubscribe_events(std::shared_ptr<HdbSignal> sig)
    {
        std::string signame = sig->name;
        try
        {
            DEBUG_STREAM <<"SharedData::"<< __func__<<": unsubscribing events... "<< signame << endl;
            sig->remove_callback();
            DEBUG_STREAM <<"SharedData::"<< __func__<<": unsubscribed events... "<< signame << endl;
        }
        catch (Tango::DevFailed &e)
        {
            // Do nothing
            // Unregister failed means Register has also failed
            //sig->siglock->writerIn();
            INFO_STREAM <<"SharedData::"<< __func__<<": Exception unsubscribing " << signame << " err=" << e.errors[0].desc << endl;
        }
    }
    //=============================================================================
    /**
     * Start saving on DB a signal.
     */
    //=============================================================================
    void SharedData::start(const string& signame)
    {
        vector<string> contexts; //TODO: not used in add(..., true)!!!
        auto signal = get_signal(signame);

        if(!signal->is_running())
        {
            if(signal->is_stopped())
            {
                hdb_dev->attr_AttributeStoppedNumber_read--;
                add(signal, contexts, NOTHING, true);
            }

            if(signal->is_paused())
                hdb_dev->attr_AttributePausedNumber_read--;

            hdb_dev->attr_AttributeStartedNumber_read++;

            signal->set_running();
        }
    }

    //=============================================================================
    /**
     * Pause saving on DB a signal.
     */
    //=============================================================================
    void SharedData::pause(const string& signame)
    {
        auto signal = get_signal(signame);

        if(!signal->is_paused())
        {
            hdb_dev->attr_AttributePausedNumber_read++;
            if(signal->is_running())
                hdb_dev->attr_AttributeStartedNumber_read--;
            if(signal->is_stopped())
                hdb_dev->attr_AttributeStoppedNumber_read--;
            signal->set_paused();
        }
    }
    //=============================================================================
    /**
     * Stop saving on DB a signal.
     */
    //=============================================================================
    void SharedData::stop(const string& signame)
    {
        auto signal = get_signal(signame);

        if(!signal->is_stopped())
        {
            hdb_dev->attr_AttributeStoppedNumber_read++;
            if(signal->is_running())
            {
                hdb_dev->attr_AttributeStartedNumber_read--;
                try
                {
                    unsubscribe_events(signal);
                }
                catch (Tango::DevFailed &e)
                {
                    //Tango::Except::print_exception(e);
                    INFO_STREAM << "SharedData::stop: error removing  " << signame << endl;
                }
            }
            if(signal->is_paused())
            {
                hdb_dev->attr_AttributePausedNumber_read--;
                try
                {
                    unsubscribe_events(signal);
                }
                catch (Tango::DevFailed &e)
                {
                    //Tango::Except::print_exception(e);
                    INFO_STREAM << "SharedData::stop: error removing  " << signame << endl;
                }
            }

            signal->set_stopped();
        }
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
            signal->set_running();
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
            signal->set_paused();
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
            signal->set_stopped();
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
        //to be locked if called outside lock in ArchiveCB::push_event

        auto signal = get_signal(signame);

        return signal->is_running();
    }
    //=============================================================================
    /**
     * Is a signal saved on DB?
     */
    //=============================================================================
    auto SharedData::is_paused(const string& signame) -> bool
    {
        //to be locked if called outside lock in ArchiveCB::push_event

        auto signal = get_signal(signame);

        return signal->is_paused();
    }
    //=============================================================================
    /**
     * Is a signal not subscribed?
     */
    //=============================================================================
    auto SharedData::is_stopped(const string& signame) -> bool
    {
        //to be locked if called outside lock in ArchiveCB::push_event

        auto signal = get_signal(signame);

        return signal->is_stopped();
    }
    //=============================================================================
    /**
     * Is a signal to be archived with current context?
     */
    //=============================================================================
    auto SharedData::is_current_context(const string& signame, string context) -> bool
    {
        std::transform(context.begin(), context.end(), context.begin(), ::toupper);
        // Special case for always context
        if(context == string(ALWAYS_CONTEXT))
        {
            return true;
        }
        //to be locked if called outside lock

        auto signal = get_signal(signame);

        return signal->is_current_context(context);
    }
    //=============================================================================
    /**
     * Is a signal first event arrived?
     */
    //=============================================================================
    auto SharedData::is_first(const string& signame) -> bool
    {
        auto signal = get_signal(signame);

        return signal->is_first();
    }
    //=============================================================================
    /**
     * Set a signal first event arrived
     */
    //=============================================================================
    void SharedData::set_first(const string& signame)
    {
        auto signal = get_signal(signame);

        signal->set_first();
    }
    //=============================================================================
    /**
     * Is a signal first consecutive error event arrived?
     */
    //=============================================================================
    auto SharedData::is_first_err(const string& signame) -> bool
    {
        auto signal = get_signal(signame);

        return signal->is_first_err();
    }
    //=============================================================================
    /**
     * Remove a signal in the list.
     */
    //=============================================================================
    void SharedData::clear_signals()
    {
        DEBUG_STREAM <<"SharedData::"<<__func__<< "    entering..."<< endl;
        veclock.readerIn();
        vector<std::shared_ptr<HdbSignal>> local_signals(signals);
        veclock.readerOut();

        {
            WriterLock lock(veclock);
            signals.clear();
        }

        for (const auto &signal : local_signals)
        {
            signal->remove_callback();
        }
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
    void SharedData::add(std::shared_ptr<HdbSignal> sig, const vector<string> & contexts, int to_do, bool start)
    {
        sig->init();
        if(start)
        {
            sig->start();
        }


        {
            omni_mutex_lock sync(*this);

            DEBUG_STREAM <<"SubscribeThread::"<< __func__<<": going to increase action... action="<<action<<" += " << to_do << endl;
            if(action <= UPDATE_PROP)
            {
                action += to_do;
            }
        }

        DEBUG_STREAM <<"SubscribeThread::"<< __func__<<": exiting... " << sig->name << endl;
        signal();
    }

    //=============================================================================
    /**
     * Add a new signal.
     */
    //=============================================================================
    void SharedData::add(const string& signame, const vector<string> & contexts, int to_do, bool start)
    {
        DEBUG_STREAM << "SharedData::"<<__func__<<": Adding " << signame << " to_do="<<to_do<<" start="<<(start ? "Y" : "N")<< endl;

        // Check if already subscribed
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
        DEBUG_STREAM << "SharedData::"<<__func__<<": signame="<<signame<<" found="<<(found ? "Y" : "N") << " start="<<(start ? "Y" : "N")<< endl;
        if (found && !start)
            Tango::Except::throw_exception(
                    (const char *)"BadSignalName",
                    "Signal " + signame + " already subscribed",
                    (const char *)"SharedData::add()");

        if (!found)
        {
            signal = std::make_shared<HdbSignal>(hdb_dev->_device, signame, contexts);

            veclock.writerIn();
            // Add in vector
            signals.push_back(signal);
            hdb_dev->attr_AttributeNumber_read++;

            if(!start)
                hdb_dev->attr_AttributeStoppedNumber_read++;

            veclock.writerOut();
        }

        add(signal, contexts, to_do, start);

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

        // Check if already subscribed
        auto sig = get_signal(signame);
        //DEBUG_STREAM << "SharedData::"<<__func__<<": signame="<<signame<<" found="<<(found ? "Y" : "N") << endl;

        sig->update_contexts(contexts);

        { 
            omni_mutex_lock sync(*this);
            if(action <= UPDATE_PROP)
                action += UPDATE_PROP;
        }
        DEBUG_STREAM <<"SubscribeThread::"<< __func__<<": exiting... " << signame << endl;

        signal();
    }
    //=============================================================================
    /**
     * Update ttl for a signal.
     */
    //=============================================================================
    void SharedData::updatettl(const string& signame, unsigned int ttl)
    {
        DEBUG_STREAM << "SharedData::"<<__func__<<": updating " << signame << " ttl=" << ttl<< endl;

        auto sig = get_signal(signame);

        //DEBUG_STREAM << "SharedData::"<<__func__<<": signame="<<signame<<" found="<<(found ? "Y" : "N") << endl;

        sig->set_ttl(ttl);

        {
            omni_mutex_lock sync(*this);
            if(action <= UPDATE_PROP)
                action += UPDATE_PROP;
        }
        DEBUG_STREAM <<"SubscribeThread::"<< __func__<<": exiting... " << signame << endl;

        signal();
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
          HdbSignal *sig2 = &signals[ii];
          int ret = pthread_rwlock_trywrlock(&sig2->siglock);
          DEBUG_STREAM << __func__<<": pthread_rwlock_trywrlock i="<<ii<<" name="<<sig2->name<<" just entered " << ret << endl;
          if(ret == 0) pthread_rwlock_unlock(&sig2->siglock);
          }*/
        //omni_mutex_lock sync(*this);
        veclock.readerIn();
        for (const auto& sig : signals)
        {
            if (sig->is_not_subscribed())
            {
                try
                {
                    vector<string> contexts; //TODO: not used in add(..., true)!!!
                    add(sig, contexts, NOTHING, true);

                }
                catch (Tango::DevFailed &e)
                {
                    //Tango::Except::print_exception(e);
                    INFO_STREAM << "SharedData::subscribe_events: error adding  " << sig->name <<" err="<< e.errors[0].desc << endl;
                }


                sig->subscribe_events(hdb_dev);
            }
        }
        veclock.readerOut();

        init_mutex.lock();
        initialized = true;
        init_mutex.unlock();

        init_condition.signal();
    }
    //=============================================================================
    //=============================================================================
    auto SharedData::is_initialized() -> bool
    {
        bool ret;
        init_mutex.lock();
        ret = initialized;
        init_mutex.unlock();
        return ret; 
    }
    //=============================================================================
    /**
     * return number of signals to be subscribed
     */
    //=============================================================================
    auto SharedData::nb_sig_to_subscribe() -> int
    {
        ReaderLock lock(veclock);

        int nb = 0;
        for(const auto& signal : signals)
        {
            if(signal->is_not_subscribed())
            {
                ++nb;
            }
        }
        return nb;
    }
    //=============================================================================
    /**
     * build a list of signal to set HDB device property
     */
    //=============================================================================
    void SharedData::put_signal_property()
    {
        omni_mutex_lock sync(*this);
        DEBUG_STREAM << "SharedData::"<<__func__<<": put_signal_property entering action=" << action << endl;
        //ReaderLock lock(veclock);
        if (action>NOTHING)
        {
            vector<string> v;
            veclock.readerIn();

            for(const auto& signal : signals)
            {
                std::string conf_string = signal->get_config();
                DEBUG_STREAM << "SharedData::"<<__func__<<": "<< conf_string << endl;
                v.push_back(conf_string);
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
     * Return the list of signals
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
     * Return the list of sources
     */
    //=============================================================================
    auto SharedData::get_sig_source_list() -> vector<bool>
    {
        ReaderLock lock(veclock);
        vector<bool> list;
        for(const auto& signal : signals)
        {
            list.push_back(signal->is_ZMQ());
        }
        return list;
    }
    //=============================================================================
    /**
     * Return the source of specified signal
     */
    //=============================================================================
    auto SharedData::get_sig_source(const string& signame) -> bool
    {
        auto signal = get_signal(signame);

        return signal->is_ZMQ();
    }
    //=============================================================================
    /**
     * Return the list of signals on error
     */
    //=============================================================================
    void SharedData::get_sig_on_error_list(vector<string> &list)
    {
        ReaderLock lock(veclock);
        list.clear();
        for(const auto& signal : signals)
        {
            if(signal->is_on_error())
            {
                list.push_back(signal->name);
            }
        }
    }
    //=============================================================================
    /**
     * Return the number of signals on error
     */
    //=============================================================================
    auto SharedData::get_sig_on_error_num() -> int
    {
        ReaderLock lock(veclock);
        int num = 0;
        for(const auto& signal : signals)
        {
            if(signal->is_on_error())
                ++num;
        }
        return num;
    }
    //=============================================================================
    /**
     * Return the list of signals not on error
     */
    //=============================================================================
    void SharedData::get_sig_not_on_error_list(vector<string> &list)
    {
        ReaderLock lock(veclock);
        list.clear();
        for(const auto& signal : signals)
        {
            if(signal->is_not_on_error())
            {
                list.push_back(signal->name);
            }
        }
    }
    //=============================================================================
    /**
     * Return the number of signals not on error
     */
    //=============================================================================
    auto SharedData::get_sig_not_on_error_num() -> int
    {
        ReaderLock lock(veclock);
        int num = 0;
        for(const auto& signal : signals)
        {
            if(signal->is_not_on_error())
                ++num;
        }
        return num;
    }
    //=============================================================================
    /**
     * Return the list of signals started
     */
    //=============================================================================
    void SharedData::get_sig_started_list(vector<string> & list)
    {
        ReaderLock lock(veclock);
        list.clear();
        for(const auto& signal : signals)
        {
            if (signal->is_running())
            {
                list.push_back(signal->name);
            }
        }
    }
    //=============================================================================
    /**
     * Return the number of signals started
     */
    //=============================================================================
    auto SharedData::get_sig_started_num() -> int
    {
        ReaderLock lock(veclock);
        int num = 0;
        for(const auto& signal : signals)
        {
            if (signal->is_running())
            {
                ++num;
            }
        }
        return num;
    }
    //=============================================================================
    /**
     * Return the list of signals not started
     */
    //=============================================================================
    void SharedData::get_sig_not_started_list(vector<string> &list)
    {
        ReaderLock lock(veclock);
        list.clear();
        for(const auto& signal : signals)
        {
            if (!signal->is_running())
            {
                list.push_back(signal->name);
            }
        }
    }
    //=============================================================================
    /**
     * Return the list of errors
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
            string err = signals[i]->get_error();

            if(err != list[i])
            {
                list[i] = err;
                changed = true;
            }
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
                string err = signals[i]->get_error();

                list.push_back(err);
                changed = true;
            }
        }
        return changed;
    }
    //=============================================================================
    /**
     * Return the list of errors
     */
    //=============================================================================
    void SharedData::get_ev_counter_list(vector<uint32_t> &list)
    {
        ReaderLock lock(veclock);
        list.clear();
        for(const auto& signal : signals)
        {
            list.push_back(signal->get_total_events());
        }
    }
    //=============================================================================
    /**
     * Return the number of signals not started
     */
    //=============================================================================
    auto SharedData::get_sig_not_started_num() -> int
    {
        ReaderLock lock(veclock);
        int num = 0;
        for(const auto& signal : signals)
        {
            if (!signal->is_running())
            {
                ++num;
            }
        }
        return num;
    }
    //=============================================================================
    /**
     * Return the complete, started and stopped lists of signals
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
            string signame(signals[i]->name);
            if(signame != s_list[i])
            {
                s_list[i] = signame;
                changed = true;
            }

            auto ttl = signals[i]->get_ttl();
            if(ttl_list[i] != ttl)
            {
                ttl_list[i] = ttl;
                changed = true;
            }

            std::string context = signals[i]->get_contexts();

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
                string signame(signals[i]->name);
                s_list.push_back(signame);

                string context = signals[i]->get_contexts();;
                s_context_list.push_back(context);
            }
        }

        for(const auto& signal : signals)
        {
            string signame(signal->name);
            if (signal->is_running())
            {
                tmp_start_list.push_back(signame);
            }
            else if(signal->is_paused())
            {
                tmp_pause_list.push_back(signame);
            }
            else if(signal->is_stopped())
            {
                tmp_stop_list.push_back(signame);
            }
        }
        //update start list
        for (i=0 ; i<tmp_start_list.size() && i< old_s_start_list_size; i++)
        {
            string signame(tmp_start_list[i]);
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
                string signame(tmp_start_list[i]);
                s_start_list.push_back(signame);
            }
        }
        //update pause list
        for (i=0 ; i<tmp_pause_list.size() && i< old_s_pause_list_size; i++)
        {
            string signame(tmp_pause_list[i]);
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
                string signame(tmp_pause_list[i]);
                s_pause_list.push_back(signame);
            }
        }
        //update stop list
        for (i=0 ; i<tmp_stop_list.size() && i< old_s_stop_list_size; i++)
        {
            string signame(tmp_stop_list[i]);
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
                string signame(tmp_stop_list[i]);
                s_stop_list.push_back(signame);
            }
        }
        return changed;
    }

    //=============================================================================
    /**
     * Get the ok counter of event rx
     */
    //=============================================================================
    auto SharedData::get_ok_event(const string& signame) -> uint32_t
    {
        auto signal = get_signal(signame);

        return signal->get_ok_events();
    }
    //=============================================================================
    /**
     * Get the ok counter of event rx for freq stats
     */
    //=============================================================================
    auto SharedData::get_ok_event_freq(const string &signame) -> uint32_t
    {
        auto signal = get_signal(signame);

        return signal->get_ok_events_freq();
    }
    //=============================================================================
    /**
     * Get last okev timestamp
     */
    //=============================================================================
    auto SharedData::get_last_okev(const string &signame) -> std::chrono::time_point<std::chrono::system_clock>
    {
        auto signal = get_signal(signame);

        return signal->get_last_ok_event();
    }
    //=============================================================================
    /**
     * Get the error counter of event rx
     */
    //=============================================================================
    auto SharedData::get_nok_event(const string& signame) -> uint32_t
    {
        auto signal = get_signal(signame);

        return signal->get_nok_events();
    }
    //=============================================================================
    /**
     * Get the error counter of event rx for freq stats
     */
    //=============================================================================
    auto SharedData::get_nok_event_freq(const string& signame) -> uint32_t
    {
        auto signal = get_signal(signame);

        return signal->get_nok_events_freq();
    }
    //=============================================================================
    /**
     * Get last nokev timestamp
     */
    //=============================================================================
    auto SharedData::get_last_nokev(const string& signame) -> std::chrono::time_point<std::chrono::system_clock>
    {
        auto signal = get_signal(signame);

        return signal->get_last_nok_event();    
    }
    //=============================================================================
    /**
     * Set state and status of timeout on periodic event
     */
    //=============================================================================
    void SharedData::set_nok_periodic_event(const string& signame)
    {
        auto signal = get_signal(signame);

        signal->set_nok_periodic_event();
    }
    //=============================================================================
    /**
     * Return the status of specified signal
     */
    //=============================================================================
    auto SharedData::get_sig_status(const string &signame) -> string
    {
        auto signal = get_signal(signame);

        return signal->get_status();
    }
    //=============================================================================
    /**
     * Return the state of specified signal
     */
    //=============================================================================
    auto SharedData::get_sig_state(const string &signame) -> Tango::DevState
    {
        auto signal = get_signal(signame);

        return signal->get_state();
    }
    //=============================================================================
    /**
     * Return the context of specified signal
     */
    //=============================================================================
    auto SharedData::get_sig_context(const string &signame) -> string
    {
        auto signal = get_signal(signame);

        return signal->get_contexts();
    }
    //=============================================================================
    /**
     * Return the ttl of specified signal
     */
    //=============================================================================
    auto SharedData::get_sig_ttl(const string &signame) -> unsigned int
    {
        auto signal = get_signal(signame);

        return signal->get_ttl();
    }
    //=============================================================================
    /**
     * Set Archive periodic event period
     */
    //=============================================================================
    void SharedData::set_conf_periodic_event(const string &signame, const string &period)
    {
        auto signal = get_signal(signame);

        signal->set_periodic_event(atoi(period.c_str()));
    }
    //=============================================================================
    /**
     * Check Archive periodic event period
     */
    //=============================================================================
    auto SharedData::check_periodic_event_timeout(const std::chrono::milliseconds& delay_tolerance_ms) -> std::chrono::milliseconds
    {
        using namespace std::chrono_literals;
        ReaderLock lock(veclock);
        auto now = std::chrono::system_clock::now();
        
        std::chrono::milliseconds min_time_to_timeout_ms = 10s;
        
        for (auto &signal : signals)
        {
            if(!signal->is_running())
            {
                continue;
            }
            if(signal->get_state() != Tango::ON)
            {
                continue;
            }
            if(signal->get_periodic_event() <= 0)
            {
                continue;
            }

            auto time_to_timeout_ms = signal->check_periodic_event_timeout(now, delay_tolerance_ms);

            if(time_to_timeout_ms > 0s && (time_to_timeout_ms < min_time_to_timeout_ms || min_time_to_timeout_ms == 0s))
                min_time_to_timeout_ms = time_to_timeout_ms;
        }
        return min_time_to_timeout_ms;
    }
    //=============================================================================
    /**
     * Reset statistic counters
     */
    //=============================================================================
    void SharedData::reset_statistics()
    {
        ReaderLock lock(veclock);
        for (auto &signal : signals)
        {
            signal->reset_statistics();
        }
    }
    //=============================================================================
    /**
     * Reset freq statistic counters
     */
    //=============================================================================
    void SharedData::reset_freq_statistics()
    {
        ReaderLock lock(veclock);
        for (auto &signal : signals)
        {
            signal->reset_freq_statistics();
        }
    }
    //=============================================================================
    /**
     * Return ALARM if at list one signal is not subscribed.
     */
    //=============================================================================
    auto SharedData::state() -> Tango::DevState
    {
        ReaderLock lock(veclock);
        Tango::DevState state = Tango::ON;
        for (const auto &sig : signals)
        {
            if (sig->get_state() == Tango::ALARM && sig->is_running())
            {
                state = Tango::ALARM;
                break;
            }
        }
        return state;
    }
    //=============================================================================
    //=============================================================================
    auto SharedData::get_if_stop() -> bool
    {
        omni_mutex_lock sync(*this);
        return stop_it;
    }
    //=============================================================================
    //=============================================================================
    void SharedData::stop_thread()
    {
        {
            omni_mutex_lock sync(*this);
            stop_it = true;
        }
        signal();
        //condition.signal();
    }
    //=============================================================================
    //=============================================================================
    void SharedData::wait_initialized()
    {
        init_mutex.lock();
        while(!initialized)
        {
            init_condition.wait();
        }
        init_mutex.unlock();
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
            // Try to subscribe
            DEBUG_STREAM << __func__<<": AWAKE"<<endl;
            updateProperty();
            shared->subscribe_events();
            int nb_to_subscribe = shared->nb_sig_to_subscribe();
            // And wait a bit before next time or
            // wait a long time if all signals subscribed
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
                    shared->wait(std::chrono::milliseconds(std::chrono::seconds(period)).count());
                }
                //shared->unlock();
            }
        }
        shared->clear_signals();
        INFO_STREAM <<"SubscribeThread::"<< __func__<<": exiting..."<<endl;
        return nullptr;
    }
    //=============================================================================
    //=============================================================================



} // namespace
