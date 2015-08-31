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
 
#include <ctime>
#include <cmath>
#include <iostream>
#include <H5Cpp.h>
 
#include "DataHandler.hh"

using namespace std;
using namespace H5;

DataHandler::DataHandler(size_t nGrabs, size_t nRepeat, string outfile, V1730Settings &_settings) : settings(_settings) {
    init(nGrabs,nRepeat,outfile);
}

DataHandler::DataHandler(V1730Settings &_settings) : settings(_settings) {
    init(0,0,"none");
}

DataHandler::~DataHandler() {
    for (size_t i = 0; i < grabs.size(); i++) {
        delete [] grabs[i];
        delete [] baselines[i];
        delete [] qshorts[i];
        delete [] qlongs[i];
        delete [] times[i];
    }
}

bool DataHandler::decode(Buffer &buf) {
    struct timespec this_time;
    clock_gettime(CLOCK_MONOTONIC,&this_time);
    time_int = (this_time.tv_sec-last_time.tv_sec)+(this_time.tv_nsec-last_time.tv_nsec)*1e-9;
    last_time = this_time;
    
    for (size_t idx = 0; idx < grabbed.size(); idx++) lastgrabbed[idx] = grabbed[idx];
    
    decode_size = buf.fill();
    cout << "buffer fill: " << buf.fill() << " bytes / " << buf.pct() << "%" << endl;
    uint32_t *next = (uint32_t*)buf.rptr(), *start = (uint32_t*)buf.rptr();
    while ((size_t)((next = decode_board_agg(next)) - start + 1)*4 < decode_size) {
        
    }
    buf.dec(decode_size);
    
    for (size_t idx = 0; idx < grabbed.size(); idx++) lastgrabbed[idx] = grabbed[idx] - lastgrabbed[idx];
    
    if (nRepeat > 0) cout << "acquisition cycle: " << cycle+1 << " / " << nRepeat << '\t';
    cout << (nGrabs ? (grabbed[0]/nGrabs > 1.0 ? "100" : to_string((int)round(100.0*grabbed[0]/nGrabs)))+"%" : "") << endl; 
    cout << "decode " << decode_counter << " : " << decode_size << " bytes / " << decode_size/1024.0/time_int << " KiB/s" << endl;
    for (size_t i = 0; i < idx2chan.size(); i++) {
        cout << "\tch" << idx2chan[i] << "\tev: " << lastgrabbed[i] << " / " << lastgrabbed[i]/time_int << " Hz" << endl;
    }
    
    if (nGrabs) {
        bool done = true;
        for (size_t idx = 0; idx < grabbed.size(); idx++) {
            if (grabbed[idx] < nGrabs) done = false;
        }
        if (done) { 
            writeout();
            if (cycle+1 == nRepeat || nRepeat == 0) return false;
            for (size_t idx = 0; idx < grabbed.size(); idx++) {
                grabbed[idx] = 0;
            }
            cycle++;
        }
    }
    
    decode_counter++;
    
    return true;
}

void DataHandler::init(size_t nGrabs, size_t nRepeat, string outfile) {
    this->nGrabs = nGrabs;
    this->nRepeat = nRepeat;
    this->outfile = outfile;
    
    clock_gettime(CLOCK_MONOTONIC,&last_time);
    
    cycle = decode_counter = chanagg_counter = boardagg_counter = 0;
    
    for (size_t ch = 0; ch < 16; ch++) {
        if (settings.getEnabled(ch)) {
            chan2idx[ch] = nsamples.size();
            idx2chan[nsamples.size()] = ch;
            nsamples.push_back(settings.getRecordLength(ch));
            grabbed.push_back(0);
            lastgrabbed.push_back(0);
            if (nGrabs > 0) {
                grabs.push_back(new uint16_t[nGrabs*nsamples.back()]);
                baselines.push_back(new uint16_t[nGrabs]);
                qshorts.push_back(new uint16_t[nGrabs]);
                qlongs.push_back(new uint16_t[nGrabs]);
                times.push_back(new uint32_t[nGrabs]);
            }
        }
    }
    
}

uint32_t* DataHandler::decode_chan_agg(uint32_t *chanagg, uint32_t group) {
    const bool format_flag = chanagg[0] & 0x80000000;
    if (!format_flag) throw runtime_error("Channel format not found");
    
    chanagg_counter++;
    
    const uint32_t size = chanagg[0] & 0x7FFF;
    const uint32_t format = chanagg[1];
    const uint32_t samples = (format & 0xFFF)*8;
    
    /*
    //Metadata
    const bool dualtrace_enable = format & (1<<31);
    const bool charge_enable =format & (1<<30);
    const bool time_enable = format & (1<<29);
    const bool baseline_enable = format & (1<<28);
    const bool waveform_enable = format & (1<<27);
    const uint32_t extras = (format >> 24) & 0x7;
    const uint32_t analog_probe = (format >> 22) & 0x3;
    const uint32_t digital_probe_2 = (format >> 19) & 0x7;
    const uint32_t digital_probe_1 = (format >> 16) & 0x7;
    */
    
    const uint32_t idx0 = chan2idx.count(group * 2 + 0) ? chan2idx[group * 2 + 0] : 999;
    const uint32_t idx1 = chan2idx.count(group * 2 + 1) ? chan2idx[group * 2 + 1] : 999;
    
    for (uint32_t *event = chanagg+2; (size_t)(event-chanagg+1) < size; event += samples/2+3) {
        
        const bool oddch = event[0] & 0x80000000;
        const uint32_t idx = oddch ? idx1 : idx0;
        const uint32_t len = nsamples[idx];
        
        if (idx == 999) throw runtime_error("Received data for disabled channel (" + to_string(group*2+oddch?1:0) + ")");
        
        if (len != samples) throw runtime_error("Number of samples received does not match expected (" + to_string(idx2chan[idx]) + ")");
        
        if (nGrabs && grabbed[idx] < nGrabs) {
            
            const size_t ev = grabbed[idx]++;
            uint16_t *data = grabs[idx];
            
            for (uint32_t *word = event+1, sample = 0; sample < len; word++, sample+=2) {
                data[ev*len+sample+0] = *word & 0x3FFF;
                //uint8_t dp10 = (*word >> 14) & 0x1;
                //uint8_t dp20 = (*word >> 15) & 0x1;
                data[ev*len+sample+1] = (*word >> 16) & 0x3FFF;
                //uint8_t dp11 = (*word >> 30) & 0x1;
                //uint8_t dp21 = (*word >> 31) & 0x1;
            }
            
            baselines[idx][ev] = event[1+samples/2+0] & 0xFFFF;
            //uint16_t extratime = (event[1+samples/2+0] >> 16) & 0xFFFF;
            qshorts[idx][ev] = event[1+samples/2+1] & 0x7FFF;
            qlongs[idx][ev] = (event[1+samples/2+1] >> 16) & 0xFFFF;
            times[idx][ev] = event[0] & 0x7FFFFFFF;
            
        } else {
            grabbed[idx]++;
        }
    
    }
    
    return chanagg + size;
}

uint32_t* DataHandler::decode_board_agg(uint32_t *boardagg) {
    if (boardagg[0] == 0xFFFFFFFF) {
        boardagg++; //sometimes padded
    }
    if ((boardagg[0] & 0xF0000000) != 0xA0000000) 
        throw runtime_error("Board aggregate missing tag");
    
    boardagg_counter++;    
    
    uint32_t size = boardagg[0] & 0x0FFFFFFF;
    
    //const uint32_t board = (boardagg[1] >> 28) & 0xF;
    //const bool fail = boardagg[1] & (1 << 26);
    //const uint32_t pattern = (boardagg[1] >> 8) & 0x7FFF;
    const uint32_t mask = boardagg[1] & 0xFF;
    
    //const uint32_t count = boardagg[2] & 0x7FFFFF;
    //const uint32_t timetag = boardagg[3];
    
    uint32_t *chans = boardagg+4;
    
    for (uint32_t gr = 0; gr < 8; gr++) {
        if (mask & (1 << gr)) {
            chans = decode_chan_agg(chans,gr);
        }
    } 
    
    return boardagg+size;
}

void DataHandler::writeout() {
    Exception::dontPrint();

    string fname = outfile;
    if (nRepeat > 0) {
        fname += "." + to_string(cycle);
    }
    fname += ".h5"; 
    
    cout << "Saving data to " << fname << endl;
    
    H5File file(fname, H5F_ACC_TRUNC);
    
    for (size_t i = 0; i < nsamples.size(); i++) {
        cout << "Dumping channel " << idx2chan[i] << "... ";
    
        string groupname = "/ch" + to_string(idx2chan[i]);
        Group group = file.createGroup(groupname);
        
        cout << "Attributes, ";
        
        DataSpace scalar(0,NULL);
        
        double dval;
        uint32_t ival;
        
        Attribute bits = group.createAttribute("bits",PredType::NATIVE_UINT32,scalar);
        ival = 14;
        bits.write(PredType::NATIVE_INT32,&ival);
        
        Attribute ns_sample = group.createAttribute("ns_sample",PredType::NATIVE_DOUBLE,scalar);
        dval = 2.0;
        ns_sample.write(PredType::NATIVE_DOUBLE,&dval);
        
        Attribute offset = group.createAttribute("offset",PredType::NATIVE_UINT32,scalar);
        ival = settings.getDCOffset(idx2chan[i]);
        offset.write(PredType::NATIVE_UINT32,&ival);
        
        Attribute samples = group.createAttribute("samples",PredType::NATIVE_UINT32,scalar);
        ival = settings.getRecordLength(idx2chan[i]);
        offset.write(PredType::NATIVE_UINT32,&ival);
        
        Attribute presamples = group.createAttribute("presamples",PredType::NATIVE_UINT32,scalar);
        ival = settings.getPreSamples(idx2chan[i]);
        offset.write(PredType::NATIVE_UINT32,&ival);
        
        Attribute threshold = group.createAttribute("threshold",PredType::NATIVE_UINT32,scalar);
        ival = settings.getThreshold(idx2chan[i]);
        offset.write(PredType::NATIVE_UINT32,&ival);
        
        hsize_t dimensions[2];
        dimensions[0] = nGrabs;
        dimensions[1] = nsamples[i];
        
        DataSpace samplespace(2, dimensions);
        DataSpace metaspace(1, dimensions);
        
        cout << "Samples, ";
        DataSet samples_ds = file.createDataSet(groupname+"/samples", PredType::NATIVE_UINT16, samplespace);
        samples_ds.write(grabs[i], PredType::NATIVE_UINT16);
        
        cout << "Baselines, ";
        DataSet baselines_ds = file.createDataSet(groupname+"/baselines", PredType::NATIVE_UINT16, metaspace);
        baselines_ds.write(baselines[i], PredType::NATIVE_UINT16);
        
        cout << "QShorts, ";
        DataSet qshorts_ds = file.createDataSet(groupname+"/qshorts", PredType::NATIVE_UINT16, metaspace);
        qshorts_ds.write(qshorts[i], PredType::NATIVE_UINT16);
        
        cout << "QLongs, ";
        DataSet qlongs_ds = file.createDataSet(groupname+"/qlongs", PredType::NATIVE_UINT16, metaspace);
        qlongs_ds.write(qlongs[i], PredType::NATIVE_UINT16);

        cout << "Times";
        DataSet times_ds = file.createDataSet(groupname+"/times", PredType::NATIVE_UINT32, metaspace);
        times_ds.write(times[i], PredType::NATIVE_UINT32);
        
        cout << endl;
    }
}
