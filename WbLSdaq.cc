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
#include <map>
#include <vector>
#include <stdexcept>
#include <unistd.h>
#include <ctime>
#include <cmath>
#include <csignal>
#include <cstring>

#include "RunDB.hh"
#include "VMEBridge.hh"
#include "V1730_dpppsd.hh"
#include "V1742.hh"
#include "V65XX.hh"

using namespace std;
using namespace H5;

bool running;

void int_handler(int x) {
    running = false;
}

typedef struct {
    vector<Buffer*> *buffers;
    vector<Decoder*> *decoders;
    pthread_mutex_t *iomutex;
    pthread_cond_t *newdata;
    size_t nEvents, nRepeat, curCycle;
    string outfile;
} decode_thread_data;

void *decode_thread(void *_data) {
    signal(SIGINT,int_handler);
    decode_thread_data* data = (decode_thread_data*)_data;
    data->curCycle = 0;
    struct timespec cur_time, last_time;
    clock_gettime(CLOCK_MONOTONIC,&last_time);
    try {
        while (running) {
            pthread_mutex_lock(data->iomutex);
            for (;;) {
                bool found = !running;
                for (size_t i = 0; i < data->buffers->size(); i++) {
                    found |= (*data->buffers)[i]->fill() > 0;
                }
                if (found) break;
                pthread_cond_wait(data->newdata,data->iomutex);
            }
            bool writeout = data->nEvents > 0;
            size_t total = 0;
            for (size_t i = 0; i < data->decoders->size(); i++) {
                size_t sz = (*data->buffers)[i]->fill();
                if (sz > 0) (*data->decoders)[i]->decode(*(*data->buffers)[i]);
                size_t ev = (*data->decoders)[i]->eventsReady();
                if (ev < data->nEvents) writeout = false;
                total += ev;
            }
            
            if (data->nRepeat) cout << "Cycle " << data->curCycle+1 << " / " << data->nRepeat << endl;
            
            clock_gettime(CLOCK_MONOTONIC,&cur_time);
            double time_int = (cur_time.tv_sec - last_time.tv_sec)+1e-9*(cur_time.tv_nsec - last_time.tv_nsec);
            cout << "Avg rate " << ((double)total/data->decoders->size())/time_int << " Hz" << endl;
            
            if (writeout) {
                Exception::dontPrint();
                string fname = data->outfile;
                if (data->nRepeat > 0) {
                    fname += "." + to_string(data->curCycle);
                    if (++data->curCycle == data->nRepeat) running = false;
                } else {
                    running = false;
                }
                fname += ".h5"; 
                cout << "Saving data to " << fname << endl;
                H5File file(fname, H5F_ACC_TRUNC);
                for (size_t i = 0; i < data->decoders->size(); i++) {
                    (*data->decoders)[i]->writeOut(file,data->nEvents);
                }
                last_time = cur_time;
            }
            pthread_mutex_unlock(data->iomutex);
        }
    } catch (runtime_error &e) {
        pthread_mutex_unlock(data->iomutex);
        running = false;
        pthread_mutex_lock(data->iomutex);
        cout << "Decode thread aborted: " << e.what() << endl;
        pthread_mutex_unlock(data->iomutex);
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
    
    RunDB db;
    db.addFile(argv[1]);
    RunTable run = db.getTable("RUN");
    
    const int nEvents = run["events"].cast<int>();
    const string outfile = run["outfile"].cast<string>();
    const int linknum = run["link_num"].cast<int>();
    const int temptime = run["check_temps_every"].cast<int>();
    int nRepeat;
    if (run.isMember("repeat_times")) {
        nRepeat = run["repeat_times"].cast<int>();
    } else {
        nRepeat = 0;
    }

    cout << "Opening VME link..." << endl;
    
    VMEBridge bridge(linknum,0);
    
    cout << "Setting up V65XX HV..." << endl;
    
    vector<V65XX*> hvs;
    
    vector<RunTable> v65XXs = db.getGroup("V65XX");
    for (size_t i = 0; i < v65XXs.size(); i++) {
        RunTable &tbl = v65XXs[i];
        cout << "\t" << tbl["index"].cast<string>() << endl;
        hvs.push_back(new V65XX(bridge,tbl["base_address"].cast<int>()));
        hvs.back()->set(tbl);
    }
    
    cout << "Setting up digitizers..." << endl;
    
    vector<DigitizerSettings*> settings;
    vector<Digitizer*> digitizers;
    vector<Buffer*> buffers;
    vector<Decoder*> decoders;
    
    size_t arm_last = 0;
    
    vector<RunTable> v1730s = db.getGroup("V1730");
    for (size_t i = 0; i < v1730s.size(); i++) {
        RunTable &tbl = v1730s[i];
        cout << "* V1730 - " << tbl.getIndex() << endl;
        V1730Settings *stngs = new V1730Settings(tbl,db);
        settings.push_back(stngs);
        digitizers.push_back(new V1730(bridge,tbl["base_address"].cast<int>()));
        ((V1730*)digitizers.back())->calib();
        buffers.push_back(new Buffer(tbl["buffer_size"].cast<int>()*1024*1024));
        if (!digitizers.back()->program(*stngs)) return -1;
        if (stngs->getIndex() == run["arm_last"].cast<string>()) arm_last = i;
        // decoders need settings after programming
        decoders.push_back(new V1730Decoder((size_t)(nEvents*1.5),*stngs));
    }
    
    vector<RunTable> v1742s = db.getGroup("V1742");
    for (size_t i = 0; i < v1742s.size(); i++) {
        RunTable &tbl = v1742s[i];
        cout << "* V1742 - " << tbl.getIndex() << endl;
        V1742Settings *stngs = new V1742Settings(tbl,db);
        settings.push_back(stngs);
        digitizers.push_back(new V1742(bridge,tbl["base_address"].cast<int>()));
        buffers.push_back(new Buffer(tbl["buffer_size"].cast<int>()*1024*1024));
        if (!digitizers.back()->program(*stngs)) return -1;
        if (stngs->getIndex() == run["arm_last"].cast<string>()) arm_last = i;
        // decoders need settings after programming
        decoders.push_back(new V1742Decoder((size_t)(nEvents*1.5),*stngs)); 
    }
    
    cout << "Waiting for HV to stabilize..." << endl;
    
    while (running) {
        bool busy = false;
        bool warning = false;
        for (size_t i = 0; i < hvs.size(); i++) {
             busy |= hvs[i]->isBusy();
             warning  |= hvs[i]->isWarning();
        }
        if (!busy) break;
        if (warning) {
            cout << "HV reports issues, aborting run..." << endl;
            return -1;
        }
        usleep(1000000);
    }
    
    
    cout << "Starting acquisition..." << endl;
    
    pthread_mutex_t iomutex;
    pthread_cond_t newdata;
    pthread_mutex_init(&iomutex,NULL);
    pthread_cond_init(&newdata, NULL);
    vector<uint32_t> temps;
    
    for (size_t i = 0; i < digitizers.size(); i++) {
        if (i == arm_last) continue;
        digitizers[i]->startAcquisition();
    }
    digitizers[arm_last]->startAcquisition();
    
    decode_thread_data data;
    data.buffers = &buffers;
    data.decoders = &decoders;
    data.iomutex = &iomutex;
    data.newdata = &newdata;
    data.nEvents = nEvents;
    data.nRepeat = nRepeat;
    data.outfile = outfile;
    pthread_t decode;
    pthread_create(&decode,NULL,&decode_thread,&data);
    
    struct timespec last_temp_time, cur_time;
    clock_gettime(CLOCK_MONOTONIC,&last_temp_time);
    
    try { 
        while (running) {
        
            //Digitizer loop
            for (size_t i = 0; i < digitizers.size() && running; i++) {
                Digitizer *dgtz = digitizers[i];
                if (dgtz->readoutReady()) {
                    buffers[i]->inc(dgtz->readoutBLT(buffers[i]->wptr(),buffers[i]->free()));
                    pthread_cond_signal(&newdata);
                }
                if (!dgtz->acquisitionRunning()) {
                    pthread_mutex_lock(&iomutex);
                    cout << "Digitizer " << settings[i]->getIndex() << " aborted acquisition!" << endl;
                    pthread_mutex_unlock(&iomutex);
                    running = false;
                }
            }
            
            //Temperature check
            clock_gettime(CLOCK_MONOTONIC,&cur_time);
            if (cur_time.tv_sec-last_temp_time.tv_sec > temptime) {
                pthread_mutex_lock(&iomutex);
                last_temp_time = cur_time;
                cout << "Temperature check..." << endl;
                bool overtemp = false;
                for (size_t i = 0; i < digitizers.size() && running; i++) {
                    overtemp |= digitizers[i]->checkTemps(temps,60);
                    cout << settings[i]->getIndex() << " : [ " << temps[0];
                    for (size_t t = 1; t < temps.size(); t++) cout << ", " << temps[t];
                    cout << " ]" << endl;
                }
                if (overtemp) {
                    cout << "Overtemp! Aborting readout." << endl;
                    running  = false;
                }
                pthread_mutex_unlock(&iomutex);
            }
        } 
    } catch (exception &e) {
        running = false;
        pthread_mutex_unlock(&iomutex);
        pthread_mutex_lock(&iomutex);
        cout << "Readout thread aborted: " << e.what() << endl;
        pthread_mutex_unlock(&iomutex);
    }
    
    pthread_mutex_lock(&iomutex);
    cout << "Stopping acquisition..." << endl;
    pthread_mutex_unlock(&iomutex);
    
    digitizers[arm_last]->stopAcquisition();
    for (size_t i = 0; i < digitizers.size(); i++) {
        if (i == arm_last) continue;
        digitizers[i]->stopAcquisition();
    }
    pthread_cond_signal(&newdata);
    
    // Should add some logic to cleanup memory, but we're done anyway
    
    pthread_exit(NULL);

}
