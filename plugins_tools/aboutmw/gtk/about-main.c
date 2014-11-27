#include <config.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>

#include <gtk/gtk.h>

#include "about_glade.h"
#include "now.h"

#ifndef _
#define _(s) (s)
#endif

static enum _bits {
	BITS_UNKNOWN,
	BITS_32,
	BITS_64,
	BITS_FOREIGN,
} bitness;

void do_viewer(GtkWidget* top, GtkListStore* data) {
	FILE* f = popen("which eid-viewer", "r");
	GtkTreeIter iter;
	char tmp[PATH_MAX];
	char* loc;

	gtk_list_store_append(data, &iter);
	if(!f) {
		gtk_list_store_set(data, &iter, 0, _("eID Viewer location"), 1, _("(not found)"), -1);
		return;
	}
	tmp[PATH_MAX-1]='\0';
	fgets(tmp, PATH_MAX, f);
	pclose(f);
	if((loc = strrchr(tmp, '\n')) != NULL) {
		*loc = '\0';
	}
	if(strlen(tmp) == 0) {
		gtk_list_store_set(data, &iter, 0, _("eID Viewer location"), 1, _("(not found)"), -1);
		return;
	}
	gtk_list_store_set(data, &iter, 0, _("eID Viewer location"), 1, tmp, -1);
}

void do_files(GtkWidget* top, GtkListStore* data) {
	GtkWidget* dialog;
	struct stat st;
	struct {
		enum _bits bitness;
		char* loc;
	} locs[] = {
		{ BITS_32, "/usr/lib/libbeidpkcs11.so.0" },
		{ BITS_32, "/usr/lib/i386-linux-gnu/libbeidpkcs11.so.0" },
		{ BITS_64, "/usr/lib64/libbeidpkcs11.so.0" },
		{ BITS_64, "/usr/lib/x86_64-linux-gnu/libbeidpkcs11.so.0" },
		{ bitness, LIBDIR },
	};
	int i;
	gboolean found32 = FALSE;
	gboolean found64 = FALSE;
	gboolean foundforeign = FALSE;
	GtkTreeIter iter;

	for(i=0;i<4;i++) {
		if(stat(locs[i].loc, &st) < 0) {
			switch(errno) {
			case ENOENT:
				/* file does not exist */
				break;
			default:
				dialog = gtk_message_dialog_new(GTK_WINDOW(top), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "stat: %s", strerror(errno));
				gtk_dialog_run(GTK_DIALOG(dialog));
				gtk_widget_destroy(dialog);
				break;
			}
		} else {
			gchar* str;
			switch(locs[i].bitness) {
				case BITS_32:
					str = _("32-bit PKCS#11 location");
					found32 = TRUE;
					break;
				case BITS_64:
					str = _("64-bit PKCS#11 location");
					found64 = TRUE;
					break;
				default:
					str = _("PKCS#11 location");
					foundforeign = TRUE;
					break;
			}

			gtk_list_store_append(data, &iter);
			gtk_list_store_set(data, &iter, 0, str, 1, locs[i].loc, -1);
		}
	}
	switch(bitness) {
	case BITS_32:
	case BITS_64:
		if(!found32) {
			gtk_list_store_append(data, &iter);
			gtk_list_store_set(data, &iter, 0, _("32-bit PKCS#11 location"), 1, _("(not found)"), -1);
		}
		if(!found64) {
			gtk_list_store_append(data, &iter);
			gtk_list_store_set(data, &iter, 0, _("64-bit PKCS#11 location"), 1, _("(not found)"), -1);
		}
		break;
	default:
		if(!foundforeign) {
			gtk_list_store_append(data, &iter);
			gtk_list_store_set(data, &iter, 0, _("PKCS#11 location"), 1, _("(not found)"), -1);
		}
		break;
	}
}

void do_uname(GtkWidget* top, GtkListStore* data) {
	GtkTreeIter iter;
	struct utsname undat;
	char* values;

	if(uname(&undat) < 0) {
		GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(top), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "uname: %s", strerror(errno));
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
	}

	gtk_list_store_append(data, &iter);
	if(!strcmp(undat.machine, "x86_64")) {
		bitness = BITS_64;
		gtk_list_store_set(data, &iter, 0, _("System architecture"), 1, _("64-bit PC"), -1);
	} else if(undat.machine[0] == 'i' && undat.machine[2] == '8' && undat.machine[3] == '6') {
		bitness = BITS_32;
		gtk_list_store_set(data, &iter, 0, _("System architecture"), 1, _("32-bit PC"), -1);
	} else {
		bitness = BITS_FOREIGN;
		asprintf(&values, _("Unknown (%s)"), undat.machine);
		gtk_list_store_set(data, &iter, 0, _("System architecture"), 1, values, -1);
		free(values);
	}

	gtk_list_store_append(data, &iter);
	asprintf(&values, "%s %s %s %s %s", undat.sysname, undat.nodename, undat.release, undat.version, undat.machine);
	gtk_list_store_set(data, &iter, 0, _("uname"), 1, values, -1);
	free(values);
}

char* get_lsb_info(char opt) {
	char cmd[] = "lsb_release -. -s";
	char *rv, *loc;
	FILE *f;

	*(strchr(cmd, '.')) = opt;
	f=popen(cmd, "r");
	if(!f) {
		return strdup(_("(unknown)"));
	}
	rv = malloc(80);
	rv[79]='\0';
	rv[0]='\0';
	fgets(rv, 79, f);
	pclose(f);
	if(strlen(rv) == 0) {
		free(rv);
		return strdup(_("(unknown)"));
	}
	if((loc = strrchr(rv, '\n')) != NULL) {
		*loc = '\0';
	}
	return rv;
}

void do_distro(GtkWidget* top, GtkListStore* data) {
	GtkTreeIter iter;
	FILE *f;
	char *dat;

	dat = get_lsb_info('i');
	gtk_list_store_append(data, &iter);
	gtk_list_store_set(data, &iter, 0, _("Distribution"), 1, dat, -1);
	free(dat);

	dat = get_lsb_info('r');
	gtk_list_store_append(data, &iter);
	gtk_list_store_set(data, &iter, 0, _("Distribution version"), 1, dat, -1);
	free(dat);

	dat = get_lsb_info('c');
	gtk_list_store_append(data, &iter);
	gtk_list_store_set(data, &iter, 0, _("Distribution codename"), 1, dat, -1);
	free(dat);
}

int main(int argc, char** argv) {
	GtkBuilder* builder;
	GtkWidget *window, *treeview;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn* col;
	GtkTreeIter iter;
	GtkListStore *store;
	gchar *tmp, *loc;

	gtk_init(&argc, &argv);

	builder = gtk_builder_new();
	gtk_builder_add_from_string(builder, ABOUT_GLADE_STRING, strlen(ABOUT_GLADE_STRING), NULL);
	window = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
	gtk_widget_show_all(window);

	g_signal_connect(G_OBJECT(window), "delete-event", gtk_main_quit, NULL);
	g_signal_connect(G_OBJECT(gtk_builder_get_object(builder, "quitbtn")), "clicked", gtk_main_quit, NULL);

	treeview = GTK_WIDGET(gtk_builder_get_object(builder, "treeview"));

	store = GTK_LIST_STORE(gtk_builder_get_object(builder, "infodata"));

	tmp = g_strdup(PACKAGE_VERSION);
	loc = strchr(tmp, '-');
	*loc = '\0';
	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter, 0, _("Middleware version"), 1, tmp, -1);
	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter, 0, _("Middleware build ID"), 1, loc+1, -1);

	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter, 0, _("Middleware build date"), 1, EID_NOW_STRING, -1);

	do_uname(window, store);
	do_files(window, store);
	do_distro(window, store);
	do_viewer(window, store);

	renderer = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(_("Item"), renderer, "text", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), col);
	col = gtk_tree_view_column_new_with_attributes(_("Value"), renderer, "text", 1, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), col);

	gtk_main();
}
