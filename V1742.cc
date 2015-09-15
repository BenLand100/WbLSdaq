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
 
#include <cmath>
#include <iostream>
#include <stdexcept>
 
#include "V1742.hh"

using namespace std;

V1742Settings::V1742Settings() : DigitizerSettings("") {
    //These are "do nothing" defaults  
    index = "DEFAULTS";
    card.tr_enable = 0; //1 bit tr enabled
    card.tr_readout = 0; //1 bit tr readout enabled
    card.tr_polarity = 0; //1 bit [positive,negative]
    card.tr0_threshold = 0x6666; //16 bit tr0 threshold
    card.tr1_threshold = 0x6666; //16 bit tr1 threshold
    card.tr0_dc_offset = 0x8000; //16 bit tr0 dc offset
    card.tr1_dc_offset = 0x8000; //16 bit tr1 dc offset
    card.custom_size = 0; //2 bit [1024, 520, 256, 136]
    card.sample_freq = 0; //2 bit [5, 2.5, 1]
    card.software_trigger_enable = 0; //1 bit bool
    card.external_trigger_enable = 0; //1 bit bool
    card.post_trigger = 0; //10 bit (8.5ns steps)
    for (uint32_t gr = 0; gr < 4; gr++) {
        groupDefaults(gr);
    }
    card.max_event_blt = 10; //8 bit events per transfer
}

V1742Settings::V1742Settings(RunTable &dgtz, RunDB &db) : DigitizerSettings(dgtz.getIndex()) {
    card.tr_enable = dgtz["tr_enabled"].cast<bool>() ? 1 : 0; //1 bit tr enabled
    card.tr_readout = dgtz["tr_readout"].cast<bool>() ? 1 : 0; //1 bit tr readout enabled
    card.tr_polarity = dgtz["tr_polarity"].cast<int>(); //1 bit [positive,negative]
    card.tr0_threshold = dgtz["tr0_threshold"].cast<int>(); //16 bit tr0 threshold
    card.tr1_threshold = dgtz["tr1_threshold"].cast<int>(); //16 bit tr1 threshold
    card.tr0_dc_offset = dgtz["tr0_dc_offset"].cast<int>(); //16 bit tr0 dc offset
    card.tr1_dc_offset = dgtz["tr1_dc_offset"].cast<int>(); //16 bit tr1 dc offset
    card.custom_size = dgtz["num_samples"].cast<int>(); //2 bit [1024, 520, 256, 136]
    card.sample_freq = dgtz["sample_freq"].cast<int>(); //2 bit [5, 2.5, 1]
    card.software_trigger_enable = dgtz["software_trigger"].cast<bool>() ? 1 : 0; //1 bit bool
    card.external_trigger_enable = dgtz["external_trigger"].cast<bool>() ? 1 : 0; //1 bit bool
    card.post_trigger = dgtz["trigger_offset"].cast<int>(); //10 bit (8.5ns steps)
    for (uint32_t gr = 0; gr < 4; gr++) {
        string grname = "GR"+to_string(gr);
        if (!db.tableExists(grname,index)) {
            groupDefaults(gr);
        } else {
            RunTable group = db.getTable(grname,index);
            card.group_enable[gr] = group["enabled"].cast<bool>() ? 1 : 0; //1 bit bool
            vector<double> offsets = group["dc_offsets"].toVector<double>();
            if (offsets.size() != 8) throw runtime_error("Group DC offsets expected to be length 8");
            for (uint32_t ch = 0; ch < 8; ch++) {
                card.dc_offset[ch+gr*8] = round((offsets[ch]+1.0)/2.0*pow(2.0,16.0)); //16 bit channel offsets
            }  
        }
    }
    card.max_event_blt = 10; //8 bit events per transfer
    
}

V1742Settings::~V1742Settings() {

}

void V1742Settings::groupDefaults(uint32_t gr) {
    card.group_enable[gr] = 0; //1 bit bool
    for (uint32_t ch = 0; ch < 8; ch++) {
        card.dc_offset[ch+gr*8] = 0x8000; //16 bit channel offsets
    }
}
        
void V1742Settings::validate() {
    if (card.tr_enable & (~0x1)) throw runtime_error("tr_enable must be 1 bit");
    if (card.tr_readout & (~0x1)) throw runtime_error("tr_readout must be 1 bit");
    if (card.tr_polarity & (~0x1)) throw runtime_error("tr_polarity must be 1 bit");
    if (card.custom_size > 3) throw runtime_error("custom_size must be < 4");
    if (card.sample_freq > 2) throw runtime_error("tr_polarity must be < 3");
    if (card.software_trigger_enable & (~0x1)) throw runtime_error("software_trigger_enable must be 1 bit");
    if (card.external_trigger_enable & (~0x1)) throw runtime_error("external_trigger_enable must be 1 bit");
    if (card.post_trigger > 1023) throw runtime_error("post_trigger must be < 1024");
    for (uint32_t gr = 0; gr < 4; gr++) {
        if (card.group_enable[gr] & (~0x1)) throw runtime_error("external_trigger_enable must be 1 bit");
    }

}

V1742::V1742(VMEBridge &_bridge, uint32_t _baseaddr) : Digitizer(_bridge,_baseaddr) {

}

V1742::~V1742() {
    //Fully reset the board just in case
    write32(REG_BOARD_CONFIGURATION_RELOAD,0);
}

bool V1742::program(DigitizerSettings &_settings) {
    V1742Settings &settings = dynamic_cast<V1742Settings&>(_settings);
    try {
        settings.validate();
    } catch (runtime_error &e) {
        cout << "Could not program V1742: " << e.what() << endl;
        return false;
    }
    
    uint32_t data;
    
    //Fully reset the board just in case
    write32(REG_BOARD_CONFIGURATION_RELOAD,0);
    
    usleep(10000);
    
    //Set TTL logic levels, ignore LVDS and debug settings
    write32(REG_FRONT_PANEL_CONTROL,1);
    
    write32(REG_TR_THRESHOLD|(0<<8),settings.card.tr0_threshold);
    write32(REG_TR_THRESHOLD|(2<<8),settings.card.tr1_threshold);
    write32(REG_TR_DC_OFFSET|(0<<8),settings.card.tr0_dc_offset);
    write32(REG_TR_DC_OFFSET|(2<<8),settings.card.tr1_dc_offset);
    
    data = (((uint32_t)settings.card.tr_enable)<<12)
         | (((uint32_t)settings.card.tr_readout)<<11)
         | (1<<8)
         | (((uint32_t)settings.card.tr_polarity)<<6)
         | (1<<4);
    write32(REG_GROUP_CONFIG,data);
    
    write32(REG_CUSTOM_SIZE,settings.card.custom_size);
    write32(REG_SAMPLE_FREQ,settings.card.sample_freq);
    write32(REG_POST_TRIGGER,settings.card.post_trigger);
    
    data = (((uint32_t)settings.card.software_trigger_enable)<<31)
         | (((uint32_t)settings.card.external_trigger_enable)<<30);
    write32(REG_TRIGGER_SOURCE,data);
    
    uint32_t group_enable = 0;
    for (uint32_t gr = 0; gr < 4; gr++) {
        group_enable |= settings.card.group_enable[gr]<<gr;
        for (uint32_t ch = 0; ch < 8; ch++) {
            data = (ch<<16) | ((uint32_t)settings.card.dc_offset[ch+gr*8]);
            write32(REG_DC_OFFSET|(gr<<8),data);
        }
    }
    write32(REG_GROUP_ENABLE,group_enable);
    
    //Set max board aggregates to transver per readout
    write32(REG_MAX_EVENT_BLT,settings.card.max_event_blt);
    
    //Enable VME BLT readout
    write32(REG_READOUT_CONTROL,1<<4);
    
    return true;
}

void V1742::startAcquisition() {
    write32(REG_ACQUISITION_CONTROL,(1<<3)|(1<<2));
}

void V1742::stopAcquisition() {
    write32(REG_ACQUISITION_CONTROL,0);
}

bool V1742::acquisitionRunning() {
    return read32(REG_ACQUISITION_STATUS) & (1 << 2);
}

bool V1742::readoutReady() {
    return read32(REG_ACQUISITION_STATUS) & (1 << 3);
}

bool V1742::checkTemps(vector<uint32_t> &temps, uint32_t danger) {
    temps.resize(4);
    bool over = false;
    for (int gr = 0; gr < 4; gr++) {
        temps[gr] = read32(REG_DRS4_TEMP|(gr<<8))&0xFF;
        if (temps[gr] >= danger) over = true;
    }
    return over;
}


V1742Decoder::V1742Decoder(size_t _eventBuffer, V1742Settings &_settings) : eventBuffer(_eventBuffer), settings(_settings) {

    group_counter = event_counter = decode_counter = 0;
    
    nSamples = settings.getNumSamples();
    for (size_t gr = 0; gr < 4; gr++) {
        if (settings.getGroupEnabled(gr)) {
            grActive[gr] = true;
            grGrabbed[gr] = 0;
            if (eventBuffer) {
                for (size_t ch = 0; ch < 8; ch++) {
                    samples[gr][ch] = new uint16_t[eventBuffer*nSamples];
                }
                start_index[gr] = new uint16_t[eventBuffer];
                trigger_count[gr] = new uint32_t[eventBuffer];
                trigger_time[gr] = new uint32_t[eventBuffer];
            }
        } else {
            grActive[gr] = false;
        }
    }
    
    if (settings.getTRReadout()) {
        trActive[0] = trActive[1] = true;
        if (eventBuffer) {
            tr_samples[0] = new uint16_t[eventBuffer*nSamples];
            tr_samples[1] = new uint16_t[eventBuffer*nSamples];
        }
    } else {
        trActive[0] = trActive[1] = false;
    }
    
    clock_gettime(CLOCK_MONOTONIC,&last_decode_time);
    
}

V1742Decoder::~V1742Decoder() {
    if (eventBuffer) {
        for (size_t gr = 0; gr < 4; gr++) {
            if (grActive[gr]) {
                for (size_t ch = 0; ch < 8; ch++) {
                    delete [] samples[gr][ch];
                }
                delete [] start_index[gr];
                delete [] trigger_count[gr];
                delete [] trigger_time[gr];
            }
        }
        if (trActive[0]) delete [] tr_samples[0];
        if (trActive[1]) delete [] tr_samples[1];
    }
}

void V1742Decoder::decode(Buffer &buffer) {
    size_t lastgrabbed[4]; 
    for (size_t gr = 0; gr < 4; gr++) lastgrabbed[gr] = grGrabbed[gr];
    
    decode_size = buffer.fill();
    cout << settings.getIndex() << " decoding " << decode_size << " bytes." << endl;
    uint32_t *next = (uint32_t*)buffer.rptr(), *start = (uint32_t*)buffer.rptr();
    while ((size_t)((next = decode_event_structure(next)) - start + 1)*4 < decode_size);
    buffer.dec(decode_size);
    decode_counter++;
    
    struct timespec cur_time;
    clock_gettime(CLOCK_MONOTONIC,&cur_time);
    double time_int = (cur_time.tv_sec - last_decode_time.tv_sec)+1e-9*(cur_time.tv_nsec - last_decode_time.tv_nsec);
    last_decode_time = cur_time;
    
    for (size_t gr = 0; gr < 4; gr++) {
        if (grActive[gr]) cout << "\tgr" << gr << "\tev: " << grGrabbed[gr]-lastgrabbed[gr] << " / " << (grGrabbed[gr]-lastgrabbed[gr])/time_int << " Hz / " << grGrabbed[gr] << " total " << endl;
    }
}
    
uint32_t* V1742Decoder::decode_event_structure(uint32_t *event) {
    if (event[0] == 0xFFFFFFFF) {
        event++; //sometimes padded
    }
    if ((event[0] & 0xF0000000) != 0xA0000000) 
        throw runtime_error("Event structure missing tag");
    
    uint32_t size = event[0] & 0xFFFFFFF;
    
    /*
    uint32_t board_id = (event[1] & 0xF800000000) >> 27;
    uint32_t pattern = (event[1] & 0x3FFF00) >> 8;
    */
    
    uint32_t mask = event[1] & 0xF;
    uint32_t count = event[2] & 0x3FFFFF;
    uint32_t timetag = event[3];
    
    if (event_counter++) {
        if (count == trigger_last) {
            cout << "****" << settings.getIndex() << " duplicate trigger " << count << endl;
        } else if (count < trigger_last) {
            cout << "****" << settings.getIndex() << " orphaned trigger " << count << endl;
        } else if (count != trigger_last + 1) { 
            cout << "****" << settings.getIndex() << " missed " << count-trigger_last-1 << " triggers" << endl;
            trigger_last = count;
        } else {
            trigger_last = count;
        }
    } else {
        trigger_last = count;
    }
    
    uint32_t *groups = event+4;
    
    for (uint32_t gr = 0; gr < 4; gr++) {
        if (mask & (1 << gr)) {
            if (eventBuffer) {
                size_t ev = grGrabbed[gr]++;
                if (ev == eventBuffer) throw runtime_error("Decoder buffer for " + settings.getIndex() + " overflowed!");
                trigger_time[gr][ev] = timetag;
                trigger_count[gr][ev] = count;
            }
            groups = decode_group_structure(groups,gr);
        }
    } 
    
    return event+size;

}

uint32_t* V1742Decoder::decode_group_structure(uint32_t *group, uint32_t gr) {

    if (!grActive[gr]) throw runtime_error("Recieved group data for inactive group (" + to_string(gr) + ")");
    
    uint32_t cell_index = (group[0] & 0x3FF00000) >> 20;
    //uint32_t freq = (group[0] >> 16) & 0x3;
    
    uint32_t tr = (group[0] >> 12) & 0x1;
    uint32_t size = group[0] & 0xFFF;
    
    if (size/3 != nSamples) throw runtime_error("Recieved sample length " + to_string(size/3) + " does not match expected " + to_string(nSamples) + " (" + to_string(gr) + ")");
    if (tr && !trActive[gr<3 ? 0 : 1]) throw runtime_error("Received TR"+to_string(gr<3 ? 0 : 1)+" data when not marked for readout (" + to_string(gr) + ")");
    
    group_counter++;
    
    if (eventBuffer) {
        size_t ev = grGrabbed[gr]-1;
        
        start_index[gr][ev] = cell_index;
        
        uint32_t *word = group+1;
        uint16_t *data[8];
        for (size_t ch = 0; ch < 8; ch++) data[ch] = samples[gr][ch] + ev*nSamples;
        for (size_t s = 0; s < nSamples; s++, word += 3) {
            data[0][s] = word[0]&0xFFF;
            data[1][s] = (word[0]>>12)&0xFFF;
            data[2][s] = ((word[1]&0xF)<<8)|((word[0]>>24)&0xFF);
            data[3][s] = (word[1]>>4)&0xFFF;
            data[4][s] = (word[1]>>16)&0xFFF;
            data[5][s] = ((word[2]&0xFF)<<4)|((word[1]>>28)&0xF);
            data[6][s] = (word[2]>>8)&0xFFF;
            data[7][s] = (word[2]>>20)&0xFFF;
        }
        
        if (tr) {
            uint16_t *tr = tr_samples[gr<3 ? 0 : 1] + ev*nSamples;
            for (size_t s = 0; s < nSamples; word += 3) {
                tr[s++] = word[0]&0xFFF;
                tr[s++] = (word[0]>>12)&0xFFF;
                tr[s++] = ((word[1]&0xF)<<8)|((word[0]>>24)&0xFF);
                tr[s++] = (word[1]>>4)&0xFFF;
                tr[s++] = (word[1]>>16)&0xFFF;
                tr[s++] = ((word[2]&0xFF)<<4)|((word[1]>>28)&0xF);
                tr[s++] = (word[2]>>8)&0xFFF;
                tr[s++] = (word[2]>>20)&0xFFF;
            }
        }
        
    } else {
        grGrabbed[gr]++;
    }
    
    return group + 2 + size + (tr ? size/8 : 0);
    
}

size_t V1742Decoder::eventsReady() {
    size_t grabs = eventBuffer;
    for (size_t gr = 0; gr < 4; gr++) {
        if (grActive[gr] && grGrabbed[gr] < grabs) grabs = grGrabbed[gr];
    }
    return grabs;
}

using namespace H5;

void V1742Decoder::writeOut(H5File &file, size_t nEvents) {

    cout << "\t/" << settings.getIndex() << endl;

    Group cardgroup = file.createGroup("/"+settings.getIndex());
        
    DataSpace scalar(0,NULL);
    
    double dval;
    uint32_t ival;
    
    Attribute bits = cardgroup.createAttribute("bits",PredType::NATIVE_UINT32,scalar);
    ival = 12;
    bits.write(PredType::NATIVE_INT32,&ival);
    
    Attribute ns_sample = cardgroup.createAttribute("ns_sample",PredType::NATIVE_DOUBLE,scalar);
    dval = settings.nsPerSample();
    ns_sample.write(PredType::NATIVE_DOUBLE,&dval);
            
    Attribute _samples = cardgroup.createAttribute("samples",PredType::NATIVE_UINT32,scalar);
    ival = nSamples;
    _samples.write(PredType::NATIVE_UINT32,&ival);
    
    for (size_t gr = 0; gr < 4; gr++) {
        if (!grActive[gr]) continue;
        string grname = "gr" + to_string(gr);
        Group grgroup = cardgroup.createGroup(grname);
        string grgroupname = "/"+settings.getIndex()+"/"+grname;
        
        cout << "\t" << grgroupname << endl;

        hsize_t dimensions[2];
        dimensions[0] = nEvents;
        dimensions[1] = nSamples;
        
        DataSpace samplespace(2, dimensions);
        DataSpace metaspace(1, dimensions);
        
        for (size_t ch = 0; ch < 8; ch++) {
            string chname = "ch" + to_string(ch);
            Group chgroup = grgroup.createGroup(chname);
            string chgroupname = "/"+settings.getIndex()+"/"+grname+"/"+chname;
            
            cout << "\t" << chgroupname << endl;
        
            Attribute offset = chgroup.createAttribute("offset",PredType::NATIVE_UINT32,scalar);
            ival = settings.getDCOffset(gr*8+ch);
            offset.write(PredType::NATIVE_UINT32,&ival);
            
            cout << "\t" << chgroupname << "/samples" << endl;
            DataSet samples_ds = file.createDataSet(chgroupname+"/samples", PredType::NATIVE_UINT16, samplespace);
            samples_ds.write(samples[gr][ch], PredType::NATIVE_UINT16);
            memcpy(samples[gr][ch],samples[gr][ch]+nEvents,sizeof(uint16_t)*(grGrabbed[gr]-nEvents));
            
        }
            
        cout << "\t" << grgroupname << "/start_index" << endl;
        DataSet start_index_ds = file.createDataSet(grgroupname+"/start_index", PredType::NATIVE_UINT16, metaspace);
        start_index_ds.write(start_index[gr], PredType::NATIVE_UINT16);
        memcpy(start_index[gr],start_index[gr]+nEvents,sizeof(uint16_t)*(grGrabbed[gr]-nEvents));
            
        cout << "\t" << grgroupname << "/trigger_time" << endl;
        DataSet trigger_time_ds = file.createDataSet(grgroupname+"/trigger_time", PredType::NATIVE_UINT32, metaspace);
        trigger_time_ds.write(trigger_time[gr], PredType::NATIVE_UINT32);
        memcpy(trigger_time[gr],trigger_time[gr]+nEvents,sizeof(uint32_t)*(grGrabbed[gr]-nEvents));
        
        cout << "\t" << grgroupname << "/trigger_count" << endl;
        DataSet trigger_count_ds = file.createDataSet(grgroupname+"/trigger_count", PredType::NATIVE_UINT32, metaspace);
        trigger_count_ds.write(trigger_count[gr], PredType::NATIVE_UINT32);
        memcpy(trigger_count[gr],trigger_count[gr]+nEvents,sizeof(uint32_t)*(grGrabbed[gr]-nEvents));
        
        grGrabbed[gr] -= nEvents;
    }
}
