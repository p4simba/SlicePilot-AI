#include "CodexAssistant.hpp"
#include "GUI_App.hpp"
#include "Plater.hpp"
#include "libslic3r/AppConfig.hpp"
#include "libslic3r/Model.hpp"
#include "libslic3r/PresetBundle.hpp"
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/weakref.h>
#include <nlohmann/json.hpp>

namespace Slic3r { namespace GUI {
namespace {
using Json = nlohmann::json;
// An allowlist prevents printer passwords, hostnames and custom G-code from
// entering model context. Values retain Orca's serialization and units.
const char* keys[] = {"printer_model", "printer_technology", "nozzle_diameter", "filament_type",
    "layer_height", "initial_layer_print_height", "wall_loops", "top_shell_layers", "bottom_shell_layers",
    "sparse_infill_density", "sparse_infill_pattern", "enable_support", "support_type", "support_threshold_angle",
    "brim_type", "brim_width", "nozzle_temperature", "nozzle_temperature_initial_layer", "hot_plate_temp",
    "hot_plate_temp_initial_layer", "filament_flow_ratio", "filament_max_volumetric_speed", "outer_wall_speed",
    "inner_wall_speed", "retraction_length", "retraction_speed", "extruder"};
template<class Config> Json selected_settings(const Config& config) {
    Json result = Json::object();
    for (const auto key : keys) if (const auto option = config.option(key)) result[key] = option->serialize();
    return result;
}
std::string live_context() {
    auto& app = wxGetApp();
    if (!app.plater() || !app.preset_bundle) throw std::runtime_error("No open project");
    Json objects = Json::array();
    for (const auto object : app.plater()->model().objects) {
        if (objects.size() >= 100) throw std::runtime_error("Too many objects");
        Json instances = Json::array(), volumes = Json::array();
        for (size_t i = 0; i < object->instances.size(); ++i) {
            if (instances.size() >= 100) throw std::runtime_error("Too many instances");
            const auto size = object->instance_bounding_box(i).size();
            instances.push_back({{"index", i}, {"size_mm", {size.x(), size.y(), size.z()}}});
        }
        for (const auto volume : object->volumes) {
            if (volumes.size() >= 100) throw std::runtime_error("Too many volumes");
            volumes.push_back({{"index", volumes.size()}, {"overrides", selected_settings(volume->config)}});
        }
        objects.push_back({{"index", objects.size()}, {"printable", object->printable},
            {"overrides", selected_settings(object->config)}, {"instances", instances}, {"volumes", volumes},
            {"has_height_range_overrides", !object->layer_config_ranges.empty()}});
    }
    Json result = {{"schema_version", 1}, {"source", "open_project_including_unsaved_changes"},
        {"global_settings", selected_settings(app.preset_bundle->full_config())}, {"objects", objects},
        {"limitations", {"No mesh, image or toolpath analysis", "No plate-specific or height-range setting resolution",
                         "Overrides are separate from global settings, not merged effective values", "No slicing estimates or physical validation"}}};
    const auto serialized = result.dump(2);
    if (serialized.size() > 256 * 1024) throw std::runtime_error("Context too large");
    return serialized;
}
}
void show_slicepilot_assistant(wxWindow* parent, bool settings) {
    static wxWeakRef<wxDialog> dialog;
    if (dialog) { focus_codex_assistant(dialog, settings); return; }
    auto& app = wxGetApp();
    wxString executable = wxString::FromUTF8(app.app_config->get("slicepilot_codex_executable"));
    if (executable.empty()) {
        wxPathList search; search.AddEnvList("PATH");
        search.Add("/opt/homebrew/bin"); search.Add("/usr/local/bin");
#ifdef _WIN32
        executable = search.FindAbsoluteValidPath("codex.exe");
#else
        executable = search.FindAbsoluteValidPath("codex");
#ifdef __APPLE__
        if (executable.empty() && wxFileExists("/Applications/ChatGPT.app/Contents/Resources/codex"))
            executable = "/Applications/ChatGPT.app/Contents/Resources/codex";
#endif
#endif
    }
    const wxString state = wxStandardPaths::Get().GetUserLocalDataDir() + wxFILE_SEP_PATH + "slicepilot-codex";
    // Own the modeless window from the main frame, not the transient preferences.
    dialog = create_codex_assistant(parent, executable, state, live_context,
        [](const wxString& value) { wxGetApp().app_config->set("slicepilot_codex_executable", std::string(value.ToUTF8())); wxGetApp().app_config->save(); }, settings);
    dialog->Show();
}
}}
