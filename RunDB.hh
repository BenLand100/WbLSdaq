/**
 *  Copyright 2014 by Benjamin Land (a.k.a. BenLand100)
 *
 *  WbLSdaq is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  WbLSdaq is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with WbLSdaq. If not, see <http://www.gnu.org/licenses/>.
 */
 
#include <map>
#include <vector>
#include <string>

#include "json.hh"

#ifndef RunDB__hh
#define RunDB__hh

class RunTable : protected json::Value {

    public:
        
        inline RunTable(json::Value value) : json::Value(value) { 
            if (!isMember("name")) throw std::runtime_error("All tables must have a name.");
            name = getMember("name").cast<std::string>();
            if (isMember("index")) {
                index = getMember("index").cast<std::string>();
            } else {
                index = "";
            }
        }
        
        inline RunTable() : json::Value() { 
        }
        
        inline bool isMember(const std::string &key) const { 
            return json::Value::isMember(key); 
        }
        
        inline json::Value& operator[](const std::string &key) const {
            if (!isMember(key)) throw std::runtime_error("Table " + name + "[" + index + "] missing field " + key);
            return getMember(key); 
        }
        
        inline std::string getName() {
            return name;
        }
        
        inline std::string getIndex() {
            return index;
        }
        
    protected:
    
        std::string name, index;

};

class RunDB {

    public: 
    
        RunDB();
        
        virtual ~RunDB();

        void addFile(std::string file);
        
        bool tableExists(std::string name, std::string index = "");
        
        RunTable getTable(std::string name, std::string index = "");
        
        bool groupExists(std::string name);
        
        std::vector<RunTable> getGroup(std::string name);
        
    protected:
    
        std::map<std::string,std::map<std::string,RunTable>> db;

};

#endif
