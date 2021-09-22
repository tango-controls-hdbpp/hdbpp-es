#include "HdbSignal.h"
#include "HdbDevice.h"
#include <eventconsumer.h>

namespace HdbEventSubscriber_ns
{
    // Init to 1 cause we make a division by this one
    std::chrono::duration<double> HdbSignal::stats_window = std::chrono::seconds(1);
    std::chrono::milliseconds HdbSignal::delay_periodic_event = std::chrono::milliseconds::zero();

    std::chrono::duration<double> HdbSignal::min_process_time = std::chrono::duration<double>::max();
    std::chrono::duration<double> HdbSignal::max_process_time = std::chrono::duration<double>::min();
    std::chrono::duration<double> HdbSignal::min_store_time = std::chrono::duration<double>::max();
    std::chrono::duration<double> HdbSignal::max_store_time = std::chrono::duration<double>::min();
    std::mutex HdbSignal::static_mutex;
    
    HdbSignal::HdbSignal(HdbDevice* dev
            , const std::string& signame
            , const std::vector<std::string>& ctxts) : LogAdapter(dev->_device)
                                                       , name(signame)
                                                       , config_set(false)
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
        running = false;
        stopped = true;
        paused = false;
        contexts = ctxts;
        contexts_upper = ctxts;
        for(auto &it : contexts_upper)
            std::transform(it.begin(), it.end(), it.begin(), ::toupper);
        event_checker = std::make_unique<PeriodicEventCheck>(*this);
        store_time_avg = std::chrono::duration<double>::zero();
        store_time_min = std::chrono::duration<double>::max();
        store_time_max = std::chrono::duration<double>::min();
        process_time_avg = std::chrono::duration<double>::zero();
        process_time_min = std::chrono::duration<double>::max();
        process_time_max = std::chrono::duration<double>::min();
        using std::placeholders::_1;
        using std::placeholders::_2;
        using std::placeholders::_3;
    }

    void HdbSignal::remove_callback()
    {
        unsubscribe_event(event_id);
        unsubscribe_event(event_conf_id);

        WriterLock lock(siglock);
        event_id = ERR;
        event_conf_id = ERR;
        archive_cb.reset(nullptr);
        attr.reset(nullptr);
    }

    auto HdbSignal::subscribe_events(HdbDevice* dev) -> void
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

    auto HdbSignal::set_ok_event() -> void
    {
        ok_events.increment();
        
        {
        WriterLock lock(siglock);

        evstate = Tango::ON;
        status = "Event received";
        first_err = true;

        event_checker->notify();
        }
    }

    auto HdbSignal::set_nok_event() -> void
    {
        nok_events.increment();
        event_checker->notify();
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
        {
            std::lock_guard<std::mutex> lock(HdbSignal::static_mutex);
            min_process_time = std::min(min_process_time, process_time);
            max_process_time = std::max(max_process_time, process_time);
            min_store_time = std::min(min_store_time, store_time);
            max_store_time = std::max(max_store_time, store_time);
        }
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

    auto HdbSignal::PeriodicEventCheck::check_periodic_event_timeout() -> void
    {
        using namespace std::chrono_literals;
        std::unique_lock<std::mutex> lk(m);
        while(!abort)
        {
            while(!check_periodic_event() && !abort)
                cv.wait(lk);
            while (!abort && check_periodic_event()) {
                if(cv.wait_for(lk, period + HdbSignal::delay_periodic_event) == std::cv_status::timeout)
                {
                    signal.set_nok_periodic_event();
                }
            }
        }
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
}
