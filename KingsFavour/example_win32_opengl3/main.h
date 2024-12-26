#pragma once
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

#include "PlayerProgression.h"

// ImGui and GLFW
//#include "imgui_impl_glfw.h"
//#include <GLFW/glfw3.h>

using namespace std;
using json = nlohmann::json;

//------------------------------------------------------------------------------------
void ensureDirectoryExists(const std::string& directoryPath) {
    try {
        if (!std::filesystem::exists(directoryPath)) {
            std::filesystem::create_directories(directoryPath);
            std::cout << "Directory created: " << directoryPath << std::endl;
        }
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error ensuring directory exists: " << e.what() << std::endl;
    }
}

std::vector<std::string> getConfigFiles(const std::string& directoryPath) {
    std::vector<std::string> files;

    try {
        if (!std::filesystem::exists(directoryPath)) {
            throw std::runtime_error("Directory does not exist: " + directoryPath);
        }

        for (const auto& entry : std::filesystem::directory_iterator(directoryPath)) {
            if (entry.is_regular_file()) {
                files.push_back(entry.path().filename().string());
            }
        }
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return files;
}

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
    int score = 0;
    int turnWin = 0;
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

    std::vector<int> pointsHistory;
    std::vector<int> coinsHistory;
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
        PLOG_ERR("Failed to load image: " << filename);
        PLOG_ERR_FLUSH("Error", 0,0);
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
    int turn = 0;
    unsigned int randomSeed = 0;
    bool isGameOver = false;
    bool isStarted = false;
    bool collectAtTheEnd = false;
    bool collectEndTurn = false;
    bool collectGap = false;
    bool collectPureHand = false;
    bool collectWinBetray = false;
    bool collectParliament = false;
    bool showKeepCard = false;
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
        turn = 0;
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
void renderGamePanel(bool& gameStarted, bool autoRestart);


int getPlayerIndexByName(const std::vector<Player>& players, const std::string& name);

void renderGameboardPanel(const playedCardsType& playedCards, bool& automove, bool& autoplay, bool& automatch, bool handCompleted);
bool restartGame(std::vector<Player>& players, std::vector<Card>& deck, bool& gameStarted);
void gameStart();

bool CollectCoins(Player& winningPlayer);


void initializeGame(Game& game) {

    playerProgression.clear();

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

            PLOG("Assigned swapCard to " << player.name << ": "
                << player.swapCard.value << " of " << player.swapCard.suit);
            PLOG_FLUSH(player.name, game.turn, game.round);

        }
        else {
            PLOG_ERR("Deck is empty! Cannot assign swapCard to " << player.name);
            PLOG_ERR_FLUSH(player.name, game.turn, game.round);
        }
    }
    game.coinsInTreasury = game.maxCoinsInTreasury;
    for (auto& player : game.players) {
        player.coins = game.initialCoins;
        game.coinsInTreasury -= game.initialCoins;
    }



}
