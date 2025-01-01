#include <iostream>
#include <getopt.h> 
#include <string>
#include <httplib.h>  
#include <nlohmann/json.hpp>  

using json = nlohmann::json;
httplib::Headers headers;

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

class ClientGame {
public:
    ClientGame(const ClientConfig& cfg, httplib::Client& cli) 
        : config(cfg), client(cli) {
        instance = this;
        setupSignalHandlers();
    }

    ~ClientGame() {
        instance = nullptr;
        cleanup();
    }

   void playGame() {
    try {
        start();
        int guess = 0;
        int lower=lowerbound;
        int upper=upperbound;        
        while (true) {
            if (config.auto_mode) {
                std::cout << "Auto mode: " << std::endl;
                if (hint == "higher") lower = guess + 1;
                else if (hint == "lower") upper = guess - 1;
                guess = (lower + upper) / 2;
                std::cout << "The computer guessed: " << guess << std::endl;
            } else {
                std::cout << "Enter your guess (integer) or 'q/Q' to quit: ";
                std::string input;
                std::cin >> input;
                if (input == "q" || input == "Q") {
                    std::cout << "You chose to give up!" << std::endl;
                    auto giveup_res = client.Post("/giveup", headers, "", "application/json");
                    handleServerResponse(giveup_res, "give up");  
                    auto response = json::parse(giveup_res->body);
                    hint = response["hint"];
                    std::cout << "Do you want to try again? ('n/N' to quit): ";
                    char choice;
                    std::cin >> choice;
                    if (choice == 'n' || choice == 'N') {
                        break;
                    }
                    startNewGame(lower, upper, guess, hint);
                    continue;
                }

                if (std::regex_match(input, std::regex("^-?[0-9]+$"))) {
                    guess = std::stoi(input);
                } else {
                    std::cout << "Invalid input. Enter an integer or 'q/Q' to quit." << std::endl;
                    continue;
                }
            }

            json guess_data = {{"guess", guess}};
            auto guess_res = client.Post("/guess", headers, guess_data.dump(), "application/json");
            handleServerResponse(guess_res, "guess");
            try {
                auto response = json::parse(guess_res->body);
                hint = response.value("hint", "");
                if (hint == "correct" || hint == "game_over") {
                   std::cout << "Do you want to try again? ('n/N' to quit):";
                    char choice;
                    std::cin >> choice;
                    if (choice == 'n' || choice == 'N') {
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::seconds(1));

                    startNewGame(lower, upper, guess, hint);
                }
            } catch (const json::parse_error& e) {
                std::cerr << "Error parsing server response: " << e.what() << std::endl;
                break;
            }
        }
    } catch (const std::runtime_error& e) {
        std::cerr << "Game aborted: " << e.what() << std::endl;
    }
}
void handleServerResponse(const httplib::Result& res, const std::string& action) {
        if (!res) {
            throw std::runtime_error("Server connection failed");
        }
        if (res->status == 200) {
            try {
                auto response = json::parse(res->body);
                if (response.contains("message")) {
                    std::cout << response["message"].get<std::string>()  << std::endl;
                }
                if (response.contains("top_scores")) {
                    std::cout << "Your best scores are ..."<<std::endl;
                    for (const auto& score : response["top_scores"]) {
                        std::cout << score << " ";
                    }
                    std::cout << std::endl;
                }
                if (response.contains("no_score")){
                    std::cout << "Your best scores are ..." << response["no_score"].get<std::string>()<< std::endl;
                }
                if (response.contains("Target")){
                    std::cout << "The value you couldn't guess is: " << (int)response["Target"]<< std::endl;
                }
            } catch (const json::parse_error& e) {
                std::cerr << "Error parsing " << action << " response: " << e.what() << std::endl;
            }
        } else {
            std::cerr << "Failed to " << action << "!" << std::endl;
            if (res->status != 0) {
                auto response = json::parse(res->body);
                throw std::runtime_error(response["error"].get<std::string>());
            }
        }
    }
     static void signalHandler(int signum) {
        if (instance) {
            instance->cleanup();
        }
        signal(signum, SIG_DFL);
        raise(signum);
    }
    void setupSignalHandlers() {
            struct sigaction sig;
            sig.sa_handler = signalHandler;
            sigemptyset(&sig.sa_mask);
            sig.sa_flags = SA_RESETHAND; 
            sigaction(SIGTERM, &sig, nullptr);  
            sigaction(SIGINT, &sig, nullptr);   
            sigaction(SIGHUP, &sig, nullptr);   
            sigaction(SIGQUIT, &sig, nullptr);  
            sigaction(SIGABRT, &sig, nullptr);  }

    void cleanup() {
            try {
                if (hint=="higher" || hint=="lower" ||hint=="invalid"|| hint.empty()) {
                 auto giveup_res = client.Post("/giveup",headers,"", "application/json");
                 handleServerResponse(giveup_res, "giveup");
                }
                auto quit_res = client.Post("/quit",headers, "", "application/json");
                handleServerResponse(quit_res, "quit");
            } catch (const std::exception& e) {
            }
        }

    void startNewGame(int& lower, int& upper, int& guess, std::string& hint) {
        auto new_game_res = client.Post("/newGame", headers,"", "application/json");
        handleServerResponse(new_game_res, "start a new game");
        guess = 0;
        lower=lowerbound;
        upper=upperbound;
        hint.clear();
    }

    void start() {
        std::string start_params = "?name=" + config.name;
        auto start_res = client.Get(("/start" + start_params));
        handleServerResponse(start_res, "start");
        auto response = json::parse(start_res->body);
        config.name = response["uniqueName"];
        headers = {
        {"Username", config.name},         
        {"Auto", std::to_string(config.auto_mode)}  };
        upperbound=response["upperbound"].get<int>();
        lowerbound=response["lowerbound"].get<int>();
        std::cout << "May the odds be ever in your favor " << config.name << "!" << std::endl;
    }

private:
    ClientConfig config;
    httplib::Client& client;
    static ClientGame* instance;
    std::string hint;
    httplib::Headers headers = {{"Username", config.name},
                        {"Auto", std::to_string(config.auto_mode)}
                    };
    int lowerbound;
    int upperbound;
};

ClientGame* ClientGame::instance = nullptr;
int main(int argc, char* argv[]) {
    try {
        ClientConfig config;
        parseArguments(argc, argv, config);
        httplib::Client client(config.host, config.port);
        if (config.auto_mode) {
            std::cout << "Auto mode enabled" << std::endl;
        } else {
            std::cout << "Player name: " << config.name << "\n";
        }
        
        ClientGame game(config, client);
        game.playGame();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}