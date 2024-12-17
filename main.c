#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <signal.h>
#include <process.h>
#include <ctype.h>
#include <time.h>
#include <dirent.h>
#include <tlhelp32.h>

#define MAX_CMD_LEN 1024
#define MAX_ARGS 64

// Danh sách tiến trình (giả lập cho quản lý tiến trình)
typedef struct {
    DWORD pid;
    char command[MAX_CMD_LEN];
    int is_running;
    time_t start_time;
    HANDLE hProcess;
} Process;

Process process_list[100];
int process_count = 0;

// Prototype các hàm
void shell_loop();
void execute_command(char *cmd);
void handle_foreground(char *cmd, char **args);
void handle_background(char *cmd, char **args);
void list_processes();
void kill_process(DWORD pid);
void stop_process(DWORD pid);
void resume_process(DWORD pid);
void handle_builtin_commands(char **args);
void handle_path_commands(char **args);
void handle_signals(int sig);
void execute_batch_file(const char *filename);
void top_process();
void print_date();
void print_time();
void list_dir();
int is_process_alive(DWORD pid);

// Hàm xử lý tín hiệu ngắt (Ctrl + C)
void handle_signals(int sig) {
    if (sig == SIGINT) {
        printf("\nSignal SIGINT received. Type 'exit' to quit.\n");
    }
}

// Hàm hiển thị vòng lặp nhận lệnh
void shell_loop() {
    char cmd[MAX_CMD_LEN];
    char *args[MAX_ARGS];

    while (1) {
        // Hiển thị prompt
        printf("myShell> ");
        fflush(stdout);

        // Đọc lệnh từ người dùng
        if (!fgets(cmd, MAX_CMD_LEN, stdin)) break;
        cmd[strcspn(cmd, "\n")] = 0; // Xóa ký tự '\n'

        // Nếu không nhập gì, tiếp tục
        if (strlen(cmd) == 0) continue;

        // Tách lệnh thành các từ
        int i = 0;
        args[i] = strtok(cmd, " ");
        while (args[i] != NULL) {
            i++;
            args[i] = strtok(NULL, " ");
        }

        // Kiểm tra lệnh đặc biệt (exit, help, ...)
        if (strcmp(args[0], "exit") == 0) break;
        //Biểu thị các chức năng
        if (strcmp(args[0], "help") == 0) {
            printf("\n=== Supported Commands ===\n");
            printf("exit            : Exit the shell.\n");
            printf("help            : Display this help message.\n");
            printf("list            : List all background processes (PID, Command, Status).\n");
            printf("kill <PID>      : Terminate a process with the given PID.\n");
            printf("stop <PID>      : Suspend a process with the given PID.\n");
            printf("resume <PID>    : Resume a suspended process with the given PID.\n");
            printf("top             : Display the list of processes (similar to 'list').\n");
            printf("date            : Show the current date .\n");
            printf("time            : Show the current time .\n");
            printf("dir             : List all files and directories in the current directory.\n");
            printf("<command> &     : Run a command in the background (e.g., notepad.exe &).\n");
            printf("<command>       : Run a command in the foreground (e.g., notepad.exe).\n");
            printf("=========================\n\n");
    continue;
}

            continue;
        } else if (strcmp(args[0], "date") == 0) {
            print_date();
            continue;
        } else if (strcmp(args[0], "time") == 0) {
            print_time();
            continue;
        } else if (strcmp(args[0], "dir") == 0) {
            list_dir();
            continue;
        }

        // Kiểm tra chạy nền (lệnh kết thúc bằng '&')
        int is_background = (args[i - 1][0] == '&');
        if (is_background) args[--i] = NULL; // Xóa '&' khỏi lệnh

        // Thực thi lệnh
        if (strcmp(args[0], "list") == 0) {
            list_processes();
        } else if (strcmp(args[0], "kill") == 0 && i > 1) {
            DWORD pid = atoi(args[1]);
            kill_process(pid);
        } else if (strcmp(args[0], "top") == 0) {
            top_process();
        } else if (strcmp(args[0], "stop") == 0 && i > 1) {
            DWORD pid = atoi(args[1]);
            stop_process(pid);
        } else if (strcmp(args[0], "resume") == 0 && i > 1) {
            DWORD pid = atoi(args[1]);
            resume_process(pid);
        } else if (is_background) {
            handle_background(cmd, args);
        } else {
            handle_foreground(cmd, args);
        }
    }
}

// Hàm thực thi lệnh foreground
void handle_foreground(char *cmd, char **args) {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);

    if (!CreateProcess(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        printf("CreateProcess failed (%d).\n", GetLastError());
        return;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

// Hàm thực thi lệnh background
void handle_background(char *cmd, char **args) {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);

    if (!CreateProcess(NULL, cmd, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
        printf("CreateProcess failed (%d).\n", GetLastError());
        return;
    }

    process_list[process_count].pid = pi.dwProcessId;
    strcpy(process_list[process_count].command, cmd);
    process_list[process_count].is_running = 1;
    process_list[process_count].start_time = time(NULL);
    process_list[process_count].hProcess = pi.hProcess;
    process_count++;

    printf("Started background process [%d]: %s\n", pi.dwProcessId, cmd);

    CloseHandle(pi.hThread);
}

// Hàm liệt kê tiến trình (chỉ hiển thị PID, Command Name, và Status)
void list_processes() {
    printf("PID\tCOMMAND\t\tSTATUS\n");
    for (int i = 0; i < process_count; i++) {
        if (is_process_alive(process_list[i].pid)) {
            printf("%d\t%s\t\t%s\n", process_list[i].pid, process_list[i].command,
                   process_list[i].is_running ? "Running" : "Stopped");
        } else {
            printf("%d\t%s\t\t%s\n", process_list[i].pid, process_list[i].command, "Killed");
            // Loại bỏ tiến trình chết khỏi danh sách
            for (int j = i; j < process_count - 1; j++) {
                process_list[j] = process_list[j + 1];
            }
            process_count--; // Giảm số lượng tiến trình
            i--; // Kiểm tra lại vị trí này
        }
    }
}

// Hàm kiểm tra xem tiến trình có còn sống không
int is_process_alive(DWORD pid) {
    HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (hProcess == NULL) {
        return 0; // Tiến trình không còn tồn tại
    }

    DWORD exitCode;
    if (GetExitCodeProcess(hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
        CloseHandle(hProcess);
        return 0; // Tiến trình đã thoát
    }

    CloseHandle(hProcess);
    return 1; // Tiến trình còn sống
}

void kill_process(DWORD pid) {
    for (int i = 0; i < process_count; i++) {
        if (process_list[i].pid == pid) {
            // Mở tiến trình với quyền PROCESS_TERMINATE
            HANDLE hProcess = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION, FALSE, pid);
            if (hProcess == NULL) {
                printf("Failed to open process [%d]. Error %d\n", pid, GetLastError());
                return;
            }
            if (TerminateProcess(hProcess, 0)) {
                printf("Killed process [%d]: %s\n", pid, process_list[i].command);
                process_list[i].is_running = 0; // Đánh dấu là "Killed"
            } else {
                printf("Failed to kill process [%d]. Error %d\n", pid, GetLastError());
            }
            CloseHandle(hProcess);

            // Loại bỏ tiến trình khỏi danh sách nếu đã bị kill
            for (int j = i; j < process_count - 1; j++) {
                process_list[j] = process_list[j + 1];
            }
            process_count--; // Giảm số lượng tiến trình
            return;
        }
    }
    printf("Process with PID [%d] not found in the list.\n", pid);
}

// Hàm dừng tiến trình
void stop_process(DWORD pid) {
    // Mở tiến trình với quyền PROCESS_QUERY_INFORMATION và PROCESS_SUSPEND_RESUME
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_SUSPEND_RESUME, FALSE, pid);
    if (hProcess == NULL) {
        printf("Failed to open process [%d]. Error %d\n", pid, GetLastError());
        return;
    }

    // Sử dụng SuspendThread để tạm ngừng tiến trình
    DWORD suspendedThreads = 0;
    THREADENTRY32 te32;
    HANDLE hThreadSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

    if (hThreadSnapshot == INVALID_HANDLE_VALUE) {
        printf("Failed to create snapshot of threads. Error %d\n", GetLastError());
        CloseHandle(hProcess);
        return;
    }

    te32.dwSize = sizeof(THREADENTRY32);

    // Lặp qua tất cả các thread trong tiến trình
    if (Thread32First(hThreadSnapshot, &te32)) {
        do {
            if (te32.th32OwnerProcessID == pid) {
                // Mở từng thread trong tiến trình
                HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te32.th32ThreadID);
                if (hThread != NULL) {
                    // Tạm ngừng thread
                    SuspendThread(hThread);
                    suspendedThreads++;
                    CloseHandle(hThread);
                }
            }
        } while (Thread32Next(hThreadSnapshot, &te32));
    }

    CloseHandle(hThreadSnapshot);

    if (suspendedThreads > 0) {
        printf("Successfully stopped process [%d]. Suspended %d threads.\n", pid, suspendedThreads);
        // Cập nhật trạng thái tiến trình trong danh sách
        for (int i = 0; i < process_count; i++) {
            if (process_list[i].pid == pid) {
                process_list[i].is_running = 0; // Đánh dấu là "Stopped"
                break;
            }
        }
    } else {
        printf("Failed to suspend any threads for process [%d].\n", pid);
    }

    CloseHandle(hProcess);
}


// Hàm tiếp tục tiến trình
void resume_process(DWORD pid) {
    // Mở tiến trình với quyền PROCESS_SUSPEND_RESUME
    HANDLE hProcess = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
    if (hProcess == NULL) {
        printf("Failed to open process [%d]. Error %d\n", pid, GetLastError());
        return;
    }

    // Lấy danh sách các thread trong tiến trình
    DWORD resumedThreads = 0;
    THREADENTRY32 te32;
    HANDLE hThreadSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

    if (hThreadSnapshot == INVALID_HANDLE_VALUE) {
        printf("Failed to create snapshot of threads. Error %d\n", GetLastError());
        CloseHandle(hProcess);
        return;
    }

    te32.dwSize = sizeof(THREADENTRY32);

    // Lặp qua tất cả các thread trong tiến trình
    if (Thread32First(hThreadSnapshot, &te32)) {
        do {
            if (te32.th32OwnerProcessID == pid) {
                // Mở từng thread trong tiến trình
                HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te32.th32ThreadID);
                if (hThread != NULL) {
                    // Tiếp tục thread
                    ResumeThread(hThread);
                    resumedThreads++;
                    CloseHandle(hThread);
                }
            }
        } while (Thread32Next(hThreadSnapshot, &te32));
    }

    CloseHandle(hThreadSnapshot);

    if (resumedThreads > 0) {
        printf("Successfully resumed process [%d]. Resumed %d threads.\n", pid, resumedThreads);
        // Cập nhật trạng thái tiến trình trong danh sách
        for (int i = 0; i < process_count; i++) {
            if (process_list[i].pid == pid) {
                process_list[i].is_running = 1; // Đánh dấu là "Running"
                break;
            }
        }
    } else {
        printf("Failed to resume any threads for process [%d].\n", pid);
    }

    CloseHandle(hProcess);
}

void top_process() {
    printf("Top processes:\n");
    list_processes();
}

void print_date() {
    time_t t;
    struct tm *tm_info;

    time(&t);
    tm_info = localtime(&t);

    // Hiển thị ngày theo định dạng DD-MM-YYYY
    printf("Current date: %02d-%02d-%04d\n", tm_info->tm_mday, tm_info->tm_mon + 1, tm_info->tm_year + 1900);
}

// Hàm hiển thị thời gian hiện tại (chỉ giờ, phút, giây)
void print_time() {
    time_t t;
    struct tm *tm_info;

    time(&t);
    tm_info = localtime(&t);

    // Hiển thị thời gian theo định dạng HH:MM:SS
    printf("Current time: %02d:%02d:%02d\n", tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
}

// Hàm liệt kê các tệp trong thư mục hiện tại
void list_dir() {
    DIR *dir = opendir(".");
    struct dirent *entry;

    if (dir == NULL) {
        printf("Unable to open directory.\n");
        return;
    }

    printf("Listing directory contents:\n");
    while ((entry = readdir(dir)) != NULL) {
        printf("%s\n", entry->d_name);
    }

    closedir(dir);
}

int main() {
    signal(SIGINT, handle_signals);

    printf("Welcome to Tiny Shell (myShell)!\n");
    shell_loop();
    return 0;
}
