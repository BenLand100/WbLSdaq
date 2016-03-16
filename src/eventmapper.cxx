#include <H5Cpp.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <list>
#include <cstring>
#include <string>
#include <math.h>
#include <unistd.h>
#include <glob.h>

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


[[noreturn]] void help() {
    cout << "eventmapper reads all files that match `${prefix}.${index}.h5` ";
    cout << "respecting the optional bounds on the index and generates ";
    cout << "an event map as `${prefix}.map.csv` containg correlated events." << endl;
    cout << "./eventmapper [options] prefix" << endl;
    cout << "\t-v            enable verbose mode" << endl;
    cout << "\t-g            give up on LVDS correlations" << endl;
    cout << "\t-s index      force start file index [least]" << endl;
    cout << "\t-e index      force end file index [greatest]" << endl;
    cout << "\t-t mask       set equality test mask [0xFF]" << endl;
    cout << "\t-c mask       set comparison mask [0x0F]" << endl;
    cout << "\t-o offset     set max offset used for comparison [8]" << endl;
    cout << "\t-m group      set master pattern group [/fast/gr0/]" << endl;
    cout << "\t-f group      set fast pattern group [/master/ch0/]" << endl;
    exit(1);
}

// Assumes two cards (fast w/ g0 patterns, master w/ c0 petterns) with card-wide 
// triggers that have stored LVDS patterns of which some bits are an external
// trigger count. Missed triggers will have -1 in the locator fields.
int main(int argc, char **argv) {
    
    // The bitmask used to compare two patterns for equality
    uint16_t test_mask = 0xFF; // only 8 bits of counter
    // The bitmask applied before magnitude comparison when patterns don't match up
    uint16_t comp_mask = 0x0F; // if they don't match... compare the first four
    // If the magnitude is greater than this assume the count rolled over
    size_t max_offset = 8;
    // Group to read master patterns from
    string master_group = "/master/ch0/";
    // Group to read fast patterns from
    string fast_group = "/fast/gr0/";
    // Start index
    int startidx = -1;
    // End index
    int endidx = -1;
    // File prefix to read from
    string fprefix;
    // enable accepting discrete offsets for master lvds
    bool accept_offsets = true;
    size_t max_offsets = 128;
    // enable auto-orphaning detected retriggers
    bool orphan_retrigger = true;
    // enable orphaning of missed triggers
    bool orphan_missed = true;
    size_t max_orphans = 8;
    // yolo - don't worry with lvds
    bool giveup = false;
    // Extra debug flag
    bool verbose = false;

    opterr = 0;
    int c;
    while ((c = getopt(argc, argv, ":s:e:vt:c:o:m:f:g")) != -1) {
        switch (c) {
            case 's':
                startidx = stoull(optarg,NULL,0);
                break;
            case 'e':
                endidx = stoull(optarg,NULL,0);
                break;
            case 'v':
                verbose = true;
                break;
            case 't':
                test_mask = stoul(optarg,NULL,0) & 0xFFFF;
                break;
            case 'c':
                comp_mask = stoul(optarg,NULL,0) & 0xFFFF;
                break;
            case 'o':
                max_offset = stoull(optarg,NULL,9);
                break;
            case 'm':
                master_group = string(optarg);
                break;
            case 'f':
                fast_group = string(optarg);
                break;
            case 'g':
                giveup = true;
                break;
            case ':':
                cout << "-" << optopt << " requires an argument" << endl;
                help();
            case '?':
                cout << "-" << optopt << " is an unknown option" << endl;
                help();
            default:
                cout << "Unexpected result from getopt" << endl;
                help();
        }
    }
    
    if (giveup) {
        cout << "************ WARNING **************" << endl;
        cout << " Giving up on trigger correlations " << endl;
        cout << "************ WARNING **************" << endl;
    }

    if (argc - optind != 1) help();
    fprefix = argv[optind];

    if (startidx < 0 || endidx < 0) {
        glob_t files;
        string pattern = fprefix+".*.h5";
        glob(pattern.c_str(),0,NULL,&files);
        size_t prelen = fprefix.length();
        size_t min = INT_MAX, max = 0;
        for (size_t i = 0; i < files.gl_pathc; i++) {
            int len = strlen(files.gl_pathv[i]);
            string fname(&(files.gl_pathv[i])[prelen+1],len-prelen-4);
            size_t j = stoull(fname);
            if (j < min) min = j;
            if (j > max) max = j;
        }
        if (startidx < 0) startidx = min;
        if (endidx < 0) endidx = max;
    }
    
    ofstream evmap;
    evmap.open(fprefix+".map.csv");
    if (!evmap.is_open()) {
        cout << "Cannot open output file!" << endl;
        return 1;
    }

    vector<event> events;
    list<locator> master_overflow, fast_overflow;
    size_t master_retrigger = 0, fast_retrigger = 0;
    size_t offsets = 0, master_offsets = 0;
    size_t orphans = 0, master_orphans = 0, fast_orphans = 0;
    size_t total_events = 0;
    
    //loop over files
    for (int fidx = startidx; fidx <= endidx; fidx++) {
    
        string fname = fprefix+"."+to_string(fidx)+".h5";
        if (verbose) cout << "Opening file " << fname << endl;
        H5File file(fname, H5F_ACC_RDONLY);
        
        vector<uint16_t> master_patterns, fast_patterns;
        getPatterns(file, master_group, master_patterns);
        getPatterns(file, fast_group, fast_patterns);

        //loop over triggers
        size_t mi = 0, fi = 0;
        while ((mi < master_patterns.size() && fi < fast_patterns.size()) || (master_overflow.size() && fast_overflow.size())) {
           
            if (orphans > max_orphans) {
                cout << "Too many orphans in a row - bailing out." << endl;
                exit(1);
            }
            if (offsets > max_offsets) {
                cout << "We've gotten stuck on an offset - bailing out." << endl;
                exit(1);
            }


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
            if (!giveup && ((ev.master.pattern & test_mask) != (ev.fast.pattern & test_mask))) {
                
                cout << "Discontinuity found - master_file: " << ev.master.file << " master_index:" << ev.master.index;
                cout << " fast_file: " << ev.fast.file << " fast_index:" << ev.fast.index << endl;
                cout << "\tmaster_pattern: " << (ev.master.pattern & 0xFF) << " / " << (ev.master.pattern & test_mask) << " / " << (ev.master.pattern & comp_mask); 
                cout << " fast_pattern: " << (ev.fast.pattern & 0xFF) << " / " << (ev.fast.pattern & test_mask) << " / " << (ev.fast.pattern & comp_mask) << endl;
                
                if (accept_offsets) {
                    if (ev.master.pattern + 16 == ev.fast.pattern) {
                        cout << "\tmaster +16 bug..." << endl;
                        offsets++;
                        master_offsets++;
                        events.push_back(ev);
                        total_events++;
                        orphans = 0;
                        continue;
                    }
                    offsets = 0;
                }

                if (orphan_retrigger && events.size()) {
                    const uint16_t cmpat = ev.master.pattern;
                    const uint16_t lmpat = events.back().master.pattern;
                    // not comprehensive...
                    if (cmpat-16==lmpat || cmpat+16==lmpat || cmpat == lmpat) {
                        cout << "\tmaster retrigger was orphaned" << endl;
                        fast_overflow.push_front(ev.fast);
                        ev.fast.file = -1;
                        ev.fast.index = -1;
                        ev.fast.pattern = -1;
                        master_retrigger++;
                        events.push_back(ev);
                        total_events++;
                        orphans = 0;
                        continue;
                    } else if (ev.fast.pattern == events.back().fast.pattern) {
                        master_overflow.push_front(ev.master);
                        cout << "\tfast retrigger was orphaned" << endl;
                        ev.master.file = -1;
                        ev.master.index = -1;
                        ev.master.pattern = -1;
                        fast_retrigger++;
                        events.push_back(ev);
                        total_events++;
                        orphans = 0;
                        continue;
                    }
                }
                
                if (orphan_missed) {
                    bool invert = (size_t)abs((ev.fast.pattern & comp_mask) - (ev.master.pattern & comp_mask)) > max_offset;
                    if (((ev.fast.pattern & comp_mask) > (ev.master.pattern & comp_mask)) != invert) {
                        cout << "\tmaster was orphaned" << endl;
                        fast_overflow.push_front(ev.fast);
                        ev.fast.file = -1;
                        ev.fast.index = -1;
                        ev.fast.pattern = -1;
                        master_orphans++;
                        orphans++;
                        events.push_back(ev);
                        total_events++;
                        continue;
                    } else {
                        master_overflow.push_front(ev.master);
                        cout << "\tfast was orphaned" << endl;
                        ev.master.file = -1;
                        ev.master.index = -1;
                        ev.master.pattern = -1;
                        fast_orphans++;
                        orphans++;
                        events.push_back(ev);
                        total_events++;
                        continue;
                    }
                    orphans = 0;
                }
                
                cout << "No clue what to do with this event - bailing out." << endl;
                exit(1);

            } else {
                if (verbose) {
                    cout << "Good event - master_file: " << ev.master.file << " master_index:" << ev.master.index;
                    cout << " fast_file: " << ev.fast.file << " fast_index:" << ev.fast.index << endl;
                    cout << "\tmaster_pattern: " << (ev.master.pattern & 0xFF) << " / " << (ev.master.pattern & test_mask) << " / " << (ev.master.pattern & comp_mask);
                    cout << " fast_pattern: " << (ev.fast.pattern & 0xFF) << " / " << (ev.fast.pattern & test_mask) << " / " << (ev.fast.pattern & comp_mask) << endl;
                }
                orphans = 0;
                offsets = 0;
                
                events.push_back(ev);
                total_events++;
            }
             
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
    
    cout << "Events: " << total_events << ", Master Orphans: " << master_orphans << ", Fast Orphans: " << fast_orphans << endl;
    cout << "Master Offsets: " << master_offsets << ", Master Retriggers: " << master_retrigger << ", Fast Retrigger: " << fast_retrigger << endl;
    
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
