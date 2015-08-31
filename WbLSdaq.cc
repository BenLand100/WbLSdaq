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
 
#include <pthread.h>
#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <stdexcept>
#include <unistd.h>
#include <ctime>
#include <cmath>
#include <csignal>
#include <cstring>

#include "json.hh"
#include "VMEBridge.hh"
#include "V1730_dpppsd.hh"
#include "V1742.hh"
#include "DataHandler.hh"

map<string,json::Value> readDB(string file) {

    ifstream dbfile;
    dbfile.open(file);
    if (!dbfile.is_open()) throw runtime_error("Could not open " + file);
    
    json::Reader reader(dbfile);
    dbfile.close();
    map<string,json::Value> db;
    
    json::Value next;
    while (reader.getValue(next)) {
        if (next.getType() != json::TOBJECT) throw runtime_error("DB contains non-object values");
        string name = next["name"].cast<string>();
        if (next.isMember("index")) {
            db[name+"["+next["index"].cast<string>()+"]"] = next;
        } else {
            db[name+"[]"] = next;
        }
    }
    
    return db;
}

bool running;

void int_handler(int x) {
    running = false;
}

typedef struct {
    EventDecoder *acq;
    Buffer *buff;
} decode_thread_data;

void *decode_thread(void *data) {
    signal(SIGINT,int_handler);
    EventDecoder *acq = ((decode_thread_data*)data)->acq;
    Buffer *buff = ((decode_thread_data*)data)->buff;
    try {
        while (running) {
            buff->ready();
            if (running && !acq->decode(*buff)) running = false;
        }
    } catch (runtime_error &e) {
        running = false;
        cout << "Decode thread aborted: " << e.what() << endl;
    }
    pthread_exit(NULL);
}

int main(int argc, char **argv) {

    if (argc != 2) {
        cout << "./v1730_dpppsd config.json" << endl;
        return -1;
    }

    running = true;
    signal(SIGINT,int_handler);

    cout << "Reading configuration..." << endl;
    
    map<string,json::Value> db = readDB(argv[1]);
    json::Value run = db["RUN[]"];
    
    const int ngrabs = run["events"].cast<int>();
    const string outfile = run["outfile"].cast<string>();
    const int linknum = run["link_num"].cast<int>();
    const int baseaddr = run["base_address"].cast<int>();
    const int checktemps = run["check_temps"].cast<bool>();
    int nrepeat;
    if (run.isMember("repeat_times")) {
        nrepeat = run["repeat_times"].cast<int>();
    } else {
        nrepeat = 0;
    }

    cout << "Opening VME link..." << endl;
    
    VMEBridge bridge(linknum,0);
    V1730 dgtz(bridge,baseaddr,false);
   
    cout << "Programming Digitizer..." << endl;
    
    V1730Settings settings(db);
    
    if (!dgtz.program(settings)) return -1;
    
    cout << "Starting acquisition..." << endl;
    
    Buffer buff(100*1024*1024);
    EventDecoder acq(ngrabs,nrepeat,outfile,settings);
    vector<uint32_t> temps;
    dgtz.startAcquisition();
    
    decode_thread_data data;
    data.acq = &acq;
    data.buff = &buff;
    pthread_t decode;
    pthread_create(&decode,NULL,&decode_thread,&data);
    
    for (size_t counter = 0; running; counter++) {
        
        while (!dgtz.readoutReady() && running) {
            if (!dgtz.acquisitionRunning()) break;
        }
        
        if (!dgtz.acquisitionRunning()) {
            cout << "Digitizer aborted acquisition!" << endl;
            running = false;
        }
        
        if (!running) break;

        buff.inc(dgtz.readoutBLT(buff.wptr(),buff.free()));
        
        //cout << buff.pct() << endl;

        if (checktemps && counter % 100 == 0) {
            bool overtemp = dgtz.checkTemps(temps,60);
            cout << "temps: [ " << temps[0];
            for (int ch = 1; ch < 16; ch++) cout << ", " << temps[ch];
            cout << " ]" << endl;
            if (overtemp) {
                cout << "Overtemp! Aborting readout." << endl;
                break;
            }
        }
        
    }
    
    buff.inc(0);
     
    cout << "Stopping acquisition..." << endl;
    
    dgtz.stopAcquisition();

    pthread_cancel(decode);
    pthread_exit(NULL);

}
