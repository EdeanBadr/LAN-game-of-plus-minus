#include <gtest/gtest.h>
#include "httplib.h"
#include "nlohmann/json.hpp"
#include <thread>
#include <chrono>
#include <vector>
#include <future>
using json = nlohmann::json;
struct ServerConfig {
    int port = 4242;
    std::string bounds = "1,100";
    int lower_bound = 1;
    int upper_bound = 100;
    int limit = -1;
};
class GameServerTest : public ::testing::Test {
protected:
    httplib::Client* cli;
    const int PORT = 4242;
    std::thread server_thread;

    void SetUp() override {
        cli = new httplib::Client("localhost", PORT);
    }

    void TearDown() override {
        delete cli;
    }
};

TEST_F(GameServerTest, StartGameValidInput) {
    json request;
    request["name"] = "testPlayer";

    auto res = cli->Post("/start", request.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);

    auto response = json::parse(res->body);
    EXPECT_TRUE(response.contains("uniqueName"));
    EXPECT_TRUE(response.contains("message"));
    EXPECT_EQ(response["message"], "The Game started! Your number is between 1 and 100");
}

TEST_F(GameServerTest, StartGameInvalidInput) {
    json request;  
    
    auto res = cli->Post("/start", request.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 400);
}

TEST_F(GameServerTest, ValidGuess) {
    json start_request;
    start_request["name"] = "testPlayer";
    auto start_res = cli->Post("/start", start_request.dump(), "application/json");
    auto start_response = json::parse(start_res->body);
    std::string player_name = start_response["uniqueName"];

    json guess_request;
    guess_request["name"] = player_name;
    guess_request["guess"] = 50;
    guess_request["auto"] = false;

    auto res = cli->Post("/guess", guess_request.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);

    auto response = json::parse(res->body);
    EXPECT_TRUE(response.contains("hint"));
    EXPECT_TRUE(response.contains("message"));
}

TEST_F(GameServerTest, GuessNonExistentPlayer) {
    json request;
    request["name"] = "nonExistentPlayer";
    request["guess"] = 50;
    request["auto"] = false;

    auto res = cli->Post("/guess", request.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 400);

    auto response = json::parse(res->body);
    EXPECT_EQ(response["hint"], "error");
}

TEST_F(GameServerTest, GuessLimit) {
    ServerConfig config;
    config.limit = 3;
    
    json start_request;
    start_request["name"] = "limitTestPlayer";
    auto start_res = cli->Post("/start", start_request.dump(), "application/json");
    auto start_response = json::parse(start_res->body);
    std::string player_name = start_response["uniqueName"];

    json guess_request;
    guess_request["name"] = player_name;
    guess_request["auto"] = false;

    for (int i = 0; i < config.limit; i++) {
        guess_request["guess"] = i;
        auto res = cli->Post("/guess", guess_request.dump(), "application/json");
        auto response = json::parse(res->body);
        
        if (i == config.limit) {
            EXPECT_EQ(response["hint"], "game_over");
        }
    }
}

TEST_F(GameServerTest, GiveUp) {
    json start_request;
    start_request["name"] = "giveUpPlayer";
    auto start_res = cli->Post("/start", start_request.dump(), "application/json");
    auto start_response = json::parse(start_res->body);
    std::string player_name = start_response["uniqueName"];

    json giveup_request;
    giveup_request["name"] = player_name;

    auto res = cli->Post("/giveup", giveup_request.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);

    auto response = json::parse(res->body);
    EXPECT_TRUE(response.contains("Target"));
    EXPECT_EQ(response["message"], "Never Give Up Again!");
}

TEST_F(GameServerTest, QuitGame) {
    json start_request;
    start_request["name"] = "quitPlayer";
    auto start_res = cli->Post("/start", start_request.dump(), "application/json");
    auto start_response = json::parse(start_res->body);
    std::string player_name = start_response["uniqueName"];

    json quit_request;
    quit_request["name"] = player_name;
    quit_request["auto"] = false;

    auto res = cli->Post("/quit", quit_request.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);

    auto response = json::parse(res->body);
    EXPECT_TRUE(response.contains("message"));
    EXPECT_TRUE(response.contains("top_scores"));
}

TEST_F(GameServerTest, NewGame) {
    json start_request;
    start_request["name"] = "newGamePlayer";
    auto start_res = cli->Post("/start", start_request.dump(), "application/json");
    auto start_response = json::parse(start_res->body);
    std::string player_name = start_response["uniqueName"];

    json newgame_request;
    newgame_request["name"] = player_name;

    auto res = cli->Post("/newGame", newgame_request.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);

    auto response = json::parse(res->body);
    EXPECT_EQ(response["hint"], "new_game");
}

TEST_F(GameServerTest, BoundaryValueGuesses) {
    json start_request;
    start_request["name"] = "boundaryPlayer";
    auto start_res = cli->Post("/start", start_request.dump(), "application/json");
    auto start_response = json::parse(start_res->body);
    std::string player_name = start_response["uniqueName"];

    std::vector<int> test_values = {
        -1,            
        0,              
        1,               
        50,              
        100,            
        101,             
        INT_MAX         
    };

    for (int value : test_values) {
        json guess_request;
        guess_request["name"] = player_name;
        guess_request["guess"] = value;
        guess_request["auto"] = false;

        auto res = cli->Post("/guess", guess_request.dump(), "application/json");
        ASSERT_TRUE(res != nullptr);
        EXPECT_EQ(res->status, 200) << "Failed for value: " << value;
        
        auto response = json::parse(res->body);
        if (value < 1 || value > 100) {
            EXPECT_TRUE(response.contains("error") || 
                       response["hint"] == "invalid") 
                << "Should reject value: " << value;
        } else {
            EXPECT_TRUE(response["hint"] == "higher" || 
                       response["hint"] == "lower" || 
                       response["hint"] == "correct")
                << "Invalid hint for value: " <<response["hint"]<< "for"<<value;
        }
    }
}

TEST_F(GameServerTest, MalformedJsonRequests) {
    std::vector<std::string> malformed_requests = {
        "",                         
        "not json at all",         
        "{",                      
        "{\"name\":}"       

    };
    for (const auto& request : malformed_requests) {
        auto res = cli->Post("/guess", request, "application/json");
        ASSERT_TRUE(res != nullptr);
        EXPECT_EQ(res->status, 400) << "Should reject malformed request: " << request;
    }
}

TEST_F(GameServerTest, ConcurrentPlayers) {
    const int NUM_PLAYERS = 10;
    std::vector<std::future<void>> futures;
    std::vector<std::string> player_names;

    for (int i = 0; i < NUM_PLAYERS; i++) {
        json start_request;
        start_request["name"] = "player" + std::to_string(i);
        auto start_res = cli->Post("/start", start_request.dump(), "application/json");
        auto start_response = json::parse(start_res->body);
        player_names.push_back(start_response["uniqueName"]);
    }

    for (const auto& player_name : player_names) {
        futures.push_back(std::async(std::launch::async, [this, player_name]() {
            for (int i = 0; i < 5; i++) {
                json guess_request;
                guess_request["name"] = player_name;
                guess_request["guess"] = 50;
                guess_request["auto"] = false;

                auto res = cli->Post("/guess", guess_request.dump(), "application/json");
                ASSERT_TRUE(res != nullptr);
                EXPECT_EQ(res->status, 200);
            }
        }));
    }

    for (auto& future : futures) {
        future.get();
    }
}

TEST_F(GameServerTest, SessionPersistence) {
    json start_request;
    start_request["name"] = "persistencePlayer";
    auto start_res = cli->Post("/start", start_request.dump(), "application/json");
    auto start_response = json::parse(start_res->body);
    std::string player_name = start_response["uniqueName"];

    json guess_request;
    guess_request["name"] = player_name;
    guess_request["guess"] = 50;
    guess_request["auto"] = false;

    auto first_res = cli->Post("/guess", guess_request.dump(), "application/json");
    ASSERT_TRUE(first_res != nullptr);

    json newgame_request;
    newgame_request["name"] = player_name;
    auto newgame_res = cli->Post("/newGame", newgame_request.dump(), "application/json");
    ASSERT_TRUE(newgame_res != nullptr);
    
    guess_request["guess"] = 75;
    auto second_res = cli->Post("/guess", guess_request.dump(), "application/json");
    ASSERT_TRUE(second_res != nullptr);
    auto response = json::parse(second_res->body);
    EXPECT_NE(response["hint"], "game_over") 
        << "Guess count should reset after new game";
}

TEST_F(GameServerTest, GameHistoryPersistence) {
    json start_request;
    start_request["name"] = "historyPlayer";
    auto start_res = cli->Post("/start", start_request.dump(), "application/json");
    auto start_response = json::parse(start_res->body);
    std::string player_name = start_response["uniqueName"];
    json guess_request;
    guess_request["name"] = player_name;
    guess_request["auto"] = false;

    for (int guess = 1; guess <= 100; guess++) {
        guess_request["guess"] = guess;
        auto res = cli->Post("/guess", guess_request.dump(), "application/json");
        auto response = json::parse(res->body);
        if (response["hint"] == "correct") {
            break;
        }
    }
    json quit_request;
    quit_request["name"] = player_name;
    quit_request["auto"] = false;
    auto quit_res = cli->Post("/quit", quit_request.dump(), "application/json");
    ASSERT_TRUE(quit_res != nullptr);
    auto quit_response = json::parse(quit_res->body);
    
    EXPECT_TRUE(quit_response.contains("top_scores"));
    EXPECT_FALSE(quit_response["top_scores"].empty());
}
int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}