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
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
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

};

using playedCardsType = std::vector<std::pair<std::string, Card>>;

//------------------------------------------------------------------------------------

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
bool renderPlayerPanel(Player& player, playedCardsType& playedCards, const std::string& currentPlayer);

// Function to render the main game panel
void renderGamePanel2(const playedCardsType& playedCards);



// Helper function to get the color corresponding to the suit (For UI)
ImVec4 getSuitColor(const std::string& suit);

// Function to determine the winner of the hand and update the winner hand, managing also special cases (Parliament, guard)
bool determineWinnerAndResetHand(playedCardsType& playedCards, std::vector<Player>& players, int& coinsInTreasury, bool isLastHand, Player& winner);

// Function to determine the winner and update the winner hand - Previous version
bool determineWinnerAndResetHand2(playedCardsType& playedCards, std::vector<Player>& players, int& coinsInTreasury, bool isLastHand, Player& winner);

void dealNewCards(std::vector<Player>& players);

//------------------------------------------------------------------------------------
void renderPlayerPanel2(Player& player);

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
void renderGamePanel(const playedCardsType& playedCards) {
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

    // Map to track played cards
    //std::map<std::string, Card> playedCards;
    playedCardsType playedCards;

    bool done = false;
    int handCount = 0; // Track the number of hands played
    bool handCompleted = false;
    Player winner;
    bool withWinner = false;
    bool isLastHand = false;

    std::string currentPlayer = players[0].name; // Initialize with the first player
    int currentPlayerIndex = 0; // Track the index of the current player

    int imgWidth = 2010;
    int imgHeight = 1080;
    GLuint backgroundTexture;
    backgroundTexture = LoadTexture("background3.png", imgWidth, imgHeight);  // Carica l'immagine

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
                if (renderPlayerPanel(player, playedCards, currentPlayer)) {
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
                }
            }


            renderGamePanel(playedCards);

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

                            // Add the played cards to the winner's wonCards
                            for (const auto& [_, card] : playedCards) {
                                winningPlayer.wonCards.push_back(card);
                                winningPlayer.points += card.pointValue; // Add card points to player's total points
                            }
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
                        /*if (isLastHand) {
                            winner.coins += coinsInTreasury;
                            coinsInTreasury = 0;
                            std::cout << winner.name << " collects the remaining treasury coins!\n";
                        }*/
                        // If last hand, winner collects all remaining coins
                        if (isLastHand) {
                            auto& winningPlayer = *std::find_if(players.begin(), players.end(),
                                [&](const Player& p) { return p.name == winner.name; });

                            winningPlayer.coins += coinsInTreasury;
                            coinsInTreasury = 0;

                            std::cout << winner.name << " collects the remaining treasury coins!\n";
                        }
                        // Clear playedCards for the next hand
                        playedCards.clear();

                        // If last hand, deal new cards to all players
                        if (isLastHand) {
                            dealNewCards(players);
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

void dealNewCards(std::vector<Player>& players)
{
    std::cout << "Dealing new cards for the next round...\n";
    std::vector<Card> deck = initializeDeck();
    shuffleDeck(deck);
    for (auto& player : players) {
        player.hand.clear();
        for (int i = 0; i < 4; ++i) {
            if (!deck.empty()) {
                player.hand.push_back(deck.back());
                deck.pop_back();
            }
        }
    }
}
/*
// Function to render a single player's panel
void renderPlayerPanel(Player& player) {
    ImGui::Begin(("Player " + player.name).c_str());
    ImGui::Text("Coins: %d", player.coins);
    ImGui::Text("Points: %d", player.points);
    ImGui::Text("Missions: %d", player.missions);

    // Display player's cards as buttons for actions
    for (size_t i = 0; i < player.hand.size(); ++i) {
        ImGui::PushStyleColor(ImGuiCol_Button, getSuitColor(player.hand[i].suit));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, getSuitColor(player.hand[i].suit));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, getSuitColor(player.hand[i].suit));

        if (ImGui::Button(("Play " + player.hand[i].value + " of " + player.hand[i].suit + "##" + std::to_string(i)).c_str(), ImVec2(200, 40))) {
            // Handle logic for playing the card
            std::cout << "Player " << player.name << " played " << player.hand[i].value << " of " << player.hand[i].suit << "\n";
        }

        ImGui::PopStyleColor(3);
    }

    ImGui::End();
}*/
/*
void renderPlayerPanel(Player& player, playedCardsType& playedCards) {
    ImGui::Begin(("Player " + player.name).c_str());
    ImGui::Text("Coins: %d", player.coins);
    ImGui::Text("Points: %d", player.points);
    ImGui::Text("Missions: %d", player.missions);

    // Display player's cards as buttons for actions
    for (size_t i = 0; i < player.hand.size(); ++i) {

        ImGui::PushStyleColor(ImGuiCol_Button, getSuitColor(player.hand[i].suit));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, getSuitColor(player.hand[i].suit));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, getSuitColor(player.hand[i].suit));

        std::string buttonText = std::format("Play\n{} of {}\n({}{})",
            player.hand[i].value,
            player.hand[i].suit,
            player.hand[i].pointValue >= 0 ? "+" : "",
            player.hand[i].pointValue);

        if (ImGui::Button((buttonText + "##" + std::to_string(i)).c_str(), ImVec2(100, 80))) {
            // Add the selected card to playedCards
            playedCards[player.name] = player.hand[i];

            // Remove the card from the player's hand
            player.hand.erase(player.hand.begin() + i);

            std::cout << "Player " << player.name << " played " << playedCards[player.name].value << " of " << playedCards[player.name].suit << "\n";

            // Break to avoid accessing an invalid index after removing the card
            //ImGui::PopStyleColor(3);
           // ImGui::PopItemWidth();
           // break;
        }

        ImGui::PopStyleColor(3);
        ImGui::SameLine();
    }

    ImGui::End();
}*/

bool renderPlayerPanelOld(Player& player, playedCardsType& playedCards, const std::string& currentPlayer) {
    ImGui::Begin(("Player " + player.name).c_str());
    ImGui::Text("Coins: %d", player.coins);
    ImGui::Text("Points: %d", player.points);
    ImGui::Text("Missions: %d", player.missions);
    bool played = false;
    bool isActivePlayer = (player.name == currentPlayer); // Check if it's this player's turn

    for (size_t i = 0; i < player.hand.size(); ++i) {
        if (isActivePlayer) {
            // Active player's cards are interactable
            ImGui::PushStyleColor(ImGuiCol_Button, getSuitColor(player.hand[i].suit));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, getSuitColor(player.hand[i].suit));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, getSuitColor(player.hand[i].suit));
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32_WHITE);
        }
        else {
            ImVec4 transparent = getSuitColor(player.hand[i].suit);
            transparent.w = 0.5f;
            // Inactive player's cards are semi-transparent
            ImGui::PushStyleColor(ImGuiCol_Button, transparent); // Dimmed color
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, transparent);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, transparent);
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 128));
        }

        std::string buttonText = std::format("Play\n{} of {}\n({}{})",
            player.hand[i].value,
            player.hand[i].suit,
            player.hand[i].pointValue >= 0 ? "+" : "",
            player.hand[i].pointValue);

        // Only allow the active player to click their cards
        if (ImGui::Button((buttonText + "##" + std::to_string(i)).c_str(), ImVec2(100, 80))) {
            // Add the selected card to playedCards
            playedCards.push_back({ player.name, player.hand[i] });

            // Remove the card from the player's hand
            player.hand.erase(player.hand.begin() + i);

            std::cout << "Player " << player.name << " played " << playedCards.back().second.value
                << " of " << playedCards.back().second.suit << "\n";
            ImGui::PopStyleColor(4);
            break;
            break; // Ensure only one card is played per turn
        }



        /*if (ImGui::Button((buttonText + "##" + std::to_string(i)).c_str(), ImVec2(100, 80))) {
            // Add the selected card to playedCards
            playedCards[player.name] = player.hand[i];

            // Remove the card from the player's hand
            player.hand.erase(player.hand.begin() + i);

            std::cout << "Player " << player.name << " played " << playedCards[player.name].value << " of " << playedCards[player.name].suit << "\n";
            played = true;
            // Break to avoid accessing an invalid index after removing the card
            ImGui::PopStyleColor(4);
            break;
        }*/

        ImGui::PopStyleColor(4);
        ImGui::SameLine();
    }

    ImGui::End();
    return played;
}

//new
bool renderPlayerPanel(Player& player, playedCardsType& playedCards, const std::string& currentPlayer) {
    ImGui::Begin(("Player " + player.name).c_str());
    ImGui::Text("Coins: %d", player.coins);
    ImGui::Text("Points: %d", player.points);
    ImGui::Text("Missions: %d", player.missions);
    bool played = false;
    // Check if it's the current player's turn
    bool isCurrentPlayer = (player.name == currentPlayer);
    if (isCurrentPlayer)
    {
        ImGui::Separator();
        ImGui::Text("*** Playing ***");
        ImGui::Separator();
    }
    // Display player's cards as buttons for actions
    for (size_t i = 0; i < player.hand.size(); ++i) {
        // Dim the button if it's not the player's turn
        if (isCurrentPlayer) {
         
            // Active player's cards are interactable
            ImGui::PushStyleColor(ImGuiCol_Button, getSuitColor(player.hand[i].suit));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, getSuitColor(player.hand[i].suit));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, getSuitColor(player.hand[i].suit));
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32_WHITE);
        }
        else {
            ImVec4 transparent = getSuitColor(player.hand[i].suit);
            transparent.w = 0.5f;
            // Inactive player's cards are semi-transparent
            ImGui::PushStyleColor(ImGuiCol_Button, transparent); // Dimmed color
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, transparent);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, transparent);
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 128));
        }

        std::string buttonText = std::format("Play\n{} of {}\n({}{})",
            player.hand[i].value,
            player.hand[i].suit,
            player.hand[i].pointValue >= 0 ? "+" : "",
            player.hand[i].pointValue);


        if (ImGui::Button((buttonText + "##" + std::to_string(i)).c_str(), ImVec2(100, 80))) {
            if (isCurrentPlayer) {
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
        if (card.second.suit != "Guard" && card.second.suit != "Betrays")
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

            winner.points += card.pointValue;
        }

        return true;
    }

    return false; // No winner
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

void dealNewCards(std::vector<Player>& players);
