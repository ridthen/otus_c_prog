#include <gtk/gtk.h>

enum {
  FILENAME = 0,
  NUM_COLS
} ;

void read_filenames_in_the_dir(GtkTreeStore *treestore, GtkTreeIter *parent, char* path) {
  GDir *dir;
  GError *error;
  const gchar *filename;
  GtkTreeIter child;

  dir = g_dir_open(path, 0, &error);
  while ((filename = g_dir_read_name(dir))) {
    // printf("%d %s\n", *level, filename);
    gtk_tree_store_append(treestore, &child, parent);
    gtk_tree_store_set(treestore, &child, FILENAME, filename, -1);

    gchar* fileWithFullPath = g_build_filename(path, filename, (gchar*)NULL);
    if(g_file_test(fileWithFullPath, G_FILE_TEST_IS_DIR)) {
      // (*level)++;
      read_filenames_in_the_dir(treestore, &child, fileWithFullPath);
    }
    g_free(fileWithFullPath);
  }
  g_dir_close(dir);
  // (*level)--;
}

static GtkTreeModel *
create_and_fill_model (void) {
  GtkTreeIter* toplevel = NULL;

  GtkTreeStore* treestore = gtk_tree_store_new(NUM_COLS, G_TYPE_STRING);

  read_filenames_in_the_dir(treestore, toplevel, ".");

  return GTK_TREE_MODEL(treestore);
}


static GtkWidget *
create_view_and_model (void)
{
  GtkTreeViewColumn   *col;
  GtkCellRenderer     *renderer;
  GtkWidget           *view;
  GtkTreeModel        *model;

  view = gtk_tree_view_new();

  /* --- Column #1 --- */

  col = gtk_tree_view_column_new();

  gtk_tree_view_column_set_title(col, "File");

  /* pack tree view column into tree view */
  gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

  renderer = gtk_cell_renderer_text_new();

  /* pack cell renderer into tree view column */
  gtk_tree_view_column_pack_start(col, renderer, TRUE);

  /* connect 'text' property of the cell renderer to
   *  model column that contains the first name */
  gtk_tree_view_column_add_attribute(col, renderer, "text", FILENAME);


  model = create_and_fill_model();

  gtk_tree_view_set_model(GTK_TREE_VIEW(view), model);

  g_object_unref(model); /* destroy model automatically with view */

  gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)),
                              GTK_SELECTION_NONE);

  return view;
}


int
main (int argc, char **argv)
{
  gtk_init(&argc, &argv);

  GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  g_signal_connect(window, "delete_event", gtk_main_quit, NULL); /* dirty */

  GtkWidget* view = create_view_and_model();

  gtk_container_add(GTK_CONTAINER(window), view);

  gtk_widget_show_all(window);

  gtk_main();

  return 0;
}