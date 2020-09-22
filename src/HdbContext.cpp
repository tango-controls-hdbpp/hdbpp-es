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
#include <exception>

namespace HdbEventSubscriber_ns
{

    std::vector<Context> ContextMap::contexts;
    std::map<std::string, const Context&> ContextMap::contexts_map;

    Context::Context(std::string name, std::string upper, std::string desc):decl_name(std::move(name))
                                                        , upper_name(std::move(upper))
                                                        , description(std::move(desc))
    {
    }

    auto ContextMap::create_context(const std::string& name, const std::string& desc) -> Context&
    {
        std::string upper = name;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper); //transform to uppercase
        Context context(name, upper, desc);
        contexts.push_back(std::move(context)); 

        Context& inserted = contexts.back();
        contexts_map.insert({name, inserted});
        contexts_map.insert({inserted.get_name(), inserted});

        return inserted;
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
            add(context);
        }
    }

    void ContextMap::add(const std::string& key)
    {
        std::string upper(key);
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        
        for(size_t i = 0; i != local_contexts.size(); ++i)
        {
            if(contexts[i].get_name() == upper)
            {
                local_contexts.push_back(i);
                string_rep.clear();
                
                // Just in case but could be there already
                contexts_map.insert({upper, contexts[i]});
                contexts_map.insert({key, contexts[i]});
                
                break;
            }
        }
    }
    
    auto ContextMap::contains(const std::string& key) -> bool
    {
        bool found = false;
        auto res = contexts_map.find(key);
        if(res == contexts_map.end())
        {
            std::string upper(key);
            std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

            for(const auto& context : contexts)
            {
                if(context.get_name() == upper)
                {
                    contexts_map.insert({upper, context});
                    contexts_map.insert({key, context});
                    res = contexts_map.find(key);
                    break;
                }
            }
        }
        if(res != contexts_map.end())
        {
            for(size_t i = 0; i != local_contexts.size(); ++i)
            {
                if(contexts[i].get_name() == res->second.get_name())
                {
                    found = true;
                    break;
                }
            }
        }
        return found;
    }

    auto ContextMap::operator[](const std::string& key) -> const Context&
    {
        if(contains(key))
        {
            return contexts_map.at(key);
        }
        throw std::exception();
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
