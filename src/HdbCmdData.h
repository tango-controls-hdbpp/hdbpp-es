/* Copyright (C) 2014-2017
   Elettra - Sincrotrone Trieste S.C.p.A.
   Strada Statale 14 - km 163,5 in AREA Science Park
   34149 Basovizza, Trieste, Italy.

   This file is part of libhdb++.

   libhdb++ is free software: you can redistribute it and/or modify
   it under the terms of the Lesser GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   libhdb++ is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser
   GNU General Public License for more details.

   You should have received a copy of the Lesser GNU General Public License
   along with libhdb++.  If not, see <http://www.gnu.org/licenses/>. */

#ifndef _HDB_CMD_DATA_H
#define _HDB_CMD_DATA_H

#include "hdb++/AbstractDB.h"
#include <tango.h>
#include <string>

namespace HdbEventSubscriber_ns
{

class  HdbCmdData
{
public:

	HdbCmdData(Tango::EventData *ev_data_, hdbpp::HdbEventDataType ev_data_type_) :
        ev_data(ev_data_), 
        ev_data_param (nullptr),
        ev_data_type(ev_data_type_),
        op_code(DB_INSERT) {}

	HdbCmdData(Tango::AttrConfEventData *ev_data_param_, hdbpp::HdbEventDataType ev_data_type_) :
        ev_data(nullptr),
        ev_data_param(ev_data_param_),
        ev_data_type(ev_data_type_),
        op_code(DB_INSERT_PARAM) {}

	HdbCmdData(uint8_t op_code_, const std::string &attr_name_) :
        op_code(op_code_),
        attr_name(attr_name_),
        ev_data(nullptr),
        ev_data_param(nullptr) {}

	HdbCmdData(uint8_t op_code_, unsigned int ttl_, const std::string &attr_name_) : 
        op_code(op_code_),
        attr_name(attr_name_),
        ttl(ttl_),
        ev_data(nullptr),
        ev_data_param(nullptr) {}

    ~HdbCmdData()
    {
        if(ev_data) 
            delete ev_data; 
            
        if(ev_data_param) 
            delete ev_data_param;
    }

	Tango::EventData *ev_data;
	Tango::AttrConfEventData *ev_data_param;
	hdbpp::HdbEventDataType ev_data_type;
	uint8_t op_code;
	unsigned int  ttl;
	std::string attr_name;
};
}	// namespace_ns
#endif // _HDB_CMD_DATA_H