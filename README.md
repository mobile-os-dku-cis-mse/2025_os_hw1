# MiniShell

## Overview

MiniShell is a lightweight implementation of a Unix-like shell. This project is designed to replicate the core functionalities of a shell, including command parsing, execution, signal handling, and support for built-in commands. It is a learning project aimed at understanding the inner workings of operating systems and shell environments.

## Features

- **Command Execution**: Execute external programs and commands using `execvp`.
- **Piping**: Support for command pipelines (e.g., `ls | grep txt`).
- **Built-in Commands**: Includes built-in commands such as `cd` and `quit`.
- **Signal Handling**: Graceful handling of signals like `SIGINT` and `SIGQUIT`.
- **Environment Variables**: Access and manipulate environment variables.
- **Error Handling**: Robust error handling for system calls and memory allocation.

## File Structure

### Source Files

- **`src/main.c`**: Entry point of the shell. Initializes the shell, sets up signal handlers, and manages the main execution loop.
- **`src/commands/commands_handling.c`**: Handles parsing and execution of commands, including support for pipelines and child processes.
- **`src/commands/builtin_commands.c`**: Implements built-in commands like `cd` and `quit`.
- **`src/environment/init.c`**: Initializes the shell environment and manages memory cleanup.
- **`src/signals/signals_handling.c`**: Defines signal handlers for `SIGINT` and `SIGQUIT`.

### Header Files

- **`include/my.h`**: Contains type definitions, function prototypes, and global variables.

### Given Files

- **`given_files/fork.c`**: Demonstrates basic usage of `fork` and `wait`.
- **`given_files/fork2.c`**: Explores advanced `fork` usage with signal handling and timers.
- **`given_files/getenv.c`**: Example of retrieving and parsing environment variables.
- **`given_files/stat.c`**: Demonstrates usage of the `stat` system call.

### Configuration Files

- **`.vscode/settings.json`**: Configures file associations for the development environment.
- **`Makefile`**: Automates the build process for the project.

### License

- **`LICENSE`**: The project is licensed under the MIT License.

## How to Build and Run

1. Clone the repository:
    ```bash
    git clone <repository-url>
    cd 2025_os_hw1
    ```

2. Build the project:
    ```bash
    make
    ```

3. Run the shell:
    ```bash
    ./mysh
    ```

## Usage

- **Run Commands**: Type any valid Unix command and press Enter.
- **Built-in Commands**:
  - `cd <directory>`: Change the current working directory.
  - `quit`: Exit the shell.
- **Pipelines**: Use `|` to chain commands (e.g., `ls | grep txt`).

## Signal Handling

- **`SIGINT`**: Interrupt the current command and display a new prompt.
- **`SIGQUIT`**: Display a quit message and return to the prompt.

## Error Handling

- Displays appropriate error messages for invalid commands, memory allocation failures, and system call errors.

## License

This project is licensed under the MIT License. See the `LICENSE` file for details.

## Acknowledgments

This project was developed as part of an Operating Systems course in 2025. It serves as a practical exercise to deepen understanding of process management, inter-process communication, and shell design.