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

#include "VMEBridge.hh"
#include "json.hh"

typedef struct {

    //REG_CHANNEL_ENABLE_MASK
    uint32_t enabled; // 1 bit
    
    //REG_GLOBAL_TRIGGER_MASK
    uint32_t global_trigger; // 1 bit
    
    //REG_TRIGGER_OUT_MASK
    uint32_t trg_out; // 1 bit
    
    //REG_RECORD_LENGTH
    uint32_t record_length; // 16* bit
    
    //REG_DYNAMIC_RANGE
    uint32_t dynamic_range; // 1 bit
    
    //REG_NEV_AGGREGATE
    uint32_t ev_per_buffer; // 10 bit
    
    //REG_PRE_TRG
    uint32_t pre_trigger; // 9* bit
    
    //REG_LONG_GATE
    uint32_t long_gate; // 12 bit
    
    //REG_SHORT_GATE
    uint32_t short_gate; // 12 bit
    
    //REG_PRE_GATE
    uint32_t gate_offset; // 8 bit
    
    //REG_DPP_TRG_THRESHOLD
    uint32_t trg_threshold; // 12 bit
    
    //REG_BASELINE_THRESHOLD
    uint32_t fixed_baseline; // 12 bit
    
    //REG_SHAPED_TRIGGER_WIDTH
    uint32_t shaped_trigger_width; // 10 bit
    
    //REG_TRIGGER_HOLDOFF
    uint32_t trigger_holdoff; // 10* bit
    
    //REG_DPP_CTRL
    uint32_t charge_sensitivity; // 3 bit (see docs)
    uint32_t pulse_polarity; // 1 bit (0->positive, 1->negative)
    uint32_t trigger_config; // 2 bit (see docs)
    uint32_t baseline_mean; // 3 bit (fixed, 16, 64, 256, 1024)
    uint32_t self_trigger; // 1 bit (0->enabled, 1->disabled)
    
    //REG_DC_OFFSET
    uint32_t dc_offset; // 16 bit (-1V to 1V)

} V1730_chan_config;

typedef struct {

    //REG_CONFIG
    uint32_t dual_trace; // 1 bit
    uint32_t analog_probe; // 2 bit (see docs)
    uint32_t oscilloscope_mode; // 1 bit
    uint32_t digital_virt_probe_1; // 3 bit (see docs)
    uint32_t digital_virt_probe_2; // 3 bit (see docs)
    
    //REG_GLOBAL_TRIGGER_MASK
    uint32_t coincidence_window; // 3 bit
    uint32_t global_majority_level; // 3 bit
    uint32_t external_global_trigger; // 1 bit
    uint32_t software_global_trigger; // 1 bit
    
    //REG_TRIGGER_OUT_MASK
    uint32_t out_logic; // 2 bit (OR,AND,MAJORITY)
    uint32_t out_majority_level; // 3 bit
    uint32_t external_trg_out; // 1 bit
    uint32_t software_trg_out; // 1 bit
    
    //REG_BUFF_ORG
    uint32_t buff_org;
    
    //REG_READOUT_BLT_AGGREGATE_NUMBER
    uint16_t max_board_agg_blt;
    
} V1730_card_config;

class V1730Settings {
    friend class V1730;

    public:
    
        V1730Settings();
        
        V1730Settings(map<string,json::Value> &db);
        
        virtual ~V1730Settings();
        
        void validate();
        
        inline bool getEnabled(uint32_t ch) {
            return chans[ch].enabled;
        }
        
        inline uint32_t getRecordLength(uint32_t ch) {
            return chans[ch].record_length;
        }
        
        inline uint32_t getDCOffset(uint32_t ch) {
            return chans[ch].dc_offset;
        }
        
        inline uint32_t getPreSamples(uint32_t ch) {
            return chans[ch].pre_trigger;
        }
        
        inline uint32_t getThreshold(uint32_t ch) {
            return chans[ch].trg_threshold;
        }
    
    protected:
    
        V1730_card_config card;
        V1730_chan_config chans[16];
        
        void chanDefaults(uint32_t ch);
        
};

class V1730 {

    //system wide
    #define REG_CONFIG 0x8000
    #define REG_CONFIG_SET 0x8004
    #define REG_CONFIG_CLEAR 0x8008
    #define REG_BUFF_ORG 0x800C
    #define REG_FRONT_PANEL_CONTROL 0x811C
    #define REG_DUMMY 0xEF20
    #define REG_SOFTWARE_RESET 0xEF24
    #define REG_SOFTWARE_CLEAR 0xEF28
    #define REG_BOARD_CONFIGURATION_RELOAD 0xEF34

    //per channel, or with 0x0n00
    #define REG_RECORD_LENGTH 0x1020
    #define REG_DYNAMIC_RANGE 0x1024
    #define REG_NEV_AGGREGATE 0x1034
    #define REG_PRE_TRG 0x1038
    #define REG_SHORT_GATE 0x1054
    #define REG_LONG_GATE 0x1058
    #define REG_PRE_GATE 0x105C
    #define REG_DPP_TRG_THRESHOLD 0x1060
    #define REG_BASELINE_THRESHOLD 0x1064
    #define REG_SHAPED_TRIGGER_WIDTH 0x1070
    #define REG_TRIGGER_HOLDOFF 0x1074
    #define REG_DPP_CTRL 0x1080
    #define REG_TRIGGER_CTRL 0x1084
    #define REG_DC_OFFSET 0x1098
    #define REG_CHANNEL_TEMP 0x10A8

    //acquisition 
    #define REG_ACQUISITION_CONTROL 0x8100
    #define REG_ACQUISITION_STATUS 0x8104
    #define REG_SOFTWARE_TRIGGER 0x8108
    #define REG_GLOBAL_TRIGGER_MASK 0x810C
    #define REG_TRIGGER_OUT_MASK 0x8110
    #define REG_CHANNEL_ENABLE_MASK 0x8120

    //readout
    #define REG_EVENT_SIZE 0x814C
    #define REG_READOUT_CONTROL 0xEF00
    #define REG_READOUT_STATUS 0xEF04
    #define REG_VME_ADDRESS_RELOCATION 0xEF10
    #define REG_READOUT_BLT_AGGREGATE_NUMBER 0xEF1C
    
    public:
        V1730(VMEBridge &_bridge, uint32_t _baseaddr, bool busErrReadout = true);
        
        virtual ~V1730();
        
        bool program(V1730Settings &settings);
        
        inline void startAcquisition() {
            write32(REG_ACQUISITION_CONTROL,1<<2);
        }
        
        inline void stopAcquisition() {
            write32(REG_ACQUISITION_CONTROL,0);
        }
        
        inline bool acquisitionRunning() {
            return read32(REG_ACQUISITION_STATUS) & (1 << 2);
        }
        
        inline bool readoutReady() {
            return read32(REG_ACQUISITION_STATUS) & (1 << 3);
        }
        
        bool checkTemps(vector<uint32_t> &temps, uint32_t danger);
        
        inline size_t readoutBLT(char *buffer, size_t buffer_size) {
            return berr ? readoutBLT_berr(buffer, buffer_size) : readoutBLT_evtsz(buffer,buffer_size);
        }
        
        inline void write16(uint32_t reg, uint32_t data) {
            bridge.write16(baseaddr|reg,data);
        }
        
        inline uint32_t read16(uint32_t reg) {
            return bridge.read16(baseaddr|reg);
        }
        
        inline void write32(uint32_t reg, uint32_t data) {
            bridge.write32(baseaddr|reg,data);
        }
        
        inline uint32_t read32(uint32_t reg) {
            return bridge.read32(baseaddr|reg);
        }
        
        inline uint32_t readBLT(uint32_t addr, void *buffer, uint32_t size) {
            return bridge.readBLT(baseaddr|addr,buffer,size);
        }
        
    protected:
        
        bool berr;
        VMEBridge &bridge;
        uint32_t baseaddr;
        
        //for bus error read
        size_t readoutBLT_berr(char *buffer, size_t buffer_size);
        
        //for event size read
        size_t readoutBLT_evtsz(char *buffer, size_t buffer_size);
        
};
