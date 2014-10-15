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

namespace HdbEventSubscriber_ns
{

//=============================================================================
//=============================================================================
HdbDevice::~HdbDevice()
{
	DEBUG_STREAM << "	Deleting HdbDevice" << endl;
	DEBUG_STREAM << "	Stopping stats thread" << endl;
	stats_thread->abortflag=true;
	DEBUG_STREAM << "	Stopping subscribe thread" << endl;
	shared->stop_thread();
	DEBUG_STREAM << "	Subscribe thread Stopped " << endl;
	thread->join(0);
	DEBUG_STREAM << "	Subscribe thread Joined " << endl;
	usleep(100000);
	DEBUG_STREAM << "	Stopping push thread" << endl;
	push_shared->stop_thread();
	DEBUG_STREAM << "	Push thread Stopped " << endl;
	push_thread->join(0);
	DEBUG_STREAM << "	Push thread Joined " << endl;
	stats_thread->join(0);
	DEBUG_STREAM << "	Stats thread Joined " << endl;
	delete shared;
	DEBUG_STREAM << "	shared deleted " << endl;
	delete push_shared;
	DEBUG_STREAM << "	push_shared deleted " << endl;
}
//=============================================================================
//=============================================================================
HdbDevice::HdbDevice(int p, int s, Tango::DeviceImpl *device)
				:Tango::LogAdapter(device)
{
	this->period = p;
	this->stats_window = s;
	_device = device;
#ifdef _USE_FERMI_DB_RW
	host_rw = "";
	Tango::Database *db = new Tango::Database();
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
	string server = "alarm-srv/test";
	Tango::DbServerInfo info = db->get_server_info(server);
	INFO_STREAM << " INFO: host=" << info.host;

	delete db;
#endif
}
//=============================================================================
//=============================================================================
void HdbDevice::initialize()
{
	vector<string>	list = get_hdb_signal_list();

	//	Create a thread to subscribe events
	shared = new SharedData(this);	
	thread = new SubscribeThread(this);

	//	Create thread to send commands to HdbAccess device
	push_shared = new PushThreadShared(this,
			(static_cast<HdbEventSubscriber *>(_device))->dbHost,
			(static_cast<HdbEventSubscriber *>(_device))->dbUser,
			(static_cast<HdbEventSubscriber *>(_device))->dbPassword,
			(static_cast<HdbEventSubscriber *>(_device))->dbName,
			(static_cast<HdbEventSubscriber *>(_device))->dbPort);


	push_thread = new PushThread(push_shared);
	stats_thread = new StatsThread(this);
	stats_thread->period = stats_window;

	build_signal_vector(list);

	stats_thread->start();
	push_thread->start();
	thread->start();

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
void HdbDevice::build_signal_vector(vector<string> list)
{
	for (unsigned int i=0 ; i<list.size() ; i++)
	{
		try
		{
			if (list[i].length()>0)
			{
				shared->add(list[i]);
				if(startArchivingAtStartup)
				{
					push_shared->start_attr(list[i]);
					shared->start(list[i]);
				}
			}
		}
		catch (Tango::DevFailed &e)
		{
			Tango::Except::print_exception(e);
			cout << "!!! Do not add " << list[i] << endl;
		}	
	}
}
//=============================================================================
//=============================================================================
void HdbDevice::add(string &signame)
{
	fix_tango_host(signame);
	shared->add(signame, UPDATE_PROP);
}
//=============================================================================
//=============================================================================
void HdbDevice::remove(string &signame)
{
	fix_tango_host(signame);
	shared->remove(signame);
	push_shared->remove(signame);
	push_shared->remove_attr(signame);
}
//=============================================================================
//=============================================================================
vector<string> HdbDevice::get_hdb_signal_list()
{
	vector<string>	list;
	vector<string>	tmplist;
	//	Read device properties from database.
	//-------------------------------------------------------------
	Tango::DbData	dev_prop;
	dev_prop.push_back(Tango::DbDatum("AttributeList"));

	//	Call database and extract values
	//--------------------------------------------
	//_device->get_property(dev_prop);
	Tango::Database *db = new Tango::Database();
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
		delete db;

	//	Extract value
	if (dev_prop[0].is_empty()==false)
		dev_prop[0]  >>  tmplist;

	for (unsigned int i=0 ; i<tmplist.size() ; i++)
	{
		if(tmplist[i].length() > 0 && tmplist[i][0] != '#')
		{
			fix_tango_host(tmplist[i]);
			list.push_back(tmplist[i]);
			DEBUG_STREAM << i << ":	" << tmplist[i][i] << endl;
		}
	}
	return list;
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
cout << ELAPSED(t0, t1) << " ms" << endl;
	}
	catch (Tango::DevFailed &e)
	{
		Tango::Except::print_exception(e);
	}
#endif

	Tango::DbData	data;
	data.push_back(Tango::DbDatum("AttributeList"));
	data[0]  <<  prop;
#ifndef _USE_FERMI_DB_RW
	Tango::Database *db = new Tango::Database();
#else
	//save properties using host_rw e port_rw to connect to database
	Tango::Database *db;
	if(host_rw != "")
		db = new Tango::Database(host_rw,port_rw);
	else
		db = new Tango::Database();
	DEBUG_STREAM << __func__<<": connecting to db "<<host_rw<<":"<<port_rw;
#endif
	try
	{
		DECLARE_TIME_VAR	t0, t1;
		GET_TIME(t0);
		db->set_timeout_millis(10000);
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
	delete db;
}
//=============================================================================
//=============================================================================
vector<string>  HdbDevice::get_sig_list()
{
	return shared->get_sig_list();
}
//=============================================================================
//=============================================================================
Tango::DevState  HdbDevice::subcribing_state()
{
/*
	Tango::DevState	state = DeviceProxy::state();	//	Get Default state
	if (state==Tango::ON)
		state = shared->state();				//	If OK get signals state
*/
	Tango::DevState	evstate =  shared->state();
	Tango::DevState	dbstate =  push_shared->state();
	if(evstate == Tango::ALARM || dbstate == Tango::ALARM)
		return Tango::ALARM;
	return Tango::ON;
}
//=============================================================================
//=============================================================================
vector<string>  HdbDevice::get_sig_on_error_list()
{
	vector<string> sig_list = shared->get_sig_on_error_list();
	vector<string> other_list = shared->get_sig_not_on_error_list();
	//check if signal not in event error is in db error
	for(vector<string>::iterator it=other_list.begin(); it!=other_list.end(); it++)
	{
		if(push_shared->get_sig_state(*it) == Tango::ALARM)
		{
			sig_list.push_back(*it);
		}
	}
	return sig_list;
}
//=============================================================================
//=============================================================================
vector<string>  HdbDevice::get_sig_not_on_error_list()
{
	vector<string> sig_list = shared->get_sig_not_on_error_list();
	vector<string> ret_sig_list;
	//check if signal not in event error is in db error
	for(vector<string>::iterator it=sig_list.begin(); it!=sig_list.end(); it++)
	{
		string sig(*it);
		if(push_shared->get_sig_state(sig) != Tango::ALARM)
		{
			ret_sig_list.push_back(sig);
		}
	}
	return ret_sig_list;
}
//=============================================================================
//=============================================================================
vector<string>  HdbDevice::get_sig_started_list()
{
	return shared->get_sig_started_list();
}
//=============================================================================
//=============================================================================
vector<string>  HdbDevice::get_sig_not_started_list()
{
	return shared->get_sig_not_started_list();
}
//=============================================================================
//=============================================================================
vector<string>  HdbDevice::get_error_list()
{
	vector<string> sig_list = shared->get_sig_list();
	vector<string> error_list = shared->get_error_list();
	vector<string> other_list = shared->get_sig_not_on_error_list();
	//check if signal not in event error is in db error
	for(vector<string>::iterator it=other_list.begin(); it!=other_list.end(); it++)
	{
		//looking for DB errors
		if(push_shared->get_sig_state(*it) == Tango::ALARM)
		{
			//find *it in sig_list and replace string in error_list with "DB error"
			vector<string>::iterator itsig_list = find(sig_list.begin(), sig_list.end(), *it);
			size_t idx = itsig_list - sig_list.begin();
			if(idx < error_list.size())
				error_list[idx] = STATUS_DB_ERROR;
		}
	}
	return error_list;
}
//=============================================================================
//=============================================================================
void  HdbDevice::get_event_number_list()
{
	vector<string> attribute_list_tmp = get_sig_list();
	for (size_t i=0 ; i<attribute_list_tmp.size() ; i++)
	{
		string signame(attribute_list_tmp[i]);
		try
		{
			shared->lock();
			bool is_running = shared->is_running(signame);
			shared->unlock();
			if(!is_running)
				continue;
		}catch(Tango::DevFailed &e)
		{
			continue;
		}
		long ok_ev_t=0;
		long nok_ev_t=0;
		ok_ev_t = shared->get_ok_event(signame);
		nok_ev_t = shared->get_nok_event(signame);
		AttributeEventNumberList[i] = ok_ev_t + nok_ev_t;
	}
}
//=============================================================================
//=============================================================================
int  HdbDevice::get_sig_on_error_num()
{
	int on_ev_err = shared->get_sig_on_error_num();

	vector<string> other_list = shared->get_sig_not_on_error_list();
	//check if signal not in event error is in db error
	for(vector<string>::iterator it=other_list.begin(); it!=other_list.end(); it++)
	{
		if(push_shared->get_sig_state(*it) == Tango::ALARM)
		{
			on_ev_err++;
		}
	}
	return on_ev_err;
}
//=============================================================================
//=============================================================================
int  HdbDevice::get_sig_not_on_error_num()
{
	int not_on_ev_err = shared->get_sig_not_on_error_num();

	vector<string> sig_list = shared->get_sig_not_on_error_list();
	//check if signal not in event error is in db error
	for(vector<string>::iterator it=sig_list.begin(); it!=sig_list.end(); it++)
	{
		if(push_shared->get_sig_state(*it) == Tango::ALARM)
		{
			not_on_ev_err--;
		}
	}
	return not_on_ev_err;
}
//=============================================================================
//=============================================================================
int  HdbDevice::get_sig_started_num()
{
	return shared->get_sig_started_num();
}
//=============================================================================
//=============================================================================
int  HdbDevice::get_sig_not_started_num()
{
	return shared->get_sig_not_started_num();
}
//=============================================================================
//=============================================================================
string  HdbDevice::get_sig_status(string &signame)
{
	string ev_status = shared->get_sig_status(signame);

	//looking for DB errors
	if(push_shared->get_sig_state(signame) == Tango::ALARM)
	{
		ev_status = STATUS_DB_ERROR;
	}

	return ev_status;
}
//=============================================================================
//=============================================================================
int HdbDevice::get_max_waiting()
{
	return push_shared->get_max_waiting();
}
//=============================================================================
//=============================================================================
int HdbDevice::nb_cmd_waiting()
{
	return push_shared->nb_cmd_waiting();
}
//=============================================================================
//=============================================================================
vector<string> HdbDevice::get_sig_list_waiting()
{
	return push_shared->get_sig_list_waiting();
}
//=============================================================================
//=============================================================================
void  HdbDevice::reset_statistics()
{
	shared->reset_statistics();
	push_shared->reset_statistics();
}
//=============================================================================
//=============================================================================
void  HdbDevice::reset_freq_statistics()
{
	shared->reset_freq_statistics();
	push_shared->reset_freq_statistics();
}
//=============================================================================
//=============================================================================





//=============================================================================
/**
 *	Attribute and Event management
 */
//=============================================================================
void ArchiveCB::push_event(Tango::EventData *data)
{

	time_t	t = time(NULL);
	//cout << __func__<<": Event '"<<data->attr_name<<" '  Received at " << ctime(&t);
	hdb_dev->fix_tango_host(data->attr_name);	//TODO: why sometimes event arrive without fqdn ??

	hdb_dev->shared->lock();
	HdbSignal	*signal=hdb_dev->shared->get_signal(data->attr_name);

	if(signal==NULL)
	{
		cout << __func__<<": Event '"<<data->attr_name<<"' NOT FOUND in signal list" << endl;
		hdb_dev->shared->unlock();
		return;
	}
	HdbEventDataType ev_data_type;
	ev_data_type.attr_name = data->attr_name;
	if(!hdb_dev->shared->is_first(data->attr_name))
	{
		ev_data_type.max_dim_x = signal->max_dim_x;
		ev_data_type.max_dim_y = signal->max_dim_y;
		ev_data_type.data_type = signal->data_type;
		ev_data_type.data_format = signal->data_format;
		ev_data_type.write_type	= signal->write_type;
	}
	else
	{
		try
		{
			Tango::AttributeInfo	info = signal->attr->get_config();
			ev_data_type.data_type = info.data_type;
			ev_data_type.data_format = info.data_format;
			ev_data_type.write_type = info.writable;
			ev_data_type.max_dim_x = info.max_dim_x;
			ev_data_type.max_dim_y = info.max_dim_y;
		}
		catch (Tango::DevFailed &e)
		{
			cout<< __func__ << ": FIRST exception in get_config: " << data->attr_name <<" ev_data_type.data_type="<<ev_data_type.data_type<<" err="<<e.errors[0].desc<< endl;
			hdb_dev->shared->unlock();
			return;
		}
	}

	//	Check if event is an error event.
	if (data->err)
	{
		signal->evstate  = Tango::ALARM;
		signal->status.clear();
		signal->status = data->errors[0].desc;

		cout<< __func__ << ": Exception on " << data->attr_name << endl;
		cout << data->errors[0].desc  << endl;
		try
		{
			hdb_dev->shared->set_nok_event(data->attr_name);
		}
		catch(Tango::DevFailed &e)
		{
			cout << __func__ << " Unable to set_nok_event: " << e.errors[0].desc << "'"<<endl;
		}

		try
		{
			if(!(hdb_dev->shared->is_running(data->attr_name) && hdb_dev->shared->is_first_err(data->attr_name)))
			{
				hdb_dev->shared->unlock();
				return;
			}
		}
		catch(Tango::DevFailed &e)
		{
			cout << __func__ << " Unable to check if is_running: " << e.errors[0].desc << "'"<<endl;
		}
		try
		{
			hdb_dev->shared->set_first_err(data->attr_name);
		}
		catch(Tango::DevFailed &e)
		{
			cout << __func__ << " Unable to set first err: " << e.errors[0].desc << "'"<<endl;
		}
	}
#if 0	//storing quality factor
	else if ( data->attr_value->get_quality() == Tango::ATTR_INVALID )
	{
		cout << "Attribute " << data->attr_name << " is invalid !" << endl;
		try
		{
			hdb_dev->shared->set_nok_event(data->attr_name);
		}
		catch(Tango::DevFailed &e)
		{
			cout << __func__ << " Unable to set_nok_event: " << e.errors[0].desc << "'"<<endl;
		}
//		hdb_dev->error_attribute(data);
		//	Check if already OK
		if (signal->evstate!=Tango::ON)
		{
			signal->evstate  = Tango::ON;
			signal->status = STATUS_SUBSCRIBED;
		}
		try
		{
			if(!(hdb_dev->shared->is_running(data->attr_name) && hdb_dev->shared->is_first_err(data->attr_name)))
			{
				hdb_dev->shared->unlock();
				return;
			}
		}
		catch(Tango::DevFailed &e)
		{
			cout << __func__ << " Unable to check if is_running: " << e.errors[0].desc << "'"<<endl;
		}
		try
		{
			hdb_dev->shared->set_first_err(data->attr_name);
		}
		catch(Tango::DevFailed &e)
		{
			cout << __func__ << " Unable to set first err: " << e.errors[0].desc << "'"<<endl;
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
			cout << __func__ << " Unable to set_ok_event: " << e.errors[0].desc << "'"<<endl;
		}
		//	Check if already OK
		if (signal->evstate!=Tango::ON)
		{
			signal->evstate  = Tango::ON;
			signal->status = "Subscribed";
		}

		//if attribute stopped, just return
		try
		{
			if(!hdb_dev->shared->is_running(data->attr_name) && !hdb_dev->shared->is_first(data->attr_name))
			{
				hdb_dev->shared->unlock();
				return;
			}
		}
		catch(Tango::DevFailed &e)
		{
			cout << __func__ << " Unable to check if is_running: " << e.errors[0].desc << "'"<<endl;
		}
		try
		{
			if(hdb_dev->shared->is_first(data->attr_name))
				hdb_dev->shared->set_first(data->attr_name);
		}
		catch(Tango::DevFailed &e)
		{
			cout << __func__ << " Unable to set first: " << e.errors[0].desc << "'"<<endl;
		}
	}

	hdb_dev->shared->unlock();

	//OK only with C++11:
	//Tango::EventData	*cmd = new Tango::EventData(*data);
	//OK with C++98 and C++11:
	Tango::DeviceAttribute *dev_attr_copy = new Tango::DeviceAttribute();
	if (!data->err)
	{
		dev_attr_copy->deep_copy(*(data->attr_value));
	}

	Tango::EventData	*ev_data = new Tango::EventData(data->device,data->attr_name, data->event, dev_attr_copy, data->errors);

	HdbCmdData *cmd = new HdbCmdData(ev_data, ev_data_type);
	hdb_dev->push_shared->push_back_cmd(cmd);
}
//=============================================================================
//=============================================================================
string HdbDevice::get_only_signal_name(string str)
{
	string::size_type	start = str.find("tango://");
	if (start == string::npos)
		return str;
	else
	{
		start += 8; //	"tango://" length
		start = str.find('/', start);
		start++;
		string	signame = str.substr(start);
		return signame;
	}
}
//=============================================================================
//=============================================================================
string HdbDevice::get_only_tango_host(string str)
{
	string::size_type	start = str.find("tango://");
	if (start == string::npos)
	{
		char	*env = getenv("TANGO_HOST");
		if (env==NULL)
			return "unknown";
		else
		{
			string	s(env);
			return s;
		}
	}
	else
	{
		start += 8; //	"tango://" length
		string::size_type	end = str.find('/', start);
		string	th = str.substr(start, end-start);
		return th;
	}
}
//=============================================================================
//=============================================================================
void HdbDevice::fix_tango_host(string &attr)
{
	std::transform(attr.begin(), attr.end(), attr.begin(), (int(*)(int))tolower);		//transform to lowercase
	string::size_type	start = attr.find("tango://");
	//if not fqdn, add TANGO_HOST
	if (start == string::npos)
	{
		//TODO: get from device/class/global property
		char	*env = getenv("TANGO_HOST");
		if (env==NULL)
		{
			return;
		}
		else
		{
			string	s(env);
			add_domain(s);
			attr = string("tango://") + s + "/" + attr;
			return;
		}
	}
	string facility = get_only_tango_host(attr);
	add_domain(facility);
	string attr_name = get_only_signal_name(attr);
	attr = string("tango://")+ facility + string("/") + attr_name;
}
//=============================================================================
//=============================================================================
#ifndef _MULTI_TANGO_HOST
void HdbDevice::add_domain(string &str)
{
	string::size_type	end1 = str.find(".");
	if (end1 == string::npos)
	{
		//get host name without tango://
		string::size_type	start = str.find("tango://");
		if (start == string::npos)
		{
			start = 0;
		}
		else
		{
			start = 8;	//tango:// len
		}
		string::size_type	end2 = str.find(":", start);

		string th = str.substr(start, end2);
		string with_domain = str;

		map<string,string>::iterator it_domain = domain_map.find(th);
		if(it_domain != domain_map.end())
		{
			with_domain = it_domain->second;
			DEBUG_STREAM << __func__ <<": found domain in map -> " << with_domain<<endl;
			str = with_domain;
			return;
		}

		struct addrinfo hints;
//		hints.ai_family = AF_INET; // use AF_INET6 to force IPv6
//		hints.ai_flags = AI_CANONNAME|AI_CANONIDN;
		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_UNSPEC; /*either IPV4 or IPV6*/
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_CANONNAME;
		struct addrinfo *result, *rp;
		int ret = getaddrinfo(th.c_str(), NULL, &hints, &result);
		if (ret != 0)
		{
			INFO_STREAM << __func__<< ": getaddrinfo error=" << gai_strerror(ret);
			return;
		}

		for (rp = result; rp != NULL; rp = rp->ai_next)
		{
			with_domain = string(rp->ai_canonname) + str.substr(end2);
			DEBUG_STREAM << __func__ <<": found domain -> " << with_domain<<endl;
			domain_map.insert(make_pair(th, with_domain));
		}
		freeaddrinfo(result); // all done with this structure
		str = with_domain;
		return;
	}
	else
	{
		return;
	}
}
//=============================================================================
//=============================================================================
string HdbDevice::remove_domain(string str)
{
	string::size_type	end1 = str.find(".");
	if (end1 == string::npos)
	{
		return str;
	}
	else
	{
		string::size_type	start = str.find("tango://");
		if (start == string::npos)
		{
			start = 0;
		}
		else
		{
			start = 8;	//tango:// len
		}
		string::size_type	end2 = str.find(":", start);
		if(end1 > end2)	//'.' not in the tango host part
			return str;
		string th = str.substr(0, end1);
		th += str.substr(end2, str.size()-end2);
		return th;
	}
}
//=============================================================================
//=============================================================================
bool HdbDevice::compare_without_domain(string str1, string str2)
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
		string_explode(facility,",",&facilities);
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
				//cout << __func__ <<": found domain in map -> " << with_domain<<endl;
				strresult += with_domain+port;
				if(it != facilities.end()-1)
					strresult += ",";
				//cout<<__func__<<": strresult 1 "<<strresult<<endl;
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
		string_explode(facility,",",&facilities);
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
	//cout << __func__<< ": entering with '" << str1<<"' - '" << str2<<"'" << endl;
	if(str1 == str2)
	{
		//cout << __func__<< ": EQUAL 1 -> '" << str1<<"'=='" << str2<<"'" << endl;
		return 0;
	}
	fix_tango_host(str1);
	fix_tango_host(str2);
	if(str1 == str2)
	{
		//cout << __func__<< ": EQUAL 2 -> '" << str1<<"'=='" << str2<<"'" << endl;
		return 0;
	}

	string facility1 = get_only_tango_host(str1);
	string attr_name1 = get_only_signal_name(str1);
	string facility2 = get_only_tango_host(str2);
	string attr_name2 = get_only_signal_name(str2);
	//if attr only part is different -> different
	if(attr_name1 != attr_name2)
		return strcmp(attr_name1.c_str(),attr_name2.c_str());

	//check combination of multiple tango hosts
	vector<string> facilities1;
	string_explode(facility1,",",&facilities1);
	vector<string> facilities2;
	string_explode(facility2,",",&facilities2);
	for(vector<string>::iterator it1=facilities1.begin(); it1!=facilities1.end(); it1++)
	{
		for(vector<string>::iterator it2=facilities2.begin(); it2!=facilities2.end(); it2++)
		{
			string name1 = string("tango://")+ *it1 + string("/") + attr_name1;
			string name2 = string("tango://")+ *it2 + string("/") + attr_name2;
			//cout << __func__<< ": checking all possible combinations: '" << str1<<"' - '" << str2<<"'" << endl;
			if(name1 == name2)
			{
				//cout << __func__<< ": EQUAL 3 -> '" << name1<<"'=='" << name2<<"'" << endl;
				return 0;
			}
		}
	}

	string str1_nd = remove_domain(str1);
	string str2_nd = remove_domain(str2);
	if(str1_nd == str2_nd)
	{
//		cout << __func__<< ": EQUAL 3 -> '" << str1_nd<<"'=='" << str2_nd<<"'" << endl;
		return 0;
	}
	string facility1_nd = get_only_tango_host(str1_nd);
	string attr_name1_nd = get_only_signal_name(str1_nd);
	string facility2_nd = get_only_tango_host(str2_nd);
	string attr_name2_nd = get_only_signal_name(str2_nd);
	//check combination of multiple tango hosts
	vector<string> facilities1_nd;
	string_explode(facility1_nd,",",&facilities1_nd);
	vector<string> facilities2_nd;
	string_explode(facility2_nd,",",&facilities2_nd);
	for(vector<string>::iterator it1=facilities1_nd.begin(); it1!=facilities1_nd.end(); it1++)
	{
		for(vector<string>::iterator it2=facilities2_nd.begin(); it2!=facilities2_nd.end(); it2++)
		{
			string name1 = string("tango://")+ *it1 + string("/") + attr_name1;
			string name2 = string("tango://")+ *it2 + string("/") + attr_name2;
			//cout << __func__<< ": checking all possible combinations without domain: '" << str1<<"' - '" << str2<<"'" << endl;
			if(name1 == name2)
			{
				//cout << __func__<< ": EQUAL 4 -> '" << name1<<"'=='" << name2<<"'" << endl;
				return 0;
			}
		}
	}

	int result=strcmp(str1_nd.c_str(),str2_nd.c_str());
//	cout << __func__<< ": DIFFERENTS -> '" << str1_nd<< (result ? "'<'" : "'>'") << str2_nd<<"'" << endl;
	return result;
}
//=============================================================================
//=============================================================================
void HdbDevice::string_explode(string str, string separator, vector<string>* results)
{
	string::size_type found;

	found = str.find_first_of(separator);
	while(found != string::npos) {
		if(found > 0) {
			results->push_back(str.substr(0,found));
		}
		str = str.substr(found+1);
		found = str.find_first_of(separator);
	}
	if(str.length() > 0) {
		results->push_back(str);
	}

}
#endif
//=============================================================================
//=============================================================================
void HdbDevice::error_attribute(Tango::EventData *data)
{
	if (data->err)
	{
		ERROR_STREAM << "Exception on " << data->attr_name << endl;
	
		for (unsigned int i=0; i<data->errors.length(); i++)
		{
			ERROR_STREAM << data->errors[i].reason << endl;
			ERROR_STREAM << data->errors[i].desc << endl;
			ERROR_STREAM << data->errors[i].origin << endl;
		}
			
		ERROR_STREAM << endl;		
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
	char el_time[80];
	char *el_ptr = el_time;
	sprintf (el_ptr, "%.3f ms", elapsed);
	
	WARN_STREAM << "Storage time : " << el_time << " for " << data->attr_name << endl;

	if ( elapsed > 50 )
		ERROR_STREAM << "LONG Storage time : " << el_time << " for " << data->attr_name << endl;
}
	
//=============================================================================
//=============================================================================
}	//	namespace
