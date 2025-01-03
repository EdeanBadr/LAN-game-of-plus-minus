#include "httplib.h"
#include "nlohmann/json.hpp"
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

std::map<std::string, int> player_targets;
std::map<std::string, int> player_guesses_count;
std::mutex player_mutex;

std::map<std::string, std::multiset<int>> player_score_history;
std::mutex score_mutex;

std::ofstream outfile; 
const std::string GAME_HISTORY_FILE = "../game_history.json";
std::mutex file_mutex;

bool openFile() {
    outfile.open(GAME_HISTORY_FILE, std::ios::app);  
    if (!outfile.is_open()) {
        std::cerr << "Error opening file for appending." << std::endl;
        return false;
    }
    std::cout << "File opened for appending." << std::endl;  
    return true;
}

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

/**void appendGameStats(const Gamestats& gamestats) {
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
}**/
void appendGameStats(const Gamestats& gamestats) {
    std::lock_guard<std::mutex> lock(file_mutex);

    if (!outfile.is_open()) {
        std::cerr << "File is not open. Cannot write game stats." << std::endl;
        return;
    }

    json game_entry;
    game_entry["player_name"] = gamestats.playerName;
    game_entry["start_time"] = gamestats.startTime;
    game_entry["end_time"] = gamestats.endTime;
    game_entry["tries_count"] = gamestats.triesCount;
    game_entry["game_state"] = gamestats.gameState;

    outfile << game_entry.dump(4) << "\n";
    outfile.flush(); 
    std::cout << "Appended game stats to file." << std::endl;  // Debug message
}
std::string getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);

    struct tm time_info;

    localtime_r(&now_time_t, &time_info);

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
    static std::random_device randev;
    static std::mt19937 rng(randev()); 
    std::uniform_int_distribution<> dis(lower, upper);
    return dis(rng);
}
std::string generateUniqueName(const std::string& player_name) {

    int random_number = randomNumberGenerator(100000, 999999);
    return player_name + "_" + std::to_string(random_number);  
}

void startGameHandler(const httplib::Request &req, httplib::Response &res, ServerConfig& serverConfig, Gamestats& gamestats) {
    std::lock_guard<std::mutex> lock(player_mutex);
    try {
        if (!req.has_param("name")) {
            res.status = 400;
            res.set_content("Invalid request: the name parameter is required!", "text/plain");
            return;
        }
        std::string player_name = req.get_param_value("name");
        std::string unique_name = generateUniqueName(player_name);

        player_targets[unique_name] = randomNumberGenerator(serverConfig.lower_bound, serverConfig.upper_bound);
        player_guesses_count[unique_name] = 0; 
        gamestats.startTime = getCurrentTime();
        json response;
        response["uniqueName"] = unique_name;
        response["message"] = "The Game started! Your number is between "+std::to_string(serverConfig.lower_bound) + 
                                " and " + 
                                std::to_string(serverConfig.upper_bound);
        response["lowerbound"] = serverConfig.lower_bound;
        response["upperbound"] = serverConfig.upper_bound;
        res.set_content(response.dump(), "application/json");
        std::cout << "The Game has started for: " << unique_name << std::endl;
    } catch (const json::parse_error& e) {
        res.status = 400;
        res.set_content(std::string("Error: ") + e.what(), "text/plain");
    } catch (const std::exception &e) {
        res.status = 500; 
        res.set_content(std::string("Error: ") + e.what(), "text/plain");
    }
}
bool isValidGuess(int guess, const ServerConfig& config) {
    return guess >= config.lower_bound && guess <= config.upper_bound;
}
void guessHandler(const httplib::Request &req, httplib::Response &res, ServerConfig& serverConfig, Gamestats& gamestats) {
    json response;
    try {
        auto data = json::parse(req.body);
        std::string player_name = req.get_header_value("Username");
        auto  auto_str = req.get_header_value("Auto");
        bool auto_mode = (auto_str == "1");
        gamestats.playerName = player_name;
        int guess = data["guess"];
        if (player_targets.find(player_name) == player_targets.end()) {
                res.status = 400;
                response["error"] = "You cant guess without starting a game first.";
                res.set_content(response.dump(), "application/json");
                return;
            }
        if (!isValidGuess(guess, serverConfig)) {
            response["hint"] = "invalid";
            response["message"] = "Invalid guess: Must be between " + 
                                std::to_string(serverConfig.lower_bound) + 
                                " and " + 
                                std::to_string(serverConfig.upper_bound);
            res.set_content(response.dump(), "application/json");
            return;
        }

        player_guesses_count[player_name]++;
        int target = player_targets[player_name];

        if (serverConfig.limit > 0 && player_guesses_count[player_name] >= serverConfig.limit) {
            response["hint"] = "game_over";
            response["message"] = "You have reached the maximum number of tries!";
            response["Target"] = target;  
            res.set_content(response.dump(), "application/json");
            gamestats.endTime = getCurrentTime();
            gamestats.triesCount = player_guesses_count[player_name];
            gamestats.gameState = "LOST";
            if(!auto_mode){
            appendGameStats(gamestats);
            }
            player_guesses_count[player_name] = 0;
            return;
        }
        if (guess < target) {
            response["hint"] = "higher";
            response["message"] = "Bro Try a higher number";
        } else if (guess > target) {
            response["hint"] = "lower";
            response["message"] = "Bro Try a lower number";
        } else {
                response["hint"] = "correct";
                response["message"] = "Congratulations! You've found the number!";
                gamestats.endTime = getCurrentTime();
                gamestats.triesCount = player_guesses_count[player_name];
                gamestats.gameState = "WON";
                int current_score = player_guesses_count[player_name];
                if(!auto_mode){
                try {
                    updatePlayerScores(player_name, current_score);
                } catch (const std::exception& e) {
                    std::cerr << "Error updating player scores: " << e.what() << std::endl;
                }
                try {
                    appendGameStats(gamestats);
                } catch (const std::exception& e) {
                     std::cerr << "Error saving the game stats" << e.what() << std::endl;
                }
                }
            }

        res.status = 200;
        res.set_content(response.dump(), "application/json");

    } catch (const json::parse_error& e) {
        res.status = 400;
        res.set_content(std::string("Error: ") + e.what(), "text/plain");
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(std::string("Error: ") + e.what(), "text/plain");
    }
}

void quitHandler(const httplib::Request &req, httplib::Response &res, Gamestats &gamestats) {
    try {
        std::string player_name = req.get_header_value("Username");
        auto  auto_str = req.get_header_value("Auto");
        bool auto_mode = (auto_str == "1");
        json response;
        {
            std::lock_guard<std::mutex> lock(player_mutex);

            if (player_targets.find(player_name) == player_targets.end()) {
                res.status = 400;
                response["error"] = "Start a game first and then quit later.";
                res.set_content(response.dump(), "application/json");
                return;
            }

            try {
                response["message"] = "Thank you for playing!";
                auto& history = player_score_history[player_name];

                if (!auto_mode) {
                    if (history.empty()) {
                    response["no_score"] = "You don't even win once, what are you waiting for?";
                     } else {
                    auto top_scores = getTopScores(player_name);
                    response["top_scores"] = top_scores;
                   }
               
                }
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
        res.set_content(std::string("Error: ") + e.what(), "text/plain");

    } catch (const std::exception &e) {
        res.status = 500;
        res.set_content(std::string("Error: ") + e.what(), "text/plain");
    }
}

void giveUpHandler(const httplib::Request &req, httplib::Response &res, ServerConfig& serverConfig, Gamestats &gamestats) {
    try {
        std::string player_name = req.get_header_value("Username");
        auto  auto_str = req.get_header_value("Auto");
        bool auto_mode = (auto_str == "1");

        json response;
        {
            std::lock_guard<std::mutex> lock(player_mutex);

            if (player_targets.find(player_name) == player_targets.end()) {
                res.status = 400;
                response["error"] = "You haven't even started a game properly and you trying to give up?";
                res.set_content(response.dump(), "application/json");
                return;
            }

            int target = player_targets[player_name];
            response["message"] = "Never Give Up Again!";
            response["Target"] = target;
            response["hint"] = "gaveup";
            gamestats.endTime = getCurrentTime();
            gamestats.triesCount = player_guesses_count[player_name];
            gamestats.gameState = "gave up";
            try {
                if (!auto_mode) appendGameStats(gamestats);
            } catch (const std::exception &e) {
                std::cerr << "Error saving the game stats" << e.what() << std::endl;
                return;
            }

            std::cout << "Player " << player_name << " has given up!" << std::endl;
        }

        res.set_content(response.dump(), "application/json");

    } catch (const json::exception &e) {
        res.status = 400;
        res.set_content(std::string("Error: ") + e.what(), "text/plain");
    } catch (const std::exception &e) {
        res.status = 500;
        res.set_content(std::string("Error: ") + e.what(), "text/plain");
    }
}

void newGameHandler(const httplib::Request &req, httplib::Response &res, ServerConfig& serverConfig, Gamestats& gamestats) {
    try{
        std::string player_name = req.get_header_value("Username");
        json response;
        std::lock_guard<std::mutex> lock(player_mutex);
        if (player_targets.find(player_name) == player_targets.end()) {
            response["hint"] = "error";
            response["error"] = "Start a game properly first";
            res.status = 400;
            res.set_content(response.dump(), "application/json");
            return;
    }

    player_targets[player_name] = randomNumberGenerator(serverConfig.lower_bound, serverConfig.upper_bound);
    player_guesses_count[player_name] = 0;
    gamestats.startTime = getCurrentTime();
    response["hint"] = "new_game";
    response["message"] = "New game, new odds!";
    res.set_content(response.dump(), "application/json");
    }catch (const json::exception &e) {
        res.status = 400;
        res.set_content(std::string("Error: ") + e.what(), "text/plain");
    } catch (const std::exception &e) {
        res.status = 500;
        res.set_content(std::string("Error: ") + e.what(), "text/plain");
    }
}

int main(int argc, char* argv[]) {
    ServerConfig serverConfig;
    Gamestats gamestats;
    parseArguments(argc, argv, serverConfig);
    httplib::Server svr;
    auto threads_num = std::thread::hardware_concurrency();
    svr.new_task_queue = [threads_num] { 
        return new httplib::ThreadPool(threads_num); 
    };
    if (!openFile()) {
        return -1;
    }

    svr.Get("/start", [&](const httplib::Request &req, httplib::Response &res) {
        startGameHandler(req, res, serverConfig, gamestats);
    });

    svr.Post("/guess", [&](const httplib::Request &req, httplib::Response &res) {
        guessHandler(req, res, serverConfig, gamestats);
    });

    svr.Post("/newGame", [&](const httplib::Request &req, httplib::Response &res) {
        newGameHandler(req, res, serverConfig, gamestats);
    });

    svr.Post("/quit", [&](const httplib::Request &req, httplib::Response &res) {
        quitHandler(req, res, gamestats);
    });

    svr.Post("/giveup", [&](const httplib::Request &req, httplib::Response &res) {
        giveUpHandler(req, res, serverConfig, gamestats);
    });

    svr.listen("0.0.0.0", serverConfig.port);
    outfile.close();
}