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
#include <iomanip>
#include "main.h"

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
void ShowAutoPlayControls(bool& autoplay, bool& automove, bool& autoMatch);
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
    bool control = false;
    int coinsCollected = 0;
};

struct CardValues {
    int baseValues[5] = { +1, +2, +2, +3, +3 };       // Valori dei semi base (1,6,7,8,9)
    int betrayalValues[9] = { -2, -1, -1, -1, -1, -1, -1, -2, -2 }; // Betrayal (1-9, 0)
    int parliamentPositive = 3;               // Valore positivo per Parliament
    int parliamentNegative = -1;              // Valore negativo per Parliament
    int guardValue = 3;                       // Valore della Guardia
    int coinValue = 1;                        // Valore della Moneta
};

using playedCardsType = std::vector<std::pair<std::string, Card>>;

//--------------------------------------------------------------------------------------

std::mt19937 randomGenerator; // Generatore di numeri casuali
unsigned int randomSeed = 0; // Seed iniziale

//------------------------------------------------------------------------------------
//static thread_local std::mt19937 rng(std::random_device{}());
static thread_local std::uniform_int_distribution<int> dist(1, 100);

inline bool chance(uint_fast8_t percent) noexcept {
    return dist(randomGenerator) <= percent;
}
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
std::vector<Card> initializeDeck();
void shuffleDeck(std::vector<Card>& deck);

struct Game {
    int coinsInTreasury = 12;
    int maxCoinsInTreasury = 12;
    std::vector<Player> players;
    std::vector<Card> deck;
    playedCardsType playedCards;
    Player winner;

    int round = 1;
    int turn = 1;
    unsigned int randomSeed = 0;
    bool isGameOver = false;
    bool isStarted = false;
    bool collectAtTheEnd = false;
    bool collectEndTurn = false;
    bool collectGap = false;
    int initialCoins = 1;

    // Funzione per resettare lo stato del gioco
    void reset() {
        isGameOver = false;
        isStarted = false;
        players.clear();
        deck = initializeDeck();
        playedCards.clear();
        winner = Player();
        randomGenerator.seed(randomSeed);
        shuffleDeck(deck);
        round = 1;
        turn = 1;
    }

    void restore()
    {
        coinsInTreasury = 12;
        maxCoinsInTreasury = 12;
        collectAtTheEnd = false;
        collectEndTurn = false;
        collectGap = false;
        initialCoins = 1;
    }
} game;

void setRandomSeed(unsigned int seed);
std::string iconify(const std::string& name);

// Functions for initializing, shuffling, and dealing the deck
void assignCardValues(std::vector<Card>& deck, const CardValues& cardValues);


void dealToHands(std::vector<Player>& players, std::vector<Card>& deck);

// Functions for playing and determining the winner of a hand
Card chooseCardToPlay(Player& player, const std::string& suitToFollow);
playedCardsType playHand(std::vector<Player>& players, const std::string& suitToFollow);
std::string determineWinner(const playedCardsType& playedCards, const std::string& suitToFollow);

// Function to handle player swapping cards
void playerRandomSwap(std::vector<Player>& players, int& coinsInTreasury);

// Helper function to get the color corresponding to the suit (For UI)
ImVec4 getSuitColor(const std::string& suit);
std::string getSuitColorString(const std::string& suit);

// Function to determine the winner of the hand and update the winner hand, managing also special cases (Parliament, guard)
bool determineWinnerAndResetHand(playedCardsType& playedCards, std::vector<Player>& players, int& coinsInTreasury, bool isLastHand, Player& winner);

void CalculateScore(playedCardsType& playedCards, Player& player, const CardValues& cardValues);
int CalculatePScore(playedCardsType& playedCards, const CardValues& cardValues);
Player determineFinalWinner(const std::vector<Player>& players);

void dealNewCards(std::vector<Player>& players, std::vector<Card>& deck);
void updatePlayerHandValues(std::vector<Player>& players, const CardValues& cardValues);

int checkMemoryDeck(const Player& player, const std::string& suitToFollow);
int checkPlayerHand(const Player& player, const std::string& suitToFollow);
int checkGameRoundPlayedDeck(const std::string& suitToFollow);
int checkGameRemainigDeck(const std::string& suitToFollow);

// UI Rendering Functions
bool renderPlayerPanel(Player& player, playedCardsType& playedCards, const std::string& currentPlayer, bool justShow, bool& automove, std::vector<Player>& allPlayers, int& coinsInTreasury, const std::string& suitToFollow, bool handCompleted, const CardValues& cardValues);
void renderValueConfigPanel(CardValues& cardValues, std::vector<Card>& deck, std::vector<Player>& players);
void renderCardValueManagementPanel(CardValues& cardValues, const std::string& directoryPath, std::string& selectedFile);
void renderFinalWinnerPanel(const Player& winner, const std::vector<Player>& players);
void renderGamePanel(bool& gameStarted);


int getPlayerIndexByName(const std::vector<Player>& players, const std::string& name);

void renderGameboardPanel(const playedCardsType& playedCards, bool& automove, bool& autoplay, bool& automatch, bool handCompleted);
bool restartGame(std::vector<Player>& players, std::vector<Card>& deck, bool& gameStarted);

void initializeGame(Game& game) {

    //game.randomSeed = seed;
    randomGenerator.seed(game.randomSeed);
    game.reset();

    // Inizializza i giocatori
    game.players = { {"Anna"}, {"Bob"}, {"Charlie"}, {"Diana"} };

    shuffleDeck(game.deck);

    // Distribuisci le carte ai giocatori
    for (auto& player : game.players) {
        for (int i = 0; i < 4; ++i) {
            if (!game.deck.empty()) {
                player.hand.push_back(game.deck.back());
                game.deck.pop_back();
            }
        }
    }

    for (auto& player : game.players) {
        if (!game.deck.empty()) {
            // Assign the top card of the deck as the swapCard
            player.swapCard = game.deck.back();
            game.deck.pop_back(); // Remove the card from the deck
            std::cout << "Assigned swapCard to " << player.name << ": "
                << player.swapCard.value << " of " << player.swapCard.suit << "\n";
        }
        else {
            std::cerr << "Deck is empty! Cannot assign swapCard to " << player.name << "\n";
        }
    }

    for (auto& player : game.players) {
        player.coins = game.initialCoins;
    }

}
//------------------------------------------------------------------------------------

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


    int imgWidth = 2010;
    int imgHeight = 1080;
    GLuint backgroundTexture;
    backgroundTexture = LoadTexture("background3.png", imgWidth, imgHeight);  // Carica l'immagine
    // Initial configuration of values
    CardValues cardValues;
    bool done = false;
    std::string selectedFile;

    initializeGame(game); // Usa un seed definito
    // Assign values to the deck
    assignCardValues(game.deck, cardValues);


    int handCount = 0; // Track the number of hands played

    bool handCompleted = false;
    bool withWinner = false;

    bool isLastHand = false;
    bool isLastRound = false;
    //bool isGameOver = false;

    bool automove = false;
    bool autoplay = false;
    bool autoMatch = false;


    int currentPlayerIndex = 0;
    std::string currentPlayer = game.players[currentPlayerIndex].name;

    //-------------------------------------------------------------------------------------

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
            RenderBackgroundWithAspectRatio(backgroundTexture, imgWidth, imgHeight);  // Draw the image as background
        }
        //------------------------------------


        renderValueConfigPanel(cardValues, game.deck, game.players);
        renderGamePanel(game.isStarted);
        // Main loop
        if (game.isStarted)
        {
            bool allPlayed = game.playedCards.size() == game.players.size();
            // Render player panels
            for (auto& player : game.players) {
                std::string suitToFollow;

                if (!game.playedCards.empty()) {
                    suitToFollow = game.playedCards[0].second.suit;
                }

                if (renderPlayerPanel(player, game.playedCards, currentPlayer, handCompleted, automove, game.players, game.coinsInTreasury, suitToFollow, handCompleted, cardValues)) {

                    // Check if the current player has played their card
                    auto it = std::find_if(game.playedCards.begin(), game.playedCards.end(),
                        [&](const std::pair<std::string, Card>& entry) {
                            return entry.first == currentPlayer;
                        });

                    //Next Player
                    allPlayed = game.playedCards.size() == game.players.size();
                    if (!allPlayed)
                    {
                        if (it != game.playedCards.end()) {
                            // Move to the next player
                            currentPlayerIndex = (currentPlayerIndex + 1) % game.players.size();
                            currentPlayer = game.players[currentPlayerIndex].name;
                        }
                    }

                    player.swappedThisTurn = false;
                }
            }

            renderGameboardPanel(game.playedCards, automove, autoplay, autoMatch, handCompleted);
            renderCardValueManagementPanel(cardValues, "./configs", selectedFile);

            isLastRound = game.deck.size() <= game.players.size();
            allPlayed = game.playedCards.size() == game.players.size();
            if (game.isGameOver)
            {
                ImGui::Begin("GameBoard Panel");
                ImGui::Text("End of the game!");
                ImGui::End();

                Player finalWinner = determineFinalWinner(game.players);
                renderFinalWinnerPanel(finalWinner, game.players);

                ImGui::Begin("GameBoard Panel");
                if (ImGui::Button("Restart"))
                {
                    game.isStarted = false;
                    restartGame(game.players, game.deck, game.isStarted);
                    initializeGame(game); // Usa un seed definito
                    // Assign values to the deck
                    assignCardValues(game.deck, cardValues);
                    handCount = 0; // Track the number of hands played
                    handCompleted = false;
                    withWinner = false;
                    isLastHand = false;
                    bool isLastRound = false;
                    //bool isGameOver = false;
                    automove = false;
                    autoplay = false;
                    autoMatch = false;

                    currentPlayerIndex = 0;
                    currentPlayer = game.players[currentPlayerIndex].name;
                }
                ImGui::End();
            }
            else {
                // Check if all players have played their cards
                if (allPlayed) {
                    // Determine the winner and reset for the next hand
                    if (!handCompleted) {
                        // Determine if this is the last hand of the turn
                        isLastHand = (++handCount % 4 == 0); // Every 4 hands mark the end of a turn
                        if (!withWinner) {
                            if (determineWinnerAndResetHand(game.playedCards, game.players, game.coinsInTreasury, isLastHand, game.winner))
                            {
                                auto& winningPlayer = *std::find_if(game.players.begin(), game.players.end(),
                                    [&](const Player& p) { return p.name == game.winner.name; });

                                winningPlayer.wonCards = game.winner.wonCards;
                                CalculateScore(game.playedCards, winningPlayer, cardValues);

                                // Assign end of round coins if available
                                if (game.coinsInTreasury > 0)
                                {
                                    if (!game.collectGap || (game.collectGap && winningPlayer.coins < 3))
                                    {
                                        if (game.collectEndTurn)
                                        {
                                            winningPlayer.coins++;
                                            game.coinsInTreasury--;
                                        }
                                        else {
                                            bool pureHand = (checkGameRoundPlayedDeck("Parliament") + checkGameRoundPlayedDeck("Betrays") + checkGameRoundPlayedDeck("Guard") == 0);
                                            if (game.coinsInTreasury > 0 && pureHand) {
                                                winningPlayer.coins++;
                                                game.coinsInTreasury--;
                                            }
                                        }
                                    }
                                }
                                std::cout << game.winner.name << " wins the hand and now has "
                                    << winningPlayer.points << " points and "
                                    << winningPlayer.coins << " coins.\n";
                                // There is a winner
                                handCompleted = true;
                                withWinner = true;
                            }
                        }
                    }
                    else if (handCompleted && withWinner) {
                        ImGui::Begin("GameBoard Panel");
                        if (isLastHand)
                        {
                            ImGui::Text("Last hand");
                            ImGui::Separator();
                        }

                        ImGui::Text(std::format("{} won the hand!", game.winner.name).c_str());
                        if (autoMatch || ImGui::Button("Continue"))
                        {

                            // Set the winner as the starting player for the next hand
                            currentPlayerIndex = getPlayerIndexByName(game.players, game.winner.name);
                            currentPlayer = game.winner.name;

                            // If last hand, winner collects all remaining coins
                            if (isLastRound) {
                                auto& winningPlayer = *std::find_if(game.players.begin(), game.players.end(),
                                    [&](const Player& p) { return p.name == game.winner.name; });

                                if (game.collectAtTheEnd)
                                {
                                    winningPlayer.coinsCollected = game.coinsInTreasury;
                                    winningPlayer.coins += game.coinsInTreasury;
                                    std::cout << game.winner.name << " collects the remaining treasury coins!\n";
                                    ImGui::Text(std::format("{} collected {} remaining treasury coins!", game.winner.name, game.coinsInTreasury).c_str());
                                    game.coinsInTreasury = 0;
                                }
                            }
                            // Clear playedCards for the next hand
                            game.playedCards.clear();

                            // If last hand, deal new cards to all players
                            if (isLastHand) {
                                if (!isLastRound) {
                                    game.round++;
                                    game.turn = 1;
                                    dealNewCards(game.players, game.deck);
                                }
                                else {
                                    game.isGameOver = true;
                                }
                            }
                            else {
                                game.turn++;
                            }


                            handCompleted = false;
                            withWinner = false;
                            isLastHand = false;
                        }
                        ImGui::End();
                        ShowAutoPlayControls(autoplay, automove, autoMatch);
                    }
                }

            }
        }

        // Global game information panel
        ImGui::Begin("Game Panel");
        ImGui::PushItemWidth(64);
        if ((game.isGameOver || !game.isStarted))
        {
            ImGui::InputInt("Treasury Coins available", &game.maxCoinsInTreasury);
            ImGui::InputInt("Initial Players Coins", &game.initialCoins);
            ImGui::PopItemWidth();
            ImGui::Checkbox("Collect all at the end", &game.collectAtTheEnd);
            ImGui::Checkbox("Collect every turn", &game.collectEndTurn);
            ImGui::Checkbox("Collect max 3", &game.collectGap);

            if (ImGui::Button("Restore"))
            {
                game.restore();
            }
        }
        ImGui::Separator();
        ImGui::Text("Treasury Coins: %d/%d", game.coinsInTreasury, game.maxCoinsInTreasury);
        ImGui::Text("Deck Size: %lu/52", game.deck.size());

        int N = 52;
        int P = game.players.size();
        int Cf = 4;
        int Cp = 4;
        int Ca = N - Cf;
        int Ct = P * Cp;

        int totalRounds = Ca / Ct;
        ImGui::Text("Round: %d/%d Hand: %d/4", game.round, totalRounds, game.turn);
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
void assignCardValues(std::vector<Card>& deck, const CardValues& cardValues) {
    for (auto& card : deck) {
        if (card.suit == "Betrays") {
            card.pointValue = cardValues.betrayalValues[std::stoi(card.value) - 1]; // Valore basato sull'indice
        }
        else if (card.suit == "Crowns" || card.suit == "Weapons" ||
            card.suit == "Faith" || card.suit == "Diamonds") {
            int index = (std::stoi(card.value) == 1) ? 0 : std::stoi(card.value) - 6;
            if (index >= 0 && index < 5) {
                card.pointValue = cardValues.baseValues[index];
            }
        }
        else if (card.suit == "Parliament") {
            card.pointValue = (card.value == "P") ? cardValues.parliamentPositive : cardValues.parliamentNegative;
        }
        else if (card.suit == "Guard") {
            card.pointValue = cardValues.guardValue;
        }
    }
}

void ShowAutoPlayControls(bool& autoplay, bool& automove, bool& autoMatch)
{
    ImGui::Begin("GameBoard Panel");
    if (ImGui::Button("Auto Move"))
    {
        automove = true;
    };
    ImGui::SameLine();
    ImGui::Checkbox("Auto Play", &autoplay);
    if (autoplay)
    {
        automove = true;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto Match", &autoMatch);
    if (autoMatch)
    {
        automove = true;
        autoplay = true;
    }

    ImGui::Separator();
    ImGui::End();
}



// Function to shuffle the deck
void shuffleDeck(std::vector<Card>& deck) {
    std::shuffle(deck.begin(), deck.end(), randomGenerator);
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

int checkMemoryDeck(const Player& player, const std::string& suitToFollow)
{
    int ret = 0;
    for (const auto& card : player.wonCards)
    {
        if (card.suit == suitToFollow)
            ret++;
    }
    return ret;
}

int checkPlayerHand(const Player& player, const std::string& suitToFollow)
{
    int ret = 0;
    for (const auto& card : player.hand)
    {
        if (card.suit == suitToFollow)
            ret++;
    }
    return ret;
}

int checkGameRoundPlayedDeck(const std::string& suitToFollow)
{
    int ret = 0;
    for (const auto& card : game.playedCards)
    {
        if (card.second.suit == suitToFollow)
            ret++;
    }
    return ret;
}

int checkGameRemainigDeck(const std::string& suitToFollow)
{
    int ret = 0;
    for (const auto& card : game.deck)
    {
        if (card.suit == suitToFollow)
            ret++;
    }
    return ret;
}

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
//--------------------------------------------------------------------------------
bool findBestCombination(
    const std::vector<Card>& hand,
    const playedCardsType& playedCards,
    Player& player, const CardValues& cardValues, const std::string& suitToFollow, std::vector<Card>& retCards, bool memory = false) {


    int maxScore = std::numeric_limits<int>::min();
    int highestOrder = std::numeric_limits<int>::min();
    Card bestCard = hand.front(); // Default to the first card in hand
    bool found = false;
    // Iterate through all cards in hand to simulate combinations
    //int parliamentars = checkMemoryDeck(hand, "Parliament");

    for (const auto& card : hand) {
        // Simulate playing this card
        playedCardsType simulatedCards = playedCards;

        // Add the current card to the simulated cards
        simulatedCards.push_back({ player.name, card });
        if (memory && card.suit == "Parliament")
        {
            if (checkMemoryDeck(player, "Parliament") >= 2)    //We don't want to risk other parliaments
                break;
        }
        // Calculate the score for this combination
        //playedCardsType& playedCards, Player& player, const CardValues& cardValues)
        if (suitToFollow != "" && card.suit == suitToFollow) {
            int simulatedScore = CalculatePScore(simulatedCards, cardValues);

            // Compare with the max score
            if (simulatedScore > 0) {
                if (simulatedScore > maxScore && card.order > highestOrder) {
                    maxScore = simulatedScore;
                    bestCard = card;
                    found = true;
                    highestOrder = card.order;
                    retCards.clear();
                    retCards.push_back(card);
                }
                else if (simulatedScore == maxScore && card.order >= highestOrder) {
                    maxScore = simulatedScore;
                    found = true;
                    highestOrder = card.order;
                    retCards.push_back(card);
                }
            }
        }
    }
    return found;
}

Card chooseBestCard(const std::vector<Card>& hand, const std::string& suitToFollow, const playedCardsType& playedCards, Player& player, const CardValues& cardValues) {

    if (suitToFollow.size() && chance(50 + (playedCards.size()) * 10)) { // Aggiungi una probabilit� di usare questa logica
        std::vector<Card> cards;
        if (findBestCombination(hand, playedCards, player, cardValues, suitToFollow, cards))
        {
            if (cards.size() == 1)
            {
                return cards[0];
            }
        }
    }

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

    // Logica 3: Gioca una Betrayal se � pi� bassa delle altre Betrayal esistenti
    if (chance(80)) {
        int lowestBetrayal = INT_MAX;
        for (const auto& entry : playedCards) {
            if (entry.second.suit == "Betrays") {
                lowestBetrayal = std::min(lowestBetrayal, std::stoi(entry.second.value));
            }
        }
        for (const auto& card : hand) {
            if (card.suit == "Betrays" && std::stoi(card.value) < lowestBetrayal && lowestBetrayal != INT_MAX) {
                return card; // Gioca una Betrayal se � pi� bassa
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

    // Logica 2: Gioca un Parliament se non si � l'ultimo di mano
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
            if (card.pointValue >= 0 && card.pointValue < minValue) {
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

bool renderPlayerPanel(Player& player, playedCardsType& playedCards, const std::string& currentPlayer, bool justShow, bool& automove, std::vector<Player>& allPlayers, int& coinsInTreasury, const std::string& suitToFollow, bool handCompleted, const CardValues& cardValues) {

    ImGui::Begin(("Player " + player.name).c_str());
    ImGui::Text("Coins: %d", player.coins);
    ImGui::Text("Points: %d(%+d)", player.points, player.points - player.lastScore);
    ImGui::SameLine();  ImGui::Text(std::format("    {}:{} {}:{} {}:{}", ICON_FA_BUILDING_COLUMNS, player.parliaments, ICON_FA_CIRCLE_PLUS, player.goodPoints, ICON_FA_CIRCLE_MINUS, player.negativePoints).c_str());
    ImGui::Text("Missions: %d", player.missions);

    ImGui::Separator();
    if (ImGui::Button("Show Won Cards")) {
        ImGui::OpenPopup("Won Cards");
    }

    // Popup logic for displaying won cards
    if (ImGui::BeginPopup("Won Cards")) {
        ImGui::Text("Cards Won by %s", player.name.c_str());
        int index = 0;
        for (const auto& card : player.wonCards) {
            ImGui::TextFormatted("%s%s%s{FFFFFFFF} (%+d points)\n",
                card.value.c_str(),
                getSuitColorString(card.suit).c_str(),
                iconify(card.suit).c_str(),
                card.pointValue);
            index++;
            if ((index % allPlayers.size()) == 0)
                ImGui::Separator();
        }
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    bool played = false;
    bool isCurrentPlayer = (player.name == currentPlayer);

    if (isCurrentPlayer) {
        ImGui::Separator();
        ImGui::Text("*** Playing ***");
        ImGui::Separator();
    }

    auto invisible = IM_COL32(255, 255, 255, 16);

    // Display player's hand
    auto handSize = player.hand.size();
    for (size_t i = 0; i < handSize; ++i) {

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
                auto& bestCard = player.hand[i];
                playedCards.push_back({ player.name, bestCard });

                auto it = std::find_if(player.hand.begin(), player.hand.end(),
                    [&bestCard](const Card& card) {
                        return card.value == bestCard.value && card.suit == bestCard.suit;
                    });
                if (it != player.hand.end()) {
                    player.hand.erase(it);
                }
                else
                {
                    std::cout << "Player " << player.name << " played " << playedCards.back().second.value
                        << " of " << playedCards.back().second.suit << "\n";
                }
                played = true;
                ImGui::PopStyleColor(4);
                break;
            }
        }

        ImGui::PopStyleColor(4);
        if (i < player.hand.size() - 1) {
            ImGui::SameLine();
        }
    }

    if (!played)
    {
        auto handSize = player.hand.size();
        for (size_t i = 0; i < handSize; ++i) {
            // Swap with swapCard
            if (isCurrentPlayer && !justShow && !player.swappedThisTurn && player.coins > 0) {

                if (ImGui::Button(("Swap##swap_" + std::to_string(i)).c_str(), ImVec2(100, 20))) {
                    // Swap the selected hand card with the swap card
                    std::swap(player.hand[i], player.swapCard);
                    player.coins--; // Deduct a coin for the swap
                    coinsInTreasury++;

                    player.swappedThisTurn = i + 1;

                    std::cout << "Player " << player.name << " swapped hand card " << player.hand[i].value
                        << " with their swap card " << player.swapCard.value << "\n";

                    // Ensure the colors update correctly (force re-rendering with updated data)
                    ImGui::PushStyleColor(ImGuiCol_Button, getSuitColor(player.hand[i].suit));
                    ImGui::PopStyleColor();
                    break;
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

        if (ImGui::Button((hiddenText + "##swapCard_" + player.name).c_str(), ImVec2(100, 80)))
        {
            if (isCurrentPlayer && player.swappedThisTurn)
            {

                playedCards.push_back({ player.name, player.swapCard });
                std::swap(player.hand[player.swappedThisTurn - 1], player.swapCard);
                player.hand.erase(player.hand.begin() + (player.swappedThisTurn - 1));
                std::cout << "Player " << player.name << " swap-played " << playedCards.back().second.value
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

        if (!played && isCurrentPlayer && !handCompleted) {

            ImGui::SameLine();

            if ((automove && !player.control) || ImGui::Button("Auto Play##auto_move", ImVec2(200, 40))) {

                if (!player.hand.empty()) {
                    // Scegli la migliore carta da giocare
                    Card bestCard = chooseBestCard(player.hand, suitToFollow, playedCards, player, cardValues);
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

                    std::cout << "Player " << player.name << " auto-played " << bestCard.value
                        << " of " << bestCard.suit << "\n";

                    played = true;
                }
            }
            ImGui::SameLine();
            ImGui::Checkbox(std::format("Control##control{}", player.name).c_str(), &player.control);
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

void saveCardValues(const std::string& filename, const CardValues& cardValues) {
    json j;

    j["baseValues"] = { cardValues.baseValues[0], cardValues.baseValues[1], cardValues.baseValues[2], cardValues.baseValues[3], cardValues.baseValues[4] };
    j["betrayalValues"] = { cardValues.betrayalValues[0], cardValues.betrayalValues[1], cardValues.betrayalValues[2], cardValues.betrayalValues[3],
                            cardValues.betrayalValues[4], cardValues.betrayalValues[5], cardValues.betrayalValues[6], cardValues.betrayalValues[7], cardValues.betrayalValues[8] };
    j["parliamentPositive"] = cardValues.parliamentPositive;
    j["parliamentNegative"] = cardValues.parliamentNegative;
    j["guardValue"] = cardValues.guardValue;
    j["coinValue"] = cardValues.coinValue;

    std::ofstream file(filename);
    if (file.is_open()) {
        file << j.dump(4); // Salva con indentazione per leggibilit�
        file.close();
    }
}

void loadCardValues(const std::string& filename, CardValues& cardValues) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return;
    }

    json j;
    file >> j;

    for (int i = 0; i < 5; ++i) {
        cardValues.baseValues[i] = j["baseValues"][i];
    }
    for (int i = 0; i < 9; ++i) {
        cardValues.betrayalValues[i] = j["betrayalValues"][i];
    }

    cardValues.parliamentPositive = j["parliamentPositive"];
    cardValues.parliamentNegative = j["parliamentNegative"];
    cardValues.guardValue = j["guardValue"];
    cardValues.coinValue = j["coinValue"];
}


//renderConfigManagementPanel(daimyoList, scoringRules, "./configs", selectedFile, daimyoNames);
void renderCardValueManagementPanel(CardValues& cardValues, const std::string& directoryPath, std::string& selectedFile) {
    std::string sourcePath = directoryPath + "/" + selectedFile;
    std::string duplicateName = selectedFile + "_copy";
    std::string destinationPath = directoryPath + "/" + duplicateName;

    ensureDirectoryExists(directoryPath);

    static std::vector<std::string> configFiles = getConfigFiles(directoryPath);
    static char newFileName[128] = "";

    ImGui::Begin("Card Value Configuration");

    // Lista di file disponibili
    ImGui::Text("Available Configurations:");
    ImGui::Separator();
    for (const auto& file : configFiles) {
        bool isSelected = (selectedFile == file);
        if (ImGui::Selectable(file.c_str(), isSelected)) {
            selectedFile = file; // Imposta il file selezionato
            strncpy(newFileName, selectedFile.c_str(), 128);
        }
    }

    ImGui::Separator();

    // Campo per creare un nuovo file
    ImGui::InputText("File Name", newFileName, sizeof(newFileName));
    ImGui::SameLine();
    if (ImGui::Button("Generate")) {
        auto now = std::chrono::system_clock::now();
        auto nowTimeT = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << "config_" << std::put_time(std::localtime(&nowTimeT), "%Y%m%d_%H%M%S") << ".json";
        strncpy(newFileName, oss.str().c_str(), sizeof(newFileName) - 1);
    }

    // Pulsante per caricare la configurazione
    if (ImGui::Button("Load Configuration")) {
        if (!selectedFile.empty()) {
            std::string fullPath = directoryPath + "/" + selectedFile;
            loadCardValues(fullPath, cardValues);
            ImGui::Text("Loaded configuration: %s", selectedFile.c_str());
        }
    }

    ImGui::Separator();

    // Pulsante per salvare la configurazione
    if (ImGui::Button("Save Configuration")) {
        std::string fullPath = directoryPath + "/" + std::string(newFileName);
        saveCardValues(fullPath, cardValues);
        ImGui::Text("Saved configuration to: %s", fullPath.c_str());
        configFiles = getConfigFiles(directoryPath); // Aggiorna la lista di file
    }

    ImGui::Separator();

    // Pulsante per cancellare un file
    if (!selectedFile.empty() && ImGui::Button("Delete Selected Configuration")) {
        std::string fullPath = directoryPath + "/" + selectedFile;
        try {
            std::filesystem::remove(fullPath);
            selectedFile.clear();
            configFiles = getConfigFiles(directoryPath); // Aggiorna l'elenco
        }
        catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Failed to delete file: " << e.what() << std::endl;
        }
    }

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
        if (card.second.suit != "Guard" && card.second.suit != "Betrays" && card.second.suit != "Parliament")
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

int CalculatePlayedScore(const CardValues& cardValues, bool checkParliaments = false)
{
    int points = 0;
    int parliamentCount = 0;

    for (auto item : game.playedCards)
    {
        Card card = item.second;
        if (card.suit != "Parliament")
        {
            points += card.pointValue;
        }
        else
        {
            parliamentCount++;
        }
    }
    if (checkParliaments)
    {
        if ((parliamentCount % 2) == 1)
        {
            points += cardValues.parliamentNegative;
        }
        else if (((parliamentCount % 2) == 0) && parliamentCount > 1)
        {
            points += cardValues.parliamentPositive;
        }
    }
    return points;
}
int CalculatePScore(playedCardsType& playedCards, const CardValues& cardValues)
{
    int points = 0;
    int parliamentCount = 0;

    for (auto item : playedCards)
    {
        Card card = item.second;
        if (card.suit != "Parliament")
        {
            points += card.pointValue;
        }
        else
        {
            parliamentCount++;
        }
    }
    if ((parliamentCount % 2) == 1)
    {
        points += cardValues.parliamentNegative;
    }
    else if (((parliamentCount % 2) == 0) && parliamentCount > 1)
    {
        points += cardValues.parliamentPositive;
    }
    return points;
}
void CalculateScore(playedCardsType& playedCards, Player& player, const CardValues& cardValues)
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
            if (card.pointValue > 0) player.goodPoints += card.pointValue;
            else if (card.pointValue < 0) player.negativePoints += card.pointValue;
        }
        else
        {
            parliamentCount++;
        }
    }
    if ((parliamentCount % 2) == 1)
    {
        player.points += cardValues.parliamentNegative;
        player.parliaments = cardValues.parliamentNegative;

    }
    else if (parliamentCount > 1)
    {
        player.points += cardValues.parliamentPositive;
        player.parliaments = cardValues.parliamentPositive;
    }

}

Player determineFinalWinner(const std::vector<Player>& players) {
    if (players.empty()) {
        throw std::runtime_error("No players to determine a winner.");
    }

    const Player* winner = &players[0];
    for (const auto& player : players) {
        // Confronto basato sui punti
        if (player.points + player.coins > winner->points + winner->coins) {
            winner = &player;
        }
    }

    return *winner;
}

// Helper function to get the color based on suit
ImVec4 getSuitColor(const std::string& suit) {
    if (suit == "Weapons") return ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
    if (suit == "Crowns") return ImVec4(0.7f, 0.7f, 0.1f, 1.0f); // Yellow
    if (suit == "Faith") return ImVec4(0.0f, 0.8f, 0.0f, 1.0f); // Green
    if (suit == "Diamonds") return ImVec4(0.0f, 0.0f, 1.0f, 1.0f); // Blue
    if (suit == "Betrays") return ImVec4(0.45f, 0.0f, 0.45f, 1.0f); // Dark Purple
    if (suit == "Parliament") return ImVec4(0.55f, 0.4f, 0.2f, 1.0f); // Brown
    if (suit == "Guard") return ImVec4(0.6f, 0.6f, 0.7f, 1.0f); // White
    return ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // Default White
}
std::string getSuitColorString(const std::string& suit) {
    ImVec4 color = getSuitColor(suit);
    // Convert ImVec4 to 8-bit RGBA values
    int r = static_cast<int>(color.x * 255);
    int g = static_cast<int>(color.y * 255);
    int b = static_cast<int>(color.z * 255);
    int a = static_cast<int>(color.w * 255);

    // Format as "{RRGGBBAA}"
    std::ostringstream oss;
    oss << std::uppercase << std::setfill('0') << std::hex;
    oss << "{" << std::setw(2) << r
        << std::setw(2) << g
        << std::setw(2) << b
        << std::setw(2) << a << "}";

    return oss.str();
}

void updatePlayerHandValues(std::vector<Player>& players, const CardValues& cardValues) {
    for (auto& player : players) {
        for (auto& card : player.hand) {
            // Update the card's point value based on the current cardValues
            if (card.suit == "Crowns" || card.suit == "Weapons" || card.suit == "Faith" || card.suit == "Diamonds") {
                // Base cards
                int index = std::stoi(card.value) - 5;
                card.pointValue = card.value == "1" ? cardValues.baseValues[0] : card.value >= "6" ? cardValues.baseValues[index] : 0;
            }
            else if (card.suit == "Betrays") {
                // Betrayal
                int index = std::stoi(card.value) - 1;
                card.pointValue = cardValues.betrayalValues[index];
            }
            else if (card.suit == "Parliament") {
                // Parliament
                card.pointValue = 3;
            }
            else if (card.suit == "Guard") {
                // Guard
                card.pointValue = cardValues.guardValue;
            }
        }
    }
}

void renderValueConfigPanel(CardValues& cardValues, std::vector<Card>& deck, std::vector<Player>& players) {
    ImGui::Begin("Card Values Configuration");

    static bool valuesChanged = false; // Traccia se ci sono cambiamenti
    /* { "Crowns", ICON_FA_CROWN },
 { "Weapons", ICON_FA_TROWEL },
 { "Faith", ICON_FA_CROSS },
 { "Diamonds", ICON_FA_GEM },
 { "Betrays", ICON_FA_DRAGON },
 { "Guards", ICON_FA_SHIELD_HALVED },
 { "Parliament", ICON_FA_BUILDING_COLUMNS }*/

 // Configuration of the values of the base cards
    static std::string frmtxt = std::format("Base Suit Values {}{} {}{} {}{} {}{}{}:\n", getSuitColorString("Crowns"), ICON_FA_CROWN, getSuitColorString("Weapons"), ICON_FA_TROWEL, getSuitColorString("Faith"), ICON_FA_CROSS, getSuitColorString("Diamonds"), ICON_FA_GEM, "{FFFFFFFF}");
    ImGui::TextFormatted(frmtxt.c_str());
    // ImGui::TextFormatted("Base Suit Values{FF0000FF}rosso{FFFFFFFF} e poi normale{00FF00}verde{FFFFFF}\n");
    static std::vector<std::string>ids = { "A", "6", "7", "8", "9" };
    ImGui::PushItemWidth(64);
    for (int i = 0; i < 5; ++i) {
        if (ImGui::InputInt(std::format("{} " ICON_FA_SD_CARD, ids[i]).c_str(), &cardValues.baseValues[i])) {
            cardValues.baseValues[i] = std::clamp<int>(cardValues.baseValues[i], -3, 4);
            valuesChanged = true;
        }
    }

    ImGui::Separator();

    // Configuration of Betrayal values
    ImGui::Text("Betrayal Values:");
    static std::vector<std::string>idBs = { "A","2","3","4","5","6", "7", "8", "9" };
    for (int i = 0; i < 9; ++i) {
        if (ImGui::InputInt(std::format("##{}", idBs[i]).c_str(), &cardValues.betrayalValues[i])) {
            cardValues.betrayalValues[i] = std::clamp<int>(cardValues.betrayalValues[i], -3, 4);
            valuesChanged = true;
        }
        ImGui::SameLine();
        ImGui::TextFormatted(std::format("{}{} {}", idBs[i], getSuitColorString("Betrays"), ICON_FA_DRAGON).c_str());
    }

    ImGui::Separator();

    // Configuration Parliament
    ImGui::Text("Parliament Values:");
    if (ImGui::InputInt("##Parliament2", &cardValues.parliamentPositive))
    {
        cardValues.parliamentPositive = std::clamp<int>(cardValues.parliamentPositive, -3, 4);
        valuesChanged = true;
    }
    ImGui::SameLine();
    ImGui::TextFormatted(std::format("{}{}{}", getSuitColorString("Parliament"), ICON_FA_BUILDING_COLUMNS, ICON_FA_BUILDING_COLUMNS).c_str());

    if (ImGui::InputInt("##Parliament1", &cardValues.parliamentNegative))
    {
        cardValues.parliamentNegative = std::clamp<int>(cardValues.parliamentNegative, -3, 4);
        valuesChanged = true;
    }
    ImGui::SameLine();
    ImGui::TextFormatted(std::format("{}{}", getSuitColorString("Parliament"), ICON_FA_BUILDING_COLUMNS).c_str());


    ImGui::Separator();

    // Guard Configuration
    ImGui::Text("Guard Value:");
    if (ImGui::InputInt("##guard", &cardValues.guardValue))
    {
        cardValues.guardValue = std::clamp<int>(cardValues.guardValue, -3, 4);
        valuesChanged = true;
    }
    ImGui::SameLine();
    ImGui::TextFormatted(std::format("{}{}", getSuitColorString("Guard"), ICON_FA_SHIELD_HALVED).c_str());
    ImGui::Separator();

    // Configuration of Coins
    ImGui::Text("Coin Value:");

    if (ImGui::InputInt(ICON_FA_COINS, &cardValues.coinValue))
    {
        cardValues.coinValue = std::clamp<int>(cardValues.coinValue, -3, 4);
        valuesChanged = true;
    }

    ImGui::PopItemWidth();

    // Update the values in the deck if they have been changed
    if (valuesChanged) {
        assignCardValues(deck, cardValues);
        updatePlayerHandValues(players, cardValues); // Update the values in the hands of players
        valuesChanged = false; // Reset after the update
        std::cout << "Card values updated!" << std::endl;
    }

    ImGui::End();
}
void renderGamePanel(bool& gameStarted) {

    ImGui::Begin("Seed Configuration");
    ImGui::Text("Configure Random Seed:");

    // Input del seed
    ImGui::InputScalar("Seed", ImGuiDataType_U32, &game.randomSeed);
    if (ImGui::Button("Apply Seed")) {
        setRandomSeed(game.randomSeed); // Applica il nuovo seed
    }

    // Pulsante per generare un seed casuale basato sul tempo
    if (ImGui::Button("Generate Random Seed")) {
        unsigned int newSeed = std::random_device{}();
        //int newSeed = static_cast<int>(std::chrono::system_clock::now().time_since_epoch().count());
        setRandomSeed(newSeed);
        game.randomSeed = newSeed; // Aggiorna il valore visibile all'utente
    }

    ImGui::Text(std::format("Current Seed: {}", randomSeed).c_str());

    ImGui::Separator();
    if (!gameStarted)
    {
        if (ImGui::Button("Start Game"))
        {
            gameStarted = true;
        }
    }
    ImGui::End();
}
void setRandomSeed(unsigned int seed) {
    randomSeed = seed;
    randomGenerator.seed(seed); // Imposta il seed
    std::cout << "Random seed set to: " << seed << "\n";
}
int getPlayerIndexByName(const std::vector<Player>& players, const std::string& name) {
    for (size_t i = 0; i < players.size(); ++i) {
        if (players[i].name == name) {
            return static_cast<int>(i); // Return the index if the name matches
        }
    }
    return -1; // Return -1 if the player is not found
}
void renderGameboardPanel(const playedCardsType& playedCards, bool& automove, bool& autoplay, bool& automatch, bool handCompleted) {
    ImGui::Begin("GameBoard Panel");
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

        ImGui::PopStyleColor();
    }
    ImGui::End();
    ImGui::Begin("GameBoard Panel");
    //First hand
    if (!handCompleted)
    {
        ShowAutoPlayControls(autoplay, automove, automatch);
    }
    ImGui::Separator();
    ImGui::End();
}
void renderFinalWinnerPanel(const Player& winner, const std::vector<Player>& players) {
    ImGui::Begin("GameBoard Panel");

    ImGui::Text("The final winner is: %s", winner.name.c_str());
    ImGui::Text("Score:%d - (Points: %d, Coins: %d)", winner.points + winner.coins, winner.points, winner.coins);
    ImGui::Separator();
    for (const auto& player : players)
    {
        ImGui::Text("%s - Score: %d - (Points: %d, Coins: %d)", player.name.c_str(), player.points + player.coins, player.points, player.coins);
    }

    ImGui::End();
}
bool restartGame(std::vector<Player>& players, std::vector<Card>& deck, bool& gameStarted)
{
    shuffleDeck(deck);
    dealNewCards(players, deck);
    gameStarted = false;

    return true;
}
