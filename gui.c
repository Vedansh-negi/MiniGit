/*
 * File: gui.c
 * Brief: GTK 4 GUI for the Mini-Git & Smart Search Engine.
 *
 * Tabs:
 *  - Search Engine (search + suggestions)
 *  - Mini-Git (init, add, commit, log, view, delete, checkout, save)
 *  - Editor (multi-file editor with simple syntax highlighting)
 */

#include <gtk/gtk.h>
#include <pango/pango.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minigit.h"
#include "trie_index.h"
#include "search_engine.h"
#include "autocomplete.h"
#include "ranking.h"

#define WORKING_DIR ".mgit_work"   /* must match minigit.c */

/* ---------------- Global GTK Widgets ---------------- */

/* Search tab */
GtkWidget *search_entry;
GtkWidget *suggestions_view;
GtkWidget *search_results_view;
GtkWidget *dark_mode_toggle;

/* Mini-Git tab */
GtkWidget *git_output_view;
GtkWidget *git_filename_entry;
GtkWidget *git_commit_entry;         /* commit staged files */
GtkWidget *git_commit_id_entry;      /* view/delete/checkout */
GtkWidget *git_save_commit_entry;    /* commit from working dir */
GtkWidget *commit_files_list;        /* list of files in checked-out commit */

/* Editor tab */
GtkWidget *editor_notebook;          /* multiple file tabs */

/* ---------------- Helper: TextView utilities ---------------- */

static void set_text_view_text(GtkWidget *text_view, const char *text) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_text_buffer_set_text(buffer, text ? text : "", -1);
}

static void append_text_view_text(GtkWidget *text_view, const char *text) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    GtkTextIter end_iter;
    gtk_text_buffer_get_end_iter(buffer, &end_iter);
    gtk_text_buffer_insert(buffer, &end_iter, text ? text : "", -1);
}

/* ---------------- Helper: File I/O + editor helpers ---------------- */

static char *read_file_to_string(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return NULL;

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    rewind(fp);

    if (sz < 0) { fclose(fp); return NULL; }

    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(fp); return NULL; }

    size_t n = fread(buf, 1, (size_t)sz, fp);
    buf[n] = '\0';
    fclose(fp);
    return buf;
}

static gboolean save_textview_to_file(GtkTextView *tv, const char *path) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(tv);
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

    FILE *fp = fopen(path, "w");
    if (!fp) {
        g_free(text);
        return FALSE;
    }

    fputs(text, fp);
    fclose(fp);
    g_free(text);
    return TRUE;
}

/* ---------------- Simple syntax highlighting ---------------- */

static void apply_syntax_highlighting(GtkTextView *text_view, const char *filename) {
    (void)filename; // not used yet, but could be used to detect language by extension

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(text_view);
    GtkTextTagTable *table = gtk_text_buffer_get_tag_table(buffer);

    GtkTextTag *keyword_tag = gtk_text_tag_table_lookup(table, "keyword");
    if (!keyword_tag) {
        keyword_tag = gtk_text_tag_new("keyword");
        g_object_set(keyword_tag,
                     "foreground", "blue",
                     "weight", PANGO_WEIGHT_BOLD,
                     NULL);
        gtk_text_tag_table_add(table, keyword_tag);
    }

    const char *keywords[] = {
        "int", "float", "double", "char", "void",
        "return", "if", "else", "for", "while",
        "public", "class", "static", "System",
        "printf", "main",
        NULL
    };

    GtkTextIter start;
    gtk_text_buffer_get_start_iter(buffer, &start);

    for (int k = 0; keywords[k] != NULL; k++) {
        const char *kw = keywords[k];
        GtkTextIter iter = start;
        GtkTextIter match_start, match_end;

        while (gtk_text_iter_forward_search(
                   &iter, kw,
                   GTK_TEXT_SEARCH_TEXT_ONLY | GTK_TEXT_SEARCH_VISIBLE_ONLY,
                   &match_start, &match_end, NULL)) {
            gtk_text_buffer_apply_tag(buffer, keyword_tag, &match_start, &match_end);
            iter = match_end;
        }
    }
}

/* ---------------- Dark mode toggle ---------------- */

static void on_dark_mode_toggled(GtkToggleButton *toggle, gpointer user_data) {
    (void)user_data;
    gboolean active = gtk_toggle_button_get_active(toggle);

    GdkDisplay *display = gdk_display_get_default();
    if (!display) return;

    GtkSettings *settings = gtk_settings_get_for_display(display);
    g_object_set(settings, "gtk-application-prefer-dark-theme", active, NULL);
}

/* ---------------- Search Engine Callbacks ---------------- */

static void on_suggest_button_clicked(GtkButton *button, gpointer user_data) {
    (void)button; (void)user_data;

    const char *prefix = gtk_editable_get_text(GTK_EDITABLE(search_entry));
    if (!prefix || strlen(prefix) == 0) {
        set_text_view_text(suggestions_view, "Please enter a prefix to get suggestions.");
        return;
    }

    autocomplete_result_t suggestions[MAX_AUTOCOMPLETE_SUGGESTIONS];
    int count = get_autocomplete_suggestions(prefix, suggestions, MAX_AUTOCOMPLETE_SUGGESTIONS);

    if (count == 0) {
        set_text_view_text(suggestions_view, "No suggestions found.");
        return;
    }

    GString *output = g_string_new("");
    for (int i = 0; i < count; i++) {
        g_string_append_printf(output, "- %s (Score: %.2f)\n",
                               suggestions[i].suggestion, suggestions[i].score);
    }

    set_text_view_text(suggestions_view, output->str);
    g_string_free(output, TRUE);
}

static void on_search_button_clicked(GtkButton *button, gpointer user_data) {
    (void)button; (void)user_data;

    const char *term = gtk_editable_get_text(GTK_EDITABLE(search_entry));
    if (!term || strlen(term) == 0) {
        set_text_view_text(search_results_view, "Please enter a search term.");
        return;
    }

    search_result_t results[MAX_SEARCH_RESULTS];
    int count = search_and_rank(term, results, MAX_SEARCH_RESULTS);

    if (count == 0) {
        set_text_view_text(search_results_view, "No results found.");
        return;
    }

    GString *output = g_string_new("");
    for (int i = 0; i < count; i++) {
        g_string_append_printf(output, "%d. %s (Relevance: %.2f)\n",
                               i + 1, results[i].title, results[i].relevance_score);
        g_string_append_printf(output, "   %s\n", results[i].description);
        g_string_append_printf(output, "   URL: %s\n\n", results[i].url);
    }

    set_text_view_text(search_results_view, output->str);
    g_string_free(output, TRUE);
}

/* ---------------- Mini-Git Callbacks ---------------- */

static void refresh_commit_log_to_textview(void) {
    Commit *temp = repo.head;
    if (!temp) {
        set_text_view_text(git_output_view, "No commits yet.\n");
        return;
    }

    GString *output = g_string_new("Commit Log:\n");
    while (temp) {
        g_string_append_printf(output, "Commit %d: %s\n", temp->commit_id, temp->message);
        temp = temp->next;
    }

    set_text_view_text(git_output_view, output->str);
    g_string_free(output, TRUE);
}

static void on_init_button_clicked(GtkButton *button, gpointer user_data) {
    (void)button; (void)user_data;
    init_repository();
    set_text_view_text(git_output_view, "Repository has been initialized.\n");
}

/* Add file (absolute or relative path) */
static void on_add_button_clicked(GtkButton *button, gpointer user_data) {
    (void)button; (void)user_data;
    const char *filename = gtk_editable_get_text(GTK_EDITABLE(git_filename_entry));
    if (!filename || strlen(filename) == 0) {
        set_text_view_text(git_output_view, "Error: Please enter a filename to add.\n");
        return;
    }

    char *filename_copy = g_strdup(filename);
    add_file(filename_copy);
    g_free(filename_copy);

    char output[256];
    snprintf(output, sizeof(output), "Attempted to add file '%s'.\n(Check console if something failed.)\n", filename);
    set_text_view_text(git_output_view, output);
    gtk_editable_set_text(GTK_EDITABLE(git_filename_entry), "");
}

/* Commit staged files */
static void on_commit_button_clicked(GtkButton *button, gpointer user_data) {
    (void)button; (void)user_data;
    const char *message = gtk_editable_get_text(GTK_EDITABLE(git_commit_entry));
    if (!message || strlen(message) == 0) {
        set_text_view_text(git_output_view, "Error: Please enter a commit message.\n");
        return;
    }

    char *message_copy = g_strdup(message);
    commit_staged(message_copy);
    g_free(message_copy);

    set_text_view_text(git_output_view, "Commit created from staged files.\n(See console for details.)\n");
    gtk_editable_set_text(GTK_EDITABLE(git_commit_entry), "");
}

/* View log */
static void on_log_button_clicked(GtkButton *button, gpointer user_data) {
    (void)button; (void)user_data;
    refresh_commit_log_to_textview();
}

/* View commit details (message only) */
static void on_view_button_clicked(GtkButton *button, gpointer user_data) {
    (void)button; (void)user_data;
    const char *id_str = gtk_editable_get_text(GTK_EDITABLE(git_commit_id_entry));
    int cid = atoi(id_str);

    if (cid <= 0) {
        set_text_view_text(git_output_view, "Error: Please enter a valid commit ID.\n");
        return;
    }

    Commit *temp = repo.head;
    while (temp) {
        if (temp->commit_id == cid) {
            char output[512];
            snprintf(output, sizeof(output),
                     "Details for Commit %d:\n%s\n", temp->commit_id, temp->message);
            set_text_view_text(git_output_view, output);
            return;
        }
        temp = temp->next;
    }
    set_text_view_text(git_output_view, "Commit not found.\n");
}

/* Delete commit */
static void on_delete_button_clicked(GtkButton *button, gpointer user_data) {
    (void)button; (void)user_data;
    const char *id_str = gtk_editable_get_text(GTK_EDITABLE(git_commit_id_entry));
    int cid = atoi(id_str);

    if (cid <= 0) {
        set_text_view_text(git_output_view, "Error: Please enter a valid commit ID.\n");
        return;
    }

    delete_commit(cid);
    refresh_commit_log_to_textview();
    append_text_view_text(git_output_view, "\n(Attempted to delete commit. See console for details.)\n");
}

/* After checkout, fill commit_files_list with filenames from that commit */
static void fill_commit_files_list_for_commit(int cid) {
    /* GTK4: simply clear all rows */
    gtk_list_box_remove_all(GTK_LIST_BOX(commit_files_list));

    Commit *temp = repo.head;
    while (temp) {
        if (temp->commit_id == cid) {
            for (int i = 0; i < temp->file_count; i++) {
                GtkWidget *row   = gtk_list_box_row_new();
                GtkWidget *label = gtk_label_new(temp->files[i].filename);
                gtk_widget_set_halign(label, GTK_ALIGN_START);
                gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
                gtk_list_box_append(GTK_LIST_BOX(commit_files_list), row);
            }
            return;
        }
        temp = temp->next;
    }
}

/* Checkout commit: use backend checkout_commit + show files in list */
static void on_checkout_button_clicked(GtkButton *button, gpointer user_data) {
    (void)button; (void)user_data;
    const char *id_str = gtk_editable_get_text(GTK_EDITABLE(git_commit_id_entry));
    int cid = atoi(id_str);

    if (cid <= 0) {
        set_text_view_text(git_output_view, "Error: Please enter a valid commit ID for checkout.\n");
        return;
    }

    /* Check if commit exists */
    Commit *temp = repo.head;
    int found = 0;
    while (temp) {
        if (temp->commit_id == cid) { found = 1; break; }
        temp = temp->next;
    }
    if (!found) {
        set_text_view_text(git_output_view, "Commit not found. Cannot checkout.\n");
        return;
    }

    checkout_commit(cid);  /* backend: writes files into .mgit_work/ */
    fill_commit_files_list_for_commit(cid);

    set_text_view_text(git_output_view,
                       "Checkout complete.\nFiles written to .mgit_work/ and listed below.\n");
}

/* Save all files from WORKING_DIR as a new commit (save_commit) */
static void on_save_commit_button_clicked(GtkButton *button, gpointer user_data) {
    (void)button; (void)user_data;
    const char *msg = gtk_editable_get_text(GTK_EDITABLE(git_save_commit_entry));
    if (!msg || strlen(msg) == 0) {
        set_text_view_text(git_output_view, "Please enter a message for the working-directory commit.\n");
        return;
    }

    save_commit(msg);
    set_text_view_text(git_output_view, "Created commit from working directory (.mgit_work/).\n");
    gtk_editable_set_text(GTK_EDITABLE(git_save_commit_entry), "");
}

/* ---------------- Editor tab: open file from commit list ---------------- */

static void on_open_in_editor_clicked(GtkButton *button, gpointer user_data) {
    (void)button; (void)user_data;

    GtkListBoxRow *row = gtk_list_box_get_selected_row(GTK_LIST_BOX(commit_files_list));
    if (!row) {
        set_text_view_text(git_output_view, "Please select a file from the commit file list.\n");
        return;
    }

    GtkWidget *child = gtk_list_box_row_get_child(row);
    const char *filename = gtk_label_get_text(GTK_LABEL(child));
    if (!filename) {
        set_text_view_text(git_output_view, "Error: selected row has no filename.\n");
        return;
    }

    char path[512];
    snprintf(path, sizeof(path), "%s/%s", WORKING_DIR, filename);

    char *contents = read_file_to_string(path);
    if (!contents) {
        set_text_view_text(git_output_view, "Could not open file from .mgit_work/.\n");
        return;
    }

    /* Create a new editor tab */
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled, TRUE);

    GtkWidget *textview = gtk_text_view_new();
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(textview), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textview), GTK_WRAP_WORD_CHAR);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), textview);

    /* Set file content */
    set_text_view_text(textview, contents);
    g_free(contents);

    /* Store filepath in widget data, so we can save later */
    g_object_set_data_full(G_OBJECT(textview), "filepath", g_strdup(path), g_free);

    /* Apply simple syntax highlighting */
    apply_syntax_highlighting(GTK_TEXT_VIEW(textview), filename);

    /* Add page to notebook */
    GtkWidget *tab_label = gtk_label_new(filename);
    int page_num = gtk_notebook_append_page(GTK_NOTEBOOK(editor_notebook), scrolled, tab_label);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(editor_notebook), page_num);
}

/* Save currently open editor tab back to its file */
static void on_editor_save_file_clicked(GtkButton *button, gpointer user_data) {
    (void)button; (void)user_data;

    int page = gtk_notebook_get_current_page(GTK_NOTEBOOK(editor_notebook));
    if (page < 0) return;

    GtkWidget *child = gtk_notebook_get_nth_page(GTK_NOTEBOOK(editor_notebook), page);
    if (!child) return;

    GtkWidget *textview = gtk_scrolled_window_get_child(GTK_SCROLLED_WINDOW(child));
    if (!GTK_IS_TEXT_VIEW(textview)) return;

    const char *path = g_object_get_data(G_OBJECT(textview), "filepath");
    if (!path) {
        set_text_view_text(git_output_view, "No filepath associated with this editor tab.\n");
        return;
    }

    if (save_textview_to_file(GTK_TEXT_VIEW(textview), path)) {
        set_text_view_text(git_output_view, "File saved to working directory.\n");
    } else {
        set_text_view_text(git_output_view, "Failed to save file.\n");
    }
}

/* ---------------- Tab creation helpers ---------------- */

static GtkWidget* create_search_tab(void) {
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_widget_set_margin_start(grid, 10);
    gtk_widget_set_margin_end(grid, 10);
    gtk_widget_set_margin_top(grid, 10);
    gtk_widget_set_margin_bottom(grid, 10);

    search_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(search_entry), "Enter search term...");

    GtkWidget *suggest_button = gtk_button_new_with_label("Suggest");
    g_signal_connect(suggest_button, "clicked", G_CALLBACK(on_suggest_button_clicked), NULL);

    GtkWidget *search_button = gtk_button_new_with_label("Search");
    g_signal_connect(search_button, "clicked", G_CALLBACK(on_search_button_clicked), NULL);

    dark_mode_toggle = gtk_check_button_new_with_label("Dark mode");
    g_signal_connect(dark_mode_toggle, "toggled", G_CALLBACK(on_dark_mode_toggled), NULL);

    GtkWidget *suggest_scrolled_win = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(suggest_scrolled_win, TRUE);
    suggestions_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(suggestions_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(suggestions_view), FALSE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(suggest_scrolled_win), suggestions_view);

    GtkWidget *results_scrolled_win = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(results_scrolled_win, TRUE);
    search_results_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(search_results_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(search_results_view), FALSE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(results_scrolled_win), search_results_view);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Query:"),     0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), search_entry,                1, 0, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), suggest_button,              3, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), search_button,               4, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), dark_mode_toggle,            5, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Suggestions:"), 0, 1, 6, 1);
    gtk_grid_attach(GTK_GRID(grid), suggest_scrolled_win,          0, 2, 6, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Search Results:"), 0, 3, 6, 1);
    gtk_grid_attach(GTK_GRID(grid), results_scrolled_win,            0, 4, 6, 1);

    return grid;
}

static GtkWidget* create_minigit_tab(void) {
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_widget_set_margin_start(grid, 10);
    gtk_widget_set_margin_end(grid, 10);
    gtk_widget_set_margin_top(grid, 10);
    gtk_widget_set_margin_bottom(grid, 10);

    GtkWidget *init_button = gtk_button_new_with_label("Initialize Repo");
    g_signal_connect(init_button, "clicked", G_CALLBACK(on_init_button_clicked), NULL);

    GtkWidget *log_button = gtk_button_new_with_label("View Log");
    g_signal_connect(log_button, "clicked", G_CALLBACK(on_log_button_clicked), NULL);

    git_filename_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(git_filename_entry), "filename (absolute or relative)");
    GtkWidget *add_button = gtk_button_new_with_label("Add File");
    g_signal_connect(add_button, "clicked", G_CALLBACK(on_add_button_clicked), NULL);

    git_commit_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(git_commit_entry), "Commit message for staged files");
    GtkWidget *commit_button = gtk_button_new_with_label("Commit Staged");
    g_signal_connect(commit_button, "clicked", G_CALLBACK(on_commit_button_clicked), NULL);

    git_commit_id_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(git_commit_id_entry), "Commit ID");

    GtkWidget *view_button = gtk_button_new_with_label("View");
    g_signal_connect(view_button, "clicked", G_CALLBACK(on_view_button_clicked), NULL);

    GtkWidget *delete_button = gtk_button_new_with_label("Delete");
    g_signal_connect(delete_button, "clicked", G_CALLBACK(on_delete_button_clicked), NULL);

    GtkWidget *checkout_button = gtk_button_new_with_label("Checkout");
    g_signal_connect(checkout_button, "clicked", G_CALLBACK(on_checkout_button_clicked), NULL);

    git_save_commit_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(git_save_commit_entry), "Commit message for working directory (.mgit_work)");
    GtkWidget *save_commit_button = gtk_button_new_with_label("Save Working Dir Commit");
    g_signal_connect(save_commit_button, "clicked", G_CALLBACK(on_save_commit_button_clicked), NULL);

    commit_files_list = gtk_list_box_new();
    GtkWidget *files_scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(files_scrolled), commit_files_list);
    gtk_widget_set_vexpand(files_scrolled, TRUE);

    GtkWidget *open_editor_button = gtk_button_new_with_label("Open Selected File in Editor");
    g_signal_connect(open_editor_button, "clicked", G_CALLBACK(on_open_in_editor_clicked), NULL);

    GtkWidget *output_scrolled_win = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(output_scrolled_win, TRUE);
    git_output_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(git_output_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(git_output_view), TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(output_scrolled_win), git_output_view);

    gtk_grid_attach(GTK_GRID(grid), init_button, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), log_button,  1, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("File:"),          0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), git_filename_entry,              1, 1, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), add_button,                      3, 1, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Msg (staged):"),  0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), git_commit_entry,                1, 2, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), commit_button,                   3, 2, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Commit ID:"),     0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), git_commit_id_entry,             1, 3, 1, 1);

    GtkWidget *id_actions_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_append(GTK_BOX(id_actions_box), view_button);
    gtk_box_append(GTK_BOX(id_actions_box), delete_button);
    gtk_box_append(GTK_BOX(id_actions_box), checkout_button);
    gtk_grid_attach(GTK_GRID(grid), id_actions_box,                  2, 3, 2, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Msg (working dir):"), 0, 4, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), git_save_commit_entry,                1, 4, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), save_commit_button,                   3, 4, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Files in checked-out commit:"), 0, 5, 4, 1);
    gtk_grid_attach(GTK_GRID(grid), files_scrolled,                               0, 6, 4, 1);
    gtk_grid_attach(GTK_GRID(grid), open_editor_button,                           0, 7, 4, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Output/Log:"), 0, 8, 4, 1);
    gtk_grid_attach(GTK_GRID(grid), output_scrolled_win,          0, 9, 4, 1);

    return grid;
}

static GtkWidget* create_editor_tab(void) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_start(vbox, 10);
    gtk_widget_set_margin_end(vbox, 10);
    gtk_widget_set_margin_top(vbox, 10);
    gtk_widget_set_margin_bottom(vbox, 10);

    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *save_file_button = gtk_button_new_with_label("Save Current File");
    g_signal_connect(save_file_button, "clicked", G_CALLBACK(on_editor_save_file_clicked), NULL);
    gtk_box_append(GTK_BOX(toolbar), save_file_button);

    editor_notebook = gtk_notebook_new();

    GtkWidget *placeholder = gtk_label_new("No file open. Use Mini-Git tab → select file → Open in Editor.");
    gtk_notebook_append_page(GTK_NOTEBOOK(editor_notebook), placeholder, gtk_label_new("Welcome"));

    gtk_box_append(GTK_BOX(vbox), toolbar);
    gtk_box_append(GTK_BOX(vbox), editor_notebook);

    return vbox;
}

/* ---------------- GTK Application lifecycle ---------------- */

static void activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;

    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Mini-Git & Search Engine GUI");
    gtk_window_set_default_size(GTK_WINDOW(window), 900, 650);

    GtkWidget *notebook = gtk_notebook_new();
    gtk_window_set_child(GTK_WINDOW(window), notebook);

    GtkWidget *search_tab_content  = create_search_tab();
    GtkWidget *minigit_tab_content = create_minigit_tab();
    GtkWidget *editor_tab_content  = create_editor_tab();

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), search_tab_content,  gtk_label_new("Search Engine"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), minigit_tab_content, gtk_label_new("Mini-Git"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), editor_tab_content,  gtk_label_new("Editor"));

    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char *argv[]) {
    printf("Initializing backend systems...\n");
    init_repository();
    initialize_trie();
    init_search_engine();
    init_autocomplete_system();
    init_ranking_system();
    printf("Backend systems initialized.\n");

    GtkApplication *app =
        gtk_application_new("com.example.minigitsearchgui",
                            G_APPLICATION_DEFAULT_FLAGS);

    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    printf("Cleaning up backend systems...\n");
    cleanup_ranking_system();
    cleanup_autocomplete_system();
    cleanup_search_engine();
    printf("Cleanup complete. Exiting.\n");

    return status;
}
