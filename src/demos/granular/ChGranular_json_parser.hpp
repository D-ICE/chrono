#include "chrono_granular/physics/ChGranular.h"
#include "chrono_thirdparty/rapidjson/filereadstream.h"
#include "chrono_thirdparty/rapidjson/document.h"

#include <string>
#include <iostream>

using std::string;
using std::cout;
using std::endl;

using namespace chrono;
using namespace chrono::granular;
using namespace rapidjson;

typedef struct sim_param_holder {
    float sphere_radius;
    float sphere_density;
    float box_X;
    float box_Y;
    float box_Z;
    float step_size;
    float time_end;
    float grav_X;
    float grav_Y;
    float grav_Z;
    float normalStiffS2S;
    float normalStiffS2W;
    float normalStiffS2M;
    float normalDampS2S;
    float normalDampS2M;
    float cohesion_ratio;
    bool verbose;
    int run_mode;
    unsigned int psi_h;
    unsigned int psi_T;
    unsigned int psi_L;
    GRN_TIME_STEPPING step_mode;
    string output_dir;
    GRN_OUTPUT_MODE write_mode;
} sim_param_holder;

void ShowJSONUsage() {
    cout << "JSON fields:" << endl;
    cout << "sphere_radius" << endl;
    cout << "sphere_density" << endl;
    cout << "box_X" << endl;
    cout << "box_Y" << endl;
    cout << "box_Z" << endl;
    cout << "step_size" << endl;
    cout << "time_end" << endl;
    cout << "grav_X" << endl;
    cout << "grav_Y" << endl;
    cout << "grav_Z" << endl;
    cout << "normalStiffS2S" << endl;
    cout << "normalStiffS2W" << endl;
    cout << "normalStiffS2M" << endl;
    cout << "normalDampS2S" << endl;
    cout << "normalDampS2M" << endl;
    cout << "cohesion_ratio" << endl;
    cout << "verbose" << endl;
    cout << "psi_h" << endl;
    cout << "psi_T" << endl;
    cout << "psi_L" << endl;
    cout << "output_dir" << endl;
    cout << "write_mode (csv, binary, or none)" << endl;
}

void InvalidArg(string arg) {
    cout << "Invalid arg: " << arg << endl;
    ShowJSONUsage();
}

// Returns true on successful parameter load.
// Returns false and prints error on invalid argument.
bool ParseJSON(const char* json_file, sim_param_holder& params) {
    FILE* fp = fopen(json_file, "r");
    if (!fp) {
        cout << "Invalid JSON file" << endl;
        return false;
    }

    char readBuffer[65536];
    FileReadStream is(fp, readBuffer, sizeof(readBuffer));
    fclose(fp);

    Document doc;
    doc.ParseStream<ParseFlag::kParseCommentsFlag>(is);
    if (!doc.IsObject()) {
        cout << "Invalid JSON file" << endl;
        return false;
    }

    cout << "--- Parsing JSON ---" << endl;
    if (doc.HasMember("sphere_radius") && doc["sphere_radius"].IsNumber()) {
        params.sphere_radius = doc["sphere_radius"].GetDouble();
        cout << "params.sphere_radius " << params.sphere_radius << endl;
    }
    if (doc.HasMember("sphere_density") && doc["sphere_density"].IsNumber()) {
        params.sphere_density = doc["sphere_density"].GetDouble();
        cout << "params.sphere_density " << params.sphere_density << endl;
    }
    if (doc.HasMember("box_X") && doc["box_X"].IsNumber()) {
        params.box_X = doc["box_X"].GetDouble();
        cout << "params.box_X " << params.box_X << endl;
    }
    if (doc.HasMember("box_Y") && doc["box_Y"].IsNumber()) {
        params.box_Y = doc["box_Y"].GetDouble();
        cout << "params.box_Y " << params.box_Y << endl;
    }
    if (doc.HasMember("box_Z") && doc["box_Z"].IsNumber()) {
        params.box_Z = doc["box_Z"].GetDouble();
        cout << "params.box_Z " << params.box_Z << endl;
    }
    if (doc.HasMember("time_end") && doc["time_end"].IsNumber()) {
        params.time_end = doc["time_end"].GetDouble();
        cout << "params.time_end " << params.time_end << endl;
    }
    if (doc.HasMember("grav_X") && doc["grav_X"].IsNumber()) {
        params.grav_X = doc["grav_X"].GetDouble();
        cout << "params.grav_X " << params.grav_X << endl;
    }
    if (doc.HasMember("grav_Y") && doc["grav_Y"].IsNumber()) {
        params.grav_Y = doc["grav_Y"].GetDouble();
        cout << "params.grav_Y " << params.grav_Y << endl;
    }
    if (doc.HasMember("grav_Z") && doc["grav_Z"].IsNumber()) {
        params.grav_Z = doc["grav_Z"].GetDouble();
        cout << "params.grav_Z " << params.grav_Z << endl;
    }
    if (doc.HasMember("normalStiffS2S") && doc["normalStiffS2S"].IsNumber()) {
        params.normalStiffS2S = doc["normalStiffS2S"].GetDouble();
        cout << "params.normalStiffS2S " << params.normalStiffS2S << endl;
    }
    if (doc.HasMember("normalStiffS2W") && doc["normalStiffS2W"].IsNumber()) {
        params.normalStiffS2W = doc["normalStiffS2W"].GetDouble();
        cout << "params.normalStiffS2W " << params.normalStiffS2W << endl;
    }
    if (doc.HasMember("normalStiffS2M") && doc["normalStiffS2M"].IsNumber()) {
        params.normalStiffS2M = doc["normalStiffS2M"].GetDouble();
        cout << "params.normalStiffS2M " << params.normalStiffS2M << endl;
    }
    if (doc.HasMember("normalDampS2S") && doc["normalDampS2S"].IsNumber()) {
        params.normalDampS2S = doc["normalDampS2S"].GetDouble();
        cout << "params.normalDampS2S " << params.normalDampS2S << endl;
    }
    if (doc.HasMember("normalDampS2M") && doc["normalDampS2M"].IsNumber()) {
        params.normalDampS2M = doc["normalDampS2M"].GetDouble();
        cout << "params.normalDampS2M " << params.normalDampS2M << endl;
    }
    if (doc.HasMember("cohesion_ratio") && doc["cohesion_ratio"].IsNumber()) {
        params.cohesion_ratio = doc["cohesion_ratio"].GetDouble();
        cout << "params.cohesion_ratio " << params.cohesion_ratio << endl;
    }
    if (doc.HasMember("verbose") && doc["verbose"].IsBool()) {
        params.verbose = doc["verbose"].GetBool();
        cout << "params.verbose " << params.verbose << endl;
    }
    if (doc.HasMember("psi_h") && doc["psi_h"].IsInt()) {
        params.psi_h = doc["psi_h"].GetInt();
        cout << "params.psi_h " << params.psi_h << endl;
    }
    if (doc.HasMember("psi_T") && doc["psi_T"].IsInt()) {
        params.psi_T = doc["psi_T"].GetInt();
        cout << "params.psi_T " << params.psi_T << endl;
    }
    if (doc.HasMember("psi_L") && doc["psi_L"].IsInt()) {
        params.psi_L = doc["psi_L"].GetInt();
        cout << "params.psi_L " << params.psi_L << endl;
    }
    GRN_TIME_STEPPING step_mode;
    if (doc.HasMember("step_mode") && doc["step_mode"].IsString()) {
        if (doc["step_mode"].GetString() == string("fixed")) {
            params.step_mode = GRN_TIME_STEPPING::FIXED;
            cout << "params.step_mode " << params.step_mode << endl;
        } else if (doc["step_mode"].GetString() == string("auto")) {
            params.step_mode = GRN_TIME_STEPPING::AUTO;
            cout << "params.step_mode " << params.step_mode << endl;
        } else {
            InvalidArg("step_mode");
            return false;
        }
    }
    if (doc.HasMember("output_dir") && doc["output_dir"].IsString()) {
        params.output_dir = doc["output_dir"].GetString();
        cout << "params.output_dir " << params.output_dir << endl;
    }
    if (doc.HasMember("write_mode") && doc["write_mode"].IsString()) {
        if (doc["write_mode"].GetString() == string("binary")) {
            params.write_mode = GRN_OUTPUT_MODE::BINARY;
            cout << "params.write_mode " << params.write_mode << endl;
        } else if (doc["write_mode"].GetString() == string("csv")) {
            params.write_mode = GRN_OUTPUT_MODE::CSV;
            cout << "params.write_mode " << params.write_mode << endl;
        } else if (doc["write_mode"].GetString() == string("none")) {
            params.write_mode = GRN_OUTPUT_MODE::NONE;
            cout << "params.write_mode " << params.write_mode << endl;
        } else {
            InvalidArg("write_mode");
            return false;
        }
    }
    if (doc.HasMember("step_size") && doc["step_size"].IsNumber()) {
        params.step_size = doc["step_size"].GetDouble();
        cout << "params.step_size " << params.step_size << endl;
    }
    cout << "--------------------" << endl;

    // TODO sanity checks
    // necessary parameters
    // step_mode
    return true;
}