#include <CAENDigitizer.h>
#include <CAENVMElib.h>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "json.hh"

using namespace std;

json::Value to_json(CAEN_DGTZ_DRS4Correction_t *dat) {
    json::Value calib(json::TOBJECT);
    for (size_t gr = 0; gr < 4; gr++) {
        json::Value group(json::TOBJECT);
        CAEN_DGTZ_DRS4Correction_t &cal = dat[gr];
        vector<double> cell_delay(1024);
        for (size_t i = 0; i < 1024; i++) {
            cell_delay[i] = cal.time[i];
        }
        group["cell_delay"] = cell_delay;
        for (size_t ch = 0; ch < 8; ch++) {
            json::Value chan(json::TOBJECT);
            vector<int> cell_offset(1024);
            vector<int> seq_offset(1024);
            for (size_t i = 0; i < 1024; i++) {
                cell_offset[i] = cal.cell[ch][i];
                seq_offset[i] = cal.nsample[ch][i];
            }
            chan["cell_offset"] = cell_offset;
            chan["seq_offset"] = seq_offset;
            group["ch"+to_string(ch)] = chan;
        }
        calib["gr"+to_string(gr)] = group;
    }
    return calib;
}


int main(int argc, char **argv) {
    if (argc != 3) {
        cout << "Usage: ./v1742calib base_addr output.json\n\tbase_addr is vme base in hex; output.json stores the calib data\n";
        return 1;
    }


    int linknum = 0;
    uint32_t baseaddr = 0x0;
    int handle = 0;
    int res = 0;
    
    stringstream ss;
    ss << std::hex << argv[1];
    ss >> baseaddr;
    
    json::Value card(json::TOBJECT);
    
    cout << "Opening dgtz" << endl;
    res = CAEN_DGTZ_OpenDigitizer(CAEN_DGTZ_USB, linknum, 0, (int)baseaddr, &handle);
    if (res != 0) throw runtime_error("Open");
    
    CAEN_DGTZ_DRS4Correction_t corr[4];
    
    cout << "Reading 1GHz" << endl;
    res = CAEN_DGTZ_GetCorrectionTables(handle,CAEN_DGTZ_DRS4_1GHz,&corr);
    if (res != 0) throw runtime_error("1GHz");
    card["1GHz"] = to_json(corr);
    
    cout << "Reading 2.5GHz" << endl;
    res = CAEN_DGTZ_GetCorrectionTables(handle,CAEN_DGTZ_DRS4_2_5GHz,&corr);
    if (res != 0) throw runtime_error("2.5GHz");
    card["2.5GHz"] = to_json(corr);
    
    cout << "Reading 5GHz" << endl;
    res = CAEN_DGTZ_GetCorrectionTables(handle,CAEN_DGTZ_DRS4_5GHz,&corr);
    if (res != 0) throw runtime_error("5GHz");
    card["5GHz"] = to_json(corr);
    
    cout << "Closing dgtz" << endl;
    res = CAEN_DGTZ_CloseDigitizer(handle);
    if (res != 0) throw runtime_error("Close");
    
    ofstream file(argv[2]);
    json::Writer out(file);
    card["name"] = string("CALIB");
    card["index"] = string("fast");
    out.putValue(card);
    
    cout << "Calibration written" << endl;
    return 0;
}
