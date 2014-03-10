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

namespace HdbEventSubscriber_ns
{

//=============================================================================
//=============================================================================
HdbDevice::~HdbDevice()
{
	DEBUG_STREAM << "	Deleting HdbDevice" << endl;

	DEBUG_STREAM << "	Stopping subscribe thread" << endl;
	shared->stop_thread();
	usleep(50000);
	DEBUG_STREAM << "	Stopping push thread" << endl;
	push_shared->stop_thread();

	thread->join(0);
	DEBUG_STREAM << "	Subscribe thread Stopped " << endl;

	push_thread->join(0);
	DEBUG_STREAM << "	Push thread Stopped " << endl;
	delete shared;
	DEBUG_STREAM << "	shared deleted " << endl;
	delete push_shared;
	DEBUG_STREAM << "	push_shared deleted " << endl;
}
//=============================================================================
//=============================================================================
HdbDevice::HdbDevice(int p, Tango::DeviceImpl *device)
				:Tango::LogAdapter(device)
{
	this->period = p;
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

	build_signal_vector(list);

	//	Create thread to send commands to HdbAccess device
	push_shared = new PushThreadShared(
			(static_cast<HdbEventSubscriber *>(_device))->dbHost,
			(static_cast<HdbEventSubscriber *>(_device))->dbUser,
			(static_cast<HdbEventSubscriber *>(_device))->dbPassword,
			(static_cast<HdbEventSubscriber *>(_device))->dbName,
			(static_cast<HdbEventSubscriber *>(_device))->dbPort);


	push_thread = new PushThread(push_shared);
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
					shared->start(list[i]);
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
	shared->add(signame, UPDATE_PROP);
}
//=============================================================================
//=============================================================================
void HdbDevice::remove(string &signame)
{
	shared->remove(signame);
	push_shared->remove(signame);
}
//=============================================================================
//=============================================================================
vector<string> HdbDevice::get_hdb_signal_list()
{
	vector<string>	list;
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
		dev_prop[0]  >>  list;

	for (unsigned int i=0 ; i<list.size() ; i++)
	{
		fix_tango_host(list[i]);
		std::transform(list[i].begin(), list[i].end(), list[i].begin(), (int(*)(int))tolower);		//transform to lowercase
		cout << i << ":	" << list[i] << endl;
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
		cout << ELAPSED(t0, t1) << " ms" << endl;
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
	Tango::DevState	state =  shared->state();
	return state;
}
//=============================================================================
//=============================================================================
vector<string>  HdbDevice::get_sig_on_error_list()
{
	return shared->get_sig_on_error_list();
}
//=============================================================================
//=============================================================================
vector<string>  HdbDevice::get_sig_not_on_error_list()
{
	return shared->get_sig_not_on_error_list();
}
//=============================================================================
//=============================================================================
int  HdbDevice::get_sig_on_error_num()
{
	return shared->get_sig_on_error_num();
}
//=============================================================================
//=============================================================================
int  HdbDevice::get_sig_not_on_error_num()
{
	return shared->get_sig_not_on_error_num();
}
//=============================================================================
//=============================================================================
string  HdbDevice::get_sig_status(string &signame)
{
	return shared->get_sig_status(signame);
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






//=============================================================================
/**
 *	Attribute and Event management
 */
//=============================================================================
void ArchiveCB::push_event(Tango::EventData *data)
{

	time_t	t = time(NULL);
	//cout << __func__<<": Event '"<<data->attr_name<<"'  Received at " << ctime(&t);
	hdb_dev->fix_tango_host(data->attr_name);	//TODO: why sometimes event arrive without fqdn ??
	//	Check if event is an error event.
	if (data->err)
	{
		signal->state  = Tango::ALARM;
		signal->status.clear();
		signal->status = data->errors[0].desc;

		cout<< "Exception on " << data->attr_name << endl;
		cout << data->errors[0].desc  << endl;
		try
		{
			hdb_dev->shared->set_nok_event(data->attr_name);
		}
		catch(Tango::DevFailed &e)
		{
			cout << __func__ << " Unable to set_nok_event: " << e.errors[0].desc << "'"<<endl;
		}

		hdb_dev->error_attribute(data);
	}
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
		hdb_dev->error_attribute(data);
		//	Check if already OK
		if (signal->state!=Tango::ON)
		{
			signal->state  = Tango::ON;
			signal->status = "Subscribed";
		}
	}
	else
	{
		try
		{
			hdb_dev->shared->set_ok_event(data->attr_name);
		}
		catch(Tango::DevFailed &e)
		{
			cout << __func__ << " Unable to set_ok_event: " << e.errors[0].desc << "'"<<endl;
		}
		//	Check if already OK
		if (signal->state!=Tango::ON)
		{
			signal->state  = Tango::ON;
			signal->status = "Subscribed";
		}

		//if attribute stopped, just return
		try
		{
			if(!hdb_dev->shared->is_running(data->attr_name) && !hdb_dev->shared->is_first(data->attr_name))
				return;
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

		//OK with C++11:
		//Tango::EventData	*cmd = new Tango::EventData(*data);
		//OK with C++98:
		Tango::DeviceAttribute *dev_attr_copy = new Tango::DeviceAttribute();
		dev_attr_copy->deep_copy(*(data->attr_value));
		Tango::EventData	*cmd = new Tango::EventData(data->device,data->attr_name, data->event, dev_attr_copy, data->errors);

		hdb_dev->push_shared->push_back_cmd(cmd);
	}
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
	string::size_type	start = attr.find("tango://");
	//if not fqdn, add TANGO_HOST
	if (start == string::npos)
	{
		//TODO: get from device/class/global property
		char	*env = getenv("TANGO_HOST");
		if (env==NULL)
			return;
		else
		{
			string	s(env);
			attr = string("tango://") + s + "/" + attr;
			return;
		}
	}
}
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
