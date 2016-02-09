#include <H5Cpp.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <list>
#include <string>
#include <math.h>

using namespace std;
using namespace H5;

// Extracts patterns dataset from a group in an hdf5 file, stores in patterns argument
void getPatterns(H5File &file, const string &gname, vector<uint16_t> &patterns) {
    Group group = file.openGroup(gname);
    DataSet dataset = group.openDataSet("patterns");

    hsize_t dims[1];
    DataSpace dataspace = dataset.getSpace();
    dataspace.getSimpleExtentDims(dims);
    
    patterns.resize(dims[0]);
    dataset.read(&patterns[0],PredType::NATIVE_UINT16,dataspace);
}

typedef struct {
    int file, index;
    uint16_t pattern;
} locator;

typedef struct {
    locator master, fast;
} event;


// Assumes two cards (fast w/ g0 patterns, master w/ c0 petterns) with card-wide 
// triggers that have stored LVDS patterns of which some bits are an external
// trigger count. Missed triggers will have -1 in the locator fields.
int main(int argc, char **argv) {

    if (argc != 4) {
        cout << "./eventmapper fileprefix startidx endidx" << endl;
        return 1;
    }

    string fprefix(argv[1]);
    const size_t startidx = atoi(argv[2]);
    const size_t endidx = atoi(argv[3]);

    // The bitmask used to compare two patterns for equality
    const uint16_t test_mask = 0xEF; // bit 4 acts up on master, only 8 bits of counter
    // The bitmask applied before magnitude comparison when patterns don't match up
    const uint16_t comp_mask = 0x0F; // if they don't match... compare the first four
    // If the magnitude is greater than this assume the count rolled over
    const size_t max_offset = 8;  
    // If more than this many orphans are found in a row, assume something went wrong and abort
    const size_t max_orphans = 8;
    
    string master_group = "/master/ch0/";
    string fast_group = "/fast/gr0/";
    
    vector<event> events;
    list<locator> master_overflow, fast_overflow;
    size_t skipped = 0, total_skipped = 0, orphans = 0, master_orphans = 0, fast_orphans = 0, total_events = 0;
    
    //loop over files
    for (size_t fidx = startidx; fidx <= endidx; fidx++) {
    
        H5File file(fprefix+"."+to_string(fidx)+".h5", H5F_ACC_RDONLY);
        
        vector<uint16_t> master_patterns, fast_patterns;
        getPatterns(file, master_group, master_patterns);
        getPatterns(file, fast_group, fast_patterns);
        
        //loop over triggers
        size_t mi = 0, fi = 0;
        while ((mi < master_patterns.size() && fi < fast_patterns.size()) || (master_overflow.size() && fast_overflow.size())) {
            
            // Populate event with pending or next independently
            event ev;
            if (master_overflow.size()) {
                ev.master = master_overflow.front();
                master_overflow.pop_front();
            } else {
                ev.master.file = fidx;
                ev.master.index = mi;
                ev.master.pattern = master_patterns[mi];
                mi++;
            }
            if (fast_overflow.size()) {
                ev.fast = fast_overflow.front();
                fast_overflow.pop_front();
            } else {
                ev.fast.file = fidx;
                ev.fast.index = fi;
                ev.fast.pattern = fast_patterns[fi];
                fi++;
            }
            
            // One of the two triggers might be an orphan
            if ((ev.master.pattern & test_mask) != (ev.fast.pattern & test_mask)) {
                
                cout << "Discontinuity found - master_file: " << ev.master.file << " master_index:" << ev.master.index;
                cout << " fast_file: " << ev.fast.file << " fast_index:" << ev.fast.index << endl;
                cout << "\tmaster_pattern: " << (ev.master.pattern & 0xFF) << " / " << (ev.master.pattern & test_mask) << " / " << (ev.master.pattern & comp_mask); 
                cout << " fast_pattern: " << (ev.fast.pattern & 0xFF) << " / " << (ev.fast.pattern & test_mask) << " / " << (ev.fast.pattern & comp_mask) << endl;
               
                int mdelta = (ev.master.pattern) - (events.back().master.pattern);
                int fdelta = (ev.fast.pattern) - (events.back().fast.pattern);
                int delta = (ev.fast.pattern & comp_mask) - (ev.master.pattern & comp_mask);
                
                if (skipped && abs(ev.fast.pattern-ev.master.pattern) < max_offset) {
                    cout << "\tPossibly back on track..." << endl;
                } else if ((abs(delta) != 1) && (abs(mdelta) > max_offset+skipped || abs(fdelta) > max_offset+skipped)) {
                    cout << "\tWay off track - skipping (" << skipped << ") events as last ditch effort" << endl;
                    skipped++;
                    total_skipped++;
                    if (skipped > 255) {
                        cout << "\tWe skipped more events than we can count, bailing out." << endl;
                        return -1;
                    }
                    continue;
                }    
                skipped = 0;
            
                bool invert = abs((ev.fast.pattern & comp_mask) - (ev.master.pattern & comp_mask)) > max_offset;
                    if (((ev.fast.pattern & comp_mask) > (ev.master.pattern & comp_mask)) != invert) {
                        cout << "\tmaster was orphaned" << endl;
                        fast_overflow.push_front(ev.fast);
                        ev.fast.file = -1;
                        ev.fast.index = -1;
                        ev.fast.pattern = -1;
                        master_orphans++;
                    } else {
                        master_overflow.push_front(ev.master);
                        cout << "\tfast was orphaned" << endl;
                        ev.master.file = -1;
                        ev.master.index = -1;
                        ev.master.pattern = -1;
                        fast_orphans++;
                    }
                orphans++;
                if (orphans > max_orphans) {
                    cout << "Too many orphans in a row - bailing out." << endl;
                    return 1;
                }
            } else {
                //cout << "Good event - master_file: " << ev.master.file << " master_index:" << ev.master.index;
                //cout << " fast_file: " << ev.fast.file << " fast_index:" << ev.fast.index << endl;
                //cout << "\tmaster_pattern: " << (ev.master.pattern & 0xFF) << " / " << (ev.master.pattern & test_mask) << " / " << (ev.master.pattern & comp_mask);
                //cout << " fast_pattern: " << (ev.fast.pattern & 0xFF) << " / " << (ev.fast.pattern & test_mask) << " / " << (ev.fast.pattern & comp_mask) << endl;
                orphans = 0;
            }
             
            // Add the final event to the list
            events.push_back(ev);
            total_events++;
        }
        
        for (; mi < master_patterns.size(); mi++) {
            locator m;
            m.file = fidx;
            m.index = mi;
            m.pattern = master_patterns[mi];
            master_overflow.push_back(m);
        }
        
        for (; fi < fast_patterns.size(); fi++) {
            locator f;
            f.file = fidx;
            f.index = fi;
            f.pattern = fast_patterns[fi];
            fast_overflow.push_back(f);
        }
        
    }
    
    cout << "Events: " << total_events << ", Master Orphans: " << master_orphans << ", Fast Orphans: " << fast_orphans << ", Skipped: " << total_skipped << endl;
    
    //May have some orphans in one or the other overflow (but not both so no checks for correlations)
    while (master_overflow.size()) {
        event ev;
        ev.master = master_overflow.front();
        master_overflow.pop_front();
        ev.fast.file = -1;
        ev.fast.index = -1;
        ev.fast.pattern = -1;
        events.push_back(ev);
        master_orphans++;
    }
    while (fast_overflow.size()) {
        event ev;
        ev.fast = fast_overflow.front();
        fast_overflow.pop_front();
        ev.master.file = -1;
        ev.master.index = -1;
        ev.master.pattern = -1;
        events.push_back(ev);
        fast_orphans++;
    }
    
    ofstream evmap;
    evmap.open(fprefix+".map.csv");
    evmap << "\"event_index\", \"master_file\", \"master_index\", \"fast_file\", \"fast_index\"" << endl;
    for (size_t i = 0; i < events.size(); i++) {
        evmap << i << ", ";
        evmap << events[i].master.file << ", ";
        evmap << events[i].master.index << ", ";
        evmap << events[i].fast.file << ", ";
        evmap << events[i].fast.index << endl;
    }
    evmap.close();
    
}
    
    
