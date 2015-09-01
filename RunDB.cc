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

#include <fstream>

#include "RunDB.hh"

using namespace std;
    
RunDB::RunDB() { }

RunDB::~RunDB() { }

void RunDB::addFile(string file) {

    ifstream dbfile;
    dbfile.open(file);
    if (!dbfile.is_open()) throw runtime_error("Could not open " + file);
    
    json::Reader reader(dbfile);
    dbfile.close();
    
    json::Value next;
    while (reader.getValue(next)) {
        if (next.getType() != json::TOBJECT) throw runtime_error("DB contains non-object values");
        if (!next.isMember("name")) throw runtime_error("Table does not contain a \"name\" field.");
        string name = next["name"].cast<string>();
        if (next.isMember("index")) {
            string index = next["index"].cast<string>();
            if (db[name].count(index)) throw runtime_error("Duplicate table found: " + name + "[" + index + "]");
            db[name][index] = next;
        } else {
            if (db[name].count("")) throw runtime_error("Duplicate table found: " + name);
            db[name][""] = next;
        }
    }
    
}

bool RunDB::tableExists(string name, string index) {
    return (db.count(name) != 0) && (db[name].count(index) != 0);
}

json::Value RunDB::getTable(string name, string index) {
    if (!db.count(name)) throw runtime_error("No table found: " + name + "[" + index + "]");
    if (!db[name].count(index)) throw runtime_error("No table found: " + name + "[" + index + "]");
    return db[name][index];
}

bool RunDB::groupExists(string name) {
    return db.count(name) != 0;
}

vector<json::Value> RunDB::getGroup(string name) {
    if (!db.count(name)) throw runtime_error("No group found: " + name);
    map<string,json::Value> &dbgroup = db[name];
    vector<json::Value> group;
    for (map<string,json::Value>::iterator iter = dbgroup.begin(); iter != dbgroup.end(); iter++) {
        group.push_back(iter->second);
    }
    return group;
}
