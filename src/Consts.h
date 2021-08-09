//=============================================================================
//
// file :        Consts.h
//
// description : Defines all kind of constants
//
// project :	Tango Device Server
//
// $Author: graziano $
//
// $Revision: 1.5 $
//
//
//
//
// copyleft :    European Synchrotron Radiation Facility
//               BP 220, Grenoble 38043
//               FRANCE
//
//=============================================================================

#ifndef _CONSTS_H
#define _CONSTS_H

/**
 * @author	$Author: graziano $
 * @version	$Revision: 1.5 $
 */

 //	constants definitions here.
 //-----------------------------------------------

namespace HdbEventSubscriber_ns
{
    const unsigned int s_to_ms_factor = 1000;
    const unsigned int ms_to_us_factor = 1000;
    const unsigned int us_to_ns_factor = 1000;
    const unsigned int s_to_ns_factor = 1000000000;
    const unsigned int ms_to_ns_factor = 1000000;
    const unsigned int s_to_us_factor = 1000000;
    const unsigned int ten_s_in_ms = 10000;
    const unsigned int hundred_s_in_ms = 100000;
    const unsigned int tango_prefix_length = 8;
    const unsigned int MAX_ATTRIBUTES = 10000;

}	// namespace_ns

#endif	// _CONSTS_H
