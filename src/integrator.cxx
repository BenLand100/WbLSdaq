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
#include <getopt.h>
#include <sstream>
#include <json.hh>

using namespace std;
using namespace H5;

/*
-v VERBOSE
-m /master/ch0 -n myint --pedstart 10 --pedend 450 --pedcut 5 --sigstart 450 --sigend 600
-f /fast/gr0/ch0 -n myint2 --pedstart 10 --pedend 450 --pedcut 5 --sigstart 450 --sigend 600
*/

enum storagetype { FAST, MASTER };

class intspec {
    public:
        uint16_t maxval;
        const string group;
        const storagetype type;
        double ps_sample;
        double V_adc;
        int pedstart, pedend;
        double pedcut;
        int sigstart, sigend;
        double threshold;
        int cfdwindow;
        string name;
        
        //fast only
        size_t grnum;
        vector<double> group_cell_delays;
        
        intspec(string _group, storagetype _type) : 
            group(_group), 
            type(_type),
            pedstart(-1), 
            pedend(-1), 
            pedcut(0.0), 
            sigstart(-1), 
            sigend(-1),
            threshold(0.0),
            cfdwindow(-1),
            name("") {
            if (type == FAST) {
                grnum = stoi(group.substr(group.find("gr")+2,1));
            } else {
                grnum = -1;
            }
        }
        
        bool check() {
            return (threshold == 0.0 ? !((pedend != -1) ^ (pedend != -1)) && (cfdwindow == -1) : (pedend != -1) && (pedend != -1))
                   && (sigstart != -1) && (sigend != -1) && name.length() != 0;
        }
        
        virtual void init(H5File &file) {
            string card = group.substr(0,group.find("/",1));
            Group card_group = file.openGroup(card.c_str());
            
            uint32_t bits;
            Attribute bits_attrib = card_group.openAttribute("bits");
            bits_attrib.read(PredType::NATIVE_UINT32, &bits);
            V_adc = (type == MASTER ? 2.0 : 1.0)/pow(2,bits);
            maxval = 1<<bits;
            
            Attribute ns_sample_attrib = card_group.openAttribute("ns_sample");
            double ns_sample;
            ns_sample_attrib.read(PredType::NATIVE_DOUBLE, &ns_sample);
            ps_sample = 1000.0 * ns_sample;
            
            //convert threshold from mV to ADC here
            threshold /= V_adc*1000.0;
        }
        
};

class masterintspec : public intspec {
    public:
        masterintspec(string group) : intspec(group,MASTER) {
        }
};

class fastintspec : public intspec {
    public:
        fastintspec(string group) : intspec(group,FAST) {
        }
};

typedef struct {
    size_t traces, samples;
    uint16_t *data;
    uint16_t *start_index;
} sampdata;

typedef struct {
    vector<double> pedmean;
    vector<uint8_t> pedvalid;
    vector<double> sigcharge;
    vector<double> times;
} intevent;

[[noreturn]] void help() {
    cout << "./spe [groups] prefix" << endl;
    cout << "\t-T --timecorr filename  specify a json file with V1742 (fast) time calibration" << endl;
    cout << "\t-o --outfile filename   specify a filename other than ${prefix}.int.h5" << endl;
    cout << "\t-m --master group       start a group for the master card" << endl;
    cout << "\t-f --fast group         start a group for the fast card" << endl;
    cout << "\t-n --name nickname      specify the name for the group to be saved as" << endl;
    cout << "\t-a --pedstart sample    specify pedestal start (in samples) for the current group" << endl;
    cout << "\t-b --pedend sample      specify pedestal end (in samples) for the current group" << endl;
    cout << "\t-x --pedcut mV          specify pedestal fluctuation (in mV) cut for the current group" << endl;
    cout << "\t-c --sigstart sample    specify signal start (in samples) for the current group" << endl;
    cout << "\t-d --sigend sample      specify signal end (in samples) for the current group" << endl;
    cout << "\t-t --threshold mV       specify mV below pedestal to record first crossing " << endl;
    cout << "\t-k --cfdwindow samples  look samples ahead for the max value to use CFD" << endl;
    exit(1);
}

int main(int argc, char **argv) {
    
    string fprefix;
    string ofname;
    string tcorrfname;
    vector<intspec*> specs;
    bool verbose = false;
    
    struct option longopts[] = {
        { "timecorr", 1, NULL, 'T' },
        { "outfile", 1, NULL, 'o' },
        { "master", 1, NULL, 'm' },
        { "fast", 1, NULL, 'f' },
        { "name", 1, NULL, 'n' },
        { "pedstart", 1, NULL, 'a' },
        { "pedend", 1, NULL, 'b' },
        { "sigstart", 1, NULL, 'c' },
        { "sigend", 1, NULL, 'd' },
        { "pedcut", 1, NULL, 'x' },
        { "threshold", 1, NULL, 't' },
        { "cfdwindow", 1, NULL, 'k' },
        { 0, 0, 0, 0 }};
    
    opterr = 0;
    int c;
    while ((c = getopt_long(argc, argv, ":vT:o:m:f:a:b:c:d:x:t:n:k:", longopts, NULL)) != -1) {
        switch (c) {
            case 'T':
                if (tcorrfname.length() != 0) {
                    cout << "Trying to set timecorr twice!" << endl;
                    help();
                } else {
                    tcorrfname = string(optarg);
                }
                break;
            case 'o':
                if (ofname.length() != 0) {
                    cout << "Trying to set outfile twice!" << endl;
                    help();
                } else {
                    ofname = string(optarg);
                }
                break;
            case 'n':
                if (!specs.size()) {
                    cout << "Trying to set name before setting a group!" << endl;
                    help();
                } else if (specs.back()->name.length() != 0) {
                    cout << "Trying to set name twice for " << specs.back()->group << endl;
                    help();
                }
                specs.back()->name = optarg;
                break;
            case 'v':
                verbose = true;
                break;
            case 'm':
                if (specs.size() && !specs.back()->check()) {
                    cout << "Group specifier missing fields for " << specs.back()->group << endl;
                    help();
                }
                specs.push_back(new masterintspec(optarg));
                break;
            case 'f':
                if (specs.size() && !specs.back()->check()) {
                    cout << "Group specifier missing fields for " << specs.back()->group << endl;
                    help();
                }
                specs.push_back(new fastintspec(optarg));
                break;
            case 'a':
                if (!specs.size()) {
                    cout << "Trying to set pedstart before setting a group!" << endl;
                    help();
                } else if (specs.back()->pedstart != -1) {
                    cout << "Trying to set pedstart twice for " << specs.back()->group << endl;
                    help();
                }
                specs.back()->pedstart = stoull(optarg,NULL,0);
                break;
            case 'b':
                if (!specs.size()) {
                    cout << "Trying to set pedend before setting a group!" << endl;
                    help();
                } else if (specs.back()->pedend != -1) {
                    cout << "Trying to set pedend twice for " << specs.back()->group << endl;
                    help();
                }
                specs.back()->pedend = stoull(optarg,NULL,0);
                break;
            case 'c':
                if (!specs.size()) {
                    cout << "Trying to set sigstart before setting a group!" << endl;
                    help();
                } else if (specs.back()->sigstart != -1) {
                    cout << "Trying to set sigstart twice for " << specs.back()->group << endl;
                    help();
                }
                specs.back()->sigstart = stoull(optarg,NULL,0);
                break;
            case 'd':
                if (!specs.size()) {
                    cout << "Trying to set sigend before setting a group!" << endl;
                    help();
                } else if (specs.back()->sigend != -1) {
                    cout << "Trying to set sigend twice for " << specs.back()->group << endl;
                    help();
                }
                specs.back()->sigend = stoull(optarg,NULL,0);
                break;
            case 'x':
                if (!specs.size()) {
                    cout << "Trying to set pedcut before setting a group!" << endl;
                    help();
                } else if (specs.back()->pedcut != 0.0) {
                    cout << "Trying to set pedcut twice for " << specs.back()->group << endl;
                    help();
                }
                specs.back()->pedcut = stod(optarg);
                break;
            case 't':
                if (!specs.size()) {
                    cout << "Trying to set threshold before setting a group!" << endl;
                    help();
                } else if (specs.back()->threshold != 0.0) {
                    cout << "Trying to set threshold twice for " << specs.back()->group << endl;
                    help();
                }
                specs.back()->threshold = stod(optarg);
                break;
            case 'k':
                if (!specs.size()) {
                    cout << "Trying to set cfdwindow before setting a group!" << endl;
                    help();
                } else if (specs.back()->cfdwindow != -1) {
                    cout << "Trying to set cfdwindow twice for " << specs.back()->group << endl;
                    help();
                }
                specs.back()->cfdwindow = stoull(optarg,NULL,0);
                break;
            case ':':
                cout << "-" << (char)optopt << " requires an argument" << endl;
                help();
            case '?':
                cout << "-" << (char)optopt << " is an unknown option" << endl;
                help();
            default:
                cout << "Unexpected result from getopt" << endl;
                help();
        }
    }
    
    if (!specs.size()) {
        cout << "No groups specified!" << endl;
        help();
    } else if (!specs.back()->check()) {
        cout << "Group specifier missing fields for " << specs.back()->group << endl;
        help();
    }

    if (argc - optind != 1) { 
        cout << "Specify exactly one dataset prefix" << endl;
        help();
    }
    fprefix = argv[optind];
    
    // Find the datafiles that match the prefix
    glob_t files;
    string pattern = fprefix+".*.h5";
    glob(pattern.c_str(),0,NULL,&files);
    if (!files.gl_pathc) {
        cout << "No datafiles match the prefix!" << endl;
        exit(1);
    }
    
    // Pull some attributes from the datafiles and prepare to read them out
    H5File initfile(files.gl_pathv[0], H5F_ACC_RDONLY);
    vector<sampdata> data(specs.size());
    vector<intevent> intevents(specs.size());
    for (size_t i = 0; i < specs.size(); i++) {
        specs[i]->init(initfile);
        if (verbose) {
            cout << (specs[i]->type == MASTER ? "M" : "F") << ": " << specs[i]->group;
            cout << " N: " << specs[i]->name;
            cout << " V/ADC: " << specs[i]->V_adc;
            cout << " ps/sample: " << specs[i]->ps_sample;
            cout << " pedstart: " << specs[i]->pedstart;
            cout << " pedend: " << specs[i]->pedend;
            cout << " pedcut: " << specs[i]->pedcut;
            cout << " sigstart: " << specs[i]->sigstart;
            cout << " sigend: " << specs[i]->sigend;
            cout << " threshold: " << specs[i]->threshold;
            cout << " cfdwindow: " << specs[i]->cfdwindow;
            cout << endl;
        }
        data[i].traces = 0;
        data[i].samples = 0;
        data[i].data = NULL;
        data[i].start_index = NULL;
    }
    
    if (tcorrfname.length() > 0) {
        ifstream calibfile(tcorrfname);
        json::Reader reader(calibfile);
        json::Value calib;
        reader.getValue(calib);
        for (size_t i = 0; i < specs.size(); i++) {
            if (specs[i]->type == FAST) {
                json::Value speed;
                switch ((int)round(specs[i]->ps_sample)) {
                    case 200:
                        speed = calib["5GHz"];
                        break;
                    case 400:
                        speed = calib["2.5GHz"];
                        break;
                    case 1000:
                        speed = calib["1GHz"];
                        break;
                    default:
                        cout << "Unknown sample rate.";
                        exit(1);
                }
                specs[i]->group_cell_delays = speed["gr"+to_string(specs[i]->grnum)]["cell_delay"].toVector<double>();
            }
        }
    }
    
    // Keep track of the open files
    int64_t masterfile = -1, fastfile = -1;
    H5File master, fast;
    
    // Read the event map to get events
    ifstream eventmap(fprefix+".map.csv");
    if (!eventmap.is_open()) {
        cout << "Could not open event map!" << endl;
        exit(1);
    }
    string line,dummy;
    getline(eventmap,line);
    while (getline(eventmap,line)) {
    
        stringstream ss(line);
        int64_t eidx,mf,mi,ff,fi;
        ss >> eidx >> dummy >> mf >> dummy >> mi >> dummy >> ff >> dummy >> fi;
        
        // Check if the new event requires new files 
        bool update_master = mf != -1 && masterfile != mf;
        bool update_fast = ff != -1 && fastfile != ff;
        if (update_fast || update_master) {
            if (verbose) cout << "Loading new " << (update_fast ? "fast ("+to_string(ff)+") " : "") << (update_master ? "master ("+to_string(mf)+")" : "") << endl;
            if (update_master) {
                if (masterfile != -1) master.close();
                master.openFile(fprefix+"."+to_string(mf)+".h5", H5F_ACC_RDONLY);
                masterfile = mf;
                for (size_t i = 0; i < specs.size(); i++) {
                    if (specs[i]->type == MASTER) {
                    
                        Group group = master.openGroup(specs[i]->group);
                        DataSet dataset = group.openDataSet("samples");

                        DataSpace dataspace = dataset.getSpace();
                        int rank = dataspace.getSimpleExtentNdims();
                        hsize_t *dims = new hsize_t[rank];
                        dataspace.getSimpleExtentDims(dims);

                        data[i].traces = dims[0];
                        data[i].samples = dims[1];
                        if (data[i].data) delete [] data[i].data;
                        data[i].data = new uint16_t[data[i].traces*data[i].samples];
                        dataset.read(data[i].data,PredType::NATIVE_UINT16);
                    
                    }
                }
            }
            if (update_fast) {
                if (fastfile != -1) fast.close();
                fast.openFile(fprefix+"."+to_string(ff)+".h5", H5F_ACC_RDONLY);
                fastfile = ff;
                for (size_t i = 0; i < specs.size(); i++) {
                    if (specs[i]->type == FAST) {
                        
                        Group group = fast.openGroup(specs[i]->group);
                        DataSet dataset = group.openDataSet("samples");

                        DataSpace dataspace = dataset.getSpace();
                        int rank = dataspace.getSimpleExtentNdims();
                        hsize_t *dims = new hsize_t[rank];
                        dataspace.getSimpleExtentDims(dims);

                        data[i].traces = dims[0];
                        data[i].samples = dims[1];
                        delete [] dims;
                        if (data[i].data) delete [] data[i].data;
                        data[i].data = new uint16_t[data[i].traces*data[i].samples];
                        dataset.read(data[i].data,PredType::NATIVE_UINT16);
                        
                        Group grgroup = fast.openGroup(specs[i]->group.substr(0,specs[i]->group.find("/",specs[i]->group.find("/",1)+1)+1));
                        DataSet sidataset = grgroup.openDataSet("start_index");
                        if (data[i].start_index) delete [] data[i].start_index;
                        data[i].start_index = new uint16_t[data[i].traces];
                        sidataset.read(data[i].start_index,PredType::NATIVE_UINT16);
                    
                        // correct for bottom'd out ADC values
                        const size_t total = data[i].traces*data[i].samples;
                        const uint16_t maxval = specs[i]->maxval;
                        uint16_t *dat = data[i].data;
                        for (size_t j = 0; j < total; j++) {
                            if (dat[j] > maxval) dat[j] = 0;
                        }
                        
                    }
                }
            }
        }
        
        //process the event for each group
        for (size_t i = 0; i < specs.size(); i++) {
            if ((specs[i]->type == MASTER ? mi : fi) != -1) {
                const size_t index = specs[i]->type == MASTER ? mi : fi;
                const size_t offset = index*data[i].samples;
                double pedmean = 0;
                if (specs[i]->pedstart != -1) {
                    uint16_t pedmin = 0xFFFF;
                    uint16_t pedmax = 0;
                    for (int j = specs[i]->pedstart; j < specs[i]->pedend; j++) {
                        const uint16_t val = data[i].data[offset+j];
                        pedmean += val;
                        if (val > pedmax) pedmax = val;
                        if (val < pedmin) pedmin = val;
                    }
                    pedmean /= (specs[i]->pedend - specs[i]->pedstart);
                    intevents[i].pedmean.push_back(1000.0*specs[i]->V_adc*pedmean);
                    if (specs[i]->pedcut > 0) {
                        intevents[i].pedvalid.push_back((pedmax-pedmin)*specs[i]->V_adc*1000.0 < specs[i]->pedcut ? 1 : 0);
                    }
                }
                double sigcharge = 0;
                if (specs[i]->threshold == 0.0) {
                    for (int j = specs[i]->sigstart; j < specs[i]->sigend; j++) {
                        sigcharge += data[i].data[offset+j];
                    }
                } else {
                    bool crossed = false;
                    for (int j = specs[i]->sigstart; j < specs[i]->sigend; j++) {
                        sigcharge += data[i].data[offset+j];
                        if (!crossed && pedmean-data[i].data[offset+j] > specs[i]->threshold) {
                            if (specs[i]->cfdwindow != -1) {
                                const int end = specs[i]->sigend < (j + specs[i]->cfdwindow) ? specs[i]->sigend : (j + specs[i]->cfdwindow);
                                const int begin = specs[i]->sigstart > (j - specs[i]->cfdwindow) ? specs[i]->sigstart : (j - specs[i]->cfdwindow);
                                uint16_t peak = pedmean;
                                for (int k = j; k <= end; k++) if (data[i].data[offset+k] < peak) peak = data[i].data[offset+k];
                                double thresh = round((pedmean-peak)*0.5);
                                if (thresh < specs[i]->threshold) continue;
                                for (int k = begin; k <= end; k++) {
                                    if (pedmean-data[i].data[offset+k] > thresh) {
                                        double prev = pedmean-data[i].data[offset+k-1];
                                        double cur = pedmean-data[i].data[offset+k];
                                        intevents[i].times.push_back(specs[i]->ps_sample*((thresh-prev)/(cur-prev)+k));
                                        crossed = true;
                                        break;
                                    }
                                }
                            } else {
                                double prev = pedmean-data[i].data[offset+j-1];
                                double cur = pedmean-data[i].data[offset+j];
                                intevents[i].times.push_back(specs[i]->ps_sample*((specs[i]->threshold-prev)/(cur-prev)+j));
                                crossed = true;
                            }
                        }
                    }
                    if (crossed) { 
                        //TIME CORRECTION CODE
                        if (specs[i]->type == FAST) {
                            const uint16_t start_cell = data[i].start_index[index];
                            const double crossing = intevents[i].times.back();
                            const double residual = crossing-round(crossing);
                            const size_t sample = round(crossing)/specs[i]->ps_sample;
                            const size_t cell = (start_cell+sample)%1024;
                            const double t0 = specs[i]->group_cell_delays[start_cell];
                            const double tcross = specs[i]->group_cell_delays[cell];
                            const double tnext = specs[i]->group_cell_delays[(cell+1)%1024];
                            const double tfine = (tnext-tcross > 0.0 ? tnext-tcross : tnext-tcross+1.024*specs[i]->ps_sample)*residual;
                            
                            intevents[i].times.back() = 1000.0 * ((tcross - t0 > 0.0 ? tcross - t0 : tcross - t0 + 1.024*specs[i]->ps_sample) + tfine);
                        }
                    } else {
                        intevents[i].times.push_back(-1.0);
                    }
                }
                sigcharge -= pedmean * (specs[i]->sigend - specs[i]->sigstart);
                intevents[i].sigcharge.push_back(-specs[i]->ps_sample * specs[i]->V_adc * sigcharge);
            } else {
                if (specs[i]->pedstart != -1) 
                    intevents[i].pedmean.push_back(0.0);
                if (specs[i]->pedcut > 0)
                    intevents[i].pedvalid.push_back(false);
                intevents[i].sigcharge.push_back(0.0);
                if (specs[i]->threshold != 0.0)
                    intevents[i].times.push_back(-1.0);
            }
        }
        
    }
    
    
    if (ofname.length() == 0) ofname = fprefix + ".int.h5";
    H5File outfile(ofname, H5F_ACC_TRUNC);
    for (size_t i = 0; i < specs.size(); i++) {
        if (verbose) cout << "Creating group " << specs[i]->name << endl;
        Group group = outfile.createGroup(specs[i]->name);    
            
        hsize_t dimensions[1];
        dimensions[0] = intevents[i].sigcharge.size();
        
        DataSpace dspace(1, dimensions);
        
        if (specs[i]->pedstart != -1) {
            if (verbose) cout << "Creating group " << specs[i]->name << "/" << "pedmean" << endl;
            group.createDataSet("pedmean", PredType::NATIVE_DOUBLE, dspace).write(intevents[i].pedmean.data(), PredType::NATIVE_DOUBLE);
        }
        if (specs[i]->pedcut != 0.0) {
            if (verbose) cout << "Creating group " << specs[i]->name << "/" << "pedvalid" << endl;
            group.createDataSet("pedvalid", PredType::NATIVE_UINT8, dspace).write(intevents[i].pedvalid.data(), PredType::NATIVE_UINT8);
        }
        if (specs[i]->threshold != 0.0) {
            if (verbose) cout << "Creating group " << specs[i]->name << "/" << "times" << endl;
            group.createDataSet("times", PredType::NATIVE_DOUBLE, dspace).write(intevents[i].times.data(), PredType::NATIVE_DOUBLE);
        }
        group.createDataSet("sigcharge", PredType::NATIVE_DOUBLE, dspace).write(intevents[i].sigcharge.data(), PredType::NATIVE_DOUBLE);
        
    }
    outfile.close();

}
