#include "src/log.hpp"
#include "src/widgets/widgets.hpp"

#include <gtk-2.0/gtk/gtk.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>
#include <string.h>
#include <stdio.h>
#include <spawn.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <pthread.h>


static gboolean lipc_set_string_property(char *publisher, char *prop, char *value) {
	char* lipc_set_prop_bin = (char*) "/usr/bin/lipc-set-prop";
	char* v_argv[] = { lipc_set_prop_bin, (char*) "-s", publisher, prop, value, NULL };
	pid_t pid;
	int status;

	if G_UNLIKELY (posix_spawn(&pid, lipc_set_prop_bin, NULL, NULL, v_argv, NULL))
		return FALSE;

	if G_LIKELY (waitpid(pid, &status, 0) != -1 && WIFEXITED(status))
		return !WEXITSTATUS(status);

	return FALSE;
}

void set_keyboard(gboolean show) {
	if (show) { lipc_set_string_property((char*) "com.lab126.keyboard", (char*) "open", (char*) "xyz.emlyn.kindle:abc:0"); }
	else { 
	    lipc_set_string_property((char*) "com.lab126.keyboard", (char*) "close", (char*) "xyz.emlyn.kindle");
	}
//		// todo - delay this by 0.5 - 1s
//		if (fork() == 0) {
//		    usleep(500000);  // .5 second delay
//			printf("slept!!\n"); fflush(stdout);
//            // execlp("eips", "eips", "''", NULL);
//            execlp("/mnt/us/libkh/bin/fbink", "fbink", "-s", "-f", NULL);
//            _exit(0);
//		}
}

void read_keyboard(kb_data_t* data) {
	const char* dev = "/dev/input/by-path/platform-imx-i2c.1-event";
//	const int X_MIN = 0, X_MAX = 1100;
//	const int Y_MIN = 1000, Y_MAX = 1500;

	// new boundry: 650 <= y' <= 1500, 0 <= x' <= 1500, where y' = x, x' = 1-y
	const int X_MIN = 0,   X_MAX = 1500;
	const int Y_MIN = 660, Y_MAX = 1100;

	printf("%s", LOGFMT("reading kb\n", LOG_DBG));
	fflush(stdout);

	int input_fd = open(dev, O_RDONLY);
	if (input_fd == -1) {
		printf(LOGFMT("cannot open %s: %s\n", LOG_ERR), dev, strerror(errno));
		fflush(stdout);
		return;
	}

	data->layer = 0;
	struct input_event ev;
	ssize_t n_read;
	char* kb_buf_ptr = data->kb_buf;
	while (1) {
		n_read = read(input_fd, &ev, sizeof(ev));
		if (n_read == (ssize_t) -1) {
			if (errno == EINTR) { continue; }
			else { break; }
		} else if (n_read != sizeof(ev)) { errno = EIO; continue; }

		if (ev.type == EV_ABS) {
			if (ev.code == 53) {
				// abs-x
				if ((ev.value < Y_MIN) || (ev.value > Y_MAX)) { data->y = -1; continue; }
				data->y = (((double) ev.value) - Y_MIN) / (Y_MAX - Y_MIN);
			} else if (ev.code == 54) {
				// abs-y
				if ((ev.value < X_MIN) || (ev.value > X_MAX)) { data->x = -1; continue; }
				data->x = 1 - ((((double) ev.value) - X_MIN) / (X_MAX - X_MIN));
			}
		} else if (ev.type == EV_KEY) {
			if (ev.code == 325) {
				// pressure event ( 0 down, 1 up )
				if (ev.value == 1) {
				    printf(LOGFMT("pressed at (%lf, %lf, %d)\n", LOG_DBG), data->x, data->y, data->layer);
					char pressed_key = match_keycode(data->lookup_table, data->layer, data->x, data->y);

					if (pressed_key == 0) {
						printf("%s", LOGFMT("failed to match coords\n", LOG_WRN));
						fflush(stdout);
						continue;
					}

					if (pressed_key < 0x05) {
					    printf(LOGFMT("going to layer %d\n", LOG_DBG), pressed_key - 1);
						data->layer = pressed_key - 1;
						continue;
					}

					*(kb_buf_ptr++) = pressed_key;
					if (!(data->on_key_press(data, pressed_key))) { break; }
					
					if ((pressed_key != 0x2) && (data->layer == 1)) { data->layer = 0; }
				}
			}
		}
	}
	*(kb_buf_ptr) = 0;
	data->on_enter(data);
}

void* _gtk_kb_redraw(void* data) { 
    GtkWidget* ref = (GtkWidget*) data; 
    fflush(stdout); usleep(500 * 1000);
    gtk_widget_queue_draw(ref);
    return NULL;
}
void* _keyboard_listener_async(void* data_vp) {
   	kb_data* data = (kb_data*) data_vp;
	data->last_showing = -1;
	memset(data->kb_buf, 0, 1024);
   
	set_keyboard(TRUE);
	read_keyboard(data);
	set_keyboard(FALSE);
	
    GtkWidget* toplevel = gtk_widget_get_toplevel(data->ref);
    pthread_t _t;
    pthread_create(&_t, NULL, _gtk_kb_redraw, (void*) toplevel);

	data->last_showing = time(NULL);
	
	return NULL;
} 

void keyboard_onpress_cb(GtkButton* _button, GdkEvent* _event, void* data_vp) {
   	kb_data* data = (kb_data*) data_vp;
	if ((data->last_showing < 0) || (time(NULL) - data->last_showing < data->min_frequency)) {
		printf("%s", LOGFMT("debounced keyboard button\n", LOG_DBG));
		fflush(stdout);
		return;
	}

   	pthread_t thread_id;
	pthread_create(&thread_id, NULL, _keyboard_listener_async, data);

}

void append_keycode_lut(kb_lut_t** kb_lut, int layer, double x, double y, double w, double h, char value) {
//	printf("\x1b[38;5;139m\x1b[1mDEBUG:\x1b[0m loading key <%c> in range (%lf, %lf) to (%lf, %lf) on layer %d\n", value, x, y, x+w, y+h, layer);
	kb_lut_t* lut = (kb_lut_t*) malloc(sizeof(kb_lut_t));
	if (!lut) { fprintf(stderr, LOGFMT("failed to alloc memory\n", LOG_ERR)); fflush(stderr); }
	if (*kb_lut) {
		(*kb_lut)->next = lut;
	} else { (*kb_lut) = lut; }

	lut->layer = layer;
	lut->x = x;
	lut->y = y;
	lut->w = w;
	lut->h = h;
	lut->value = value;
	lut->next = NULL;
	*kb_lut = lut;
}

void generate_keycode_lut(kb_lut_t** kb_lut) {
//  	layout (layer 0)
// 		   q w e r t y u i o p
// 			a s d f g h j k l
//		   sh z x c v b n m \b
// 	   	  123 [    sp    ]. \n
//
// 		shift (layer 1)
// todo!
//
// 		123 layer (layer 2)
// 		   1 2 3 4 5 6 7 8 9 0
// 		 	£ & ( ) " ' - + /
//	 	  #~= @ ! ? : ; , … \b
//  	  abc [    sp    ]. \n
//
//	    #~= layer (layer 3)
// 	       # % ~ ^ [ ] { } | \
// 		    . ™ ¬ ` < > _ * =
// 		  123 © ® § ¢ ¥ € $ \b
// 		  abc [    sp    ]. \n

	kb_lut_t* lut;
	append_keycode_lut(kb_lut, 0, 0   , 0   , 0.1 , 0.25, 'q');
	lut = *kb_lut;
	append_keycode_lut(kb_lut, 0, 0.1 , 0   , 0.1 , 0.25, 'w');
	append_keycode_lut(kb_lut, 0, 0.2 , 0   , 0.1 , 0.25, 'e');
    append_keycode_lut(kb_lut, 0, 0.3 , 0   , 0.1 , 0.25, 'r');
	append_keycode_lut(kb_lut, 0, 0.4 , 0   , 0.1 , 0.25, 't');
    append_keycode_lut(kb_lut, 0, 0.5 , 0   , 0.1 , 0.25, 'y');
	append_keycode_lut(kb_lut, 0, 0.6 , 0   , 0.1 , 0.25, 'u');
    append_keycode_lut(kb_lut, 0, 0.7 , 0   , 0.1 , 0.25, 'i');
	append_keycode_lut(kb_lut, 0, 0.8 , 0   , 0.1 , 0.25, 'o');
    append_keycode_lut(kb_lut, 0, 0.9 , 0   , 0.1 , 0.25, 'p');
	append_keycode_lut(kb_lut, 0, 0.05, 0.25, 0.1 , 0.25, 'a');
	append_keycode_lut(kb_lut, 0, 0.15, 0.25, 0.1 , 0.25, 's');
	append_keycode_lut(kb_lut, 0, 0.25, 0.25, 0.1 , 0.25, 'd');
	append_keycode_lut(kb_lut, 0, 0.35, 0.25, 0.1 , 0.25, 'f');
	append_keycode_lut(kb_lut, 0, 0.45, 0.25, 0.1 , 0.25, 'g');
	append_keycode_lut(kb_lut, 0, 0.55, 0.25, 0.1 , 0.25, 'h');
	append_keycode_lut(kb_lut, 0, 0.65, 0.25, 0.1 , 0.25, 'j');
	append_keycode_lut(kb_lut, 0, 0.75, 0.25, 0.1 , 0.25, 'k');
	append_keycode_lut(kb_lut, 0, 0.85, 0.25, 0.1 , 0.25, 'l');
	append_keycode_lut(kb_lut, 0, 0   , 0.5 , 0.15, 0.25, 0x2); // lshift - goto layer 1
	append_keycode_lut(kb_lut, 0, 0.15, 0.5 , 0.1 , 0.25, 'z');
	append_keycode_lut(kb_lut, 0, 0.25, 0.5 , 0.1 , 0.25, 'x');
	append_keycode_lut(kb_lut, 0, 0.35, 0.5 , 0.1 , 0.25, 'c');
	append_keycode_lut(kb_lut, 0, 0.45, 0.5 , 0.1 , 0.25, 'v');
	append_keycode_lut(kb_lut, 0, 0.55, 0.5 , 0.1 , 0.25, 'b');
	append_keycode_lut(kb_lut, 0, 0.65, 0.5 , 0.1 , 0.25, 'n');
	append_keycode_lut(kb_lut, 0, 0.75, 0.5 , 0.1 , 0.25, 'm');
	append_keycode_lut(kb_lut, 0, 0.8 , 0.5 , 0.15, 0.25, '\b');
	append_keycode_lut(kb_lut, 0, 0   , 0.75, 0.15, 0.25, 0x3); // goto layer 2
	append_keycode_lut(kb_lut, 0, 0.15, 0.75, 0.6 , 0.25, ' ');
	append_keycode_lut(kb_lut, 0, 0.75, 0.75, 0.1 , 0.25, '.');
	append_keycode_lut(kb_lut, 0, 0.85, 0.75, 0.15, 0.25, '\n');

	append_keycode_lut(kb_lut, 1, 0   , 0   , 0.1 , 0.25, 'Q');
    append_keycode_lut(kb_lut, 1, 0.1 , 0   , 0.1 , 0.25, 'W');
    append_keycode_lut(kb_lut, 1, 0.2 , 0   , 0.1 , 0.25, 'E');
    append_keycode_lut(kb_lut, 1, 0.3 , 0   , 0.1 , 0.25, 'R');
    append_keycode_lut(kb_lut, 1, 0.4 , 0   , 0.1 , 0.25, 'T');
    append_keycode_lut(kb_lut, 1, 0.5 , 0   , 0.1 , 0.25, 'Y');
    append_keycode_lut(kb_lut, 1, 0.6 , 0   , 0.1 , 0.25, 'U');
    append_keycode_lut(kb_lut, 1, 0.7 , 0   , 0.1 , 0.25, 'I');
    append_keycode_lut(kb_lut, 1, 0.8 , 0   , 0.1 , 0.25, 'O');
    append_keycode_lut(kb_lut, 1, 0.9 , 0   , 0.1 , 0.25, 'P');
    append_keycode_lut(kb_lut, 1, 0.05, 0.25, 0.1 , 0.25, 'A');
    append_keycode_lut(kb_lut, 1, 0.15, 0.25, 0.1 , 0.25, 'S');
    append_keycode_lut(kb_lut, 1, 0.25, 0.25, 0.1 , 0.25, 'D');
    append_keycode_lut(kb_lut, 1, 0.35, 0.25, 0.1 , 0.25, 'F');
    append_keycode_lut(kb_lut, 1, 0.45, 0.25, 0.1 , 0.25, 'G');
    append_keycode_lut(kb_lut, 1, 0.55, 0.25, 0.1 , 0.25, 'H');
    append_keycode_lut(kb_lut, 1, 0.65, 0.25, 0.1 , 0.25, 'J');
    append_keycode_lut(kb_lut, 1, 0.75, 0.25, 0.1 , 0.25, 'K');
    append_keycode_lut(kb_lut, 1, 0.85, 0.25, 0.1 , 0.25, 'L');
    append_keycode_lut(kb_lut, 1, 0   , 0.5 , 0.15, 0.25, 0x1); // lshift off - goto layer 0
    append_keycode_lut(kb_lut, 1, 0.15, 0.5 , 0.1 , 0.25, 'Z');
    append_keycode_lut(kb_lut, 1, 0.25, 0.5 , 0.1 , 0.25, 'X');
    append_keycode_lut(kb_lut, 1, 0.35, 0.5 , 0.1 , 0.25, 'C');
    append_keycode_lut(kb_lut, 1, 0.45, 0.5 , 0.1 , 0.25, 'V');
    append_keycode_lut(kb_lut, 1, 0.55, 0.5 , 0.1 , 0.25, 'B');
    append_keycode_lut(kb_lut, 1, 0.65, 0.5 , 0.1 , 0.25, 'N');
    append_keycode_lut(kb_lut, 1, 0.75, 0.5 , 0.1 , 0.25, 'M');
    append_keycode_lut(kb_lut, 1, 0.8 , 0.5 , 0.15, 0.25, '\b');
    append_keycode_lut(kb_lut, 1, 0   , 0.75, 0.15, 0.25, 0x3); // goto layer 2
    append_keycode_lut(kb_lut, 1, 0.15, 0.75, 0.6 , 0.25, ' ');
    append_keycode_lut(kb_lut, 1, 0.75, 0.75, 0.1 , 0.25, '.');
    append_keycode_lut(kb_lut, 1, 0.85, 0.75, 0.15, 0.25, '\n');

	append_keycode_lut(kb_lut, 2, 0   , 0   , 0.1 , 0.25, '1');
	append_keycode_lut(kb_lut, 2, 0.2 , 0   , 0.1 , 0.25, '2');
	append_keycode_lut(kb_lut, 2, 0.3 , 0   , 0.1 , 0.25, '3');
    append_keycode_lut(kb_lut, 2, 0.4 , 0   , 0.1 , 0.25, '4');
	append_keycode_lut(kb_lut, 2, 0.5 , 0   , 0.1 , 0.25, '5');
    append_keycode_lut(kb_lut, 2, 0.6 , 0   , 0.1 , 0.25, '6');
	append_keycode_lut(kb_lut, 2, 0.7 , 0   , 0.1 , 0.25, '7');
    append_keycode_lut(kb_lut, 2, 0.8 , 0   , 0.1 , 0.25, '8');
	append_keycode_lut(kb_lut, 2, 0.9 , 0   , 0.1 , 0.25, '9');
    append_keycode_lut(kb_lut, 2, 1.0 , 0   , 0.1 , 0.25, '0');
	append_keycode_lut(kb_lut, 2, 0   , 0.25, 0.15, 0.25, '$');
	append_keycode_lut(kb_lut, 2, 0.15, 0.25, 0.1 , 0.25, '&');
	append_keycode_lut(kb_lut, 2, 0.25, 0.25, 0.1 , 0.25, '(');
	append_keycode_lut(kb_lut, 2, 0.35, 0.25, 0.1 , 0.25, ')');
	append_keycode_lut(kb_lut, 2, 0.45, 0.25, 0.1 , 0.25, '"');
	append_keycode_lut(kb_lut, 2, 0.55, 0.25, 0.1 , 0.25, '\'');
	append_keycode_lut(kb_lut, 2, 0.65, 0.25, 0.1 , 0.25, '-');
	append_keycode_lut(kb_lut, 2, 0.75, 0.25, 0.1 , 0.25, '+');
	append_keycode_lut(kb_lut, 2, 0.85, 0.25, 0.15, 0.25, '/');
	append_keycode_lut(kb_lut, 2, 0   , 0.5 , 0.15, 0.25, 0x4); // goto layer 3
	append_keycode_lut(kb_lut, 2, 0.15, 0.5 , 0.1 , 0.25, '@');
	append_keycode_lut(kb_lut, 2, 0.25, 0.5 , 0.1 , 0.25, '!');
	append_keycode_lut(kb_lut, 2, 0.35, 0.5 , 0.1 , 0.25, '?');
	append_keycode_lut(kb_lut, 2, 0.45, 0.5 , 0.1 , 0.25, ':');
	append_keycode_lut(kb_lut, 2, 0.55, 0.5 , 0.1 , 0.25, ';');
	append_keycode_lut(kb_lut, 2, 0.65, 0.5 , 0.1 , 0.25, ',');
	append_keycode_lut(kb_lut, 2, 0.75, 0.5 , 0.1 , 0.25, '_');
	append_keycode_lut(kb_lut, 2, 0.85, 0.5 , 0.15, 0.25, '\b');
	append_keycode_lut(kb_lut, 2, 0   , 0.75, 0.15, 0.25, 0x1); // goto layer 0
	append_keycode_lut(kb_lut, 2, 0.15, 0.75, 0.6 , 0.25, ' ');
	append_keycode_lut(kb_lut, 2, 0.75, 0.75, 0.1 , 0.25, '.');
	append_keycode_lut(kb_lut, 2, 0.85, 0.75, 0.15, 0.25, '\n');

	append_keycode_lut(kb_lut, 3, 0   , 0   , 0.1 , 0.25, '#');
	append_keycode_lut(kb_lut, 3, 0.1 , 0   , 0.1 , 0.25, '%');
	append_keycode_lut(kb_lut, 3, 0.2 , 0   , 0.1 , 0.25, '~');
	append_keycode_lut(kb_lut, 3, 0.3 , 0   , 0.1 , 0.25, '^');
	append_keycode_lut(kb_lut, 3, 0.4 , 0   , 0.1 , 0.25, '[');
	append_keycode_lut(kb_lut, 3, 0.5 , 0   , 0.1 , 0.25, ']');
	append_keycode_lut(kb_lut, 3, 0.6 , 0   , 0.1 , 0.25, '{');
	append_keycode_lut(kb_lut, 3, 0.7 , 0   , 0.1 , 0.25, '}');
	append_keycode_lut(kb_lut, 3, 0.8 , 0   , 0.1 , 0.25, '|');
	append_keycode_lut(kb_lut, 3, 0.9 , 0   , 0.1 , 0.25, '\\');
	append_keycode_lut(kb_lut, 3, 0   , 0.25, 0.15, 0.25, '.');
//	append_keycode_lut(kb_lut, 3, 0.15, 0.25, 0.1 , 0.25, '™');
//	append_keycode_lut(kb_lut, 3, 0.25, 0.25, 0.1 , 0.25, '¬');
	append_keycode_lut(kb_lut, 3, 0.35, 0.25, 0.1 , 0.25, '`');
	append_keycode_lut(kb_lut, 3, 0.45, 0.25, 0.1 , 0.25, '<');
	append_keycode_lut(kb_lut, 3, 0.55, 0.25, 0.1 , 0.25, '>');
	append_keycode_lut(kb_lut, 3, 0.65, 0.25, 0.1 , 0.25, '_');
	append_keycode_lut(kb_lut, 3, 0.75, 0.25, 0.1 , 0.25, '*');
	append_keycode_lut(kb_lut, 3, 0.85, 0.25, 0.15, 0.25, '=');
	append_keycode_lut(kb_lut, 3, 0   , 0.5 , 0.15, 0.25, 0x3); // goto layer 2
//	append_keycode_lut(kb_lut, 3, 0.15, 0.5 , 0.1 , 0.25, '©');
//	append_keycode_lut(kb_lut, 3, 0.25, 0.5 , 0.1 , 0.25, '®');
//	append_keycode_lut(kb_lut, 3, 0.35, 0.5 , 0.1 , 0.25, '§');
//	append_keycode_lut(kb_lut, 3, 0.45, 0.5 , 0.1 , 0.25, '¢');
//	append_keycode_lut(kb_lut, 3, 0.55, 0.5 , 0.1 , 0.25, '¥');
//	append_keycode_lut(kb_lut, 3, 0.65, 0.5 , 0.1 , 0.25, '€');
	append_keycode_lut(kb_lut, 3, 0.75, 0.5 , 0.1 , 0.25, '$');
	append_keycode_lut(kb_lut, 3, 0.85, 0.5 , 0.15, 0.25, '\b');
	append_keycode_lut(kb_lut, 3, 0   , 0.75, 0.15, 0.25, 0x1); // goto layer 0
	append_keycode_lut(kb_lut, 3, 0.15, 0.75, 0.6 , 0.25, ' ');
	append_keycode_lut(kb_lut, 3, 0.75, 0.75, 0.1 , 0.25, '.');
	append_keycode_lut(kb_lut, 3, 0.85, 0.75, 0.15, 0.25, '\n');

	*kb_lut = lut;
}

char match_keycode(kb_lut_t* lut, int layer, double x, double y) {
	if ( (layer == lut->layer) && (x >= lut->x) && (x <= lut->x+lut->w) && (y >= lut->y) && (y <= lut->y+lut->h) ) {
	    return lut->value;
	} else if (lut->next) {
	    return match_keycode(lut->next, layer, x, y);
	} else { return 0; }
}

GtkWidget* keyboard_widget(void* add_data, gboolean (*onpress)(kb_data_t* data, char key), void (*onenter)(kb_data_t* data)) {
	kb_data_t* data = (kb_data*) malloc(sizeof(kb_data));
	data->min_frequency = 0.1;
	data->last_showing = 0;
	data->kb_buf = (char*) malloc(1024 * sizeof(char));
	data->layer = 0;
	data->on_key_press = onpress;
	data->on_enter = onenter;
	data->lookup_table = NULL;
	data->add_data = add_data;
	generate_keycode_lut(&data->lookup_table);

	GtkWidget* wrapper = gtk_vbox_new(FALSE, 0);
	data->ref = wrapper;

	GtkWidget* align = gtk_alignment_new(0.5, 0, 1, 1);
	GtkWidget* ev_box = gtk_event_box_new();
	gtk_widget_add_events(ev_box, GDK_BUTTON_PRESS_MASK);
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(ev_box), FALSE);
	GTK_WIDGET_UNSET_FLAGS(GTK_EVENT_BOX(ev_box), GTK_CAN_FOCUS);
	g_signal_connect(ev_box, "button-press-event", G_CALLBACK(keyboard_onpress_cb), data);
	gtk_container_add(GTK_CONTAINER(align), ev_box);

	gtk_box_pack_start(GTK_BOX(wrapper), align, TRUE, TRUE, 0);
	return wrapper;
}
