// Standalone native UI test host. Compile with CodexAssistant.cpp and wxWidgets.
// The child-process mode implements a synthetic Codex peer, never an AI service.
#include "slic3r/GUI/CodexAssistant.hpp"
#include <wx/app.h>
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/textctrl.h>
#include <wx/timer.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <cstdlib>

using Json = nlohmann::json;
static bool self_test = false;
static bool invalid_login_test = false;
static int test_exit = 0;
static int fake_server() {
    auto emit = [](const Json& value) { std::cout << value.dump() << std::endl; };
    bool connected = std::getenv("SLICEPILOT_TEST_INVALID_LOGIN") == nullptr;
    for (std::string line; std::getline(std::cin, line);) {
        const auto message = Json::parse(line);
        if (!message.contains("id")) continue;
        const auto method = message.value("method", std::string());
        Json result = Json::object();
        if (method == "initialize") result = {{"userAgent", "slicepilot-ui-test"}};
        else if (method == "account/read") result = {{"account", connected ? Json({{"type", "chatgpt"}}) : Json(nullptr)}};
        else if (method == "account/logout") connected = false;
        else if (method == "account/login/start") result = {{"loginId", "test-login"}, {"authUrl", "https://example.invalid/login"}};
        else if (method == "account/login/cancel") result = Json::object();
        else if (method == "thread/start") result = {{"thread", {{"id", "test-thread"}}}};
        else if (method == "turn/start") {
            const auto params = message.at("params");
            const auto prompt = params.at("input").at(0).at("text").get<std::string>();
            if (prompt.find("synthetic_test_fixture") == std::string::npos ||
                params.at("sandboxPolicy").at("type") != "readOnly" ||
                params.at("approvalPolicy") != "never") return 2;
            emit({{"id", message["id"]}, {"result", {{"turn", {{"id", "test-turn"}}}}}});
            emit({{"method", "item/agentMessage/delta"}, {"params", {{"threadId", "test-thread"},
                {"delta", "Resposta de teste: recebi o contexto da peça de 20 mm. Nenhuma configuração foi alterada."}}}});
            emit({{"method", "turn/completed"}, {"params", {{"threadId", "test-thread"}, {"turn", {{"status", "completed"}}}}}});
            continue;
        } else {
            emit({{"id", message["id"]}, {"error", {{"code", -32601}, {"message", "Unsupported test operation"}}}}); continue;
        }
        emit({{"id", message["id"]}, {"result", result}});
    }
    return 0;
}
class SmokeApp : public wxApp {
    wxTimer timer{this};
    wxDialog* dialog = nullptr;
    int stage = 0, ticks = 0;
    void click(const wxString& label) {
        auto button = dynamic_cast<wxButton*>(wxWindow::FindWindowByLabel(label, dialog));
        if (!button || !button->IsEnabled()) return;
        wxCommandEvent event(wxEVT_BUTTON, button->GetId());
        event.SetEventObject(button); button->GetEventHandler()->ProcessEvent(event);
    }
    bool contains(wxWindow* window, const wxString& value) {
        if (window->GetLabel().Contains(value)) return true;
        if (auto control = dynamic_cast<wxTextCtrl*>(window)) if (control->GetValue().Contains(value)) return true;
        for (auto child : window->GetChildren()) if (contains(child, value)) return true;
        return false;
    }
    wxTextCtrl* question(wxWindow* window) {
        if (auto control = dynamic_cast<wxTextCtrl*>(window)) if (control->IsEditable() && control->IsMultiLine()) return control;
        for (auto child : window->GetChildren()) if (auto found = question(child)) return found;
        return nullptr;
    }
    void tick() {
        if (++ticks > 200) { std::cerr << "UI test timed out at stage " << stage << std::endl; test_exit = 1; timer.Stop(); dialog->Close(); return; }
        if (stage == 0) {
            click("Visualizar dados do projeto");
            if (!contains(dialog, "synthetic_test_fixture")) { test_exit = 1; timer.Stop(); dialog->Close(); return; }
            click("Conectar"); stage = 1;
        } else if (stage == 1 && invalid_login_test && contains(dialog, wxString::FromUTF8("endereço de login não reconhecido"))) {
            std::cout << "PASS: untrusted login URL rejected before opening browser" << std::endl;
            timer.Stop(); dialog->Close();
        } else if (stage == 1 && contains(dialog, wxString::FromUTF8("Codex conectado à sua conta."))) {
            auto share = dynamic_cast<wxCheckBox*>(wxWindow::FindWindowByLabel(wxString::FromUTF8("Enviar os parâmetros e dimensões do projeto ao Codex com minhas perguntas"), dialog));
            share->SetValue(true);
            wxCommandEvent change(wxEVT_CHECKBOX, share->GetId()); share->GetEventHandler()->ProcessEvent(change);
            question(dialog)->SetValue(wxString::FromUTF8("Como melhorar esta impressão de teste?")); click("Enviar pergunta"); stage = 2;
        } else if (stage == 2 && contains(dialog, wxString::FromUTF8("Análise concluída."))) {
            if (!contains(dialog, wxString::FromUTF8("Resposta de teste: recebi o contexto"))) test_exit = 1;
            click("Desconectar"); stage = 3;
        } else if (stage == 3 && contains(dialog, "Desconectado. Clique")) {
            std::cout << "PASS: native connect, project preview, context send, streaming, completion, logout" << std::endl;
            timer.Stop(); dialog->Close();
        }
    }
public:
    bool OnInit() override {
        dialog = Slic3r::GUI::create_codex_assistant(nullptr,
            wxStandardPaths::Get().GetExecutablePath(),
            wxFileName::GetTempDir() + "/slicepilot-native-ui-test",
            [] { return Json({{"source", "synthetic_test_fixture"}, {"layer_height", "0.2"}, {"size_mm", {20, 20, 20}}}).dump(2); },
            [](const wxString&) {});
        dialog->SetTitle(wxString::FromUTF8("SlicePilot AI — TESTE LOCAL (sem IA)"));
        dialog->Show(); SetTopWindow(dialog);
        if (self_test) { Bind(wxEVT_TIMER, [this](wxTimerEvent&) { tick(); }); timer.Start(100); }
        return true;
    }
};
wxIMPLEMENT_APP_NO_MAIN(SmokeApp);
int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "app-server") return fake_server();
    invalid_login_test = argc > 1 && std::string(argv[1]) == "--self-test-invalid-login";
    if (invalid_login_test) wxSetEnv("SLICEPILOT_TEST_INVALID_LOGIN", "1");
    self_test = invalid_login_test || (argc > 1 && std::string(argv[1]) == "--self-test");
    if (self_test) argc = 1;
    const auto result = wxEntry(argc, argv);
    return result ? result : test_exit;
}
