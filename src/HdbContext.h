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
    /**
     * Always context definition strings
     */
    static const std::string ALWAYS_CONTEXT("ALWAYS");
    static const std::string ALWAYS_CONTEXT_DESC("Always stored");

    class Context;

    /**
     * Contexts container.
     * Contains information about the current contexts
     * loaded in the EventSubscriber and a list of
     * local contexts associated to a signal.
     */
    class ContextMap
    {
        friend class Context;

        private:
            static std::vector<Context> contexts;
            static std::map<std::string, const Context&> contexts_map;
            
            static auto create_context(const std::string& name, const std::string& desc) -> Context&;
            
            void add(const std::string& key);
            
            std::vector<size_t> local_contexts;

            std::string string_rep;
        
        public:
            /**
             * Initialize the global contexts list.
             * This method should be the only one creating Context objects.
             * Output out_rep: a list of all the contexts and their description.
             */
            static void init(const std::vector<std::string>& contexts, std::vector<std::string>& out_rep);

            /**
             * List of contexts to be used as local contexts.
             * Note that if a context was not defined at startup,
             * it will not be added to the list.
             */
            void populate(const std::vector<std::string>& contexts);

            /**
             * Return true a context with name 'key', case insensitive,
             * is present in the list of local contexts.
             */
            auto contains(const std::string& key) -> bool;
            
            /**
             * Return the number of local contexts.
             */
            auto size() const -> size_t
            {
                return local_contexts.size();
            };

            /**
             * Return the context at the index pos.
             */
            auto operator[](size_t pos) -> const Context&
            {
                return contexts.at(local_contexts[pos]);
            };
            
            /**
             * Return the context with the name 'key', case insensitive.
             * If the key is not present throw an exception.
             */
            auto operator[](const std::string& key) -> const Context&;
            
            /**
             * Return this local contexts as a single string.
             * Each context are separated by '|'
             * E.g. RUN | SHUTDOWN
             */
            auto get_as_string() -> std::string;

    };
    
    /**
     * Context class.
     * Only a set of context should be initialized on application startup
     * and then used.
     * Contexts can be set using case insensitive string representation.
     */
    class Context
    {
        friend Context& ContextMap::create_context(const std::string& name, const std::string& desc);
        
        private:
            const std::string upper_name;
            const std::string decl_name;
            const std::string description;

            Context(std::string name, std::string upper, std::string desc);

            auto operator=(const Context&) -> Context& = delete;

        public:
            /**
             * Return this context name in upper case.
             */
            auto get_name() const -> const std::string&
            {
                return upper_name;
            };
            
            /**
             * Return this context name as it was declared in the configuration.
             */
            auto get_decl_name() const -> const std::string&
            {
                return decl_name;
            };
    };


}// namespace_ns
#endif // _HDB_CONTEXT_H
