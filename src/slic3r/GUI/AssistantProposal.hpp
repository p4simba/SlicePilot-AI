#pragma once
#include "CodexAssistant.hpp"
#include <cmath>
#include <regex>
#include <set>
#include <stdexcept>

namespace Slic3r { namespace GUI {
// Deliberately limited to scalar global process settings; no G-code, credentials,
// temperatures, printer configuration, or object-specific edits.
inline void validate_assistant_changes(const std::vector<AssistantChange>& changes)
{
    if (changes.empty() || changes.size() > 8)
        throw std::runtime_error("A proposta deve conter entre 1 e 8 alterações.");
    const std::set<std::string> allowed = {"layer_height", "wall_loops", "top_shell_layers",
        "bottom_shell_layers", "sparse_infill_density", "brim_width"};
    std::set<std::string> seen;
    for (const auto& change : changes) {
        if (!allowed.count(change.key) || !seen.insert(change.key).second)
            throw std::runtime_error("A proposta contém parâmetros não suportados ou repetidos. Solicite outra proposta.");
        const bool percent = change.key == "sparse_infill_density";
        const bool integer = change.key == "wall_loops" || change.key == "top_shell_layers" || change.key == "bottom_shell_layers";
        const std::regex format(integer ? "[0-9]+" : percent ? "[0-9]+(\\.[0-9]+)?%" : "[0-9]+(\\.[0-9]+)?");
        // Check both sides so every accepted proposal has a supported inverse.
        for (const auto& serialized : {change.before, change.after}) {
            if (serialized.size() > 64 || !std::regex_match(serialized, format))
                throw std::runtime_error("A proposta contém um valor inválido.");
            double value;
            try { value = std::stod(serialized); }
            catch (...) { throw std::runtime_error("O valor proposto está fora dos limites."); }
            if (!std::isfinite(value) || value < 0 || (percent && value > 100) ||
                (integer && value > 100) || (change.key == "brim_width" && value > 100) ||
                (change.key == "layer_height" && (value <= 0 || value > 2)))
                throw std::runtime_error("O valor proposto está fora dos limites de aplicação do assistente.");
        }
    }
}
}}
