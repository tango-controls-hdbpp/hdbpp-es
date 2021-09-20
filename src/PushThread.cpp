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
    void PushThread::push_back_cmd(std::unique_ptr<HdbCmdData> argin)
    {
        //omni_mutex_lock sync(new_data_mutex);
        //	Add data at end of vector
        if(!is_aborted())
        {
            size_t events_size = 0; 
            {
                omni_mutex_lock lock(new_data_mutex);
                events.push_back(std::move(argin));
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
                signame = string(ev->signal.name);
            list.push_back(signame);
        }
    }

    //=============================================================================
    //=============================================================================
    auto PushThread::get_next_cmds() -> std::vector<std::unique_ptr<HdbCmdData>>
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
        std::vector<std::unique_ptr<HdbCmdData>> cmds;
        for(size_t i = 0; i < events_size; ++i)
        {
            cmds.push_back(std::move(events.front()));
            events.pop_front();
        }
        
        // We cleared the list, so this should be 0.
        hdb_dev->AttributePendingNumber = 0;

        return cmds;
    }

    void PushThread::start_attr(HdbSignal& signal)
    {
        //------Configure DB------------------------------------------------
        auto cmd = std::make_unique<HdbCmdData>(signal, DB_START);
        push_back_cmd(std::move(cmd));
    }

    void PushThread::pause_attr(HdbSignal& signal)
    {
        //------Configure DB------------------------------------------------
        auto cmd = std::make_unique<HdbCmdData>(signal, DB_PAUSE);
        push_back_cmd(std::move(cmd));
    }

    void PushThread::stop_attr(HdbSignal& signal)
    {
        //------Configure DB------------------------------------------------
        auto cmd = std::make_unique<HdbCmdData>(signal, DB_STOP);
        push_back_cmd(std::move(cmd));
    }

    void PushThread::remove_attr(HdbSignal& signal)
    {
        //------Configure DB------------------------------------------------
        auto cmd = std::make_unique<HdbCmdData>(signal, DB_REMOVE);
        push_back_cmd(std::move(cmd));
    }

    void PushThread::add_attr(HdbSignal& signal, int data_type, int data_format, int write_type)
    {
        //------Configure DB------------------------------------------------
        auto cmd = std::make_unique<HdbCmdData>(signal, DB_ADD, data_type, data_format, write_type);
        push_back_cmd(std::move(cmd));
    }

    void PushThread::updatettl(HdbSignal& signal, unsigned int ttl)
    {
        //------Configure DB------------------------------------------------
        auto cmd = std::make_unique<HdbCmdData>(signal, DB_UPDATETTL, ttl);
        push_back_cmd(std::move(cmd));
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
        std::vector<std::unique_ptr<HdbCmdData>> cmds;
        while (!(cmds = get_next_cmds()).empty())
        {
            std::vector<std::tuple<Tango::EventData *, hdbpp::HdbEventDataType>> events;
            std::vector<std::tuple<HdbSignal&, std::chrono::duration<double>>> signals;
            bool batch = batch_insert && cmds.size() > 1;
            for(const auto& cmd : cmds)
            {
                switch(cmd->op_code)
                {
                    case DB_INSERT:
                        {
                            std::chrono::duration<double> rcv_time = std::chrono::seconds(cmd->ev_data->get_date().tv_sec) + std::chrono::milliseconds(cmd->ev_data->get_date().tv_usec);
                            if(batch)
                            {
                                events.emplace_back(std::make_tuple(cmd->ev_data, cmd->ev_data_type));
                                
                                signals.push_back({cmd->signal, rcv_time});
                            }
                            else
                            {
                                auto start = std::chrono::steady_clock::now();
                                try
                                {
                                    mdb->insert_event(cmd->ev_data, cmd->ev_data_type);

                                    auto end = std::chrono::steady_clock::now();
                                    
                                    cmd->signal.set_ok_db(end-start, std::chrono::system_clock::now().time_since_epoch()-rcv_time);
                                }
                                catch(Tango::DevFailed  &e)
                                {
                                    cmd->signal.set_nok_db(string(e.errors[0].desc));
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
                                mdb->insert_history_event(cmd->signal.name, cmd->op_code);
                            }
                            catch(Tango::DevFailed  &e)
                            {
                                ERROR_STREAM << "PushThread::run_undetached: An was error detected when recording a start, stop, pause or remove event for attribute: "
                                    << cmd->signal.name << endl;

                                Tango::Except::print_exception(e);
                            }
                            break;
                        }
                    case DB_UPDATETTL:
                        {
                            try
                            {
                                //	Send it to DB
                                mdb->update_ttl(cmd->signal.name, cmd->ttl);
                            }
                            catch(Tango::DevFailed  &e)
                            {
                                ERROR_STREAM << "PushThread::run_undetached: An was error detected when updating the TTL on attribute: "
                                    << cmd->signal.name << endl;

                                Tango::Except::print_exception(e);
                            }
                            break;
                        }
                    case DB_ADD:
                        {
                            try
                            {
                                //	add it to DB
                                mdb->add_attribute(cmd->signal.name, cmd->data_type, cmd->data_format, cmd->write_type);
                            }
                            catch(Tango::DevFailed  &e)
                            {
                                ERROR_STREAM << "PushThread::run_undetached: An error was detected when adding the attribute: "
                                    << cmd->signal.name << endl;

                                Tango::Except::print_exception(e);
                            }
                            break;
                        }
                }
            }
            if(!events.empty())
            {
                auto start = std::chrono::steady_clock::now();
                try
                {
                    mdb->insert_events(events);

                    auto end = std::chrono::steady_clock::now();
                    
                    size_t n_signals = signals.size();

                    // We can't get the individual speed for each signal
                    for(const auto& sig : signals)
                    {
                        std::get<0>(sig).set_ok_db((end-start)/n_signals, (std::chrono::system_clock::now().time_since_epoch()-std::get<1>(sig))/n_signals);
                    }
                }
                catch(Tango::DevFailed  &e)
                {
                    for(size_t i = 0; i < events.size(); ++i)
                    {
                        auto start = std::chrono::steady_clock::now();
                        HdbSignal& signal = std::get<0>(signals[i]);
                        auto rcv_time = std::get<1>(signals[i]);
                        try
                        {
                            mdb->insert_event(std::get<0>(events[i]), std::get<1>(events[i]));

                            auto end = std::chrono::steady_clock::now();

                            signal.set_ok_db(end-start, std::chrono::system_clock::now().time_since_epoch()-rcv_time);
                        }
                        catch(Tango::DevFailed  &e)
                        {
                            signal.set_nok_db(string(e.errors[0].desc));
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

    auto PushThread::get_abort_loop_period() -> std::chrono::milliseconds
    {
        using namespace std::chrono_literals;
        return -1s;
    }

    void PushThread::do_abort()
    {
        // if waiting for new cmd, signal.
        new_data.signal();
    }
}//	namespace
