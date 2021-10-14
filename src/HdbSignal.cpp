#include "HdbSignal.h"
#include "HdbDevice.h"
#include "SubscribeThread.h"
#include <eventconsumer.h>

namespace HdbEventSubscriber_ns
{
    HdbSignal::HdbSignal(HdbDevice* dev
            , SharedData& vec
            , const std::string& signame
            , const std::vector<std::string>& ctxts) : LogAdapter(dev->_device)
                                                       , name(signame)
                                                       , config_set(false)
                                                       , ok_events(*this)
                                                       , nok_events(*this)
                                                       , okdb_events(*this)
                                                       , nokdb_events(*this)
                                                       , dev(dev)
                                                       , container(vec)
    {
        // on name, split device name and attrib name
        string::size_type idx = name.find_last_of('/');
        if (idx == string::npos)
        {
            Tango::Except::throw_exception(
                    (const char *)"SyntaxError",
                    "Syntax error in signal name " + name,
                    (const char *)"SharedData::add()");
        }
        // Build Hdb Signal object
        devname = name.substr(0, idx);
        attname = name.substr(idx+1);
        status = "NOT connected";
        attr = nullptr;
        state = SignalState::STOPPED;
        contexts = ctxts;
        contexts_upper = ctxts;
        for(auto &it : contexts_upper)
            std::transform(it.begin(), it.end(), it.begin(), ::toupper);
        
        store_time_avg = std::chrono::duration<double>::zero();
        store_time_min = std::chrono::duration<double>::max();
        store_time_max = std::chrono::duration<double>::min();
        process_time_avg = std::chrono::duration<double>::zero();
        process_time_min = std::chrono::duration<double>::max();
        process_time_max = std::chrono::duration<double>::min();

        container.stopped_number++;
    }

    HdbSignal::~HdbSignal()
    {
        try
        {
            remove_callback();
        }
        catch (Tango::DevFailed &e)
        {
            INFO_STREAM <<"HdbSignal::"<< __func__<<": Exception unsubscribing " << name << " err=" << e.errors[0].desc << endl;
        }

        ReaderLock lck(siglock);
        switch(state)
        {
            case SignalState::PAUSED:
                {
                    container.paused_number--;
                    break;
                }
            case SignalState::RUNNING:
                {
                    container.started_number--;
                    break;
                }
            case SignalState::STOPPED:
                {
                    container.stopped_number--;
                    break;
                }
        }
    }

    void HdbSignal::remove_callback()
    {
        bool unsub = false;
        {
            ReaderLock lk(siglock);
            unsub = archive_cb != nullptr;
        }
        if(unsub)
        {
            unsubscribe_event(event_conf_id);
            unsubscribe_event(event_id);
        }

        WriterLock lock(siglock);
        event_id = ERR;
        event_conf_id = ERR;
        archive_cb.reset(nullptr);
        attr.reset(nullptr);
    }

    auto HdbSignal::subscribe_events() -> void
    {
        if(attr)
        {
            WriterLock lock(siglock);
            archive_cb = std::make_unique<ArchiveCB>(dev);

            // Subscribe att_conf_event
            try
            {
                event_conf_id = attr->subscribe_event(
                        Tango::ATTR_CONF_EVENT,
                        archive_cb.get(),
                        /*stateless=*/false);                
            }
            catch (Tango::DevFailed &e)
            {
                INFO_STREAM <<__func__<< " attr->subscribe_event EXCEPTION:" << endl;
                Tango::Except::print_exception(e);
                status = e.errors[0].desc;
                event_conf_id = ERR;
            }

            // Subscribe att_event
            try
            {
                try
                {
                    event_id = attr->subscribe_event(
                            Tango::ARCHIVE_EVENT,
                            archive_cb.get(),
                            /*stateless=*/false);
                }
                catch (Tango::DevFailed &e)
                {
                    Tango::Except::print_exception(e);
                    if (dev->subscribe_change)
                    {
                        INFO_STREAM <<__func__<< " " << name << "->subscribe_event EXCEPTION, try CHANGE_EVENT" << endl;
                        event_id = attr->subscribe_event(
                                Tango::CHANGE_EVENT,
                                archive_cb.get(),
                                /*stateless=*/false);
                        INFO_STREAM <<__func__<< " " << name << "->subscribe_event CHANGE_EVENT SUBSCRIBED" << endl;
                    } 
                    else 
                    {
                        throw(e);
                    }
                }

                // Check event source  ZMQ/Notifd ?
                Tango::ZmqEventConsumer *consumer = 
                    Tango::ApiUtil::instance()->get_zmq_event_consumer();

                isZMQ = (consumer->get_event_system_for_event_id(event_id) == Tango::ZMQ);

                DEBUG_STREAM << name << "(id="<< event_id <<"): Subscribed " << ((isZMQ)? "ZMQ Event" : "NOTIFD Event") << endl;
            }
            catch(Tango::DevFailed& e)
            {
                INFO_STREAM <<__func__<< " attr->subscribe_event EXCEPTION:" << endl;
                Tango::Except::print_exception(e);
                status = e.errors[0].desc;
                event_id = ERR;
            }

        }
    }

    auto HdbSignal::is_current_context(const std::string& context) -> bool
    {
        ReaderLock lock(siglock);
        auto it = find(contexts_upper.begin(), contexts_upper.end(), context);
        if(it != contexts_upper.end())
        {
            return true;
        }
        it = find(contexts_upper.begin(), contexts_upper.end(), ALWAYS_CONTEXT);
        if(it != contexts_upper.end())
        {
            return true;
        }
        return false;
    }

    auto HdbSignal::init() -> void
    {
        WriterLock lock(siglock);
        event_id = ERR;
        event_conf_id = ERR;
        evstate = Tango::ALARM;
        isZMQ = false;
        ok_events.reset();
        nok_events.reset();
        first_err = true;
        ttl = DEFAULT_TTL;
    }

    auto HdbSignal::start() -> void
    {
        WriterLock lock(siglock);
        status = "NOT connected";
        //DEBUG_STREAM << "created proxy to " << signame << endl;
        //	create Attribute proxy
        if(!attr)
        {
            attr = std::make_unique<Tango::AttributeProxy>(name);	//TODO: OK out of siglock? accessed only inside the same thread?
            DEBUG_STREAM << "SharedData::"<<__func__<<": signame="<<name<<" created proxy"<< endl;
        }
        
        try
        {
            get_signal_config();
        }
        catch (Tango::DevFailed &e)
        {
            status = e.errors[0].desc;
            INFO_STREAM <<"HdbSignal::"<<__func__<< " ERROR for " << name << " in get_config err=" << e.errors[0].desc << endl;
        }        
    }
    auto HdbSignal::update_contexts(const std::vector<std::string>& ctxts) -> void
    {
        WriterLock lock(siglock);
        contexts.clear();
        contexts = ctxts;
        contexts_upper = ctxts;

        for(auto& context : contexts_upper)
            std::transform(context.begin(), context.end(), context.begin(), ::toupper);
    }

    auto HdbSignal::get_config() -> std::string
    {
        ReaderLock lock(siglock);
        std::string context = get_contexts();
        std::stringstream conf_string;
        conf_string << name << ";" << CONTEXT_KEY << "=" << context << ";" << TTL_KEY << "=" << ttl;
        return conf_string.str();
    }

    auto HdbSignal::get_error() -> std::string
    {
        ReaderLock lock(siglock);

        if (evstate != Tango::ON && is_running())
        {
            return status;
        }
        ReaderLock lk(dblock);
        if(dbstate == Tango::ALARM)
        {
            return dberror;
        }

        return "";
    }

    auto HdbSignal::set_running() -> void
    {
        auto prev_state = update_state(SignalState::RUNNING);
        
        if(prev_state == SignalState::STOPPED)
        {
            init();
            start();
            subscribe_events();
        }
    }

    auto HdbSignal::set_paused() -> void
    {
        update_state(SignalState::PAUSED);
    }

    auto HdbSignal::set_stopped() -> void
    {
        if(update_state(SignalState::STOPPED) != SignalState::STOPPED)
        {
            try
            {
                remove_callback();
            }
            catch (Tango::DevFailed &e)
            {
                INFO_STREAM << "__func__" << "Error unsubscribing to events for attribute " << name << endl;
            }
        }
    }
    
    auto HdbSignal::update_state(const SignalState& new_state) -> SignalState
    {
        SignalState prev_state;
        {
            WriterLock lock(siglock);
            // Nothing to do, return
            if(state == new_state)
                return state;

            prev_state = state;
            state = new_state;
        }
        switch(prev_state)
        {
            case SignalState::RUNNING:
                {
                container.started_number--;
                break;
                }
            case SignalState::PAUSED:
                {
                container.paused_number--;
                break;
                }
            case SignalState::STOPPED:
                {
                container.stopped_number--;
                break;
                }
        }

        switch(new_state)
        {
            case SignalState::RUNNING:
                {
                container.started_number++;
                break;
                }
            case SignalState::PAUSED:
                {
                container.paused_number++;
                break;
                }
            case SignalState::STOPPED:
                {
                container.stopped_number++;
                break;
                }
        }
        container.event_checker->notify();
        dev->notify_attr_states_updated();

        return prev_state;
    }

    auto HdbSignal::set_ok_event() -> void
    {
        ok_events.increment();
        
        {
        WriterLock lock(siglock);

        evstate = Tango::ON;
        status = "Event received";
        first_err = true;

        container.event_checker->notify();
        }
    }

    auto HdbSignal::set_nok_event() -> void
    {
        nok_events.increment();
        container.event_checker->notify();
    }

    auto HdbSignal::set_nok_periodic_event() -> void
    {
        WriterLock lock(siglock);
        evstate = Tango::ALARM;
        status = "Timeout on periodic event";
    }

    auto HdbSignal::set_nok_db(const std::string& error) -> void
    {
        nokdb_events.increment();
        {
        WriterLock lock(dblock);
        dbstate = Tango::ALARM;
        dberror = STATUS_DB_ERROR;
        if(error.length() > 0)
            dberror += ": " + error;
        }
    }

    auto HdbSignal::set_ok_db(std::chrono::duration<double> store_time, std::chrono::duration<double> process_time) -> void
    {
        {
            WriterLock lock(dblock);

            dbstate = Tango::ON;
            dberror = "";

            store_time_avg = ((store_time_avg * okdb_events.counter.load()) + store_time)/(okdb_events.counter+1);
            //stat store min
            store_time_min = std::min(store_time, store_time_min);
            //stat store max
            store_time_max = std::max(store_time, store_time_max);

            process_time_avg = ((process_time_avg * okdb_events.counter.load()) + process_time)/(okdb_events.counter+1);
            //stat process min
            process_time_min = std::min(process_time, process_time_min);
            //stat process max
            process_time_max = std::max(process_time, process_time_max);
        }
        container.update_timing(store_time, process_time);
        okdb_events.increment();
    }

    auto HdbSignal::get_contexts() -> std::string
    {
        ReaderLock lock(siglock);
        std::stringstream context;
        for(auto it = contexts.begin(); it != contexts.end(); it++)
        {
            try
            {
                context << *it;
                if(it != contexts.end() - 1)
                    context <<  "|";
            }
            catch(std::out_of_range &e)
            {
            }
        }
        return context.str();
    }


    auto HdbSignal::get_signal_config() -> HdbSignal::SignalConfig
    {
        if(!config_set)
        {
            WriterLock lock(siglock);
            if(attr)
            {
                Tango::AttributeInfo info = attr->get_config();

                config.data_type = info.data_type;
                config.data_format = info.data_format;
                config.write_type = info.writable;
                config.max_dim_x = info.max_dim_x;
                config.max_dim_y = info.max_dim_y;

                config_set = true;
            }
            else
            {
                Tango::Except::throw_exception(
                        (const char *)"AttributeNotStarted",
                        "Attribute " + name + " not started.",
                        (const char *)__func__);
            }
        }
        ReaderLock lock(siglock);
        return config;
    }

    void HdbSignal::unsubscribe_event(const int event_id)
    {
        ReaderLock lock(siglock);
        if(event_id != ERR && attr != nullptr)
        {
            //unlocking, locked in SharedData::stop but possible deadlock if unsubscribing remote attribute with a faulty event connection
            //siglock.writerOut();
            attr->unsubscribe_event(event_id);
            //siglock.writerIn();
        }
    }
        auto HdbSignal::set_error(const std::string& err) -> void
        {
            {
                WriterLock lock(siglock);
                evstate = Tango::ALARM;
                status = err;
            }
            container.event_checker->notify();
        }

        auto HdbSignal::set_on() -> void
        {
            {
                WriterLock lock(siglock);
                evstate = Tango::ON;
                status = "Subscribed";
            }
            container.event_checker->notify();
        }
            auto HdbSignal::EventCounter::check_timestamps_in_window() -> std::chrono::time_point<std::chrono::system_clock>
            {
                auto now = std::chrono::system_clock::now();
                std::chrono::duration<double> interval = std::chrono::duration<double>::max();

                std::lock_guard<mutex> lk(timestamps_mutex);
                while(timestamps.size() > 0 && (interval = now - timestamps.front()) > signal.container.stats_window)
                { 
                    timestamps.pop_front();
                }
                return now;
            }

            double HdbSignal::EventCounter::get_freq()
            {
                check_timestamps_in_window();
                std::lock_guard<mutex> lk(timestamps_mutex);
                return timestamps.size()/signal.container.stats_window.count();
            }
}
