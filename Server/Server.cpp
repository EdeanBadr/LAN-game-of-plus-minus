#include "httplib.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <random>
#include <map>
#include <mutex>
#include <string>
#include <getopt.h>
#include <set>
#include <vector>
#include <fstream>

using json = nlohmann::json;
const std::string GAME_HISTORY_FILE = "../game_history.json";

std::map<std::string, int> player_targets;
std::map<std::string, int> player_guesses_count;
std::mutex player_mutex;
std::map<std::string, std::multiset<int>> player_score_history;
std::mutex score_mutex;
std::mutex file_mutex;

auto getTopScores(const std::string& player_name) {
    std::lock_guard<std::mutex> lock(score_mutex);
    auto& history = player_score_history[player_name];

    std::vector<int> top_scores;
    auto it = history.begin();
    for (int i = 0; i < 5 && it != history.end(); ++i, ++it) {
        top_scores.push_back(*it);
    }
    return top_scores;
}

void updatePlayerScores(const std::string& player_name, int score) {
    std::lock_guard<std::mutex> lock(score_mutex);
    player_score_history[player_name].insert(score);
}

struct Gamestats {
    std::string playerName = "";
    std::string startTime = "";
    std::string endTime = "";
    int triesCount;
    std::string gameState = "";
};

void appendGameStats(const Gamestats& gamestats) {
    std::lock_guard<std::mutex> lock(file_mutex);
    std::ifstream infile(GAME_HISTORY_FILE);
    json game_history;

    if (infile.is_open()) {
        infile >> game_history;
    } else {
        game_history = json::array();
    }
    infile.close();

    json game_entry;
    game_entry["player_name"] = gamestats.playerName;
    game_entry["start_time"] = gamestats.startTime;
    game_entry["end_time"] = gamestats.endTime;
    game_entry["tries_count"] = gamestats.triesCount;
    game_entry["game_state"] = gamestats.gameState;

    game_history.push_back(game_entry);

    std::ofstream outfile(GAME_HISTORY_FILE);
    if (!outfile.is_open()) {
        std::cerr << "Error opening file for writing." << std::endl;
        return;
    }

    outfile << game_history.dump(4);
    outfile.close();
}

std::string getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);

    struct tm time_info;

#ifdef _WIN32
    localtime_s(&time_info, &now_time_t);
#else
    localtime_r(&now_time_t, &time_info);
#endif

    std::ostringstream oss;
    oss << std::put_time(&time_info, "%d/%m/%y %H:%M:%S");

    return oss.str();
}

struct ServerConfig {
    int port = 4242;
    std::string bounds = "1,100";
    int lower_bound = 1;
    int upper_bound = 100;
    int limit = -1;
};

void extractBounds(const std::string& bounds_str, int& lower_bound, int& upper_bound) {
    std::stringstream strstream(bounds_str);
    std::string bound;
    std::getline(strstream, bound, ',');
    lower_bound = std::stoi(bound);
    std::getline(strstream, bound, ',');
    upper_bound = std::stoi(bound);
}

void parseArguments(int argc, char* argv[], ServerConfig& config) {
    int opt;
    const char* short_opts = "p:l:b:";
    const struct option long_opts[] = {
        {"port", required_argument, nullptr, 'p'},
        {"limit", required_argument, nullptr, 'l'},
        {"bounds", required_argument, nullptr, 'b'},
        {nullptr, 0, nullptr, 0}
    };

    while ((opt = getopt_long(argc, argv, short_opts, long_opts, nullptr)) != -1) {
        switch (opt) {
            case 'p':
                config.port = std::stoi(optarg);
                break;
            case 'l':
                config.limit = std::stoi(optarg);
                break;
            case 'b':
                config.bounds = optarg;
                extractBounds(config.bounds, config.lower_bound, config.upper_bound);
                break;
            default:
                std::cerr << "Usage: " << argv[0] << " -p port  -l limit] -b bounds" << std::endl;
                exit(EXIT_FAILURE);
        }
    }
}

auto randomNumberGenerator(int lower, int upper) {
    std::random_device randev;
    std::mt19937 rng(randev());
    std::uniform_int_distribution<> dis(lower, upper);
    return dis(rng);
}

void startGameHandler(const httplib::Request &req, httplib::Response &res, ServerConfig& serverConfig, Gamestats& gamestats) {
    auto data = json::parse(req.body);
    std::string player_name = data["name"];
    {
        std::lock_guard<std::mutex> lock(player_mutex);
        if (player_targets.find(player_name) == player_targets.end()) {
            player_targets[player_name] = randomNumberGenerator(serverConfig.lower_bound, serverConfig.upper_bound);
            player_guesses_count[player_name] = 0; 
            std::cout << "The Game has started for: " << player_name << std::endl;
            gamestats.startTime = getCurrentTime();
        }
    }
    res.set_content("Game started!", "text/plain");
}
void guessHandler(const httplib::Request &req, httplib::Response &res, ServerConfig& serverConfig, Gamestats& gamestats) {
    json response;
    try {
        auto data = json::parse(req.body);
        std::string player_name = data["name"];
        gamestats.playerName = player_name;
        int guess = data["guess"];
        std::cout<<"the client guessed"<<guess<<std::endl;

        std::lock_guard<std::mutex> lock(player_mutex);
        if (player_targets.find(player_name) == player_targets.end()) {
            response["hint"] = "error";
            response["message"] = "Player doesn't exist.";
            res.status = 400;
            res.set_content(response.dump(), "application/json");
            return;
        }

        player_guesses_count[player_name]++;
        int target = player_targets[player_name];

        if (serverConfig.limit > 0 && player_guesses_count[player_name] > serverConfig.limit) {
            response["hint"] = "game_over";
            response["message"] = "You have reached the maximum number of tries!";
            response["target"] = target;  
            res.status = 200;
            res.set_content(response.dump(), "application/json");
            
            gamestats.endTime = getCurrentTime();
            gamestats.triesCount = player_guesses_count[player_name];
            gamestats.gameState = "LOST";
            appendGameStats(gamestats);
            player_targets[player_name] = randomNumberGenerator(serverConfig.lower_bound, serverConfig.upper_bound);
            player_guesses_count[player_name] = 0;
            gamestats.startTime = getCurrentTime();
            return;
        }

        if (guess < target) {
            std::cout<<guess<<" is lower"<<std::endl;
            response["hint"] = "higher";
            response["message"] = "Try a higher number";
        } else if (guess > target) {
            std::cout<<guess<<" is higher"<<std::endl;
            response["hint"] = "lower";
            response["message"] = "Try a lower number";
        } else {
                response["hint"] = "correct";
                response["message"] = "Congratulations! You've found the number!";
                
                gamestats.endTime = getCurrentTime();
                gamestats.triesCount = player_guesses_count[player_name];
                gamestats.gameState = "WON";
                int current_score = player_guesses_count[player_name];
                
                try {
                    updatePlayerScores(player_name, current_score);
                } catch (const std::exception& e) {
                    std::cerr << "Error updating player scores: " << e.what() << std::endl;
                }

                player_targets[player_name] = randomNumberGenerator(serverConfig.lower_bound, serverConfig.upper_bound);
                player_guesses_count[player_name] = 0;

                try {
                    appendGameStats(gamestats);
                } catch (const std::exception& e) {
                    std::cerr << "Error appending game stats: " << e.what() << std::endl;
                }

                gamestats.startTime = getCurrentTime();
            }

        res.status = 200;
        res.set_content(response.dump(), "application/json");

    } catch (const json::parse_error& e) {
        response["hint"] = "error";
        response["message"] = "Invalid request format";
        res.status = 400;
        res.set_content(response.dump(), "application/json");
    } catch (const std::exception& e) {
        response["hint"] = "error";
        response["message"] = "Internal server error";
        res.status = 500;
        res.set_content(response.dump(), "application/json");
    }
}

void quitHandler(const httplib::Request &req, httplib::Response &res, Gamestats &gamestats) {
    try {
        auto data = json::parse(req.body);
        std::string player_name = data["name"];

        json response;
        {
            std::lock_guard<std::mutex> lock(player_mutex);

            if (player_targets.find(player_name) == player_targets.end()) {
                res.status = 400;
                response["error"] = "Player does not exist.";
                res.set_content(response.dump(), "application/json");
                return;
            }

            try {
                auto top_scores = getTopScores(player_name);
                response["message"] = "Thank you for playing!";
                response["top_scores"] = top_scores;
            } catch (const std::exception &e) {
                res.status = 500;
                response["error"] = "Failed to retrieve top scores.";
                response["details"] = e.what();
                res.set_content(response.dump(), "application/json");
                return;
            }

            player_targets.erase(player_name);
            player_guesses_count.erase(player_name);

            {
                std::lock_guard<std::mutex> score_lock(score_mutex);
                player_score_history.erase(player_name);
            }

            std::cout << "Player " << player_name << " has quit the game." << std::endl;
        }

        res.set_content(response.dump(), "application/json");

    } catch (const json::exception &e) {
        res.status = 400;
        json response = {
            {"error", "Invalid JSON format."},
            {"details", e.what()}
        };
        res.set_content(response.dump(), "application/json");

    } catch (const std::exception &e) {
        res.status = 500;
        json response = {
            {"error", "An unexpected error occurred."},
            {"details", e.what()}
        };
        res.set_content(response.dump(), "application/json");
    }
}

void giveUpHandler(const httplib::Request &req, httplib::Response &res, Gamestats &gamestats) {
    try {
        auto data = json::parse(req.body);
        std::string player_name = data["name"];

        json response;
        {
            std::lock_guard<std::mutex> lock(player_mutex);

            if (player_targets.find(player_name) == player_targets.end()) {
                res.status = 400;
                response["error"] = "Player does not exist.";
                res.set_content(response.dump(), "application/json");
                return;
            }

            int target = player_targets[player_name];

            response["message"] = "Never Give Up Again!";
            response["Target"] = target;

            gamestats.endTime = getCurrentTime();
            gamestats.triesCount = player_guesses_count[player_name];
            gamestats.gameState = "gave up";
            gamestats.startTime = getCurrentTime();

            try {
                appendGameStats(gamestats);
            } catch (const std::exception &e) {
                res.status = 500;
                response["error"] = "Failed to append game stats.";
                response["details"] = e.what();
                res.set_content(response.dump(), "application/json");
                return;
            }

            std::cout << "Player " << player_name << " has given up!" << std::endl;
        }

        res.set_content(response.dump(), "application/json");

    } catch (const json::exception &e) {
        res.status = 400;
        json response = {
            {"error", "Invalid JSON format."},
            {"details", e.what()}
        };
        res.set_content(response.dump(), "application/json");

    } catch (const std::exception &e) {
        res.status = 500;
        json response = {
            {"error", "An unexpected error occurred."},
            {"details", e.what()}
        };
        res.set_content(response.dump(), "application/json");
    }
}



int main(int argc, char* argv[]) {
    ServerConfig serverConfig;
    Gamestats gamestats;
    parseArguments(argc, argv, serverConfig);
    std::cout << serverConfig.lower_bound << " to " << serverConfig.upper_bound << std::endl;

    httplib::Server svr;
    
    svr.Post("/start", [&](const httplib::Request &req, httplib::Response &res) {
        startGameHandler(req, res, serverConfig, gamestats);
    });

    svr.Post("/guess", [&](const httplib::Request &req, httplib::Response &res) {
        guessHandler(req, res, serverConfig, gamestats);
    });

    svr.Post("/quit", [&](const httplib::Request &req, httplib::Response &res) {
        quitHandler(req, res, gamestats);
    });
    svr.Post("/giveup", [&](const httplib::Request &req, httplib::Response &res) {
        giveUpHandler(req, res, gamestats);
    });

    svr.listen("0.0.0.0", serverConfig.port);
}