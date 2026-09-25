#pragma once

#include <wx/dialog.h>
#include <functional>
#include <string>

namespace Slic3r { namespace GUI {
// The UI is independent of the slicer so its process lifecycle can be tested
// without loading a printer or the rendering engine.
wxDialog* create_codex_assistant(wxWindow* parent, const wxString& executable,
    const wxString& state_dir, std::function<std::string()> snapshot,
    std::function<void(const wxString&)> save_executable, bool settings = false);
void focus_codex_assistant(wxDialog* dialog, bool settings);
void show_slicepilot_assistant(wxWindow* parent, bool settings = false);
}}
