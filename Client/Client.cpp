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


void handleServerResponse(const httplib::Response& res, const std::string& action) {
    if (res.status == 200) {
        try {
            auto response = json::parse(res.body);
            if (response.contains("message")) {
                std::cout << response["message"] << std::endl;
            }
            if (response.contains("top_scores")) {
                std::cout << "Your best scores: ";
                for (const auto& score : response["top_scores"]) {
                    std::cout << score << " ";
                }
                std::cout << std::endl;
            }
        } catch (const json::parse_error& e) {
            std::cerr << "Error parsing " << action << " response: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "Failed to " << action << "!" << std::endl;
        if (res.status != 0) {
            std::cerr << "Server returned status code: " << res.status << std::endl;
            std::cerr << "Response body: " << res.body << std::endl;
        }
    }
}

void playGame(const ClientConfig& config) {
    httplib::Client client(config.host, config.port);
    json start_data;
    if (!config.name.empty()) {
        start_data["name"] = config.name;
    }

    auto start_res = client.Post("/start", start_data.dump(), "application/json");
    if (!start_res || start_res->status != 200) {
        std::cerr << "Failed to start the game!" << std::endl;
        return;
    }
    std::cout << start_res->body << std::endl;

    int lower = 0, upper = 100, guess = 0;
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
            std::cout << "The computer made the guess: " << guess << std::endl;
        } else {
            std::cout << "Enter your guess (must be integer) or 'q||Q' to quit: ";
            std::string input;
            std::cin >> input;

            if (input == "q" || input == "Q") {
                std::cout << "You chose to quit!" << std::endl;
                json quit_data = {{"name", config.name}, {"auto", config.auto_mode}};
                auto quit_res = client.Post("/giveup", quit_data.dump(), "application/json");
                handleServerResponse(*quit_res, "give up");

                std::cout << "Do you want to continue playing? (y/n): ";
                char choice;
                std::cin >> choice;
                if (choice == 'n' || choice == 'N') {
                    json quit_data = {{"name", config.name}, {"auto", config.auto_mode}};
                    auto quit_res = client.Post("/quit", quit_data.dump(), "application/json");
                    handleServerResponse(*quit_res, "quit");
                    break;
                }

                lower = 0;
                upper = 100;
                guess = 0;
                hint.clear();
            } else {
                std::regex int_regex("^-?[0-9]+$");
                if (std::regex_match(input, int_regex)) {
                    guess = std::stoi(input);
                } else {
                    std::cout << "Invalid input. Please enter a valid integer or 'q' to quit." << std::endl;
                    continue;
                }
            }
        }

        json guess_data = {{"name", config.name}, {"guess", guess}, {"auto", config.auto_mode}};
        std::cout << "Player " << guess_data["name"] << " guessed " << guess_data["guess"] << std::endl;
        auto guess_res = client.Post("/guess", guess_data.dump(), "application/json");

        if (!guess_res) {
            std::cerr << "Error: Unable to send guess!" << std::endl;
            break;
        }

        try {
            auto response = json::parse(guess_res->body);
            hint = response["hint"];
            std::cout << "Hint: " << hint << std::endl;

            if (response.contains("message")) {
                std::cout << "Server message: " << response["message"] << std::endl;
            }

            if (hint == "correct") {
                std::cout << "You guessed the number! Do you want to try again? (y/n): ";
                char choice;
                std::cin >> choice;
                if (choice == 'n' || choice == 'N') {
                    json quit_data = {{"name", config.name}, {"auto", config.auto_mode}};
                    auto quit_res = client.Post("/quit", quit_data.dump(), "application/json");
                    handleServerResponse(*quit_res, "quit");
                    break;
                }
                lower = 0;
                upper = 100;
                guess = 0;
                hint.clear();
            } else if (hint == "game_over") {
                if (response.contains("message")) {
                    std::cout << response["message"] << std::endl;
                }
                std::cout << "You lost! Do you want to try again? (y/n): ";
                char choice;
                std::cin >> choice;
                if (choice == 'n' || choice == 'N') {
                    json quit_data = {{"name", config.name}, {"auto", config.auto_mode}};
                    auto quit_res = client.Post("/quit", quit_data.dump(), "application/json");
                    handleServerResponse(*quit_res, "quit");
                    break;
                }
                lower = 0;
                upper = 100;
                guess = 0;
                hint.clear();
            }
        } catch (const json::parse_error& e) {
            std::cerr << "Error parsing server response: " << e.what() << std::endl;
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