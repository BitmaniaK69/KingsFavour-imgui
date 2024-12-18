#pragma once
#include "imgui.h"
#include <string_view>
#include <charconv>
#include <vector>
#include <map>
#include <string>
#include <iostream>

namespace ImGui {
    //Sample
    // TextFormatted("{h1}Titolo Principale{h0}\n{h2}Sottotitolo{h0}\nTesto normale {FF0000}rosso{h2} e poi di nuovo sottotitolo{0000FF}blu{h0} e poi normale");


    // Mappa per associare i tag ai fattori di scala del testo
    static std::map<std::string, float> textScaleMap = {
        {"h0", 1.0f}, // Dimensione normale
        {"h1", 2.0f}, // Titolo principale
        {"h2", 1.5f}, // Sottotitolo
        {"h3", 1.2f} // Titolo di terzo livello (esempio aggiuntivo)
    };

    bool ProcessInlineHexColor(std::string_view hexColorStr, ImVec4& color) {
        if (hexColorStr.empty() || (hexColorStr.size() != 6 && hexColorStr.size() != 8)) {
            std::cerr << "Error: Invalid hex color string length: " << hexColorStr << std::endl;
            return false;
        }

        // Check for invalid characters (optional)
        for (char c : hexColorStr) {
            if (!std::isxdigit(c)) {
                std::cerr << "Error: Invalid character in hex color string: " << c << std::endl;
                return false;
            }
        }

        unsigned int hexColor = 0;
        auto [ptr, ec] = std::from_chars(hexColorStr.data(), hexColorStr.data() + hexColorStr.size(), hexColor, 16);
        if (ec != std::errc{}) {
            std::cerr << "Conversion Error! Value: " << std::hex << hexColor << " for " << hexColorStr << std::endl;
            return false;
        }

        // Extract RGBA channels (considering the right order)
        if (hexColorStr.size() == 8) { // RGBA
            color.w = static_cast<float>((hexColor & 0x000000FF)) / 255.0f;       // Alpha
            color.z = static_cast<float>((hexColor & 0x0000FF00) >> 8) / 255.0f; // Blue
            color.y = static_cast<float>((hexColor & 0x00FF0000) >> 16) / 255.0f; // Green
            color.x = static_cast<float>((hexColor & 0xFF000000) >> 24) / 255.0f; // Red
        }
        else { // RGB
            color.w = 1.0f; // Default Alpha
            color.z = static_cast<float>((hexColor & 0x0000FF)) / 255.0f;         // Blue
            color.y = static_cast<float>((hexColor & 0x00FF00) >> 8) / 255.0f;   // Green
            color.x = static_cast<float>((hexColor & 0xFF0000) >> 16) / 255.0f;  // Red
        }

        return true;
    }


    void TextFormatted(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);

        int bufferSize = std::vsnprintf(nullptr, 0, fmt, args) + 1;
        if (bufferSize <= 0) {
            va_end(args);
            return;
        }

        std::vector<char> buffer(bufferSize);
        va_start(args, fmt);
        int result = std::vsnprintf(buffer.data(), bufferSize, fmt, args);
        va_end(args);
        if (result < 0)
            return;

        std::string formattedText(buffer.begin(), buffer.end() - 1);

        bool pushedStyle = false;
        float currentScale = 1.0f;
        float oldscale = 1.0f;
        size_t start = 0;
        size_t pos = 0;

        while (pos < formattedText.size()) {
            if (formattedText[pos] == '{') {
                if (pos != start) {
                    ImGui::SetWindowFontScale(currentScale); // Applica la scala corrente
                    ImGui::TextUnformatted(formattedText.data() + start, formattedText.data() + pos);
                    ImGui::SameLine(0.0f, 0.0f);
                }

                size_t end = formattedText.find('}', pos + 1);
                if (end == std::string::npos) {
                    break;
                }

                //std::string_view tag = formattedText.substr(pos + 1, end - pos - 1);
                std::string tag = formattedText.substr(pos + 1, end - pos - 1);
                ImVec4 textColor; // Dichiara textColor QUI
                if (ProcessInlineHexColor(tag, textColor)) {
                    if (pushedStyle) {
                        ImGui::PopStyleColor();
                    }
                    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
                    pushedStyle = true;
                }
                else if (textScaleMap.count(std::string(tag))) {
                    oldscale = currentScale;
                    currentScale = textScaleMap[std::string(tag)];
                }

                start = end + 1;
                pos = end;
            }

            else if (formattedText[pos] == '\n' || pos == formattedText.size() - 1) {
                if (pos != start) {

                    std::string textToMeasure;
                    if (formattedText[pos] == '\n')
                        textToMeasure = formattedText.substr(start, pos - start);
                    else
                        textToMeasure = formattedText.substr(start, pos - start + 1);

                    ImGui::PushFont(ImGui::GetFont());
                    float originalScale = ImGui::GetFont()->Scale; //Salvo la scala attuale del font
                    ImGui::GetFont()->Scale = oldscale;
                    ImVec2 textSize = ImGui::CalcTextSize(textToMeasure.data()); // Ora calcolo con la scala corretta!
                    ImGui::GetFont()->Scale = originalScale; //Ripristino la scala originale

                    float yOffset = (ImGui::GetTextLineHeight() - textSize.y) / 2.0f; // Calcolo l'offset
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + yOffset); // Applica l'offset (corretto)
                    ImGui::TextUnformatted(textToMeasure.data());
                    ImGui::PopFont();


                }
                start = pos + 1;
            }
            ++pos;
        }


        if (pushedStyle) {
            ImGui::PopStyleColor();
        }

        ImGui::SetWindowFontScale(1.0f);
    }
}
