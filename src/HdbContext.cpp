/* Copyright (C) : 2014-2020
   European Synchrotron Radiation Facility
   BP 220, Grenoble 38043, FRANCE

   This file is part of hdbpp-es.
   
   hdbpp-es is free software: you can redistribute it and/or modify
   it under the terms of the Lesser GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   hdbpp-es is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser
   GNU General Public License for more details.
   
   You should have received a copy of the Lesser GNU General Public License
   along with hdbpp-es.  If not, see <http://www.gnu.org/licenses/>. */

#include "HdbContext.h"
#include "HdbDevice.h"
#include <algorithm>

namespace HdbEventSubscriber_ns
{

    std::vector<Context> ContextMap::contexts;

    Context::Context(const std::string& name, const std::string& desc):decl_name(name)
                                                                             , description(desc)
    {
        upper_name = name;
        std::transform(upper_name.begin(), upper_name.end(), upper_name.begin(), ::toupper); //transform to uppercase
    }

    auto ContextMap::create_context(const std::string& name, const std::string& desc) -> Context&
    {
        Context context(name, desc);
        contexts.push_back(std::move(context)); 

        return contexts.back();
//        context_map.insert({name, inserted});
//        context_map.insert({inserted.get_name(), inserted});
    }

    void ContextMap::init(const std::vector<std::string>& init_contexts, std::vector<std::string>& out_rep)
    {
        bool always_init = false;
        for(const auto& context : init_contexts)
        {
            std::vector<std::string> res;
            HdbDevice::string_explode(context, ":", res);
            if(res.size() == 2)
            {
                Context& c = create_context(res[0], res[1]);

                if(c.get_name() == ALWAYS_CONTEXT)
                {
                    always_init = true;
                }
                out_rep.push_back(context);
            }
        }
        
        if(!always_init)
        {
            create_context(ALWAYS_CONTEXT, ALWAYS_CONTEXT_DESC);
            out_rep.push_back(ALWAYS_CONTEXT + ": " + ALWAYS_CONTEXT_DESC);
        }
        
    }

    void ContextMap::populate(const std::vector<std::string>& sub_contexts)
    {
        for(const auto& context : sub_contexts)
        {
            find(context);
        }
    }

    auto ContextMap::find(const std::string& key) -> std::map<std::string, const Context&>::iterator
    {
        auto res = context_map.find(key);
        if(res == context_map.end())
        {
            std::string upper(key);
            std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

            for(const auto& context : contexts)
            {
                if(context.get_name() == upper)
                {
                    context_map.insert({upper, context});
                    context_map.insert({key, context});
                    bool found = false;
                    for(const auto& lc : local_contexts)
                    {
                        if(lc == upper)
                        {
                            found = true;
                            break;
                        }
                    }
                    if(!found)
                    {
                        local_contexts.push_back(upper);
                        string_rep.clear();
                    }
                    res = context_map.find(key);
                    break;
                }
            }
        }
        return res;
    }

    auto ContextMap::contains(const std::string& key) -> bool
    {
        return find(key) != context_map.end();
    }

    auto ContextMap::operator[](const std::string& key) -> const Context&
    {
        return find(key)->second;
    }
    
    auto ContextMap::get_as_string() -> std::string
    {
        if(string_rep.empty())
        {
            size_t idx = 0;
            size_t max_size = size();
            for(const auto& lc : local_contexts)
            {
                string_rep += (*this)[lc].get_decl_name();
                
                if(++idx != max_size)
                    string_rep += "|";
            }
        }
        return string_rep;
    }

}// namespace_ns
