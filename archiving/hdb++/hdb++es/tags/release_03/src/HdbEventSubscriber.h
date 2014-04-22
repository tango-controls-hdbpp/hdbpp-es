/*----- PROTECTED REGION ID(HdbEventSubscriber.h) ENABLED START -----*/
//=============================================================================
//
// file :        HdbEventSubscriber.h
//
// description : Include for the HdbEventSubscriber class.
//
// project :     Tango Device Server.
//
// $Author: graziano $
//
// $Revision: 1.6 $
// $Date: 2014-03-07 14:05:54 $
//
// SVN only:
// $HeadURL$
//
// CVS only:
// $Source: /home/cvsadm/cvsroot/fermi/servers/hdb++/hdb++es/src/HdbEventSubscriber.h,v $
// $Log: HdbEventSubscriber.h,v $
// Revision 1.6  2014-03-07 14:05:54  graziano
// added ResetStatistics command
//
// Revision 1.5  2014-02-20 15:16:16  graziano
// name and path fixing
// regenerated with new pogo
// added StartArchivingAtStartup property
//
// Revision 1.4  2013-09-02 12:17:35  graziano
// cleanings
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
//=============================================================================
//                This file is generated by POGO
//        (Program Obviously used to Generate tango Object)
//=============================================================================


#ifndef HDBEVENTSUBSCRIBER_H
#define HDBEVENTSUBSCRIBER_H


#include <tango.h>
#include <HdbDevice.h>


#define HISTO_MAX_SIZE	1024	//	attribute maximum size



/*----- PROTECTED REGION END -----*/	//	HdbEventSubscriber.h

/**
 *  HdbEventSubscriber class description:
 *    This class is able to subscribe on archive events and store value in Historical DB
 */

namespace HdbEventSubscriber_ns
{
/*----- PROTECTED REGION ID(HdbEventSubscriber::Additional Class Declarations) ENABLED START -----*/

		//		Additional Class Declarations

	/*----- PROTECTED REGION END -----*/	//	HdbEventSubscriber::Additional Class Declarations

class HdbEventSubscriber : public TANGO_BASE_CLASS
{

/*----- PROTECTED REGION ID(HdbEventSubscriber::Data Members) ENABLED START -----*/

	//		Add your own data members
public:
	HdbDevice	*hdb_dev;
	bool		initialized;



private:
	vector<string> attribute_list_str;
	vector<string> attribute_ok_list_str;
	vector<string> attribute_nok_list_str;
	vector<string> attribute_pending_list_str;

#ifdef _USE_FERMI_DB_RW
private:
	string host_rw;
	long port_rw;
#endif


	/*----- PROTECTED REGION END -----*/	//	HdbEventSubscriber::Data Members

//	Device property data members
public:
	//	SubscribeRetryPeriod:	Subscribe event retrying period in seconds.
	Tango::DevLong	subscribeRetryPeriod;
	//	AttributeList:	List of configured attributes.
	vector<string>	attributeList;
	//	DbHost:	
	string	dbHost;
	//	DbUser:	
	string	dbUser;
	//	DbPassword:	
	string	dbPassword;
	//	DbName:	
	string	dbName;
	//	DbPort:	
	Tango::DevShort	dbPort;
	//	StartArchivingAtStartup:	Start archiving at startup
	Tango::DevBoolean	startArchivingAtStartup;

//	Attribute data members
public:
	Tango::DevLong	*attr_AttributeOkNumber_read;
	Tango::DevLong	*attr_AttributeNokNumber_read;
	Tango::DevLong	*attr_AttributePendingNumber_read;
	Tango::DevLong	*attr_AttributeNumber_read;
	Tango::DevString	*attr_AttributeList_read;
	Tango::DevString	*attr_AttributeOkList_read;
	Tango::DevString	*attr_AttributeNokList_read;
	Tango::DevString	*attr_AttributePendingList_read;

//	Constructors and destructors
public:
	/**
	 * Constructs a newly device object.
	 *
	 *	@param cl	Class.
	 *	@param s 	Device Name
	 */
	HdbEventSubscriber(Tango::DeviceClass *cl,string &s);
	/**
	 * Constructs a newly device object.
	 *
	 *	@param cl	Class.
	 *	@param s 	Device Name
	 */
	HdbEventSubscriber(Tango::DeviceClass *cl,const char *s);
	/**
	 * Constructs a newly device object.
	 *
	 *	@param cl	Class.
	 *	@param s 	Device name
	 *	@param d	Device description.
	 */
	HdbEventSubscriber(Tango::DeviceClass *cl,const char *s,const char *d);
	/**
	 * The device object destructor.
	 */	
	~HdbEventSubscriber() {delete_device();};


//	Miscellaneous methods
public:
	/*
	 *	will be called at device destruction or at init command.
	 */
	void delete_device();
	/*
	 *	Initialize the device
	 */
	virtual void init_device();
	/*
	 *	Read the device properties from database
	 */
	void get_device_property();
	/*
	 *	Always executed method before execution command method.
	 */
	virtual void always_executed_hook();


//	Attribute methods
public:
	//--------------------------------------------------------
	/*
	 *	Method      : HdbEventSubscriber::read_attr_hardware()
	 *	Description : Hardware acquisition for attributes.
	 */
	//--------------------------------------------------------
	virtual void read_attr_hardware(vector<long> &attr_list);

/**
 *	Attribute AttributeOkNumber related methods
 *	Description: Number of archived attributes not in error
 *
 *	Data type:	Tango::DevLong
 *	Attr type:	Scalar
 */
	virtual void read_AttributeOkNumber(Tango::Attribute &attr);
	virtual bool is_AttributeOkNumber_allowed(Tango::AttReqType type);
/**
 *	Attribute AttributeNokNumber related methods
 *	Description: Number of archived attributes in error
 *
 *	Data type:	Tango::DevLong
 *	Attr type:	Scalar
 */
	virtual void read_AttributeNokNumber(Tango::Attribute &attr);
	virtual bool is_AttributeNokNumber_allowed(Tango::AttReqType type);
/**
 *	Attribute AttributePendingNumber related methods
 *	Description: Number of attributes waiting to be archived
 *
 *	Data type:	Tango::DevLong
 *	Attr type:	Scalar
 */
	virtual void read_AttributePendingNumber(Tango::Attribute &attr);
	virtual bool is_AttributePendingNumber_allowed(Tango::AttReqType type);
/**
 *	Attribute AttributeNumber related methods
 *	Description: Number of configured attributes
 *
 *	Data type:	Tango::DevLong
 *	Attr type:	Scalar
 */
	virtual void read_AttributeNumber(Tango::Attribute &attr);
	virtual bool is_AttributeNumber_allowed(Tango::AttReqType type);
/**
 *	Attribute AttributeList related methods
 *	Description: Returns the configured attribute list
 *
 *	Data type:	Tango::DevString
 *	Attr type:	Spectrum max = 10000
 */
	virtual void read_AttributeList(Tango::Attribute &attr);
	virtual bool is_AttributeList_allowed(Tango::AttReqType type);
/**
 *	Attribute AttributeOkList related methods
 *	Description: Returns the attributes not on error list
 *
 *	Data type:	Tango::DevString
 *	Attr type:	Spectrum max = 10000
 */
	virtual void read_AttributeOkList(Tango::Attribute &attr);
	virtual bool is_AttributeOkList_allowed(Tango::AttReqType type);
/**
 *	Attribute AttributeNokList related methods
 *	Description: Returns the attributes on error list
 *
 *	Data type:	Tango::DevString
 *	Attr type:	Spectrum max = 10000
 */
	virtual void read_AttributeNokList(Tango::Attribute &attr);
	virtual bool is_AttributeNokList_allowed(Tango::AttReqType type);
/**
 *	Attribute AttributePendingList related methods
 *	Description: Returns the attributes waiting to be archived
 *
 *	Data type:	Tango::DevString
 *	Attr type:	Spectrum max = 10000
 */
	virtual void read_AttributePendingList(Tango::Attribute &attr);
	virtual bool is_AttributePendingList_allowed(Tango::AttReqType type);


	//--------------------------------------------------------
	/**
	 *	Method      : HdbEventSubscriber::add_dynamic_attributes()
	 *	Description : Add dynamic attributes if any.
	 */
	//--------------------------------------------------------
	void add_dynamic_attributes();



//	Command related methods
public:
	/**
	 *	Command AttributeAdd related method
	 *	Description: Add a new attribute to archive in HDB.
	 *
	 *	@param argin Attribute name
	 */
	virtual void attribute_add(Tango::DevString argin);
	virtual bool is_AttributeAdd_allowed(const CORBA::Any &any);
	/**
	 *	Command AttributeRemove related method
	 *	Description: Remove attribute from configuration.
	 *
	 *	@param argin Attribute name
	 */
	virtual void attribute_remove(Tango::DevString argin);
	virtual bool is_AttributeRemove_allowed(const CORBA::Any &any);
	/**
	 *	Command AttributeStatus related method
	 *	Description: Read a attribute status.
	 *
	 *	@param argin The attribute name
	 *	@returns The attribute status. TODO: DevString OK?
	 */
	virtual Tango::DevString attribute_status(Tango::DevString argin);
	virtual bool is_AttributeStatus_allowed(const CORBA::Any &any);
	/**
	 *	Command Start related method
	 *	Description: Start archiving
	 *
	 */
	virtual void start();
	virtual bool is_Start_allowed(const CORBA::Any &any);
	/**
	 *	Command Stop related method
	 *	Description: Stop archiving
	 *
	 */
	virtual void stop();
	virtual bool is_Stop_allowed(const CORBA::Any &any);
	/**
	 *	Command AttributeStart related method
	 *	Description: Start archiving single attribute
	 *
	 *	@param argin Attribute name
	 */
	virtual void attribute_start(Tango::DevString argin);
	virtual bool is_AttributeStart_allowed(const CORBA::Any &any);
	/**
	 *	Command AttributeStop related method
	 *	Description: Stop archiving single attribute
	 *
	 *	@param argin Attribute name
	 */
	virtual void attribute_stop(Tango::DevString argin);
	virtual bool is_AttributeStop_allowed(const CORBA::Any &any);
	/**
	 *	Command ResetStatistics related method
	 *	Description: Reset statistic counters
	 *
	 */
	virtual void reset_statistics();
	virtual bool is_ResetStatistics_allowed(const CORBA::Any &any);


/*----- PROTECTED REGION ID(HdbEventSubscriber::Additional Method prototypes) ENABLED START -----*/

	//	Additional Method prototypes
protected :

	/*----- PROTECTED REGION END -----*/	//	HdbEventSubscriber::Additional Method prototypes
};

/*----- PROTECTED REGION ID(HdbEventSubscriber::Additional Classes Definitions) ENABLED START -----*/

	//	Additional Classes definitions

	/*----- PROTECTED REGION END -----*/	//	HdbEventSubscriber::Additional Classes Definitions

}	//	End of namespace

#endif   //	HdbEventSubscriber_H
