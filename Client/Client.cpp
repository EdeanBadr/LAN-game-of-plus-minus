#include <iostream>
#include <getopt.h> 
#include <string>
#include <httplib.h>  
#include <nlohmann/json.hpp>  

using json = nlohmann::json;

struct ClientConfig {
    std::string host = "localhost";  
    int port = 4242;  
    std::string name="unkown player"; 
    bool auto_mode = false;  
};
int AutoGuesser(int lower, int upper) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(lower, upper);
    return dis(gen);
}
void parseArguments(int argc, char* argv[], ClientConfig& config) {
    int opt;
    const char* short_opts = "h:p:n:a";  
    const struct option long_opts[] = {
        {"host", required_argument, nullptr, 'h'},  
        {"port", required_argument, nullptr, 'p'},  
        {"name", required_argument, nullptr, 'n'},  
        {"auto", no_argument, nullptr, 'a'},        
        {nullptr, 0, nullptr, 0}  
    };

    while ((opt = getopt_long(argc, argv, short_opts, long_opts, nullptr)) != -1) {
        switch (opt) {
            case 'h':
                config.host = optarg;  
                break;
            case 'p':
                config.port = std::stoi(optarg);  
                break;
            case 'n':
                config.name = optarg;  
                break;
            case 'a':
                config.auto_mode = true;  
                break;
            default:
                std::cerr << "Usage: " << argv[0] << " [-h] [-p port] [-n name] [-a]" << std::endl;
                exit(EXIT_FAILURE);
        }
    }

    if (config.auto_mode) {
        config.name= "computer";  
    }
}

void playGame(const ClientConfig& config) {
    httplib::Client client(config.host, config.port);

    json start_data;
    if (!config.name.empty()) {
        start_data["name"] = config.name;
    }

    auto start_res = client.Post("/start", start_data.dump(), "application/json");

    if (start_res && start_res->status == 200) {
        std::cout << start_res->body << std::endl;
    } else {
        std::cerr << "Failed to start the game!" << std::endl;
        return;
    }

    int lower = 0;
    int upper = 100; 
    int guess = 0;
    std::string hint;

    while (true) {
        if (config.auto_mode) {
            std::cout << "Auto mode: " << std::endl;
            if (hint == "higher") {
                lower = guess + 1;
            } else if (hint == "lower") {
                upper = guess - 1;
            }
            guess = (lower + upper) / 2; 
        }  else {
            std::cout << "Enter your guess (must be integer): ";
            while (!(std::cin >> guess)) {
                std::cin.clear(); 
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); 
                std::cout << "Please enter an integer: ";
            }
        }

        json guess_data;
        guess_data["name"] = config.name;
        guess_data["guess"] = guess;
        auto guess_res = client.Post("/guess", guess_data.dump(), "application/json");

        if (guess_res && guess_res->status == 200) {
            auto response = json::parse(guess_res->body);
            hint = response["hint"];
            std::cout << "Hint: " << hint << std::endl;

            if (hint == "correct") {
                std::cout << "You guessed the number! A new game has started. Try again!" << std::endl;
                
                start_res = client.Post("/start", start_data.dump(), "application/json");
                if (start_res && start_res->status == 200) {
                    std::cout << start_res->body << std::endl;
                } else {
                    std::cerr << "Failed to start a new game!" << std::endl;
                    break;
                }

                lower = 0;
                upper = 100;
                guess = 0;
                hint.clear();
            }
        } else {
            std::cerr << "Error: Unable to send guess!" << std::endl;
            break;
        }
    }
}

int main(int argc, char* argv[]) {
    ClientConfig config;

    parseArguments(argc, argv, config);
    if (config.auto_mode) {
        std::cout << "Auto mode enabled"<<std::endl;;
    } else {
            std::cout << "Player name: " << config.name << "\n";
    }

    playGame(config);

    return 0;
}
