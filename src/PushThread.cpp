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

#include "PushThread.h"
#include <HdbDevice.h>

namespace HdbEventSubscriber_ns
{
    const unsigned int default_period = -1;
    HdbStat::HdbStat(): nokdb_counter(0)
        , nokdb_counter_freq(0)
        , okdb_counter(0)
        , last_nokdb()
        , dbstate()
        , process_time_avg(0)
        , process_time_min(0)
        , process_time_max(0)
        , store_time_avg(0)
        , store_time_min(0)
        , store_time_max(0)
    {
    }
    
    //=============================================================================
    //=============================================================================
    PushThread::PushThread(
            HdbDevice *dev, const string &ds_name, const vector<string>& configuration): AbortableThread(dev->_device)
                                                                                         , new_data(&new_data_mutex)
                                                                                         , max_waiting(0)
    {
        try
        {
            mdb = std::unique_ptr<hdbpp::AbstractDB>(getDBFactory()->create_db(ds_name, configuration));
        }
        catch (Tango::DevFailed &err)
        {
            FATAL_STREAM << __func__ << ": error connecting DB: " << err.errors[0].desc << endl;
            exit();
        }
        batch_insert = mdb->supported(hdbpp::HdbppFeatures::BATCH_INSERTS);

        hdb_dev = dev;
    }

    //=============================================================================
    //=============================================================================
    void PushThread::push_back_cmd(const std::shared_ptr<HdbCmdData>& argin)
    {
        //omni_mutex_lock sync(new_data_mutex);
        //	Add data at end of vector
        if(!is_aborted())
        {
            size_t events_size = 0; 
            {
                omni_mutex_lock lock(new_data_mutex);
                events.push_back(argin);
                events_size = events.size();
                //	Check if nb waiting more the stored one.
                if (events_size > max_waiting)
                    max_waiting = events_size;
                
                hdb_dev->AttributePendingNumber = events_size;
                hdb_dev->AttributeMaxPendingNumber = max_waiting;
            }

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
                INFO_STREAM <<"PushThread::"<< __func__<<": error pushing events="<<e.errors[0].desc<<endl;
            }
#endif
        }
        // Signal only if there was no data
        new_data.signal();
    }

    //=============================================================================
    //=============================================================================
    auto PushThread::nb_cmd_waiting() -> size_t
    {
        omni_mutex_lock sync(new_data_mutex);
        return events.size();
    }
    //=============================================================================
    //=============================================================================
    auto PushThread::get_max_waiting() -> size_t
    {
        omni_mutex_lock sync(new_data_mutex);
        return max_waiting;
    }
    //=============================================================================
    //=============================================================================
    void PushThread::get_sig_list_waiting(vector<string>& list)
    {
        omni_mutex_lock sync(new_data_mutex);
        list.clear();
        for (auto& ev : events)
        {
            string signame;
            if(ev->op_code == DB_INSERT)
                signame = string(ev->ev_data->attr_name);
            else if(ev->op_code == DB_INSERT_PARAM)
                signame = string(ev->ev_data_param->attr_name);
            else
                signame = string(ev->attr_name);
            list.push_back(signame);
        }
    }

    //=============================================================================
    //=============================================================================
    void PushThread::reset_statistics()
    {
        omni_mutex_lock sync(sig_lock);
        for (auto& signal : signals)
        {
            signal.second.nokdb_counter = 0;
            signal.second.okdb_counter = 0;
            signal.second.store_time_avg = 0;
            signal.second.store_time_min = -1;
            signal.second.store_time_max = -1;
            signal.second.process_time_avg = 0;
            signal.second.process_time_min = -1;
            signal.second.process_time_max = -1;
        }
        hdb_dev->attr_AttributeMinStoreTime_read = -1;
        hdb_dev->attr_AttributeMaxStoreTime_read = -1;
        hdb_dev->attr_AttributeMinProcessingTime_read = -1;
        hdb_dev->attr_AttributeMaxProcessingTime_read = -1;
    }
    //=============================================================================
    //=============================================================================
    void PushThread::reset_freq_statistics()
    {
        omni_mutex_lock sync(sig_lock);
        for(auto& signal : signals)
        {
            signal.second.nokdb_counter_freq = 0;
        }
    }
    //=============================================================================
    //=============================================================================
    auto PushThread::get_next_cmds() -> std::vector<std::shared_ptr<HdbCmdData>>
    {
        size_t events_size = 0; 
        
        omni_mutex_lock sync(new_data_mutex);
            
        events_size = events.size();

        while(events_size == 0 && !is_aborted())
        {
            new_data.wait();
            events_size = events.size();
        }

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
        std::vector<std::shared_ptr<HdbCmdData>> cmds;
        for(size_t i = 0; i < events_size; ++i)
        {
            cmds.push_back(events.front());
            events.pop_front();
        }
        
        // We cleared the list, so this should be 0.
        hdb_dev->AttributePendingNumber = 0;

        return cmds;
    }
    //=============================================================================
    /**
     *
     */
    //=============================================================================
    void PushThread::remove(const string& signame)
    {
        sig_lock.lock();
        if(signals.erase(signame) == 0)
        {
            for (auto& signal : signals)
            {
#ifndef _MULTI_TANGO_HOST
                if (HdbDevice::compare_without_domain(signal.first,signame))
#else
                if (!hdb_dev->compare_tango_names(signal.first,signame))
#endif
                {
                    signals.erase(signal.first);
                    break;
                }
            }
        }
        sig_lock.unlock();
    }

    //=============================================================================
    /**
     *	Return the list of signals on error
     */
    //=============================================================================
    auto PushThread::get_sig_on_error_list() -> vector<string>
    {
        sig_lock.lock();
        vector<string> list;
        for(const auto& signal : signals)
        {
            if (signal.second.dbstate==Tango::ALARM)
            {
                list.push_back(signal.first);
            }
        }

        sig_lock.unlock();
        return list;
    }
    //=============================================================================
    /**
     *	Return the number of signals on error
     */
    //=============================================================================
    auto PushThread::get_sig_on_error_num() -> int
    {
        sig_lock.lock();
        int num=0;
        for(const auto& signal : signals)
        {
            if (signal.second.dbstate==Tango::ALARM)
            {
                num++;
            }
        }
        sig_lock.unlock();
        return num;
    }
    //=============================================================================
    /**
     *	Return the list of signals not on error
     */
    //=============================================================================
    auto PushThread::get_sig_not_on_error_list() -> vector<string>
    {
        sig_lock.lock();
        vector<string> list;
        for(const auto& signal : signals)
        {
            if (signal.second.dbstate==Tango::ON)
            {
                list.push_back(signal.first);
            }
        }
        sig_lock.unlock();
        return list;
    }
    //=============================================================================
    /**
     *	Return the number of signals not on error
     */
    //=============================================================================
    auto PushThread::get_sig_not_on_error_num() -> int
    {
        sig_lock.lock();
        int num=0;
        for(const auto& signal : signals)
        {
            if (signal.second.dbstate==Tango::ON)
            {
                num++;
            }
        }
        sig_lock.unlock();
        return num;
    }
    //=============================================================================
    /**
     *	Return the db state of the signal
     */
    //=============================================================================
    auto PushThread::get_sig_state(const string& signame) -> Tango::DevState
    {
        sig_lock.lock();

        Tango::DevState state = get_signal(signame).dbstate;

        sig_lock.unlock();
        //        return Tango::ON;
        return state;
    }
    //=============================================================================
    /**
     *	Return the db error status of the signal
     */
    //=============================================================================
    auto PushThread::get_sig_status(const string& signame) -> string
    {
        sig_lock.lock();

        string status = get_signal(signame).dberror;

        sig_lock.unlock();
        //        return STATUS_DB_ERROR;
        return status;
    }
    //=============================================================================
    /**
     *	Increment the error counter of db saving
     */
    //=============================================================================
    void PushThread::set_nok_db(const string &signame, const string& error)
    {
        sig_lock.lock();

        HdbStat& signal = get_signal(signame);

        if(&signal != &NO_SIGNAL)
        {
            signal.nokdb_counter++;
            signal.nokdb_counter_freq++;
            signal.dbstate = Tango::ALARM;
            signal.dberror = STATUS_DB_ERROR;
            if(error.length() > 0)
                signal.dberror += ": " + error;
            clock_gettime(CLOCK_MONOTONIC, &signal.last_nokdb);
        }
        else
        {
            HdbStat sig;
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
            clock_gettime(CLOCK_MONOTONIC, &sig.last_nokdb);
            signals[signame] = sig;
        }
        sig_lock.unlock();
    }
    //=============================================================================
    /**
     *	Get the error counter of db saving
     */
    //=============================================================================
    auto PushThread::get_nok_db(const string &signame) -> uint32_t
    {
        sig_lock.lock();

        uint32_t nok_db = get_signal(signame).nokdb_counter;

        sig_lock.unlock();
        //        return 0;
        return nok_db;
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
    auto PushThread::get_nok_db_freq(const string &signame) -> uint32_t
    {
        sig_lock.lock();


        uint32_t nok_db_freq = get_signal(signame).nokdb_counter_freq;

        sig_lock.unlock();
        //        return 0;
        return nok_db_freq;
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
    auto PushThread::get_avg_store_time(const string &signame) -> double
    {
        sig_lock.lock();

        double avg_store_time = get_signal(signame).store_time_avg;

        sig_lock.unlock();

        //        return -1;
        return avg_store_time;
    }
    //=============================================================================
    /**
     *	Get min store time
     */
    //=============================================================================
    auto PushThread::get_min_store_time(const string &signame) -> double
    {
        sig_lock.lock();

        double min_store_time = get_signal(signame).store_time_min;

        sig_lock.unlock();

        //        return -1;
        return min_store_time;
    }
    //=============================================================================
    /**
     *	Get max store time
     */
    //=============================================================================
    auto PushThread::get_max_store_time(const string &signame) -> double
    {
        sig_lock.lock();

        double max_store_time = get_signal(signame).store_time_max;

        sig_lock.unlock();

        //        return -1;
        return max_store_time;
    }
    //=============================================================================
    /**
     *	Get avg process time
     */
    //=============================================================================
    auto PushThread::get_avg_process_time(const string &signame) -> double
    {
        sig_lock.lock();

        double avg_process_time = get_signal(signame).process_time_avg;

        sig_lock.unlock();

        //        return -1;
        return avg_process_time;
    }
    //=============================================================================
    /**
     *	Get min process time
     */
    //=============================================================================
    auto PushThread::get_min_process_time(const string &signame) -> double
    {
        sig_lock.lock();

        double min_process_time = get_signal(signame).process_time_min;

        sig_lock.unlock();

        //        return -1;
        return min_process_time;
    }
    //=============================================================================
    /**
     *	Get max process time
     */
    //=============================================================================
    auto PushThread::get_max_process_time(const string &signame) -> double
    {
        sig_lock.lock();

        double max_process_time = get_signal(signame).process_time_max;

        sig_lock.unlock();

        //        return -1;
        return max_process_time;
    }
    //=============================================================================
    /**
     *	Get last nokdb timestamp
     */
    //=============================================================================
    auto PushThread::get_last_nokdb(const string &signame) -> timespec
    {
        sig_lock.lock();

        timespec last_nokdb = get_signal(signame).last_nokdb;

        sig_lock.unlock();
        //        timeval ret;
        //        ret.tv_sec=0;
        //        ret.tv_usec=0;
        return last_nokdb;
    }
    //=============================================================================
    /**
     *	reset state
     */
    //=============================================================================
    void PushThread::set_ok_db(const string &signame, double store_time, double process_time)
    {
        sig_lock.lock();

        HdbStat& signal = get_signal(signame);

        if(&signal != &NO_SIGNAL)
        {
            signal.dbstate = Tango::ON;
            signal.dberror = "";
            signal.store_time_avg = ((signal.store_time_avg * signal.okdb_counter) + store_time)/(signal.okdb_counter+1);
            //signal store min
            if(signal.store_time_min == -1)
                signal.store_time_min = store_time;
            if(store_time < signal.store_time_min)
                signal.store_time_min = store_time;
            //signal store max
            if(signal.store_time_max == -1)
                signal.store_time_max = store_time;
            if(store_time > signal.store_time_max)
                signal.store_time_max = store_time;

            signal.process_time_avg = ((signal.process_time_avg * signal.okdb_counter) + process_time)/(signal.okdb_counter+1);
            //signal process min
            if(signal.process_time_min == -1)
                signal.process_time_min = process_time;
            if(process_time < signal.process_time_min)
                signal.process_time_min = process_time;
            //signal process max
            if(signal.process_time_max == -1)
                signal.process_time_max = process_time;
            if(process_time > signal.process_time_max)
                signal.process_time_max = process_time;
            signal.okdb_counter++;
        }
        else
        {
            HdbStat sig;
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
            signals[signame] = sig;
        }
        //global store min
        if(hdb_dev->attr_AttributeMinStoreTime_read == -1)
            hdb_dev->attr_AttributeMinStoreTime_read = store_time;
        if(store_time < hdb_dev->attr_AttributeMinStoreTime_read)
            hdb_dev->attr_AttributeMinStoreTime_read = store_time;
        //global store max
        if(hdb_dev->attr_AttributeMaxStoreTime_read == -1)
            hdb_dev->attr_AttributeMaxStoreTime_read = store_time;
        if(store_time > hdb_dev->attr_AttributeMaxStoreTime_read)
            hdb_dev->attr_AttributeMaxStoreTime_read = store_time;
        //global process min
        if(hdb_dev->attr_AttributeMinProcessingTime_read == -1)
            hdb_dev->attr_AttributeMinProcessingTime_read = process_time;
        if(process_time < hdb_dev->attr_AttributeMinProcessingTime_read)
            hdb_dev->attr_AttributeMinProcessingTime_read = process_time;
        //global process max
        if(hdb_dev->attr_AttributeMaxProcessingTime_read == -1)
            hdb_dev->attr_AttributeMaxProcessingTime_read = process_time;
        if(process_time > hdb_dev->attr_AttributeMaxProcessingTime_read)
            hdb_dev->attr_AttributeMaxProcessingTime_read = process_time;
        sig_lock.unlock();
    }

    void PushThread::start_attr(const string &signame)
    {
        //------Configure DB------------------------------------------------
        auto cmd = std::make_shared<HdbCmdData>(DB_START, signame);
        push_back_cmd(cmd);
    }

    void PushThread::pause_attr(const string &signame)
    {
        //------Configure DB------------------------------------------------
        auto cmd = std::make_shared<HdbCmdData>(DB_PAUSE, signame);
        push_back_cmd(cmd);
    }

    void PushThread::stop_attr(const string &signame)
    {
        //------Configure DB------------------------------------------------
        auto cmd = std::make_shared<HdbCmdData>(DB_STOP, signame);
        push_back_cmd(cmd);
    }

    void PushThread::remove_attr(const string &signame)
    {
        //------Configure DB------------------------------------------------
        auto cmd = std::make_shared<HdbCmdData>(DB_REMOVE, signame);
        push_back_cmd(cmd);
    }

    void PushThread::add_attr(const string &signame, int data_type, int data_format, int write_type)
    {
        //------Configure DB------------------------------------------------
        auto cmd = std::make_shared<HdbCmdData>(DB_ADD, data_type, data_format, write_type, signame);
        push_back_cmd(cmd);
    }

    void PushThread::updatettl(const string &signame, unsigned int ttl)
    {
        //------Configure DB------------------------------------------------
        auto cmd = std::make_shared<HdbCmdData>(DB_UPDATETTL, ttl, signame);
        push_back_cmd(cmd);
    }

    void PushThread::start_all()
    {
        sig_lock.lock();
        for(const auto& signal : signals)
        {
            start_attr(signal.first);
        }
        sig_lock.unlock();
    }

    void PushThread::pause_all()
    {
        sig_lock.lock();
        for(const auto& signal : signals)
        {
            pause_attr(signal.first);
        }
        sig_lock.unlock();
    }

    void PushThread::stop_all()
    {
        sig_lock.lock();
        for(const auto& signal : signals)
        {
            stop_attr(signal.first);
        }
        sig_lock.unlock();
    }

    //=============================================================================
    /**
     *	Return ALARM if at list one signal is not subscribed.
     */
    //=============================================================================
    auto PushThread::state() -> Tango::DevState 
    {
        sig_lock.lock();
        Tango::DevState	state = Tango::ON;
        for (const auto& signal : signals)
        {
            if (signal.second.dbstate == Tango::ALARM)
            {
                state = Tango::ALARM;
                break;
            }
        }
        sig_lock.unlock();
        return state;
    }


    void PushThread::init_abort_loop()
    {
    }

    //=============================================================================
    /**
     * Execute the thread infinite loop.
     */
    //=============================================================================
    void PushThread::run_thread_loop()
    {
        //	Check if command ready
        std::vector<std::shared_ptr<HdbCmdData>> cmds;
        while (!(cmds = get_next_cmds()).empty())
        {
            std::vector<std::tuple<Tango::EventData *, hdbpp::HdbEventDataType>> events;
            std::vector<std::tuple<std::string, double>> signals;
            bool batch = batch_insert && cmds.size() > 1;
            for(const auto& cmd : cmds)
            {
                switch(cmd->op_code)
                {
                    case DB_INSERT:
                        {
                            double rcv_time = cmd->ev_data->get_date().tv_sec + (double)cmd->ev_data->get_date().tv_usec/s_to_us_factor;
                            if(batch)
                            {
                                events.emplace_back(std::make_tuple(cmd->ev_data, cmd->ev_data_type));
                                
                                signals.emplace_back(std::make_tuple(cmd->ev_data->attr_name, rcv_time));
                            }
                            else
                            {
                                timeval now{};
                                gettimeofday(&now, nullptr);
                                double dstart = now.tv_sec + (double)now.tv_usec/s_to_us_factor;
                                try
                                {
                                    mdb->insert_event(cmd->ev_data, cmd->ev_data_type);

                                    gettimeofday(&now, nullptr);
                                    double  dnow = now.tv_sec + (double)now.tv_usec/s_to_us_factor;
                                    
                                    set_ok_db(cmd->ev_data->attr_name, dnow-dstart, dnow-rcv_time);
                                }
                                catch(Tango::DevFailed  &e)
                                {
                                    set_nok_db(cmd->ev_data->attr_name, string(e.errors[0].desc));
                                    Tango::Except::print_exception(e);
                                }
                            }
                            break;
                        }
                    case DB_INSERT_PARAM:
                        {
                            try
                            {
                                //	Send it to DB
                                mdb->insert_param_event(cmd->ev_data_param, cmd->ev_data_type);
                            }
                            catch(Tango::DevFailed  &e)
                            {
                                ERROR_STREAM << "PushThread::run_undetached: An error was detected when inserting attribute parameter for: "
                                    << cmd->ev_data_param->attr_name << endl;

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
                                mdb->insert_history_event(cmd->attr_name, cmd->op_code);
                            }
                            catch(Tango::DevFailed  &e)
                            {
                                ERROR_STREAM << "PushThread::run_undetached: An was error detected when recording a start, stop, pause or remove event for attribute: "
                                    << cmd->attr_name << endl;

                                Tango::Except::print_exception(e);
                            }
                            break;
                        }
                    case DB_UPDATETTL:
                        {
                            try
                            {
                                //	Send it to DB
                                mdb->update_ttl(cmd->attr_name, cmd->ttl);
                            }
                            catch(Tango::DevFailed  &e)
                            {
                                ERROR_STREAM << "PushThread::run_undetached: An was error detected when updating the TTL on attribute: "
                                    << cmd->attr_name << endl;

                                Tango::Except::print_exception(e);
                            }
                            break;
                        }
                    case DB_ADD:
                        {
                            try
                            {
                                //	add it to DB
                                mdb->add_attribute(cmd->attr_name, cmd->data_type, cmd->data_format, cmd->write_type);
                            }
                            catch(Tango::DevFailed  &e)
                            {
                                ERROR_STREAM << "PushThread::run_undetached: An error was detected when adding the attribute: "
                                    << cmd->attr_name << endl;

                                Tango::Except::print_exception(e);
                            }
                            break;
                        }
                }
            }
            if(!events.empty())
            {
                timeval now{};
                gettimeofday(&now, nullptr);
                double dstart = now.tv_sec + (double)now.tv_usec/s_to_us_factor;
                try
                {
                    mdb->insert_events(events);

                    gettimeofday(&now, nullptr);
                    double dnow = now.tv_sec + (double)now.tv_usec/s_to_us_factor;
                    size_t n_signals = signals.size();

                    // We can't get the individual speed for each signal
                    for(const auto& sig : signals)
                    {
                        set_ok_db(std::get<0>(sig), (dnow-dstart)/n_signals, (dnow-std::get<1>(sig))/n_signals);
                    }
                }
                catch(Tango::DevFailed  &e)
                {
                    for(size_t i = 0; i < events.size(); ++i)
                    {
                        timeval now{};
                        gettimeofday(&now, nullptr);
                        double dstart = now.tv_sec + (double)now.tv_usec/s_to_us_factor;
                        std::string& attr_name = std::get<0>(signals[i]);
                        double rcv_time = std::get<1>(signals[i]);
                        try
                        {
                            mdb->insert_event(std::get<0>(events[i]), std::get<1>(events[i]));

                            gettimeofday(&now, nullptr);
                            double  dnow = now.tv_sec + (double)now.tv_usec/s_to_us_factor;

                            set_ok_db(attr_name, dnow-dstart, dnow-rcv_time);
                        }
                        catch(Tango::DevFailed  &e)
                        {
                            set_nok_db(attr_name, string(e.errors[0].desc));
                            Tango::Except::print_exception(e);
                        }
                    }
                }
            }
        }
    }

    void PushThread::finalize_abort_loop()
    {
        cout <<"PushThread::"<< __func__<<": exiting..."<<endl;
    }

    auto PushThread::get_abort_loop_period_ms() -> unsigned int
    {
        return default_period;
    }

    auto PushThread::get_signal(const std::string& signame) -> HdbStat&
    {
        if(signals.find(signame) != signals.end())
        {
            return signals[signame];
        }

        for(auto& signal : signals)
        {
#ifndef _MULTI_TANGO_HOST
            if (HdbDevice::compare_without_domain(signal.first,signame))
#else
            if (!hdb_dev->compare_tango_names(signal.first,signame))
#endif
            {
                return signal.second;
            }
        }
        return NO_SIGNAL;
    }

    void PushThread::do_abort()
    {
        // if waiting for new cmd, signal.
        new_data.signal();
    }
}//	namespace
