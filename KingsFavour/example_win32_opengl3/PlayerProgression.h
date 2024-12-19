#pragma once
#include <map>
#include <vector>
#include <string>
#include <imgui.h>
#include "IconsFontAwesome6.h"

#define PLOG(MSG)  pLogStream.str(""); pLogStream.clear(); pLogStream << MSG
#define PLOG_ERR(MSG)  pLogStream_err.str(""); pLogStream_err.clear(); pLogStream_err
#define PLOG_FLUSH(TAG, TURN)  pLog(TAG,TURN, pLogStream.str());
#define PLOG_ERR_FLUSH(TAG, TURN)  pLog(TAG,TURN, pLogStream_err.str());


struct Progression {
    int turn;
    std::string action;
};

std::map<std::string, std::vector<Progression>> playerProgression;

std::ostringstream pLogStream;
std::ostringstream pLogStream_err;

// playerProgression[d.name].push_back({ turn, std::format("Place {} Card. Used:{}/{} - OnBoard:{}", quantity, cardsInDeck - d.cards, d.cards, d.inGameCards) });
void pLog(const std::string& tag, int turn, const std::string& message )
{
    playerProgression[tag].push_back({ turn, message });
}

void renderPlayerProgressionPanelByTurn(int numPlayers)
    {
    // Begin a new ImGui window for the player progression
    ImGui::Begin("Turn Progression");

    ImGui::Text("Progression Log by Turn");
    ImGui::Separator();

    // Determine the maximum turn across all players
    int maxTurn = 0;
    for (const auto& player : playerProgression) {
        for (const auto& log : player.second) {
            if (log.turn > maxTurn) {
                maxTurn = log.turn;
            }
        }
    }

    bool shouldScroll = false; // Track if we should scroll to the bottom

    // Iterate through each turn
    for (int turn = 0; turn <= maxTurn; ++turn) {
        bool printed = false;

        // For each turn, display the actions of all players
        for (const auto& player : playerProgression) {
            const std::string& playerName = player.first;
            const std::vector<Progression>& progressionLog = player.second;

            // Find actions for this turn
            for (const auto& log : progressionLog) {
                if (log.turn == turn) {
                    if (!printed) {
                        int currentRound = turn / numPlayers;

                        // Retrieve the color for the Daimyo
                        ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // Default white
                       /* if (daimyoColors.find(playerName) != daimyoColors.end()) {
                            color = daimyoColors.at(playerName);
                        }*/

                        // Display the player's turn with the colored name or flag
                        ImGui::TextColored(color, ICON_FA_FLAG); // Colored flag icon
                        ImGui::SameLine();
                        ImGui::TextColored(color, "%s's Turn %d - Round %d", playerName.c_str(), turn, currentRound + 1);
                        printed = true;
                    }
                    ImGui::BulletText("%s", log.action.c_str());
                }
            }
        }
        if (printed) {
            ImGui::Separator();
            shouldScroll = true; // Set the scroll flag if there are new entries
        }
        ImGui::Spacing();  // Add some space between different turns
    }

    // Scroll to the bottom if there are new entries
    if (shouldScroll && (ImGui::IsWindowAppearing() || !ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::End();  // End the ImGui window
}

/*void renderPlayerProgressionPanel(const std::map<std::string, std::vector<Progression>>& playerProgression,
    int numPlayers,
    const std::map<std::string, ImVec4>& daimyoColors,
    const std::vector<Daimyo>& daimyoList) {
    // Begin a new ImGui window for the player progression
    ImGui::Begin("Player Progression");
    int totalShown = 0;
    // Static variable to keep track of the selected tab
    static std::string selectedTab;

    // Fixed non-scrollable header
    ImGui::BeginChild("TabBar", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 2), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // Begin the tab bar for filtering Daimyo
    if (ImGui::BeginTabBar("DaimyoTabs")) {
        for (const auto& d : daimyoList) {
            if (totalShown >= numPlayers) break;
            ++totalShown;
            const std::string& playerName = d.name;

            // Start a tab for each Daimyo in order of the list
            if (ImGui::BeginTabItem(playerName.c_str())) {
                selectedTab = playerName; // Update the selected tab
                ImGui::EndTabItem();
            }
        }
        // End the tab bar
        ImGui::EndTabBar();
    }
    ImGui::EndChild();

    // Scrollable content
    ImGui::BeginChild("ScrollableContent", ImVec2(0, 0), true);
    if (!selectedTab.empty() && playerProgression.find(selectedTab) != playerProgression.end()) {
        const std::vector<Progression>& progressionLog = playerProgression.at(selectedTab);

        // Retrieve the color for the Daimyo
        ImVec4 color = daimyoColors.at(selectedTab);

        // Display the player's name with color or a flag
        ImGui::TextColored(color, ICON_FA_FLAG); // Display a colored flag
        ImGui::SameLine();
        ImGui::TextColored(color, "Player: %s", selectedTab.c_str());
        ImGui::Separator();

        // Display each progression log for the selected player
        int preRound = 0;
        for (const auto& log : progressionLog) {
            int currentRound = log.turn / numPlayers;
            if (currentRound > preRound) {
                ImGui::Separator();
                ImGui::Text("Round %d - Turn %d:", currentRound, log.turn);
                preRound = currentRound;
            }
            ImGui::BulletText(log.action.c_str());
        }
        ImGui::Spacing();
    }
    else {
        ImGui::Text("No progression data available for the selected Daimyo.");
    }
    ImGui::EndChild();

    ImGui::End(); // End the ImGui window
}

*/
