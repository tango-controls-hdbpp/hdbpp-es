static const char *RcsId = "$Header: /home/cvsadm/cvsroot/fermi/servers/hdb++/hdb++es/src/HdbDevice.cpp,v 1.8 2014-03-06 15:21:42 graziano Exp $";
//+=============================================================================
//
// file :         HdbEventHandler.cpp
//
// description :  C++ source for the HdbDevice
// project :      TANGO Device Server
//
// $Author: graziano $
//
// $Revision: 1.8 $
//
// $Log: HdbDevice.cpp,v $
// Revision 1.8  2014-03-06 15:21:42  graziano
// StartArchivingAtStartup,
// start_all and stop_all,
// archiving of first event received at subscribe
//
// Revision 1.7  2014-02-20 14:57:50  graziano
// name and path fixing
// bug fixed in remove
//
// Revision 1.6  2013-09-24 08:42:21  graziano
// bug fixing
//
// Revision 1.5  2013-09-02 12:20:11  graziano
// cleaned
//
// Revision 1.4  2013-08-26 13:29:59  graziano
// fixed lowercase and fqdn
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
//
// copyleft :     European Synchrotron Radiation Facility
//                BP 220, Grenoble 38043
//                FRANCE
//
//-=============================================================================





#include <HdbDevice.h>
#include <HdbEventSubscriber.h>
#include <sys/time.h>
#include <netdb.h> //for getaddrinfo
#include "PollerThread.h"
#include "StatsThread.h"
#include "CheckPeriodicThread.h"
#include "PushThread.h"
#include "SubscribeThread.h"
#include "Consts.h"

namespace HdbEventSubscriber_ns
{
    const unsigned int long_storage_time_threshold = 50;

    //=============================================================================
    //=============================================================================
    HdbDevice::~HdbDevice()
    {
        INFO_STREAM << "	Deleting HdbDevice" << endl;
        DEBUG_STREAM << "	Stopping stats thread" << endl;
        stats_thread->abort();
        check_periodic_thread->abort();
        poller_thread->abort();
        check_periodic_thread->join(nullptr);
        DEBUG_STREAM << "	CheckPeriodic thread Joined " << endl;
        stats_thread->join(nullptr);
        DEBUG_STREAM << "	Stats thread Joined " << endl;
        poller_thread->join(nullptr);
        DEBUG_STREAM << "	Polling thread Joined " << endl;
        DEBUG_STREAM << "	Stopping subscribe thread" << endl;
        shared->stop_thread();
        DEBUG_STREAM << "	Subscribe thread Stopped " << endl;
        thread->join(nullptr);
        DEBUG_STREAM << "	Subscribe thread Joined " << endl;
        usleep(hundred_s_in_ms);
        DEBUG_STREAM << "	Stopping push thread" << endl;
        push_thread->abort();
        DEBUG_STREAM << "	Push thread Stopped " << endl;
        push_thread->join(nullptr);
        DEBUG_STREAM << "	Push thread Joined " << endl;
    }
    //=============================================================================
    //=============================================================================
    HdbDevice::HdbDevice(int p, int pp, int s, int c, bool ch, Tango::DeviceImpl *device)
        :Tango::LogAdapter(device)
    {
        this->period = p;
        this->poller_period = pp;
        this->stats_window = s;
        this->check_periodic_delay = c;
        this->subscribe_change = ch;
        _device = device;
#ifdef _USE_FERMI_DB_RW
        host_rw = "";
        auto db = std::make_unique<Tango::Database>();
        try
        {
            Tango::DbData db_data;
            db_data.push_back((Tango::DbDatum("Host")));
            db_data.push_back((Tango::DbDatum("Port")));
            db->get_property("Database",db_data);

            db_data[0] >> host_rw;
            db_data[1] >> port_rw;
        }catch(Tango::DevFailed &e)
        {
            ERROR_STREAM << __FUNCTION__ << " Error reading Database property='" << e.errors[0].desc << "'";
        }
#endif
        attribute_list_str_size = 0;
        attribute_ok_list_str_size = 0;
        attribute_nok_list_str_size = 0;
        attribute_pending_list_str_size = 0;
        attribute_started_list_str_size = 0;
        attribute_paused_list_str_size = 0;
        attribute_stopped_list_str_size = 0;
        attribute_error_list_str_size = 0;
        attribute_context_list_str_size = 0;
        attribute_list_str.reserve(MAX_ATTRIBUTES);
        attribute_ok_list_str.reserve(MAX_ATTRIBUTES);
        attribute_nok_list_str.reserve(MAX_ATTRIBUTES);
        attribute_pending_list_str.reserve(MAX_ATTRIBUTES);
        attribute_started_list_str.reserve(MAX_ATTRIBUTES);
        attribute_paused_list_str.reserve(MAX_ATTRIBUTES);
        attribute_stopped_list_str.reserve(MAX_ATTRIBUTES);
        attribute_error_list_str.reserve(MAX_ATTRIBUTES);
        attribute_context_list_str.reserve(MAX_ATTRIBUTES);
    }
    //=============================================================================
    //=============================================================================
    void HdbDevice::initialize()
    {
        vector<string>	list;
        get_hdb_signal_list(list);
        attr_AttributeNumber_read = 0;
        attr_AttributeStartedNumber_read = 0;
        attr_AttributeStoppedNumber_read = attr_AttributeNumber_read;
        attr_AttributePausedNumber_read = attr_AttributeNumber_read;

        //	Create a thread to subscribe events
        shared = std::make_shared<SharedData>(this);	
        thread = std::make_unique<SubscribeThread>(this);

        attr_AttributeMinStoreTime_read = -1;
        attr_AttributeMaxStoreTime_read = -1;
        attr_AttributeMinProcessingTime_read = -1;
        attr_AttributeMaxProcessingTime_read = -1;
        push_thread = std::make_unique<PushThread>(this, Tango::Util::instance()->get_ds_inst_name(),
                (dynamic_cast<HdbEventSubscriber *>(_device))->libConfiguration);
        stats_thread = std::make_unique<StatsThread>(this);
        stats_thread->set_period(stats_window);
        poller_thread = std::make_unique<PollerThread>(this);
        check_periodic_thread = std::make_unique<CheckPeriodicThread>(this);
        check_periodic_thread->delay_tolerance_ms = check_periodic_delay * s_to_ms_factor;

        build_signal_vector(list, defaultStrategy);

        stats_thread->start();
        push_thread->start();
        poller_thread->start();
        thread->start();
        check_periodic_thread->start();

        //	Wait end of first subscribing loop
        do
        {
            sleep(1);
        }
        while( !shared->is_initialized() );
    }
    //=============================================================================
    //=============================================================================
    //#define TEST
    void HdbDevice::build_signal_vector(const vector<string> &list, const string &defaultStrategy)
    {
        for (const auto &val : list)
        {
            try
            {
                if (!val.empty())
                {
                    vector<string> list_exploded;
                    string_explode(val, string(";"), list_exploded);
                    vector<string> contexts;
                    Tango::DevULong ttl=DEFAULT_TTL;

                    if(list_exploded.size() > 1)
                    {
                        //skip attr_name and transform remaining vector to a map
                        vector<string> v_conf(list_exploded.begin()+1,list_exploded.end());
                        string separator("=");
                        map<string,string> db_conf;
                        //void HdbClient::string_vector2map(vector<string> str, string separator, map<string,string>* results)
                        {
                            for(const auto &conf : v_conf)
                            {
                                string::size_type found_eq = string::npos;
                                found_eq = conf.find_first_of(separator);
                                if(found_eq != string::npos && found_eq > 0)
                                {
                                    db_conf.insert(make_pair(conf.substr(0,found_eq),conf.substr(found_eq+1)));
                                    DEBUG_STREAM <<__func__ << ": added in map '" << conf.substr(0,found_eq) << "' -> '" << conf.substr(found_eq+1) << "' now size="<<db_conf.size();
                                }
                            }
                        }

                        string s_contexts;
                        try
                        {
                            s_contexts = db_conf.at(CONTEXT_KEY);
                            string_explode(s_contexts, string("|"), contexts);
                        }
                        catch(const std::out_of_range& e)
                        {
                            stringstream tmp;
                            tmp << ": Configuration parsing error looking for key '"<<CONTEXT_KEY<<"'";
                            DEBUG_STREAM << __func__ << tmp.str();
                            string context_key = string(CONTEXT_KEY)+string("=");
                            size_t pos = defaultStrategy.find(context_key);
                            if(pos != string::npos)
                            {
                                string_explode(defaultStrategy.substr(pos+context_key.length()), string("|"), contexts);
                            }
                        }
                        catch(...)
                        {
                            DEBUG_STREAM << __func__ << "generic exception looking for '" << CONTEXT_KEY << "'";
                        }
                        string s_ttl;
                        try
                        {
                            s_ttl = db_conf.at(TTL_KEY);
                            stringstream val;
                            val << s_ttl;
                            val >> ttl;
                        }
                        catch(const std::out_of_range& e)
                        {
                            stringstream tmp;
                            tmp << " Configuration parsing error looking for key '"<<TTL_KEY<<"'";
                            DEBUG_STREAM << __func__ << tmp.str();
                        }
                        catch(...)
                        {
                            DEBUG_STREAM << __func__ << ": error extracting ttl from '" << s_ttl << "'";
                        }
                    }

                    vector<string> adjusted_contexts;
                    for(const auto &context : contexts) //vector<string>::iterator it = contexts.begin(); it != contexts.end(); it++)
                    {
                        string context_upper(context);
                        std::transform(context_upper.begin(), context_upper.end(), context_upper.begin(), ::toupper);
                        auto itmap = contexts_map_upper.find(context_upper);
                        if(itmap != contexts_map_upper.end())
                        {
                            adjusted_contexts.push_back(itmap->second);
                        }
                        else
                        {
                            INFO_STREAM << "HdbDevice::" << __func__<< " attr="<<list_exploded[0]<<" IGNORING context '"<< context <<"'";
                        }
                    }
                    shared->add(list_exploded[0], adjusted_contexts);
                    push_thread->updatettl(list_exploded[0], ttl);
                }
            }
            catch (Tango::DevFailed &e)
            {
                Tango::Except::print_exception(e);
                INFO_STREAM << "HdbDevice::" << __func__<< " NOT added " << val << endl;
            }	
        }
    }
    //=============================================================================
    //=============================================================================
    void HdbDevice::add(string &signame, vector<string>& contexts, int data_type, int data_format, int write_type)
    {
        fix_tango_host(signame);
        push_thread->add_attr(signame, data_type, data_format, write_type);
        shared->add(signame, contexts, UPDATE_PROP, false);
    }
    //=============================================================================
    //=============================================================================
    void HdbDevice::remove(string &signame)
    {
        fix_tango_host(signame);
        shared->remove(signame, false);
        push_thread->remove(signame);
        push_thread->remove_attr(signame);
    }
    //=============================================================================
    //=============================================================================
    void HdbDevice::update(string &signame, vector<string>& contexts)
    {
        fix_tango_host(signame);
        shared->update(signame, contexts);
    }
    //=============================================================================
    //=============================================================================
    void HdbDevice::updatettl(string &signame, Tango::DevULong ttl)
    {
        fix_tango_host(signame);
        shared->updatettl(signame, ttl);
        push_thread->updatettl(signame, ttl);
    }
    //=============================================================================
    //=============================================================================
    void HdbDevice::get_hdb_signal_list(vector<string> & list)
    {
        list.clear();
        vector<string>	tmplist;
        //	Read device properties from database.
        //-------------------------------------------------------------
        Tango::DbData	dev_prop;
        dev_prop.push_back(Tango::DbDatum("AttributeList"));

        //	Call database and extract values
        //--------------------------------------------
        //_device->get_property(dev_prop);
        {
            auto db = std::make_unique<Tango::Database>();
            try
            {
                db->get_device_property(_device->get_name(), dev_prop);
            }
            catch(Tango::DevFailed &e)
            {
                stringstream o;
                o << "Error reading properties='" << e.errors[0].desc << "'";
                WARN_STREAM << __FUNCTION__<< o.str();
            }
        }

        //	Extract value
        if (!dev_prop[0].is_empty())
            dev_prop[0]  >>  tmplist;

        for (unsigned int i=0 ; i<tmplist.size() ; i++)
        {
            if(tmplist[i].length() > -1 && tmplist[i][0] != '#')
            {
                string::size_type found = string::npos;
                string tmplist_name;
                string tmplist_conf;
                found = tmplist[i].find_first_of(';');
                if(found != string::npos && found > 0)
                {
                    tmplist_name = tmplist[i].substr(0,found);
                    tmplist_conf = tmplist[i].substr(found+1);
                    size_t pos_strat = tmplist_conf.find(string(CONTEXT_KEY)+"=");
                    size_t pos_ttl = tmplist_conf.find(string(TTL_KEY)+"=");
                    if(tmplist_conf.length() == 0 || (pos_strat == string::npos && pos_ttl == string::npos))
                    {
                        stringstream ssttl;
                        ssttl << DEFAULT_TTL;
                        tmplist_conf = string(CONTEXT_KEY) + "=" + defaultStrategy + TTL_KEY + "=" + ssttl.str();//TODO: loosing all the other configurations if any
                    }
                    else if(pos_strat != string::npos && pos_ttl == string::npos)
                    {
                        if(tmplist_conf[tmplist_conf.length()-1] != ';')
                            tmplist_conf += string(";");
                        stringstream ssttl;
                        ssttl << DEFAULT_TTL;
                        tmplist_conf += string(TTL_KEY) + "=" + ssttl.str();
                    }
                    else if(pos_strat == string::npos && pos_ttl != string::npos)
                    {
                        if(tmplist_conf[tmplist_conf.length()-1] != ';')
                            tmplist_conf += string(";");
                        tmplist_conf += string(CONTEXT_KEY) + "=" + defaultStrategy;
                    }
                }
                else	//if present only the attribute name
                {
                    tmplist_name = tmplist[i];
                    tmplist_conf = string(CONTEXT_KEY) + "=" + defaultStrategy;
                    stringstream ssttl;
                    ssttl << DEFAULT_TTL;
                    tmplist_conf += string(";") + string(TTL_KEY) + "=" + ssttl.str();
                }

                fix_tango_host(tmplist_name);
                tmplist_name += ";";
                tmplist_name += tmplist_conf;
                list.push_back(tmplist_name);
                INFO_STREAM << "HdbDevice::" << __func__ << ": " << i << ": " << tmplist_name << endl;
            }
        }
    }
    //=============================================================================
    //=============================================================================
    void HdbDevice::put_signal_property(vector<string> &prop)
    {
#if 0
        Tango::DbData	data;
        data.push_back(Tango::DbDatum("SignalList"));
        data[0]  <<  prop;
        try
        {
            DECLARE_TIME_VAR	t0, t1;
            GET_TIME(t0);
            //put_property(data);
            GET_TIME(t1);
            DEBUG_STREAM << ELAPSED(t0, t1) << " ms" << endl;
        }
        catch (Tango::DevFailed &e)
        {
            Tango::Except::print_exception(e);
        }
#endif

        Tango::DbData	data;
        data.push_back(Tango::DbDatum("AttributeList"));
        data[0]  <<  prop;
        {
#ifndef _USE_FERMI_DB_RW
            auto db = std::make_unique<Tango::Database>();
#else
            //save properties using host_rw e port_rw to connect to database
            std::unique_ptr<Tango::Database> db;
            if(host_rw != "")
                db = std::make_unique<Tango::Database>(host_rw,port_rw);
            else
                db = std::make_unique<Tango::Database>();
            DEBUG_STREAM << __func__<<": connecting to db "<<host_rw<<":"<<port_rw;
#endif
            try
            {
                DECLARE_TIME_VAR t0{};
                DECLARE_TIME_VAR t1{};
                GET_TIME(t0);
                db->set_timeout_millis(ten_s_in_ms);
                db->put_device_property(_device->get_name(), data);
                GET_TIME(t1);
                DEBUG_STREAM << __func__ << ": saving properties -> " << ELAPSED(t0, t1) << " ms" << endl;
            }
            catch(Tango::DevFailed &e)
            {
                stringstream o;
                o << " Error saving properties='" << e.errors[0].desc << "'";
                WARN_STREAM << __FUNCTION__<< o.str();
            }
        }
    }

    //=============================================================================
    //=============================================================================
    void HdbDevice::get_sig_list(vector<string> &list)
    {
        shared->get_sig_list(list);
    }
    //=============================================================================
    //=============================================================================
    auto HdbDevice::subcribing_state() -> Tango::DevState
    {
        /*
           Tango::DevState	state = DeviceProxy::state();	//	Get Default state
           if (state==Tango::ON)
           state = shared->state();				//	If OK get signals state
           */
        Tango::DevState	evstate =  shared->state();
        Tango::DevState	dbstate =  push_thread->state();
        if(evstate == Tango::ALARM || dbstate == Tango::ALARM)
            return Tango::ALARM;
        return Tango::ON;
    }
    //=============================================================================
    //=============================================================================
    void HdbDevice::get_sig_on_error_list(vector<string> &sig_list)
    {
        shared->get_sig_on_error_list(sig_list);
        vector<string> other_list;
        shared->get_sig_not_on_error_list(other_list);
        //check if signal not in event error is in db error
        for(const auto &sig : other_list) //vector<string>::iterator it=other_list.begin(); it!=other_list.end(); it++)
        {
            if(push_thread->get_sig_state(sig) == Tango::ALARM)
            {
                sig_list.push_back(sig);
            }
        }
    }
    //=============================================================================
    //=============================================================================
    void HdbDevice::get_sig_not_on_error_list(vector<string> & ret_sig_list)
    {
        vector<string> sig_list;
        shared->get_sig_not_on_error_list(sig_list);
        ret_sig_list.clear();
        //check if signal not in event error is in db error
        for(const auto &signal : sig_list)
        {
            if(push_thread->get_sig_state(signal) != Tango::ALARM)
            {
                ret_sig_list.push_back(signal);
            }
        }
    }
    //=============================================================================
    //=============================================================================
    void  HdbDevice::get_sig_started_list(vector<string> & list)
    {
        return shared->get_sig_started_list(list);
    }
    //=============================================================================
    //=============================================================================
    void HdbDevice::get_sig_not_started_list(vector<string> & list)
    {
        shared->get_sig_not_started_list(list);
    }
    //=============================================================================
    //=============================================================================
    auto HdbDevice::get_error_list(vector<string> & error_list) -> bool
    {
        bool changed = false;
        vector<string> sig_list;
        shared->get_sig_list(sig_list);
        changed = shared->get_error_list(error_list);
        vector<string> other_list;
        shared->get_sig_not_on_error_list(other_list);
        //check if signal not in event error is in db error
        for(const auto &signal : other_list)
        {
            //looking for DB errors
            if(push_thread->get_sig_state(signal) == Tango::ALARM)
            {
                //find *it in sig_list and replace string in error_list with "DB error"
                auto itsig_list = find(sig_list.begin(), sig_list.end(), signal);
                size_t idx = itsig_list - sig_list.begin();
                if(itsig_list != sig_list.end() && idx < error_list.size())
                {
                    string dberr = push_thread->get_sig_status(signal);
                    if(dberr != error_list[idx])
                    {
                        error_list[idx] = dberr;
                        changed = true;
                    }
                }
            }
        }
        return changed;
    }
    //=============================================================================
    //=============================================================================
    void  HdbDevice::get_event_number_list()
    {
        vector<string> attribute_list_tmp;
        get_sig_list(attribute_list_tmp);
        for (size_t i=0 ; i<attribute_list_tmp.size() ; i++)
        {
            string signame(attribute_list_tmp[i]);
            /*try
              {
              shared->veclock.readerIn();
              bool is_stopped = shared->is_stopped(signame);
              shared->veclock.readerOut();
              if(is_stopped)
              continue;
              }catch(Tango::DevFailed &e)
              {
              shared->veclock.readerOut();
              continue;
              }*/
            long ok_ev_t=0;
            long nok_ev_t=0;
            try
            {
                ok_ev_t = shared->get_ok_event(signame);
                nok_ev_t = shared->get_nok_event(signame);
            }
            catch(Tango::DevFailed &e)
            {}
            AttributeEventNumberList[i] = ok_ev_t + nok_ev_t;
        }
    }
    //=============================================================================
    //=============================================================================
    auto HdbDevice::get_sig_on_error_num() -> int
    {
        int on_ev_err = shared->get_sig_on_error_num();

        vector<string> other_list;
        shared->get_sig_not_on_error_list(other_list);
        //check if signal not in event error is in db error
        for(const auto &signal : other_list)
        {
            if(push_thread->get_sig_state(signal) == Tango::ALARM)
            {
                on_ev_err++;
            }
        }
        return on_ev_err;
    }
    //=============================================================================
    //=============================================================================
    auto HdbDevice::get_sig_not_on_error_num() -> int
    {
        int not_on_ev_err = shared->get_sig_not_on_error_num();

        vector<string> sig_list;
        shared->get_sig_not_on_error_list(sig_list);
        //check if signal not in event error is in db error
        for(const auto &signal : sig_list)
        {
            if(push_thread->get_sig_state(signal) == Tango::ALARM)
            {
                not_on_ev_err--;
            }
        }
        return not_on_ev_err;
    }
    //=============================================================================
    //=============================================================================
    auto HdbDevice::get_sig_started_num() -> int
    {
        return shared->get_sig_started_num();
    }
    //=============================================================================
    //=============================================================================
    auto HdbDevice::get_sig_not_started_num() -> int
    {
        return shared->get_sig_not_started_num();
    }
    //=============================================================================
    //=============================================================================
    auto HdbDevice::get_sig_status(string &signame) -> string
    {
        string ev_status = shared->get_sig_status(signame);

        //looking for DB errors
        if(push_thread->get_sig_state(signame) == Tango::ALARM)
        {
            ev_status = push_thread->get_sig_status(signame);
        }

        return ev_status;
    }
    //=============================================================================
    //=============================================================================
    auto HdbDevice::get_max_waiting() const -> int
    {
        return push_thread->get_max_waiting();
    }
    //=============================================================================
    //=============================================================================
    auto HdbDevice::nb_cmd_waiting() const -> int
    {
        return push_thread->nb_cmd_waiting();
    }
    //=============================================================================
    //=============================================================================
    void HdbDevice::get_sig_list_waiting(vector<string> & list) const
    {
        push_thread->get_sig_list_waiting(list);
    }
    //=============================================================================
    //=============================================================================
    void HdbDevice::reset_statistics()
    {
        shared->reset_statistics();
        push_thread->reset_statistics();
    }
    //=============================================================================
    //=============================================================================
    void HdbDevice::reset_freq_statistics()
    {
        shared->reset_freq_statistics();
        push_thread->reset_freq_statistics();
    }
    //=============================================================================
    //=============================================================================
    auto HdbDevice::get_lists(vector<string> &_list, vector<string> &_start_list, vector<string> &_pause_list, vector<string> &_stop_list, vector<string> &_context_list, Tango::DevULong *ttl_list) -> bool
    {
        return shared->get_lists(_list, _start_list, _pause_list, _stop_list, _context_list, ttl_list);
    }
    //=============================================================================
    //=============================================================================

    ArchiveCB::ArchiveCB(HdbDevice	*dev):Tango::LogAdapter(dev->_device)
    {
        hdb_dev=dev;
    }

    //=============================================================================
    /**
     *	Attribute and Event management
     */
    //=============================================================================
    void ArchiveCB::push_event(Tango::EventData *data)
    {

        //time_t	t = time(NULL);
        //DEBUG_STREAM << __func__<<": Event '"<<data->attr_name<<"' id="<<omni_thread::self()->id() << "  Received at " << ctime(&t);
        hdb_dev->fix_tango_host(data->attr_name);	//TODO: why sometimes event arrive without fqdn ??

        hdb_dev->shared->veclock.readerIn();
        try
        {
            auto signal = hdb_dev->shared->get_signal(data->attr_name);

            hdbpp::HdbEventDataType ev_data_type;
            ev_data_type.attr_name = data->attr_name;
            if(!hdb_dev->shared->is_first(data->attr_name))
            {
                ev_data_type.max_dim_x = signal.max_dim_x;
                ev_data_type.max_dim_y = signal.max_dim_y;
                ev_data_type.data_type = signal.data_type;
                ev_data_type.data_format = signal.data_format;
                ev_data_type.write_type	= signal.write_type;
            }
            else
            {
                try
                {
                    Tango::AttributeInfo	info = signal.attr->get_config();
                    ev_data_type.data_type = info.data_type;
                    ev_data_type.data_format = info.data_format;
                    ev_data_type.write_type = info.writable;
                    ev_data_type.max_dim_x = info.max_dim_x;
                    ev_data_type.max_dim_y = info.max_dim_y;
                }
                catch (Tango::DevFailed &e)
                {
                    INFO_STREAM<< __func__ << ": FIRST exception in get_config: " << data->attr_name <<" ev_data_type.data_type="<<ev_data_type.data_type<<" err="<<e.errors[0].desc<< endl;
                    hdb_dev->shared->veclock.readerOut();
                    return;
                }
            }

            //	Check if event is an error event.
            if (data->err)
            {
                signal.evstate  = Tango::ALARM;
                signal.siglock->writerIn();
                signal.status = data->errors[0].desc;
                signal.siglock->writerOut();

                INFO_STREAM<< __func__ << ": Exception on " << data->attr_name << endl;
                INFO_STREAM << data->errors[0].desc  << endl;
                try
                {
                    hdb_dev->shared->set_nok_event(data->attr_name);
                }
                catch(Tango::DevFailed &e)
                {
                    WARN_STREAM << __func__ << " Unable to set_nok_event: " << e.errors[0].desc << "'"<<endl;
                }

                try
                {
                    if(!(hdb_dev->shared->is_running(data->attr_name) && hdb_dev->shared->is_first_err(data->attr_name)))
                    {
                        hdb_dev->shared->veclock.readerOut();
                        return;
                    }
                }
                catch(Tango::DevFailed &e)
                {
                    WARN_STREAM << __func__ << " Unable to check if is_running: " << e.errors[0].desc << "'"<<endl;
                }
                try
                {
                    hdb_dev->shared->set_first_err(data->attr_name);
                }
                catch(Tango::DevFailed &e)
                {
                    WARN_STREAM << __func__ << " Unable to set first err: " << e.errors[0].desc << "'"<<endl;
                }
            }
#if 0	//storing quality factor
            else if ( data->attr_value->get_quality() == Tango::ATTR_INVALID )
            {
                INFO_STREAM << "Attribute " << data->attr_name << " is invalid !" << endl;
                try
                {
                    hdb_dev->shared->set_nok_event(data->attr_name);
                }
                catch(Tango::DevFailed &e)
                {
                    WARN_STREAM << __func__ << " Unable to set_nok_event: " << e.errors[0].desc << "'"<<endl;
                }
                //		hdb_dev->error_attribute(data);
                //	Check if already OK
                if (signal.evstate!=Tango::ON)
                {
                    signal.evstate  = Tango::ON;
                    signal.status = STATUS_SUBSCRIBED;
                }
                try
                {
                    if(!(hdb_dev->shared->is_running(data->attr_name) && hdb_dev->shared->is_first_err(data->attr_name)))
                    {
                        hdb_dev->shared->veclock.readerOut();
                        return;
                    }
                }
                catch(Tango::DevFailed &e)
                {
                    WARN_STREAM << __func__ << " Unable to check if is_running: " << e.errors[0].desc << "'"<<endl;
                }
                try
                {
                    hdb_dev->shared->set_first_err(data->attr_name);
                }
                catch(Tango::DevFailed &e)
                {
                    WARN_STREAM << __func__ << " Unable to set first err: " << e.errors[0].desc << "'"<<endl;
                }
            }
#endif
            else
            {
                try
                {
                    hdb_dev->shared->set_ok_event(data->attr_name);	//also reset first_err
                }
                catch(Tango::DevFailed &e)
                {
                    WARN_STREAM << __func__ << " Unable to set_ok_event: " << e.errors[0].desc << "'"<<endl;
                }
                //	Check if already OK
                if (signal.evstate!=Tango::ON)
                {
                    signal.siglock->writerIn();
                    signal.evstate  = Tango::ON;
                    signal.status = "Subscribed";
                    signal.siglock->writerOut();
                }

                //if attribute stopped, just return
                try
                {
                    if(!hdb_dev->shared->is_running(data->attr_name) && !hdb_dev->shared->is_first(data->attr_name))
                    {
                        hdb_dev->shared->veclock.readerOut();
                        return;
                    }
                }
                catch(Tango::DevFailed &e)
                {
                    WARN_STREAM << __func__ << " Unable to check if is_running: " << e.errors[0].desc << "'"<<endl;
                }
                try
                {
                    if(hdb_dev->shared->is_first(data->attr_name))
                        hdb_dev->shared->set_first(data->attr_name);
                }
                catch(Tango::DevFailed &e)
                {
                    WARN_STREAM << __func__ << " Unable to set first: " << e.errors[0].desc << "'"<<endl;
                }
            }

            hdb_dev->shared->veclock.readerOut();

            //OK only with C++11:
            //Tango::EventData	*cmd = new Tango::EventData(*data);
            //OK with C++98 and C++11:
            auto *dev_attr_copy = new Tango::DeviceAttribute();
            if (!data->err)
            {
                dev_attr_copy->deep_copy(*(data->attr_value));
            }

            auto *ev_data = new Tango::EventData(data->device,data->attr_name, data->event, dev_attr_copy, data->errors);

            auto cmd = std::make_shared<HdbCmdData>(ev_data, ev_data_type);
            hdb_dev->push_thread->push_back_cmd(cmd);
        }
        catch(Tango::DevFailed &e)
        {
            ERROR_STREAM << __func__<<": Event '"<<data->attr_name<<"' NOT FOUND in signal list" << endl;
            hdb_dev->shared->veclock.readerOut();
            return;
        }
    }
    //=============================================================================
    /**
     *	Attribute and Event management
     */
    //=============================================================================
    void ArchiveCB::push_event(Tango::AttrConfEventData *data)
    {
        //DEBUG_STREAM << __func__<<": AttrConfEvent '"<<data->attr_name<<"' id="<<omni_thread::self()->id() << "  Received at " << ctime(&t);
        hdb_dev->fix_tango_host(data->attr_name);	//TODO: why sometimes event arrive without fqdn ??

        //	Check if event is an error event.
        if (data->err)
        {
            INFO_STREAM<< __func__ << ": AttrConfEvent Exception on " << data->attr_name << endl;
            INFO_STREAM << data->errors[0].desc  << endl;
            return;
        }
        hdbpp::HdbEventDataType ev_data_type;
        ev_data_type.attr_name = data->attr_name;
        hdb_dev->shared->veclock.readerIn();
        try
        {
            auto signal = hdb_dev->shared->get_signal(data->attr_name);
        } catch(Tango::DevFailed &e)
        {
            ERROR_STREAM << __func__<<": AttrConfEvent '"<<data->attr_name<<"' NOT FOUND in signal list" << endl;
            hdb_dev->shared->veclock.readerOut();
            return;
        }
        //if attribute stopped, just return
        try
        {
            if(!hdb_dev->shared->is_running(data->attr_name) && !hdb_dev->shared->is_first(data->attr_name))
            {
                hdb_dev->shared->veclock.readerOut();
                return;
            }
        }
        catch(Tango::DevFailed &e)
        {
            WARN_STREAM << __func__ << " AttrConfEvent Unable to check if is_running: " << e.errors[0].desc << "'"<<endl;
        }

        try
        {
            hdb_dev->shared->set_conf_periodic_event(data->attr_name, data->attr_conf->events.arch_event.archive_period);
        }
        catch(Tango::DevFailed &e)
        {
            WARN_STREAM << __func__ << " Unable to set_nok_event: " << e.errors[0].desc << "'"<<endl;
        }
        hdb_dev->shared->veclock.readerOut();

        auto *attr_conf = new Tango::AttributeInfoEx();
        *attr_conf = *(data->attr_conf);

        auto *ev_data = new Tango::AttrConfEventData(data->device,data->attr_name, data->event, attr_conf, data->errors);
        auto cmd = std::make_shared<HdbCmdData>(ev_data, ev_data_type);

        hdb_dev->push_thread->push_back_cmd(cmd);
    }
    //=============================================================================
    //=============================================================================
    void HdbDevice::get_tango_host_and_signal_name(const string &signame, string& tango_host, string& name)
    {
        string::size_type start = signame.find("tango://");
        if (start == string::npos)
        {
            name = signame;
            
            char *env = getenv("TANGO_HOST");
            if (env == nullptr)
                tango_host = "unknown";
            else
                tango_host = env;
        }
        else
        {
            start += tango_prefix_length; //	"tango://" length
            string::size_type end = signame.find('/', start);

            tango_host = signame.substr(start, end-start);
            name = signame.substr(end + 1);
        }
    }
    
    //=============================================================================
    //=============================================================================
    void HdbDevice::fix_tango_host(string &attr)
    {
        std::transform(attr.begin(), attr.end(), attr.begin(), (int(*)(int))tolower);		//transform to lowercase
        
        bool modify = false;
        string facility;
        string attr_name;
        
        string::size_type start = attr.find("tango://");
        //if not fqdn, add TANGO_HOST
        if (start == string::npos)
        {
            //TODO: get from device/class/global property
            char *env = getenv("TANGO_HOST");
            if (env != nullptr)
            {
                facility = env;
                attr_name = attr;
                modify = true;
            }
        }
        else
        {
            get_tango_host_and_signal_name(attr, facility, attr_name);
            modify = true;
        }
        if(modify)
        {
            add_domain(facility);
            attr = string("tango://") + facility + string("/") + attr_name;
        }
    }
    //=============================================================================
    //=============================================================================
#ifndef _MULTI_TANGO_HOST
    void HdbDevice::add_domain(string &attr)
    {
        string::size_type	end1 = attr.find('.');
        if (end1 == string::npos)
        {
            //get host name without tango://
            string::size_type	start = attr.find("tango://");
            if (start == string::npos)
            {
                start = 0;
            }
            else
            {
                start = tango_prefix_length;	//tango:// len
            }
            string::size_type	end2 = attr.find(':', start);

            string th = attr.substr(start, end2);
            string with_domain = attr;

            auto it_domain = domain_map.find(th);
            if(it_domain != domain_map.end())
            {
                with_domain = it_domain->second;
                DEBUG_STREAM << __func__ <<": found domain in map -> " << with_domain<<endl;
                attr = with_domain;
                return;
            }

            struct addrinfo hints{};
            //		hints.ai_family = AF_INET; // use AF_INET6 to force IPv6
            //		hints.ai_flags = AI_CANONNAME|AI_CANONIDN;
            memset(&hints, 0, sizeof hints);
            hints.ai_family = AF_UNSPEC; /*either IPV4 or IPV6*/
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_flags = AI_CANONNAME;
            struct addrinfo *result = nullptr;
            struct addrinfo *rp = nullptr;
            int ret = getaddrinfo(th.c_str(), nullptr, &hints, &result);
            if (ret != 0)
            {
                INFO_STREAM << __func__<< ": getaddrinfo error=" << gai_strerror(ret);
                return;
            }

            for (rp = result; rp != nullptr; rp = rp->ai_next)
            {
                with_domain = string(rp->ai_canonname) + attr.substr(end2);
                DEBUG_STREAM << __func__ <<": found domain -> " << with_domain<<endl;
                domain_map.insert(make_pair(th, with_domain));
            }
            freeaddrinfo(result); // all done with this structure
            attr = with_domain;
        }
    }
    //=============================================================================
    //=============================================================================
    auto HdbDevice::remove_domain(const string &str) -> string
    {
        string::size_type end1 = str.find('.');
        if (end1 == string::npos)
        {
            return str;
        }

        string::size_type start = str.find("tango://");
        if (start == string::npos)
        {
            start = 0;
        }
        else
        {
            start = tango_prefix_length;	//tango:// len
        }
        string::size_type end2 = str.find(':', start);
        if(end1 > end2)	//'.' not in the tango host part
            return str;
        string th = str.substr(0, end1);
        th += str.substr(end2, str.size()-end2);
        return th;
    }
    //=============================================================================
    //=============================================================================
    auto HdbDevice::compare_without_domain(const string &str1, const string &str2) -> bool
    {
        string str1_nd = remove_domain(str1);
        string str2_nd = remove_domain(str2);
        return (str1_nd==str2_nd);
    }
#else
    void HdbDevice::add_domain(string &str)
    {
        string strresult="";
        string facility(str);
        vector<string> facilities;
        if(facility.find(",") == string::npos)
        {
            facilities.push_back(facility);
        }
        else
        {
            string_explode(facility,",", facilities);
        }
        for(vector<string>::iterator it = facilities.begin(); it != facilities.end(); it++)
        {
            string::size_type	end1 = it->find(".");
            if (end1 == string::npos)
            {
                //get host name without tango://
                string::size_type	start = it->find("tango://");
                if (start == string::npos)
                {
                    start = 0;
                }
                else
                {
                    start = 8;	//tango:// len
                }
                string::size_type end2 = it->find(":", start);
                if (end2 == string::npos)
                {
                    strresult += *it;
                    if(it != facilities.end()-1)
                        strresult += ",";
                    continue;
                }
                string th = it->substr(start, end2);
                string port = it->substr(end2);
                string with_domain = *it;

                map<string,string>::iterator it_domain = domain_map.find(th);
                if(it_domain != domain_map.end())
                {
                    with_domain = it_domain->second;
                    //DEBUG_STREAM << __func__ <<": found domain in map -> " << with_domain<<endl;
                    strresult += with_domain+port;
                    if(it != facilities.end()-1)
                        strresult += ",";
                    //DEBUG_STREAM<<__func__<<": strresult 1 "<<strresult<<endl;
                    continue;
                }

                struct addrinfo hints;
                //			hints.ai_family = AF_INET; // use AF_INET6 to force IPv6
                //			hints.ai_flags = AI_CANONNAME|AI_CANONIDN;
                memset(&hints, 0, sizeof hints);
                hints.ai_family = AF_UNSPEC; /*either IPV4 or IPV6*/
                hints.ai_socktype = SOCK_STREAM;
                hints.ai_flags = AI_CANONNAME;
                struct addrinfo *result, *rp;
                int ret = getaddrinfo(th.c_str(), NULL, &hints, &result);
                if (ret != 0)
                {
                    INFO_STREAM << __func__<< ": getaddrinfo error='" << gai_strerror(ret)<<"' while looking for " << th<<endl;
                    strresult += th+port;
                    if(it != facilities.end()-1)
                        strresult += ",";
                    continue;
                }

                for (rp = result; rp != NULL; rp = rp->ai_next)
                {
                    with_domain = string(rp->ai_canonname) + port;
                    domain_map.insert(make_pair(th, string(rp->ai_canonname)));
                }
                freeaddrinfo(result); // all done with this structure
                strresult += with_domain;
                if(it != facilities.end()-1)
                    strresult += ",";
                continue;
            }
            else
            {
                strresult += *it;
                if(it != facilities.end()-1)
                    strresult += ",";
                continue;
            }
        }
        str = strresult;
    }
    string HdbDevice::remove_domain(string str)
    {
        string result="";
        string facility(str);
        vector<string> facilities;
        if(str.find(",") == string::npos)
        {
            facilities.push_back(facility);
        }
        else
        {
            string_explode(facility,",", facilities);
        }
        for(vector<string>::iterator it = facilities.begin(); it != facilities.end(); it++)
        {
            string::size_type	end1 = it->find(".");
            if (end1 == string::npos)
            {
                result += *it;
                if(it != facilities.end()-1)
                    result += ",";
                continue;
            }
            else
            {
                string::size_type	start = it->find("tango://");
                if (start == string::npos)
                {
                    start = 0;
                }
                else
                {
                    start = 8;	//tango:// len
                }
                string::size_type	end2 = it->find(":", start);
                if(end1 > end2)	//'.' not in the tango host part
                {
                    result += *it;
                    if(it != facilities.end()-1)
                        result += ",";
                    continue;
                }
                string th = it->substr(0, end1);
                th += it->substr(end2, it->size()-end2);
                result += th;
                if(it != facilities.end()-1)
                    result += ",";
                continue;
            }
        }
        return result;
    }

    /**
     *	compare 2 tango names considering fqdn, domain, multi tango host
     *	returns 0 if equal
     */
    int HdbDevice::compare_tango_names(string str1, string str2)
    {
        //DEBUG_STREAM << __func__<< ": entering with '" << str1<<"' - '" << str2<<"'" << endl;
        if(str1 == str2)
        {
            //DEBUG_STREAM << __func__<< ": EQUAL 1 -> '" << str1<<"'=='" << str2<<"'" << endl;
            return 0;
        }
        fix_tango_host(str1);
        fix_tango_host(str2);
        if(str1 == str2)
        {
            //DEBUG_STREAM << __func__<< ": EQUAL 2 -> '" << str1<<"'=='" << str2<<"'" << endl;
            return 0;
        }

        string facility1;
        string attr_name1;
        string facility2;
        string attr_name2;
        
        get_tango_host_and_signal_name(str1, facility1, attr_name1);
        get_tango_host_and_signal_name(str2, facility2, attr_name2);
        
        //if attr only part is different -> different
        if(attr_name1 != attr_name2)
            return strcmp(attr_name1.c_str(),attr_name2.c_str());

        //check combination of multiple tango hosts
        vector<string> facilities1;
        string_explode(facility1,",", facilities1);
        vector<string> facilities2;
        string_explode(facility2,",", facilities2);
        for(vector<string>::iterator it1=facilities1.begin(); it1!=facilities1.end(); it1++)
        {
            for(vector<string>::iterator it2=facilities2.begin(); it2!=facilities2.end(); it2++)
            {
                string name1 = string("tango://")+ *it1 + string("/") + attr_name1;
                string name2 = string("tango://")+ *it2 + string("/") + attr_name2;
                //DEBUG_STREAM << __func__<< ": checking all possible combinations: '" << str1<<"' - '" << str2<<"'" << endl;
                if(name1 == name2)
                {
                    //DEBUG_STREAM << __func__<< ": EQUAL 3 -> '" << name1<<"'=='" << name2<<"'" << endl;
                    return 0;
                }
            }
        }

        string str1_nd = remove_domain(str1);
        string str2_nd = remove_domain(str2);
        if(str1_nd == str2_nd)
        {
            //		DEBUG_STREAM << __func__<< ": EQUAL 3 -> '" << str1_nd<<"'=='" << str2_nd<<"'" << endl;
            return 0;
        }
        string facility1_nd;
        string attr_name1_nd;
        string facility2_nd;
        string attr_name2_nd;
        
        get_tango_host_and_signal_name(str1_nd, facility1_nd, attr_name1_nd);
        get_tango_host_and_signal_name(str2_nd, facility2_nd, attr_name2_nd);
        //check combination of multiple tango hosts
        vector<string> facilities1_nd;
        string_explode(facility1_nd,",", facilities1_nd);
        vector<string> facilities2_nd;
        string_explode(facility2_nd,",", facilities2_nd);
        for(vector<string>::iterator it1=facilities1_nd.begin(); it1!=facilities1_nd.end(); it1++)
        {
            for(vector<string>::iterator it2=facilities2_nd.begin(); it2!=facilities2_nd.end(); it2++)
            {
                string name1 = string("tango://")+ *it1 + string("/") + attr_name1;
                string name2 = string("tango://")+ *it2 + string("/") + attr_name2;
                //DEBUG_STREAM << __func__<< ": checking all possible combinations without domain: '" << str1<<"' - '" << str2<<"'" << endl;
                if(name1 == name2)
                {
                    //DEBUG_STREAM << __func__<< ": EQUAL 4 -> '" << name1<<"'=='" << name2<<"'" << endl;
                    return 0;
                }
            }
        }

        int result=strcmp(str1_nd.c_str(),str2_nd.c_str());
        //	DEBUG_STREAM << __func__<< ": DIFFERENTS -> '" << str1_nd<< (result ? "'<'" : "'>'") << str2_nd<<"'" << endl;
        return result;
    }
#endif
    //=============================================================================
    //=============================================================================
    void HdbDevice::string_explode(const string &str, const string &separator, vector<string>& results)
    {
        string::size_type found = 0;
        string::size_type index = 0;

        if(!str.empty())
        {
            do
            {
                found = str.find_first_of(separator, index);
                if(found != string::npos) {
                    results.push_back(str.substr(index, found - index));
                    index = found + 1;
                }
                else
                {
                    results.push_back(str.substr(index));
                }
            }
            while(found != string::npos);
        }
    }

    //=============================================================================
    //=============================================================================
    void HdbDevice::error_attribute(Tango::EventData *data)
    {
        if (data->err)
        {
            INFO_STREAM << "Exception on " << data->attr_name << endl;

            for (unsigned int i=0; i<data->errors.length(); i++)
            {
                INFO_STREAM << data->errors[i].reason << endl;
                INFO_STREAM << data->errors[i].desc << endl;
                INFO_STREAM << data->errors[i].origin << endl;
            }

            INFO_STREAM << endl;
        }
        else
        {
            if ( data->attr_value->get_quality() == Tango::ATTR_INVALID )
            {
                WARN_STREAM << "Invalid data detected for " << data->attr_name << endl;
            }		
        }
    }

    //=============================================================================
    //=============================================================================
    void HdbDevice::storage_time(Tango::EventData *data, double elapsed)
    {

        std::ostringstream str_stream;
        str_stream.precision(3);

        str_stream << elapsed << " ms" << std::endl;;

        std::string fmt_time = str_stream.str();

        INFO_STREAM << "Storage time : " << fmt_time << " for " << data->attr_name << endl;

        if ( elapsed > long_storage_time_threshold )
            ERROR_STREAM << "LONG Storage time : " << fmt_time << " for " << data->attr_name << endl;
    }

    //=============================================================================
    //=============================================================================
}	//	namespace
