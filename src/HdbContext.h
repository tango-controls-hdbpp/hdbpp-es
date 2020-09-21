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

#ifndef _HDB_CONTEXT_H
#define _HDB_CONTEXT_H

#include <string>
#include <map>
#include <vector>

namespace HdbEventSubscriber_ns
{

    static const std::string ALWAYS_CONTEXT("ALWAYS");
    static const std::string ALWAYS_CONTEXT_DESC("Always stored");

    class Context;

    class ContextMap
    {
        friend class Context;

        private:
            static std::vector<Context> contexts;
            static auto create_context(const std::string& name, const std::string& desc) -> Context&;
            
            std::map<std::string, const Context&> context_map;
            std::vector<std::string> local_contexts;

            std::string string_rep;
        
        public:
            static void init(const std::vector<std::string>& contexts, std::vector<std::string>& out_rep);

            void populate(const std::vector<std::string>& contexts);

            auto find(const std::string& key) -> std::map<std::string, const Context&>::iterator;
            
            auto contains(const std::string& key) -> bool;
            
            auto size() const -> size_t
            {
                return local_contexts.size();
            };

            auto operator[](size_t pos) -> const Context&
            {
                return find(local_contexts[pos])->second;
            };
            
            auto operator[](const std::string& key) -> const Context&;

            auto get_as_string() -> std::string;

    };
    
    class Context
    {
        friend Context& ContextMap::create_context(const std::string& name, const std::string& desc);
        
        private:
            std::string upper_name;
            const std::string decl_name;
            const std::string description;

            Context(const std::string& name, const std::string& desc);

            auto operator=(const Context&) -> Context& = delete;

        public:
            auto get_name() const -> const std::string&
            {
                return upper_name;
            };
            
            auto get_decl_name() const -> const std::string&
            {
                return decl_name;
            };
    };


}// namespace_ns
#endif // _HDB_CONTEXT_H
