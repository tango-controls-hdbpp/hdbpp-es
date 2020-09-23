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
#include <HdbContext.h>


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
    std::shared_ptr<HdbDevice> hdb_dev;
	bool		initialized;


private:

        void stop_attribute(const std::string& attribute);
        void start_attribute(const std::string& attribute);
        void pause_attribute(const std::string& attribute);

        static auto format_date(const timeval& tv, size_t ev) -> std::string;
	double last_statistics_reset_time;

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
	//	StatisticsTimeWindow:	Statistics time window in seconds
	Tango::DevLong	statisticsTimeWindow;
	//	CheckPeriodicTimeoutDelay:	Delay in seconds before timeout when checking periodic events
	Tango::DevLong	checkPeriodicTimeoutDelay;
	//	PollingThreadPeriod:	Polling Thread period in seconds.
	Tango::DevLong	pollingThreadPeriod;
	//	LibConfiguration:	Configuration for the library
	vector<string>	libConfiguration;
	//	ContextsList:	Possible contexts in the form label:description
	vector<string>	contextsList;
	//	DefaultStrategy:	Default strategy to be used when not specified in the single attribute configuration
	string	defaultStrategy;
	//	SubscribeChangeAsFallback:	It will subscribe to change event 
	//  if archive events are not configured
	Tango::DevBoolean	subscribeChangeAsFallback;
	//	AttributeListFile:	File with attribute list, alternative to AttributeList property
	string	attributeListFile;

//	Attribute data members
public:
	Tango::DevLong	*attr_AttributeOkNumber_read;
	Tango::DevLong	*attr_AttributeNokNumber_read;
	Tango::DevLong	*attr_AttributePendingNumber_read;
	Tango::DevLong	*attr_AttributeNumber_read;
	Tango::DevDouble	*attr_AttributeMaxStoreTime_read;
	Tango::DevDouble	*attr_AttributeMinStoreTime_read;
	Tango::DevDouble	*attr_AttributeMaxProcessingTime_read;
	Tango::DevDouble	*attr_AttributeMinProcessingTime_read;
	Tango::DevDouble	*attr_AttributeRecordFreq_read;
	Tango::DevDouble	*attr_AttributeFailureFreq_read;
	Tango::DevLong	*attr_AttributeStartedNumber_read;
	Tango::DevLong	*attr_AttributeStoppedNumber_read;
	Tango::DevLong	*attr_AttributeMaxPendingNumber_read;
	Tango::DevDouble	*attr_StatisticsResetTime_read;
	Tango::DevLong	*attr_AttributePausedNumber_read;
	Tango::DevString	*attr_Context_read;
	Tango::DevString	*attr_AttributeList_read;
	Tango::DevString	*attr_AttributeOkList_read;
	Tango::DevString	*attr_AttributeNokList_read;
	Tango::DevString	*attr_AttributePendingList_read;
	Tango::DevDouble	*attr_AttributeRecordFreqList_read;
	Tango::DevDouble	*attr_AttributeFailureFreqList_read;
	Tango::DevString	*attr_AttributeStartedList_read;
	Tango::DevString	*attr_AttributeStoppedList_read;
	Tango::DevLong	*attr_AttributeEventNumberList_read;
	Tango::DevString	*attr_AttributeErrorList_read;
	Tango::DevString	*attr_AttributePausedList_read;
	Tango::DevString	*attr_AttributeStrategyList_read;
	Tango::DevString	*attr_ContextsList_read;
	Tango::DevULong	*attr_AttributeTTLList_read;

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
	//--------------------------------------------------------
	/*
	 *	Method      : HdbEventSubscriber::write_attr_hardware()
	 *	Description : Hardware writing for attributes.
	 */
	//--------------------------------------------------------
	virtual void write_attr_hardware(vector<long> &attr_list);

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
 *	Attribute AttributeMaxStoreTime related methods
 *	Description: Maximum storing time
 *
 *	Data type:	Tango::DevDouble
 *	Attr type:	Scalar
 */
	virtual void read_AttributeMaxStoreTime(Tango::Attribute &attr);
	virtual bool is_AttributeMaxStoreTime_allowed(Tango::AttReqType type);
/**
 *	Attribute AttributeMinStoreTime related methods
 *	Description: Minimum storing time
 *
 *	Data type:	Tango::DevDouble
 *	Attr type:	Scalar
 */
	virtual void read_AttributeMinStoreTime(Tango::Attribute &attr);
	virtual bool is_AttributeMinStoreTime_allowed(Tango::AttReqType type);
/**
 *	Attribute AttributeMaxProcessingTime related methods
 *	Description: Maximum processing (from event reception to storage) time
 *
 *	Data type:	Tango::DevDouble
 *	Attr type:	Scalar
 */
	virtual void read_AttributeMaxProcessingTime(Tango::Attribute &attr);
	virtual bool is_AttributeMaxProcessingTime_allowed(Tango::AttReqType type);
/**
 *	Attribute AttributeMinProcessingTime related methods
 *	Description: Minimum processing (from event reception to storage) time
 *
 *	Data type:	Tango::DevDouble
 *	Attr type:	Scalar
 */
	virtual void read_AttributeMinProcessingTime(Tango::Attribute &attr);
	virtual bool is_AttributeMinProcessingTime_allowed(Tango::AttReqType type);
/**
 *	Attribute AttributeRecordFreq related methods
 *	Description: Record frequency
 *
 *	Data type:	Tango::DevDouble
 *	Attr type:	Scalar
 */
	virtual void read_AttributeRecordFreq(Tango::Attribute &attr);
	virtual bool is_AttributeRecordFreq_allowed(Tango::AttReqType type);
/**
 *	Attribute AttributeFailureFreq related methods
 *	Description: Failure frequency
 *
 *	Data type:	Tango::DevDouble
 *	Attr type:	Scalar
 */
	virtual void read_AttributeFailureFreq(Tango::Attribute &attr);
	virtual bool is_AttributeFailureFreq_allowed(Tango::AttReqType type);
/**
 *	Attribute AttributeStartedNumber related methods
 *	Description: Number of archived attributes started
 *
 *	Data type:	Tango::DevLong
 *	Attr type:	Scalar
 */
	virtual void read_AttributeStartedNumber(Tango::Attribute &attr);
	virtual bool is_AttributeStartedNumber_allowed(Tango::AttReqType type);
/**
 *	Attribute AttributeStoppedNumber related methods
 *	Description: Number of archived attributes stopped
 *
 *	Data type:	Tango::DevLong
 *	Attr type:	Scalar
 */
	virtual void read_AttributeStoppedNumber(Tango::Attribute &attr);
	virtual bool is_AttributeStoppedNumber_allowed(Tango::AttReqType type);
/**
 *	Attribute AttributeMaxPendingNumber related methods
 *	Description: Max number of attributes waiting to be archived
 *
 *	Data type:	Tango::DevLong
 *	Attr type:	Scalar
 */
	virtual void read_AttributeMaxPendingNumber(Tango::Attribute &attr);
	virtual bool is_AttributeMaxPendingNumber_allowed(Tango::AttReqType type);
/**
 *	Attribute StatisticsResetTime related methods
 *	Description: Seconds elapsed since the last statistics reset
 *
 *	Data type:	Tango::DevDouble
 *	Attr type:	Scalar
 */
	virtual void read_StatisticsResetTime(Tango::Attribute &attr);
	virtual bool is_StatisticsResetTime_allowed(Tango::AttReqType type);
/**
 *	Attribute AttributePausedNumber related methods
 *	Description: Number of archived attributes paused
 *
 *	Data type:	Tango::DevLong
 *	Attr type:	Scalar
 */
	virtual void read_AttributePausedNumber(Tango::Attribute &attr);
	virtual bool is_AttributePausedNumber_allowed(Tango::AttReqType type);
/**
 *	Attribute Context related methods
 *	Description: 
 *
 *	Data type:	Tango::DevString
 *	Attr type:	Scalar
 */
	virtual void read_Context(Tango::Attribute &attr);
	virtual void write_Context(Tango::WAttribute &attr);
	virtual bool is_Context_allowed(Tango::AttReqType type);
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
 *	Description: Returns the list attributes waiting to be archived
 *
 *	Data type:	Tango::DevString
 *	Attr type:	Spectrum max = 10000
 */
	virtual void read_AttributePendingList(Tango::Attribute &attr);
	virtual bool is_AttributePendingList_allowed(Tango::AttReqType type);
/**
 *	Attribute AttributeRecordFreqList related methods
 *	Description: Returns the list of record frequencies
 *
 *	Data type:	Tango::DevDouble
 *	Attr type:	Spectrum max = 10000
 */
	virtual void read_AttributeRecordFreqList(Tango::Attribute &attr);
	virtual bool is_AttributeRecordFreqList_allowed(Tango::AttReqType type);
/**
 *	Attribute AttributeFailureFreqList related methods
 *	Description: Returns the list of failure frequencies
 *
 *	Data type:	Tango::DevDouble
 *	Attr type:	Spectrum max = 10000
 */
	virtual void read_AttributeFailureFreqList(Tango::Attribute &attr);
	virtual bool is_AttributeFailureFreqList_allowed(Tango::AttReqType type);
/**
 *	Attribute AttributeStartedList related methods
 *	Description: Returns the attributes started list
 *
 *	Data type:	Tango::DevString
 *	Attr type:	Spectrum max = 10000
 */
	virtual void read_AttributeStartedList(Tango::Attribute &attr);
	virtual bool is_AttributeStartedList_allowed(Tango::AttReqType type);
/**
 *	Attribute AttributeStoppedList related methods
 *	Description: Returns the attributes stopped list
 *
 *	Data type:	Tango::DevString
 *	Attr type:	Spectrum max = 10000
 */
	virtual void read_AttributeStoppedList(Tango::Attribute &attr);
	virtual bool is_AttributeStoppedList_allowed(Tango::AttReqType type);
/**
 *	Attribute AttributeEventNumberList related methods
 *	Description: Returns the list of numbers of events received
 *
 *	Data type:	Tango::DevLong
 *	Attr type:	Spectrum max = 10000
 */
	virtual void read_AttributeEventNumberList(Tango::Attribute &attr);
	virtual bool is_AttributeEventNumberList_allowed(Tango::AttReqType type);
/**
 *	Attribute AttributeErrorList related methods
 *	Description: Returns the list of attribute errors
 *
 *	Data type:	Tango::DevString
 *	Attr type:	Spectrum max = 10000
 */
	virtual void read_AttributeErrorList(Tango::Attribute &attr);
	virtual bool is_AttributeErrorList_allowed(Tango::AttReqType type);
/**
 *	Attribute AttributePausedList related methods
 *	Description: Returns the attributes stopped list
 *
 *	Data type:	Tango::DevString
 *	Attr type:	Spectrum max = 10000
 */
	virtual void read_AttributePausedList(Tango::Attribute &attr);
	virtual bool is_AttributePausedList_allowed(Tango::AttReqType type);
/**
 *	Attribute AttributeStrategyList related methods
 *	Description: Returns the list of attribute strategy
 *
 *	Data type:	Tango::DevString
 *	Attr type:	Spectrum max = 10000
 */
	virtual void read_AttributeStrategyList(Tango::Attribute &attr);
	virtual bool is_AttributeStrategyList_allowed(Tango::AttReqType type);
/**
 *	Attribute ContextsList related methods
 *	Description: 
 *
 *	Data type:	Tango::DevString
 *	Attr type:	Spectrum max = 1000
 */
	virtual void read_ContextsList(Tango::Attribute &attr);
	virtual bool is_ContextsList_allowed(Tango::AttReqType type);
/**
 *	Attribute AttributeTTLList related methods
 *	Description: Returns the list of attribute strategy
 *
 *	Data type:	Tango::DevULong
 *	Attr type:	Spectrum max = 10000
 */
	virtual void read_AttributeTTLList(Tango::Attribute &attr);
	virtual bool is_AttributeTTLList_allowed(Tango::AttReqType type);


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
	 *	@param argin Attribute name, strategy, data_type, data_format, write_type.
	 *               
	 *               Note that only attribute name is mandatory.
	 *               If the strategy is not provided, default strategy will be used.
	 *               If any of the 3 last arguments is not provided the database will be queried to retrieve the proper values.
	 */
	virtual void attribute_add(const Tango::DevVarStringArray *argin);
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
	 *	@returns The attribute status.
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
	/**
	 *	Command Pause related method
	 *	Description: Pause archiving
	 *
	 */
	virtual void pause();
	virtual bool is_Pause_allowed(const CORBA::Any &any);
	/**
	 *	Command AttributePause related method
	 *	Description: Pause archiving single attribute
	 *
	 *	@param argin Attribute name
	 */
	virtual void attribute_pause(Tango::DevString argin);
	virtual bool is_AttributePause_allowed(const CORBA::Any &any);
	/**
	 *	Command SetAttributeStrategy related method
	 *	Description: Update strategy associated to an already archived attribute.
	 *
	 *	@param argin Attribute name, strategy
	 */
	virtual void set_attribute_strategy(const Tango::DevVarStringArray *argin);
	virtual bool is_SetAttributeStrategy_allowed(const CORBA::Any &any);
	/**
	 *	Command GetAttributeStrategy related method
	 *	Description: Read a attribute contexts.
	 *
	 *	@param argin The attribute name
	 *	@returns The attribute contexts.
	 */
	virtual Tango::DevString get_attribute_strategy(Tango::DevString argin);
	virtual bool is_GetAttributeStrategy_allowed(const CORBA::Any &any);
	/**
	 *	Command StopFaulty related method
	 *	Description: Stop archiving faulty attributes
	 *
	 */
	virtual void stop_faulty();
	virtual bool is_StopFaulty_allowed(const CORBA::Any &any);
	/**
	 *	Command SetAttributeTTL related method
	 *	Description: Update TTL associated to an already archived attribute.
	 *
	 *	@param argin Attribute name, TTL
	 */
	virtual void set_attribute_ttl(const Tango::DevVarStringArray *argin);
	virtual bool is_SetAttributeTTL_allowed(const CORBA::Any &any);
	/**
	 *	Command GetAttributeTTL related method
	 *	Description: Read an attribute TTL.
	 *
	 *	@param argin The attribute name
	 *	@returns The attribute TTL.
	 */
	virtual Tango::DevULong get_attribute_ttl(Tango::DevString argin);
	virtual bool is_GetAttributeTTL_allowed(const CORBA::Any &any);


	//--------------------------------------------------------
	/**
	 *	Method      : HdbEventSubscriber::add_dynamic_commands()
	 *	Description : Add dynamic commands if any.
	 */
	//--------------------------------------------------------
	void add_dynamic_commands();

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
