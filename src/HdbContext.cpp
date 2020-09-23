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
    std::map<std::string, size_t> ContextMap::contexts_map;

    const Context Context::NO_CONTEXT("", "", "");
    const Context Context::ALWAYS_CONTEXT("ALWAYS", "ALWAYS", "Always stored");

    Context::Context(std::string name, std::string upper, std::string desc):decl_name(std::move(name))
                                                        , upper_name(std::move(upper))
                                                        , description(std::move(desc))
    {
    }

    auto Context::to_string() const -> std::string
    {
        std::string res(decl_name);
        res.append(": ");
        res.append(description);
        return std::move(res);
    }

    auto ContextMap::create_context(const std::string& name, const std::string& desc) -> const Context&
    {
        std::string upper = name;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper); //transform to uppercase

        // Do not create a new one if already existing.
        if(contexts_map.find(upper) != contexts_map.end())
        {
            return contexts[contexts_map.at(upper) - 1];
        }
       
        // Create new unique context.
        Context context(name, upper, desc);
        contexts.push_back(std::move(context)); 

        contexts_map.insert({name, contexts.size()});
        contexts_map.insert({upper, contexts.size()});

        return contexts.back();
    }

    void ContextMap::init(const std::vector<std::string>& init_contexts, std::vector<std::string>& out_rep)
    {
        size_t idx = always_context_idx;
        // Insert always context, as it is always defined.
        contexts_map.insert({Context::always_context().get_name(), idx});
        bool always_init = false;
        for(const auto& context : init_contexts)
        {
            std::vector<std::string> res;
            HdbDevice::string_explode(context, ":", res);
            if(res.size() == 2)
            {
                const Context& c = create_context(res[0], res[1]);

                if(&c == &Context::always_context())
                {
                    always_init = true;
                }
                out_rep.push_back(context);
            }
        }
        
        if(!always_init)
        {
            out_rep.push_back(Context::always_context().to_string());
        }
        
    }

    auto ContextMap::index(const Context& context) -> size_t
    {
        // Trivial cases
        if(&context == &Context::always_context())
        {
            return always_context_idx;
        }
        size_t i = 0;
        for(; i != contexts.size(); ++i)
        {
            if(&(contexts[i]) == &context)
                break;
        }
        
        // Not found return out of bounds idx
        return i + 1;
    }
    
    auto ContextMap::index(const std::string& context) -> size_t
    {
        if(defined(context))
        {
            return index(contexts[contexts_map[context] - 1]);
        }
        
        // Not found return out of bounds idx
        return contexts.size() + 1;
    }
    
    auto ContextMap::at(size_t idx) -> const Context&
    {
        // Trivial case
        if(idx == always_context_idx)
        {
            return Context::always_context();
        }

        if(idx < 1 || idx > contexts.size())
        {
            return Context::no_context();
        }

        return contexts[idx - 1];
    }

    auto ContextMap::defined(const std::string& context) -> bool
    {
        size_t always_idx = always_context_idx;
        bool found = false;
        auto res = contexts_map.find(context);
        if(res == contexts_map.end())
        {
            std::string upper(context);
            std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

            if(Context::always_context().get_name() == upper)
            {
                contexts_map.insert({context, always_idx});
                found = true;
            }
            else
            {
                for(size_t i = 0; i != contexts.size(); ++i)
                {
                    if(contexts[i].get_name() == upper)
                    {
                        contexts_map.insert({context, i + 1});
                        found = true;
                        break;
                    }
                }
            }
        }
        else
        {
            found = true;
        }
        return found;
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
        size_t always_idx = always_context_idx;
        std::string upper(key);
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        
        if(Context::always_context().get_name() == upper)
        {
            local_contexts.insert(always_idx);
            string_rep.clear();
        }
        else
        {
            for(size_t i = 0; i != contexts.size(); ++i)
            {
                if(contexts[i].get_name() == upper)
                {
                    local_contexts.insert(i + 1);
                    string_rep.clear();

                    // Just in case but could be there already
                    contexts_map.insert({upper, i + 1});
                    contexts_map.insert({key, i + 1});

                    break;
                }
            }
        }
    }

    auto ContextMap::contains(const Context& context) -> bool
    {
        bool found = false;
        
        for(auto lc : local_contexts)
        {
            if(&(*this)[lc] == &context)
            {
                found = true;
                break;
            }
        }
        
        return found;
    }
    
    auto ContextMap::contains(const std::string& key) -> bool
    {
        size_t always_idx = always_context_idx;
        bool found = false;
        auto res = contexts_map.find(key);
        if(res == contexts_map.end())
        {
            std::string upper(key);
            std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

            if(Context::always_context().get_name() == upper)
            {
                contexts_map.insert({key, always_idx});
                res = contexts_map.find(key);
            }
            else
            {
                for(size_t i = 0; i != contexts.size(); ++i)
                {
                    if(contexts[i].get_name() == upper)
                    {
                        contexts_map.insert({upper, i + 1});
                        contexts_map.insert({key, i + 1});
                        res = contexts_map.find(key);
                        break;
                    }
                }
            }
        }
        if(res != contexts_map.end())
        {
            for(auto lc : local_contexts)
            {
                if(lc == res->second)
                {
                    found = true;
                    break;
                }
            }
        }
        return found;
    }

    auto ContextMap::operator[](size_t pos) -> const Context&
    {
        if(pos == 0)
        {
            return Context::always_context();
        }

        return contexts[pos - 1];
    }

    auto ContextMap::operator[](const std::string& key) -> const Context&
    {
        if(contains(key))
        {
            return contexts[contexts_map.at(key) - 1];
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
                string_rep += contexts[lc].get_decl_name();
                
                if(++idx != max_size)
                    string_rep += "|";
            }
        }
        return string_rep;
    }

}// namespace_ns
