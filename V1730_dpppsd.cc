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
 
#include <stdexcept>
 
#include "V1730_dpppsd.hh"

V1730Settings::V1730Settings() {
    //These are "do nothing" defaults
    card.dual_trace = 0; // 1 bit
    card.analog_probe = 0; // 2 bit (see docs)
    card.oscilloscope_mode = 1; // 1 bit
    card.digital_virt_probe_1 = 0; // 3 bit (see docs)
    card.digital_virt_probe_2 = 0; // 3 bit (see docs)
    card.coincidence_window = 1; // 3 bit
    card.global_majority_level = 0; // 3 bit
    card.external_global_trigger = 0; // 1 bit
    card.software_global_trigger = 0; // 1 bit
    card.out_logic = 0; // 2 bit (OR,AND,MAJORITY)
    card.out_majority_level = 0; // 3 bit
    card.external_trg_out = 0; // 1 bit
    card.software_trg_out = 0; // 1 bit
    card.max_board_agg_blt = 5;
    
    for (uint32_t ch = 0; ch < 16; ch++) {
        chanDefaults(ch);
    }
    
}

V1730Settings::V1730Settings(map<string,json::Value> &db) {
    
    json::Value &digitizer = db["DIGITIZER[]"];
    
    card.dual_trace = 0; // 1 bit
    card.analog_probe = 0; // 2 bit (see docs)
    card.oscilloscope_mode = 1; // 1 bit
    card.digital_virt_probe_1 = 0; // 3 bit (see docs)
    card.digital_virt_probe_2 = 0; // 3 bit (see docs)
    card.coincidence_window = 1; // 3 bit
    
    card.global_majority_level = digitizer["global_majority_level"].cast<int>(); // 3 bit
    card.external_global_trigger = digitizer["external_trigger_enable"].cast<bool>() ? 1 : 0; // 1 bit
    card.software_global_trigger = digitizer["software_trigger_enable"].cast<bool>() ? 1 : 0; // 1 bit
    card.out_logic = digitizer["trig_out_logic"].cast<int>(); // 2 bit (OR,AND,MAJORITY)
    card.out_majority_level = digitizer["trig_out_majority_level"].cast<int>(); // 3 bit
    card.external_trg_out = digitizer["external_trigger_out"].cast<bool>() ? 1 : 0; // 1 bit
    card.software_trg_out = digitizer["external_trigger_out"].cast<bool>() ? 1 : 0; // 1 bit
    card.max_board_agg_blt = digitizer["aggregates_per_transfer"].cast<int>(); 
    
    for (int ch = 0; ch < 16; ch++) {
        string chname = "CH["+to_string(ch)+"]";
        if (db.find(chname) == db.end()) {
            chanDefaults(ch);
        } else {
            json::Value &channel = db[chname];
    
            chans[ch].dynamic_range = 0; // 1 bit
            chans[ch].fixed_baseline = 0; // 12 bit
            
            chans[ch].enabled = channel["enabled"].cast<bool>() ? 1 : 0; //1 bit
            chans[ch].global_trigger = channel["request_global_trigger"].cast<bool>() ? 1 : 0; // 1 bit
            chans[ch].trg_out = channel["request_trig_out"].cast<bool>() ? 1 : 0; // 1 bit
            chans[ch].record_length = channel["record_length"].cast<int>(); // 16* bit
            chans[ch].dc_offset = round((channel["dc_offset"].cast<double>()+1.0)/2.0*pow(2.0,16.0)); // 16 bit (-1V to 1V)
            chans[ch].baseline_mean = channel["baseline_average"].cast<int>(); // 3 bit (fixed,16,64,256,1024)
            chans[ch].pulse_polarity = channel["pulse_polarity"].cast<int>(); // 1 bit (0->positive, 1->negative)
            chans[ch].trg_threshold =  channel["trigger_threshold"].cast<int>();// 12 bit
            chans[ch].self_trigger = channel["self_trigger"].cast<bool>() ? 0 : 1; // 1 bit (0->enabled, 1->disabled)
            chans[ch].pre_trigger = channel["pre_trigger"].cast<int>(); // 9* bit
            chans[ch].gate_offset = channel["gate_offset"].cast<int>(); // 8 bit
            chans[ch].long_gate = channel["long_gate"].cast<int>(); // 12 bit
            chans[ch].short_gate = channel["short_gate"].cast<int>(); // 12 bit
            chans[ch].charge_sensitivity = channel["charge_sensitivity"].cast<int>(); // 3 bit (see docs)
            chans[ch].shaped_trigger_width = channel["shaped_trigger_width"].cast<int>(); // 10 bit
            chans[ch].trigger_holdoff = channel["trigger_holdoff"].cast<int>(); // 10* bit
            chans[ch].trigger_config = channel["trigger_type"].cast<int>(); // 2 bit (see docs)
            chans[ch].ev_per_buffer = channel["events_per_buffer"].cast<int>(); // 10 bit
        }
    }
}

V1730Settings::~V1730Settings() {

}
        
void V1730Settings::validate() { //FIXME validate bit fields too
    for (int ch = 0; ch < 16; ch++) {
        if (ch % 2 == 0) {
            if (chans[ch].record_length > 65535) throw runtime_error("Number of samples exceeds 65535 (ch " + to_string(ch) + ")");
        } else {
            if (chans[ch].record_length != chans[ch-1].record_length) throw runtime_error("Record length is not the same between pairs (ch " + to_string(ch) + ")"); 
        }
        if (chans[ch].ev_per_buffer < 2) throw runtime_error("Number of events per channel buffer must be at least 2 (ch " + to_string(ch) + ")");
        if (chans[ch].ev_per_buffer > 1023) throw runtime_error("Number of events per channel buffer exceeds 1023 (ch " + to_string(ch) + ")");
        if (chans[ch].pre_trigger > 2044) throw runtime_error("Pre trigger samples exceeds 2044 (ch " + to_string(ch) + ")");
        if (chans[ch].short_gate > 4095) throw runtime_error("Short gate samples exceeds 4095 (ch " + to_string(ch) + ")");
        if (chans[ch].long_gate > 4095) throw runtime_error("Long gate samples exceeds 4095 (ch " + to_string(ch) + ")");
        if (chans[ch].gate_offset > 255) throw runtime_error("Gate offset samples exceeds 255 (ch " + to_string(ch) + ")");
        if (chans[ch].pre_trigger < chans[ch].gate_offset + 19) throw runtime_error("Gate offset and pre_trigger relationship violated (ch " + to_string(ch) + ")");
        if (chans[ch].trg_threshold > 4095) throw runtime_error("Trigger threshold exceeds 4095 (ch " + to_string(ch) + ")");
        if (chans[ch].fixed_baseline > 4095) throw runtime_error("Fixed baseline exceeds 4095 (ch " + to_string(ch) + ")");
        if (chans[ch].shaped_trigger_width > 1023) throw runtime_error("Shaped trigger width exceeds 1023 (ch " + to_string(ch) + ")");
        if (chans[ch].trigger_holdoff > 4092) throw runtime_error("Trigger holdoff width exceeds 4092 (ch " + to_string(ch) + ")");
        if (chans[ch].dc_offset > 65535) throw runtime_error("DC Offset cannot exceed 65535 (ch " + to_string(ch) + ")");
    }
}

void V1730Settings::chanDefaults(uint32_t ch) {
    chans[ch].enabled = 0; //1 bit
    chans[ch].global_trigger = 0; // 1 bit
    chans[ch].trg_out = 0; // 1 bit
    chans[ch].record_length = 200; // 16* bit
    chans[ch].dynamic_range = 0; // 1 bit
    chans[ch].ev_per_buffer = 50; // 10 bit
    chans[ch].pre_trigger = 30; // 9* bit
    chans[ch].long_gate = 20; // 12 bit
    chans[ch].short_gate = 10; // 12 bit
    chans[ch].gate_offset = 5; // 8 bit
    chans[ch].trg_threshold = 100; // 12 bit
    chans[ch].fixed_baseline = 0; // 12 bit
    chans[ch].shaped_trigger_width = 10; // 10 bit
    chans[ch].trigger_holdoff = 30; // 10* bit
    chans[ch].charge_sensitivity = 000; // 3 bit (see docs)
    chans[ch].pulse_polarity = 1; // 1 bit (0->positive, 1->negative)
    chans[ch].trigger_config = 0; // 2 bit (see docs)
    chans[ch].baseline_mean = 3; // 3 bit (fixed,16,64,256,1024)
    chans[ch].self_trigger = 1; // 1 bit (0->enabled, 1->disabled)
    chans[ch].dc_offset = 0x8000; // 16 bit (-1V to 1V)
}


V1730::V1730(VMEBridge &_bridge, uint32_t _baseaddr, bool busErrReadout) : bridge(_bridge), baseaddr(_baseaddr), berr(busErrReadout) {

}

V1730::~V1730() {
    //Fully reset the board just in case
    write32(REG_BOARD_CONFIGURATION_RELOAD,0);
}

bool V1730::program(V1730Settings &settings) {
    try {
        settings.validate();
    } catch (runtime_error &e) {
        cout << "Could not program V1730: " << e.what() << endl;
        return false;
    }

    //used to build bit fields
    uint32_t data;
    
    //Fully reset the board just in case
    write32(REG_BOARD_CONFIGURATION_RELOAD,0);
    
    //Set TTL logic levels, ignore LVDS and debug settings
    write32(REG_FRONT_PANEL_CONTROL,1);

    data = (1 << 4) 
         | (1 << 8) 
         | (settings.card.dual_trace << 11) 
         | (settings.card.analog_probe << 12) 
         | (settings.card.oscilloscope_mode << 16) 
         | (1 << 17) 
         | (1 << 18)
         | (1 << 19)
         | (settings.card.digital_virt_probe_1 << 23)
         | (settings.card.digital_virt_probe_2 << 26);
    write32(REG_CONFIG,data);

    //build masks while configuring channels
    uint32_t channel_enable_mask = 0;
    uint32_t global_trigger_mask = (settings.card.coincidence_window << 20)
                                 | (settings.card.global_majority_level << 24) 
                                 | (settings.card.external_global_trigger << 30) 
                                 | (settings.card.software_global_trigger << 31);
    uint32_t trigger_out_mask = (settings.card.out_logic << 8) 
                              | (settings.card.out_majority_level << 10)
                              | (settings.card.external_trg_out << 30) 
                              | (settings.card.software_trg_out << 31);
    
    //keep track of the size of the buffers for each memory location
    uint32_t buffer_sizes[8] = { 0, 0, 0, 0, 0, 0, 0, 0}; //in locations

    //keep track of how to config the local triggers
    uint32_t local_logic[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    for (int ch = 0; ch < 16; ch++) {
        channel_enable_mask |= (settings.chans[ch].enabled << ch);
        global_trigger_mask |= (settings.chans[ch].global_trigger << (ch/2));
        trigger_out_mask |= (settings.chans[ch].trg_out << (ch/2));
        
        if (ch % 2 == 0) { //memory shared between pairs
        
            if (settings.chans[ch].global_trigger) {
                if (settings.chans[ch+1].global_trigger) {
                    local_logic[ch/2] = 3;
                } else {
                    local_logic[ch/2] = 1;
                }
            } else {
                if (settings.chans[ch+1].global_trigger) {
                    local_logic[ch/2] = 2;
                } else {
                    local_logic[ch/2] = 0;
                }
            }
            
            data = settings.chans[ch].record_length%8 ?  settings.chans[ch].record_length/8+1 : settings.chans[ch].record_length/8;
            write32(REG_RECORD_LENGTH|(ch<<8),data);
            settings.chans[ch].record_length = read32(REG_RECORD_LENGTH|(ch<<8))*8;
        } else {
            settings.chans[ch].record_length = settings.chans[ch-1].record_length;
            settings.chans[ch].ev_per_buffer = settings.chans[ch-1].ev_per_buffer;
        }
        
        if (settings.chans[ch].enabled) {
            buffer_sizes[ch/2] = (2 + settings.chans[ch].record_length/8)*settings.chans[ch].ev_per_buffer;
        }
        
        write32(REG_NEV_AGGREGATE|(ch<<8),settings.chans[ch].ev_per_buffer);
        write32(REG_PRE_TRG|(ch<<8),settings.chans[ch].pre_trigger/4);
        write32(REG_SHORT_GATE|(ch<<8),settings.chans[ch].short_gate);
        write32(REG_LONG_GATE|(ch<<8),settings.chans[ch].long_gate);
        write32(REG_PRE_GATE|(ch<<8),settings.chans[ch].gate_offset);
        write32(REG_DPP_TRG_THRESHOLD|(ch<<8),settings.chans[ch].trg_threshold);
        write32(REG_BASELINE_THRESHOLD|(ch<<8),settings.chans[ch].fixed_baseline);
        write32(REG_SHAPED_TRIGGER_WIDTH|(ch<<8),settings.chans[ch].shaped_trigger_width);
        write32(REG_TRIGGER_HOLDOFF|(ch<<8),settings.chans[ch].trigger_holdoff/4);
        data = (settings.chans[ch].charge_sensitivity)
             | (settings.chans[ch].pulse_polarity << 16)
             | (settings.chans[ch].trigger_config << 18)
             | (settings.chans[ch].baseline_mean << 20)
             | (settings.chans[ch].self_trigger << 24);
        write32(REG_DPP_CTRL|(ch<<8),data);
        data = local_logic[ch/2] | (1<<2);
        write32(REG_TRIGGER_CTRL|(ch<<8),data);
        write32(REG_DC_OFFSET|(ch<<8),settings.chans[ch].dc_offset);
        
    }
    
    write32(REG_CHANNEL_ENABLE_MASK,channel_enable_mask);
    write32(REG_GLOBAL_TRIGGER_MASK,global_trigger_mask);
    write32(REG_TRIGGER_OUT_MASK,trigger_out_mask);
    
    uint32_t largest_buffer = 0;
    for (int i = 0; i < 8; i++) if (largest_buffer < buffer_sizes[i]) largest_buffer = buffer_sizes[i];
    const uint32_t total_locations = 640000/8; 
    const uint32_t num_buffers = total_locations%largest_buffer ? total_locations/largest_buffer : total_locations/largest_buffer+1;
    uint32_t shifter = num_buffers;
    for (settings.card.buff_org = 0; shifter != 1; shifter = shifter >> 1, settings.card.buff_org++);
    if (1 << settings.card.buff_org > num_buffers) settings.card.buff_org--;
    if (settings.card.buff_org > 0xA) settings.card.buff_org = 0xA;
    if (settings.card.buff_org < 0x2) settings.card.buff_org = 0x2;
    cout << "Largest buffer: " << largest_buffer << " loc\nDesired buffers: " << num_buffers << "\nProgrammed buffers: " << (1<<settings.card.buff_org) << endl;
    write32(REG_BUFF_ORG,settings.card.buff_org);
    
    //Set max board aggregates to transver per readout
    write16(REG_READOUT_BLT_AGGREGATE_NUMBER,settings.card.max_board_agg_blt);
    
    //Enable VME BLT readout
    write16(REG_READOUT_CONTROL,(berr ? 1 : 0)<<4);
    
    return true;
}

bool V1730::checkTemps(vector<uint32_t> &temps, uint32_t danger) {
    temps.resize(16);
    bool over = false;
    for (int ch = 0; ch < 16; ch++) {
        temps[ch] = read32(REG_CHANNEL_TEMP|(ch<<8));
        if (temps[ch] >= danger) over = true;
    }
    return over;
}

size_t V1730::readoutBLT_berr(char *buffer, size_t buffer_size) {
    size_t offset = 0, size = 0;
    while (offset < buffer_size && (size = readBLT(0x0000, buffer+offset, 4090))) {
        offset += size;
    }
    return offset;
}

size_t V1730::readoutBLT_evtsz(char *buffer, size_t buffer_size) {
    size_t offset = 0, total = 0;
    while (readoutReady()) {
        uint32_t next = read32(REG_EVENT_SIZE);
        total += 4*next;
        if (offset+total > buffer_size) throw runtime_error("Readout buffer too small!");
        size_t lastoff = offset;
        while (offset < total) {
            size_t remaining = total-offset, read;
            if (remaining > 4090) {
                read = readBLT(0x0000, buffer+offset, 4090);
            } else {
                remaining = 8*(remaining%8 ? remaining/8+1 : remaining/8); // needs to be multiples of 8 (64bit)
                read = readBLT(0x0000, buffer+offset, remaining);
            }
            offset += read;
            if (!read) {
                cout << "\tfailed event size " << offset-lastoff << " / " << next*4 << endl;
                break;
            }
        }
    }
    return total;
}
