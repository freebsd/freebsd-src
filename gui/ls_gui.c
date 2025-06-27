#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Callback function to execute ls -l and display output
static void
list_directory (GtkButton *button, gpointer user_data)
{
  GtkWidget *path_entry = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER(user_data), "path_entry"));
  GtkWidget *output_view = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER(user_data), "output_view"));
  const char *path = gtk_editable_get_text (GTK_EDITABLE (path_entry));
  char command[256];
  FILE *fp;
  char buffer[1024];
  GtkTextBuffer *text_buffer;

  // Construct the command
  if (strlen (path) == 0)
    {
      strcpy (command, "ls -l");
    }
  else
    {
      snprintf (command, sizeof (command), "ls -l %s", path);
    }

  // Open a pipe to the command
  fp = popen (command, "r");
  if (fp == NULL)
    {
      perror ("popen");
      return;
    }

  // Get the text buffer from the text view
  text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (output_view));
  gtk_text_buffer_set_text (text_buffer, "", -1); // Clear previous output

  // Read the command output and append to the text buffer
  while (fgets (buffer, sizeof (buffer), fp) != NULL)
    {
      gtk_text_buffer_insert_at_cursor (text_buffer, buffer, -1);
    }

  // Close the pipe
  pclose (fp);
}

static void
activate (GtkApplication *app, gpointer user_data) // user_data will be argv
{
  char **argv = (char **)user_data; // Cast user_data to argv
  GtkBuilder *builder;
  GtkWidget *window;
  GtkWidget *button;

  // Create a new GtkBuilder instance and load the UI from the XML file
  builder = gtk_builder_new_from_file ("gui/ls_gui.ui");
  if (!builder)
    {
      g_printerr ("Error loading UI file: %s\n", "gui/ls_gui.ui");
      // If UI file is not found, it might be because it's not installed with the binary.
      // Try a relative path assuming it's in the same directory as the executable.
      // This is common during development/testing before proper installation.
      char *exedir = g_path_get_dirname (argv[0]);
      if (exedir) {
          char *uifile = g_build_filename (exedir, "ls_gui.ui", NULL);
          g_free (exedir);
          builder = gtk_builder_new_from_file (uifile);
          if (!builder) {
              g_printerr ("Error loading UI file from executable directory: %s\n", uifile);
              g_free (uifile);
              return;
          }
          g_free (uifile);
      } else {
          return;
      }
    }

  // Get the main window
  window = GTK_WIDGET (gtk_builder_get_object (builder, "main_window"));
  gtk_window_set_application (GTK_WINDOW (window), app);

  // Get the button and connect its "clicked" signal to the list_directory callback
  button = GTK_WIDGET (gtk_builder_get_object (builder, "list_button"));
  g_signal_connect (button, "clicked", G_CALLBACK (list_directory), builder);


  // Show the window
  gtk_widget_set_visible (window, TRUE);

  // g_object_unref (builder); // Builder is passed as user_data to callback, unref later
}

int
main (int argc, char **argv)
{
  GtkApplication *app;
  int status;

  // Handle --version argument
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--version") == 0) {
      g_print("ls_gui version 0.1\n");
      return 0;
    }
  }

  app = gtk_application_new ("org.freebsd.lsgui", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect (app, "activate", G_CALLBACK (activate), argv); // Pass argv to activate
  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return status;
}
