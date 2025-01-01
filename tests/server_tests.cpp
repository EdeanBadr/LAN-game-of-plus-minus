#include <gtest/gtest.h>
#include "httplib.h"
#include "nlohmann/json.hpp"
#include <thread>
#include <chrono>
#include <vector>
#include <future>

using json = nlohmann::json;

class GameServerTest : public ::testing::Test {
protected:
    httplib::Client* cli;
    const int PORT = 4242;
    
    void SetUp() override {
        cli = new httplib::Client("localhost", PORT);
    }
    
    void TearDown() override {
        delete cli;
    }
};

TEST_F(GameServerTest, StartGameValidName) {
    auto res = cli->Get("/start?name=testPlayer");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);

    auto response = json::parse(res->body);
    EXPECT_TRUE(response.contains("uniqueName"));
    EXPECT_TRUE(response.contains("message"));
    EXPECT_TRUE(response.contains("lowerbound"));
    EXPECT_TRUE(response.contains("upperbound"));
    
    int lower = response["lowerbound"].get<int>();
    int upper = response["upperbound"].get<int>();
    EXPECT_EQ(lower, 1);
    EXPECT_EQ(upper, 100);
}

TEST_F(GameServerTest, StartGameMissingName) {
    auto res = cli->Get("/start");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 400);
    EXPECT_EQ(res->body, "Invalid request: the name parameter is required!");
}

TEST_F(GameServerTest, GuessWithoutStarting) {
    json guess_request;
    guess_request["guess"] = 50;
    
    httplib::Headers headers = {
        {"Username", "nonexistent_player"},
        {"Auto", "0"}
    };
    
    auto res = cli->Post("/guess", headers, guess_request.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    
    auto response = json::parse(res->body);
    EXPECT_TRUE(response.contains("error"));
    EXPECT_EQ(response["error"], "You cant guess without starting a game first.");
}

TEST_F(GameServerTest, ValidGuessFlow) {
    auto start_res = cli->Get("/start?name=testPlayer");
    auto start_response = json::parse(start_res->body);
    std::string unique_name = start_response["uniqueName"];
    
    json guess_request;
    guess_request["guess"] = 50;
    
    httplib::Headers headers = {
        {"Username", unique_name},
        {"Auto", "0"}
    };
    
    auto res = cli->Post("/guess", headers, guess_request.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);
    
    auto response = json::parse(res->body);
    EXPECT_TRUE(response.contains("hint"));
    EXPECT_TRUE(response.contains("message"));
    EXPECT_TRUE(response["hint"] == "higher" || response["hint"] == "lower" || response["hint"] == "correct");
}

TEST_F(GameServerTest, InvalidGuessRange) {
    auto start_res = cli->Get("/start?name=testPlayer");
    auto start_response = json::parse(start_res->body);
    std::string unique_name = start_response["uniqueName"];
    
    json guess_request;
    guess_request["guess"] = 101;
    
    httplib::Headers headers = {
        {"Username", unique_name},
        {"Auto", "0"}
    };
    
    auto res = cli->Post("/guess", headers, guess_request.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    
    auto response = json::parse(res->body);
    EXPECT_EQ(response["hint"], "invalid");
}

TEST_F(GameServerTest, GiveUpWithoutStarting) {
    httplib::Headers headers = {
        {"Username", "nonexistent_player"},
        {"Auto", "0"}
    };
    
    auto res = cli->Post("/giveup", headers, "", "application/json");
    ASSERT_TRUE(res != nullptr);
    
    auto response = json::parse(res->body);
    EXPECT_TRUE(response.contains("error"));
    EXPECT_EQ(response["error"], "You haven't even started a game properly and you trying to give up?");
}

TEST_F(GameServerTest, ValidGiveUp) {
    auto start_res = cli->Get("/start?name=testPlayer");
    auto start_response = json::parse(start_res->body);
    std::string unique_name = start_response["uniqueName"];
    
    httplib::Headers headers = {
        {"Username", unique_name},
        {"Auto", "0"}
    };
    
    auto res = cli->Post("/giveup", headers, "", "application/json");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);
    
    auto response = json::parse(res->body);
    EXPECT_TRUE(response.contains("Target"));
    EXPECT_TRUE(response.contains("message"));
    EXPECT_EQ(response["hint"], "gaveup");
}

TEST_F(GameServerTest, NewGameWithoutStarting) {
    httplib::Headers headers = {
        {"Username", "nonexistent_player"}
    };
    
    auto res = cli->Post("/newGame", headers, "", "application/json");
    ASSERT_TRUE(res != nullptr);
    
    auto response = json::parse(res->body);
    EXPECT_TRUE(response.contains("error"));
    EXPECT_EQ(response["error"], "Start a game properly first");
}

TEST_F(GameServerTest, ValidNewGame) {
    auto start_res = cli->Get("/start?name=testPlayer");
    auto start_response = json::parse(start_res->body);
    std::string unique_name = start_response["uniqueName"];
    
    httplib::Headers headers = {
        {"Username", unique_name}
    };
    
    auto res = cli->Post("/newGame", headers, "", "application/json");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);
    
    auto response = json::parse(res->body);
    EXPECT_EQ(response["hint"], "new_game");
    EXPECT_EQ(response["message"], "New game, new odds!");
}

TEST_F(GameServerTest, QuitWithoutStarting) {
    httplib::Headers headers = {
        {"Username", "nonexistent_player"},
        {"Auto", "0"}
    };
    
    auto res = cli->Post("/quit", headers, "", "application/json");
    ASSERT_TRUE(res != nullptr);
    
    auto response = json::parse(res->body);
    EXPECT_TRUE(response.contains("error"));
    EXPECT_EQ(response["error"], "Start a game first and then quit later.");
}

TEST_F(GameServerTest, ValidQuit) {
    auto start_res = cli->Get("/start?name=testPlayer");
    auto start_response = json::parse(start_res->body);
    std::string unique_name = start_response["uniqueName"];
    
    httplib::Headers headers = {
        {"Username", unique_name},
        {"Auto", "0"}
    };
    
    auto res = cli->Post("/quit", headers, "", "application/json");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);
    
    auto response = json::parse(res->body);
    EXPECT_TRUE(response.contains("message"));
    EXPECT_TRUE(response.contains("no_score"));
    EXPECT_EQ(response["message"], "Thank you for playing!");
}

TEST_F(GameServerTest, CompleteGameWinFlow) {
    auto start_res = cli->Get("/start?name=testPlayer");
    auto start_response = json::parse(start_res->body);
    std::string unique_name = start_response["uniqueName"];
    
    httplib::Headers headers = {
        {"Username", unique_name},
        {"Auto", "0"}
    };
    
    json guess_request;
    bool found = false;
    for(int i = 1; i <= 100 && !found; i++) {
        guess_request["guess"] = i;
        auto res = cli->Post("/guess", headers, guess_request.dump(), "application/json");
        auto response = json::parse(res->body);
        if(response["hint"] == "correct") {
            found = true;
            EXPECT_EQ(response["message"], "Congratulations! You've found the number!");
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(GameServerTest, AutoModeGame) {
    auto start_res = cli->Get("/start?name=testPlayer");
    auto start_response = json::parse(start_res->body);
    std::string unique_name = start_response["uniqueName"];
    
    httplib::Headers headers = {
        {"Username", unique_name},
        {"Auto", "1"}
    };
    json guess_request;
    guess_request["guess"] = 50;
    auto res = cli->Post("/guess", headers, guess_request.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);
}



int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}