// Dear ImGui: standalone example application for Win32 + OpenGL 3

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

// This is provided for completeness, however it is strongly recommended you use OpenGL with SDL or GLFW.

#include "imgui.h"
#include "implot.h"
#include "IconsFontAwesome6.h"
#include "fa.h"

#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"

#include "text_formatted.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define NOMINMAX

#include <windows.h>
#include <GL/GL.h>
#include <tchar.h>
#include <vector>
#include <map>
#include <algorithm>
//#include <format>

#include <sstream>
#include <string>
#include <iostream>

#include <functional>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "json.hpp"
#include <fstream>

#include <filesystem>
#include <random>
#include <ctime>
#include <utility>
#include <limits>
#include <cstdint> // uint_fast8_t

// ImGui and GLFW
//#include "imgui_impl_glfw.h"
//#include <GLFW/glfw3.h>


using namespace std;
using json = nlohmann::json;

// Data stored per platform window
struct WGL_WindowData { HDC hDC; };

// Data
static HGLRC            g_hRC;
static WGL_WindowData   g_MainWindow;
static int              g_Width;
static int              g_Height;

// Forward declarations of helper functions
bool CreateDeviceWGL(HWND hWnd, WGL_WindowData* data);
void CleanupDeviceWGL(HWND hWnd, WGL_WindowData* data);
void ResetDeviceWGL();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Windows specific functions (for window creation and message handling)

//***********************************************
ImVec4 clear_color = ImVec4(0, 0.0f / 255.0f, 19.0f / 255.0f, 1.00f);

// Card structure to hold card details
struct Card {
    std::string value;
    std::string suit;
    int pointValue;
    int order;
};


// Player structure to hold relevant details
struct Player {
    std::string name;
    int coins = 1; // Starting coins
    int points = 0; // Points accumulated
    int missions = 1; // Missions completed
    std::vector<Card> hand; // Cards in hand
    std::vector<Card> wonCards; // Cards won by the player
    Card swapCard;
    //stats
    int lastScore = 0;
    int goodPoints = 0;
    int negativePoints = 0;
    int parliaments = 0;
    int swappedThisTurn = 0;
};

using playedCardsType = std::vector<std::pair<std::string, Card>>;

//------------------------------------------------------------------------------------
static thread_local std::mt19937 rng(std::random_device{}());
static thread_local std::uniform_int_distribution<int> dist(1, 100);

inline bool chance(uint_fast8_t percent) noexcept {
    return dist(rng) <= percent;
}
//--------------------------------------------------------------------------------------

GLuint LoadTexture(const char* filename, int& width, int& height) {
    int channels;
    unsigned char* data = stbi_load(filename, &width, &height, &channels, 4); // Load as RGBA

    if (!data) {
        std::cerr << "Failed to load image: " << filename << std::endl;
        return 0;
    }

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);  // Free the image memory
    return textureID;
}


void RenderBackgroundWithAspectRatio(GLuint textureID, int imgWidth, int imgHeight) {

    ImVec2 windowPos = ImGui::GetMainViewport()->Pos;  // Position of the main viewport
    ImVec2 windowSize = ImGui::GetMainViewport()->Size; // Size of the window

    // Calculate the scaling factor to maintain aspect ratio
    float windowAspect = windowSize.x / windowSize.y;
    float imageAspect = (float)imgWidth / (float)imgHeight;

    float scaleX = windowSize.x;
    float scaleY = windowSize.y;

    // Scale the image to fit the window while maintaining aspect ratio
    if (imageAspect > windowAspect) {
        // Image is wider than the window: scale based on width
        scaleY = windowSize.x / imageAspect;
    }
    else {
        // Image is taller than the window: scale based on height
        scaleX = windowSize.y * imageAspect;
    }

    // Calculate the position for centering the image
    ImVec2 imagePos = ImVec2(windowPos.x + (windowSize.x - scaleX) * 0.5f, windowPos.y + (windowSize.y - scaleY) * 0.5f);

    // Get the ImDrawList for the background
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();

    // Draw the image with the calculated size and position
    drawList->AddImage(
        (ImTextureID)(intptr_t)textureID,                // Texture ID
        imagePos,                                       // Top-left position
        ImVec2(imagePos.x + scaleX, imagePos.y + scaleY) // Bottom-right position
    );
}
//------------------------------------------------------------------------------------

std::string iconify(const std::string& name) {
    static std::map<std::string, std::string> suitsSymbols = {
        {"Crowns", ICON_FA_CROWN},
        {"Weapons", ICON_FA_TROWEL},
        {"Faith", ICON_FA_CROSS},
        {"Diamonds", ICON_FA_GEM},
        {"Betrays", ICON_FA_DRAGON},
        {"Guards", ICON_FA_SHIELD_HALVED},
        {"Parliament", ICON_FA_BUILDING_COLUMNS}
    };

    auto it = suitsSymbols.find(name);
    if (it != suitsSymbols.end()) {
        return it->second;
    }
    else {
        return "";
    }
}

// Functions for initializing, shuffling, and dealing the deck
std::vector<Card> initializeDeck();

void shuffleDeck(std::vector<Card>& deck);

void dealToHands(std::vector<Player>& players, std::vector<Card>& deck);

// Functions for playing and determining the winner of a hand
Card chooseCardToPlay(Player& player, const std::string& suitToFollow);
playedCardsType playHand(std::vector<Player>& players, const std::string& suitToFollow);
std::string determineWinner(const playedCardsType& playedCards, const std::string& suitToFollow);

// Function to handle player swapping cards
void playerRandomSwap(std::vector<Player>& players, int& coinsInTreasury);

// Function to simulate a single turn of the game
void simulateTurn(int turn, int& coinsInTreasury, std::vector<Player>& players, std::vector<Card>& deck);

// UI Rendering Functions (using Dear ImGui as an example)
bool renderPlayerPanel(Player& player, playedCardsType& playedCards, const std::string& currentPlayer, bool justShow,bool& automove, std::vector<Player>& allPlayers, int& coinsInTreasury, const std::string& suitToFollow, bool handCompleted);
bool renderPlayerPanel2(Player& player, playedCardsType& playedCards, const std::string& currentPlayer, bool justShow);

// Helper function to get the color corresponding to the suit (For UI)
ImVec4 getSuitColor(const std::string& suit);

// Function to determine the winner of the hand and update the winner hand, managing also special cases (Parliament, guard)
bool determineWinnerAndResetHand(playedCardsType& playedCards, std::vector<Player>& players, int& coinsInTreasury, bool isLastHand, Player& winner);

void CalculateScore(playedCardsType& playedCards, Player& winner);

// Function to determine the winner and update the winner hand - Previous version
bool determineWinnerAndResetHand2(playedCardsType& playedCards, std::vector<Player>& players, int& coinsInTreasury, bool isLastHand, Player& winner);

void dealNewCards(std::vector<Player>& players, std::vector<Card>& deck);

//------------------------------------------------------------------------------------

int getPlayerIndexByName(const std::vector<Player>& players, const std::string& name) {
    for (size_t i = 0; i < players.size(); ++i) {
        if (players[i].name == name) {
            return static_cast<int>(i); // Return the index if the name matches
        }
    }
    return -1; // Return -1 if the player is not found
}
int getNextPlayerIndex(int currentPlayerIndex, int totalPlayers) {
    return (currentPlayerIndex + 1) % totalPlayers; // Circular turn order
}
void renderGamePanel(const playedCardsType& playedCards, bool& automove, bool handCompleted) {
    ImGui::Begin("Game Panel");
    //TextFormatted("{h1}Titolo Principale{h0}\n{h2}Sottotitolo{h0}\nTesto normale {FF0000}rosso{h2} e poi di nuovo sottotitolo{0000FF}blu{h0} e poi normale");
   // TextFormatted("Testo normale {FF0000}rosso{FFFFFF} e poi normale");
    ImGui::Text("Cards Played This Turn:");
    ImGui::Separator();

    for (const auto& [playerName, card] : playedCards) {
        ImGui::PushStyleColor(ImGuiCol_Text, getSuitColor(card.suit)); // Color based on the suit
        ImGui::Text("%s played %s of %s %s  (%+d points)",
            playerName.c_str(),
            card.value.c_str(),
            card.suit.c_str(),
            iconify(card.suit).c_str(),
            card.pointValue); // Card value, suit, and point value*/

       /* TextFormatted("%s played {h1}%s{h0} of %s {h1}%s{h0}  (%+d points)",
            playerName.c_str(),
            card.value.c_str(),
            card.suit.c_str(),
            iconify(card.suit).c_str(),
            card.pointValue);*/

        ImGui::PopStyleColor();
    }
    if (!handCompleted)
    {
       ImGui::Checkbox("Auto Move", &automove);
    }
    ImGui::Separator();
    ImGui::End();
}

//------------------------------------------------------------------------------------
// MAIN
//------------------------------------------------------------------------------------
int main() {
     // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = { sizeof(wc), CS_OWNDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Dear ImGui Win32+OpenGL3 Example", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize OpenGL
    if (!CreateDeviceWGL(hwnd, &g_MainWindow))
    {
        CleanupDeviceWGL(hwnd, &g_MainWindow);
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }
    wglMakeCurrent(g_MainWindow.hDC, g_hRC);

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;   // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;    // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();
    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    ImFontConfig icons_config;
    ImFontConfig font_config;
    icons_config.MergeMode = true; // Merge icon font to the previous font if you want to have both icons and text
    //io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Verdana.ttf", 16.0f);
    io.Fonts->AddFontDefault();
    io.Fonts->AddFontFromMemoryCompressedTTF(FA_compressed_data, FA_compressed_size, 9.0f, &icons_config, icons_ranges);

    io.Fonts->Build();

    // Setup ImPlot context
    ImPlot::CreateContext();


    // Setup Platform/Renderer backends
    ImGui_ImplWin32_InitForOpenGL(hwnd);
    ImGui_ImplOpenGL3_Init();

    // Initialize players and deck
    std::vector<Player> players = { {"Anna"}, {"Bob"}, {"Charlie"}, {"Diana"} };
    std::vector<Card> deck = initializeDeck();
    shuffleDeck(deck);
    int coinsInTreasury = 12;

    // Deal initial cards to players
    for (auto& player : players) {
        for (int i = 0; i < 4; ++i) { // Deal 4 cards to each player
            if (!deck.empty()) {
                player.hand.push_back(deck.back());
                deck.pop_back();
            }
        }
    }

    for (auto& player : players) {
        if (!deck.empty()) {
            // Assign the top card of the deck as the swapCard
            player.swapCard = deck.back();
            deck.pop_back(); // Remove the card from the deck
            std::cout << "Assigned swapCard to " << player.name << ": "
                << player.swapCard.value << " of " << player.swapCard.suit << "\n";
        }
        else {
            std::cerr << "Deck is empty! Cannot assign swapCard to " << player.name << "\n";
        }
    }

    // Map to track played cards
    //std::map<std::string, Card> playedCards;
    playedCardsType playedCards;

    bool done = false;
    int handCount = 0; // Track the number of hands played
    bool handCompleted = false;
    Player winner;
    bool withWinner = false;
    bool isLastHand = false;
    bool isLastRound = false;
    bool isGameOver = false;


    std::string currentPlayer = players[0].name; // Initialize with the first player
    int currentPlayerIndex = 0; // Track the index of the current player

    int imgWidth = 2010;
    int imgHeight = 1080;
    GLuint backgroundTexture;
    backgroundTexture = LoadTexture("background3.png", imgWidth, imgHeight);  // Carica l'immagine
    bool automove = false;

    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;
        if (::IsIconic(hwnd))
        {
            ::Sleep(10);
            continue;
        }

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (backgroundTexture != 0) {
            RenderBackgroundWithAspectRatio(backgroundTexture, imgWidth, imgHeight);  // Disegna l'immagine come sfondo
        }

        // Main loop
        {
                // Render player panels
                for (auto& player : players) {
                    std::string suitToFollow;
                    if (!playedCards.empty()) {
                        suitToFollow = playedCards[0].second.suit;
                    }

                    if (renderPlayerPanel(player, playedCards, currentPlayer, handCompleted, automove, players, coinsInTreasury, suitToFollow, handCompleted)) {

                        // Check if the current player has played their card
                        auto it = std::find_if(playedCards.begin(), playedCards.end(),
                            [&](const std::pair<std::string, Card>& entry) {
                                return entry.first == currentPlayer;
                            });

                        if (it != playedCards.end()) {
                            // Move to the next player
                            currentPlayerIndex = (currentPlayerIndex + 1) % players.size();
                            currentPlayer = players[currentPlayerIndex].name;
                        }
                        player.swappedThisTurn = false;
                    }
            }
            
            renderGamePanel(playedCards, automove, handCompleted);
            
            isLastRound = deck.size() <= players.size();
            if (isGameOver)
            {
                ImGui::Begin("Game Panel");
                ImGui::Text("End of the game!");
                if (ImGui::Button("Restart"))
                {
                    shuffleDeck(deck);
                    dealNewCards(players, deck);
                }
                ImGui::End();
            }
            else {
                // Check if all players have played their cards
                if (playedCards.size() == players.size()) {
                    // Determine the winner and reset for the next hand
                    if (!handCompleted) {
                        // Determine if this is the last hand of the turn
                        isLastHand = (++handCount % 4 == 0); // Every 4 hands mark the end of a turn
                        if (!withWinner) {
                            if (determineWinnerAndResetHand(playedCards, players, coinsInTreasury, isLastHand, winner))
                            {
                                auto& winningPlayer = *std::find_if(players.begin(), players.end(),
                                    [&](const Player& p) { return p.name == winner.name; });
                              
                                winningPlayer.wonCards = winner.wonCards;
                                CalculateScore(playedCards, winningPlayer);
                                /*winningPlayer.points = winner.points; // Add card points to player's total points
                                winningPlayer.lastScore = winner.lastScore; // Add card points to player's total points*/

                                // Add coins to the winner (if available)
                                if (coinsInTreasury > 0) {
                                    winningPlayer.coins++;
                                    coinsInTreasury--;
                                }

                                std::cout << winner.name << " wins the hand and now has "
                                    << winningPlayer.points << " points and "
                                    << winningPlayer.coins << " coins.\n";
                                // There is a winner
                                handCompleted = true;
                                withWinner = true;
                            }
                        }
                    }
                    else if (handCompleted && withWinner) {
                        ImGui::Begin("Game Panel");

                        if (isLastHand)
                        {
                            ImGui::Text("Last hand");
                            ImGui::Separator();
                        }

                        ImGui::Text(std::format("{} won the hand!", winner.name).c_str());
                        if (ImGui::Button("Continue"))
                        {
                            // Set the winner as the starting player for the next hand
                            currentPlayerIndex = getPlayerIndexByName(players, winner.name);
                            currentPlayer = winner.name;

                            // If last hand, winner collects all remaining coins
                            if (isLastRound) {
                                auto& winningPlayer = *std::find_if(players.begin(), players.end(),
                                    [&](const Player& p) { return p.name == winner.name; });

                                winningPlayer.coins += coinsInTreasury;
                                std::cout << winner.name << " collects the remaining treasury coins!\n";
                                ImGui::Text(std::format("{} collected {} remaining treasury coins!", winner.name, coinsInTreasury).c_str());
                                coinsInTreasury = 0;
                            }
                            // Clear playedCards for the next hand
                            playedCards.clear();

                            // If last hand, deal new cards to all players
                            if (isLastHand) {
                                if (!isLastRound)
                                {
                                    dealNewCards(players, deck);
                                }
                                else
                                {
                                    isGameOver = true;
                                }
                            }

                            handCompleted = false;
                            withWinner = false;
                            isLastHand = false;
                        }
                        ImGui::Separator();
                        ImGui::End();
                    }
                }

            }
}
    

        // Determine winner of the hand
        

        // Global game information panel
        ImGui::Begin("Game Info");
        ImGui::Text("Treasury Coins: %d", coinsInTreasury);
        ImGui::Text("Deck Size: %lu", deck.size());
        ImGui::End();

        // Rendering
        ImGui::Render();
        glViewport(0, 0, g_Width, g_Height);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Present
        ::SwapBuffers(g_MainWindow.hDC);
    }

    // Shutdown ImPlot
    ImPlot::DestroyContext();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceWGL(hwnd, &g_MainWindow);
    wglDeleteContext(g_hRC);
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

//------------------------------------------------------------------------------------

// Helper functions
bool CreateDeviceWGL(HWND hWnd, WGL_WindowData* data)
{
    HDC hDc = ::GetDC(hWnd);
    PIXELFORMATDESCRIPTOR pfd = { 0 };
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;

    const int pf = ::ChoosePixelFormat(hDc, &pfd);
    if (pf == 0)
        return false;
    if (::SetPixelFormat(hDc, pf, &pfd) == FALSE)
        return false;
    ::ReleaseDC(hWnd, hDc);

    data->hDC = ::GetDC(hWnd);
    if (!g_hRC)
        g_hRC = wglCreateContext(data->hDC);
    return true;
}

void CleanupDeviceWGL(HWND hWnd, WGL_WindowData* data)
{
    wglMakeCurrent(nullptr, nullptr);
    ::ReleaseDC(hWnd, data->hDC);
}

void ResetDeviceWGL()
{

}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            g_Width = LOWORD(lParam);
            g_Height = HIWORD(lParam);
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}



// Function to initialize the deck of cards
std::vector<Card> initializeDeck() {
    std::vector<Card> deck;
    std::vector<std::string> suits = { "Crowns", "Weapons", "Faith", "Diamonds", "Betrays" };
    std::vector<int> values = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };

    for (const auto& suit : suits) {
        for (const auto& value : values) {
            int pointValue = 0;
            if (suit == "Betrays") {
                pointValue = value == 1 ? -2 : value < 6 ? -1 : -2; // Lower values have less penalty
            }
            else {
                pointValue = value == 1 ? 1 : value < 6 ? 0 : value - 5; // Higher cards give points
            }
            deck.push_back({ std::to_string(value), suit, pointValue, value });
        }
    }

    // Adding special cards (Guards and Parliament)
    deck.push_back({ "G", "Guard", 3,1 });
    deck.push_back({ "G", "Guard", 3,2 });
    deck.push_back({ "P", "Parliament", 0,1 });
    deck.push_back({ "P", "Parliament", 0,2 });
    deck.push_back({ "P", "Parliament", 0,3 });
    deck.push_back({ "P", "Parliament", 0,4 });
    deck.push_back({ "P", "Parliament", 0,5 });

    return deck;
}

// Function to shuffle the deck
void shuffleDeck(std::vector<Card>& deck) {
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(deck.begin(), deck.end(), g);
}

// Function to deal cards into players' hands
void dealToHands(std::vector<Player>& players, std::vector<Card>& deck) {
    for (auto& player : players) {
        for (int i = 0; i < 4; ++i) { // Assuming each player gets 4 cards
            player.hand.push_back(deck.back());
            deck.pop_back();
        }
    }
}

// Function for players to choose a card to play
Card chooseCardToPlay(Player& player, const std::string& suitToFollow) {
    // Choose the first card matching the lead suit, if possible
    for (auto it = player.hand.begin(); it != player.hand.end(); ++it) {
        if (it->suit == suitToFollow) {
            Card chosenCard = *it;
            player.hand.erase(it); // Remove the chosen card from hand
            return chosenCard;
        }
    }
    // If no cards match the suit, play the first card
    Card chosenCard = player.hand.front();
    player.hand.erase(player.hand.begin());
    return chosenCard;
}

playedCardsType playHand(std::vector<Player>& players, const std::string& suitToFollow) {
    playedCardsType playedCards;
    for (auto& player : players) {
        Card playedCard = chooseCardToPlay(player, suitToFollow);
        playedCards.push_back({ player.name, playedCard }); // Append to the vector
        std::cout << "    " << player.name << " plays " << playedCard.value << " of " << playedCard.suit << "\n";
    }
    return playedCards;
}


// Function to determine the winner of a hand
std::string determineWinner(const playedCardsType& playedCards, const std::string& suitToFollow) {
    int highestValue = -1;
    std::string winner;
    bool guardPresent = false;
    bool betrayalPresent = false;

    // First pass: Check for presence of Guard and Betrays cards
    for (const auto& [_, card] : playedCards) {
        if (card.suit == "Guard") guardPresent = true;
        if (card.suit == "Betrays") betrayalPresent = true;
    }

    // Second pass: Determine the winner based on rules
    for (const auto& [name, card] : playedCards) {
        if (guardPresent && card.suit == "Guard") {
            // Guard wins if Betrayal is present
            if (betrayalPresent) {
                winner = name;
                break; // Guard wins immediately
            }
        }
        else if (!guardPresent && card.suit == "Betrays" && std::stoi(card.value) > highestValue) {
            // Betrayal wins if no Guard is present
            highestValue = std::stoi(card.value);
            winner = name;
        }
        else if (!betrayalPresent && card.suit == suitToFollow && std::stoi(card.value) > highestValue) {
            // Normal suit comparison if no Betrays are present
            highestValue = std::stoi(card.value);
            winner = name;
        }
    }

    return winner;
}


// Function to handle player swapping cards
void playerRandomSwap(std::vector<Player>& players, int& coinsInTreasury) {
    if (coinsInTreasury > 0) {
        players[0].coins--;
        coinsInTreasury++;
        std::cout << "    " << players[0].name << " pays 1 coin to swap a card.\n";
    }
}
void simulateTurn(int turn, int& coinsInTreasury, std::vector<Player>& players, std::vector<Card>& deck) {
    std::cout << "Turn " << turn << "\n";

    // Deal cards to players' hands
    dealToHands(players, deck);

    // Determine the starting player index (winner of the previous turn starts)
    int startingPlayerIndex = 0;

    for (int hand = 1; hand <= 4; ++hand) {
        std::cout << "  Hand " << hand << "\n";

        // Play a hand
        playedCardsType playedCards;
        std::string suitToFollow;

        // Start with the current starting player
        for (int i = 0; i < players.size(); ++i) {
            int currentIndex = (startingPlayerIndex + i) % players.size();
            Player& currentPlayer = players[currentIndex];

            if (playedCards.empty()) {
                // Determine suit to follow from the first player's hand
                suitToFollow = currentPlayer.hand.front().suit;
            }

            // Each player plays a card
            Card playedCard = chooseCardToPlay(currentPlayer, suitToFollow);
            playedCards.push_back({ currentPlayer.name, playedCard });
            std::cout << "    " << currentPlayer.name << " plays " << playedCard.value << " of " << playedCard.suit << "\n";
        }

        // Determine winner of the hand
        std::string winnerName = determineWinner(playedCards, suitToFollow);

        // Award points and coins to the winner
        if (!winnerName.empty()) {
            auto& winningPlayer = *std::find_if(players.begin(), players.end(),
                [&](const Player& p) { return p.name == winnerName; });

            // Assign points based on the cards won in this hand
            for (const auto& [_, card] : playedCards) {
                winningPlayer.points += card.pointValue;
            }

            // Assign coins if available
            if (coinsInTreasury > 0) {
                winningPlayer.coins++;
                coinsInTreasury--;
            }

            std::cout << "    Winner: " << winnerName << " earns " << winningPlayer.points << " points.\n";

            // The winner of the hand starts the next hand
            startingPlayerIndex = getPlayerIndexByName(players, winnerName);
        }

        // Handle random card swapping at the end of the hand
        playerRandomSwap(players, coinsInTreasury);
    }

    // End of turn - Assign a mission to the player with the most points
    std::sort(players.begin(), players.end(),
        [](const Player& a, const Player& b) { return a.points > b.points; });

    players[0].missions++; // Player with the most points gets a mission
    std::cout << "  " << players[0].name << " receives an additional mission!\n\n";
}

void dealNewCards(std::vector<Player>& players, std::vector<Card>& deck)
{
    std::cout << "Dealing new cards for the next round...\n";
     for (auto& player : players) {
        player.hand.clear();
        for (int i = 0; i < 4; ++i) // Cards in hand = 4
        {
            if (!deck.empty()) {
                player.hand.push_back(deck.back());
                deck.pop_back();
            }
        }
    }
}


//--------------------------------------------------------------------------------

Card chooseBestCard(const std::vector<Card>& hand, const std::string& suitToFollow, const playedCardsType& playedCards, Player& player) {

    // Verifica se sono state giocate delle Betrayal
    if (chance(80))
    {
        bool betrayalPlayed = std::any_of(playedCards.begin(), playedCards.end(),
            [](const std::pair<std::string, Card>& entry) {
                return entry.second.suit == "Betrays";
            });

        // Logica 1: Gioca una guardia se ci sono Betrayal in gioco
        if (betrayalPlayed) {
            for (const auto& card : hand) {
                if (card.suit == "Guard") {
                    return card; // Gioca la guardia per contrastare le Betrayal
                }
            }
        }
    }

    // Logica 3: Gioca una Betrayal se è più bassa delle altre Betrayal esistenti
    if (chance(80)) {
        int lowestBetrayal = INT_MAX;
        for (const auto& entry : playedCards) {
            if (entry.second.suit == "Betrays") {
                lowestBetrayal = std::min(lowestBetrayal, std::stoi(entry.second.value));
            }
        }
        for (const auto& card : hand) {
            if (card.suit == "Betrays" && std::stoi(card.value) < lowestBetrayal && lowestBetrayal != INT_MAX) {
                return card; // Gioca una Betrayal se è più bassa
            }
        }
    }

    // Logica 4: Segui il seme, se possibile
    if (chance(80))
    {
        if (suitToFollow != "Parliament" && suitToFollow != "Guard" && suitToFollow != "Betrays") {
            std::string bestSuit = "0";
            for (const auto& entry : playedCards)
            {
                if (entry.second.suit == suitToFollow && entry.second.value > bestSuit)
                {
                    bestSuit = entry.second.value;
                }
            }
            // Logica 4: Segui il seme, se possibile
            for (const auto& card : hand) {
                if (card.suit == suitToFollow) {
                    if (card.value > bestSuit) {
                        return card;
                    }
                }
            }
        }
    }

    // Logica 2: Gioca un Parliament se non si è l'ultimo di mano
    if (chance(80)) {

        size_t cardsPlayed = playedCards.size();
        if (cardsPlayed < 3) { // Supponendo che ci siano 4 giocatori
            for (const auto& card : hand) {
                if (card.suit == "Parliament") {
                    return card; // Gioca un Parliament se non sei l'ultimo
                }
            }
        }
    }

    // Logica 5: Gioca una carta non negativa
    if (chance(80)) {
        int minValue = 999;
        Card selected;
        for (const auto& card : hand) {
            if (card.pointValue >= 0 && card.pointValue< minValue) {
                minValue = card.pointValue;
                selected = card;
            }
        }
        if (!selected.suit.empty())
        {
            return selected;
        }
    }

    /*if (chance(20))
    {
        if (!player.swappedThisTurn && player.coins > 0) {
                       
                // Swap the selected hand card with the swap card
                std::swap(player.hand[i], player.swapCard);
                player.coins--; // Deduct a coin for the swap
                coinsInTreasury++;

                player.swappedThisTurn = i + 1;
            
        }
    }*/

    // Logica 6: Gioca la carta meno penalizzante
        const Card* leastNegativeCard = &hand.front();
        for (const auto& card : hand) {
            if (card.pointValue > leastNegativeCard->pointValue) {
                leastNegativeCard = &card;
            }
        }
    

    return *leastNegativeCard;
}


//--------------------------------------------------------------------------------

bool renderPlayerPanel(Player& player, playedCardsType& playedCards, const std::string& currentPlayer, bool justShow,bool& automove,  std::vector<Player>& allPlayers, int& coinsInTreasury, const std::string& suitToFollow, bool handCompleted) {

    ImGui::Begin(("Player " + player.name).c_str());
    ImGui::Text("Coins: %d", player.coins);
    ImGui::Text("Points: %d(%+d)", player.points, player.points - player.lastScore);
    ImGui::SameLine();  ImGui::Text(std::format("    {}:{} {}:{} {}:{}", ICON_FA_BUILDING_COLUMNS, player.parliaments, ICON_FA_CIRCLE_PLUS, player.goodPoints, ICON_FA_CIRCLE_MINUS, player.negativePoints).c_str());
    ImGui::Text("Missions: %d", player.missions);

    bool played = false;
    bool isCurrentPlayer = (player.name == currentPlayer);

    if (isCurrentPlayer) {
        ImGui::Separator();
        ImGui::Text("*** Playing ***");
        ImGui::Separator();
    }

    auto invisible = IM_COL32(255, 255, 255, 16);

    // Display player's hand
    for (size_t i = 0; i < player.hand.size(); ++i) {

        std::string buttonText = std::format("Play {} {}\nof {}\n({}{})",
            player.hand[i].value,
            iconify(player.hand[i].suit),
            player.hand[i].suit,
            player.hand[i].pointValue >= 0 ? "+" : "",
            player.hand[i].pointValue);

        if (isCurrentPlayer && !justShow) {
            ImGui::PushStyleColor(ImGuiCol_Button, getSuitColor(player.hand[i].suit));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, getSuitColor(player.hand[i].suit));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, getSuitColor(player.hand[i].suit));
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32_WHITE);
        }
        else {
            ImVec4 transparent = getSuitColor(player.hand[i].suit); transparent.w = 0.4f;
            ImGui::PushStyleColor(ImGuiCol_Button, invisible);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, invisible);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, invisible);
            ImGui::PushStyleColor(ImGuiCol_Text, invisible);
            buttonText = "Hidden Card";
        }

      

        if (ImGui::Button((buttonText + "##" + std::to_string(i)).c_str(), ImVec2(100, 80))) {
            if (isCurrentPlayer && !justShow) {
                playedCards.push_back({ player.name, player.hand[i] });
                player.hand.erase(player.hand.begin() + i);
                std::cout << "Player " << player.name << " played " << playedCards.back().second.value
                    << " of " << playedCards.back().second.suit << "\n";
                played = true;
            }
        }
        
        ImGui::PopStyleColor(4);
        if (i < player.hand.size() - 1) {
            ImGui::SameLine();
        }

    }
   
    for (size_t i = 0; i < player.hand.size(); ++i) {
        // Swap with swapCard
        if (isCurrentPlayer && !justShow && !player.swappedThisTurn && player.coins > 0) {
         
            if (ImGui::Button(("Swap##swap_" + std::to_string(i)).c_str(), ImVec2(100, 20))) {
                // Swap the selected hand card with the swap card
                std::swap(player.hand[i], player.swapCard);
                player.coins--; // Deduct a coin for the swap
                coinsInTreasury++;

                player.swappedThisTurn = i+1;
                
                std::cout << "Player " << player.name << " swapped hand card " << player.hand[i].value
                    << " with their swap card " << player.swapCard.value << "\n";

                // Ensure the colors update correctly (force re-rendering with updated data)
                ImGui::PushStyleColor(ImGuiCol_Button, getSuitColor(player.hand[i].suit));
                ImGui::PopStyleColor();
            }
            if (i < player.hand.size() - 1) {
                ImGui::SameLine();
            }
        }
    }
    ImGui::Separator();

    ImGui::Text("Swap Card:");
    if (isCurrentPlayer && player.swappedThisTurn) {
        ImGui::PushStyleColor(ImGuiCol_Button, getSuitColor(player.swapCard.suit));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, getSuitColor(player.swapCard.suit));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, getSuitColor(player.swapCard.suit));
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32_WHITE);
    }
    else {
        ImVec4 transparent = getSuitColor(player.swapCard.suit); transparent.w = 0.4f;
        ImGui::PushStyleColor(ImGuiCol_Button, invisible);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, invisible);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, invisible);
        ImGui::PushStyleColor(ImGuiCol_Text, invisible);
    }
    // Show player's swap card
    std::string hiddenText = "Hidden Card";
    if (isCurrentPlayer && player.swappedThisTurn)
    {
        hiddenText = std::format("Play {} {}\nof {}\n({}{})",
            player.swapCard.value,
            iconify(player.swapCard.suit),
            player.swapCard.suit,
            player.swapCard.pointValue >= 0 ? "+" : "",
            player.swapCard.pointValue);
    }

    if (ImGui::Button((hiddenText +"##swapCard_" + player.name).c_str(), ImVec2(100, 80)))
    {
        if (isCurrentPlayer && player.swappedThisTurn)
        {
            
            playedCards.push_back({ player.name, player.swapCard });
            std::swap(player.hand[player.swappedThisTurn - 1], player.swapCard);
            player.hand.erase(player.hand.begin() + (player.swappedThisTurn-1));
            std::cout << "Player " << player.name << " played " << playedCards.back().second.value
                << " of " << playedCards.back().second.suit << "\n";
            played = true;
        }
    }
    ImGui::PopStyleColor(4);

    // Swap with another player's swapCard
    if (isCurrentPlayer && !justShow && !player.swappedThisTurn && player.coins > 0) {
        ImGui::SameLine();
        if (ImGui::Button("Swap with\nanother\nplayer's\nSwap Card", ImVec2(100, 80))) {
            ImGui::OpenPopup("Swap Card");
        }

        // Popup for swapping swap cards
        if (ImGui::BeginPopup("Swap Card")) {
            for (auto& otherPlayer : allPlayers) {
                if (otherPlayer.name == player.name) continue; // Skip self
                if (ImGui::Button(("Swap with " + otherPlayer.name + "##swap_" + player.name + "_" + otherPlayer.name).c_str())) {
                    std::swap(player.swapCard, otherPlayer.swapCard);
                    player.coins--; // Deduct one coin for the swap
                    coinsInTreasury++;
                    player.swappedThisTurn = true;
                    std::cout << "Player " << player.name << " swapped their swap card with " << otherPlayer.name << "\n";
                    ImGui::CloseCurrentPopup();
                    break;
                }
            }
            ImGui::EndPopup();
        }
    }


    if (isCurrentPlayer && !handCompleted) {
        ImGui::SameLine();
        if ( automove || ImGui::Button("Auto Move##auto_move", ImVec2(200, 40))) {
            if (!player.hand.empty()) {
                // Scegli la migliore carta da giocare
                Card bestCard = chooseBestCard(player.hand, suitToFollow, playedCards, player);
                automove = false;
                // Aggiungi la carta scelta a playedCards
                playedCards.push_back({ player.name, bestCard });

                // Rimuovi la carta dalla mano del giocatore
                auto it = std::find_if(player.hand.begin(), player.hand.end(),
                    [&bestCard](const Card& card) {
                        return card.value == bestCard.value && card.suit == bestCard.suit;
                    });
                if (it != player.hand.end()) {
                    player.hand.erase(it);
                }

                std::cout << "Player " << player.name << " played (Auto) " << bestCard.value
                    << " of " << bestCard.suit << "\n";

                played = true;
            }
        }

    }

    ImGui::End();

    // Reset swappedThisTurn at the end of the player's turn
    if (!isCurrentPlayer) {
        player.swappedThisTurn = false;
    }

    return played;
}






//--------------------------------------------------------------------------------


bool renderPlayerPanel2(Player& player, playedCardsType& playedCards, const std::string& currentPlayer, bool justShow) {
    // Check if it's the current player's turn
    bool isCurrentPlayer = (player.name == currentPlayer);

    // Minimize the window for non-current players
  /*  if (!isCurrentPlayer) {
        ImGui::SetNextWindowCollapsed(true, ImGuiCond_Always);
    }
    else {
        ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);
    }*/

    ImGui::Begin(("Player " + player.name).c_str());

    ImGui::Text("Coins: %d", player.coins);
    ImGui::Text("Points: %d(%+d)", player.points, player.points - player.lastScore);
    ImGui::SameLine();  ImGui::Text(std::format("    {}:{} {}:{} {}:{}", ICON_FA_BUILDING_COLUMNS, player.parliaments, ICON_FA_CIRCLE_PLUS, player.goodPoints, ICON_FA_CIRCLE_MINUS, player.negativePoints).c_str());
    ImGui::Text("Missions: %d", player.missions);
    bool played = false;

    if (isCurrentPlayer) {
        ImGui::Separator();
        ImGui::Text("*** Playing ***");
        ImGui::Separator();
    }

    // Display player's cards as buttons for actions
    for (size_t i = 0; i < player.hand.size(); ++i)
    {
        std::string buttonText = std::format("Play {} {}\nof {}\n({}{})",
            player.hand[i].value,
            iconify(player.hand[i].suit),
            player.hand[i].suit,
            player.hand[i].pointValue >= 0 ? "+" : "",
            player.hand[i].pointValue);

        if (isCurrentPlayer && !justShow) {
            // Active player's cards are interactable
            ImGui::PushStyleColor(ImGuiCol_Button, getSuitColor(player.hand[i].suit));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, getSuitColor(player.hand[i].suit));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, getSuitColor(player.hand[i].suit));
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32_WHITE);
        }
        else {
            // Dim cards for non-current players
            ImVec4 transparent = getSuitColor(player.hand[i].suit);
            transparent.w = 0.4f;
            auto invisible = IM_COL32(255, 255, 255, 16);
            ImGui::PushStyleColor(ImGuiCol_Button, invisible);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, invisible);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, invisible);
            ImGui::PushStyleColor(ImGuiCol_Text, invisible);
            buttonText = "";
        }

       
        if (ImGui::Button((buttonText + "##" + std::to_string(i)).c_str(), ImVec2(100, 80))) {
            if (isCurrentPlayer && !justShow) {
                // Add the selected card to playedCards
                playedCards.push_back({ player.name, player.hand[i] });

                // Remove the card from the player's hand
                player.hand.erase(player.hand.begin() + i);

                std::cout << "Player " << player.name << " played " << playedCards.back().second.value
                    << " of " << playedCards.back().second.suit << "\n";

                played = true;
            }
        }

        ImGui::PopStyleColor(4);
        ImGui::SameLine();
    }

    ImGui::End();
    return played;
}



// Function to render the main game panel
void renderGamePanel2(const playedCardsType& playedCards) {
    ImGui::Begin("Game Panel");

    ImGui::Text("Cards Played This Turn:");
    ImGui::Separator();

    for (const auto& [playerName, card] : playedCards) {
        ImGui::PushStyleColor(ImGuiCol_Text, getSuitColor(card.suit)); // Color based on the suit
        ImGui::Text("%s played %s of %s (%+d points)",
            playerName.c_str(),
            card.value.c_str(),
            card.suit.c_str(),
            card.pointValue); // Card value, suit, and point value
        ImGui::PopStyleColor();
    }

    ImGui::Separator();
    ImGui::End();
}

bool determineWinnerAndResetHand(
    playedCardsType& playedCards,
    std::vector<Player>& players,
    int& coinsInTreasury,
    bool isLastHand,
    Player& winner) {

    // Determine the suit to follow (first card's suit)

    std:string suitToFollow;
    for (auto card : playedCards)
    {
        if (card.second.suit != "Guard" && card.second.suit != "Betrays" && card.second.suit!="Parliament")
        {
            suitToFollow = card.second.suit;
            break;
        }
    }

    // Variables to track the winner
    std::string winnerName;
    int highestOrder = -1; // Use `order` for comparison
    int highestBetrayalOrder = -1;
    bool allParliament = true; // Check if all cards are Parliament
    bool guardPresent = false;
    bool betrayalPresent = false;

    // Check for special cards and whether all cards are Parliament
    for (const auto& [_, card] : playedCards) {
        if (card.suit != "Parliament") {
            allParliament = false; // If any card is not Parliament, set this to false
        }
        if (card.suit == "Guard") guardPresent = true;
        if (card.suit == "Betrays") betrayalPresent = true;
    }

    if (allParliament) {
        // If all cards are Parliament, determine the winner based on the highest order
        for (const auto& [playerName, card] : playedCards) {
            if (card.suit == "Parliament" && card.order > highestOrder) {
                highestOrder = card.order;
                winnerName = playerName;
            }
        }
    }
    else {
        // Evaluate cards to determine the winner
        for (const auto& [playerName, card] : playedCards) {
            if (guardPresent && card.suit == "Guard") {
                // Guard wins if Betrayal is present
                if (betrayalPresent) {
                    winnerName = playerName;
                    break; // Guard wins immediately
                }
            }
            else if (!guardPresent && card.suit == "Betrays" && card.order > highestBetrayalOrder) {
                // Betrayal wins over other suits if no Guard is present
                winnerName = playerName;
                highestBetrayalOrder = card.order;
            }
            else if (!betrayalPresent && card.suit == suitToFollow && card.order > highestOrder) {
                // Normal suit comparison
                highestOrder = card.order;
                winnerName = playerName;
            }
        }
    }

    // Assign all played cards to the winner's wonCards
    if (!winnerName.empty()) {
        std::cout << winnerName << " won the hand!\n";

        winner = *std::find_if(players.begin(), players.end(),
            [&](const Player& p) { return p.name == winnerName; });

        for (const auto& [_, card] : playedCards) {
            winner.wonCards.push_back(card);
        }
        return true;
    }

    return false; // No winner
}

void CalculateScore(playedCardsType& playedCards, Player& player)
{
    player.lastScore = player.points;
    player.points = 0;
    player.goodPoints = 0;
    player.negativePoints = 0;
    player.parliaments = 0;
    int parliamentCount = 0;
   
    for (auto card : player.wonCards)
    {
        if (card.suit != "Parliament")
        {
            player.points += card.pointValue;
            if (card.pointValue>0) player.goodPoints+= card.pointValue;
            else if (card.pointValue < 0) player.negativePoints+= card.pointValue;
        }
        else
        {
            parliamentCount++;
            
        }
    }
    if ((parliamentCount % 2) == 1)
    {
        player.points -= -1;
        player.parliaments=-1;
    }
    else if (parliamentCount > 1)
    {
        player.points += 3;
        player.parliaments = 3;
    }

}

bool determineWinnerAndResetHand2(
    playedCardsType& playedCards,
    std::vector<Player>& players,
    int& coinsInTreasury,
    bool isLastHand, Player& winner) {

    // Determine the suit to follow (first card's suit)
    std::string suitToFollow = playedCards.begin()->second.suit;

    // Determine the winner of the hand
    std::string winnerName;
    int highestValue = -1;

    for (const auto& [playerName, card] : playedCards) {
        if (card.suit == suitToFollow && std::stoi(card.value) > highestValue) {
            highestValue = std::stoi(card.value);
            winnerName = playerName;
        }

        // Guard wins against Betrays
        if (card.suit == "Guard") {
            for (const auto& [_, otherCard] : playedCards) {
                if (otherCard.suit == "Betrays") {
                    winnerName = playerName;
                    break;
                }
            }
        }
    }

    // Assign all played cards to the winner's wonCards
    if (!winnerName.empty()) {
        std::cout << winnerName << " won the hand!\n";

        winner = *std::find_if(players.begin(), players.end(),
            [&](const Player& p) { return p.name == winnerName; });

        for (const auto& [_, card] : playedCards) {
            winner.wonCards.push_back(card);
        }

        return true;
    }
    return false;
}

// Helper function to get the color based on suit
ImVec4 getSuitColor(const std::string& suit) {
    if (suit == "Weapons") return ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
    if (suit == "Crowns") return ImVec4(0.7f, 0.7f, 0.0f, 1.0f); // Yellow
    if (suit == "Faith") return ImVec4(0.0f, 0.7f, 0.0f, 1.0f); // Green
    if (suit == "Diamonds") return ImVec4(0.0f, 0.0f, 1.0f, 1.0f); // Blue
    if (suit == "Betrays") return ImVec4(0.35f, 0.0f, 0.35f, 1.0f); // Dark Purple
    if (suit == "Parliament") return ImVec4(0.55f, 0.4f, 0.1f, 1.0f); // Brown
    if (suit == "Guard") return ImVec4(0.6f, 0.6f, 0.6f, 1.0f); // White
    return ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // Default White
}

