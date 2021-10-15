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
#include "PushThread.h"
#include <HdbEventSubscriber.h>
#include "HdbSignal.h"
#include "Consts.h"

namespace HdbEventSubscriber_ns
{
    SharedData::SharedData(HdbDevice *dev, std::chrono::seconds window, std::chrono::milliseconds period): Tango::LogAdapter(dev->_device)
                                            , stats_window(window)
                                            , delay_periodic_event(period)
                                            , initialized(false)
                                            , init_condition(&init_mutex)
    {
        hdb_dev=dev;
        action=NOTHING;
        stop_it=false;
        timing_events = std::make_unique<std::thread>(&SharedData::push_timing_events, this);
        event_checker = std::make_unique<PeriodicEventCheck>(*this);
    }

    SharedData::~SharedData()
    {
        init_condition.broadcast();
        event_checker->abort_periodic_check();
        stop_all();
        WriterLock lock(veclock);
        signals.clear();
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
    void SharedData::remove(const string& signame)
    {
        // Remove in signals list (vector)
        auto sig = get_signal(signame);

        hdb_dev->push_thread->remove_attr(*sig);

        size_t idx = 0;
        {
            WriterLock lock(veclock);
            auto pos = signals.begin();

            for(size_t i = 0; i < signals.size(); ++i, pos++)
            {
                std::shared_ptr<HdbSignal> sig = (signals)[i];
                if(is_same_signal_name(sig->name, signame))
                {
                    // devalidate its index in case it tries to access one of the list
                    signals.erase(pos);
                    DEBUG_STREAM <<"SharedData::"<< __func__<<": removed " << signame << endl;
                    break;
                }
            }
        }

        // then, update property
        {
            omni_mutex_lock sync(*this);
            DEBUG_STREAM <<"SubscribeThread::"<< __func__<<": going to increase action... action="<<action<<"++" << endl;
            if(action <= UPDATE_PROP)
                action++;
        }

        signal();
    }
    //=============================================================================
    /**
     * Start saving on DB a signal.
     */
    //=============================================================================
    void SharedData::start(const string& signame)
    {
        auto signal = get_signal(signame);

        hdb_dev->push_thread->start_attr(*signal);

        signal->set_running();
    }

    //=============================================================================
    /**
     * Pause saving on DB a signal.
     */
    //=============================================================================
    void SharedData::pause(const string& signame)
    {
        auto signal = get_signal(signame);

        hdb_dev->push_thread->pause_attr(*signal);

        signal->set_paused();
    }
    //=============================================================================
    /**
     * Stop saving on DB a signal.
     */
    //=============================================================================
    void SharedData::stop(const string& signame)
    {
        auto signal = get_signal(signame);

        hdb_dev->push_thread->stop_attr(*signal);

        signal->set_stopped();
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
     * Add a new signal.
     */
    //=============================================================================
    void SharedData::add(const string& signame, const vector<string> & contexts, unsigned int ttl)
    {
        auto signal = add(signame, contexts, ttl, false);
        add(signal, NOTHING, false);
        hdb_dev->push_thread->updatettl(*signal, ttl);
    }
    //=============================================================================
    /**
     * Add a new signal.
     */
    //=============================================================================
    void SharedData::add(std::shared_ptr<HdbSignal> sig, int to_do, bool start)
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
    void SharedData::add(const string &signame, const vector<string> & contexts, int data_type, int data_format, int write_type, int to_do, bool start)
    {
        auto signal = add(signame, contexts, DEFAULT_TTL, start);

        hdb_dev->push_thread->add_attr(*signal, data_type, data_format, write_type);

        add(signal, to_do, start);
    }

    std::shared_ptr<HdbSignal> SharedData::add(const string& signame, const vector<string> & contexts, unsigned int ttl, bool start)
    {
        DEBUG_STREAM << "SharedData::"<<__func__<<": Adding " << signame << " start="<<(start ? "Y" : "N")<< endl;

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
            veclock.writerIn();
            signal = std::make_shared<HdbSignal>(hdb_dev, *this, signame, contexts, ttl);
            
            // Add in vector
            signals.push_back(signal);
            
            veclock.writerOut();
        }

        return signal;

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

        hdb_dev->push_thread->updatettl(*sig, ttl);
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
                    add(sig, NOTHING, true);

                }
                catch (Tango::DevFailed &e)
                {
                    //Tango::Except::print_exception(e);
                    INFO_STREAM << "SharedData::subscribe_events: error adding  " << sig->name <<" err="<< e.errors[0].desc << endl;
                }


                sig->subscribe_events();
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
    auto SharedData::get_sig_list(vector<string> &list) -> bool
    {
        std::function<std::string(const HdbSignal&)> f = &HdbSignal::name;
        return populate_vector(list, signals_number_changed, f);
    }
    auto SharedData::get_sig_contexts_list(vector<string> &list) -> bool
    {
        std::function<std::string(const HdbSignal&)> f = &HdbSignal::get_contexts;
        return populate_vector(list, signals_context_changed, f);
    }
    auto SharedData::get_sig_ttl_list(vector<unsigned int> &list) -> bool
    {
        std::function<unsigned int(const HdbSignal&)> f = &HdbSignal::get_ttl;
        return populate_vector(list, signals_ttl_changed, f);
    }
    //=============================================================================
    /**
     * Return the list of sources
     */
    //=============================================================================
    auto SharedData::get_sig_source_list(vector<bool> &list) -> bool
    {
        std::function<bool(const HdbSignal&)> f = &HdbSignal::is_ZMQ;
        return populate_vector(list, signals_source_changed, f);
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
    auto SharedData::get_sig_on_error_list(std::unordered_set<std::string> &list, bool force) -> bool
    {
        std::lock_guard<std::mutex> lock(signals_info_mutex);
        bool ret = build_sig_on_error_lists();
        list.insert(on_error_signals.begin(), on_error_signals.end());

        return ret;
    }
    //=============================================================================
    /**
     * Return the number of signals on error
     */
    //=============================================================================
    auto SharedData::get_sig_on_error_num() -> size_t
    {
        std::lock_guard<std::mutex> lk(signals_info_mutex);
        bool ret = build_sig_on_error_lists();
        
        return on_error_signals.size();
    }
    
    auto SharedData::get_sig_not_on_error_list(std::unordered_set<std::string> &list) -> bool
    {
        std::lock_guard<std::mutex> lock(signals_info_mutex);
        bool ret = build_sig_on_error_lists();
        list.insert(not_on_error_signals.begin(), not_on_error_signals.end());

        return ret;
    }
    
    auto SharedData::get_sig_not_on_error_num() -> size_t
    {
        std::lock_guard<std::mutex> lk(signals_info_mutex);
        bool ret = build_sig_on_error_lists();
        return not_on_error_signals.size();
    }
    
    //=============================================================================
    /**
     * Return the list of errors
     */
    //=============================================================================
    auto SharedData::get_error_list(vector<string> &list) -> bool
    {
        std::function<std::string(const HdbSignal&)> f = &HdbSignal::get_error;
        return populate_vector(list, signals_error_changed, f);
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
     *	Get the error counter of db saving
     */
    //=============================================================================
    auto SharedData::get_nok_db(const string &signame) -> uint32_t
    {
        auto signal = get_signal(signame);

        return signal->get_nok_db();
    }
    //=============================================================================
    /**
     *	Get the error counter of db saving for freq stats
     */
    //=============================================================================
    auto SharedData::get_nok_db_freq(const string &signame) -> uint32_t
    {
        auto signal = get_signal(signame);

        return signal->get_nok_db_freq();
    }
    //=============================================================================
    /**
     *	Get avg store time
     */
    //=============================================================================
    auto SharedData::get_avg_store_time(const string &signame) -> std::chrono::duration<double>
    {
        auto signal = get_signal(signame);

        return signal->get_avg_store_time();
    }
    //=============================================================================
    /**
     *	Get min store time
     */
    //=============================================================================
    auto SharedData::get_min_store_time(const string &signame) -> std::chrono::duration<double>
    {
        auto signal = get_signal(signame);

        return signal->get_min_store_time();
    }
    //=============================================================================
    /**
     *	Get max store time
     */
    //=============================================================================
    auto SharedData::get_max_store_time(const string &signame) -> std::chrono::duration<double>
    {
        auto signal = get_signal(signame);

        return signal->get_max_store_time();
    }
    //=============================================================================
    /**
     *	Get avg process time
     */
    //=============================================================================
    auto SharedData::get_avg_process_time(const string &signame) -> std::chrono::duration<double>
    {
        auto signal = get_signal(signame);

        return signal->get_avg_process_time();
    }
    //=============================================================================
    /**
     *	Get min process time
     */
    //=============================================================================
    auto SharedData::get_min_process_time(const string &signame) -> std::chrono::duration<double>
    {
        auto signal = get_signal(signame);

        return signal->get_min_process_time();
    }
    //=============================================================================
    /**
     *	Get max process time
     */
    //=============================================================================
    auto SharedData::get_max_process_time(const string &signame) -> std::chrono::duration<double>
    {
        auto signal = get_signal(signame);

        return signal->get_max_process_time();
    }
    //=============================================================================
    /**
     *	Get last nokdb timestamp
     */
    //=============================================================================
    auto SharedData::get_last_nokdb(const string &signame) -> std::chrono::time_point<std::chrono::system_clock>
    {
        auto signal = get_signal(signame);

        return signal->get_last_nok_db();    
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

        event_checker->set_period(signal, std::chrono::milliseconds(atoi(period.c_str())));
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
        reset_min_max();
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

    auto SharedData::get_record_freq_inst() -> double
    {
        ReaderLock lock(veclock);
        double ret = 0.;
        for(const auto& sig : signals)
        {
            ret += sig->get_ok_events_freq_inst() - sig->get_nok_db_freq_inst();
        }
        return ret;
    }
    
    auto SharedData::get_failure_freq_inst() -> double
    {
        ReaderLock lock(veclock);
        double ret = 0.;
        for(const auto& sig : signals)
        {
            ret += sig->get_nok_events_freq_inst() + sig->get_nok_db_freq_inst();
        }
        return ret;
    }

    auto SharedData::get_record_freq() -> double
    {
        ReaderLock lock(veclock);
        double ret = 0.;
        for(const auto& sig : signals)
        {
            ret += sig->get_ok_events_freq() - sig->get_nok_db_freq();
        }
        return ret;
    }

    auto SharedData::get_failure_freq() -> double
    {
        ReaderLock lock(veclock);
        double ret = 0.;
        for(const auto& sig : signals)
        {
            ret += sig->get_nok_events_freq() + sig->get_nok_db_freq();
        }
        return ret;
    }

    auto SharedData::get_record_freq_list(std::vector<double>& ret) -> bool
    {
        ReaderLock lock(veclock);
        ret.clear();
        for(const auto& sig : signals)
        {
            ret.push_back(sig->get_ok_events_freq() - sig->get_nok_db_freq());
        }
        return true;
    }

    auto SharedData::get_failure_freq_list(std::vector<double>& ret) -> bool
    {
        ReaderLock lock(veclock);
        ret.clear();
        for(const auto& sig : signals)
        {
            ret.push_back(sig->get_nok_events_freq() + sig->get_nok_db_freq());
        }
        return true;
    }

    auto SharedData::get_record_freq_inst_list(std::vector<double>& ret) -> bool
    {
        ReaderLock lock(veclock);
        ret.clear();
        for(const auto& sig : signals)
        {
            ret.push_back(sig->get_ok_events_freq_inst() - sig->get_nok_db_freq_inst());
        }
        return true;
    }

    auto SharedData::get_failure_freq_inst_list(std::vector<double>& ret) -> bool
    {
        ReaderLock lock(veclock);
        ret.clear();
        for(const auto& sig : signals)
        {
            ret.push_back(sig->get_nok_events_freq_inst() + sig->get_nok_db_freq_inst());
        }
        return true;
    }

    auto SharedData::get_event_number_list(std::vector<unsigned int>& ret) -> bool
    {
        ReaderLock lock(veclock);
        ret.clear();
        for(const auto& sig : signals)
        {
            ret.push_back(sig->get_ok_events() + sig->get_nok_events());
        }
        return true;
    }
    
    auto SharedData::get_global_min_store_time() -> std::chrono::duration<double>
    {
        std::lock_guard<std::mutex> lk(timing_mutex);
        return min_store_time;
    }

    auto SharedData::get_global_max_store_time() -> std::chrono::duration<double>
    {
        std::lock_guard<std::mutex> lk(timing_mutex);
        return max_store_time;
    }

    auto SharedData::get_global_min_process_time() -> std::chrono::duration<double>
    {
        std::lock_guard<std::mutex> lk(timing_mutex);
        return min_process_time;
    }

    auto SharedData::get_global_max_process_time() -> std::chrono::duration<double>
    {
        std::lock_guard<std::mutex> lk(timing_mutex);
        return max_process_time;
    }

    auto SharedData::get_sig_started_num() -> size_t
    {
        std::lock_guard<std::mutex> lock(signals_info_mutex);
        return started_signals.size();
    }

    auto SharedData::get_sig_paused_num() -> size_t
    {
        std::lock_guard<std::mutex> lock(signals_info_mutex);
        return paused_signals.size();
    }

    auto SharedData::get_sig_stopped_num() -> size_t
    {
        std::lock_guard<std::mutex> lock(signals_info_mutex);
        return stopped_signals.size();
    }
    
    auto SharedData::get_sig_started_list(std::unordered_set<std::string>& list) -> bool
    {
        return populate_set(list, signals_info_mutex, started_signals_changed, started_signals);
    }
    
    auto SharedData::get_sig_paused_list(std::unordered_set<std::string>& list) -> bool
    {
        return populate_set(list, signals_info_mutex, paused_signals_changed, paused_signals);
    }
    
    auto SharedData::get_sig_stopped_list(std::unordered_set<std::string>& list) -> bool
    {
        return populate_set(list, signals_info_mutex, stopped_signals_changed, stopped_signals);
    }
        
    auto SharedData::register_signal(const HdbSignal::SignalState& state, std::string&& ctxts, const std::string& name) -> void
    {
        {
        std::lock_guard<std::mutex> lock(signals_info_mutex);
        _register_state(state, name);
        }
        hdb_dev->notify_attr_number_updated();
        signals_number_changed = true;
        signals_ttl_changed = true;
        signals_context_changed = true;
    }

    auto SharedData::_register_state(const HdbSignal::SignalState& state, const std::string& name) -> void
    {
        switch(state)
        {
            case HdbSignal::SignalState::PAUSED:
                {
                    paused_signals.insert(name);
                    paused_signals_changed = true;
                    break;
                }
            case HdbSignal::SignalState::RUNNING:
                {
                    started_signals.insert(name);
                    started_signals_changed = true;
                    break;
                }
            case HdbSignal::SignalState::STOPPED:
                {
                    stopped_signals.insert(name);
                    stopped_signals_changed = true;
                    break;
                }
        }
    }
        
    auto SharedData::unregister_signal(const HdbSignal::SignalState& state, const std::string& name) -> void
    {
        {
        std::lock_guard<std::mutex> lock(signals_info_mutex);
        _unregister_state(state, name);
        }
        hdb_dev->notify_attr_number_updated();
        signals_number_changed = true;
    }
    
    auto SharedData::_unregister_state(const HdbSignal::SignalState& state, const std::string& name) -> void
    {
        switch(state)
        {
            case HdbSignal::SignalState::PAUSED:
                {
                    paused_signals.erase(name);
                    paused_signals_changed = true;
                    break;
                }
            case HdbSignal::SignalState::RUNNING:
                {
                    started_signals.erase(name);
                    started_signals_changed = true;
                    break;
                }
            case HdbSignal::SignalState::STOPPED:
                {
                    stopped_signals.erase(name);
                    stopped_signals_changed = true;
                    break;
                }
        }
    }
        
    auto SharedData::switch_state(const HdbSignal::SignalState& prev_state, const HdbSignal::SignalState& new_state, const std::string& name) -> void
    {
        std::lock_guard<std::mutex> lock(signals_info_mutex);
        _unregister_state(prev_state, name);
        _register_state(new_state, name);
    }
    
    auto SharedData::update_ttl(const std::string& name, unsigned int ttl) -> void
    {
        signals_ttl_changed = true;
    }

    auto SharedData::update_contexts(const std::string& name, std::string&& ctxts) -> void
    {
        signals_context_changed = true;
    }
    
    auto SharedData::update_error_state(const std::string& name) -> void
    {
        signals_on_error_changed = true;
        signals_error_changed = true;
    }
    
    auto SharedData::update_errors(const std::string& name) -> void
    {
        signals_error_changed = true;
    }
        
    auto SharedData::reset_min_max() -> void
    {
        std::lock_guard<std::mutex> lk(timing_mutex);
        min_process_time = std::chrono::duration<double>::max();
        max_process_time = std::chrono::duration<double>::min();
        min_store_time = std::chrono::duration<double>::max();
        max_store_time = std::chrono::duration<double>::min();
    }

    auto SharedData::size() -> size_t
    {
        ReaderLock lk(veclock);
        return signals.size();
    }

    auto SharedData::update_timing(std::chrono::duration<double> store_time, std::chrono::duration<double> process_time) -> void
    {
        bool updated = false;
        {
            std::lock_guard<std::mutex> lock(timing_mutex);
            if(process_time < min_process_time)
            {
                updated = true;
                min_process_time = process_time;
            }
            else if(process_time > max_process_time)
            {
                updated = true;
                max_process_time = process_time;
            }
            if(store_time < min_store_time)
            {
                updated = true;
                min_store_time = store_time;
            }
            else if(store_time > max_store_time)
            {
                updated = true;
                max_store_time = store_time;
            }
        }
        if(updated)
            timing_cv.notify_one();
    }
    
    auto SharedData::push_timing_events() -> void
    {
        std::unique_lock<std::mutex> lk(timing_mutex);
        while(!timing_abort)
        {
            timing_cv.wait(lk);
            if(!timing_abort)
            {
                Tango::DevDouble max_stime = max_store_time.count();
                Tango::DevDouble min_stime = min_store_time.count();
                Tango::DevDouble max_ptime = max_process_time.count();
                Tango::DevDouble min_ptime = min_process_time.count();
                hdb_dev->push_events("AttributeMaxStoreTime", &max_stime);
                hdb_dev->push_events("AttributeMinStoreTime", &min_stime);
                hdb_dev->push_events("AttributeMaxProcessingTime", &max_ptime);
                hdb_dev->push_events("AttributeMinProcessingTime", &min_ptime);
            }
        }

    }

    auto SharedData::PeriodicEventCheck::check_periodic_event() -> bool
    {
        if(period < std::chrono::milliseconds::max())
        {
            for(const auto& signal : periods)
            {
                if(signal.first->is_on())
                {
                    return true;
                }
            }
        }
        return false;
    };
    
    auto SharedData::PeriodicEventCheck::check_periodic_event_timeout() -> void
    {
        using namespace std::chrono_literals;
        std::unique_lock<std::mutex> lk(m);
        while(!abort)
        {
            while(!check_periodic_event() && !abort)
                cv.wait(lk);
            while (!abort && check_periodic_event()) {
                if(cv.wait_for(lk, period + signals.delay_periodic_event) == std::cv_status::timeout)
                {
                    for(const auto& signal : periods)
                    {
                        if(signal.second > std::chrono::milliseconds::zero())
                        {
                            signal.first->check_periodic_event_timeout(signal.second + signals.delay_periodic_event);
                        }
                    }
                }
            }
        }
    }
   
    auto SharedData::populate_set(std::unordered_set<std::string>& out, std::mutex& m, std::atomic_bool& flag, const std::unordered_set<std::string>& in, bool force) -> bool
    {
        bool ret = flag.exchange(false);
        if(force || ret)
        {
            std::lock_guard<std::mutex> lk(m);
            out.clear();
            out.insert(in.begin(), in.end());    
        }
        return ret;
    }

    auto SharedData::build_sig_on_error_lists() -> bool
    {
        bool ret = signals_on_error_changed.exchange(false);
        if(signals_number_changed || ret)
        {
            ReaderLock lk(veclock);
            on_error_signals.clear();
            not_on_error_signals.clear();
            for(const auto& signal : signals)
            {
                if(signal->is_on_error())
                {
                    on_error_signals.insert(signal->name);
                }
                if(signal->is_not_on_error())
                {
                    not_on_error_signals.insert(signal->name);
                }
            }
        }
        return ret;
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
        INFO_STREAM <<"SubscribeThread::"<< __func__<<": exiting..."<<endl;
        return nullptr;
    }
    //=============================================================================
    //=============================================================================



} // namespace
