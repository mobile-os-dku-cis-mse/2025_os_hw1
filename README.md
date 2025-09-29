# Project Simple MyShell
This project is a simple shell program (SiSH) implemented in C. It mimics a minimal shell environment for learning operating system concepts.

## Feature
- Display a shell prompt for user input
- Split user input into commands and arguments
- Search for executables in the system PATH
- Execute external commands with process creation (`fork()`, `execve()`)
- print error messages for invalid commands

## Project Structure
```
project-root/
├── Code/ # About directory
│ ├── main.c
│ ├── command_executor.c
│ ├── command_finder.c
│ ├── command_splitter.c
│ ├── error_printer.c
│ ├── prompt_printer.c
│ ├── command_executor.h
│ ├── command_finder.h
│ ├── command_splitter.h
│ ├── error_printer.h
│ ├── prompt_printer.h
│ └── Makefile
├── README.md
├── LICENSE
└── Assignment1 - Project Document (유준혁, 32212808, Department of MSE)
```

## Installation
1. Clone the repository
	```
	git clone [Link]
	```

2. Navigate to the project directory
	```
	cd code
	```

## Usage
1. Build the project
	```
	make
	```

2. Run the shell
	```
	./myshell
	```

3. Enter commands just like a normal shell (e.g., `echo Hello`, `ls`, `pwd`)

4. To exit the shell, type:
	```
	exit
	```
	or
	```
	quit
	```

5. Clean up build files
	```
	make clear
	```
  
## Test
- Enter `echo Hello`, `ls`, `pwd` to check basic command execution
- Enter an invalid command (e.g., `hello`) to test error handling
- Enter multiple arguments (e.g., `ls -l /home`)

## Contributing
1. Fork this repository.

2. Create a new branch.
	```
	git checkout -b feature/YourFeature
	```

3. Commit your changes.
	```
	git commit -m "Add some feature"
	```

4. Push to the branch.
	```
	git push origin feature/YourFeature
	```

5. Open a pull request.

## License
This project is licensed under the MIT License.

## Authors
- Yoo JunHyuk ([@YooJunHyuk123](https://github.com/YooJunHyuk123))
- Email: yjh32212808@dankook.ac.kr