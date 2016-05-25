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
#include <fstream>

#include "RunDB.hh"
#include "VMEBridge.hh"
#include "V1730_dpppsd.hh"
#include "V1742.hh"
#include "V65XX.hh"
#include "LeCroy6Zi.hh"

using namespace std;
using namespace H5;


bool stop;
bool readout_running;
bool decode_running;

class RunType {
    public:
        //called just before readout begins
        virtual void begin() = 0;
        
        //called after each read to determine if we need to write files
        //evtsReady functions as input and output
        //  IN = number of decoded events from each decoder
        // OUT = number of decoded events to write to file if result is true
        virtual bool writeout(std::vector<size_t> &evtsReady) = 0;
        
        //called before writing to modify the output filename
        virtual string fname() = 0;
        
        //add any runtype metadata to output file
        virtual void write(H5File &file) = 0;
        
        //called after data is written to add more data or prepare for next file
        virtual bool keepgoing() = 0;
        
};

// Gets fixed numbers of events, optionally splitting into multiple files (repeating)
// nEvents is the number of events to grab for all cards (0 for continuious run)
// nRepeat is the number of times to repeat (0 for once, 1 for once, N for multiple)
class NEventsRun : public RunType {
    protected:
        string basename;
        size_t nEvents, nRepeat, curCycle;
        double total;
        struct timespec cur_time, last_time;
        
    public: 
        NEventsRun(string _basename, size_t _nEvents, size_t _nRepeat = 0) : 
            basename(_basename),
            nEvents(_nEvents), 
            nRepeat(_nRepeat), 
            curCycle(0) { }
        
        virtual ~NEventsRun() {
        }
        
        virtual void begin() {
            clock_gettime(CLOCK_MONOTONIC,&last_time);
        }
        
        virtual bool writeout(std::vector<size_t> &evtsReady) {
            total = 0.0;
            bool writeout = nEvents > 0;
            for (size_t i = 0; i < evtsReady.size(); i++) {
                total += evtsReady[i];
                if (evtsReady[i] < nEvents) writeout = false;
            }
            total /= evtsReady.size();
            
            if (nRepeat) cout << "Cycle " << curCycle+1 << " / " << nRepeat << endl;
            
            clock_gettime(CLOCK_MONOTONIC,&cur_time);
            double time_int = (cur_time.tv_sec - last_time.tv_sec)+1e-9*(cur_time.tv_nsec - last_time.tv_nsec);
            cout << "Avg rate " << total/time_int << " Hz" << endl;
            
            return writeout;
        }
        
        virtual string fname() {
            if (nRepeat > 0) {
                return basename + "." + to_string(curCycle);
            } else {
                return basename;
            }
        }
        
        virtual void write(H5File &file) {
            double time_int = (cur_time.tv_sec - last_time.tv_sec)+1e-9*(cur_time.tv_nsec - last_time.tv_nsec);
            
            DataSpace scalar(0,NULL);
            Group root = file.openGroup("/");
            
            Attribute runtime = root.createAttribute("file_runtime",PredType::NATIVE_DOUBLE,scalar);
            runtime.write(PredType::NATIVE_DOUBLE,&time_int);
            
            uint32_t timestamp = time(NULL);
            Attribute tstampattr = root.createAttribute("creation_time",PredType::NATIVE_UINT32,scalar);
            tstampattr.write(PredType::NATIVE_UINT32,&timestamp);

			last_time = cur_time;
        }
        
        virtual bool keepgoing() {
            if (nRepeat > 0) {
                last_time = cur_time;
                curCycle++;
                return curCycle != nRepeat;
            } else {
                return false;
            }
        }
};

//Runs for a specified amount of time (seconds) splitting files by a certain 
//number of events (0 means all one file - but watch your buffers). 
//Assumes event rate is faster than runtime, i.e. the acquisition will stop on 
//the first event after time runs out.
class TimedRun : public RunType {
    protected:
        string basename;
        size_t runtime, evtsPerFile, curCycle;
        double total;
        struct timespec cur_time, last_time, begin_time;
        
    public: 
        TimedRun(string _basename, size_t _runtime, size_t _evtsPerFile) : 
            basename(_basename),
            runtime(_runtime), 
            evtsPerFile(_evtsPerFile), 
            curCycle(0) { }
        
        virtual ~TimedRun() {
        }
        
        virtual void begin() {
            clock_gettime(CLOCK_MONOTONIC,&begin_time);
            clock_gettime(CLOCK_MONOTONIC,&last_time);
        }
        
        virtual bool writeout(std::vector<size_t> &evtsReady) {
            total = 0.0;
            bool writeout = evtsPerFile > 0;
            for (size_t i = 0; i < evtsReady.size(); i++) {
                total += evtsReady[i];
                if (evtsReady[i] < evtsPerFile) writeout = false;
            }
            total /= evtsReady.size();
            
            if (evtsPerFile > 0) cout << "Cycle " << curCycle+1 << endl;
            
            clock_gettime(CLOCK_MONOTONIC,&cur_time);
            double time_int = (cur_time.tv_sec - last_time.tv_sec)+1e-9*(cur_time.tv_nsec - last_time.tv_nsec);
            cout << "Avg rate " << total/time_int << " Hz" << endl;
            
            time_int = (cur_time.tv_sec - begin_time.tv_sec)+1e-9*(cur_time.tv_nsec - begin_time.tv_nsec);
            if (time_int >= runtime) writeout = true;
            
            return writeout;
        }
        
        virtual string fname() {
            if (evtsPerFile > 0) {
                return basename + "." + to_string(curCycle);
            } else {
                return basename;
            }
        }
        
        virtual void write(H5File &file) {
            double time_int = (cur_time.tv_sec - last_time.tv_sec)+1e-9*(cur_time.tv_nsec - last_time.tv_nsec);
            
            DataSpace scalar(0,NULL);
            Group root = file.openGroup("/");
            
            Attribute runtime = root.createAttribute("file_runtime",PredType::NATIVE_DOUBLE,scalar);
            runtime.write(PredType::NATIVE_DOUBLE,&time_int);

            
            uint32_t timestamp = time(NULL);
            Attribute tstampattr = root.createAttribute("creation_time",PredType::NATIVE_UINT32,scalar);
            tstampattr.write(PredType::NATIVE_UINT32,&timestamp);


			last_time = cur_time;
        }
        
        virtual bool keepgoing() {
            if (evtsPerFile > 0) curCycle++;
            double time_int = (cur_time.tv_sec - begin_time.tv_sec)+1e-9*(cur_time.tv_nsec - begin_time.tv_nsec);
            return time_int < runtime;
        }
};

void int_handler(int x) {
    if (stop) exit(1);
    stop = true;
}

typedef struct {
    vector<Buffer*> *buffers;
    vector<Decoder*> *decoders;
    pthread_mutex_t *iomutex;
    pthread_cond_t *newdata;
    string config;
    RunType *runtype;
} decode_thread_data;

void *decode_thread(void *_data) {
    signal(SIGINT,int_handler);
    decode_thread_data* data = (decode_thread_data*)_data;
    
    vector<size_t> evtsReady(data->buffers->size());
    data->runtype->begin();
    try {
        decode_running = true;
        while (decode_running) {
            pthread_mutex_lock(data->iomutex);
            for (;;) {
                bool found = stop;
                for (size_t i = 0; i < data->buffers->size(); i++) {
                    found |= (*data->buffers)[i]->fill() > 0;
                }
                if (found) break;
                pthread_cond_wait(data->newdata,data->iomutex);
            }
            
            size_t total = 0;
            for (size_t i = 0; i < data->decoders->size(); i++) {
                size_t sz = (*data->buffers)[i]->fill();
                if (sz > 0) (*data->decoders)[i]->decode(*(*data->buffers)[i]);
                size_t ev = (*data->decoders)[i]->eventsReady();
                evtsReady[i] = ev;
                total += ev;
            }
            
            if (stop && total == 0) {
                decode_running = false;
            } else if (stop || data->runtype->writeout(evtsReady)) {
                Exception::dontPrint();
                
                string fname = data->runtype->fname() + ".h5"; 
                cout << "Saving data to " << fname << endl;
                
                H5File file(fname, H5F_ACC_TRUNC);
                data->runtype->write(file);
                  
                DataSpace scalar(0,NULL);
                Group root = file.openGroup("/");
               
                StrType configdtype(PredType::C_S1, data->config.size());
                Attribute config = root.createAttribute("run_config",configdtype,scalar);
                config.write(configdtype,data->config.c_str());
                
                int epochtime = time(NULL);
                Attribute timestamp = root.createAttribute("created_unix_timestamp",PredType::NATIVE_INT,scalar);
                timestamp.write(PredType::NATIVE_INT,&epochtime);
                
                for (size_t i = 0; i < data->decoders->size(); i++) {
                    (*data->decoders)[i]->writeOut(file,evtsReady[i]);
                }
                
                decode_running = data->runtype->keepgoing();
            }
            pthread_mutex_unlock(data->iomutex);
        }
        stop = true;
    } catch (runtime_error &e) {
        pthread_mutex_unlock(data->iomutex);
        stop = true;
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

    stop = false;
    readout_running = decode_running = false;
    signal(SIGINT,int_handler);

    cout << "Reading configuration..." << endl;
    
    RunDB db;
    db.addFile(argv[1]);
    RunTable run = db.getTable("RUN");
    
    const string runtypestr = run["runtype"].cast<string>();
    RunType *runtype = NULL;
    size_t eventBufferSize = 0;
    if (run.isMember("event_buffer_size")) {
        eventBufferSize = run["event_buffer_size"].cast<int>();
    } 
    if (runtypestr == "nevents") {
	    cout << "Setting up an event limited run..." << endl;
        const string outfile = run["outfile"].cast<string>();
        const int nEvents = run["events"].cast<int>();
        int nRepeat;
        if (run.isMember("repeat_times")) {
            nRepeat = run["repeat_times"].cast<int>();
        } else {
            nRepeat = 0;
        }
        runtype = new NEventsRun(outfile,nEvents,nRepeat);
        if (!eventBufferSize) eventBufferSize = (size_t)(nEvents*1.5);
    } else if (runtypestr == "timed") {
        cout << "Setting up a time limited run..." << endl;
        const string outfile = run["outfile"].cast<string>();
        int evtsPerFile;
        if (run.isMember("events_per_file")) {
            evtsPerFile = run["events_per_file"].cast<int>();
            if (evtsPerFile == 0) {
                cout << "Cannot do a timed run all in one file - events_per_file must be nonzero" << endl;
                return -1;
            }
        } else {
            evtsPerFile = 1000; //we need a well defined buffer amount
        }
        runtype = new TimedRun(outfile,run["runtime"].cast<int>(),evtsPerFile);
        if (!eventBufferSize) eventBufferSize = (size_t)(evtsPerFile*1.5);
    } 
    
    cout << "Using " << eventBufferSize << " event buffers." << endl;

    if (!runtype){
        cout << "Unknown runtype: " << runtypestr << endl;
        return -1;
    }
    
    //Every run has these options
    const int linknum = run["link_num"].cast<int>();
    const int temptime = run["check_temps_every"].cast<int>();
    bool config_only = false;
    if (run.isMember("config_only")) {
        config_only = run["config_only"].cast<bool>();
    }
    
    cout << "Grabbing V1742 calibration..." << endl;
    
    //This has to be done before using the CANEVME library due to bugs in the
    //CAENDigitizer library... so hack it in here.
    vector<RunTable> v1742s = db.getGroup("V1742");
    vector<V1742Settings*> v1742settings;
    vector<V1742calib*> v1742calibs;
    for (size_t i = 0; i < v1742s.size(); i++) {
        RunTable &tbl = v1742s[i];
        cout << "* V1742 - " << tbl.getIndex() << endl;
        V1742Settings *stngs = new V1742Settings(tbl,db);
        v1742settings.push_back(stngs);
        v1742calibs.push_back(V1742::staticGetCalib(stngs->sampleFreq(),run["link_num"].cast<int>(),tbl["base_address"].cast<int>()));
    }

    cout << "Opening VME link..." << endl;
    
    VMEBridge bridge(linknum,0);
    
    vector<V65XX*> hvs;
    vector<RunTable> v65XXs = db.getGroup("V65XX");
    if (v65XXs.size() > 0) cout << "Setting up V65XX HV..." << endl;
    for (size_t i = 0; i < v65XXs.size(); i++) {
        RunTable &tbl = v65XXs[i];
        cout << "\t" << tbl["index"].cast<string>() << endl;
        hvs.push_back(new V65XX(bridge,tbl["base_address"].cast<int>()));
        hvs.back()->set(tbl);
    }
    
    vector<LeCroy6Zi*> lescopes;
    vector<RunTable> lecroy6zis = db.getGroup("LECROY6ZI");
    if (lecroy6zis.size() > 0) cout << "Setting up LeCroy6Zi..." << endl;
    for (size_t i = 0; i < lecroy6zis.size(); i++) {
        RunTable &tbl = lecroy6zis[i];
        cout << "\t" << tbl["index"].cast<string>() << endl;
        lescopes.push_back(new LeCroy6Zi(tbl["host"].cast<string>(),tbl["port"].cast<int>(),tbl["timeout"].cast<double>()));
        lescopes.back()->reset();
        lescopes.back()->checklast();
        lescopes.back()->recall(tbl["load"].cast<int>());
        lescopes.back()->checklast();
    }
    
    cout << "Setting up digitizers..." << endl;
    
    vector<DigitizerSettings*> settings;
    vector<Digitizer*> digitizers;
    vector<Buffer*> buffers;
    vector<Decoder*> decoders;
    
    vector<RunTable> v1730s = db.getGroup("V1730");
    for (size_t i = 0; i < v1730s.size(); i++) {
        RunTable &tbl = v1730s[i];
        cout << "* V1730 - " << tbl.getIndex() << endl;
        V1730Settings *stngs = new V1730Settings(tbl,db);
        settings.push_back(stngs);
        digitizers.push_back(new V1730(bridge,tbl["base_address"].cast<int>()));
        ((V1730*)digitizers.back())->stopAcquisition();
        ((V1730*)digitizers.back())->calib();
        buffers.push_back(new Buffer(tbl["buffer_size"].cast<int>()*1024*1024));
        if (!digitizers.back()->program(*stngs)) return -1;
        // decoders need settings after programming
        decoders.push_back(new V1730Decoder(eventBufferSize,*stngs));
    }
    
    for (size_t i = 0; i < v1742s.size(); i++) {
        RunTable &tbl = v1742s[i];
        cout << "* V1742 - " << tbl.getIndex() << endl;
        V1742Settings *stngs = v1742settings[i];
        settings.push_back(stngs);
        V1742 *card = new V1742(bridge,tbl["base_address"].cast<int>());
        card->stopAcquisition();
        digitizers.push_back(card);
        buffers.push_back(new Buffer(tbl["buffer_size"].cast<int>()*1024*1024));
        if (!digitizers.back()->program(*stngs)) return -1;
        // decoders need settings after programming
        decoders.push_back(new V1742Decoder(eventBufferSize,v1742calibs[i],*stngs)); 
    }
    
    size_t arm_last = 0;
    for (size_t i = 0; i < digitizers.size(); i++) {
        if (run.isMember("arm_last") && settings[i]->getIndex() == run["arm_last"].cast<string>()) 
            arm_last = i;
    }
    
    cout << "Waiting for HV to stabilize..." << endl;
    
    while (!stop) {
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
        if (run.isMember("soft_trig") && settings[i]->getIndex() == run["soft_trig"].cast<string>()) {
            cout << "Software triggering " << settings[i]->getIndex() << endl;
            digitizers[i]->softTrig();
        }
    }
    if (digitizers.size() > 0) {
        digitizers[arm_last]->startAcquisition();
        if (run.isMember("soft_trig") && settings[arm_last]->getIndex() == run["soft_trig"].cast<string>()) {
            cout << "Software triggering " << settings[arm_last]->getIndex() << endl;
            digitizers[arm_last]->softTrig();
        }
    }
    for (size_t i = 0; i < lescopes.size(); i++) {
        lescopes[i]->normal();
        delete lescopes[i]; //get rid of these until the acquisition is done
        lescopes[i] = NULL;
    }
    
    decode_thread_data data;
    data.buffers = &buffers;
    data.decoders = &decoders;
    data.iomutex = &iomutex;
    data.newdata = &newdata;
    data.runtype = runtype;
    { //copy entire config as-is to be saved in each file
        std::ifstream file(argv[1]);
        std::stringstream buf;
        buf << file.rdbuf();
        data.config = buf.str();
    }
    pthread_t decode;
    pthread_create(&decode,NULL,&decode_thread,&data);
    
    struct timespec last_temp_time, cur_time;
    clock_gettime(CLOCK_MONOTONIC,&last_temp_time);
    
    if (config_only) stop = true;

    try { 
        readout_running = true;
        while (readout_running && !stop) {
            //Digitizer loop
            for (size_t i = 0; i < digitizers.size() && !stop; i++) {
                Digitizer *dgtz = digitizers[i];
                if (dgtz->readoutReady()) {
                    buffers[i]->inc(dgtz->readoutBLT(buffers[i]->wptr(),buffers[i]->free()));
                    pthread_cond_signal(&newdata);
                }
                if (!dgtz->acquisitionRunning()) {
                    pthread_mutex_lock(&iomutex);
                    cout << "Digitizer " << settings[i]->getIndex() << " aborted acquisition!" << endl;
                    pthread_mutex_unlock(&iomutex);
                    stop = true;
                }
            }
            
            //Temperature check
            clock_gettime(CLOCK_MONOTONIC,&cur_time);
            if (cur_time.tv_sec-last_temp_time.tv_sec > temptime) {
                pthread_mutex_lock(&iomutex);
                last_temp_time = cur_time;
                cout << "Temperature check..." << endl;
                bool overtemp = false;
                for (size_t i = 0; i < digitizers.size() && !stop; i++) {
                    overtemp |= digitizers[i]->checkTemps(temps,60);
                    cout << settings[i]->getIndex() << " : [ " << temps[0];
                    for (size_t t = 1; t < temps.size(); t++) cout << ", " << temps[t];
                    cout << " ]" << endl;
                }
                if (overtemp) {
                    cout << "Overtemp! Aborting readout." << endl;
                    stop = true;
                }
                pthread_mutex_unlock(&iomutex);
            }
        } 
        readout_running = false;
    } catch (exception &e) {
        readout_running = false;
        pthread_mutex_unlock(&iomutex);
        pthread_mutex_lock(&iomutex);
        cout << "Readout thread aborted: " << e.what() << endl;
        pthread_mutex_unlock(&iomutex);
    }
    
    stop = true;
    pthread_mutex_lock(&iomutex);
    cout << "Stopping acquisition..." << endl;
    pthread_mutex_unlock(&iomutex);
    
    for (size_t i = 0; i < lecroy6zis.size(); i++) {
        RunTable &tbl = lecroy6zis[i];
        try { //want to make sure this doesn't ever cause a crash
            LeCroy6Zi *tmpscope = new LeCroy6Zi(tbl["host"].cast<string>(),tbl["port"].cast<int>(),tbl["timeout"].cast<double>());
            tmpscope->stop();
        } catch (runtime_error &e) {
            cout << "Could not stop scope! : " << e.what() << endl;
        }
    }
    if (digitizers.size() > 0) digitizers[arm_last]->stopAcquisition();
    for (size_t i = 0; i < digitizers.size(); i++) {
        if (i == arm_last) continue;
        digitizers[i]->stopAcquisition();
    }
    pthread_cond_signal(&newdata);
    
    //busy wait for all data to be written out
    while (decode_running) { sleep(1); }
    
    // Should add some logic to cleanup memory, but we're done anyway
    
    pthread_exit(NULL);

}
