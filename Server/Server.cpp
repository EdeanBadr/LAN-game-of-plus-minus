#include "httplib.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <random>
#include <map>
#include <mutex>
#include <string>
#include <getopt.h>

using json = nlohmann::json;
const std::string GAME_HISTORY_FILE = "game_history.json";

std::map<std::string, int> player_targets; 
std::mutex player_mutex;                   


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

int main(int argc, char* argv[]) {
    ServerConfig serverConfig;
    parseArguments(argc, argv, serverConfig);
    std::cout<<serverConfig.lower_bound<<" to "<< serverConfig.upper_bound<<std::endl;

    httplib::Server svr;
    svr.Post("/start", [&](const httplib::Request &req, httplib::Response &res) {
        auto data = json::parse(req.body);
        std::string player_name = data["name"];
        {
            std::lock_guard<std::mutex> lock(player_mutex);
            if (player_targets.find(player_name) == player_targets.end()) {
                player_targets[player_name] = randomNumberGenerator(serverConfig.lower_bound, serverConfig.upper_bound);
                std::cout << "The Game has started for : " << player_name << std::endl;
            }
        }
        //res.set_content("Game started!", "text/plain");
    });

    svr.Post("/guess", [&](const httplib::Request &req, httplib::Response &res) {
        auto data = json::parse(req.body);
        std::string player_name = data["name"];
        int guess = data["guess"];

        json response;
        {
            std::lock_guard<std::mutex> lock(player_mutex);
            if (player_targets.find(player_name) == player_targets.end()) {
                res.status = 400;
                response["error"] = "Player does't exist.";
                res.set_content(response.dump(), "application/json");
                return;
            }
            int target = player_targets[player_name];
            if (guess < target) {
                response["hint"] = "higher";
            } else if (guess > target) {
                response["hint"] = "lower";
          } else {
             response["hint"] = "correct";
             player_targets[player_name] = randomNumberGenerator(serverConfig.lower_bound, serverConfig.upper_bound);
             std::cout << "You guessed correctly. Starting a new game for you!" << std::endl;
        }
        }

        res.set_content(response.dump(), "application/json");
    });

    svr.listen("0.0.0.0", serverConfig.port);
    return 0;
}
