# LAN Game of Plus/Minus
This project is a client-server implementation of a guessing game where the client interacts with a server via HTTP. The client can guess a number provided by the server, and the server provides hints until the correct number is guessed or the game ends.

## Features
- **Manual Mode**: Players manually enter their guesses.
- **Auto Mode**: Automatically guesses the number using a binary search algorithm.
- **Command-Line Configuration**: Customize the host, port, player name, and gameplay mode.
- **JSON Communication**: Utilizes JSON for structured data exchange.
- **Game Management**: Start, quit, and restart games dynamically.

## Prerequisites
1. **C++ Compiler**: Compatible with C++17 or higher.
2. **Dependencies**:
   - [nlohmann/json](https://github.com/nlohmann/json)
   - [cpp-httplib](https://github.com/yhirose/cpp-httplib)
3. **Server**: Ensure the server is running and accessible.

## Installation

1. Clone the repository:
   ```bash
   git clone https://github.com/EdeanBadr/LAN-game-of-plus-minus.git
   cd LAN-game-of-plus-minus
   ```

2. Install dependencies:
   Ensure the `nlohmann/json` and `cpp-httplib` libraries are available and included in your project.

3. Build the project using CMake:
   ```bash
   mkdir build
   cd build
   cmake ..
   make
   ```

## Usage
Run the Server with the following options:

```bash
./server [options]
```

### Options
| Short | Long          | Description                                     |
|-------|---------------|-------------------------------------------------|
| `-p`  | `--port`      | Listening port (default: `4242`).               |
| `-l`  | `--limit`     | Max number of try  (default: `-1`).             |
| `-b`  | `--bounds`    | Bounds for the random number (default: `1,100`).|

### Examples
  ```bash
  ./server -b 1,10 -l 10
  ``` 


Run the client with the following options:

```bash
./client [options]
```

### Options
| Short | Long          | Description                             |
|-------|---------------|-----------------------------------------|
| `-h`  | `--host`      | Server hostname (default: `localhost`). |
| `-p`  | `--port`      | Server port (default: `4242`).          |
| `-n`  | `--name`      | Player name (default: `unknown player`).|
| `-a`  | `--auto`      | Enable auto mode.                       |

### Examples
- **Manual Mode**:
  ```bash
  ./client --host 127.0.0.1 --port 4242 --name badro
  ```

- **Auto Mode**:
  ```bash
  ./client --host 127.0.0.1 --port 4242 --auto
  ```

## Gameplay
1. **Start a Game**:
   The client connects to the server and starts a new game.
2. **Make Guesses**:
   - In manual mode, enter guesses as prompted.
   - In auto mode, the client calculates guesses automatically.
3. **Hints**:
   The server responds with hints (“higher”, “lower”, or “correct”).
4. **Quit**:
   Enter `q` or `Q` to quit the game.
5. **Restart**:
   After finishing a game, you can choose to start a new one.

## Server Interaction
The client communicates with the following server endpoints:
- **`/start`**: Initialize the game.
- **`/newGame`**: Start a new game.
- **`/guess`**: Send a guess to the server.
- **`/giveup`**: Indicate that the player gives up.
- **`/quit`**: End the session.

## Error Handling
- Invalid server responses or network errors are handled informative error messages.
- Input validation ensures that only valid integers are sent as guesses.


## Acknowledgments
- [nlohmann/json](https://github.com/nlohmann/json)
- [cpp-httplib](https://github.com/yhirose/cpp-httplib)

---

Life is a guessing game!
OK COMPUTER!

