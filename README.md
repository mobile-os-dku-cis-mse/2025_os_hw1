 SiSH - Simple MyShell

**SiSH (Simple Shell)** is a lightweight custom shell program developed for the **Operating Systems & Advanced Mobile** course at Dankook University. It enables users to execute commands, run programs, and interact with the operating system in an intuitive way.



Features

- Execute standard system commands: `ls`, `pwd`, `echo`, `whoami`, `date`
- Support for command arguments (e.g., `ls -l /home`)
- Execute commands using absolute paths (e.g., `/bin/ls`)
- Custom prompt showing username: `javok@SiSH>`
- Graceful handling of invalid commands with clear error messages


 How to Compile and Run
 Compile using the Makefile:
```bash
make

Run the shell:
./sish


Exit the shell:
Type quit at the prompt

Example Usage
javok@SiSH> ls
sish.c  sish.exe

javok@SiSH> pwd
/home/javok/os_hw_1

javok@SiSH> echo This is my shell test
This is my shell test

javok@SiSH> whoami
javok

javok@SiSH> date
Tue Sep 30 15:40:40 KST 2025

javok@SiSH> quit
Exiting SiSH...



Project Structure
os_hw_1/
├─ sish.c        # Main source code
├─ Makefile      # Build instructions
├─ README.md     # Project overview
├─ .gitignore    # Ignore compiled files (*.exe, *.o)





Author
Javokhir Khalikov
Dankook University, 2025


This design uses:

- Bold for key terms  
- Horizontal lines (`---`)** for separation  
- Bullet points** for features  
- Code blocks** for commands and example outputs  






